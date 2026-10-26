/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "synth_track.h"
#include "drums.h"
#include <string.h>

void st_init(st_track_t *t, unsigned area, unsigned voices, uint32_t seed)
{
    memset(t, 0, sizeof *t);
    syn_init(&t->part, SYN_MKS, voices, seed);
    arp_init(&t->arp);
    t->area = (uint8_t)area;
    t->level_value = 100;
    t->last_note = area == MKS_AREA_BASS ? 36 : area == MKS_AREA_STRINGS ? 48 : 60;
    for (unsigned i = 0; i < ST_PATTERNS; ++i) {
        t->pattern[i].length = area == MKS_AREA_STRINGS ? 4 : 16;
        t->pattern[i].gate = area == MKS_AREA_STRINGS ? 100 : 50;
        t->pattern[i].rate = area == MKS_AREA_STRINGS ? ST_RATE_BAR : ST_RATE_16TH;
        for (unsigned s = 0; s < ST_STEPS; ++s) t->pattern[i].step[s].note = t->last_note;
    }
}

const char *const st_rate_names[ST_RATES] = {"1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS"};
static const uint8_t rate_ticks[ST_RATES] EL_DSP_DATA = {1, 2, 4, 8, 16, 32};

EL_DSP_HELPER unsigned st_chord_notes(int root, unsigned chord, int16_t *out)
{
    unsigned type = ST_CHORD_TYPE(chord), inv = ST_CHORD_INV(chord);
    if (!type || type > MKS_CHORDS) { out[0] = (int16_t)el_clampi(root, 0, 127); return 1; }
    int notes[MKS_CHORD_NOTES];
    unsigned n = 0;
    for (unsigned i = 0; i < MKS_CHORD_NOTES && mks_chords[type - 1][i] != -128; ++i) notes[n++] = root + mks_chords[type - 1][i];
    /* Inversion: move the lowest notes up an octave, keep ascending order. */
    for (unsigned k = 0; k < inv && k < n - 1u; ++k) {
        int low = notes[0] + 12;
        for (unsigned i = 0; i + 1 < n; ++i) notes[i] = notes[i + 1];
        notes[n - 1] = low;
    }
    unsigned count = 0;
    for (unsigned i = 0; i < n; ++i)
        if (notes[i] >= 0 && notes[i] <= 127) out[count++] = (int16_t)notes[i];
    return count;
}
#define chord_notes st_chord_notes

static EL_DSP_CODE void release_all(st_track_t *t)
{
    for (unsigned i = 0; i < t->sounding_count; ++i) syn_note_off(&t->part, t->sounding[i]);
    t->sounding_count = 0;
    t->tied = 0;
    t->gate_countdown = 0;
}

/* Live recording into the pattern: nearest 16th, like the drum recorder. */
static EL_DSP_CODE void record_step(st_track_t *t, int note, unsigned chord, float velocity)
{
    if (!t->recording || !t->playing) return;
    st_pattern_t *p = &t->pattern[t->current];
    unsigned step = t->pos;
    if (t->since_step > 0.5f * t->step_frames) step = (step + 1u) % p->length;
    p->step[step].note = (uint8_t)note;
    p->step[step].chord = (uint8_t)chord;
    p->step[step].flags = (uint8_t)(ST_ON | (velocity > 0.9f ? ST_ACCENT : 0));
    ++t->revision;
}

/* Arpeggiator notes: played and recorded like keys. */
static EL_DSP_CODE void arp_play(st_track_t *t, arp_events_t ev)
{
    if (ev.off >= 0) syn_note_off(&t->part, ev.off);
    if (ev.on >= 0) {
        float velocity = ev.velocity / 127.0f;
        syn_note_on(&t->part, ev.on, velocity, 0);
        record_step(t, ev.on, 0, velocity);
    }
}

EL_DSP_CODE void st_arp_set(st_track_t *t, unsigned param, int value)
{
    t->arp.chord = t->chord;
    arp_play(t, arp_set(&t->arp, param, value));
}

EL_DSP_CODE void st_rewind(st_track_t *t)
{
    release_all(t);
    t->playing = 0;
    t->pos = 0;
    t->tick = 0;
}

