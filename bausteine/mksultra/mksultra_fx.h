/* SPDX-License-Identifier: GPL-3.0-only
 * mksultra: Oszillator (dco.rs), LFO (lfo.rs), Effekte (chorus.rs, delay.rs, reverb.rs) und Arpeggiator (arp.rs).
 * Nur von mksultra.c eingebunden. Puffer kommen aus dem Zustand (kein malloc). */
#ifndef MKSULTRA_FX_H
#define MKSULTRA_FX_H
#include "mksultra_dsp.h"

/* ---------- Ableitungen der Parameter (params.rs, Params) ---------- */
typedef struct {
    float pitch_wheel, mod_wheel, volume_target;
    int32_t porta_mode;
    float porta_time;
    int porta_time_mode, chord_orig_mono;
    int32_t saw_mode, pulse_mode;
    float pwm_rate, pwm_depth;
    int32_t sub_mode;
    float sub_level, noise, dco_lfo_mod, octave;
    int no_dc_offset;
    int32_t dco_env_mode;
    float dco_env_mod, dco_at_mod, sp_env, sp_lfo, sp_tune, sp_fine, saw_volume, pulse_volume;
    int pulse_sync;
    float dco_range, fine_tune, bender_range, mw_dco_lfo, vcf_cutoff, vcf_res, vcf_kf;
    int32_t res_boost;
    int bass_boost_freq;
    float vcf_lfo;
    int32_t vcf_env_mode;
    float vcf_env_mod, mw_vcf_range, mw_vcf_lfo, vcf_at;
    int32_t hpf_mode;
    double env_t1, env_l1, env_t2, env_l2, env_t3, env_l3, env_t4, env_t1f, env_t4f;
    float env_kf;
    int32_t vca_env_mode;
    float vca_level, vca_at, chorus_rate;
    int chorus_on;
    int32_t unison_extra;
    float unison_detune, unison_spread;
    int round_robin, arp_active;
    float pwm_lfo_out, pwm_lfo_phase, pwm_lfo_inc, pwm_lfo_rate_cache, sample_rate, aftertouch;
    float tuning[128];
    int32_t chord[6];
} prm_t;

/* ---------- dco.rs ---------- */
typedef struct {
    ring_t ring;
    float delay[16];
    int delay_idx;
    bosc_t m, x8, x2, p, ps, s1, s2x, s4x, s2;
    int32_t noise_seed;
    float lfo_sr, lfo_phase, lfo_inc, lfo_rate_cache, inv_2sr, half, inc_cache, pitch_cache, inc2_cache, pitch2_cache;
    int internal_pwm_lfo;
    float pw, pwm_lfo_out;
} dco_t;

static inline float pitch_inc(float pitch, float inv_2sr)
{
    float f = fastpow2(pitch * 0.083333336f + (-5.75f));
    float r = (f * 440.0f) * inv_2sr;
    r = r < 0.5f ? r : 0.5f;
    return r > 1e-6f ? r : 1e-6f;
}

static float pwm_lfo_shape(float ph)
{
    float x = fmodf(ph, 1.0f);
    float tri0 = fabsf((x + x) + (-1.0f));
    float tri = (tri0 + tri0) + (-1.0f);
    if (tri >= 0.0f) return tri;
    float y = ph * 6.2831855f + 1.5707964f;
    float z = ((float)f2i(y * 0.15915494f) + (y < 0.0f ? -0.5f : 0.5f)) * 6.2831855f - y;
    uint32_t sgn = signbit(z) ? 0x80000000u : 0u;
    float q = (z * -0.40528473f) * fabsf(z) + z * 1.2732395f;
    float q2 = q * q;
    float c3 = bits2f(sgn ^ 0xbb533217u);
    float c2 = bits2f(sgn + 0x3c77ce9au);
    float c1 = bits2f(sgn + 0x3e5086d7u);
    float poly = ((c3 * q2 + c2) * q2 + c1) * q2 + q * 0.78444487f;
    return tri * 0.85f + poly * 0.15f;
}

static void dco_realign(dco_t *d, float mod_sum)
{
    float p0 = d->s2.phase;
    float v = 4.0f * p0 + 0.5f;
    v = v - floorf(v);
    d->m.phase = v; d->x8.phase = v; d->x2.phase = v;
    if (mod_sum < 1e-4f) { d->p.phase = v; d->ps.phase = v; }
    d->s2x.phase = v; d->s4x.phase = v;
    float u = (p0 + p0) + 0.5f;
    d->s1.phase = u - floorf(u);
}
static void dco_set_sr(dco_t *d, float sr, float mod_sum)
{
    d->lfo_sr = sr;
    d->inv_2sr = 1.0f / (sr * 2.0f);
    d->half = 1.0f / 2.0f;
    d->pitch_cache = -1.0f;
    d->inc2_cache = -1.0f;
    dco_realign(d, mod_sum);
}
static void dco_new(dco_t *d, int internal, seeds_t *seeds)
{
    int32_t lfo_seed = next_seed(seeds);
    memset(d, 0, sizeof *d);
    bosc_new(&d->m); bosc_new(&d->x8); bosc_new(&d->x2); bosc_new(&d->p); bosc_new(&d->ps);
    bosc_new(&d->s1); bosc_new(&d->s2x); bosc_new(&d->s4x); bosc_new(&d->s2);
    d->lfo_sr = 44100.0f;
    d->lfo_phase = (float)((uint32_t)wmul(lfo_seed, 16807) & 0x7fffffffu) * 4.656613e-10f;
    d->lfo_inc = 1.0f; d->lfo_rate_cache = 1.0f; d->inv_2sr = 1.0f; d->half = 1.0f;
    d->internal_pwm_lfo = internal;
    d->noise_seed = next_seed(seeds);
    dco_set_sr(d, 44100.0f, 0.0f);
}
static inline float dco_inc_main(dco_t *d, float pitch)
{
    if (pitch == d->pitch_cache) return d->inc_cache;
    float r = pitch_inc(pitch, d->inv_2sr);
    d->pitch_cache = pitch; d->inc_cache = r;
    return r;
}
static inline float dco_inc_mod(dco_t *d, float pitch)
{
    if (pitch == d->pitch2_cache) return d->inc2_cache;
    float r = pitch_inc(pitch, d->inv_2sr);
    d->pitch2_cache = pitch; d->inc2_cache = r;
    return r;
}

