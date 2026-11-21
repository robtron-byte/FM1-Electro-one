/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P engine: the code of its DSP building blocks (types: engine/jx8p_dsp.h), from fm1-synths
 * (github.com/robtron-byte/fm1-synths, commit 2a7b4fa, shared/dsp/syn_dsp_inline.h and syn_chop_inline.h,
 * GPL-3.0-only, robtron-byte; there moved from FM1-Electro's engine/mks.c, commit e7b3b14). Only the prefix
 * changed (syn_ -> jxd_). Include from engine/jx8p.c only (its audio code, in SRAM on the device). */
#ifndef JX8P_DSP_INLINE_H
#define JX8P_DSP_INLINE_H
#include "jx8p_dsp.h"
#include "dsp_math.h"

/* ---- math ---------------------------------------------------------------- */

static inline EL_DSP_HELPER float jxd_wrap01(float x)
{
    int i = (int)x;
    if ((float)i > x) --i;
    return x - (float)i;
}

static inline EL_DSP_HELPER float jxd_polyblep(float t, float dt)
{
    if (t < dt) { float x = t / dt; return x + x - x * x - 1.0f; }
    if (t > 1.0f - dt) { float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
    return 0.0f;
}

static inline EL_DSP_HELPER float jxd_soft_clip(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline EL_DSP_HELPER float jxd_powf(float a, float b) { return a <= 0.0f ? 0.0f : dsp_exp2(b * dsp_log2(a)); }
static inline EL_DSP_HELPER float jxd_note_hz(float note) { return 440.0f * dsp_exp2((note - 69.0f) * (1.0f / 12.0f)); }

static inline EL_DSP_HELPER float jxd_xorshift(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n ? n : 0x9e3779b9u;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

/* ---- hardware curves: 9 points over 0..127, one every 16 (HardwareCurves.swift) ---- */

/* the same in the MKS-50, JX-8P and JP-6 apps */
static const float jxd_curve_lfo[9] EL_DSP_DATA = {0.02f, 0.0508f, 0.1434f, 0.4028f, 1.1935f, 3.2167f, 9.7766f, 25.6894f, 64.9854f};
static const float jxd_curve_chorus[9] EL_DSP_DATA = {0.01f, 0.0217f, 0.0524f, 0.1243f, 0.3117f, 0.777f, 1.8335f, 4.83f, 10.2564f};
/* LFO sync divisions in quarter notes: 1/32 ... 4/1 */
static const float jxd_divisions[14] EL_DSP_DATA = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3,
                                                    1, 1.5f, 2, 4, 8, 16};

static inline EL_DSP_HELPER float jxd_curve_exp(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] * jxd_powf(t[i + 1] / t[i], f);
}

static inline float jxd_curve_lin(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] + (t[i + 1] - t[i]) * f;
}

/* ---- envelope -------------------------------------------------------------- */

static EL_DSP_HELPER void jxd_env_begin(jxd_env_t *e, int stage, float level, float seconds)
{
    float samples = seconds * EL_SAMPLE_RATE;
    e->stage = (uint8_t)stage;
    e->hurry = 0;
    e->start = e->value;
    e->target = level;
    e->progress = 0;
    e->increment = 1.0f / (samples > 1.0f ? samples : 1.0f);
    e->curve = 1.0f;
    e->curve_decay = dsp_exp(-4.5f * e->increment);
}

static inline EL_DSP_HELPER float jxd_env_next(jxd_env_t *e, const jxd_env_settings_t *s)
{
    switch (e->stage) {
    case JXD_ENV_IDLE: return 0;
    case JXD_ENV_SUSTAIN:
        e->value += (s->l3 - e->value) * 0.002f;
        return e->value;
    default:
        e->progress += e->increment;
        if (e->progress >= 1.0f) {
            e->value = e->target;
            switch (e->stage) {
            case JXD_ENV_T1: jxd_env_begin(e, JXD_ENV_T2, s->l2, s->t2); break;
            case JXD_ENV_T2: jxd_env_begin(e, JXD_ENV_T3, s->l3, s->t3); break;
            case JXD_ENV_T3: e->stage = JXD_ENV_SUSTAIN; break;
            default: e->stage = JXD_ENV_IDLE; e->value = 0; break;
            }
        } else {
            float shape;
            if (e->stage == JXD_ENV_T1) shape = e->progress;
            else { e->curve *= e->curve_decay; shape = (1.0f - e->curve) * 1.011235f; }
            e->value = e->start + (e->target - e->start) * shape;
        }
        return e->value;
    }
}

/* ---- filters --------------------------------------------------------------- */

/* decim (a constant where it is called, so each caller is compiled for its rate): the ladder runs at the
 * sample rate / decim. 1 normally; 2 was FM1-Electro's on the device (mks50: the switch mks_t.vcf_half). */
static inline __attribute__((always_inline)) void jxd_ladder_set(jxd_ladder_t *v, float cutoff, float resonance, const int decim)
{
    const float rate = EL_SAMPLE_RATE / (float)decim;
    float fc = dsp_clamp(cutoff, 8.0f, rate * 0.45f);
    v->g = dsp_tan(DSP_PI * fc / rate);
    v->G = v->g / (1.0f + v->g);
    v->k = resonance * 3.8f;
    v->G2 = v->G * v->G;
    v->G3 = v->G2 * v->G;
    v->G4 = v->G3 * v->G;
    v->inv = 1.0f / (1.0f + v->g);
    v->den = 1.0f / (1.0f + v->k * v->G4);
}

static inline EL_DSP_HELPER float jxd_ladder(jxd_ladder_t *v, float input)
{
    float G = v->G, G2 = v->G2, G3 = v->G3, G4 = v->G4;
    float sigma = (G3 * v->s1 + G2 * v->s2 + G * v->s3 + v->s4) * v->inv;
    float x = input * (1.0f + 0.55f * v->k) + 1e-20f;
    float y4e = (G4 * x + sigma) * v->den;
    float u = 2.0f * jxd_soft_clip(0.5f * (x - v->k * y4e));
    float t = (u - v->s1) * G, y1 = t + v->s1; v->s1 = y1 + t;
    t = (y1 - v->s2) * G; float y2 = t + v->s2; v->s2 = y2 + t;
    t = (y2 - v->s3) * G; float y3 = t + v->s3; v->s3 = y3 + t;
    t = (y3 - v->s4) * G; float y4 = t + v->s4; v->s4 = y4 + t;
    return y4;
}

static inline void jxd_ladder_reset(jxd_ladder_t *v) { v->s1 = v->s2 = v->s3 = v->s4 = 0; }

static inline EL_DSP_HELPER float jxd_biquad(const jxd_biquad_t *c, float *z1, float *z2, float x)
{
    float y = c->b0 * x + *z1;
    *z1 = c->b1 * x - c->a1 * y + *z2;
    *z2 = c->b2 * x - c->a2 * y;
    return y;
}

/* ---- chorus (MN3009 BBD, Chorus.swift) ----------------------------------- */

/* CHORUS: OFF, FREE (rate, as on the MKS-50), and the Juno-60's buttons I, II and I+II with their fixed
 * rates and depths. mode 0 = FREE. */
static EL_DSP_CODE void jxd_chorus(jxd_chorus_t *c, float *left, float *right, int frames, int enabled, int mode, float rate)
{
    const float lp = 0.6802f; /* 1 - exp(-2 pi 8000 / 44100) */
    float hz, depth_seconds;
    switch (mode) {
    case 1: hz = 0.513f; depth_seconds = 0.00185f; break;
    case 2: hz = 0.863f; depth_seconds = 0.00185f; break;
    case 3: hz = 9.75f; depth_seconds = 0.0002f; break;
    default: hz = rate; depth_seconds = 0.0019f - (rate < 8 ? rate : 8) * 0.0001f; break;
    }
    float target = enabled ? 1.0f : 0.0f, inc = hz / EL_SAMPLE_RATE;
    float center = 0.0035f * EL_SAMPLE_RATE;
    float depth_target = depth_seconds * EL_SAMPLE_RATE;
    if (!enabled && c->mix < 0.0001f) return;
    for (int n = 0; n < frames; ++n) {
        float xl = left[n], xr = right[n];
        c->lp1 += (0.5f * (xl + xr) - c->lp1) * lp;
        c->lp2 += (c->lp1 - c->lp2) * lp;
        c->buffer[c->write & (JXD_CHORUS_SIZE - 1)] = jxd_soft_clip(c->lp2 * 0.8f) * 1.25f;
        c->mix += (target - c->mix) * 0.001f;
        c->depth += (depth_target - c->depth) * 0.0005f;
        c->phase += inc;
        if (c->phase >= 1.0f) c->phase -= 1.0f;
        float tri = 4.0f * dsp_abs(c->phase - 0.5f) - 1.0f;
        float wet[2];
        for (int side = 0; side < 2; ++side) {
            float pos = (float)c->write - (center + (side ? -c->depth : c->depth) * tri);
            int i = (int)pos;
            if ((float)i > pos) --i;
            float frac = pos - (float)i, a = c->buffer[i & (JXD_CHORUS_SIZE - 1)];
            wet[side] = a + (c->buffer[(i + 1) & (JXD_CHORUS_SIZE - 1)] - a) * frac;
        }
        float noise = jxd_xorshift(&c->hiss) * 0.00006f;
        c->wl1 += (wet[0] + noise - c->wl1) * lp;
        c->wr1 += (wet[1] - noise - c->wr1) * lp;
        c->wl2 += (c->wl1 - c->wl2) * lp;
        c->wr2 += (c->wr1 - c->wr2) * lp;
        float dry = 1.0f - 0.3f * c->mix;
        left[n] = xl * dry + c->wl2 * 0.75f * c->mix;
        right[n] = xr * dry + c->wr2 * 0.75f * c->mix;
        c->write = (c->write + 1u) & (JXD_CHORUS_SIZE - 1);
    }
}


static const jxd_chop_t jxd_chop_off EL_DSP_DATA = {0, 0, 0.5f, 0, 0.5f, 0, 0.5f, 0, 0};

/* JXD_CHOP_GRID: shapes whose edges all fall on one grid (saw 5, sub 1-3
 * and 5: ramps and half-open gates at whole multiples) look only at the
 * nearest grid point instead of jxd_chop_general's seven edge families
 * (DARK STR, saw 5: STRINGS 54 -> see FM1-Electro docs/adr/0019). Same edges, same
 * corrections; only the float rounding of where an edge lies differs. */
#ifndef JXD_CHOP_GRID
#define JXD_CHOP_GRID 1
#endif

static inline __attribute__((always_inline)) int jxd_chop_enabled(const jxd_chop_t *c) { return c->ramp > 0 || c->m0 > 0; }

static inline EL_DSP_HELPER float jxd_chop_naive(const jxd_chop_t *c, float p)
{
    if (c->m0 > 0 && jxd_wrap01(p * c->m0) < c->w0) return 0;
    if (c->m1 > 0 && jxd_wrap01(p * c->m1) < c->w1) return 0;
    if (c->m2 > 0 && jxd_wrap01(p * c->m2) < c->w2) return 0;
    return c->ramp > 0 ? jxd_wrap01(p * c->ramp) : 1.0f;
}

static EL_DSP_CODE float jxd_chop_general(const jxd_chop_t *c, float p, float dt);

/* The two most common shapes, a plain ramp and a single gate (square /
 * pulse), inline in the voice loop (a call per sample and oscillator cost
 * more than these: FM1-Electro docs/adr/0019); every other shape in jxd_chop_general. */
static inline __attribute__((always_inline)) float jxd_chop_render(const jxd_chop_t *c, float p, float dt)
{
    if (c->m1 <= 0 && c->m2 <= 0) {
        if (c->m0 <= 0) {
            float t = jxd_wrap01(p * c->ramp), d = dt * c->ramp;
            return 2.0f * t - 1.0f - jxd_polyblep(t, d);
        }
        if (c->ramp <= 0) {
            float t = jxd_wrap01(p * c->m0), d = dt * c->m0, w = c->w0;
            float out = t >= w ? 1.0f : -1.0f;
            out += jxd_polyblep(jxd_wrap01(t - w), d) - jxd_polyblep(t, d);
            return out;
        }
        if (c->gated_ramp && dt * c->m0 < 0.5f) {
            /* Saw 2 / 3 / 4: the ramp, open only while a square of m0 (a
             * multiple of the ramp, so the ramp resets where the square
             * shuts) is high. Edges: the square opens at w0 (a jump up by
             * the ramp there), it shuts at 0 (down by the ramp just before;
             * 1 where the ramp resets). As jxd_chop_general, without its seven
             * edge tests. */
            float t = jxd_wrap01(p * c->m0), d = dt * c->m0, w = c->w0, r = c->ramp;
            float out = t >= w ? 2.0f * jxd_wrap01(p * r) - 1.0f : -1.0f;
            float xo = jxd_wrap01(t - w);
            if (xo < d || xo > 1.0f - d) {
                float e = p - (xo < 0.5f ? xo : xo - 1.0f) / c->m0;
                out += jxd_wrap01((e + 1e-5f) * r) * jxd_polyblep(xo, d);
            }
            if (t < d || t > 1.0f - d) {
                float e = p - (t < 0.5f ? t : t - 1.0f) / c->m0;
                out -= jxd_wrap01((e - 1e-5f) * r) * jxd_polyblep(t, d);
            }
            return out;
        }
    }
#if JXD_CHOP_GRID
    if (c->grid > 0.0f && dt * c->grid < 0.5f) {
        float l0 = c->m0 > 0 ? jxd_wrap01(p * c->m0) : 0, l1 = c->m1 > 0 ? jxd_wrap01(p * c->m1) : 0;
        float lr = c->ramp > 0 ? jxd_wrap01(p * c->ramp) : 0;
        int open = (c->m0 <= 0 || l0 >= c->w0) && (c->m1 <= 0 || l1 >= c->w1);
        float out = open ? 2.0f * (c->ramp > 0 ? lr : 1.0f) - 1.0f : -1.0f;
        float g = jxd_wrap01(p * c->grid), x = g < 0.5f ? g : g - 1.0f, d = x / c->grid;
        if (d < dt && d > -dt) {   /* the one grid point within reach: an edge, if the wave jumps there */
            float edge = p - d;
            float h = jxd_chop_naive(c, jxd_wrap01(edge + 1e-5f)) - jxd_chop_naive(c, jxd_wrap01(edge - 1e-5f));
            if (h != 0) out += h * jxd_polyblep(jxd_wrap01(d), dt);
        }
        return out;
    }
#endif
    return jxd_chop_general(c, p, dt);
}

static EL_DSP_CODE float jxd_chop_general(const jxd_chop_t *c, float p, float dt)
{
    float l0 = c->m0 > 0 ? jxd_wrap01(p * c->m0) : 0, l1 = c->m1 > 0 ? jxd_wrap01(p * c->m1) : 0;
    float l2 = c->m2 > 0 ? jxd_wrap01(p * c->m2) : 0, lr = c->ramp > 0 ? jxd_wrap01(p * c->ramp) : 0;
    int open = (c->m0 <= 0 || l0 >= c->w0) && (c->m1 <= 0 || l1 >= c->w1) && (c->m2 <= 0 || l2 >= c->w2);
    float out = open ? 2.0f * (c->ramp > 0 ? lr : 1.0f) - 1.0f : -1.0f;
    float seen[7];
    int seen_count = 0;
    /* 7 edge families: ramp reset, and rise / fall of each gate. Most
     * samples are far from every edge: |x| >= dt * m clearly (the 1.001
     * margin keeps the exact test below for the close ones). */
#define CHOP_EDGE(M, LOCAL, OFFSET)                                                          \
    if ((M) > 0) {                                                                           \
        float m = (M), x = (LOCAL) - (OFFSET), lim = dt * m * 1.001f;                        \
        if (x < -0.5f) x += 1.0f; else if (x >= 0.5f) x -= 1.0f;                             \
        if (x < lim && x > -lim) {                                                           \
            float d = x / m;                                                                 \
            if (d < dt && d > -dt) {                                                         \
                float edge = p - d;                                                          \
                int duplicate = 0;                                                           \
                for (int j = 0; j < seen_count; ++j) if (dsp_abs(seen[j] - edge) < 1e-5f) duplicate = 1; \
                if (!duplicate) {                                                            \
                    seen[seen_count++] = edge;                                               \
                    float h = jxd_chop_naive(c, jxd_wrap01(edge + 1e-5f)) - jxd_chop_naive(c, jxd_wrap01(edge - 1e-5f)); \
                    if (h != 0) out += h * jxd_polyblep(jxd_wrap01(d), dt);                          \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
    }
    CHOP_EDGE(c->ramp, lr, 0)
    CHOP_EDGE(c->m0, l0, 0)
    CHOP_EDGE(c->m0, l0, c->w0)
    CHOP_EDGE(c->m1, l1, 0)
    CHOP_EDGE(c->m1, l1, c->w1)
    CHOP_EDGE(c->m2, l2, 0)
    CHOP_EDGE(c->m2, l2, c->w2)
#undef CHOP_EDGE
    return out;
}

static void jxd_chop_set(jxd_chop_t *c, float ramp, float m0, float w0, float m1, float w1)
{
    /* (every control block: unchanged, nothing to work out) */
    if (c->ramp == ramp && c->m0 == m0 && c->w0 == w0 && c->m1 == m1 && c->w1 == w1 && c->m2 == 0) return;
    c->ramp = ramp; c->m0 = m0; c->w0 = w0; c->m1 = m1; c->w1 = w1; c->m2 = 0; c->w2 = 0.5f;
    /* the fast path for a ramp under one square of a multiple of its rate */
    c->gated_ramp = ramp > 0 && m0 > 0 && m1 <= 0 && w0 > 0.0f && w0 < 1.0f && (m0 / ramp) == (float)(int)(m0 / ramp);
    /* the grid: whole rates, gates half open (edges at k / (2 m)); their
     * least common multiple */
    unsigned g = 1;
    int ok = 1;
    const float r[3] = {ramp, 2.0f * m0, 2.0f * m1};
    const float w[3] = {0.5f, w0, w1};
    for (int i = 0; i < 3; ++i) {
        if (r[i] <= 0) continue;
        unsigned n = (unsigned)r[i];
        if ((float)n != r[i] || (i && w[i] != 0.5f) || n > 256u) { ok = 0; break; }
        unsigned a = g, b = n;
        while (b) { unsigned t = a % b; a = b; b = t; }
        g = g / a * n;
    }
    c->grid = ok && g <= 256u ? (float)g : 0.0f;
}
#endif