EL_DSP_CODE void st_mute(st_track_t *t, int on)
{
    t->muted = on ? 1 : 0;
    if (on) release_all(t);
}

/* Called on every 16th; a step lasts rate_ticks 16ths. */
EL_DSP_CODE void st_step(st_track_t *t, uint32_t step, float sixteenth_frames)
{
    t->arp.chord = t->chord;
    arp_play(t, arp_sync(&t->arp, step, sixteenth_frames));
    if (t->next >= ST_PATTERNS) t->next = 0;
    if (t->next != t->current && (step % 16u == 0 || !t->playing)) { t->current = t->next; t->playing = 0; }
    const st_pattern_t *p = &t->pattern[t->current];
    unsigned ticks = rate_ticks[p->rate < ST_RATES ? p->rate : 0];
    if (t->playing && ++t->tick < ticks) return;
    t->tick = 0;
    unsigned pos = t->playing ? t->pos + 1u : 0u;
    if (pos >= p->length) pos = 0;
    t->pos = (uint8_t)pos;
    t->playing = 1;
    float step_frames = sixteenth_frames * (float)ticks;
    t->step_frames = step_frames;
    t->since_step = 0;
    const st_step_t *s = &p->step[pos];
    int on = (s->flags & ST_ON) && !t->muted;
    if (!on) { release_all(t); return; }
    int16_t notes[MKS_CHORD_NOTES];
    unsigned count = chord_notes(el_clampi(s->note + t->transpose, 0, 127), s->chord, notes);
    float velocity = (s->flags & ST_ACCENT) ? 1.0f : 0.7f;
    if (t->tied && t->sounding_count) {
        /* Tie from the previous step: glide/legato into the new notes and let
         * go of the ones that are not part of them. */
        int same = count == t->sounding_count;
        for (unsigned i = 0; same && i < count; ++i) same = notes[i] == t->sounding[i];
        if (!same) {
            for (unsigned i = 0; i < count; ++i) syn_note_on(&t->part, notes[i], velocity, 1);
            for (unsigned i = 0; i < t->sounding_count; ++i) {
                int keep = 0;
                for (unsigned j = 0; j < count; ++j) keep |= notes[j] == t->sounding[i];
                if (!keep) syn_note_off(&t->part, t->sounding[i]);
            }
        }
    } else {
        release_all(t);
        for (unsigned i = 0; i < count; ++i) syn_note_on(&t->part, notes[i], velocity, 0);
    }
    for (unsigned i = 0; i < count; ++i) t->sounding[i] = notes[i];
    t->sounding_count = (uint8_t)count;
    t->tied = (s->flags & ST_TIE) ? 1 : 0;
    t->gate_countdown = t->tied ? 0 : step_frames * el_clampi(p->gate, 10, 100) * 0.01f;
}

EL_DSP_CODE int st_frames_until(const st_track_t *t, int limit)
{
    limit = arp_frames_until(&t->arp, limit);
    if (t->gate_countdown <= 0 || !t->sounding_count) return limit;
    int frames = (int)t->gate_countdown;
    if ((float)frames < t->gate_countdown) ++frames;
    return frames < limit ? frames : limit;
}

EL_DSP_CODE void st_render(st_track_t *t, float *left, float *right, float *dly, float *rev, int frames, float bpm)
{
    /* Area trims balance the sounds against the 808 at equal level settings. */
    static const float trim[4] EL_DSP_DATA = {1.0f, 2.6f, 0.75f, 4.3f};
    syn_render(&t->part, left, right, dly, rev, frames, bpm, t->level_value / 127.0f * trim[t->area & 3u]);
    t->since_step += (float)frames;
    if (t->gate_countdown > 0) {
        t->gate_countdown -= (float)frames;
        if (t->gate_countdown <= 0) {
            for (unsigned i = 0; i < t->sounding_count; ++i) syn_note_off(&t->part, t->sounding[i]);
            t->sounding_count = 0;
            t->gate_countdown = 0;
        }
    }
    t->arp.chord = t->chord;
    arp_play(t, arp_advance(&t->arp, frames, t->playing, bpm));
}

