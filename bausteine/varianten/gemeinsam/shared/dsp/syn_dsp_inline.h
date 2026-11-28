/* SPDX-License-Identifier: GPL-3.0-only
 * fm1-synths: the DSP building blocks every synth shares, as inline code. Include it from ONE engine file
 * per synth (its audio code, in SRAM on the device), after dsp_math.h.
 *
 * Moved here unchanged from the MKS-50 firmware's engine/mks.c (14.11.2026; there FM1-Electro's,
 * github.com/robtron-byte/FM1-Electro-one, commit e7b3b14, a port of the MKS-50 iOS app's DSP:
 * DSPMath.swift, Envelope.swift, LadderFilter.swift, Chorus.swift, HardwareCurves.swift). Only the names
 * got the prefix syn_. mks50's make identity holds every sound bit for bit against FM1-Electro's engine. */
#ifndef SYN_DSP_INLINE_H
#define SYN_DSP_INLINE_H
#include "syn_dsp.h"
#include "dsp_math.h"

/* ---- math ---------------------------------------------------------------- */

static inline EL_DSP_HELPER float syn_wrap01(float x)
{
    int i = (int)x;
    if ((float)i > x) --i;
    return x - (float)i;
}

static inline EL_DSP_HELPER float syn_polyblep(float t, float dt)
{
    if (t < dt) { float x = t / dt; return x + x - x * x - 1.0f; }
    if (t > 1.0f - dt) { float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
    return 0.0f;
}

static inline EL_DSP_HELPER float syn_soft_clip(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline EL_DSP_HELPER float syn_powf(float a, float b) { return a <= 0.0f ? 0.0f : dsp_exp2(b * dsp_log2(a)); }
static inline EL_DSP_HELPER float syn_note_hz(float note) { return 440.0f * dsp_exp2((note - 69.0f) * (1.0f / 12.0f)); }

static inline EL_DSP_HELPER float syn_xorshift(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n ? n : 0x9e3779b9u;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

/* ---- hardware curves: 9 points over 0..127, one every 16 (HardwareCurves.swift) ---- */

/* the same in the MKS-50, JX-8P and JP-6 apps */
static const float syn_curve_lfo[9] EL_DSP_DATA = {0.02f, 0.0508f, 0.1434f, 0.4028f, 1.1935f, 3.2167f, 9.7766f, 25.6894f, 64.9854f};
static const float syn_curve_chorus[9] EL_DSP_DATA = {0.01f, 0.0217f, 0.0524f, 0.1243f, 0.3117f, 0.777f, 1.8335f, 4.83f, 10.2564f};
/* LFO sync divisions in quarter notes: 1/32 ... 4/1 */
static const float syn_divisions[14] EL_DSP_DATA = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3,
                                                    1, 1.5f, 2, 4, 8, 16};

static inline EL_DSP_HELPER float syn_curve_exp(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] * syn_powf(t[i + 1] / t[i], f);
}

static inline float syn_curve_lin(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] + (t[i + 1] - t[i]) * f;
}

/* ---- envelope -------------------------------------------------------------- */

static EL_DSP_HELPER void syn_env_begin(syn_env_t *e, int stage, float level, float seconds)
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

static inline EL_DSP_HELPER float syn_env_next(syn_env_t *e, const syn_env_settings_t *s)
{
    switch (e->stage) {
    case SYN_ENV_IDLE: return 0;
    case SYN_ENV_SUSTAIN:
        e->value += (s->l3 - e->value) * 0.002f;
        return e->value;
    default:
        e->progress += e->increment;
        if (e->progress >= 1.0f) {
            e->value = e->target;
            switch (e->stage) {
            case SYN_ENV_T1: syn_env_begin(e, SYN_ENV_T2, s->l2, s->t2); break;
            case SYN_ENV_T2: syn_env_begin(e, SYN_ENV_T3, s->l3, s->t3); break;
            case SYN_ENV_T3: e->stage = SYN_ENV_SUSTAIN; break;
            default: e->stage = SYN_ENV_IDLE; e->value = 0; break;
            }
        } else {
            float shape;
            if (e->stage == SYN_ENV_T1) shape = e->progress;
            else { e->curve *= e->curve_decay; shape = (1.0f - e->curve) * 1.011235f; }
            e->value = e->start + (e->target - e->start) * shape;
        }
        return e->value;
    }
}

/* ---- filters --------------------------------------------------------------- */

/* decim (a constant where it is called, so each caller is compiled for its rate): the ladder runs at the
 * sample rate / decim. 1 normally; 2 was FM1-Electro's on the device (mks50: the switch mks_t.vcf_half). */
static inline __attribute__((always_inline)) void syn_ladder_set(syn_ladder_t *v, float cutoff, float resonance, const int decim)
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

static inline EL_DSP_HELPER float syn_ladder(syn_ladder_t *v, float input)
{
    float G = v->G, G2 = v->G2, G3 = v->G3, G4 = v->G4;
    float sigma = (G3 * v->s1 + G2 * v->s2 + G * v->s3 + v->s4) * v->inv;
    float x = input * (1.0f + 0.55f * v->k) + 1e-20f;
    float y4e = (G4 * x + sigma) * v->den;
    float u = 2.0f * syn_soft_clip(0.5f * (x - v->k * y4e));
    float t = (u - v->s1) * G, y1 = t + v->s1; v->s1 = y1 + t;
    t = (y1 - v->s2) * G; float y2 = t + v->s2; v->s2 = y2 + t;
    t = (y2 - v->s3) * G; float y3 = t + v->s3; v->s3 = y3 + t;
    t = (y3 - v->s4) * G; float y4 = t + v->s4; v->s4 = y4 + t;
    return y4;
}

static inline void syn_ladder_reset(syn_ladder_t *v) { v->s1 = v->s2 = v->s3 = v->s4 = 0; }

static inline EL_DSP_HELPER float syn_biquad(const syn_biquad_t *c, float *z1, float *z2, float x)
{
    float y = c->b0 * x + *z1;
    *z1 = c->b1 * x - c->a1 * y + *z2;
    *z2 = c->b2 * x - c->a2 * y;
    return y;
}

/* ---- chorus (MN3009 BBD, Chorus.swift) ----------------------------------- */

/* CHORUS: OFF, FREE (rate, as on the MKS-50), and the Juno-60's buttons I, II and I+II with their fixed
 * rates and depths. mode 0 = FREE. */
static EL_DSP_CODE void syn_chorus(syn_chorus_t *c, float *left, float *right, int frames, int enabled, int mode, float rate)
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
        c->buffer[c->write & (SYN_CHORUS_SIZE - 1)] = syn_soft_clip(c->lp2 * 0.8f) * 1.25f;
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
            float frac = pos - (float)i, a = c->buffer[i & (SYN_CHORUS_SIZE - 1)];
            wet[side] = a + (c->buffer[(i + 1) & (SYN_CHORUS_SIZE - 1)] - a) * frac;
        }
        float noise = syn_xorshift(&c->hiss) * 0.00006f;
        c->wl1 += (wet[0] + noise - c->wl1) * lp;
        c->wr1 += (wet[1] - noise - c->wr1) * lp;
        c->wl2 += (c->wl1 - c->wl2) * lp;
        c->wr2 += (c->wr1 - c->wr2) * lp;
        float dry = 1.0f - 0.3f * c->mix;
        left[n] = xl * dry + c->wl2 * 0.75f * c->mix;
        right[n] = xr * dry + c->wr2 * 0.75f * c->mix;
        c->write = (c->write + 1u) & (SYN_CHORUS_SIZE - 1);
    }
}
#endif
