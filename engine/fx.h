/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Shared send effects: a tempo-synced ping-pong delay and an FDN reverb,
 * after the iOS apps' StereoDelay / Reverb, shrunk for the FM-1's RAM:
 * the delay runs at half rate in 16 bit (up to 0.6 s, darker repeats like
 * a BBD/tape echo), the reverb uses 4 instead of 8 lines. About 90 KB. */
#ifndef EL_FX_H
#define EL_FX_H
#include "el_config.h"
#include "dist.h"

enum {
    FX_DELAY_CAP = 13312,  /* half-rate frames: 0.6 s */
    FX_REVERB_CAP = 2048,  /* per line */
    FX_DIFF_CAP = 512,
    FX_GATE_CAP = 8192     /* half-rate frames: 0.37 s non-linear (gated) reverb */
};
enum fx_param { FX_DELAY_DIV, FX_DELAY_FB, FX_DELAY_TONE, FX_DELAY_RETURN,
                FX_REVERB_SIZE, FX_REVERB_DECAY, FX_REVERB_DAMP, FX_REVERB_RETURN,
                /* Shared distortion send: one instance of the apps' distortion
                 * (mono), fed by a send per area. */
                FX_DIST_MODE, FX_DIST_DRIVE, FX_DIST_TONE,
                FX_DIST_SEND_BEAT, FX_DIST_SEND_RHYTHM, FX_DIST_SEND_STRINGS, FX_DIST_SEND_BASS, FX_PARAMS };

typedef struct {
    int16_t delay[2][FX_DELAY_CAP];
    float lines[4][FX_REVERB_CAP];
    float diff[2][FX_DIFF_CAP];
    int16_t gate[FX_GATE_CAP];
    float gate_diff[2][256];
    uint8_t v[FX_PARAMS];
    unsigned dw, rw, fw;
    uint8_t half;
    float acc[2], lp[2], last[2], time, damp[4], lfo[4];
    float rev_hp[2], dly_hp[2], gate_hp[2], gate_acc, gate_lp[2];
    unsigned gw, gfw;
    uint8_t gate_half;
    /* Reverb throw: a big, long hall while held; on release its return
     * fades out quickly and the lines are cleared. */
    uint8_t throw_on, throw_clear;
    float throw_mix;
    el_dist_t dist;
    float dist_r[EL_BLOCK];
} el_fx_t;

void fx_init(el_fx_t *fx);
int fx_param_max(unsigned param);
const char *fx_param_name(unsigned param);
const char *fx_delay_division_name(int value);
/* Add the wet signal of the sends to left/right. gate is the send to the
 * non-linear reverb (claps and snares, after TC's NonLin2 that Rother uses). */
const char *fx_dist_mode_name(int value);
/* The distortion send: send (mono, frames <= EL_BLOCK) is distorted in
 * place and added to left/right. Skipped while all sends are at 0. */
EL_DSP_CODE void fx_distortion(el_fx_t *fx, float *send, float *left, float *right, int frames);
EL_DSP_CODE void fx_render(el_fx_t *fx, const float *dly, const float *rev, const float *gate,
                           float *left, float *right, int frames, float bpm);
#endif