EL_DSP_CODE void st_live(st_track_t *t, int note, float velocity)
{
    int vel = el_clampi((int)(velocity * 127.0f + 0.5f), 0, 127);
    if (arp_active(&t->arp)) {
        /* The arpeggiator takes the keys (and the live chord). */
        t->arp.chord = t->chord;
        arp_play(t, arp_key(&t->arp, note, vel));
        if (vel) t->last_note = (uint8_t)el_clampi(note, 0, 127);
        return;
    }
    int16_t notes[MKS_CHORD_NOTES];
    unsigned count = chord_notes(note, t->chord, notes);
    if (velocity <= 0) {
        for (unsigned i = 0; i < count; ++i) syn_note_off(&t->part, notes[i]);
        return;
    }
    for (unsigned i = 0; i < count; ++i) syn_note_on(&t->part, notes[i], velocity, 0);
    t->last_note = (uint8_t)el_clampi(note, 0, 127);
    record_step(t, note, t->chord, velocity);
}

/* ---- generator (control side), after the iOS app's SequenceGenerator ----- */

static const char *const style_names[2][ST_STYLES] = {
    {"KRAFT", "ROTHER", "ROBOT", "ACID", "MIAMI"}, {"PADS", "SLOW", "PULSE", "STABS", ""}};

unsigned st_styles(unsigned area) { return area == MKS_AREA_STRINGS ? ST_STRING_STYLES : ST_STYLES; }

const char *st_style_name(unsigned area, unsigned style)
{
    return style_names[area == MKS_AREA_STRINGS][style % st_styles(area)];
}

static uint32_t rnd(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return *s = n ? n : 0x9e3779b9u;
}
static int chance(uint32_t *s, unsigned per_mille) { return rnd(s) % 1000u < per_mille; }
static int pick(uint32_t *s, const int *values, unsigned n) { return values[rnd(s) % n]; }
static int euclid(unsigned k, unsigned n, unsigned rotate, unsigned i) { return ((i + rotate) % n) * k % n < k; }

static void set(st_step_t *s, int root, int offset, int accent, int tie, unsigned chord)
{
    s->note = (uint8_t)el_clampi(root + offset, 0, 127);
    s->flags = (uint8_t)(ST_ON | (accent ? ST_ACCENT : 0) | (tie ? ST_TIE : 0));
    s->chord = (uint8_t)chord;
}

/* Phrygian minor, the scale of Rother's lines: a move lands on its
 * nearest degree. */
static int phrygian(int n)
{
    static const uint8_t degree[12] = {0, 1, 1, 3, 3, 5, 5, 7, 8, 8, 10, 10};
    int o = n >= 0 ? n / 12 : -((11 - n) / 12), pc = n - 12 * o;
    return 12 * o + degree[pc];
}

/* ROTHER, measured on the bass stems of Anthony Rother's DEKATRON: 3-11
 * notes a bar (most 4-8), almost all single 16ths (ties ~12 %), mostly off
 * the kick (7-30 % on kick steps: the line interlocks with the electro kick
 * x..x..x...x..x..), and moving by steps: repeats ~18 %, a semitone
 * (mostly down) ~20 %, fourths / fifths ~15 %, thirds and tones the rest. */
static void rother_line(st_step_t *s, unsigned density, uint32_t *r, int root)
{
    static const unsigned counts[] = {3, 4, 4, 5, 5, 6, 7};
    static const int moves[] = {0, 0, -1, -1, -1, -1, 1, 5, -5, 7, -7, 3, -3, 2, -2, 4, -4, 12};
    static const uint16_t kick = 1u << 3 | 1u << 6 | 1u << 10 | 1u << 13;   /* the kick figure it dodges */
    unsigned hits = (unsigned)el_clampi((int)counts[rnd(r) % 7] + (int)density / 43 - 1, 3, 11);
    unsigned rotate = rnd(r) % 16u;
    uint16_t on = 1;   /* the root on 1 */
    for (unsigned i = 0; i < 16; ++i)
        if (euclid(hits, 16, rotate, i)) {
            unsigned k = i;
            if ((kick >> k) & 1u && !chance(r, 250)) k = (k + 1) % 16;   /* step off the kick */
            on |= (uint16_t)(1u << k);
        }
    int note = 0;
    for (unsigned i = 0; i < 16; ++i) {
        if (!((on >> i) & 1u)) continue;
        if (i) note = phrygian(el_clampi(note + pick(r, moves, 18), -5, 13));
        int offbeat = i % 4 == 2 || i % 4 == 3, next = i < 15 && ((on >> (i + 1)) & 1u);
        set(&s[i], root, note, !i || (offbeat && chance(r, 450)), next && chance(r, 120), 0);
    }
    if (chance(r, 600)) {   /* call and response: the second half answers the first */
        static const int ends[] = {12, -2, 1, 7};
        for (unsigned i = 0; i < 8; ++i) s[i + 8] = s[i];
        if (s[14].flags & ST_ON) s[14].note = (uint8_t)el_clampi(root + pick(r, ends, 4), 0, 127);
    }
}

