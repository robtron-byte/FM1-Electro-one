/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "arp.h"
#include "synth_track.h"
#include <string.h>

const char *const arp_mode_names[ARP_MODES] = {"UP", "DOWN", "UP/DN", "RAND", "ORDER"};
/* The apps' divisions from 1/32 to 1/1, in 24ths of a quarter. */
const char *const arp_rate_names[ARP_RATES] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8",
                                               "1/8.", "1/4T", "1/4", "1/2", "1/1"};
static const uint8_t rate_ticks[ARP_RATES] EL_DSP_DATA = {3, 4, 6, 9, 8, 12, 18, 16, 24, 48, 96};
static const arp_events_t none = {-1, -1, 0};

int arp_param_max(unsigned param)
{
    static const uint8_t max[ARP_PARAMS] = {1, ARP_MODES - 1, ARP_RATES - 1, 3, 100, 1};
    return param < ARP_PARAMS ? max[param] : 0;
}

void arp_init(arp_t *a)
{
    memset(a, 0, sizeof *a);
    a->v[ARP_RATE] = 2;      /* 1/16 */
    a->v[ARP_GATE] = 65;     /* the apps' default gate */
    a->sounding = -1;
    a->random = 0xa4b3c2d1u;
    a->tick_frames = EL_SAMPLE_RATE * 60.0f / (120.0f * 24.0f);
    a->countdown = a->tick_frames;
}

static EL_DSP_CODE int held(const arp_t *a, int note) { return (a->held[note >> 3] >> (note & 7)) & 1; }

static EL_DSP_CODE int any_held(const arp_t *a)
{
    for (unsigned i = 0; i < sizeof a->held; ++i) if (a->held[i]) return 1;
    return 0;
}

static EL_DSP_CODE void remove_key(arp_t *a, int note)
{
    unsigned w = 0;
    for (unsigned r = 0; r < a->count; ++r)
        if (a->keys[r] != note) { a->keys[w] = a->keys[r]; a->velocities[w] = a->velocities[r]; ++w; }
    a->count = (uint8_t)w;
}

/* Next note of the pattern: every held key expanded by the chord tones,
 * sorted by pitch (ORDER keeps the play order), over the octaves. */
static EL_DSP_CODE void next_note(arp_t *a, int *note, int *velocity)
{
    uint8_t pool[ARP_POOL], vel[ARP_POOL], order[ARP_POOL];
    unsigned n = 0;
    for (unsigned k = 0; k < a->count; ++k) {
        int16_t tones[MKS_CHORD_NOTES];
        unsigned count = st_chord_notes(a->keys[k], a->chord, tones);
        for (unsigned t = 0; t < count && n < ARP_POOL; ++t) { pool[n] = (uint8_t)tones[t]; vel[n] = a->velocities[k]; ++n; }
    }
    if (!n) { *note = -1; return; }
    for (unsigned i = 0; i < n; ++i) order[i] = (uint8_t)i;
    if (a->v[ARP_MODE] != 4)
        for (unsigned i = 1; i < n; ++i) {
            uint8_t v = order[i];
            int j = (int)i - 1;
            while (j >= 0 && pool[order[j]] > pool[v]) { order[j + 1] = order[j]; --j; }
            order[j + 1] = v;
        }
    unsigned total = n * (a->v[ARP_OCTAVES] + 1u), index;
    switch (a->v[ARP_MODE]) {
    case 1: index = total - 1u - a->position % total; break;              /* DOWN */
    case 2: {                                                              /* UP/DN, ends once */
        unsigned period = total > 1 ? 2u * total - 2u : 1u, m = a->position % period;
        index = m < total ? m : period - m;
        break;
    }
    case 3:                                                                /* RANDOM */
        a->random ^= a->random << 13; a->random ^= a->random >> 17; a->random ^= a->random << 5;
        index = a->random % total;
        break;
    default: index = a->position % total; break;                           /* UP, ORDER */
    }
    a->position = (a->position + 1u) % 1000000u;
    unsigned slot = order[index % n];
    *note = el_clampi(pool[slot] + 12 * (int)(index / n), 0, 127);
    *velocity = vel[slot];
}

static EL_DSP_CODE arp_events_t fire(arp_t *a)
{
    arp_events_t ev = none;
    int note, velocity = 100;
    next_note(a, &note, &velocity);
    if (note < 0) return ev;
    ev.off = a->sounding;
    ev.on = (int16_t)note;
    ev.velocity = (uint8_t)velocity;
    a->sounding = (int16_t)note;
    unsigned rate = rate_ticks[a->v[ARP_RATE] < ARP_RATES ? a->v[ARP_RATE] : 2];
    a->gate_left = (float)rate * a->tick_frames * (float)el_clampi(a->v[ARP_GATE], 5, 100) * 0.01f;
    return ev;
}

/* One clock tick: a note on every grid step of the rate. */
static EL_DSP_CODE arp_events_t on_tick(arp_t *a)
{
    unsigned rate = rate_ticks[a->v[ARP_RATE] < ARP_RATES ? a->v[ARP_RATE] : 2];
    if (!a->count || !a->v[ARP_ON] || a->tick % rate) return none;
    uint32_t grid = a->tick / rate;
    if (a->grid_valid && grid == a->last_grid) return none;
    a->last_grid = grid;
    a->grid_valid = 1;
    return fire(a);
}

