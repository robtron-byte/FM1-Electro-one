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
#include "po12.h"

/* ZP and PU are additions: an electro zap (pitch-swept oscillator) and the
 * bare 808 trigger pulse as a sound (the click of the TRIGGER OUT). */
/* Two tom/conga circuits (low and high) of the 808's three; the middle one
 * is left out to keep the kit lean. */
enum { D8_BD, D8_SD, D8_LT, D8_HT, D8_RS, D8_CP, D8_CB, D8_CY, D8_OH, D8_CH, D8_ZP, D8_PU, D8_NUM, D8_KIT = D8_NUM };
enum { TR_PARAMS = 9, TR_KIT_PARAMS = 4, CY_CAL = 18, SD_CAL = 11, BD_CAL = 15, MX_CAL = 10, CP_CAL = 13, CB_CAL = 9 };
extern float tr808_mx_cal[MX_CAL];   /* cowbell, clap, rim, hats calibration */
extern float tr808_bd_cal[BD_CAL];   /* bass drum calibration (see tr808.c) */
extern float tr808_cy_cal[CY_CAL];   /* cymbal calibration (see tr808.c) */
extern float tr808_sd_cal[SD_CAL];   /* snare calibration (see tr808.c) */
extern float tr808_cp_cal[CP_CAL];   /* clap model 3 calibration (see tr808.c) */
extern float tr808_cb_cal[CB_CAL];   /* cowbell model 1 calibration (see tr808.c) */

typedef struct { float ic1, ic2, g, k, a1, a2, a3; } tr_svf_t;
/* A coefficient remembered with its argument: transcendental functions run
 * only when a knob (or the age-free argument) changes, not every sample. */
typedef struct { float x, y; } tr_memo_t;

typedef struct {
    uint8_t active;
    uint32_t age, rng;
    float pulse, pulse_lp, volts, accent;  /* trigger pulse (normalised volts) */
    float env, env2, env3, aux, aux2, pitch;
    float out_lp, dc_x, dc_y, pink[3];
    float lag1, lag2, lp1, hp1;          /* cymbal: envelope lags, tone pole, 1-pole HP */
    tr_svf_t r1, r2, r3;
    tr_memo_t memo[8];
} tr_voice_t;

typedef struct {
    uint8_t pot[D8_NUM][TR_PARAMS];
    uint8_t kit[TR_KIT_PARAMS];             /* level, accent, choke, engine (0 808, 1 PO-12) */
    tr_voice_t v[D8_NUM];
    po_voice_t po[D8_NUM];                   /* the PO-12 kit's voices */
    float metal_phase[6];                    /* shared six-oscillator metal source */
    /* Their pitch wanders: a slow drift and a new small error every cycle
     * (TR808_METAL_JITTER). metal_mod = 1 + drift + jitter. */
    float metal_drift[6], metal_jit[6], metal_mod[6];
    uint32_t metal_rng;
    int metal_count;
    tr_svf_t metal_bp1, metal_bp2;           /* its 3.44 kHz and 7.1 kHz band-passes */
    /* Open and closed hat, cowbell with their TUNE off centre: their own
     * oscillators (centred they share the bank above, as on the 808). */
    float hat_phase[2][6], cb_phase[2];
    tr_svf_t hat_bp[2];
    uint32_t noise;
    float noise_now;                         /* this frame's sample of the shared noise source */
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
