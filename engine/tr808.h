/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Circuit-informed Roland TR-808 voices. Values follow the TR-808 Service
 * Notes (1981), K. J. Werner's TR-808 analyses (DAFx-14, ICMC-14, 2016
 * dissertation) and measurements of a real unit; see docs/TR808.md.
 *
 * Every drum is resonators excited by the 808's 1 ms trigger pulse whose
 * height is the accent (4..14 V), so accent changes timbre, not only level.
 * Resonator states are never reset on retrigger (no "machine gun" effect).
 * Tune / decay knobs that the original lacks are extensions; their centre
 * position is the stock circuit. */
#ifndef EL_TR808_H
#define EL_TR808_H
#include "el_config.h"
#include "fm1_param.h"

/* ZP and PU are additions: an electro zap (pitch-swept oscillator) and the
 * bare 808 trigger pulse as a sound (the click of the TRIGGER OUT). */
/* Two tom/conga circuits (low and high) of the 808's three; the middle one
 * is left out to keep the kit lean. */
enum { D8_BD, D8_SD, D8_LT, D8_HT, D8_RS, D8_CP, D8_CB, D8_CY, D8_OH, D8_CH, D8_ZP, D8_PU, D8_NUM, D8_KIT = D8_NUM };
enum { TR_PARAMS = 9, TR_KIT_PARAMS = 3, CY_CAL = 18, SD_CAL = 11 };
extern float tr808_cy_cal[CY_CAL];   /* cymbal calibration (see tr808.c) */
extern float tr808_sd_cal[SD_CAL];   /* snare calibration (see tr808.c) */

typedef struct { float ic1, ic2, g, k, a1, a2, a3; } tr_svf_t;

typedef struct {
    uint8_t active;
    uint32_t age, rng;
    float pulse, pulse_lp, volts, accent;  /* trigger pulse (normalised volts) */
    float env, env2, env3, aux, aux2, pitch;
    float out_lp, dc_x, dc_y, pink[3];
    float lag1, lag2, lp1, hp1;          /* cymbal: envelope lags, tone pole, 1-pole HP */
    tr_svf_t r1, r2, r3;
} tr_voice_t;

typedef struct {
    uint8_t pot[D8_NUM][TR_PARAMS];
    uint8_t kit[TR_KIT_PARAMS];             /* level, accent, choke */
    tr_voice_t v[D8_NUM];
    float metal_phase[6];                    /* shared six-oscillator metal source */
    tr_svf_t metal_bp1, metal_bp2;           /* its 3.44 kHz and 7.1 kHz band-passes */
    uint32_t noise;
    float pan_l[D8_NUM], pan_r[D8_NUM];
} tr808_t;

void tr808_init(tr808_t *t);
int tr808_nparams(int part);
const fm1_param_t *tr808_param(int part, int parameter);
EL_DSP_CODE void tr808_set(tr808_t *t, int part, int parameter, int value);
EL_DSP_CODE int tr808_get(const tr808_t *t, int part, int parameter);
/* velocity 0..1: 0.66 and below = normal trigger, 1 = full accent. */
EL_DSP_CODE void tr808_trigger(tr808_t *t, int part, float velocity);
EL_DSP_CODE void tr808_silence(tr808_t *t);
EL_DSP_CODE int tr808_active(const tr808_t *t);
/* Adds frames to the buses: kick on its own (mono, for the mix bus and
 * ducking), the rest of the kit in stereo, and the effect sends. */
EL_DSP_CODE void tr808_render(tr808_t *t, float *kick, float *left, float *right,
                              float *rev, float *gate, float *dly, int frames);
#endif
