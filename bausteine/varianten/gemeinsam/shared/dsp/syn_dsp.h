/* SPDX-License-Identifier: GPL-3.0-only
 * fm1-synths: the state of the DSP building blocks every synth shares (the types only; the code is in
 * syn_dsp_inline.h, which one engine file per synth includes).
 *
 * From the MKS-50 firmware's engine/mks.c and mks.h (14.11.2026), there FM1-Electro's
 * (github.com/robtron-byte/FM1-Electro-one, commit e7b3b14), a port of the MKS-50 iOS app's DSP
 * (github.com/robtron-byte/Mks50-iOS-). The JX-8P and JP-6 iOS apps carry the same Envelope.swift,
 * LadderFilter.swift, Chorus.swift and LFOShape.swift (only comments differ), so these blocks are theirs too. */
#ifndef SYN_DSP_H
#define SYN_DSP_H
#include "syn_config.h"

enum { SYN_CONTROL = 16 };   /* frames per control step */
enum { SYN_ENV_IDLE, SYN_ENV_T1, SYN_ENV_T2, SYN_ENV_T3, SYN_ENV_SUSTAIN, SYN_ENV_RELEASE };
enum { SYN_CHORUS_SIZE = 1024 };

/* The envelope (Envelope.swift): T1 to L1 (linear), T2 to L2 and T3 to L3 (exponential), sustain at L3,
 * T4 to zero. An ADSR is T1 = attack to 1, T2 = decay to S, T3 = 0 to S (the JX-8P app's mapping). */
typedef struct { float t1, l1, t2, l2, t3, l3, t4; } syn_env_settings_t;
typedef struct {
    uint8_t stage, hurry;     /* hurry: the release sped up (mks50: mks_t.release_hurry) */
    float value, start, target, progress, increment, curve, curve_decay;
} syn_env_t;

/* The 24 dB zero-delay-feedback ladder (LadderFilter.swift): its four stages and per-block coefficients. */
typedef struct {
    float s1, s2, s3, s4, g, G, k;
    float G2, G3, G4, inv, den;
} syn_ladder_t;

typedef struct { float b0, b1, b2, a1, a2; } syn_biquad_t;

/* A chopped-ramp shape (ChopShape, syn_chop_inline.h): ramp multiplier (0: none), up to three gates of
 * multiplier m and threshold w. gated_ramp, grid: syn_chop_render's fast paths (grid: every edge on multiples
 * of 1 / grid, 0 none). */
typedef struct { float ramp, m0, w0, m1, w1, m2, w2; int gated_ramp; float grid; } syn_chop_t;

/* The BBD chorus (MN3009, Chorus.swift), stereo, one per part. */
typedef struct {
    float buffer[SYN_CHORUS_SIZE];
    unsigned write;
    float phase, lp1, lp2, wl1, wr1, wl2, wr2, mix, depth;
    uint32_t hiss;
} syn_chorus_t;
#endif
