/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P engine (engine/jx8p.c): the DSP building blocks it uses (envelope, ladder, biquad, chopped-ramp DCO
 * shape, BBD chorus, curves): the types; the code is engine/jx8p_dsp_inline.h. Taken from fm1-synths (github.com/robtron-byte/fm1-synths, commit 2a7b4fa,
 * shared/dsp/syn_dsp.h, syn_dsp_inline.h, syn_chop_inline.h; GPL-3.0-only, robtron-byte), where they were
 * moved from FM1-Electro's own engine/mks.c (commit e7b3b14). Only the prefix changed: syn_ -> jxd_, so they
 * do not meet FM1-Electro's synth.h (syn_*). Include from engine/jx8p.c only, after dsp_math.h. */
#ifndef JX8P_DSP_H
#define JX8P_DSP_H
#include "el_config.h"

enum { JXD_CONTROL = 16 };   /* frames per control step */
enum { JXD_ENV_IDLE, JXD_ENV_T1, JXD_ENV_T2, JXD_ENV_T3, JXD_ENV_SUSTAIN, JXD_ENV_RELEASE };
enum { JXD_CHORUS_SIZE = 1024 };

/* The envelope (Envelope.swift): T1 to L1 (linear), T2 to L2 and T3 to L3 (exponential), sustain at L3,
 * T4 to zero. An ADSR is T1 = attack to 1, T2 = decay to S, T3 = 0 to S (the JX-8P app's mapping). */
typedef struct { float t1, l1, t2, l2, t3, l3, t4; } jxd_env_settings_t;
typedef struct {
    uint8_t stage, hurry;     /* hurry: the release sped up (mks50: mks_t.release_hurry) */
    float value, start, target, progress, increment, curve, curve_decay;
} jxd_env_t;

/* The 24 dB zero-delay-feedback ladder (LadderFilter.swift): its four stages and per-block coefficients. */
typedef struct {
    float s1, s2, s3, s4, g, G, k;
    float G2, G3, G4, inv, den;
} jxd_ladder_t;

typedef struct { float b0, b1, b2, a1, a2; } jxd_biquad_t;

/* A chopped-ramp shape (ChopShape, jxd_chop_inline.h): ramp multiplier (0: none), up to three gates of
 * multiplier m and threshold w. gated_ramp, grid: jxd_chop_render's fast paths (grid: every edge on multiples
 * of 1 / grid, 0 none). */
typedef struct { float ramp, m0, w0, m1, w1, m2, w2; int gated_ramp; float grid; } jxd_chop_t;

/* The BBD chorus (MN3009, Chorus.swift), stereo, one per part. */
typedef struct {
    float buffer[JXD_CHORUS_SIZE];
    unsigned write;
    float phase, lp1, lp2, wl1, wr1, wl2, wr2, mix, depth;
    uint32_t hiss;
} jxd_chorus_t;
#endif
