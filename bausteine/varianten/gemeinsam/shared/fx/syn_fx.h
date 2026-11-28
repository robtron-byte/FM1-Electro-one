/* SPDX-License-Identifier: GPL-3.0-only
 * Delay and reverb every fm1-synths firmware shares (until 14.11.2026 the MKS-50 firmware's engine/mks_fx.h):
 * a port of the user's MKS-50 iOS app's effects (github.com/robtron-byte/Mks50-iOS-, commit 080db22,
 * Shared/DSP/Effects.swift: StereoDelay, Reverb; the parameter curves from Shared/DSP/SynthEngine.swift).
 * The JX-8P and JP-6 iOS apps carry the same Effects.swift unchanged. One of each for the whole instrument,
 * fed by every part's send (mks50/docs/adr/0009).
 *
 * What differs from the app, for the FM-1's 330 KB of buffer memory (mks50/docs/adr/0009):
 *   delay   0.70 s at most (the app: 2 s), its lines stored as 16-bit samples
 *   reverb  8 lines of 4096 samples (the app: 32768): SPACE at the largest sizes keeps its longest lines
 *           at 4032 samples; its input (pre-delay, early reflections) stored as 16-bit samples */
#ifndef SYN_FX_H
#define SYN_FX_H
#include "syn_config.h"

enum syn_fx_param {
    FX_DLY_TIME, FX_DLY_FB, FX_DLY_TONE, FX_DLY_LEVEL, FX_DLY_MOD, FX_DLY_PING, FX_DLY_SYNC, FX_DLY_DIV,
    FX_REV_SIZE, FX_REV_DECAY, FX_REV_DAMP, FX_REV_LEVEL, FX_REV_MOD, FX_REV_TYPE,
    FX_COUNT
};
enum { FX_DIVS = 12, FX_REV_TYPES = 3 };

void syn_fx_init(void);
/* audio side */
EL_DSP_CODE void syn_fx_set(unsigned param, int value);
/* the two sends (mono) of n frames in, the wet signal added to out_l / out_r */
EL_DSP_CODE void syn_fx_process(const float *dly_in, const float *rev_in, float *out_l, float *out_r, int n, float bpm);
/* display */
int syn_fx_max(unsigned param);
int syn_fx_default(unsigned param);
const char *syn_fx_value_name(unsigned param, int value);   /* NULL for plain numbers */
float syn_fx_delay_seconds(int time, int sync, int div, float bpm);
#endif
