/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Shared 16th-note clock for drums, step sequencer and arpeggiator.
 * Audio-owned: only the audio interrupt calls these functions. */
#ifndef EL_TRANSPORT_H
#define EL_TRANSPORT_H
#include "el_config.h"

enum { EL_CLOCK_INTERNAL, EL_CLOCK_MIDI };
enum { EL_MIDI_TICKS_PER_STEP = 6 }; /* 24 PPQN / 4 */

typedef struct {
    float bpm;          /* 30..300 */
    uint8_t swing;      /* 50..75: percentage of a 16th pair taken by the even step */
    uint8_t running;
    uint8_t source;     /* EL_CLOCK_INTERNAL or EL_CLOCK_MIDI */
    uint32_t step;      /* absolute 16th counter of the next step to fire */
    float countdown;    /* frames until the next step (internal clock) */
    float last_length;  /* length of the step that fired last, in frames */
    float since_step;   /* frames since the last step */
    /* MIDI clock follower (a second-order loop, "flywheel"): ticks reach the
     * engine only at block starts and after the main loop, so a single tick
     * time is off by up to a block or more. The loop smooths period and phase
     * over many ticks; steps fire on the predicted frame, but never more than
     * a few ticks ahead of the ticks received. */
    float mc_period;    /* smoothed tick period, frames */
    float mc_phase;     /* frames until the predicted time of tick mc_rx */
    float mc_err;       /* mean of the recent prediction errors (a drift detector) */
    uint32_t mc_rx;     /* ticks received since Start/Continue */
    uint32_t mc_fired;  /* steps fired since Start/Continue (step k: tick 6k) */
    uint16_t mc_learn;  /* ticks since the last (re)sync (0: none yet) */
} el_transport_t;

void el_transport_init(el_transport_t *t);
/* Start at step 0 on the next frame. Stop keeps the position for Continue. */
void el_transport_start(el_transport_t *t);
void el_transport_stop(el_transport_t *t);
void el_transport_continue(el_transport_t *t);
void el_transport_set_bpm(el_transport_t *t, float bpm);

/* Length of a given 16th step in frames, including swing. */
EL_DSP_CODE float el_transport_step_frames(const el_transport_t *t, uint32_t step);
/* Frames until the next step fires; returns limit when none is due inside it. */
EL_DSP_CODE int el_transport_frames_until(const el_transport_t *t, int limit);
/* Advance by frames. Returns 1 when a step fires at the end of this span and
 * stores its index in *step. Call with the value from frames_until. */
EL_DSP_CODE int el_transport_advance(el_transport_t *t, int frames, uint32_t *step);

/* MIDI clock (F8) received; Start/Continue/Stop map to the calls above. */
EL_DSP_CODE void el_transport_midi_tick(el_transport_t *t);
#endif
