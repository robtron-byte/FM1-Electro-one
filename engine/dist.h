/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * The apps' acid distortion (Effects.swift AcidDistortion): tightening
 * high-pass, pre-emphasis, drive into a 2x oversampled clipper (OD, DIST,
 * RAT, TUBE), DC blocker, two-pole tone low-pass, dry/wet mix. 24 samples
 * of latency, kept when bypassed so switching never clicks. Used inside
 * the JP-6 sounds and as the shared DIST send effect. */
#ifndef EL_DIST_H
#define EL_DIST_H
#include "el_config.h"

enum { DIST_OFF, DIST_OD, DIST_DIST, DIST_RAT, DIST_TUBE, DIST_MODES };

/* One channel's rings and filters; mono signals run only the left one. */
typedef struct {
    float in[32], os[64], dry[32];
    float pre, tight, dcx, dcy, lp1, lp2;
} el_dist_ch_t;

typedef struct {
    el_dist_ch_t ch[2];
    float gain, mix;
    uint8_t in_pos, os_pos, mono;
} el_dist_t;

/* In place. mode DIST_*, drive / tone / level / mix 0..127 (level 64 +-0 dB
 * re the mode's make-up, mix 127 = wet only). stereo 0: left only, copied
 * to right. Start from a zeroed el_dist_t with gain = 1, mono = 1. */
EL_DSP_CODE void dist_process(el_dist_t *d, float *left, float *right, int frames, int mode, int drive, int tone,
                              int level, int mix, int stereo);
#endif
