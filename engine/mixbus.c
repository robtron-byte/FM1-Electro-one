/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: the constants of this file sit in internal SRAM; its audio
 * code is marked EL_DSP_CODE / EL_DSP_HELPER (control code runs from Flash). */
#if !defined(FM1_HOST_TEST) && !defined(EL_PLATFORM)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "mixbus.h"
#include "dsp_math.h"
#include <string.h>

/* MB_LITE_EQ 1 (default, docs/adr/0019): the bus EQs as one-pole shelves
 * and high-passes (about a fifth of the cost of the 14 biquads per sample
 * they replace; the presence peak at 2.5 kHz is left out); 0: the biquads. */
#ifndef MB_LITE_EQ
#define MB_LITE_EQ 1
#endif
#if MB_LITE_EQ
/* One-pole coefficients 1 - exp(-2 pi f / 44100) and shelf gains 10^(dB/20) - 1. */
#define LITE_DRUM_LOW_C 0.011334f    /* 80 Hz */
#define LITE_DRUM_LOW_G 0.188502f    /* +1.5 dB */
#define LITE_DRUM_HIGH_C 0.722339f   /* 9 kHz */
#define LITE_DRUM_HIGH_G 0.188502f
#define LITE_SYNTH_HP_C 0.022541f    /* 160 Hz */
#define LITE_BASS_HP_C 0.004549f     /* 32 Hz */
#define LITE_MASTER_LOW_C 0.008512f  /* 60 Hz */
#define LITE_MASTER_LOW_G 0.122018f  /* +1 dB */
#define LITE_MASTER_HIGH_C 0.818806f /* 12 kHz */
#define LITE_MASTER_HIGH_G 0.188502f
#endif

#ifndef MB_KICK_GAIN
#define MB_KICK_GAIN 1.5f
#endif

/* ---- building blocks ----------------------------------------------------- */

enum { COMP_EVERY = 4 };
/* Below -120 dBFS counts as silence; half a second of it (longer than every
 * filter tail, the compressors' release and the look-ahead) puts the bus to
 * sleep. */
#define MB_QUIET 1e-6f
#define MB_SLEEP_FRAMES 22050u

static float coef(float ms) { return 1.0f - dsp_exp(-1.0f / (ms * 0.001f * EL_SAMPLE_RATE)); }

static void comp_set(mb_comp_t *c, float threshold_db, float ratio, float attack_ms, float release_ms,
                     float knee_db, float makeup_db)
{
    c->threshold_db = threshold_db;
    c->ratio = ratio;
    c->attack = attack_ms;
    c->release = release_ms;
    c->knee_db = knee_db;
    c->makeup_db = makeup_db;
    c->a_coef = coef(attack_ms / COMP_EVERY);   /* per detector step */
    c->r_coef = coef(release_ms / COMP_EVERY);
    c->makeup = dsp_exp2(makeup_db / 6.0206f);
    c->env_db = -120.0f;
    c->gain_db = 0;
    c->gain = c->makeup;
    c->gain_step = 0;
    c->phase = 0;
}

/* Feed-forward, log domain, soft knee; detector on the stereo peak. Returns
 * the gain to apply (linear). */
static inline EL_DSP_HELPER float comp_detect(mb_comp_t *c, float level)
{
    float in_db = 6.0206f * dsp_log2(level + 1e-9f);
    c->env_db += (in_db - c->env_db) * (in_db > c->env_db ? c->a_coef : c->r_coef);
    float over = c->env_db - c->threshold_db, k = c->knee_db, reduce;
    if (over <= -k * 0.5f) reduce = 0;
    else if (over >= k * 0.5f) reduce = over * (1.0f - 1.0f / c->ratio);
    else { float x = over + k * 0.5f; reduce = (1.0f - 1.0f / c->ratio) * x * x / (2.0f * k); }
    c->gain_db = -reduce;
    return dsp_exp2(-reduce / 6.0206f) * c->makeup;
}