static void dco_process(dco_t *d, float *out, float pitch, float mod_a, float mod_b, const prm_t *prm, const float *tab)
{
    float lvl_saw = prm->saw_volume, lvl_pulse = prm->pulse_volume, ext_lfo = prm->pwm_lfo_out, sub_lvl = prm->sub_level;
    float inc = dco_inc_main(d, pitch);
    int32_t saw_mode = prm->saw_mode;
    ring_t *ring = &d->ring;

    /* Sägezahn-Familie */
    d->m.wrapped = 0;
    d->m.inc = inc;
    d->m.phase += inc;
    float pre = d->m.phase;
    if (pre >= 1.0f) {
        for (;;) { d->m.phase += -1.0f; if (d->m.phase < 1.0f) break; }
        if (lvl_saw != 0.0f && saw_mode != 0) ring_insert(ring, tab, d->m.phase / inc, -lvl_saw);
        d->m.wrapped = 1;
    }
    float m_phase = d->m.phase, m_inc = d->m.inc;
    float a = lvl_saw * m_phase;
    int synced = pre >= 1.0f;
    float saw_out = 0.0f;
    switch (saw_mode) {
    case 1: saw_out = a; break;
    case 2: {
        float r = synced ? bosc_pulse_sync(&d->x2, ring, tab, inc + inc, 0.5f, m_phase, m_inc, a)
                         : bosc_pulse(&d->x2, ring, tab, inc + inc, 0.5f, a);
        saw_out = (r + 0.5f) * a;
        break;
    }
    case 3: {
        float pwx = d->pw * 0.9924f;
        float r = synced ? bosc_pulse_sync(&d->x2, ring, tab, inc + inc, pwx, m_phase, m_inc, a)
                         : bosc_pulse(&d->x2, ring, tab, inc + inc, pwx, a);
        saw_out = (r + 0.5f) * a;
        break;
    }
    case 4: {
        float r = synced ? bosc_pulse_sync(&d->x8, ring, tab, 8.0f * inc, 0.5f, m_phase, m_inc, a)
                         : bosc_pulse(&d->x8, ring, tab, 8.0f * inc, 0.5f, a);
        saw_out = (r + 0.5f) * a;
        break;
    }
    case 5: {
        float r = synced ? bosc_pulse_sync(&d->x2, ring, tab, inc + inc, 0.5f, m_phase, m_inc, a)
                         : bosc_pulse(&d->x2, ring, tab, inc + inc, 0.5f, a);
        float b = (r + 0.5f) * a;
        float r2 = d->m.wrapped ? bosc_pulse_sync(&d->x8, ring, tab, 8.0f * inc, 0.5f, m_phase, m_inc, b)
                                : bosc_pulse(&d->x8, ring, tab, 8.0f * inc, 0.5f, b);
        saw_out = (r2 + 0.5f) * b;
        break;
    }
    default: break;
    }

    /* Pulsbreite einmal je Periode */
    int gate = saw_mode == 3 ? d->x2.wrapped : d->p.wrapped;
    if (gate) {
        float rate = prm->pwm_rate;
        float l = ext_lfo;
        if (d->internal_pwm_lfo) {
            l = d->pwm_lfo_out;
            int upd = saw_mode == 3 ? d->x2.wrapped : d->m.wrapped;
            if (upd) {
                float q = ((20.0f * rate) * d->half) / inc;
                if (saw_mode == 3) q *= 0.5f;
                if (q != d->lfo_rate_cache) { d->lfo_rate_cache = q; d->lfo_inc = q / d->lfo_sr; }
                float ph = d->lfo_phase + d->lfo_inc;
                ph = ph < 1.0f ? ph : ph + (-1.0f);
                d->lfo_phase = ph;
                l = pwm_lfo_shape(ph);
                d->pwm_lfo_out = l;
            }
        }
        d->pw = rate == 0.0f ? prm->pwm_depth : (l * 0.5f + 0.5f) * prm->pwm_depth;
    }

    /* Sub-Familie */
    float sub_out = 0.0f;
    int32_t sub_mode = prm->sub_mode;
    if (sub_mode >= 0 && sub_mode < 6) {
        float sa = sub_lvl * 0.75f;
        switch (sub_mode) {
        case 0: case 1: {
            float pw = sub_mode == 0 ? 0.5f : 0.75f;
            float r = bosc_pulse(&d->s1, ring, tab, inc * 0.5f, pw, sa);
            sub_out = (r + 0.5f) * sa;
            bosc_silent(&d->s2, ring, tab, 0.25f * inc, 0.5f);
            break;
        }
        case 2: {
            float r1 = bosc_pulse(&d->s1, ring, tab, inc * 0.5f, 0.5f, sa);
            float u = (r1 + 0.5f) * sa;
            float r2 = d->s1.wrapped ? bosc_pulse_sync(&d->s2x, ring, tab, inc + inc, 0.5f, d->s1.phase, d->s1.inc, sa)
                                     : bosc_pulse(&d->s2x, ring, tab, inc + inc, 0.5f, sa);
            sub_out = u * (r2 + 0.5f);
            bosc_silent(&d->s2, ring, tab, 0.25f * inc, 0.5f);
            break;
        }
        case 3: {
            float r1 = bosc_pulse(&d->s1, ring, tab, inc * 0.5f, 0.5f, sa);
            float u = (r1 + 0.5f) * sa;
            float r2 = d->s1.wrapped ? bosc_pulse_sync(&d->s4x, ring, tab, 4.0f * inc, 0.5f, d->s1.phase, d->s1.inc, u)
                                     : bosc_pulse(&d->s4x, ring, tab, 4.0f * inc, 0.5f, u);
            sub_out = (r2 + 0.5f) * u;
            bosc_silent(&d->s2, ring, tab, 0.25f * inc, 0.5f);
            break;
        }
        default: {
            bosc_silent(&d->s1, ring, tab, inc * 0.5f, 0.5f);
            float pw = sub_mode == 4 ? 0.5f : 0.25f;
            float r = bosc_pulse(&d->s2, ring, tab, 0.25f * inc, pw, -sa);
            sub_out = (0.5f - r) * sa;
            break;
        }
        }
    }

    /* Puls-Familie */
    int sync = prm->pulse_sync && d->s1.wrapped;
    float md = (mod_b * prm->sp_lfo) + ((prm->sp_fine + prm->sp_tune) + mod_a * prm->sp_env);
    float inc2 = fabsf(md) > 1e-5f ? dco_inc_mod(d, md + pitch) : inc;
    float s1p = d->s1.phase, s1i = d->s1.inc;
    float pul_out = 0.0f;
    int32_t pulse_mode = prm->pulse_mode;
#define PQ(pw_, amp_) (sync ? bosc_pulse_sync(&d->p, ring, tab, inc2, (pw_), s1p, s1i, (amp_)) : bosc_pulse(&d->p, ring, tab, inc2, (pw_), (amp_)))
    switch (pulse_mode) {
    case 0: (void)PQ(0.49995f, 0.0f); break;
    case 1: { float r = PQ(0.49995f, lvl_pulse); pul_out = (r + (-0.5f)) * lvl_pulse; break; }
    case 2: { float r = PQ(0.749925f, lvl_pulse); pul_out = (r + (-0.5f)) * lvl_pulse; break; }
    case 3: { float pwp = d->pw * 0.4962f + 0.49995f; float r = PQ(pwp, lvl_pulse); pul_out = (r + (-0.5f)) * lvl_pulse; break; }
    case 4: {
        bosc_t *ps = &d->ps;
        ps->wrapped = 0;
        ps->inc = inc2;
        if (!sync) {
            ps->phase += inc2;
            if (ps->phase >= 1.0f) {
                for (;;) { ps->phase += -1.0f; if (ps->phase < 1.0f) break; }
                if (lvl_pulse != 0.0f) ring_insert(ring, tab, ps->phase / inc2, -lvl_pulse);
                ps->wrapped = 1;
            }
        } else {
            float ph = ps->phase + inc2;
            ps->phase = ph;
            float mm = s1p + 1.0f;
            for (;;) {
                mm += -1.0f;
                float ts = mm / s1i;
                for (;;) {
                    if (ph < 1.0f) break;
                    float tw = (ph + (-1.0f)) / inc2;
                    if (!(tw > ts)) break;
                    for (;;) { ph += -1.0f; if (ph < 1.0f) break; }
                    ps->phase = ph;
                    if (lvl_pulse != 0.0f) ring_insert(ring, tab, tw, -lvl_pulse);
                    ps->wrapped = 1;
                }
                if (ts < 0.0f) break;
                float p_at = ph - ts * inc2;
                float dd = ps->sync_reset - p_at;
                float s = dd * lvl_pulse;
                if (s != 0.0f) ring_insert(ring, tab, ts, s);
                ph = (ps->sync_reset - p_at) + ph;
                ps->phase = ph;
            }
        }
        pul_out = (d->ps.phase + (-1.0f)) * lvl_pulse;
        break;
    }
    default: break;
    }
#undef PQ

    /* Mischung, Rauschen, BLEP-Ring und Verzögerung des naiven Pfads */
    if (saw_mode != 0) saw_out += -0.5f;
    if (pulse_mode != 0) pul_out += 0.5f;
    d->noise_seed = wmul(d->noise_seed, 16807);
    float n = (float)d->noise_seed * 1.8626451e-8f;
    n = n > -1.0f ? n : -1.0f;
    n = n < 1.0f ? n : 1.0f;
    float g = prm->noise * 0.65f + 0.002f;
    float naive = (((sub_out + (-0.5f)) + saw_out) + pul_out) + g * n;
    float b = ring_advance_read(&d->ring);
    d->delay[d->delay_idx] = naive;
    d->delay_idx = d->delay_idx + 1 < 16 ? d->delay_idx + 1 : 0;
    float dly = d->delay[d->delay_idx];
    *out = (b + dly) + *out;
}

