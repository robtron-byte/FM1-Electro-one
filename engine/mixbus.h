/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Mix and master processing so the box sounds finished out of the box:
 * drum bus (compressor + tape-style saturation), kick-keyed ducking of the
 * bass and pads, high-pass and presence on the synth bus, and a master chain
 * of EQ, glue compressor, tube-style saturation and a peak limiter. */
#ifndef EL_MIXBUS_H
#define EL_MIXBUS_H
#include "el_config.h"

typedef struct {
    float threshold_db, ratio, attack, release, knee_db, makeup_db; /* settings */
    float a_coef, r_coef, makeup, env_db, gain_db;                   /* state */
} mb_comp_t;

typedef struct { float b0, b1, b2, a1, a2, z1[2], z2[2]; } mb_biquad_t;

enum { MB_LOOKAHEAD = 48 };

typedef struct {
    float buffer[2][MB_LOOKAHEAD];
    unsigned pos;
    float gain, ceiling, release;
} mb_limiter_t;

typedef struct {
    mb_comp_t drum_comp, master_comp;
    mb_biquad_t drum_low, drum_high;      /* drum bus tilt */
    mb_biquad_t synth_hp, synth_presence; /* synth bus */
    mb_biquad_t bass_hp;                  /* keeps the sub below the kick clean */
    mb_biquad_t master_low, master_high;
    mb_limiter_t limiter;
    float duck_env, duck_attack, duck_release;
    float drum_drive, master_drive, dc_x[2], dc_y[2];
    uint8_t enabled;
    float gain_reduction_db;   /* for the display */
} el_mixbus_t;

void mb_init(el_mixbus_t *mb);
/* Master compressor settings (the detector keeps running). ratio 1 = off. */
void mb_master_comp(el_mixbus_t *mb, float threshold_db, float ratio, float attack_ms, float release_ms, float makeup_db);
/* kick: mono kick signal (sidechain). drums / bass / synth: stereo buses.
 * Writes the finished stereo mix to out_l/out_r (fx returns already added
 * by the caller into synth). */
EL_DSP_CODE void mb_process(el_mixbus_t *mb, const float *kick,
                            float *drum_l, float *drum_r, float *bass_l, float *bass_r,
                            float *synth_l, float *synth_r, float *out_l, float *out_r, int frames);
#endif
