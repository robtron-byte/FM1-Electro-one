/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: the constants of this file sit in internal SRAM; its audio
 * code is marked EL_DSP_CODE / EL_DSP_HELPER (control code runs from Flash). */
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
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
#if MB_FIXED
    c->qsync = 1;
#endif
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
#if MB_FIXED
    mb->q_init = 0;   /* the fixed states from these on the next block */
    for (unsigned i = 0; i < 2; ++i) cs[i]->qsync = 1;
#endif
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

#if MB_FIXED && MB_LITE_EQ
/* The same chain in fixed point: signals Q24, coefficients Q30, the
 * compressors' detectors in float every COMP_EVERY samples as before
 * (their gain glides in Q28), tape and tube's soft clip
 * y (27 + y^2) / (27 + 9 y^2) from a table (-3 .. 3, quadratic), the
 * limiter dividing only while it holds a peak. */
static inline EL_DSP_HELPER int32_t mq(int32_t a, int32_t b, int s) { return (int32_t)(((int64_t)a * b) >> s); }
static inline EL_DSP_HELPER int32_t mq30f(float x) { return (int32_t)(x * 1073741824.0f); }
static inline EL_DSP_HELPER int32_t mq28f(float x) { return (int32_t)(x * 268435456.0f); }
enum { MB_SOFT_TAB = 1537 };   /* -3 .. 3 in 1/256 */
static int32_t mb_soft_tab[MB_SOFT_TAB + 1];
static uint8_t mb_soft_ready;
static void mb_soft_init(void)
{
    for (int i = 0; i <= MB_SOFT_TAB; ++i) {
        float y = dsp_clamp(-3.0f + (float)i * (1.0f / 256.0f), -3.0f, 3.0f);
        mb_soft_tab[i] = (int32_t)(y * (27.0f + y * y) / (27.0f + 9.0f * y * y) * 16777216.0f);
    }
    mb_soft_ready = 1;
}
/* y (27 + y^2) / (27 + 9 y^2) of a Q24 y, clamped to +-3 (tape(), tube()) */
static inline EL_DSP_HELPER int32_t mb_soft(int32_t y)
{
    if (y <= -(3 << 24)) return mb_soft_tab[0];
    if (y >= (3 << 24)) return mb_soft_tab[MB_SOFT_TAB - 1];
    uint32_t p = (uint32_t)(y + (3 << 24)), i = p >> 16;
    int32_t f = (int32_t)((p >> 4) & 4095u), ff = (f * (f - 4096)) >> 1;   /* Q12, f (f - 1) / 2 in Q24 */
    int32_t t0 = mb_soft_tab[i], t1 = mb_soft_tab[i + 1], t2 = mb_soft_tab[i + 2];
    return t0 + (int32_t)(((int64_t)(t1 - t0) * f) >> 12) + (int32_t)(((int64_t)(t2 - 2 * t1 + t0) * ff) >> 24);
}
/* comp_gain: the detector as before, the glide in Q28 */
static inline EL_DSP_HELPER int32_t comp_gain_q(mb_comp_t *c, int32_t level24)
{
    if (c->qsync) { c->qgain = mq28f(c->gain); c->qstep = 0; c->qsync = 0; }
    if (++c->phase >= COMP_EVERY) {
        c->phase = 0;
        float target = comp_detect(c, (float)level24 * (1.0f / 16777216.0f));
        c->qstep = (mq28f(target) - c->qgain) / COMP_EVERY;
    }
    c->qgain += c->qstep;
    c->gain = (float)c->qgain * (1.0f / 268435456.0f);
    return c->qgain;
}
static inline EL_DSP_HELPER int32_t q_abs(int32_t x) { return x < 0 ? -x : x; }
static inline EL_DSP_HELPER int32_t q_max(int32_t a, int32_t b) { return a > b ? a : b; }