/* ---------- lfo.rs ---------- */
static const double SYNC_MULT[32] = {
    0.00390625, 0.004166666666666667, 0.004464285714285714, 0.004807692307692308, 0.005208333333333333,
    0.005681818181818182, 0.00625, 0.006944444444444444, 0.0078125, 0.008928571428571428, 0.010416666666666666, 0.0125,
    0.015625, 0.020833333333333332, 0.03125, 0.041666666665625, 0.046875, 0.0625, 0.08333333333125, 0.09375, 0.125,
    0.1666666666625, 0.1875, 0.25, 0.333333333325, 0.375, 0.5, 0.66666666665, 0.75, 1.0, 1.3333333333, 1.5};

typedef struct {
    float rate_hz, rate_v, out;
    int32_t wave;
    float fade, fade_inc;
    int32_t cnt;
    float sr, inv;
    int trigger;
    float delay_v, dly;
    int sync;
    float bpm;
    double phase, inc;
    float y_out;                                /* (float über double zurück zu float ist dasselbe) */
#if MKSULTRA_FM1
    uint64_t phf, incf;                         /* Phase und Schritt in Festkomma Q12.52 statt double (FM-1) */
#endif
    float osc_sr, sh;
    int32_t rng;
    smoother_t sm_a, sm_b;
    float tab[4][257];
} lfo_t;

static void lfo_set_rate(lfo_t *l, float v)
{
    l->rate_v = v;
    double r;
    if (!l->sync) {
        float hz = exp2f(v * 12.0f + (-12.0f)) * 61.0f;
        l->rate_hz = hz;
        r = (double)hz;
    } else {
        int32_t k = d2i(floor(31.0 * (double)v + 0.5));
        if (k >= 0 && k < 32) {
            float q = l->bpm * 0.06666667f;
            r = (double)q * SYNC_MULT[k];
            l->rate_hz = (float)r;
        } else {
            l->rate_hz = v;
            r = (double)v;
        }
    }
    l->inc = (r * 255.0) / (double)l->osc_sr;
#if MKSULTRA_FM1
    l->incf = (uint64_t)(l->inc * 4503599627370496.0);
#endif
}
static void lfo_new(lfo_t *l, int32_t seed)
{
    memset(l, 0, sizeof *l);
    for (int i = 0; i < 256; i++) {
        int32_t k = i % 64, v;
        switch (i / 64) { case 0: v = k; break; case 1: v = 64 - k; break; case 2: v = -k; break; default: v = k - 64; }
        l->tab[0][i] = (float)v * 0.015625f;
        l->tab[1][i] = (float)sin((double)((float)i * 0.00390625f) * 6.283185307179586);
        l->tab[2][i] = (float)i * 0.0078431377f + (-1.0f);
        l->tab[3][i] = i < 128 ? 1.0f : -1.0f;
    }
    l->tab[0][256] = 0.0f;
    l->tab[1][256] = bits2f(0xa58d3132u);
    l->tab[2][256] = -1.0f;
    l->tab[3][256] = 1.0f;
    l->rate_hz = 1.0f; l->fade_inc = 1.02f / 44100.0f; l->sr = 44100.0f; l->inv = 1.0f; l->bpm = 120.0f;
    l->inc = 0.0057954545; l->osc_sr = 44000.0f; l->rng = seed;
#if MKSULTRA_FM1
    l->phf = 0; l->incf = (uint64_t)(l->inc * 4503599627370496.0);
#endif
    sm_new(&l->sm_a, 20.0f);
    sm_new(&l->sm_b, 4.0f);
    lfo_set_rate(l, 0.0f);
}
static void lfo_set_sr(lfo_t *l, float sr)
{
    l->sr = sr;
    l->fade_inc = 1.02f * (1.0f / sr);
    l->fade = 0.0f;
    l->cnt = 0;
    l->osc_sr = sr;
    sm_set_sr(&l->sm_a, sr);
    sm_set_sr(&l->sm_b, sr);
    lfo_set_rate(l, l->rate_v);
}
static void lfo_set_delay(lfo_t *l, float v) { l->delay_v = v; l->dly = exp2f(12.8f * v + (-12.8f)); }
static void lfo_set_sync(lfo_t *l, int on) { l->sync = on; lfo_set_rate(l, l->rate_v); }
static void lfo_set_bpm(lfo_t *l, float bpm) { if (bpm != l->bpm) { l->bpm = bpm; lfo_set_rate(l, l->rate_v); } }
static void lfo_note_on(lfo_t *l, int any_held)
{
    if (l->trigger) {
        l->phase = 0.0;
#if MKSULTRA_FM1
        l->phf = 0;
#endif
        l->rng = wmul(l->rng, 16807);
        l->sh = (float)l->rng * 9.3132257e-10f;
    }
    if (!any_held) { l->fade = 0.0f; l->cnt = 0; }
}
static inline float lfo_value(const lfo_t *l) { return l->out * l->fade; }
static void lfo_tick(lfo_t *l)
{
    if (l->delay_v == 0.0f) l->fade = 1.0f;
    else if (l->fade > 1.0f) l->fade = 1.0f;
    else {
        int32_t thr = f2i((l->dly * 34.0f) * l->sr);
        int32_t c = l->cnt;
        l->cnt = (int32_t)((uint32_t)c + 1u);
        if (c >= thr) l->fade = (3.0f - l->delay_v) * l->fade_inc + l->fade;
    }
#if MKSULTRA_FM1
    /* Phase in Festkomma (Q12.52): dieselbe Rechnung wie unten, Interpolation in float; Abweichung ~1e-7 */
    const uint64_t P255 = (uint64_t)255 << 52;
    uint64_t pf0 = l->phf, pf = pf0;
    while (pf > P255) pf -= P255;
    int32_t idx = (int32_t)(pf >> 52);
    float frac = (float)(uint32_t)((pf >> 29) & 0x7FFFFFu) * (1.0f / 8388608.0f);
    l->phf = pf + l->incf;
    idx = clampi(idx, 0, 255);
    int wrapped = pf0 > P255;
#else
    double ph0 = l->phase, ph = ph0;
    if (ph > 255.0) {
        for (;;) { ph += -255.0; if (!(ph > 255.0)) break; }
    }
    int32_t idx = d2i(floor(ph));
    double frac = ph - (double)idx;
    l->phase = ph + l->inc;
    idx = clampi(idx, 0, 255);
    int wrapped = ph0 > 255.0;
#endif
    switch (l->wave) {
    case 0: case 1: case 2: case 3: {
        const float *t = l->tab[l->wave];
#if MKSULTRA_FM1
        float raw = (t[idx + 1] - t[idx]) * frac + t[idx];
#else
        double raw = ((double)t[idx + 1] - (double)t[idx]) * frac + (double)t[idx];
#endif
        l->y_out = sm_tick(&l->sm_a, (float)raw);
        break;
    }
    case 4:
        if (wrapped) { l->rng = wmul(l->rng, 16807); l->sh = (float)l->rng * 9.3132257e-10f; }
        l->y_out = sm_tick(&l->sm_a, l->sh);
        break;
    case 5: {
        l->rng = wmul(l->rng, 16807);
        float v = (float)l->rng * 4.6566129e-10f;
        l->y_out = sm_tick(&l->sm_b, v);
        break;
    }
    default: break;
    }
    l->out = l->y_out * l->inv;
}

