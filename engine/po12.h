/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * PO-12 style percussion: one generic digital drum voice (oscillator with
 * pitch envelope and FM, filtered noise with its own envelope and clap
 * bursts, gate, drive, sample-and-hold "lo-fi"), and 16 parameter sets
 * fitted to recordings of a Teenage Engineering PO-12 rhythm at its
 * default knob settings (host/calibration/fit_po12.py). Resynthesis, no
 * samples. A and B act like the PO-12's knobs: A pitch, B length. */
#ifndef EL_PO12_H
#define EL_PO12_H
#include "el_config.h"

enum { PO_BD, PO_SD, PO_CH, PO_OH, PO_SYNSD, PO_STICKS, PO_CY, PO_NOISE, PO_CLAP, PO_CLICK,
       PO_LOTOM, PO_HITOM, PO_COWBELL, PO_BLIP, PO_TONE, PO_BASSTONE, PO_SOUNDS };

/* Parameters of one sound (floats so the fitter can set them). */
enum {
    POP_WAVE,       /* 0 sine, 1 triangle, 2 square/pulse, 3 saw */
    POP_F_START, POP_F_END, POP_PITCH_MS,   /* Hz, Hz, exponential glide time constant */
    POP_OSC_LEVEL, POP_OSC_ATTACK, POP_OSC_HOLD, POP_OSC_DECAY, /* level, ms, ms, ms (tau) */
    POP_NOISE_LEVEL, POP_NOISE_MODE, POP_NOISE_HZ, POP_NOISE_Q,   /* mode 0 LP, 1 BP, 2 HP */
    POP_NOISE_ATTACK, POP_NOISE_DECAY,      /* ms (rise), ms (tau) */
    POP_BURSTS, POP_BURST_MS,               /* clap: extra bursts and their spacing */
    POP_FM_RATIO, POP_FM_INDEX,             /* sine modulator at f * ratio */
    POP_DRIVE, POP_GATE_MS,                 /* tanh drive (0 = clean), hard end (0 = none) */
    POP_CRUSH_HZ, POP_PULSE_WIDTH,          /* sample-and-hold rate (0 = off), square duty */
    POP_OUT_LP_HZ, POP_GAIN,                /* output low-pass, output gain */
    POP_OSC_FILTER,                         /* 0..1: oscillator through the noise filter */
    POP_PARAMS
};

extern float po_sounds[PO_SOUNDS][POP_PARAMS];
extern const char *const po_sound_names[PO_SOUNDS];

typedef struct {
    uint8_t active, sound, choke;   /* choke: fade out now (closed hat stops open hat) */
    uint32_t age, rng;
    float phase, mphase, pitch, a_env, n_env, n_rise, hold, fade;
    float s1, s2, lp, crush_phase, crush_val, a, b, velocity;
} po_voice_t;

/* a, b: the knobs, 0..1 with 0.5 = as recorded (a: pitch /2..x2, b: length /4..x4). */
EL_DSP_CODE void po_trigger(po_voice_t *v, unsigned sound, float velocity, float a, float b);
EL_DSP_CODE float po_tick(po_voice_t *v);
#endif
