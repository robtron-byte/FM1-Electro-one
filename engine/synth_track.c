/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "synth_track.h"
#include "drums.h"
#include <string.h>

void st_init(st_track_t *t, unsigned area, unsigned voices, uint32_t seed)
{
    memset(t, 0, sizeof *t);
    mks_part_init(&t->part, voices, seed);
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

unsigned st_chord_notes(int root, unsigned chord, int16_t *out)
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
    for (unsigned i = 0; i < t->sounding_count; ++i) mks_note_off(&t->part, t->sounding[i]);
    t->sounding_count = 0;
    t->tied = 0;
    t->gate_countdown = 0;
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
EL_DSP_CODE void st_step(st_track_t *t, unsigned pattern, float sixteenth_frames)
{
    if (pattern >= ST_PATTERNS) pattern = 0;
    if (pattern != t->current) { t->current = (uint8_t)pattern; t->playing = 0; }
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
    unsigned count = chord_notes(s->note, s->chord, notes);
    float velocity = (s->flags & ST_ACCENT) ? 1.0f : 0.7f;
    if (t->tied && t->sounding_count) {
        /* Tie from the previous step: glide/legato into the new notes and let
         * go of the ones that are not part of them. */
        int same = count == t->sounding_count;
        for (unsigned i = 0; same && i < count; ++i) same = notes[i] == t->sounding[i];
        if (!same) {
            for (unsigned i = 0; i < count; ++i) mks_note_on(&t->part, notes[i], velocity, 1);
            for (unsigned i = 0; i < t->sounding_count; ++i) {
                int keep = 0;
                for (unsigned j = 0; j < count; ++j) keep |= notes[j] == t->sounding[i];
                if (!keep) mks_note_off(&t->part, t->sounding[i]);
            }
        }
    } else {
        release_all(t);
        for (unsigned i = 0; i < count; ++i) mks_note_on(&t->part, notes[i], velocity, 0);
    }
    for (unsigned i = 0; i < count; ++i) t->sounding[i] = notes[i];
    t->sounding_count = (uint8_t)count;
    t->tied = (s->flags & ST_TIE) ? 1 : 0;
    t->gate_countdown = t->tied ? 0 : step_frames * el_clampi(p->gate, 10, 100) * 0.01f;
}

EL_DSP_CODE int st_frames_until(const st_track_t *t, int limit)
{
    if (t->gate_countdown <= 0 || !t->sounding_count) return limit;
    int frames = (int)t->gate_countdown;
    if ((float)frames < t->gate_countdown) ++frames;
    return frames < limit ? frames : limit;
}

EL_DSP_CODE void st_render(st_track_t *t, float *left, float *right, float *dly, float *rev, int frames, float bpm)
{
    /* Area trims balance the sounds against the 808 at equal level settings. */
    static const float trim[4] EL_DSP_DATA = {1.0f, 2.6f, 0.75f, 4.3f};
    t->part.level = t->level_value / 127.0f * trim[t->area & 3u];
    mks_render(&t->part, left, right, dly, rev, frames, bpm);
    t->since_step += (float)frames;
    if (t->gate_countdown > 0) {
        t->gate_countdown -= (float)frames;
        if (t->gate_countdown <= 0) {
            for (unsigned i = 0; i < t->sounding_count; ++i) mks_note_off(&t->part, t->sounding[i]);
            t->sounding_count = 0;
            t->gate_countdown = 0;
        }
    }
}

EL_DSP_CODE void st_live(st_track_t *t, int note, float velocity)
{
    int16_t notes[MKS_CHORD_NOTES];
    unsigned count = chord_notes(note, t->chord, notes);
    if (velocity <= 0) {
        for (unsigned i = 0; i < count; ++i) mks_note_off(&t->part, notes[i]);
        return;
    }
    for (unsigned i = 0; i < count; ++i) mks_note_on(&t->part, notes[i], velocity, 0);
    t->last_note = (uint8_t)el_clampi(note, 0, 127);
    if (!t->recording || !t->playing) return;
    /* Quantize to the nearest 16th, like the drum recorder. */
    st_pattern_t *p = &t->pattern[t->current];
    unsigned step = t->pos;
    if (t->since_step > 0.5f * t->step_frames) step = (step + 1u) % p->length;
    p->step[step].note = (uint8_t)note;
    p->step[step].chord = t->chord;
    p->step[step].flags = (uint8_t)(ST_ON | (velocity > 0.9f ? ST_ACCENT : 0));
    ++t->revision;
}

/* ---- generator (control side), after the iOS app's SequenceGenerator ----- */

static const char *const style_names[2][ST_STYLES] = {
    {"KRAFT", "ROTHER", "ROBOT", "ACID"}, {"PADS", "SLOW", "PULSE", "STABS"}};

const char *st_style_name(unsigned area, unsigned style)
{
    return style_names[area == MKS_AREA_STRINGS][style % ST_STYLES];
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
 * tonic, chord slot + 1, bars). Chord slots: 1 m7, 2 m9, 3 MIN, 4 5+8,
 * 5 4THS, 6 7SUS4, 7 MAJ7, 8 m7b5, 9 MAJ, 10 SUS2, 11 ADD9, 12 MAJ9, 13 7.
 * ROTHER: dark minor, long chords over the tonic, phrygian bII, sus and
 *   quartal colours. KRAFT: plain triads, two-chord swings (The Model:
 *   i - v), sus2 over the tonic and a maj7 interlude (Computer Love), minor
 *   bVI (The Robots), mediant shifts (Europe Endless). Loops marked with a
 *   song are approximations from community chord sheets; the rest follow
 *   the described harmonic language. ELECTRO: Detroit / Miami minor loops. */
static const int8_t progressions[DR_HARMONIES][8][4][3] = {
    { /* ROTHER */
        {{0, 2, 2}, {0, 2, 2}, {1, 7, 2}, {0, 1, 2}},     /* phrygian drone */
        {{0, 1, 2}, {8, 7, 2}, {3, 12, 2}, {10, 6, 2}},
        {{0, 4, 1}, {0, 4, 1}, {1, 4, 1}, {10, 4, 1}},    /* power-fifth stab drone */
        {{0, 2, 2}, {5, 2, 2}, {0, 2, 2}, {7, 1, 2}},     /* i - iv - i - v */
        {{0, 5, 1}, {10, 5, 1}, {8, 5, 1}, {7, 5, 1}},    /* parallel 4ths falling */
        {{0, 1, 2}, {3, 7, 2}, {8, 7, 2}, {1, 9, 2}},
        {{0, 10, 2}, {0, 1, 2}, {8, 11, 2}, {10, 10, 2}},
        {{0, 2, 2}, {9, 8, 2}, {8, 7, 2}, {7, 6, 2}},
    },
    { /* KRAFT */
        {{0, 3, 1}, {7, 3, 1}, {0, 3, 1}, {7, 3, 1}},     /* The Model: i - v */
        {{0, 3, 2}, {5, 10, 2}, {0, 3, 2}, {5, 10, 2}},   /* Computer Love verse */
        {{8, 7, 1}, {8, 9, 1}, {3, 9, 1}, {10, 9, 1}},    /* Computer Love interlude */
        {{0, 3, 2}, {8, 3, 2}, {5, 3, 2}, {7, 9, 2}},     /* The Robots colours */
        {{0, 3, 2}, {8, 9, 2}, {5, 3, 2}, {3, 9, 2}},     /* Radioactivity colours */
        {{0, 3, 1}, {3, 9, 1}, {5, 9, 1}, {10, 9, 1}},    /* Tour de France colours */
        {{0, 3, 2}, {4, 9, 2}, {8, 9, 2}, {4, 9, 2}},     /* Europe Endless mediants */
        {{0, 3, 2}, {10, 9, 2}, {8, 9, 2}, {10, 9, 2}},   /* parallel triads */
    },
    { /* ELECTRO */
        {{0, 2, 1}, {-4, 7, 1}, {-2, 6, 1}, {5, 1, 1}},
        {{0, 1, 1}, {5, 1, 1}, {-4, 7, 1}, {-5, 1, 1}},
        {{0, 2, 1}, {-2, 4, 1}, {-4, 7, 1}, {-2, 6, 1}},
        {{0, 1, 1}, {3, 7, 1}, {-2, 6, 1}, {-4, 7, 1}},
        {{0, 2, 1}, {0, 6, 1}, {-4, 7, 1}, {-5, 2, 1}},
        {{0, 1, 1}, {-2, 5, 1}, {-4, 5, 1}, {-2, 5, 1}},
        {{0, 2, 1}, {-4, 2, 1}, {3, 7, 1}, {-2, 6, 1}},
        {{0, 1, 1}, {0, 2, 1}, {5, 2, 1}, {-2, 6, 1}},
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
    int root = area == MKS_AREA_BASS ? 36 : area == MKS_AREA_STRINGS ? 48 : (style == 2 ? 72 : 60);
    memset(out, 0, sizeof *out);
    out->length = (uint8_t)el_clampi((int)length, 1, ST_STEPS);
    out->gate = area == MKS_AREA_STRINGS ? 90 : area == MKS_AREA_BASS ? 55 : 40;
    for (unsigned s = 0; s < ST_STEPS; ++s) out->step[s].note = (uint8_t)root;
    out->rate = ST_RATE_16TH;
    if (area == MKS_AREA_STRINGS) { strings(out, style % ST_STYLES, density, &r, root, harmony); return; }
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