/* ---------- chorus.rs ---------- */
#define CHORUS_CAP (2 * (MKSULTRA_MAX_SR * 53 / 10000) + 4)
typedef struct {
    float sr, delay_ms, lfo, lfo_step, dir, z;
    float *buf;
    int n, w;
    svf_t f1, f2, f3;
} chorus_t;
static void chorus_new(chorus_t *c, float *mem, float sr, float phase_arg)
{
    int64_t n2 = 2 * f2l(floorf(sr * 0.0053f));
    int n = (int)(n2 > 2 ? n2 : 2);
    if (n > CHORUS_CAP) n = CHORUS_CAP;
    svf_new(&c->f1, 1000.0f, 0.5f, SVF_LP);
    svf_new(&c->f2, 1000.0f, 0.5f, SVF_LP);
    svf_new(&c->f3, 1000.0f, 0.5f, SVF_HP);
    float g_lp = tanf(18849.557f / sr), g_hp = tanf(157.07964f / sr);
    svf_set_gk(&c->f1, g_lp, 1.847759f);
    svf_set_gk(&c->f2, g_lp, 0.76536685f);
    svf_set_gk(&c->f3, g_hp, 2.0f);
    c->sr = sr; c->delay_ms = 5.3f; c->lfo = 4.0f * phase_arg; c->lfo_step = 0.5f / sr; c->dir = 1.0f; c->z = 0.0f;
    c->buf = mem; c->n = n; c->w = n - 1;
    memset(mem, 0, sizeof(float) * (size_t)n);
}
static inline void chorus_set_rate(chorus_t *c, float rate) { c->lfo_step = rate / c->sr; }
static inline float chorus_process(chorus_t *c, float x)
{
    if (c->lfo >= 1.0f) c->dir = -1.0f;
    else if (c->lfo <= -1.0f) c->dir = 1.0f;
    c->lfo = c->dir * c->lfo_step + c->lfo;
    int64_t n = c->n;
    float d = c->delay_ms * 0.001f * c->sr * (0.3f * c->lfo + 0.4f);
    float fl = floorf(d);
    int64_t r0 = (int64_t)c->w - f2l(fl);
    if (r0 < 0) r0 += n;
    int64_t r1 = r0 - 1;
    if (r1 < 0) r1 += n;
    float frac = d - fl;
    float out = (c->buf[r0] - c->z) * (1.0f - frac) + c->buf[r1];
    c->z = out;
    float t = svf_tick(&c->f1, x);
    t = svf_tick(&c->f2, t);
    float h = 0.5f - c->lfo * 0.5f;
    float h2 = h * h;
    float gain = (h2 * h2) * (-0.2f) + 1.0f;
    c->buf[c->w] = svf_tick(&c->f3, gain * t);
    c->w = c->w + 1 < c->n ? c->w + 1 : 0;
    return out;
}

/* ---------- Speicher der Effekte (Schalter fürs FM-1; Standard = wie die Vorlage) ----------
 * MKSULTRA_DELAY_TENTHS     Delay-Puffer in Zehntelsekunden (Standard 40 = 4 s). Die Zeiten rechnen weiter mit den
 *                           4 s der Vorlage; was länger wäre als der Puffer, endet am Puffer.
 * MKSULTRA_PREDELAY_TENTHS  Vorverzögerung des Halls (Standard 10 = 1 s), ebenso begrenzt.
 * MKSULTRA_FX_INT16         Delay- und Hall-Leitungen als 16 Bit statt float (halber Speicher; Delay ±1 voll,
 *                           Hall ±8). */
#ifndef MKSULTRA_DELAY_TENTHS
#define MKSULTRA_DELAY_TENTHS 40
#endif
#ifndef MKSULTRA_PREDELAY_TENTHS
#define MKSULTRA_PREDELAY_TENTHS 10
#endif
#ifndef MKSULTRA_FX_INT16
#define MKSULTRA_FX_INT16 0
#endif
#if MKSULTRA_FX_INT16
typedef int16_t fxs_t;
static inline fxs_t fxs_put(float x, float sc)
{
    float y = x * sc;
    y = y > 32767.0f ? 32767.0f : y < -32767.0f ? -32767.0f : y;
    return (fxs_t)(y + (y >= 0.0f ? 0.5f : -0.5f));
}
#define FXS_GET(v, isc) ((float)(v) * (isc))
#define FXS_PUT(x, sc) fxs_put((x), (sc))
#else
typedef float fxs_t;
#define FXS_GET(v, isc) (v)
#define FXS_PUT(x, sc) (x)
#endif
#define DLY_SC 32767.0f
#define DLY_ISC (1.0f / 32767.0f)
#define RV_SC 4096.0f
#define RV_ISC (1.0f / 4096.0f)

/* ---------- delay.rs ---------- */
#define DELAY_CAP (MKSULTRA_MAX_SR * MKSULTRA_DELAY_TENTHS / 10 + 2)
static const uint32_t DELAY_SYNC_X[21] = {
    0x40700000, 0x401fffff, 0x40b40000, 0x40f00000, 0x409fffff, 0x41340000, 0x41700000, 0x411fffff, 0x41b40000,
    0x41f00000, 0x419fffff, 0x42340000, 0x42700000, 0x421fffff, 0x42b40000, 0x42f00000, 0x429fffff, 0x43340000,
    0x43700000, 0x431fffff, 0x43b40000};