/* The detector (a log2 and an exp2) runs on every COMP_EVERY-th sample
 * with its coefficients scaled to match; the gain glides to its result
 * over the next ones. */
static inline EL_DSP_HELPER float comp_gain(mb_comp_t *c, float level)
{
    if (++c->phase >= COMP_EVERY) {
        c->phase = 0;
        float target = comp_detect(c, level);
        c->gain_step = (target - c->gain) * (1.0f / COMP_EVERY);
    }
    c->gain += c->gain_step;
    return c->gain;
}

void mb_master_comp(el_mixbus_t *mb, float threshold_db, float ratio, float attack_ms, float release_ms, float makeup_db)
{
    mb_comp_t *c = &mb->master_comp;
    float env = c->env_db, gain = c->gain_db;
    comp_set(c, threshold_db, ratio < 1.0f ? 1.0f : ratio, attack_ms, release_ms, c->knee_db, makeup_db);
    c->env_db = env;
    c->gain_db = gain;
}

static void bq_shelf(mb_biquad_t *b, float hz, float gain_db, int high)
{
    float a = dsp_exp2(gain_db / 40.0f * 3.321928f), w = 2 * DSP_PI * hz / EL_SAMPLE_RATE;
    float cw = dsp_cos(w), sw = dsp_sin(w), alpha = sw / 2 * 1.41421356f, sa = 2 * dsp_sqrt(a) * alpha;
    float s = high ? -1.0f : 1.0f, a0 = (a + 1) + s * (a - 1) * cw + sa;
    b->b0 = a * ((a + 1) - s * (a - 1) * cw + sa) / a0;
    b->b1 = 2 * s * a * ((a - 1) - s * (a + 1) * cw) / a0;
    b->b2 = a * ((a + 1) - s * (a - 1) * cw - sa) / a0;
    b->a1 = -2 * s * ((a - 1) + s * (a + 1) * cw) / a0;
    b->a2 = ((a + 1) + s * (a - 1) * cw - sa) / a0;
}

static void bq_highpass(mb_biquad_t *b, float hz, float q)
{
    float w = 2 * DSP_PI * hz / EL_SAMPLE_RATE, cw = dsp_cos(w), alpha = dsp_sin(w) / (2 * q), a0 = 1 + alpha;
    b->b0 = (1 + cw) / 2 / a0; b->b1 = -(1 + cw) / a0; b->b2 = (1 + cw) / 2 / a0;
    b->a1 = -2 * cw / a0; b->a2 = (1 - alpha) / a0;
}

static void bq_peak(mb_biquad_t *b, float hz, float q, float gain_db)
{
    float a = dsp_exp2(gain_db / 40.0f * 3.321928f), w = 2 * DSP_PI * hz / EL_SAMPLE_RATE;
    float cw = dsp_cos(w), alpha = dsp_sin(w) / (2 * q), a0 = 1 + alpha / a;
    b->b0 = (1 + alpha * a) / a0; b->b1 = -2 * cw / a0; b->b2 = (1 - alpha * a) / a0;
    b->a1 = -2 * cw / a0; b->a2 = (1 - alpha / a) / a0;
}

static inline EL_DSP_HELPER float bq(mb_biquad_t *b, int ch, float x)
{
    float y = b->b0 * x + b->z1[ch];
    b->z1[ch] = b->b1 * x - b->a1 * y + b->z2[ch];
    b->z2[ch] = b->b2 * x - b->a2 * y;
    return y;
}

/* Tape-style: symmetric soft clip, odd harmonics, rounds the peaks. */
static inline EL_DSP_HELPER float tape(float x, float drive, float inv_drive)
{
    float y = x * drive;
    y = dsp_clamp(y, -3.0f, 3.0f);
    return y * (27.0f + y * y) / (27.0f + 9.0f * y * y) * inv_drive;
}

/* Tube-style (in the spirit of a Kultube on the master): a little even
 * harmonic content from an asymmetric curve, DC removed afterwards. */