/* MIAMI: the 808 bass of The Egyptian Lover's 1986: 2-6 notes a bar on
 * the kick (35-67 % of them), held for 2-5 steps, the root most of the time
 * (70-95 %), with fourths, fifths, the minor third, the seventh below and
 * the octave. */
static void miami_line(st_step_t *s, unsigned density, uint32_t *r, int root)
{
    static const uint16_t figures[] = {
        1u | 1u << 6 | 1u << 10,                    /* x.....x...x..... */
        1u | 1u << 4 | 1u << 6 | 1u << 10,          /* x...x.x...x..... */
        1u | 1u << 6 | 1u << 8 | 1u << 11,          /* x.....x.x..x.... */
        1u | 1u << 4 | 1u << 6 | 1u << 8 | 1u << 12,
        1u | 1u << 6 | 1u << 10 | 1u << 13,
    };
    static const int others[] = {5, 7, -5, 3, -2, 12, 12};
    uint16_t on = figures[rnd(r) % 5u];
    if (density > 80 && chance(r, 500)) on |= (uint16_t)(1u << (14 + rnd(r) % 2u));
    for (unsigned i = 0; i < 16; ++i) {
        if (!((on >> i) & 1u)) continue;
        int note = !i || chance(r, 750) ? 0 : pick(r, others, 7);
        unsigned next = i + 1;
        while (next < 16 && !((on >> next) & 1u)) ++next;
        set(&s[i], root, note, !i || chance(r, 300), 0, 0);
        /* Long 808 notes: held up to the next one (or 2-5 steps). */
        if (chance(r, 650)) {
            unsigned len = 2 + rnd(r) % 4u;
            for (unsigned k = 1; k < len && i + k < next; ++k) set(&s[i + k], root, note, 0, 0, 0), s[i + k - 1].flags |= ST_TIE;
        }
    }
}

