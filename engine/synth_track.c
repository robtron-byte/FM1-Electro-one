/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "synth_track.h"
#include <string.h>

void st_init(st_track_t *t, unsigned area, unsigned voices, uint32_t seed)
{
    memset(t, 0, sizeof *t);
    mks_part_init(&t->part, voices, seed);
    t->area = (uint8_t)area;
    t->level_value = 100;
    t->last_note = area == MKS_AREA_BASS ? 36 : area == MKS_AREA_STRINGS ? 48 : 60;
    for (unsigned i = 0; i < ST_PATTERNS; ++i) {
        t->pattern[i].length = 16;
        t->pattern[i].gate = 50;
        for (unsigned s = 0; s < ST_STEPS; ++s) t->pattern[i].step[s].note = t->last_note;
    }
}

static unsigned chord_notes(int root, unsigned chord, int16_t *out)
{
    if (!chord || chord > MKS_CHORDS) { out[0] = (int16_t)root; return 1; }
    unsigned n = 0;
    for (unsigned i = 0; i < MKS_CHORD_NOTES && mks_chords[chord - 1][i] != -128; ++i) {
        int note = root + mks_chords[chord - 1][i];
        if (note >= 0 && note <= 127) out[n++] = (int16_t)note;
    }
    return n;
}

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
}

EL_DSP_CODE void st_mute(st_track_t *t, int on)
{
    t->muted = on ? 1 : 0;
    if (on) release_all(t);
}

EL_DSP_CODE void st_step(st_track_t *t, unsigned pattern, float step_frames)
{
    if (pattern >= ST_PATTERNS) pattern = 0;
    if (pattern != t->current) { t->current = (uint8_t)pattern; t->playing = 0; }
    const st_pattern_t *p = &t->pattern[t->current];
    unsigned pos = t->playing ? t->pos + 1u : 0u;
    if (pos >= p->length) pos = 0;
    t->pos = (uint8_t)pos;
    t->playing = 1;
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
    {"KRAFT", "ROTHER", "ROBOT", "ACID"}, {"PAD", "STABS", "PULSE", "SWELL"}};

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

/* Strings: chords on electro progressions (i - bVI - bVII - iv and friends). */
static void strings(st_step_t *s, unsigned style, unsigned density, uint32_t *r, int root)
{
    static const int moves[] = {0, -4, -2, 5, 3, -5, 7};
    static const unsigned chords[] = {1, 2, 1, 6, 3, 5}; /* m7, m9, m7, 7SUS4, MIN, 4THS (slot + 1) */
    int second = pick(r, moves, 7);
    unsigned c1 = chords[rnd(r) % 6], c2 = chords[rnd(r) % 6];
    switch (style) {
    case 0: /* PAD: two tied chords per bar */
        for (unsigned i = 0; i < 16; ++i) set(&s[i], root, i < 8 ? 0 : second, i == 0 || i == 8, i != 7 && i != 15, i < 8 ? c1 : c2);
        break;
    case 1: { /* STABS: euclidean chord hits */
        unsigned hits = 3u + rnd(r) % 3u + density / 64u, rotate = rnd(r) % 3u;
        for (unsigned i = 0; i < 16; ++i)
            if (!i || euclid(hits, 16, rotate, i)) set(&s[i], root, i < 8 ? 0 : second, !i || chance(r, 300), 0, i < 8 ? c1 : c2);
        break;
    }
    case 2: /* PULSE: 8th-note chords */
        for (unsigned i = 0; i < 16; i += (density > 90 ? 1u : 2u))
            set(&s[i], root, i < 8 ? 0 : second, i % 4 == 0, 0, i < 8 ? c1 : c2);
        break;
    default: /* SWELL: one chord held through the pattern */
        for (unsigned i = 0; i < 16; ++i) set(&s[i], root, 0, i == 0, i != 15, c1);
        break;
    }
}

void st_generate(st_pattern_t *out, unsigned area, unsigned style, unsigned density, uint32_t seed, unsigned length)
{
    uint32_t r = seed ? seed : 0x2545f491u;
    int root = area == MKS_AREA_BASS ? 36 : area == MKS_AREA_STRINGS ? 48 : (style == 2 ? 72 : 60);
    memset(out, 0, sizeof *out);
    out->length = (uint8_t)el_clampi((int)length, 1, ST_STEPS);
    out->gate = area == MKS_AREA_STRINGS ? 90 : area == MKS_AREA_BASS ? 55 : 40;
    for (unsigned s = 0; s < ST_STEPS; ++s) out->step[s].note = (uint8_t)root;
    if (area == MKS_AREA_STRINGS) strings(out->step, style % ST_STYLES, density, &r, root);
    else melodic(out->step, style % ST_STYLES, density, &r, root);
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
