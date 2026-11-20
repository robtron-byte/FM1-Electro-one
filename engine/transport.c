/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "transport.h"

void el_transport_init(el_transport_t *t)
{
    t->bpm = 120.0f;
    t->swing = 50;
    t->running = 0;
    t->source = EL_CLOCK_INTERNAL;
    t->step = 0;
    t->countdown = 0.0f;
    t->since_step = 0.0f;
    t->last_length = el_transport_step_frames(t, 0);
    t->mc_period = EL_SAMPLE_RATE * 60.0f / (24.0f * t->bpm);
    t->mc_phase = t->mc_err = 0.0f;
    t->mc_rx = t->mc_fired = 0;
    t->mc_learn = 0;
}

/* Ticks the follower measures before it predicts (half a beat); how far
 * ahead of the ticks received it may fire. */
enum { EL_MC_LEARN = 12, EL_MC_LEAD = 2 };
#ifndef EL_MC_A
#define EL_MC_A 0.05f
#define EL_MC_B 0.0004f
#endif
#define EL_MC_HYST 0.2f

static void mc_sync(el_transport_t *t)
{
    t->mc_rx = t->mc_fired = 0;
    t->mc_learn = 0;
    t->mc_phase = t->mc_err = 0.0f;
    t->mc_period = EL_SAMPLE_RATE * 60.0f / (24.0f * el_clampf(t->bpm, 30.0f, 300.0f));
}

void el_transport_start(el_transport_t *t)
{
    t->step = 0;
    t->countdown = 0.0f;
    mc_sync(t);
    t->running = 1;
}

void el_transport_continue(el_transport_t *t)
{
    t->countdown = 0.0f;
    mc_sync(t);
    t->running = 1;
}

void el_transport_stop(el_transport_t *t) { t->running = 0; }

void el_transport_set_bpm(el_transport_t *t, float bpm) { t->bpm = el_clampf(bpm, 30.0f, 300.0f); }

EL_DSP_CODE float el_transport_step_frames(const el_transport_t *t, uint32_t step)
{
    float pair = 2.0f * EL_SAMPLE_RATE * 60.0f / (4.0f * t->bpm);
    float share = (float)el_clampi(t->swing, 50, 75) * 0.01f;
    return pair * ((step & 1u) ? 1.0f - share : share);
}

EL_DSP_CODE int el_transport_frames_until(const el_transport_t *t, int limit)
{
    if (!t->running) return limit;
    if (t->source == EL_CLOCK_MIDI) {
        uint32_t tick = t->mc_fired * EL_MIDI_TICKS_PER_STEP;
        /* Measuring: on the tick itself. Later: on the predicted frame, even
         * when the tick came a little early; only a follower more than
         * EL_MC_LEAD ticks behind fires at once, and none runs further ahead. */
        if (t->mc_learn < EL_MC_LEARN) return tick < t->mc_rx ? 0 : limit;
        if (tick > t->mc_rx + EL_MC_LEAD) return limit;
        if (tick + EL_MC_LEAD < t->mc_rx) return 0;
        float due = t->mc_phase + (float)((int32_t)(tick - t->mc_rx)) * t->mc_period;
        if (due <= 0.0f) return 0;
        int frames = (int)due;
        if ((float)frames < due) ++frames;
        return frames < limit ? frames : limit;
    }
    if (t->countdown <= 0.0f) return 0;
    /* A step fires on the first frame at or after its exact position. */
    int frames = (int)t->countdown;
    if ((float)frames < t->countdown) ++frames;
    return frames < limit ? frames : limit;
}

EL_DSP_CODE int el_transport_advance(el_transport_t *t, int frames, uint32_t *step)
{
    if (!t->running) return 0;
    t->since_step += (float)frames;
    if (t->source == EL_CLOCK_MIDI) {
        t->mc_phase -= (float)frames;
        if (el_transport_frames_until(t, 1) != 0) return 0;
        ++t->mc_fired;
        /* The smoothed tempo, so rolls, gates and delays scale with the clock
         * without wobbling. */
        t->last_length = t->mc_period * (float)EL_MIDI_TICKS_PER_STEP;
        t->since_step = 0.0f;
        *step = t->step++;
        return 1;
    }
    t->countdown -= (float)frames;
    if (t->countdown > 0.0f) return 0;
    *step = t->step;
    t->since_step = 0.0f;
    t->last_length = el_transport_step_frames(t, t->step);
    t->countdown += t->last_length;
    ++t->step;
    return 1;
}

EL_DSP_CODE void el_transport_midi_tick(el_transport_t *t)
{
    if (!t->running || t->source != EL_CLOCK_MIDI) return;
    /* A tick is applied at a block start: "now" (0 frames from here). The
     * step fires on the tick that starts it, so the first tick after Start
     * plays step 0 at once. */
    const float lo = EL_SAMPLE_RATE * 60.0f / (24.0f * 300.0f), hi = EL_SAMPLE_RATE * 60.0f / (24.0f * 30.0f);
    float err = -t->mc_phase;   /* > 0: the tick came later than predicted */
    if (t->mc_learn) {
        float far = 1.5f * t->mc_period, drift = 0.25f * t->mc_period;
        if (far < 0.04f * EL_SAMPLE_RATE) far = 0.04f * EL_SAMPLE_RATE;      /* (more than any jitter) */
        if (drift < 0.008f * EL_SAMPLE_RATE) drift = 0.008f * EL_SAMPLE_RATE;
        t->mc_err += 0.1f * (err - t->mc_err);
        if (err > far || err < -far || ((t->mc_learn >= EL_MC_LEARN) && (t->mc_err > drift || t->mc_err < -drift))) {
            /* Far off or drifting away (a pause in the clock, a new tempo):
             * measure afresh from this tick, from the tempo known so far. */
            t->mc_learn = 0;
        }
    }
    if (!t->mc_learn) {
        t->mc_learn = 1;
        t->mc_err = 0.0f;
        t->mc_phase = t->mc_period;
    } else {
        /* An alpha-beta filter: while young the least-squares line through
         * all ticks since the (re)sync, then fixed small gains. */
        float n = (float)(t->mc_learn + 1u), d = n * (n + 1.0f);   /* (n >= 2: d never 0) */
        float a = 2.0f * (2.0f * n - 1.0f) / d, b = 6.0f / d;
        if (a < EL_MC_A) a = EL_MC_A;
        if (b < EL_MC_B) b = EL_MC_B;
        t->mc_period = el_clampf(t->mc_period + b * err, lo, hi);
        t->mc_phase += t->mc_period + a * err;
        if (t->mc_learn < 60000u) ++t->mc_learn;
    }
    ++t->mc_rx;
    if (t->mc_learn >= 7u) {
        /* The tempo shown and used by delays and rolls moves only when the
         * clock really changed, not with the last bit of jitter. */
        float bpm = el_clampf(EL_SAMPLE_RATE * 60.0f / (24.0f * t->mc_period), 30.0f, 300.0f);
        float d = bpm - t->bpm, hyst = 0.002f * bpm;
        if (hyst < EL_MC_HYST) hyst = EL_MC_HYST;
        if (t->mc_learn < EL_MC_LEARN || d > hyst || d < -hyst) t->bpm = bpm;
    }
}
