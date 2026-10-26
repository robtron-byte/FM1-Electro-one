/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "mixbus.h"
#include "dsp_math.h"
#include <string.h>

#ifndef MB_KICK_GAIN
#define MB_KICK_GAIN 1.5f
#endif

/* ---- building blocks ----------------------------------------------------- */

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
    c->a_coef = coef(attack_ms);
    c->r_coef = coef(release_ms);
    c->makeup = dsp_exp2(makeup_db / 6.0206f);
    c->env_db = -120.0f;
    c->gain_db = 0;
}

/* Feed-forward, log domain, soft knee; detector on the stereo peak. Returns
 * the gain to apply (linear). */
static inline float comp_gain(mb_comp_t *c, float level)
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

static inline float bq(mb_biquad_t *b, int ch, float x)
{
    float y = b->b0 * x + b->z1[ch];
    b->z1[ch] = b->b1 * x - b->a1 * y + b->z2[ch];
    b->z2[ch] = b->b2 * x - b->a2 * y;
    return y;
}

/* Tape-style: symmetric soft clip, odd harmonics, rounds the peaks. */
static inline float tape(float x, float drive)
{
    float y = x * drive;
    y = dsp_clamp(y, -3.0f, 3.0f);
    return y * (27.0f + y * y) / (27.0f + 9.0f * y * y) / drive;
}

/* Tube-style (in the spirit of a Kultube on the master): a little even
 * harmonic content from an asymmetric curve, DC removed afterwards. */
static inline float tube(float x, float drive)
{
    float y = x * drive;
    y = y + 0.12f * y * y;
    y = dsp_clamp(y, -3.0f, 3.0f);
    return y * (27.0f + y * y) / (27.0f + 9.0f * y * y) / drive;
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
    float reduction = 0;
    for (int n = 0; n < frames; ++n) {
        float dl = drum_l[n], dr = drum_r[n], bl = bass_l[n], br = bass_r[n], sl = synth_l[n], sr = synth_r[n];
        if (mb->enabled) {
            /* Drum bus: tilt EQ, compressor, tape saturation. */
            dl = bq(&mb->drum_low, 0, dl); dr = bq(&mb->drum_low, 1, dr);
            dl = bq(&mb->drum_high, 0, dl); dr = bq(&mb->drum_high, 1, dr);
            float g = comp_gain(&mb->drum_comp, dsp_max(dsp_abs(dl), dsp_abs(dr)));
            dl = tape(dl * g, mb->drum_drive);
            dr = tape(dr * g, mb->drum_drive);
            /* Kick-keyed ducking: the bass makes room for the kick, pads breathe. */
            float k = dsp_abs(kick[n]);
            mb->duck_env += (k - mb->duck_env) * (k > mb->duck_env ? mb->duck_attack : mb->duck_release);
            float duck = dsp_clamp(mb->duck_env * 2.5f, 0.0f, 1.0f);
            float bass_gain = 1.0f - 0.55f * duck, synth_gain = 1.0f - 0.25f * duck;
            bl = bq(&mb->bass_hp, 0, bl) * bass_gain;
            br = bq(&mb->bass_hp, 1, br) * bass_gain;
            sl = bq(&mb->synth_presence, 0, bq(&mb->synth_hp, 0, sl)) * synth_gain;
            sr = bq(&mb->synth_presence, 1, bq(&mb->synth_hp, 1, sr)) * synth_gain;
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
            l = bq(&mb->master_high, 0, bq(&mb->master_low, 0, l));
            r = bq(&mb->master_high, 1, bq(&mb->master_low, 1, r));
            float g = comp_gain(&mb->master_comp, dsp_max(dsp_abs(l), dsp_abs(r)));
            reduction = dsp_min(reduction, mb->master_comp.gain_db);
            l = tube(l * g, mb->master_drive);
            r = tube(r * g, mb->master_drive);
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
        float target = peak > lim->ceiling ? lim->ceiling / peak : 1.0f;
        if (target < lim->gain) lim->gain += (target - lim->gain) * 0.25f;
        else lim->gain += (target - lim->gain) * lim->release;
        float delayed_l = lim->buffer[0][lim->pos], delayed_r = lim->buffer[1][lim->pos];
        lim->buffer[0][lim->pos] = l;
        lim->buffer[1][lim->pos] = r;
        lim->pos = (lim->pos + 1u) % MB_LOOKAHEAD;
        /* Hold the lowest gain seen over the look-ahead window. */
        out_l[n] = dsp_clamp(delayed_l * lim->gain, -lim->ceiling, lim->ceiling);
        out_r[n] = dsp_clamp(delayed_r * lim->gain, -lim->ceiling, lim->ceiling);
    }
    mb->gain_reduction_db = reduction;
}
