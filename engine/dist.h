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
/* The samples dist_process delays the signal by (its oversampling filters),
 * kept when bypassed. The send's wet signal is that late against the dry
 * mix; engine/electro.c delays the dry buses by as much (docs/adr/0030). */
enum { DIST_LATENCY = 24 };

/* DIST_FIXED: the distortion in fixed point (own platform: its FPU is slow,
 * the float version took more than a core with the HOME sends up). */
#ifndef DIST_FIXED
#if defined(EL_PLATFORM)
#define DIST_FIXED 1
#else
#define DIST_FIXED 0
#endif
#endif

/* One channel's rings and filters; mono signals run only the left one. */
typedef struct {
    /* in and os are rings of 32 and 64, each value written twice (at i and
     * i + 32 / i + 64): the filters read them in one piece, no masking */
    float in[64], os[128], dry[32];
    float pre, tight, dcx, dcy, lp1, lp2;
#if DIST_FIXED
    /* the same in fixed point (dist.c, DIST_FIXED): the driven signal Q18,
     * the rest Q24 */
    int32_t qin[64], qos[128], qdry[32];
    int32_t qpre, qtight, qdcx, qdcy, qlp1, qlp2;
#endif
} el_dist_ch_t;

typedef struct {
    el_dist_ch_t ch[2];
    float gain, mix;
    uint8_t in_pos, os_pos, mono;
#if DIST_FIXED
    int32_t qgain, qmix;   /* Q20, Q30 (taken from gain / mix at the first call) */
    uint8_t qinit;
#endif
} el_dist_t;

/* In place. mode DIST_*, drive / tone / level / mix 0..127 (level 64 +-0 dB
 * re the mode's make-up, mix 127 = wet only). stereo 0: left only, copied
 * to right. Start from a zeroed el_dist_t with gain = 1, mono = 1. */
/* The "tightening" high-pass each type has before its drive (Hz). */
float dist_tight_hz(int mode);
/* The curves as tables (own platform; nothing elsewhere): once, before the sound starts. */
void dist_init_tables(void);
EL_DSP_CODE void dist_process(el_dist_t *d, float *left, float *right, int frames, int mode, int drive, int tone,
                              int level, int mix, int stereo);
#endif
