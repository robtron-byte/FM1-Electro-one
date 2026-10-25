/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "dist.h"
#include "dsp_math.h"
#include <string.h>

static inline float powf_fast(float a, float b) { return a <= 0.0f ? 0.0f : dsp_exp2(b * dsp_log2(a)); }

/* ---- distortion (Effects.swift AcidDistortion) ----------------------------- */

/* Half-band taps at odd offsets 1, 3, 5 ... from the centre (0.5). */
static const float half_band[12] EL_DSP_DATA = {
    0.3163647824f, -0.1003918960f, 0.0545327660f, -0.0334618158f, 0.0211372679f, -0.0132048055f,
    0.0079527640f, -0.0045132248f, 0.0023474055f, -0.0010708521f, 0.0003905110f, -0.0000820879f};
enum { DIST_LATENCY = 24 };

static inline float dist_clip(float x, int mode)
{
    switch (mode) {
    case 1: { /* OD: two cascaded asymmetric tube stages */
        float a = dsp_tanh(x + 0.35f) - 0.3363758f;
        return dsp_tanh(2.2f * a) * 1.03f;
    }
    case 2: { /* DIST: soft stage into a smooth-knee hard clipper (p = 6) */
        float v = dsp_tanh(x) * 1.9f, a = dsp_abs(v) * 1.25f, a3 = a * a * a;
        return v / powf_fast(1.0f + a3 * a3, 1.0f / 6.0f) * 1.25f;
    }
    case 4: { /* TUBE: triode-like asymmetry, soft knee */
        float u = x + 0.2f;
        float t = u >= 0 ? dsp_tanh(u * 0.9f) * (1.0f / 0.9f) : dsp_tanh(u * 1.35f) * (1.0f / 1.35f);
        return (t - 0.1978676f) * 1.15f;
    }
    default: { /* RAT: silicon diodes, unequal halves, some rectification */
        float d = x >= 0 ? 1.0f - dsp_exp(-2.2f * x) : -(1.0f - dsp_exp(1.8f * x)) * 0.8f;
        return dsp_clamp(0.8f * d + 0.35f * dsp_abs(d), -1.0f, 1.0f);
    }
    }
}

/* 2x oversampled clipping of one sample (24 samples latency). */
static inline float dist_oversampled(el_dist_ch_t *c, unsigned ip, unsigned op, float x, int mode)
{
    c->in[ip] = x;
    float even = c->in[(ip - 12u) & 31u], odd = 0;
    for (unsigned k = 0; k < 12; ++k) odd += half_band[k] * (c->in[(ip - 12u - k) & 31u] + c->in[(ip - 11u + k) & 31u]);
    odd *= 2.0f;
    c->os[op] = dist_clip(even, mode);
    unsigned newest = (op + 1u) & 63u;
    c->os[newest] = dist_clip(odd, mode);
    float y = 0.5f * c->os[(newest - 25u) & 63u];
    for (unsigned k = 0; k < 12; ++k)
        y += half_band[k] * (c->os[(newest - 25u + 2u * k + 1u) & 63u] + c->os[(newest - 25u - 2u * k - 1u) & 63u]);
    return y;
}

float dist_tight_hz(int mode)
{
    static const float hz[5] = {18, 45, 110, 90, 18};
    return hz[mode < 1 ? 0 : mode > 4 ? 4 : mode];
}

