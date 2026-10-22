/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "transport.h"

void el_transport_init(el_transport_t *t)
{
    t->bpm = 120.0f;
    t->swing = 50;
    t->running = 0;
    t->source = EL_CLOCK_INTERNAL;
    t->midi_ticks = t->midi_pending = 0;
    t->step = 0;
    t->countdown = 0.0f;
    t->since_step = 0.0f;
    t->last_length = el_transport_step_frames(t, 0);
}

void el_transport_start(el_transport_t *t)
{
    t->step = 0;
    t->countdown = 0.0f;
    t->midi_ticks = t->midi_pending = 0;
    t->running = 1;
}

void el_transport_continue(el_transport_t *t)
{
    t->countdown = 0.0f;
    t->midi_ticks = t->midi_pending = 0;
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
    if (t->source == EL_CLOCK_MIDI) return t->midi_pending ? 0 : limit;
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
        if (frames || !t->midi_pending) return 0;
        --t->midi_pending;
        /* Follow the measured tempo so rolls and gates scale with the clock. */
        if (t->step && t->since_step > 0.0f && t->since_step < EL_SAMPLE_RATE * 2.0f) {
            t->last_length = t->since_step;
            t->bpm = el_clampf(EL_SAMPLE_RATE * 15.0f / t->since_step, 30.0f, 300.0f);
        }
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
    /* The step fires on the tick that starts it, so the first tick after
     * Start plays step 0 immediately. */
    if (t->midi_ticks == 0 && t->midi_pending < 4) ++t->midi_pending;
    if (++t->midi_ticks >= EL_MIDI_TICKS_PER_STEP) t->midi_ticks = 0;
}