typedef struct {
    fxs_t *buf;
    int n, w, nn;                               /* nn: Länge der Vorlage (für die Zeit), n: Puffer (<= nn) */
    float sm_a, sm_inv, sm_s, time, fb;
    float a[14], b[8];
} delayfx_t;
static void delayfx_new(delayfx_t *d, fxs_t *mem, float sr)
{
    sr = sr > 0.0f ? sr : 41000.0f;
    int64_t n = f2l(floorf(sr * 4.0f + 1.0f));
    int64_t nn = n > MKSULTRA_MAX_SR * 4 + 2 ? MKSULTRA_MAX_SR * 4 + 2 : n;
    if (n > DELAY_CAP) n = DELAY_CAP;
    d->buf = mem; d->n = (int)n; d->w = (int)n - 1; d->nn = (int)nn;
    memset(mem, 0, sizeof(fxs_t) * (size_t)n);
    d->sm_a = 0.045351472f * sr;
    float fcs = fminr(44100.0f / sr, 1.0f);
    memset(d->a, 0, sizeof d->a);
    memset(d->b, 0, sizeof d->b);
    d->a[0] = PI_F; d->a[1] = 2.0f; d->a[2] = 0.5f; d->a[9] = 0.9988f; d->a[10] = 1.2f; d->a[13] = fcs;
    d->b[3] = fcs; d->b[7] = 1.0f;
    d->sm_inv = 1.0f / (d->sm_a + 1.0f); d->sm_s = 0.0f; d->time = 1.0f; d->fb = 0.5f;
}
static void delayfx_reset(delayfx_t *d)
{
    memset(d->buf, 0, sizeof(fxs_t) * (size_t)d->n);
    d->w = d->n - 1;
    for (int k = 3; k <= 7; k++) d->a[k] = 0.0f;
    d->b[0] = d->b[1] = d->b[2] = d->b[4] = d->b[5] = d->b[6] = 0.0f;
}
static void delayfx_set_tone(delayfx_t *d, float p)
{
    float fc;
    if (p <= 0.5f) { float c = 1.7f * p + 0.14999998f; fc = d->a[13] * (c * c); }
    else fc = d->a[13];
    float f = fc * 0.38f;
    d->a[8] = f;
    d->a[9] = (0.9988f - fc * 0.24662f) + (fc * 0.71174f + 0.4955f) * f * f;
    d->a[10] = f + 1.2f + f * f * (-6.1f);
    float q = fc * (-0.38f) * d->a[0] * d->a[9];
    d->a[11] = ((((-0.041666668f - q * 0.008333334f) * q - 0.16666667f) * q - 0.5f) * q * q - q) * d->a[1];
    if (p <= 0.5f) d->b[7] = 0.999f;
    else { float u = 1.7f * p - 0.85f; d->b[7] = 0.999f - d->b[3] * (u * u); }
}
static inline float delayfx_tick(delayfx_t *d, float input)
{
    d->sm_s = (d->sm_s * d->sm_a + d->time) * d->sm_inv;
    int n = d->n;
    int64_t idx = f2l(floorf(((float)d->nn + (-1.0f)) * d->sm_s));
    if (idx > n - 1) idx = n - 1;               /* (nur mit kürzerem Puffer) */
    int64_t rp = (int64_t)d->w - idx;
    while (rp < 0) rp += n;
    float y = FXS_GET(d->buf[rp], DLY_ISC);
    float u = d->fb * y + input;
    float v = (u * d->a[2] - d->a[7]) * d->a[11] + d->a[5];
    d->a[7] = d->a[2] * v;
    float wv = d->a[6] * 0.125f + 0.875f * v;
    d->a[3] = v; d->a[4] = wv; d->a[5] = v; d->a[6] = wv;
    float h = d->b[1] * d->b[7] + (wv - d->b[0]);
    d->b[1] = h; d->b[0] = wv; d->b[2] = h;
    float x2 = h + h;
    float a = fabsf(x2);
    float pq = (3.0f + a) * a + 6.0f;
    d->buf[d->w] = FXS_PUT((x2 * pq) / (pq * a + 12.0f), DLY_SC);
    d->w = d->w + 1 < n ? d->w + 1 : 0;
    return y;
}

typedef struct {
    delayfx_t l, r;
    float time_p, dry, wet, bpm;
    int sync;
    float spread_l, spread_r;
    int reset;
    float fb2, p_drywet, p_spread, p_tone, p_fb;
} delay_t;

static void delay_apply_times(delay_t *d)
{
    float p = d->time_p, t;
    if (d->sync) {
        int32_t k = f2i(floorf(p * 20.0f + 0.5f));
        float x = (k >= 0 && k < 21) ? bits2f(DELAY_SYNC_X[k]) : p;
        t = x / d->bpm;
    } else {
        t = expf(2.9957323f * p) * 0.052105263f + (-0.042105265f);
    }
    d->l.time = fminr(fmaxr(d->spread_l * t, 0.01f), 1.0f);
    d->r.time = fminr(fmaxr(t * d->spread_r, 0.01f), 1.0f);
}
static void delay_apply_fb(delay_t *d, float g)
{
    d->l.fb = (d->spread_r * 0.5f + 0.5f) * g;
    d->r.fb = (d->spread_l * 0.5f + 0.5f) * g;
}
static void delay_set_drywet(delay_t *d, float p)
{
    d->p_drywet = p; d->wet = p;
    d->dry = p < 0.5f ? 1.0f : 2.0f - (p + p);
    if (p == 0.0f) d->reset = 1;
}
static void delay_set_time(delay_t *d, float p) { d->time_p = p; delay_apply_times(d); }
static void delay_set_sync(delay_t *d, int on) { d->sync = on; delay_apply_times(d); }
static void delay_set_bpm(delay_t *d, float bpm)
{
    if (bpm != d->bpm) { d->bpm = bpm; if (bpm > 1.0f && bpm < 1000.0f) delay_apply_times(d); }
}
static void delay_set_spread(delay_t *d, float p)
{
    d->p_spread = p;
    if (p < 0.5f) { d->spread_r = 1.0f; d->spread_l = p + 0.5f; }
    else { d->spread_r = 1.5f - p; d->spread_l = 1.0f; }
    delay_apply_times(d);
    float x = d->fb2 * 0.99f;
    float g = x > 0.0f ? fsqrt(x) : 0.0f;
    delay_apply_fb(d, g);
}
static void delay_set_feedback(delay_t *d, float p)
{
    d->p_fb = p;
    d->fb2 = p * p;
    float g = p * p * 0.99f > 0.0f ? fabsf(p) * 0.99498743f : 0.0f;
    delay_apply_fb(d, g);
}
static void delay_set_tone(delay_t *d, float p) { d->p_tone = p; delayfx_set_tone(&d->l, p); delayfx_set_tone(&d->r, p); }
static void delay_init_params(delay_t *d)
{
    float dw = d->p_drywet, t = d->time_p, sp = d->p_spread, tn = d->p_tone, fb = d->p_fb;
    int s = d->sync;
    delay_set_drywet(d, dw);
    delay_set_time(d, t);
    delay_set_sync(d, s);
    delay_set_spread(d, sp);
    delay_set_tone(d, tn);
    delay_set_feedback(d, fb);
}
static void delay_new(delay_t *d, fxs_t *ml, fxs_t *mr, float sr)
{
    delayfx_new(&d->l, ml, sr);
    delayfx_new(&d->r, mr, sr);
    d->time_p = 0.3f; d->dry = 1.0f; d->wet = 0.0f; d->bpm = 120.0f; d->sync = 1; d->spread_l = 1.0f; d->spread_r = 1.0f;
    d->reset = 1; d->fb2 = 0.0f; d->p_drywet = 0.0f; d->p_spread = 0.5f; d->p_tone = 0.5f; d->p_fb = 0.2f;
    delay_init_params(d);
}
static void delay_set_sr(delay_t *d, float sr)
{
    delayfx_new(&d->l, d->l.buf, sr);
    delayfx_new(&d->r, d->r.buf, sr);
    delay_init_params(d);
}
static inline void delay_process(delay_t *d, float *l, float *r)
{
    if (d->reset) { delayfx_reset(&d->l); delayfx_reset(&d->r); d->reset = 0; }
    if (d->wet > 1e-5f) {
        float in_l = *l * 0.2f, in_r = *r * 0.2f;
        float yl = delayfx_tick(&d->l, in_l);
        float yr = delayfx_tick(&d->r, in_r);
        *l = (yl * 5.0f) * d->wet + d->dry * *l;
        *r = (yr * 5.0f) * d->wet + d->dry * *r;
    }
}

