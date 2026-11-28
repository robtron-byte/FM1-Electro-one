/* SPDX-License-Identifier: GPL-3.0-only
 * Delay and reverb: a port of the user's MKS-50 iOS app (github.com/robtron-byte/Mks50-iOS-, commit 080db22,
 * Shared/DSP/Effects.swift StereoDelay and Reverb, the parameter curves of Shared/DSP/SynthEngine.swift),
 * line for line where the memory allows (see syn_fx.h). */
#if defined(EL_PLATFORM)
#pragma const_seg(".dsp_rodata")
#endif
#include "syn_fx.h"
#include "dsp_math.h"
#include <string.h>

static const float quarters[FX_DIVS] EL_DSP_DATA = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3,
                                                    1, 1.5f, 2, 4};
static const char *const div_names[FX_DIVS] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T",
                                               "1/4", "1/4.", "1/2", "1/1"};
static const char *const type_names[FX_REV_TYPES] = {"SPACE", "ROOM", "PLATE"};
static const char *const off_on[2] = {"OFF", "ON"};
static const uint8_t maxv[FX_COUNT] = {127, 127, 127, 127, 127, 1, 1, FX_DIVS - 1, 127, 127, 127, 127, 127, FX_REV_TYPES - 1};
static const uint8_t defv[FX_COUNT] = {96, 50, 70, 100, 30, 0, 1, 6, 70, 72, 50, 100, 40, 0};

static uint8_t v[FX_COUNT];