EL_DSP_CODE arp_events_t arp_key(arp_t *a, int note, int velocity)
{
    arp_events_t ev = none;
    note = el_clampi(note, 0, 127);
    if (velocity > 0) {
        /* HOLD: a new chord (no key down) replaces the latched one. */
        if (a->v[ARP_HOLD] && !any_held(a)) a->count = 0;
        a->held[note >> 3] |= (uint8_t)(1u << (note & 7));
        remove_key(a, note);
        if (a->count == ARP_KEYS) {
            memmove(a->keys, a->keys + 1, ARP_KEYS - 1u);
            memmove(a->velocities, a->velocities + 1, ARP_KEYS - 1u);
            --a->count;
        }
        int fresh = a->count == 0;
        a->keys[a->count] = (uint8_t)note;
        a->velocities[a->count] = (uint8_t)el_clampi(velocity, 1, 127);
        ++a->count;
        if (fresh) {
            /* A fresh chord starts from the top, right away; the next notes
             * follow on the grid. */
            a->position = 0;
            unsigned rate = rate_ticks[a->v[ARP_RATE] < ARP_RATES ? a->v[ARP_RATE] : 2];
            a->last_grid = a->tick / rate;
            a->grid_valid = 1;
            ev = fire(a);
        }
        return ev;
    }
    a->held[note >> 3] &= (uint8_t)~(1u << (note & 7));
    if (!a->v[ARP_HOLD]) remove_key(a, note);
    if (!a->count && a->sounding >= 0) { ev.off = a->sounding; a->sounding = -1; }
    return ev;
}

EL_DSP_CODE int arp_clear(arp_t *a)
{
    int note = a->sounding;
    a->count = 0;
    a->sounding = -1;
    a->gate_left = 0;
    a->grid_valid = 0;
    return note;
}

EL_DSP_CODE arp_events_t arp_set(arp_t *a, unsigned param, int value)
{
    arp_events_t ev = none;
    if (param >= ARP_PARAMS) return ev;
    a->v[param] = (uint8_t)el_clampi(value, param == ARP_GATE ? 5 : 0, arp_param_max(param));
    if (param == ARP_ON && !a->v[ARP_ON]) {
        ev.off = (int16_t)arp_clear(a);
        memset(a->held, 0, sizeof a->held);
    } else if (param == ARP_HOLD && !a->v[ARP_HOLD]) {
        for (int i = (int)a->count - 1; i >= 0; --i)
            if (!held(a, a->keys[i])) remove_key(a, a->keys[i]);
        if (!a->count && a->sounding >= 0) { ev.off = a->sounding; a->sounding = -1; }
    }
    return ev;
}

EL_DSP_CODE arp_events_t arp_sync(arp_t *a, uint32_t step, float sixteenth_frames)
{
    a->tick = step * ARP_TICKS_PER_16TH;
    a->sub = 0;
    a->tick_frames = sixteenth_frames / ARP_TICKS_PER_16TH;
    a->countdown = a->tick_frames;
    return on_tick(a);
}

EL_DSP_CODE int arp_frames_until(const arp_t *a, int limit)
{
    if (!a->v[ARP_ON]) return limit;
    float f = a->countdown;
    if (a->sounding >= 0 && a->gate_left > 0 && a->gate_left < f) f = a->gate_left;
    if (f >= (float)limit) return limit;
    int n = (int)f;
    if ((float)n < f) ++n;
    return n < 1 ? 1 : n;
}

EL_DSP_CODE arp_events_t arp_advance(arp_t *a, int frames, int running, float bpm)
{
    arp_events_t ev = none;
    if (!a->v[ARP_ON]) return ev;
    if (a->sounding >= 0 && a->gate_left > 0) {
        a->gate_left -= (float)frames;
        if (a->gate_left <= 0.5f) { ev.off = a->sounding; a->sounding = -1; a->gate_left = 0; }
    }
    if (!running) {
        /* Free-running at the set tempo while the transport is stopped. */
        a->tick_frames = EL_SAMPLE_RATE * 60.0f / ((bpm > 20.0f ? bpm : 20.0f) * 24.0f);
        if (a->countdown > 2.0f * a->tick_frames) a->countdown = a->tick_frames;
    }
    a->countdown -= (float)frames;
    if (a->countdown > 0.5f) return ev;
    if (running && a->sub >= ARP_TICKS_PER_16TH - 1) {
        a->countdown = 1e9f;      /* the next tick is the transport's 16th */
        return ev;
    }
    ++a->tick;
    a->sub = (uint8_t)((a->sub + 1u) % ARP_TICKS_PER_16TH);
    a->countdown += a->tick_frames;
    if (a->countdown < 1.0f) a->countdown = a->tick_frames;
    arp_events_t t = on_tick(a);
    if (t.on >= 0) {
        /* A gate that ended this very frame was the sounding note. */
        if (ev.off < 0) ev.off = t.off;
        ev.on = t.on;
        ev.velocity = t.velocity;
    }
    return ev;
}