/* 16 steps of a melodic style, relative to root. */
static void melodic(st_step_t *s, unsigned style, unsigned density, uint32_t *r, int root)
{
    static const int kraft[] = {0, 0, 12, 7, 3, 10, 5, -5}, rother[] = {0, 0, 0, 12, 12, 1, -2, 7, 3, -12, 10};
    static const int robot[] = {0, 7, 12, 15, 3, 10, 17, 5}, acid[] = {0, 0, 12, 3, 7, 10, 5, -2, 8};
    int extra = (int)density / 43; /* 0..2 more hits with higher density */
    if (style == 0) {
        unsigned motif = chance(r, 500) ? 4 : 8;
        int bounce = chance(r, 400);
        static const int answers[] = {0, 0, -2, 3, 5, -5}, fifths[] = {7, 3, 10};
        int answer = pick(r, answers, 6);
        st_step_t m[8];
        memset(m, 0, sizeof m);
        for (unsigned i = 0; i < motif; ++i) {
            if (i == 0) set(&m[i], root, 0, 1, 0, 0);
            else if (bounce) set(&m[i], root, chance(r, 250) ? pick(r, fifths, 3) : (i & 1) ? 12 : 0, 0, 0, 0);
            else if (!chance(r, 180 - 50 * extra)) set(&m[i], root, pick(r, kraft, 8), i % 4 == 0, 0, 0);
        }
        for (unsigned i = 0; i < 16; ++i) {
            s[i] = m[i % motif];
            if (i >= 16 - motif && (s[i].flags & ST_ON)) s[i].note = (uint8_t)el_clampi(s[i].note + answer, 0, 127);
        }
        return;
    }
    if (style == 1) { rother_line(s, density, r, root); return; }
    if (style == 4) { miami_line(s, density, r, root); return; }
    static const unsigned hits_rother[] = {7, 9, 10, 11}, hits_robot[] = {4, 5, 6, 7}, hits_acid[] = {11, 12, 13};
    unsigned hits = style == 1 ? hits_rother[rnd(r) % 4] : style == 2 ? hits_robot[rnd(r) % 4] : hits_acid[rnd(r) % 3];
    hits = (unsigned)el_clampi((int)hits + extra - 1, 3, 15);
    unsigned rotate = style == 3 ? 0 : rnd(r) % (style == 2 ? 4u : 3u);
    for (unsigned i = 0; i < 16; ++i) {
        if (!euclid(hits, 16, rotate, i) && i) continue;
        int next = euclid(hits, 16, rotate, (i + 1) % 16) && i < 15;
        if (style == 1) {
            int offbeat = i % 4 == 2 || i % 4 == 3;
            set(&s[i], root, i ? pick(r, rother, 11) : 0, !i || (offbeat && chance(r, 450)), next && chance(r, 120), 0);
        } else if (style == 2) {
            set(&s[i], root, i ? pick(r, robot, 8) : 0, chance(r, 300), 0, 0);
        } else {
            set(&s[i], root, i ? pick(r, acid, 9) : 0, chance(r, 350), next && chance(r, 250), 0);
        }
    }
    if (style == 1 && chance(r, 600)) {
        static const int ends[] = {12, -2, 1, 7};
        for (unsigned i = 0; i < 8; ++i) s[i + 8] = s[i];
        if (s[14].flags & ST_ON) s[14].note = (uint8_t)el_clampi(root + pick(r, ends, 4), 0, 127);
    }
}

/* Strings harmony: four-chord loops per family, as (semitones above the
 * tonic, chord slot + 1, bars). Chord slots: 1 m7, 2 SUS2, 3 MIN, 4 5+8,
 * 5 SUS4, 6 7TH, 7 MAJ, 8 m7b5.
 * ROTHER: dark minor, long chords over the tonic, phrygian bII, sus and
 *   quartal colours. KRAFT: plain triads, two-chord swings (The Model:
 *   i - v), sus2 over the tonic and a maj7 interlude (Computer Love), minor
 *   bVI (The Robots), mediant shifts (Europe Endless). Loops marked with a
 *   song are approximations from community chord sheets; the rest follow
 *   the described harmonic language. ELECTRO: Detroit / Miami minor loops. */