static inline EL_DSP_HELPER float tube(float x, float drive, float inv_drive)
{
    float y = x * drive;
    y = y + 0.12f * y * y;
    y = dsp_clamp(y, -3.0f, 3.0f);
    return y * (27.0f + y * y) / (27.0f + 9.0f * y * y) * inv_drive;
}

/* Every state back to silence, as after a long pause (the sleep). */
static EL_DSP_CODE void mb_settle(el_mixbus_t *mb)
{
    mb_biquad_t *bqs[] = {&mb->drum_low, &mb->drum_high, &mb->synth_hp, &mb->synth_presence,
                          &mb->bass_hp, &mb->master_low, &mb->master_high};
    for (unsigned i = 0; i < sizeof bqs / sizeof bqs[0]; ++i)
        for (int ch = 0; ch < 2; ++ch) bqs[i]->z1[ch] = bqs[i]->z2[ch] = 0.0f;
    mb_comp_t *cs[] = {&mb->drum_comp, &mb->master_comp};
    for (unsigned i = 0; i < 2; ++i) {
        cs[i]->env_db = -120.0f;
        cs[i]->gain_db = 0.0f;
        cs[i]->gain = cs[i]->makeup;
        cs[i]->gain_step = 0.0f;
    }
    for (int i = 0; i < MB_LOOKAHEAD; ++i) mb->limiter.buffer[0][i] = mb->limiter.buffer[1][i] = 0.0f;
    mb->limiter.gain = 1.0f;
    mb->duck_env = 0.0f;
    mb->dc_x[0] = mb->dc_x[1] = mb->dc_y[0] = mb->dc_y[1] = 0.0f;
    for (int i = 0; i < 6; ++i) mb->lite[0][i] = mb->lite[1][i] = 0.0f;
}

/* ---- setup ------------------------------------------------------------------ */

void mb_init(el_mixbus_t *mb)
{
    memset(mb, 0, sizeof *mb);
    mb->enabled = 1;
    /* Drum bus glue (kick bypasses it, as in Rother's 808 kicks): 2:1, 10 ms
     * attack, 100 ms release, soft knee, about 2 dB gain reduction. */
    comp_set(&mb->drum_comp, -18.0f, 2.0f, 10.0f, 100.0f, 6.0f, 2.0f);
    /* Master in the spirit of the SPL Kultube Rother runs on his master:
     * gentle ratio, 25 ms attack, medium release, about 1-3 dB reduction. */
    comp_set(&mb->master_comp, -14.0f, 1.8f, 25.0f, 220.0f, 8.0f, 4.5f);
    bq_shelf(&mb->drum_low, 80.0f, 1.5f, 0);
    bq_shelf(&mb->drum_high, 9000.0f, 1.5f, 1);
    bq_highpass(&mb->synth_hp, 160.0f, 0.707f);
    bq_peak(&mb->synth_presence, 2500.0f, 0.8f, 1.5f);
    bq_highpass(&mb->bass_hp, 32.0f, 0.707f);
    bq_shelf(&mb->master_low, 60.0f, 1.0f, 0);
    bq_shelf(&mb->master_high, 12000.0f, 1.5f, 1);
    mb->duck_attack = coef(2.0f);
    mb->duck_release = coef(90.0f);
    mb->drum_drive = 1.6f;
    mb->master_drive = 1.3f;
    mb->limiter.gain = 1.0f;
    mb->limiter.ceiling = 0.89f;   /* -1 dBFS */
    mb->limiter.release = coef(80.0f);
}

/* ---- processing ------------------------------------------------------------- */