/* ---------- reverb.rs ---------- */
#define REVERB_CAP (MKSULTRA_MAX_SR * (14 + MKSULTRA_PREDELAY_TENTHS) / 10 + 12000)
#define PRE_CAP (MKSULTRA_MAX_SR * MKSULTRA_PREDELAY_TENTHS / 10 + 2)
static const float RV_LEN[17] = {42.706253f, 44.483521f, 46.334747f, 48.263012f, 50.271526f, 52.363632f,
                                 54.542793f, 56.812649f, 59.176964f, 61.639671f, 64.204872f, 66.876816f,
                                 69.659966f, 72.558937f, 75.578552f, 78.723831f, 82.0f};
static const float RV_C[17] = {-0.15677883f, -0.16330335f, -0.17009938f, -0.17717823f, -0.18455169f, -0.192232f,
                               -0.20023192f, -0.20856479f, -0.2172444f, -0.22628526f, -0.23570235f, -0.24551134f,
                               -0.25572854f, -0.26637095f, -0.27745622f, -0.28900287f, -0.30103001f};

static int is_prime_fast(uint32_t m)
{
    float s = fsqrt((float)m);
    if (s < 2.0f) return 1;
    for (uint32_t d = 2; (float)d <= s; d++)
        if (m % d == 0) return 0;
    return 1;
}
static uint32_t next_prime(uint32_t n)
{
    for (;;) {
        uint32_t m = n > 1 ? n : 1;
        if (is_prime_fast(m)) return m;
        n++;
    }
}

typedef struct { fxs_t *buf; int n, w, nn; } line_t;
typedef struct { float gain; line_t line; float lp; } comb_t;
typedef struct { int32_t seed; float v, inc; int32_t period; float sr; } comblfo_t;
typedef struct { fxs_t *buf; int n, i; } allpass_t;

typedef struct {
    fxs_t *pool;
    int pool_used, failed;
    line_t pre;
    comb_t combs[2][6];
    comblfo_t lfo[2][6];
    allpass_t ap[2][11];
    biquad_t hp, lp;
    float cached_room, predelay, size, lp_amt, hp_amt, mod_room, mod_depth, dry, wet;
    int clear;
} reverb_t;

static fxs_t *rv_alloc(reverb_t *r, uint32_t n)
{
    if (r->pool_used + (int)n > REVERB_CAP) { r->failed = 1; return r->pool; }
    fxs_t *p = r->pool + r->pool_used;
    r->pool_used += (int)n;
    memset(p, 0, sizeof(fxs_t) * n);
    return p;
}
static inline float clfo_r(const comblfo_t *m) { return (float)((uint32_t)m->seed & 0x7fffffffu); }
static inline void clfo_step(comblfo_t *m) { m->seed = wmul(m->seed, 16807); }