static const int8_t progressions[DR_HARMONIES][8][4][3] = {
    { /* ROTHER: measured on the string / pad stems of DEKATRON: mostly
       * three close voices (minor triads, some m7, sus, fifths, a diminished
       * colour), a new chord about every bar, roots moving by tone, major
       * third, semitone and fourth. */
        {{0, 3, 1}, {0, 3, 1}, {1, 7, 1}, {0, 1, 1}},     /* phrygian drone: i i bII i7 */
        {{0, 3, 1}, {8, 7, 1}, {3, 7, 1}, {10, 5, 1}},    /* i bVI bIII bVIIsus4 */
        {{0, 4, 1}, {0, 4, 1}, {1, 4, 1}, {10, 4, 1}},    /* power-fifth stab drone */
        {{0, 3, 2}, {5, 3, 1}, {0, 3, 2}, {7, 1, 1}},     /* i - iv - i - v7 */
        {{0, 5, 1}, {10, 5, 1}, {8, 5, 1}, {7, 5, 1}},    /* parallel sus4 falling */
        {{0, 3, 1}, {3, 7, 1}, {8, 7, 1}, {1, 7, 1}},
        {{0, 2, 1}, {0, 3, 1}, {8, 7, 1}, {10, 2, 1}},   /* sus2 colour */
        {{0, 3, 2}, {9, 8, 1}, {8, 7, 1}, {7, 5, 2}},     /* m7b5 shadow, as on Meta Me */
    },
    { /* KRAFT */
        {{0, 3, 1}, {7, 3, 1}, {0, 3, 1}, {7, 3, 1}},     /* The Model: i - v */
        {{0, 3, 2}, {5, 2, 2}, {0, 3, 2}, {5, 2, 2}},     /* Computer Love verse */
        {{8, 7, 1}, {8, 7, 1}, {3, 7, 1}, {10, 7, 1}},    /* Computer Love interlude */
        {{0, 3, 2}, {8, 3, 2}, {5, 3, 2}, {7, 7, 2}},     /* The Robots colours */
        {{0, 3, 2}, {8, 7, 2}, {5, 3, 2}, {3, 7, 2}},     /* Radioactivity colours */
        {{0, 3, 1}, {3, 7, 1}, {5, 7, 1}, {10, 7, 1}},    /* Tour de France colours */
        {{0, 3, 2}, {4, 7, 2}, {8, 7, 2}, {4, 7, 2}},     /* Europe Endless mediants */
        {{0, 3, 2}, {10, 7, 2}, {8, 7, 2}, {10, 7, 2}},   /* parallel triads */
    },
    { /* ELECTRO */
        {{0, 1, 1}, {-4, 7, 1}, {-2, 5, 1}, {5, 1, 1}},
        {{0, 1, 1}, {5, 1, 1}, {-4, 7, 1}, {-5, 1, 1}},
        {{0, 1, 1}, {-2, 4, 1}, {-4, 7, 1}, {-2, 5, 1}},
        {{0, 1, 1}, {3, 7, 1}, {-2, 5, 1}, {-4, 7, 1}},
        {{0, 1, 1}, {0, 5, 1}, {-4, 7, 1}, {-5, 1, 1}},
        {{0, 1, 1}, {-2, 5, 1}, {-4, 5, 1}, {-2, 5, 1}},
        {{0, 1, 1}, {-4, 1, 1}, {3, 7, 1}, {-2, 5, 1}},
        {{0, 1, 1}, {0, 1, 1}, {5, 1, 1}, {-2, 5, 1}},
    },
    { /* EGYPT: measured on the pad stems of The Egyptian Lover's 1986:
       * fifth-octave power voicings, major, dominant 7th and minor chords,
       * a new chord every bar, roots moving by minor third, fourth and the
       * tritone. Low register (C2 C3 G3). */
        {{0, 4, 1}, {0, 4, 1}, {3, 4, 1}, {5, 4, 1}},     /* power: i i bIII iv */
        {{0, 4, 1}, {5, 4, 1}, {0, 4, 1}, {-5, 4, 1}},    /* fourths */
        {{0, 3, 1}, {3, 7, 1}, {5, 6, 1}, {0, 3, 1}},     /* i bIII IV7 i */
        {{0, 7, 1}, {-6, 4, 1}, {0, 7, 1}, {3, 4, 1}},    /* tritone swing */
        {{0, 6, 1}, {3, 6, 1}, {5, 6, 1}, {3, 6, 1}},     /* dominant 7ths */
        {{0, 4, 1}, {3, 4, 1}, {-2, 4, 1}, {0, 6, 1}},
        {{0, 3, 1}, {0, 4, 1}, {-2, 7, 1}, {-4, 7, 1}},
        {{0, 4, 2}, {3, 4, 1}, {5, 6, 1}, {-3, 4, 1}},
    },
};

/* Pick the inversion that keeps the chord closest to the previous one
 * (smooth voice leading, like a player would). */
static uint8_t voice_lead(int root, unsigned type, const int16_t *prev, unsigned prev_count)
{
    uint8_t best = (uint8_t)type;
    int best_cost = 1 << 30;
    for (unsigned inv = 0; inv < 3; ++inv) {
        int16_t notes[MKS_CHORD_NOTES];
        unsigned chord = type | inv << 4, n = st_chord_notes(root, chord, notes);
        int cost = 0;
        for (unsigned i = 0; i < n; ++i) {
            int nearest = 1 << 20;
            for (unsigned j = 0; j < prev_count; ++j) {
                int d = notes[i] - prev[j];
                if (d < 0) d = -d;
                if (d < nearest) nearest = d;
            }
            cost += prev_count ? nearest : notes[i] - root;
        }
        if (notes[n - 1] > root + 26) cost += 40;   /* keep the voicing compact */
        if (cost < best_cost) { best_cost = cost; best = (uint8_t)chord; }
    }
    return best;
}