EL_DSP_CODE void dist_process(el_dist_t *d, float *left, float *right, int frames, int mode, int drive_v, int tone_v,
                              int level_v, int mix_v, int stereo)
{
    int channels = stereo ? 2 : 1;
    if (stereo && d->mono) d->ch[1] = d->ch[0]; /* mono -> stereo: R takes over L's state */
    d->mono = (uint8_t)!stereo;
    unsigned ip0 = d->in_pos, op0 = d->os_pos, ip = ip0, op = op0;
    if (mode == 0 && d->mix < 0.0001f) {
        /* Bypassed: keep the dry delay running so the latency never jumps
         * (click) and let the rings fall silent. */
        d->mix = 0;
        for (int ch = 0; ch < channels; ++ch) {
            el_dist_ch_t *c = &d->ch[ch];
            float *x = ch ? right : left;
            ip = ip0; op = op0;
            for (int n = 0; n < frames; ++n) {
                c->dry[ip] = x[n];
                x[n] = c->dry[(ip - DIST_LATENCY) & 31u];
                c->in[ip] = 0;
                c->os[op] = 0;
                c->os[(op + 1u) & 63u] = 0;
                ip = (ip + 1u) & 31u;
                op = (op + 2u) & 63u;
            }
        }
        d->in_pos = (uint8_t)ip; d->os_pos = (uint8_t)op;
        if (!stereo) memcpy(right, left, (size_t)frames * sizeof *left);
        return;
    }
    int m = mode < 1 ? 1 : mode > 4 ? 4 : mode;
    float drive = drive_v / 127.0f, drive_db, emphasis, tight_hz, makeup;
    switch (m) {
    case 1: drive_db = 6 + drive * 48; emphasis = 1.0f; tight_hz = 45; makeup = 0.55f; break;
    case 2: drive_db = 6 + drive * 48; emphasis = 2.5f; tight_hz = 110; makeup = 0.4f; break;
    case 3: drive_db = 12 + drive * 48; emphasis = 1.6f; tight_hz = 90; makeup = 0.45f; break;
    default: drive_db = drive * 30; emphasis = 0.3f; tight_hz = 18; makeup = 0.85f; break;
    }
    const float pre_coef = 0.0391080f, dc_coef = 0.9982918f; /* 280 Hz low-pass, 12 Hz DC blocker */
    float target_gain = dsp_exp2(drive_db * 0.1660964f);
    float target_mix = mode == 0 ? 0.0f : mix_v / 127.0f;
    float tight_coef = 1.0f - dsp_exp(-2.0f * DSP_PI * tight_hz / EL_SAMPLE_RATE);
    float out = dsp_exp2((level_v / 127.0f * 24.0f - 18.0f) * 0.1660964f) * makeup;
    /* Tone: 1 ... 16 kHz, second pole an octave above. */
    float tone_hz = dsp_min(1000.0f * dsp_exp2(tone_v / 127.0f * 4.0f), EL_SAMPLE_RATE * 0.45f);
    float lp_coef = 1.0f - dsp_exp(-2.0f * DSP_PI * tone_hz / EL_SAMPLE_RATE);
    float lp_coef2 = 1.0f - dsp_exp(-2.0f * DSP_PI * dsp_min(tone_hz * 2.0f, EL_SAMPLE_RATE * 0.45f) / EL_SAMPLE_RATE);
    float gain = d->gain, mix = d->mix;
    for (int ch = 0; ch < channels; ++ch) {
        el_dist_ch_t *c = &d->ch[ch];
        float *x = ch ? right : left;
        ip = ip0; op = op0; gain = d->gain; mix = d->mix;
        for (int n = 0; n < frames; ++n) {
            gain += (target_gain - gain) * 0.002f;
            mix += (target_mix - mix) * 0.002f;
            float xn = x[n];
            c->dry[ip] = xn;
            float dry = c->dry[(ip - DIST_LATENCY) & 31u];
            /* Tightening high-pass, then pre-emphasis and drive. */
            c->tight += (xn - c->tight) * tight_coef;
            float h = xn - c->tight;
            c->pre += (h - c->pre) * pre_coef;
            float y = dist_oversampled(c, ip, op, (h + emphasis * (h - c->pre)) * gain, m);
            ip = (ip + 1u) & 31u;
            op = (op + 2u) & 63u;
            float dc = y - c->dcx + dc_coef * c->dcy;
            c->dcx = y; c->dcy = dc;
            c->lp1 += (dc - c->lp1) * lp_coef;
            c->lp2 += (c->lp1 - c->lp2) * lp_coef2;
            x[n] = dry + (c->lp2 * out - dry) * mix;
        }
    }
    d->gain = gain; d->mix = mix;
    d->in_pos = (uint8_t)ip; d->os_pos = (uint8_t)op;
    if (!stereo) memcpy(right, left, (size_t)frames * sizeof *left);
}