static void reverb_initialize(reverb_t *r, float sr, seeds_t *seeds)
{
    float sri = (float)f2l(sr);
    r->pool_used = 0;
    r->failed = 0;
    uint32_t sp = next_prime((uint32_t)f2i(sr)), spb = sp > PRE_CAP ? PRE_CAP : sp;
    r->pre.buf = rv_alloc(r, spb); r->pre.n = (int)spb; r->pre.w = 0; r->pre.nn = (int)sp;
    for (int i = 0; i < 6; i++) {
        float fl, fr;
        if (i % 2 == 0) { fl = 0.002016f * sri; fr = 0.002f * sri; }
        else { fl = 0.00201f * sri; fr = 0.00202608f * sri; }
        float g = powf(10.0f, RV_C[i]);
        uint32_t sl = next_prime(f2u(RV_LEN[i] * fl));
        uint32_t srr = next_prime(f2u(RV_LEN[i] * fr));
        r->combs[0][i].gain = g; r->combs[0][i].line.buf = rv_alloc(r, sl); r->combs[0][i].line.n = (int)sl;
        r->combs[0][i].line.w = 0; r->combs[0][i].lp = 0.0f;
        r->combs[1][i].gain = g; r->combs[1][i].line.buf = rv_alloc(r, srr); r->combs[1][i].line.n = (int)srr;
        r->combs[1][i].line.w = 0; r->combs[1][i].lp = 0.0f;
        int32_t period0 = f2i((sr / 44100.0f) * (86000.0f + (float)i * 1433.3334f));
        for (int side = 0; side < 2; side++) {
            comblfo_t *m = &r->lfo[side][i];
            m->seed = wmul(next_seed(seeds), 16807);
            m->v = 1.001f; m->inc = 0.0f; m->period = period0; m->sr = sr;
            m->inc = 1.0f / ((clfo_r(m) * 3.4924597e-10f + 0.25f) * (float)period0);
        }
    }
    for (int i = 0; i < 11; i++) {
        uint32_t s = next_prime((uint32_t)f2i(RV_LEN[5 + i] * (sri * 0.00015f)));
        for (int side = 0; side < 2; side++) {
            r->ap[side][i].buf = rv_alloc(r, s); r->ap[side][i].n = (int)s; r->ap[side][i].i = 0;
        }
    }
    bq_new(&r->hp, 1, sr);
    bq_new(&r->lp, 0, sr);
    r->cached_room = -1.0f;
}
static void reverb_new(reverb_t *r, fxs_t *pool, float sr, seeds_t *seeds)
{
    r->pool = pool;
    r->predelay = 0.0f; r->size = 0.0f; r->lp_amt = 0.0f; r->hp_amt = 0.0f; r->mod_room = 0.6f; r->mod_depth = 0.1f;
    r->dry = 1.0f; r->wet = 0.0f; r->clear = 0;
    reverb_initialize(r, sr, seeds);
}
static void reverb_set_drywet(reverb_t *r, float p)
{
    r->wet = p * p * 0.5f;
    r->dry = p < 0.5f ? 1.0f : 2.0f - (p + p);
    if (p == 0.0f) r->clear = 1;
}
static void reverb_set_delay(reverb_t *r, float p) { r->predelay = p * p; }
static void reverb_set_size(reverb_t *r, float p) { r->size = expf(2.9957323f - p * 2.9957323f) * (-0.051052634f) + 1.0510527f; }
static void reverb_set_tone(reverb_t *r, float p)
{
    if (p <= 0.5f) { float a = 1.5f * p + 0.25f, a2 = a * a; r->lp_amt = a2 * a2; r->hp_amt = 0.0f; }
    else { float a = 1.5f * p - 0.75f, a2 = a * a; r->lp_amt = 1.0f; r->hp_amt = a2 * a2; }
}
static void reverb_process(reverb_t *r, float *l, float *rr_)
{
    if (r->clear) {
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i < 11; i++) memset(r->ap[s][i].buf, 0, sizeof(fxs_t) * (size_t)r->ap[s][i].n);
            for (int i = 0; i < 6; i++) memset(r->combs[s][i].line.buf, 0, sizeof(fxs_t) * (size_t)r->combs[s][i].line.n);
        }
        r->clear = 0;
    }
    if (r->wet == 0.0f) return;
    if (r->cached_room != r->mod_room) {
        r->cached_room = r->mod_room;
        float om = 1.0f - r->mod_room;
        float room = (om * om) * 88200.0f + 1000.0f;
        static const float k[6] = {1.0f, 1.0166667f, 1.0333333f, 1.05f, 1.0666667f, 1.0833334f};
        for (int i = 0; i < 6; i++)
            for (int s = 0; s < 2; s++) {
                comblfo_t *m = &r->lfo[s][i];
                m->period = f2i(m->sr * ((k[i] * room) / 44100.0f));
                m->v = 0.0f;
            }
    }
    float input = (*rr_ + *l) * 0.25f;
    /* Indizes in 32 Bit (die Puffer sind kürzer als 2^31: gleiche Werte wie mit 64 Bit, ohne Software-Rechnung) */
    int32_t sp = r->pre.n;
    int32_t off = f2i(r->predelay * (float)(r->pre.nn - 2) + 1.0f);
    if (off > sp - 1) off = sp - 1;             /* (nur mit kürzerem Puffer) */
    int32_t rp = r->pre.w - off;
    if (rp < 0) rp += sp;
    float x = FXS_GET(r->pre.buf[rp], RV_ISC);
    r->pre.buf[r->pre.w] = FXS_PUT(input, RV_SC);
    r->pre.w = r->pre.w + 1 < sp ? r->pre.w + 1 : 0;
    float fch = r->hp_amt * 20000.0f + 10.0f;
    float fcl = r->lp_amt * 20000.0f + 10.0f;
    bq_set(&r->hp, fch, 0.68f, 1.0f);
    float h = bq_tick(&r->hp, x);
    bq_set(&r->lp, fcl, 0.68f, 1.0f);
    float t = bq_tick(&r->lp, h);
    float fb = 0.994f * r->size;
    float base = r->size * 0.497f;
    float dep = r->mod_depth * 0.1f;
    float sign = 1.0f;
    float acc[2] = {0.0f, 0.0f};
    for (int i = 0; i < 6; i++) {
        for (int side = 0; side < 2; side++) {
            comblfo_t *m = &r->lfo[side][i];
            comb_t *c = &r->combs[side][i];
            if (!(1.0f > m->v)) {
                clfo_step(m);
                m->inc = -1.0f / ((clfo_r(m) * 3.4924597e-10f + 0.25f) * (float)m->period);
            }
            float inc;
            if (m->v > -1.0f) inc = m->inc;
            else {
                clfo_step(m);
                m->inc = 1.0f / ((clfo_r(m) * 3.4924597e-10f + 0.25f) * (float)m->period);
                inc = m->inc;
            }
            m->v = inc + m->v;
            int32_t s = c->line.n;
            float d = ((float)(s - 2)) * ((m->v * 0.5f + 0.5f) * (dep * sign) + base) + 1.0f;
            if (d < 1.0f) d = 1.0f;
            int32_t di = f2i(d);
            int32_t rr = c->line.w - di;
            if (rr < 0) rr += s;
            int32_t r2 = (rr > 0 ? 0 : s) + rr - 1;
            float a = FXS_GET(c->line.buf[rr], RV_ISC);
            float o = (FXS_GET(c->line.buf[r2], RV_ISC) - a) * (d - (float)di) + a;
            c->lp = (c->lp - o) * (c->gain * 0.01f) + o;
            c->line.buf[c->line.w] = FXS_PUT(c->lp * fb + t, RV_SC);
            c->line.w = c->line.w + 1 >= s ? 0 : c->line.w + 1;
            acc[side] += sign * o;
        }
        sign = -sign;
    }
    float y[2] = {acc[0], acc[1]};
    for (int i = 0; i < 11; i++)
        for (int side = 0; side < 2; side++) {
            allpass_t *a = &r->ap[side][i];
            float z = FXS_GET(a->buf[a->i], RV_ISC);
            float vv = 0.68f * z + y[side];
            a->buf[a->i] = FXS_PUT(vv, RV_SC);
            a->i = a->i + 1 >= a->n ? 0 : a->i + 1;
            y[side] = z - vv * 0.68f;
        }
    *l = r->dry * *l + ((y[0] + acc[0]) * 0.5f) * r->wet;
    *rr_ = r->dry * *rr_ + ((y[1] + acc[1]) * 0.5f) * r->wet;
}

/* ---------- arp.rs ---------- */
#define ARP_MAX 1280
typedef struct { int32_t note, orig, ch; float vel; } noteinfo_t;
static const double ARP_SYNC_M[18] = {0.125, 0.1666666716337204, 0.1875, 0.25, 0.3333333432674408, 0.375, 0.5,
                                      0.6666666865348816, 0.75, 1.0, 1.3333333730697632, 1.5, 2.0, 2.6666667461395264,
                                      3.0, 3.0, 4.0, 4.5};
typedef struct {
    int active, dirty, trigger, sync;
    int32_t sync_mode;
    float rate, bpm;
    double phase, inc;
    float sr;
    int host_sync, host_trigger, ppq_trigger;
    noteinfo_t held[128];
    int nheld;
    noteinfo_t work[ARP_MAX];
    int nwork;
    noteinfo_t out[ARP_MAX];
    int32_t pos, out_len, base_count;
    int latched;
    int32_t cur_range, cur_mode, cur_order, range_p, mode_p, order_p;
    int last_voice; /* -1 = keine */
    uint32_t rng;
} arp_t;

static void arp_set_rate_free(arp_t *a, float r)
{
    float hz = r * r * r * (25.0f - 0.5f) + 0.5f;
    a->inc = (double)(hz / a->sr);
}
static void arp_set_tempo(arp_t *a, float r)
{
    if (!a->sync) { arp_set_rate_free(a, r); return; }
    int32_t k = f2i(floorf(r * 17.0f + 0.5f));
    double m = (k >= 0 && k < 18) ? ARP_SYNC_M[k] : 1.0;
    a->inc = (m * 4.0 * (double)a->bpm) / ((double)a->sr * 60.0);
}
static void arp_new(arp_t *a, int32_t seed)
{
    memset(a, 0, sizeof *a);
    a->bpm = 120.0f; a->sr = 44100.0f; a->cur_range = 1; a->cur_mode = 1; a->range_p = 1; a->mode_p = 1;
    a->last_voice = -1;
    a->rng = seed == 0 ? 12u : (uint32_t)seed;
    arp_set_rate_free(a, 0.5f);
}
static void arp_set_rate_param(arp_t *a, float r) { a->rate = r; arp_set_tempo(a, r); }
static void arp_set_sync_mode(arp_t *a, int32_t m)
{
    a->sync_mode = m;
    a->sync = m != 0;
    arp_set_tempo(a, a->rate);
    if (m >= 1 && m <= 4) a->host_sync = ((0x01010000u >> (8 * (m - 1))) & 0xffu) != 0;
}
static void arp_set_bpm(arp_t *a, float bpm) { if (bpm != a->bpm) { a->bpm = bpm; arp_set_tempo(a, a->rate); } }
static void arp_set_mode(arp_t *a, int32_t m)
{
    a->active = m != 0;
    if (m >= 1 && m <= 5) a->mode_p = m;
    a->dirty = 1;
}
static void arp_set_range(arp_t *a, int32_t r) { a->range_p = r; a->dirty = 1; }