static void strings(st_pattern_t *out, unsigned style, unsigned density, uint32_t *r, int root, unsigned harmony)
{
    const int8_t (*prog)[3] = progressions[harmony < DR_HARMONIES ? harmony : 0][rnd(r) % 8u];
    int16_t prev[MKS_CHORD_NOTES];
    unsigned prev_count = 0, bars[4], total = 0;
    uint8_t chords[4];
    int roots[4];
    for (unsigned k = 0; k < 4; ++k) {
        int offset = prog[k][0];
        if (offset > 6) offset -= 12;              /* stay near the tonic */
        roots[k] = root + offset;
        chords[k] = voice_lead(roots[k], (unsigned)prog[k][1], prev, prev_count);
        prev_count = st_chord_notes(roots[k], chords[k], prev);
        bars[k] = (unsigned)prog[k][2];
        total += bars[k];
    }
    unsigned step = 0;
    switch (style) {
    case 0:   /* PADS: held chords, one step per bar */
    case 1:   /* SLOW: the same at two bars per step */
        out->rate = style ? ST_RATE_2BARS : ST_RATE_BAR;
        out->gate = 100;
        for (unsigned k = 0; k < 4; ++k)
            for (unsigned b = 0; b < bars[k]; ++b, ++step) set(&out->step[step], roots[k], 0, k == 0 && b == 0, step + 1u < total, chords[k]);
        out->length = (uint8_t)total;
        (void)density;
        break;
    case 2: { /* PULSE: repeated chords, a new chord per bar of the progression */
        unsigned per_bar = total * 8u <= ST_STEPS ? 8u : 4u;
        out->rate = per_bar == 8u ? ST_RATE_8TH : ST_RATE_QUARTER;
        out->gate = 60;
        for (unsigned k = 0; k < 4; ++k)
            for (unsigned i = 0; i < bars[k] * per_bar; ++i, ++step) set(&out->step[step], roots[k], 0, i % per_bar == 0, 0, chords[k]);
        out->length = (uint8_t)step;
        break;
    }
    default: {/* STABS: euclidean 16th stabs over the first two chords */
        unsigned hits = 3u + rnd(r) % 3u + density / 64u, rotate = rnd(r) % 3u;
        out->rate = ST_RATE_16TH;
        out->length = 16;
        out->gate = 35;
        for (unsigned i = 0; i < 16; ++i)
            if (!i || euclid(hits, 16, rotate, i)) set(&out->step[i], roots[i / 8], 0, !i || chance(r, 300), 0, chords[i / 8]);
        break;
    }
    }
}

void st_generate(st_pattern_t *out, unsigned area, unsigned style, unsigned density, uint32_t seed, unsigned length,
                 unsigned harmony)
{
    uint32_t r = seed ? seed : 0x2545f491u;
    /* Strings: Rother's pads sit about a fifth higher (lowest note E3-C4). */
    int strings_root = harmony == DR_HARMONY_ROTHER ? 57 : 48;
    int root = area == MKS_AREA_BASS ? 36 : area == MKS_AREA_STRINGS ? strings_root : (style == 2 ? 72 : 60);
    memset(out, 0, sizeof *out);
    out->length = (uint8_t)el_clampi((int)length, 1, ST_STEPS);
    out->gate = area == MKS_AREA_STRINGS ? 90 : area == MKS_AREA_BASS ? 55 : 40;
    for (unsigned s = 0; s < ST_STEPS; ++s) out->step[s].note = (uint8_t)root;
    out->rate = ST_RATE_16TH;
    if (area == MKS_AREA_STRINGS) { strings(out, style % ST_STRING_STYLES, density, &r, root, harmony); return; }
    melodic(out->step, style % ST_STYLES, density, &r, root);
    /* Longer patterns: repeat with a small variation per bar. */
    for (unsigned bar = 16; bar < out->length; bar += 16) {
        for (unsigned i = 0; i < 16 && bar + i < ST_STEPS; ++i) out->step[bar + i] = out->step[i];
        unsigned i = bar + 1u + rnd(&r) % 15u;
        if (i < ST_STEPS && (out->step[i].flags & ST_ON) && area != MKS_AREA_STRINGS) {
            static const int shift[] = {12, -12, 7, 3, -2};
            out->step[i].note = (uint8_t)el_clampi(out->step[i].note + pick(&r, shift, 5), 0, 127);
        }
    }
}