EL_DSP_CODE void mb_process(el_mixbus_t *mb, const float *kick,
                            float *drum_l, float *drum_r, float *bass_l, float *bass_r,
                            float *synth_l, float *synth_r, float *out_l, float *out_r, int frames)
{
    mb_limiter_t *lim = &mb->limiter;
    float reduction = 0, in_peak = 0, out_peak = 0;
    for (int n = 0; n < frames; ++n)
        in_peak = dsp_max(in_peak, dsp_max(dsp_max(dsp_max(dsp_abs(kick[n]), dsp_abs(drum_l[n])), dsp_max(dsp_abs(drum_r[n]), dsp_abs(bass_l[n]))),
                                           dsp_max(dsp_abs(bass_r[n]), dsp_max(dsp_abs(synth_l[n]), dsp_abs(synth_r[n])))));
    if (in_peak >= MB_QUIET) mb->sleep = 0;
    else if (mb->sleep) {
        for (int n = 0; n < frames; ++n) out_l[n] = out_r[n] = 0.0f;
        mb->gain_reduction_db = 0.0f;
        return;
    }
    const float inv_drum = 1.0f / mb->drum_drive, inv_master = 1.0f / mb->master_drive;
    for (int n = 0; n < frames; ++n) {
        float dl = drum_l[n], dr = drum_r[n], bl = bass_l[n], br = bass_r[n], sl = synth_l[n], sr = synth_r[n];
        if (mb->enabled) {
            /* Drum bus: tilt EQ, compressor, tape saturation. */
#if MB_LITE_EQ
            float *ql = mb->lite[0], *qr = mb->lite[1];
            ql[0] += (dl - ql[0]) * LITE_DRUM_LOW_C; qr[0] += (dr - qr[0]) * LITE_DRUM_LOW_C;
            ql[1] += (dl - ql[1]) * LITE_DRUM_HIGH_C; qr[1] += (dr - qr[1]) * LITE_DRUM_HIGH_C;
            dl += LITE_DRUM_LOW_G * ql[0] + LITE_DRUM_HIGH_G * (dl - ql[1]);
            dr += LITE_DRUM_LOW_G * qr[0] + LITE_DRUM_HIGH_G * (dr - qr[1]);
#else
            dl = bq(&mb->drum_low, 0, dl); dr = bq(&mb->drum_low, 1, dr);
            dl = bq(&mb->drum_high, 0, dl); dr = bq(&mb->drum_high, 1, dr);
#endif
            float g = comp_gain(&mb->drum_comp, dsp_max(dsp_abs(dl), dsp_abs(dr)));
            dl = tape(dl * g, mb->drum_drive, inv_drum);
            dr = tape(dr * g, mb->drum_drive, inv_drum);
            /* Kick-keyed ducking: the bass makes room for the kick, pads breathe. */
            float k = dsp_abs(kick[n]);
            mb->duck_env += (k - mb->duck_env) * (k > mb->duck_env ? mb->duck_attack : mb->duck_release);
            float duck = dsp_clamp(mb->duck_env * 2.5f, 0.0f, 1.0f);
            float bass_gain = 1.0f - 0.55f * duck, synth_gain = 1.0f - 0.25f * duck;
#if MB_LITE_EQ
            /* BASS is mono here (electro.c folds it): one high-pass for both. */
            ql[3] += (bl - ql[3]) * LITE_BASS_HP_C;
            bl = br = (bl - ql[3]) * bass_gain;
            ql[2] += (sl - ql[2]) * LITE_SYNTH_HP_C; qr[2] += (sr - qr[2]) * LITE_SYNTH_HP_C;
            sl = (sl - ql[2]) * synth_gain;
            sr = (sr - qr[2]) * synth_gain;
#else
            bl = bq(&mb->bass_hp, 0, bl) * bass_gain;
            br = bq(&mb->bass_hp, 1, br) * bass_gain;
            sl = bq(&mb->synth_presence, 0, bq(&mb->synth_hp, 0, sl)) * synth_gain;
            sr = bq(&mb->synth_presence, 1, bq(&mb->synth_hp, 1, sr)) * synth_gain;
#endif
        }
        /* The kick joins after the drum bus: no compression on it. By
         * default it also goes around the master compressor and tube (they
         * squeezed its attack and pumped its tail up by 1-4 dB); with
         * MB_KICK_THROUGH_MASTER it runs through them as before. */
#ifdef MB_KICK_THROUGH_MASTER
        float l = dl + bl + sl + kick[n], r = dr + br + sr + kick[n];
#else
        float l = dl + bl + sl, r = dr + br + sr;
        if (!mb->enabled) { l += kick[n]; r += kick[n]; }
#endif
        if (mb->enabled) {
#if MB_LITE_EQ
            float *ml = mb->lite[0], *mr = mb->lite[1];
            ml[4] += (l - ml[4]) * LITE_MASTER_LOW_C; mr[4] += (r - mr[4]) * LITE_MASTER_LOW_C;
            ml[5] += (l - ml[5]) * LITE_MASTER_HIGH_C; mr[5] += (r - mr[5]) * LITE_MASTER_HIGH_C;
            l += LITE_MASTER_LOW_G * ml[4] + LITE_MASTER_HIGH_G * (l - ml[5]);
            r += LITE_MASTER_LOW_G * mr[4] + LITE_MASTER_HIGH_G * (r - mr[5]);
#else
            l = bq(&mb->master_high, 0, bq(&mb->master_low, 0, l));
            r = bq(&mb->master_high, 1, bq(&mb->master_low, 1, r));
#endif
            float g = comp_gain(&mb->master_comp, dsp_max(dsp_abs(l), dsp_abs(r)));
            reduction = dsp_min(reduction, mb->master_comp.gain_db);
            l = tube(l * g, mb->master_drive, inv_master);
            r = tube(r * g, mb->master_drive, inv_master);
#ifndef MB_KICK_THROUGH_MASTER
            l += kick[n] * MB_KICK_GAIN;    /* as loud as through the master's make-up */
            r += kick[n] * MB_KICK_GAIN;
#endif
            /* DC blocker after the asymmetric stage. */
            float yl = l - mb->dc_x[0] + 0.9995f * mb->dc_y[0];
            mb->dc_x[0] = l; mb->dc_y[0] = yl; l = yl;
            float yr = r - mb->dc_x[1] + 0.9995f * mb->dc_y[1];
            mb->dc_x[1] = r; mb->dc_y[1] = yr; r = yr;
        }
        /* Into the limiter about 1.5 dB hot: a finished, club-level master
         * (around -10 LUFS) with the peaks held at -1 dBFS. */
        if (mb->enabled) { l *= 1.19f; r *= 1.19f; }
        /* Look-ahead peak limiter: the gain reaches its target before the
         * peak leaves the 1 ms delay line. */
        float peak = dsp_max(dsp_abs(l), dsp_abs(r));
        /* Never a division by zero: the compiler computes a guarded divide
         * ahead of its test (peak > ceiling ? ceiling / peak : 1), and on the
         * FM-1 that traps at the first silent sample (docs/adr/0018). Below
         * the ceiling this is ceiling / ceiling = 1 exactly. */
        float target = lim->ceiling / dsp_max(peak, lim->ceiling);
        if (target < lim->gain) lim->gain += (target - lim->gain) * 0.25f;
        else lim->gain += (target - lim->gain) * lim->release;
        float delayed_l = lim->buffer[0][lim->pos], delayed_r = lim->buffer[1][lim->pos];
        lim->buffer[0][lim->pos] = l;
        lim->buffer[1][lim->pos] = r;
        if (++lim->pos >= MB_LOOKAHEAD) lim->pos = 0;
        /* Hold the lowest gain seen over the look-ahead window. */
        out_l[n] = dsp_clamp(delayed_l * lim->gain, -lim->ceiling, lim->ceiling);
        out_r[n] = dsp_clamp(delayed_r * lim->gain, -lim->ceiling, lim->ceiling);
        out_peak = dsp_max(out_peak, dsp_max(dsp_abs(out_l[n]), dsp_abs(out_r[n])));
    }
    mb->gain_reduction_db = reduction;
    if (in_peak < MB_QUIET && out_peak < MB_QUIET) {
        mb->quiet += (uint32_t)frames;
        if (mb->quiet >= MB_SLEEP_FRAMES) { mb_settle(mb); mb->sleep = 1; mb->quiet = 0; }
    } else
        mb->quiet = 0;
}