static EL_DSP_CODE void mb_process_fixed(el_mixbus_t *mb, const float *kick, float *drum_l, float *drum_r, float *bass_l,
                                         float *bass_r, float *synth_l, float *synth_r, float *out_l, float *out_r, int frames)
{
    static int32_t ik[EL_BLOCK], idl[EL_BLOCK], idr[EL_BLOCK], ibl[EL_BLOCK], ibr[EL_BLOCK], isl[EL_BLOCK], isr[EL_BLOCK];
    if (!mb_soft_ready) mb_soft_init();
    mb_limiter_t *lim = &mb->limiter;
    if (!mb->q_init) {   /* from the float states (start, after a sleep) */
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < 6; ++i) mb->q_lite[c][i] = (int32_t)(mb->lite[c][i] * 16777216.0f);
            mb->q_dcx[c] = (int32_t)(mb->dc_x[c] * 16777216.0f); mb->q_dcy[c] = (int32_t)(mb->dc_y[c] * 16777216.0f);
            for (int i = 0; i < MB_LOOKAHEAD; ++i) mb->q_lim[c][i] = (int32_t)(lim->buffer[c][i] * 16777216.0f);
        }
        mb->q_duck = (int32_t)(mb->duck_env * 16777216.0f);
        mb->q_lim_gain = mq30f(lim->gain);
        mb->q_init = 1;
    }
    /* in: Q24, and the input's peak (the sleep) */
    int32_t in_peak = 0;
    for (int n = 0; n < frames; ++n) {
        ik[n] = (int32_t)(kick[n] * 16777216.0f); idl[n] = (int32_t)(drum_l[n] * 16777216.0f); idr[n] = (int32_t)(drum_r[n] * 16777216.0f);
        ibl[n] = (int32_t)(bass_l[n] * 16777216.0f); ibr[n] = (int32_t)(bass_r[n] * 16777216.0f);
        isl[n] = (int32_t)(synth_l[n] * 16777216.0f); isr[n] = (int32_t)(synth_r[n] * 16777216.0f);
        int32_t a = q_max(q_max(q_max(q_abs(ik[n]), q_abs(idl[n])), q_max(q_abs(idr[n]), q_abs(ibl[n]))),
                          q_max(q_abs(ibr[n]), q_max(q_abs(isl[n]), q_abs(isr[n]))));
        in_peak = q_max(in_peak, a);
    }
    const int32_t quiet = (int32_t)(MB_QUIET * 16777216.0f) + 1;   /* (1e-6: 16 in Q24) */
    if (in_peak >= quiet) mb->sleep = 0;
    else if (mb->sleep) {
        for (int n = 0; n < frames; ++n) out_l[n] = out_r[n] = 0.0f;
        mb->gain_reduction_db = 0.0f;
        return;
    }
    const int32_t c_dl = mq30f(LITE_DRUM_LOW_C), g_dl = mq30f(LITE_DRUM_LOW_G), c_dh = mq30f(LITE_DRUM_HIGH_C), g_dh = mq30f(LITE_DRUM_HIGH_G);
    const int32_t c_shp = mq30f(LITE_SYNTH_HP_C), c_bhp = mq30f(LITE_BASS_HP_C);
    const int32_t c_ml = mq30f(LITE_MASTER_LOW_C), g_ml = mq30f(LITE_MASTER_LOW_G), c_mh = mq30f(LITE_MASTER_HIGH_C), g_mh = mq30f(LITE_MASTER_HIGH_G);
    const int32_t d_drive = mq28f(mb->drum_drive), d_inv = mq28f(1.0f / mb->drum_drive);
    const int32_t m_drive = mq28f(mb->master_drive), m_inv = mq28f(1.0f / mb->master_drive);
    const int32_t c_att = mq30f(mb->duck_attack), c_rel = mq30f(mb->duck_release), c012 = mq30f(0.12f);
    const int32_t c_dc = mq30f(0.9995f), c_kick = mq28f(MB_KICK_GAIN), c_hot = mq28f(1.19f);
    const int32_t ceil = (int32_t)(lim->ceiling * 16777216.0f), c_lrel = mq30f(lim->release), c_quarter = mq30f(0.25f);
    const int32_t c055 = mq30f(0.55f), c025 = mq30f(0.25f), c25 = mq28f(2.5f);
    int32_t *ql = mb->q_lite[0], *qr = mb->q_lite[1];
    float reduction = 0;
    int32_t out_peak = 0;
    for (int n = 0; n < frames; ++n) {
        int32_t dl = idl[n], dr = idr[n], bl = ibl[n], br = ibr[n], sl = isl[n], sr = isr[n], l, r;
        if (mb->enabled) {
            /* drum bus: tilt EQ, compressor, tape */
            ql[0] += mq(dl - ql[0], c_dl, 30); qr[0] += mq(dr - qr[0], c_dl, 30);
            ql[1] += mq(dl - ql[1], c_dh, 30); qr[1] += mq(dr - qr[1], c_dh, 30);
            dl += mq(ql[0], g_dl, 30) + mq(dl - ql[1], g_dh, 30);
            dr += mq(qr[0], g_dl, 30) + mq(dr - qr[1], g_dh, 30);
            int32_t g = comp_gain_q(&mb->drum_comp, q_max(q_abs(dl), q_abs(dr)));
            dl = mq(mb_soft(mq(mq(dl, g, 28), d_drive, 28)), d_inv, 28);
            dr = mq(mb_soft(mq(mq(dr, g, 28), d_drive, 28)), d_inv, 28);
            /* kick-keyed ducking */
            int32_t k = q_abs(ik[n]);
            mb->q_duck += mq(k - mb->q_duck, k > mb->q_duck ? c_att : c_rel, 30);
            int32_t duck = mq(mb->q_duck, c25, 28);
            duck = duck < 0 ? 0 : duck > (1 << 24) ? (1 << 24) : duck;   /* Q24 */
            int32_t bass_gain = (1 << 24) - mq(duck, c055, 30), synth_gain = (1 << 24) - mq(duck, c025, 30);
            ql[3] += mq(bl - ql[3], c_bhp, 30);
            bl = br = mq(bl - ql[3], bass_gain, 24);
            ql[2] += mq(sl - ql[2], c_shp, 30); qr[2] += mq(sr - qr[2], c_shp, 30);
            sl = mq(sl - ql[2], synth_gain, 24);
            sr = mq(sr - qr[2], synth_gain, 24);
        }
#ifdef MB_KICK_THROUGH_MASTER
        l = dl + bl + sl + ik[n]; r = dr + br + sr + ik[n];
#else
        l = dl + bl + sl; r = dr + br + sr;
        if (!mb->enabled) { l += ik[n]; r += ik[n]; }
#endif
        if (mb->enabled) {
            ql[4] += mq(l - ql[4], c_ml, 30); qr[4] += mq(r - qr[4], c_ml, 30);
            ql[5] += mq(l - ql[5], c_mh, 30); qr[5] += mq(r - qr[5], c_mh, 30);
            l += mq(ql[4], g_ml, 30) + mq(l - ql[5], g_mh, 30);
            r += mq(qr[4], g_ml, 30) + mq(r - qr[5], g_mh, 30);
            int32_t g = comp_gain_q(&mb->master_comp, q_max(q_abs(l), q_abs(r)));
            reduction = dsp_min(reduction, mb->master_comp.gain_db);
            /* tube: y + 0.12 y^2, then the soft clip */
            int32_t yl = mq(mq(l, g, 28), m_drive, 28), yr = mq(mq(r, g, 28), m_drive, 28);
            yl += mq(mq(yl, yl, 24), c012, 30); yr += mq(mq(yr, yr, 24), c012, 30);
            l = mq(mb_soft(yl), m_inv, 28); r = mq(mb_soft(yr), m_inv, 28);
#ifndef MB_KICK_THROUGH_MASTER
            l += mq(ik[n], c_kick, 28); r += mq(ik[n], c_kick, 28);
#endif
            int32_t ol = l - mb->q_dcx[0] + mq(mb->q_dcy[0], c_dc, 30); mb->q_dcx[0] = l; mb->q_dcy[0] = ol; l = ol;
            int32_t orr = r - mb->q_dcx[1] + mq(mb->q_dcy[1], c_dc, 30); mb->q_dcx[1] = r; mb->q_dcy[1] = orr; r = orr;
            l = mq(l, c_hot, 28); r = mq(r, c_hot, 28);
        }
        /* look-ahead limiter: ceiling / peak only while a peak is over it */
        int32_t peak = q_max(q_abs(l), q_abs(r)), target = 1 << 30;
        if (peak > ceil) {
            int sh = __builtin_clz((uint32_t)peak) - 16;   /* both to 16 bits, then one 32-bit division */
            int32_t pn = sh >= 0 ? peak << sh : peak >> -sh, cn = sh >= 0 ? ceil << sh : ceil >> -sh;
            target = (int32_t)(((uint32_t)cn << 15) / (uint32_t)pn) << 15;   /* Q15 -> Q30 */
        }
        mb->q_lim_gain += mq(target - mb->q_lim_gain, target < mb->q_lim_gain ? c_quarter : c_lrel, 30);
        int32_t dlv = mb->q_lim[0][lim->pos], drv = mb->q_lim[1][lim->pos];
        mb->q_lim[0][lim->pos] = l; mb->q_lim[1][lim->pos] = r;
        if (++lim->pos >= MB_LOOKAHEAD) lim->pos = 0;
        int32_t ol = mq(dlv, mb->q_lim_gain, 30), orr = mq(drv, mb->q_lim_gain, 30);
        ol = ol > ceil ? ceil : ol < -ceil ? -ceil : ol;
        orr = orr > ceil ? ceil : orr < -ceil ? -ceil : orr;
        out_peak = q_max(out_peak, q_max(q_abs(ol), q_abs(orr)));
        out_l[n] = (float)ol * (1.0f / 16777216.0f);
        out_r[n] = (float)orr * (1.0f / 16777216.0f);
    }
    lim->gain = (float)mb->q_lim_gain * (1.0f / 1073741824.0f);
    mb->duck_env = (float)mb->q_duck * (1.0f / 16777216.0f);
    mb->gain_reduction_db = reduction;
    if (in_peak < quiet && out_peak < quiet) {
        mb->quiet += (uint32_t)frames;
        if (mb->quiet >= MB_SLEEP_FRAMES) { mb_settle(mb); mb->sleep = 1; mb->quiet = 0; }
    } else
        mb->quiet = 0;
}
#endif

EL_DSP_CODE void mb_process(el_mixbus_t *mb, const float *kick,
                            float *drum_l, float *drum_r, float *bass_l, float *bass_r,
                            float *synth_l, float *synth_r, float *out_l, float *out_r, int frames)
{
#if MB_FIXED && MB_LITE_EQ
    mb_process_fixed(mb, kick, drum_l, drum_r, bass_l, bass_r, synth_l, synth_r, out_l, out_r, frames);
    return;
#endif
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
