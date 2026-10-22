/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Shared send effects: a tempo-synced ping-pong delay and an FDN reverb,
 * after the iOS apps' StereoDelay / Reverb, shrunk for the FM-1's RAM:
 * the delay runs at half rate in 16 bit (up to 0.6 s, darker repeats like
 * a BBD/tape echo), the reverb uses 4 instead of 8 lines. About 90 KB. */
#ifndef EL_FX_H
#define EL_FX_H
#include "el_config.h"

enum {
    FX_DELAY_CAP = 13312,  /* half-rate frames: 0.6 s */
    FX_REVERB_CAP = 2048,  /* per line */
    FX_DIFF_CAP = 512
};
enum fx_param { FX_DELAY_DIV, FX_DELAY_FB, FX_DELAY_TONE, FX_DELAY_RETURN,
                FX_REVERB_SIZE, FX_REVERB_DECAY, FX_REVERB_DAMP, FX_REVERB_RETURN, FX_PARAMS };

typedef struct {
    int16_t delay[2][FX_DELAY_CAP];
    float lines[4][FX_REVERB_CAP];
    float diff[2][FX_DIFF_CAP];
    uint8_t v[FX_PARAMS];
    unsigned dw, rw, fw;
    uint8_t half;
    float acc[2], lp[2], last[2], time, damp[4], lfo[4];
} el_fx_t;

void fx_init(el_fx_t *fx);
int fx_param_max(unsigned param);
const char *fx_param_name(unsigned param);
const char *fx_delay_division_name(int value);
/* Add the wet signal of the sends to left/right. */
EL_DSP_CODE void fx_render(el_fx_t *fx, const float *dly, const float *rev, float *left, float *right, int frames, float bpm);
#endif