void st_vary(st_pattern_t *p, unsigned area, unsigned amount, uint32_t seed)
{
    uint32_t r = seed ? seed : 0x1234567u;
    unsigned per_mille = (amount > 127 ? 127 : amount) * 3u, length = p->length ? p->length : 16;
    if (length > ST_STEPS) length = ST_STEPS;
    /* Notes and chords the pattern already uses: a variation stays in them. */
    uint8_t notes[ST_STEPS], chords[ST_STEPS];
    unsigned count = 0, first = length;
    for (unsigned i = 0; i < length; ++i)
        if (p->step[i].flags & ST_ON) {
            if (first == length) first = i;
            notes[count] = p->step[i].note;
            chords[count++] = p->step[i].chord;
        }
    if (!count) return;
    int strings = area == MKS_AREA_STRINGS;
    for (unsigned i = 0; i < length; ++i) {
        st_step_t *s = &p->step[i];
        if (!(s->flags & ST_ON) || i == first || !chance(&r, per_mille)) continue;
        switch (rnd(&r) % (strings ? 2u : 4u)) {
        case 0:   /* another note of the pattern, or the same an octave away */
            if (count > 1 && chance(&r, 700)) s->note = notes[rnd(&r) % count];
            else s->note = (uint8_t)el_clampi(s->note + (s->note >= 48 && chance(&r, 500) ? -12 : 12), 24, 96);
            break;
        case 1:   /* strings: inversion or a chord type of the pattern; else tie / accent */
            if (strings) {
                unsigned type = chance(&r, 400) ? ST_CHORD_TYPE(chords[rnd(&r) % count]) : ST_CHORD_TYPE(s->chord);
                s->chord = (uint8_t)(type | (rnd(&r) % 3u) << 4);
            } else s->flags ^= chance(&r, 500) ? ST_TIE : ST_ACCENT;
            break;
        default: { /* moved by one step where that one is free */
            unsigned to = rnd(&r) & 1u ? (i + 1u) % length : (i + length - 1u) % length;
            if (to != first && !(p->step[to].flags & ST_ON)) {
                p->step[to] = *s;
                s->flags = 0;
                if (to == i + 1u) ++i;   /* not moved twice */
            }
            break;
        }
        }
    }
    /* Rhythm and bass: now and then one more note of the pattern. */
    if (!strings && chance(&r, per_mille)) {
        unsigned i = rnd(&r) % length;
        if (!(p->step[i].flags & ST_ON)) {
            p->step[i].note = notes[rnd(&r) % count];
            p->step[i].chord = chords[0];
            p->step[i].flags = ST_ON;
        }
    }
}

EL_DSP_CODE void st_step_at(st_track_t *t, uint32_t step, float sixteenth_frames, unsigned target)
{
    if (t->next >= ST_PATTERNS) t->next = 0;
    const st_pattern_t *p = &t->pattern[t->current];
    unsigned ticks = rate_ticks[p->rate < ST_RATES ? p->rate : 0], len = p->length ? p->length : 1u;
    /* Where the playhead would be after this 16th. */
    unsigned tick = 0, pos = 0;
    if (t->playing && t->tick + 1u < ticks) { tick = t->tick + 1u; pos = t->pos; }
    else if (t->playing) pos = t->pos + 1u < len ? t->pos + 1u : 0u;
    uint8_t next = t->next;
    t->next = t->current;
    t->playing = 1;
    t->tick = (uint8_t)(ticks - 1u);
    t->pos = (uint8_t)((target / ticks % len + len - 1u) % len);
    st_step(t, step, sixteenth_frames);
    t->next = next;
    t->tick = (uint8_t)tick;
    t->pos = (uint8_t)pos;
}
