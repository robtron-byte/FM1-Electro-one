/* SPDX-License-Identifier: GPL-3.0-only
 * fm1-synths: the chopped-ramp oscillator shape every divider-chain DCO here uses (ChopShape in the iOS apps'
 * DCO.swift, the same in the MKS-50 and JX-8P apps): a ramp AND-ed with square gates at multiples of the base
 * frequency, every edge band-limited with a PolyBLEP whose height is measured on the naive wave. Include from
 * ONE engine file per synth, after syn_dsp_inline.h.
 *
 * Moved here unchanged from the MKS-50 firmware's engine/mks.c (14.11.2026; there FM1-Electro's,
 * github.com/robtron-byte/FM1-Electro-one, commit e7b3b14, with its fast paths). Only the names got the
 * prefix syn_; mks50's make identity holds it bit for bit. */
#ifndef SYN_CHOP_INLINE_H
#define SYN_CHOP_INLINE_H
#include "syn_dsp_inline.h"

static const syn_chop_t syn_chop_off EL_DSP_DATA = {0, 0, 0.5f, 0, 0.5f, 0, 0.5f, 0, 0};

/* SYN_CHOP_GRID: shapes whose edges all fall on one grid (saw 5, sub 1-3
 * and 5: ramps and half-open gates at whole multiples) look only at the
 * nearest grid point instead of syn_chop_general's seven edge families
 * (DARK STR, saw 5: STRINGS 54 -> see FM1-Electro docs/adr/0019). Same edges, same
 * corrections; only the float rounding of where an edge lies differs. */
#ifndef SYN_CHOP_GRID
#define SYN_CHOP_GRID 1
#endif

static inline __attribute__((always_inline)) int syn_chop_enabled(const syn_chop_t *c) { return c->ramp > 0 || c->m0 > 0; }

static inline EL_DSP_HELPER float syn_chop_naive(const syn_chop_t *c, float p)
{
    if (c->m0 > 0 && syn_wrap01(p * c->m0) < c->w0) return 0;
    if (c->m1 > 0 && syn_wrap01(p * c->m1) < c->w1) return 0;
    if (c->m2 > 0 && syn_wrap01(p * c->m2) < c->w2) return 0;
    return c->ramp > 0 ? syn_wrap01(p * c->ramp) : 1.0f;
}

static EL_DSP_CODE float syn_chop_general(const syn_chop_t *c, float p, float dt);

/* The two most common shapes, a plain ramp and a single gate (square /
 * pulse), inline in the voice loop (a call per sample and oscillator cost
 * more than these: FM1-Electro docs/adr/0019); every other shape in syn_chop_general. */
static inline __attribute__((always_inline)) float syn_chop_render(const syn_chop_t *c, float p, float dt)
{
    if (c->m1 <= 0 && c->m2 <= 0) {
        if (c->m0 <= 0) {
            float t = syn_wrap01(p * c->ramp), d = dt * c->ramp;
            return 2.0f * t - 1.0f - syn_polyblep(t, d);
        }
        if (c->ramp <= 0) {
            float t = syn_wrap01(p * c->m0), d = dt * c->m0, w = c->w0;
            float out = t >= w ? 1.0f : -1.0f;
            out += syn_polyblep(syn_wrap01(t - w), d) - syn_polyblep(t, d);
            return out;
        }
        if (c->gated_ramp && dt * c->m0 < 0.5f) {
            /* Saw 2 / 3 / 4: the ramp, open only while a square of m0 (a
             * multiple of the ramp, so the ramp resets where the square
             * shuts) is high. Edges: the square opens at w0 (a jump up by
             * the ramp there), it shuts at 0 (down by the ramp just before;
             * 1 where the ramp resets). As syn_chop_general, without its seven
             * edge tests. */
            float t = syn_wrap01(p * c->m0), d = dt * c->m0, w = c->w0, r = c->ramp;
            float out = t >= w ? 2.0f * syn_wrap01(p * r) - 1.0f : -1.0f;
            float xo = syn_wrap01(t - w);
            if (xo < d || xo > 1.0f - d) {
                float e = p - (xo < 0.5f ? xo : xo - 1.0f) / c->m0;
                out += syn_wrap01((e + 1e-5f) * r) * syn_polyblep(xo, d);
            }
            if (t < d || t > 1.0f - d) {
                float e = p - (t < 0.5f ? t : t - 1.0f) / c->m0;
                out -= syn_wrap01((e - 1e-5f) * r) * syn_polyblep(t, d);
            }
            return out;
        }
    }
#if SYN_CHOP_GRID
    if (c->grid > 0.0f && dt * c->grid < 0.5f) {
        float l0 = c->m0 > 0 ? syn_wrap01(p * c->m0) : 0, l1 = c->m1 > 0 ? syn_wrap01(p * c->m1) : 0;
        float lr = c->ramp > 0 ? syn_wrap01(p * c->ramp) : 0;
        int open = (c->m0 <= 0 || l0 >= c->w0) && (c->m1 <= 0 || l1 >= c->w1);
        float out = open ? 2.0f * (c->ramp > 0 ? lr : 1.0f) - 1.0f : -1.0f;
        float g = syn_wrap01(p * c->grid), x = g < 0.5f ? g : g - 1.0f, d = x / c->grid;
        if (d < dt && d > -dt) {   /* the one grid point within reach: an edge, if the wave jumps there */
            float edge = p - d;
            float h = syn_chop_naive(c, syn_wrap01(edge + 1e-5f)) - syn_chop_naive(c, syn_wrap01(edge - 1e-5f));
            if (h != 0) out += h * syn_polyblep(syn_wrap01(d), dt);
        }
        return out;
    }
#endif
    return syn_chop_general(c, p, dt);
}

static EL_DSP_CODE float syn_chop_general(const syn_chop_t *c, float p, float dt)
{
    float l0 = c->m0 > 0 ? syn_wrap01(p * c->m0) : 0, l1 = c->m1 > 0 ? syn_wrap01(p * c->m1) : 0;
    float l2 = c->m2 > 0 ? syn_wrap01(p * c->m2) : 0, lr = c->ramp > 0 ? syn_wrap01(p * c->ramp) : 0;
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
                    float h = syn_chop_naive(c, syn_wrap01(edge + 1e-5f)) - syn_chop_naive(c, syn_wrap01(edge - 1e-5f)); \
                    if (h != 0) out += h * syn_polyblep(syn_wrap01(d), dt);                          \
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

static void syn_chop_set(syn_chop_t *c, float ramp, float m0, float w0, float m1, float w1)
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