static inline EL_DSP_HELPER float soft_clip(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
static inline EL_DSP_HELPER float xs_next(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}
static inline EL_DSP_HELPER int floor_i(float x) { int i = (int)x; return (float)i > x ? i - 1 : i; }

/* ==== delay (StereoDelay) ============================================================================ */
enum { DL_SIZE = 1 << 15, DL_MASK = DL_SIZE - 1 };
static int16_t dl_l[DL_SIZE] EL_POOL, dl_r[DL_SIZE] EL_POOL;
static struct {
    unsigned w;
    float smoothed, low_l, low_r, high_l, high_r, high_coef, mix;
    float wow, flutter, drift, drift_target;
    int drift_count;
    uint32_t noise;
    unsigned quiet;
} dl;

/* the app's curves */
static float delay_feedback(float raw)
{
    float x = dsp_clamp(raw, 0, 127) / 127.0f;
    return x <= 0.5f ? x * 0.95f : 0.475f + (x - 0.5f) * 2.0f * 0.675f;
}
float syn_fx_delay_seconds(int time, int sync, int div, float bpm)
{
    if (sync) return quarters[div < 0 ? 0 : div >= FX_DIVS ? FX_DIVS - 1 : div] * 60.0f / (bpm < 20.0f ? 20.0f : bpm);
    return 0.0005f * dsp_exp2((float)time / 127.0f * 11.965784f);   /* 0.0005 * 4000^(t/127) */
}

static inline EL_DSP_HELPER float dl_read(const int16_t *b, float delay)
{
    float pos = (float)dl.w - delay;
    int i = floor_i(pos);
    float t = pos - (float)i;
    const float k = 1.0f / 32767.0f;
    float xm1 = b[(i - 1) & DL_MASK] * k, x0 = b[i & DL_MASK] * k, x1 = b[(i + 1) & DL_MASK] * k, x2 = b[(i + 2) & DL_MASK] * k;
    float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
static inline EL_DSP_HELPER float dl_sine(float ph)
{
    float x = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;
    return x * (1.5f - 0.5f * x * x);
}
static inline EL_DSP_HELPER int16_t q16(float x) { x = dsp_clamp(x, -1.0f, 1.0f) * 32767.0f; return (int16_t)(x < 0 ? x - 0.5f : x + 0.5f); }

static EL_DSP_CODE void delay_process(const float *in, float *out_l, float *out_r, int n, float bpm)
{
    const float sr = EL_SAMPLE_RATE;
    float mix = v[FX_DLY_LEVEL] ? dsp_exp2(1.5f * dsp_log2(v[FX_DLY_LEVEL] / 127.0f)) : 0.0f;
    float peak_in = 0;
    if (mix < 0.0005f && dl.mix < 0.0005f) return;
    for (int i = 0; i < n; i++) peak_in = dsp_max(peak_in, dsp_abs(in[i]));
    if (peak_in < 1e-6f && dl.quiet > 4000u) return;    /* (5.8 s of silence in and out: asleep) */
    float target = dsp_clamp(syn_fx_delay_seconds(v[FX_DLY_TIME], v[FX_DLY_SYNC], v[FX_DLY_DIV], bpm) * sr, 2.0f, DL_SIZE * 0.95f);
    if (dl.smoothed == 0) dl.smoothed = target;
    float tone = v[FX_DLY_TONE] / 127.0f, low_coef = 1.0f - dsp_exp(-2.0f * DSP_PI * (900.0f + tone * tone * 15000.0f) / sr);
    float fb = dsp_clamp(delay_feedback(v[FX_DLY_FB]), 0, 1.15f), m = v[FX_DLY_MOD] / 127.0f;
    float wow_inc = 0.55f / sr, flutter_inc = 6.3f / sr, rel = m * m * 0.004f, absd = m * 0.0006f * sr;
    float drive = 1.1f + 0.4f * fb, post = 0.8f / drive * 1.25f, peak_out = 0;
    int ping = v[FX_DLY_PING];
    for (int i = 0; i < n; i++) {
        dl.smoothed += (target - dl.smoothed) * 0.0004f;
        dl.mix += (mix - dl.mix) * 0.002f;
        dl.wow += wow_inc; if (dl.wow >= 1) dl.wow -= 1;
        dl.flutter += flutter_inc; if (dl.flutter >= 1) dl.flutter -= 1;
        if (--dl.drift_count <= 0) { dl.drift_count = 2048; dl.drift_target = xs_next(&dl.noise); }
        dl.drift += (dl.drift_target - dl.drift) * 0.0003f;
        float mov = 0.7f * dl_sine(dl.wow) + 0.12f * dl_sine(dl.flutter) + 0.5f * dl.drift;
        float off = mov * (dl.smoothed * rel + absd);
        float d_l = dsp_max(2.0f, dl.smoothed + off), d_r = dsp_max(2.0f, dl.smoothed * 1.012f - off);
        float x = in[i];
        float wl = dl_read(dl_l, d_l), wr = dl_read(dl_r, d_r);
        dl.low_l += (wl - dl.low_l) * low_coef;
        dl.low_r += (wr - dl.low_r) * low_coef;
        dl.high_l += (dl.low_l - dl.high_l) * dl.high_coef;
        dl.high_r += (dl.low_r - dl.high_r) * dl.high_coef;
        wl = dl.low_l - dl.high_l;
        wr = dl.low_r - dl.high_r;
        float to_l, to_r;
        if (ping) { to_l = x + wr * fb; to_r = wl * fb; }
        else { to_l = x + wl * fb; to_r = x + wr * fb; }
        dl_l[dl.w] = q16(soft_clip(to_l * drive) * post);
        dl_r[dl.w] = q16(soft_clip(to_r * drive) * post);
        dl.w = (dl.w + 1u) & DL_MASK;
        out_l[i] += wl * dl.mix;
        out_r[i] += wr * dl.mix;
        peak_out = dsp_max(peak_out, dsp_abs(wl) + dsp_abs(wr));
    }
    dl.quiet = peak_in < 1e-6f && peak_out < 1e-5f ? dl.quiet + (unsigned)n / 16u : 0;
}

/* ==== reverb (Reverb: 8-line FDN, 4 diffusers, pre-delay, early reflections) ======================== */
enum { RV_LINES = 8, RV_CAP = 4096, RV_MASK = RV_CAP - 1, RV_DCAP = 1024, RV_DMASK = RV_DCAP - 1, RV_PCAP = 8192, RV_PMASK = RV_PCAP - 1 };
static float rv_lines[RV_LINES][RV_CAP] EL_POOL;
/* (in RAM, not the pool: the pool holds the lines and the delay; docs/adr/0009) */
#ifndef SYN_FX_DIFF_ATTR
#define SYN_FX_DIFF_ATTR                        /* (combo: in the RAM code area's data, combo/tools/build.py) */
#endif
#ifndef SYN_FX_PRE_ATTR
#define SYN_FX_PRE_ATTR                         /* (combo: in the pool) */
#endif
static float rv_diff[4][RV_DCAP] SYN_FX_DIFF_ATTR;
static int16_t rv_pre[RV_PCAP] SYN_FX_PRE_ATTR;  /* the reverb's input, 16 bit, +-2 full scale */
#define PRE_IN(x) (int16_t)(dsp_clamp((x), -1.99f, 1.99f) * 16384.0f)
#define PRE_OUT(i) ((float)rv_pre[(i) & RV_PMASK] * (1.0f / 16384.0f))
static const float base_len[RV_LINES] EL_DSP_DATA = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static const float diff_len[4] EL_DSP_DATA = {142, 107, 379, 277};
static const float er_l[6] EL_DSP_DATA = {4.3f, 8.9f, 13.1f, 19.7f, 27.3f, 36.1f};
static const float er_r[6] EL_DSP_DATA = {5.9f, 10.7f, 15.8f, 22.4f, 31.2f, 39.7f};
static const float er_g[6] EL_DSP_DATA = {0.84f, -0.71f, 0.6f, -0.5f, 0.41f, -0.33f};
static const float lfo_rates[RV_LINES] EL_DSP_DATA = {0.23f, 0.31f, 0.37f, 0.47f, 0.53f, 0.67f, 0.79f, 0.97f};
static struct {
    unsigned w, dw, pw;
    float damp[RV_LINES], lfo[RV_LINES], mix, size;
    unsigned quiet;
} rv;

static EL_DSP_CODE void reverb_process(const float *in, float *out_l, float *out_r, int n)
{
    const float sr = EL_SAMPLE_RATE;
    float mix = v[FX_REV_LEVEL] ? dsp_exp2(1.5f * dsp_log2(v[FX_REV_LEVEL] / 127.0f)) : 0.0f;
    float peak_in = 0, peak_out = 0;
    if (mix < 0.0005f && rv.mix < 0.0005f) return;
    for (int i = 0; i < n; i++) peak_in = dsp_max(peak_in, dsp_abs(in[i]));
    if (peak_in < 1e-6f && rv.quiet > 4000u) return;
    float size = v[FX_REV_SIZE] / 127.0f;
    if (rv.size < 0) rv.size = size;
    rv.size += (size - rv.size) * dsp_min(1.0f, (float)n / (0.05f * sr));
    float sz = rv.size;
    int kind = v[FX_REV_TYPE];
    float room = kind == 1 ? 0.2f + 0.9f * sz : kind == 2 ? 0.3f + 0.45f * sz : 0.18f + 2.6f * sz * sz;
    float t60 = 0.15f * dsp_exp2(v[FX_REV_DECAY] / 127.0f * 8.643856f);   /* 0.15 * 400^(d/127) */
    float len[RV_LINES], gain[RV_LINES], incs[RV_LINES];
    for (int i = 0; i < RV_LINES; i++) {
        len[i] = dsp_min((float)(RV_CAP - 64), base_len[i] * room);
        gain[i] = dsp_exp2(-3.0f * 3.321928f * len[i] / (t60 * sr));
        incs[i] = lfo_rates[i] / sr;
    }
    int dsize[4];
    float dscale = 0.35f + 1.4f * sz;
    for (int d = 0; d < 4; d++) { int s = (int)(diff_len[d] * dscale); dsize[d] = s < 1 ? 1 : s > RV_DCAP - 1 ? RV_DCAP - 1 : s; }
    float diffusion, pre_s, damp_top, mod_scale;
    switch (kind) {
    case 1: diffusion = 0.5f; pre_s = 0.012f * sz; damp_top = 12000; mod_scale = 0.3f; break;
    case 2: diffusion = 0.75f; pre_s = 0.002f; damp_top = 18000; mod_scale = 0.5f; break;
    default: diffusion = 0.55f + 0.17f * sz; pre_s = sz * sz * 0.06f; damp_top = 16000; mod_scale = 1; break;
    }
    int pre = (int)(pre_s * sr);
    pre = pre < 1 ? 1 : pre > RV_PCAP / 2 ? RV_PCAP / 2 : pre;
    float damp = v[FX_REV_DAMP] / 127.0f;
    float dcoef = 1.0f - dsp_exp(-2.0f * DSP_PI * dsp_min(damp_top - damp * (damp_top - 1500.0f), sr * 0.45f) / sr);
    float mdepth = v[FX_REV_MOD] / 127.0f * mod_scale * (1.5f + 14.0f * sz);
    float er_level = kind == 1 ? 0.35f : 0.0f;
    int tl[6] = {0}, tr[6] = {0};
    if (kind == 1) {
        float sc = (0.25f + 0.75f * sz) * sr / 1000.0f;
        for (int k = 0; k < 6; k++) {
            int a = (int)(er_l[k] * sc), b = (int)(er_r[k] * sc);
            tl[k] = (a < 1 ? 1 : a) + pre; if (tl[k] > RV_PCAP - 1) tl[k] = RV_PCAP - 1;
            tr[k] = (b < 1 ? 1 : b) + pre; if (tr[k] > RV_PCAP - 1) tr[k] = RV_PCAP - 1;
        }
    }
    const float hh = 2.0f / RV_LINES;
    for (int i = 0; i < n; i++) {
        rv.mix += (mix - rv.mix) * 0.002f;
        rv_pre[rv.pw] = PRE_IN(in[i]);
        float x = PRE_OUT(rv.pw - (unsigned)pre), el = 0, er = 0;
        if (er_level > 0) {
            for (int k = 0; k < 6; k++) {
                el += er_g[k] * PRE_OUT(rv.pw - (unsigned)tl[k]);
                er += er_g[k] * PRE_OUT(rv.pw - (unsigned)tr[k]);
            }
            x = 0.6f * x + 0.25f * (el + er);
        }
        rv.pw = (rv.pw + 1u) & RV_PMASK;
        for (int d = 0; d < 4; d++) {
            float del = rv_diff[d][(rv.dw - (unsigned)dsize[d]) & RV_DMASK], u = x + diffusion * del;
            rv_diff[d][rv.dw & RV_DMASK] = u;
            x = del - diffusion * u;
        }
        float y[RV_LINES], sum = 0;
        for (int l = 0; l < RV_LINES; l++) {
            float p = rv.lfo[l] + incs[l];
            if (p >= 1) p -= 1;
            rv.lfo[l] = p;
            float tri = p < 0.5f ? 4 * p - 1 : 3 - 4 * p;
            float pos = (float)rv.w - (len[l] + mdepth * tri);
            int k = floor_i(pos);
            float fr = pos - (float)k, a = rv_lines[l][k & RV_MASK], b = rv_lines[l][(k + 1) & RV_MASK];
            rv.damp[l] += (a + (b - a) * fr - rv.damp[l]) * dcoef;
            y[l] = rv.damp[l];
            sum += y[l] * gain[l];
        }
        sum *= hh;
        for (int l = 0; l < RV_LINES; l++)
            rv_lines[l][rv.w & RV_MASK] = y[l] * gain[l] - sum + x * (l & 1 ? -0.35f : 0.35f);
        rv.w = (rv.w + 1u) & RV_MASK;
        rv.dw = (rv.dw + 1u) & RV_DMASK;
        float wl = 0.4f * (y[0] - y[2] + y[4] - y[6] + 0.5f * y[1]) + el * er_level;
        float wr = 0.4f * (y[1] - y[3] + y[5] - y[7] + 0.5f * y[0]) + er * er_level;
        out_l[i] += wl * rv.mix;
        out_r[i] += wr * rv.mix;
        peak_out = dsp_max(peak_out, dsp_abs(wl) + dsp_abs(wr));
    }
    rv.quiet = peak_in < 1e-6f && peak_out < 1e-5f ? rv.quiet + (unsigned)n / 16u : 0;
}

/* ==== the module ======================================================================================== */
void syn_fx_init(void)
{
    memset(&dl, 0, sizeof dl);
    memset(&rv, 0, sizeof rv);
    memset(dl_l, 0, sizeof dl_l); memset(dl_r, 0, sizeof dl_r);
    memset(rv_lines, 0, sizeof rv_lines); memset(rv_diff, 0, sizeof rv_diff); memset(rv_pre, 0, sizeof rv_pre);
    dl.high_coef = 1.0f - dsp_exp(-2.0f * DSP_PI * 60.0f / EL_SAMPLE_RATE);
    dl.flutter = 0.37f;
    dl.noise = 0xEC401A70u;
    {
        static const float ph[RV_LINES] = {0.0f, 0.13f, 0.29f, 0.41f, 0.57f, 0.66f, 0.78f, 0.9f};
        for (int i = 0; i < RV_LINES; i++) rv.lfo[i] = ph[i];
    }
    rv.size = -1;
    for (unsigned i = 0; i < FX_COUNT; i++) v[i] = defv[i];
}
EL_DSP_CODE void syn_fx_set(unsigned param, int value)
{
    if (param >= FX_COUNT) return;
    v[param] = (uint8_t)(value < 0 ? 0 : value > maxv[param] ? maxv[param] : value);
}
EL_DSP_CODE void syn_fx_process(const float *dly_in, const float *rev_in, float *out_l, float *out_r, int n, float bpm)
{
    delay_process(dly_in, out_l, out_r, n, bpm);
    reverb_process(rev_in, out_l, out_r, n);
}
int syn_fx_max(unsigned param) { return param < FX_COUNT ? maxv[param] : 0; }
int syn_fx_default(unsigned param) { return param < FX_COUNT ? defv[param] : 0; }
const char *syn_fx_value_name(unsigned param, int value)
{
    switch (param) {
    case FX_DLY_PING: case FX_DLY_SYNC: return off_on[value ? 1 : 0];
    case FX_DLY_DIV: return div_names[value < 0 ? 0 : value >= FX_DIVS ? FX_DIVS - 1 : value];
    case FX_REV_TYPE: return type_names[value < 0 ? 0 : value >= FX_REV_TYPES ? FX_REV_TYPES - 1 : value];
    default: return 0;
    }
}