static void arp_put(arp_t *a, int i, noteinfo_t e)
{
    if (i >= ARP_MAX) return;
    if (a->nwork <= i) {
        for (int k = a->nwork; k < i; k++) memset(&a->work[k], 0, sizeof(noteinfo_t));
        a->nwork = i + 1;
    }
    a->work[i] = e;
}
static int32_t arp_add_octave_array(arp_t *a, int32_t n)
{
    if (n < 1) return 0;
    int32_t range = a->cur_range;
    int32_t dir = a->cur_order == 2 ? (a->cur_mode != 2 ? 1 : -1) : 1;
    noteinfo_t base[128]; /* n = gehaltene Tasten (höchstens 128); work[0..n) bleibt, Oktaven kommen dahinter */
    int nb = n < 128 ? n : 128;
    memcpy(base, a->work, sizeof(noteinfo_t) * (size_t)nb);
    int nt = nb;
    int filter = !(a->cur_order == 1 || a->cur_order == 2);
    for (int32_t o = 0; o < (range > 0 ? range : 0); o++) {
        int32_t shift = (o * 12 + 12) * dir;
        for (int k = 0; k < nb; k++) {
            int32_t nn = base[k].note + shift;
            if ((!filter || nn < 0x61) && nt < ARP_MAX) {
                noteinfo_t e = {nn, base[k].note, base[k].ch, base[k].vel};
                arp_put(a, nt++, e);
            }
        }
    }
    return nt;
}
static int cmp_note_asc(const void *x, const void *y)
{
    int32_t a = ((const noteinfo_t *)x)->note, b = ((const noteinfo_t *)y)->note;
    return (a > b) - (a < b);
}
static void arp_sort_and_calc(arp_t *a)
{
    a->cur_range = a->range_p;
    a->cur_mode = a->mode_p;
    a->cur_order = a->order_p;
    int32_t n = a->base_count;
    (void)cmp_note_asc; /* Sortierung nach Tonhöhe ist im Original tot (order_p bleibt 0) */
    int32_t c = arp_add_octave_array(a, n);
    int32_t count = c;
    if (a->cur_mode == 3) {
        if (c > 2) {
            int32_t k = c;
            for (int32_t j = c - 2; j >= 1; j--) { arp_put(a, k, a->work[j]); k++; }
            count = k;
        }
    } else if (a->cur_mode == 5 && c >= 1) {
        for (int32_t j = c; j >= 1; j--) arp_put(a, j, a->work[j - 1]);
        int32_t k = c + 1;
        if (c > 1)
            for (int32_t j = c; j >= 2; j--) { arp_put(a, k, a->work[j]); k++; }
        count = k;
    }
    int cnt = count > 0 ? count : 0;
    if (cnt > ARP_MAX) cnt = ARP_MAX;
    memcpy(a->out, a->work, sizeof(noteinfo_t) * (size_t)cnt);
    a->out_len = cnt;
}
static void arp_rebuild(arp_t *a)
{
    int n = a->nheld;
    if (a->nwork < n) a->nwork = n;
    memcpy(a->work, a->held, sizeof(noteinfo_t) * (size_t)n);
    a->base_count = n;
    arp_sort_and_calc(a);
}
static void arp_key_on(arp_t *a, int32_t note, float vel)
{
    if (a->out_len <= 0) { a->phase = 1.0; a->trigger = 1; }
    if (a->sync_mode == 1 && a->out_len <= 0) a->phase = 1.0;
    int found = 0;
    for (int i = 0; i < a->nheld; i++) if (a->held[i].note == note) found = 1;
    if (!found && a->nheld < 128) {
        noteinfo_t e = {note, note, 0, vel};
        a->held[a->nheld++] = e;
    }
    arp_rebuild(a);
}
static int arp_key_off(arp_t *a, int32_t note)
{
    if (a->nheld == 1 && !a->latched) {
        a->pos = 0; a->base_count = 0; a->out_len = 0; a->nheld = 0;
        return 1;
    }
    for (int i = 0; i < a->nheld; i++)
        if (a->held[i].note == note) {
            memmove(&a->held[i], &a->held[i + 1], sizeof(noteinfo_t) * (size_t)(a->nheld - i - 1));
            a->nheld--;
            break;
        }
    if (!a->latched) arp_rebuild(a);
    return 0;
}
static void arp_set_hold(arp_t *a, int on)
{
    if (a->latched != on) { a->latched = on; if (!on) arp_rebuild(a); }
}
static void arp_reset_all(arp_t *a) { a->base_count = 0; a->pos = 0; a->out_len = 0; a->nheld = 0; }

/* 0 = nichts, 1 = feuert ohne Muster, 2 = feuert mit *e */
static int arp_tick(arp_t *a, noteinfo_t *e)
{
    if (!a->active) return 0;
    if (a->dirty) { a->dirty = 0; arp_sort_and_calc(a); }
    int fire;
    if (a->ppq_trigger) { a->ppq_trigger = 0; fire = 1; }
    else if (a->host_sync && a->sync) {
        if (a->host_trigger) { a->host_trigger = 0; fire = 1; }
        else fire = a->trigger;
    } else {
        double p = a->phase + a->inc;
        a->phase = p;
        if (p >= 1.0) { a->phase = p + (-1.0); fire = 1; }
        else fire = a->trigger;
    }
    if (!fire) return 0;
    a->trigger = 0;
    if (a->out_len < 1) { a->pos = 0; a->last_voice = -1; return 1; }
    if (!a->latched) arp_rebuild(a);
    int32_t len = a->out_len;
    if (len < 1) { a->pos = 0; a->last_voice = -1; return 1; }
    int32_t i = a->pos;
    if (i >= len) { i = 0; a->pos = 0; }
    else if (i < 0) { i = len - 1; a->pos = i; }
    int32_t mode = a->cur_mode, idx;
    if (mode == 4) {
        a->rng = a->rng * 16807u;
        float x = (float)(a->rng & 0x7fffffffu);
        float f = (float)(len - 1) * 4.6566129e-10f * x + 0.5f;
        idx = f2i(f);
    } else {
        int32_t off = 0;
        if (mode == 2 && a->cur_order != 2) {
            int32_t n = a->base_count;
            if (n != 0) off = (len - n) + ((n - len) + a->cur_range * n) % n;
        }
        idx = (off + i) % len;
    }
    idx = clampi(idx, 0, len - 1);
    *e = a->out[idx];
    int32_t step = mode == 2 ? (a->cur_order == 2 ? 1 : -1) : 1;
    a->pos = i + step;
    return 2;
}

#endif
