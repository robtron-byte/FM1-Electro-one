/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Mix and master processing so the box sounds finished out of the box:
 * drum bus (compressor + tape-style saturation), kick-keyed ducking of the
 * bass and pads, high-pass and presence on the synth bus, and a master chain
 * of EQ, glue compressor, tube-style saturation and a peak limiter. */
#ifndef EL_MIXBUS_H
#define EL_MIXBUS_H
#include "el_config.h"

/* MB_FIXED: the mix bus in fixed point. Off: in the emulator's strict cycle
 * model (FM1_STRICT=1, it matches the device's MB load) the float bus costs
 * MB14, the fixed one MB18 (ADR 0019, Verlauf). */
#ifndef MB_FIXED
#define MB_FIXED 0
#endif

typedef struct {
    float threshold_db, ratio, attack, release, knee_db, makeup_db; /* settings */
    float a_coef, r_coef, makeup, env_db, gain_db;                   /* state */
    float gain, gain_step;        /* detector runs every COMP_EVERY samples, gain glides between */
    uint8_t phase;
    /* CLUB (mb_master_club, docs/adr/0029): release by the program (fast
     * above the slow average, slow on it) and 30 % of the uncompressed
     * signal kept (parallel) */
    uint8_t club;
    float avg_db, avg_coef, rs_coef;
#if MB_FIXED
    int32_t qgain, qstep;         /* the glide in Q28 */
    uint8_t qsync;                /* 1: take qgain from gain (after comp_set / a sleep) */
#endif
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
    /* Sleep (docs/adr/0019): silent in and out for MB_SLEEP_FRAMES, the
     * states are cleared once and the bus is skipped until input returns. */
    uint8_t sleep;
    uint32_t quiet;
    /* MB_LITE_EQ (docs/adr/0019): one-pole states of the cheap EQ, per channel:
     * drum low / high shelf, synth high-pass, bass high-pass (mono),
     * master low / high shelf. */
    float lite[2][6];
#if MB_FIXED
    /* the same states in fixed point (mixbus.c mb_process_fixed): Q24
     * signals, the limiter's gain Q30 */
    int32_t q_lite[2][6], q_duck, q_dcx[2], q_dcy[2], q_lim[2][MB_LOOKAHEAD], q_lim_gain;
    uint8_t q_init;
#endif
} el_mixbus_t;

void mb_init(el_mixbus_t *mb);
/* Master compressor settings (the detector keeps running). ratio 1 = off. */
void mb_master_comp(el_mixbus_t *mb, float threshold_db, float ratio, float attack_ms, float release_ms, float makeup_db);
/* The master compressor as CLUB (after mb_master_comp; on = 0: as set):
 * 4:1, 10 ms attack, release 80 ms on peaks above the program's slow
 * average and 700 ms on it, 30 % parallel (docs/adr/0029). */
void mb_master_club(el_mixbus_t *mb, int on);
/* The gain a compressor's glide gives, with CLUB's parallel share. */
static inline float mb_comp_out(const mb_comp_t *c, float gain) { return c->club ? 0.3f + 0.7f * gain : gain; }
/* kick: mono kick signal (sidechain). drums / bass / synth: stereo buses.
 * Writes the finished stereo mix to out_l/out_r (fx returns already added
 * by the caller into synth). */
EL_DSP_CODE void mb_process(el_mixbus_t *mb, const float *kick,
                            float *drum_l, float *drum_r, float *bass_l, float *bass_r,
                            float *synth_l, float *synth_r, float *out_l, float *out_r, int frames);
#endif
