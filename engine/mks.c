/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Port of the MKS-50 iOS app's DSP (DCO.swift, LadderFilter.swift,
 * Envelope.swift, HardwareCurves.swift, Chorus.swift, SynthEngine.swift).
 * The audio path uses no libc math: approximations from dsp_math.h. */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "mks.h"
#include "dsp_math.h"
#include <string.h>

enum { CONTROL = 16 };
enum { ENV_IDLE, ENV_T1, ENV_T2, ENV_T3, ENV_SUSTAIN, ENV_RELEASE };

/* The MKS-50 app's eight chords: the types measured in Rother's and
 * The Egyptian Lover's pads. */
const char *const mks_chord_names[MKS_CHORDS] = {"m7", "SUS2", "MIN", "5+8", "SUS4", "7TH", "MAJ", "m7b5"};
const int8_t mks_chords[MKS_CHORDS][MKS_CHORD_NOTES] EL_DSP_DATA = {
    {0, 3, 7, 10, -128}, {0, 2, 7, -128, 0}, {0, 3, 7, -128, 0}, {0, 7, 12, -128, 0},
    {0, 5, 7, -128, 0}, {0, 4, 7, 10, -128}, {0, 4, 7, -128, 0}, {0, 3, 6, 10, -128}};

const uint8_t st_chord_v15[14] = {0, 1, 1, 3, 4, 5, 5, 7, 8, 7, 2, 7, 7, 6};

/* ---- math ---------------------------------------------------------------- */

static inline float wrap01(float x)
{
    int i = (int)x;
    if ((float)i > x) --i;
    return x - (float)i;
}

static inline float polyblep(float t, float dt)
{
    if (t < dt) { float x = t / dt; return x + x - x * x - 1.0f; }
    if (t > 1.0f - dt) { float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
    return 0.0f;
}

static inline float soft_clip(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline float powf_fast(float a, float b) { return a <= 0.0f ? 0.0f : dsp_exp2(b * dsp_log2(a)); }
static inline float note_hz(float note) { return 440.0f * dsp_exp2((note - 69.0f) * (1.0f / 12.0f)); }

static inline float xorshift(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n ? n : 0x9e3779b9u;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

/* ---- hardware curves (june-21 measurements, see HardwareCurves.swift) ---- */

static const float curve_t3[9] EL_DSP_DATA = {0.09f, 0.184f, 0.266f, 0.474f, 1.032f, 3.03f, 11.864f, 23.102f, 37.58f};
static const float curve_t4[9] EL_DSP_DATA = {0.007f, 0.0564f, 0.2032f, 0.6632f, 1.904f, 5.2f, 11.454f, 23.182f, 39.34f};
static const float curve_lfo[9] EL_DSP_DATA = {0.02f, 0.0508f, 0.1434f, 0.4028f, 1.1935f, 3.2167f, 9.7766f, 25.6894f, 64.9854f};
static const float curve_pwm[9] EL_DSP_DATA = {0.02f, 0.0436f, 0.1048f, 0.2494f, 0.6535f, 1.5306f, 3.6868f, 8.7365f, 20.429f};
static const float curve_chorus[9] EL_DSP_DATA = {0.01f, 0.0217f, 0.0524f, 0.1243f, 0.3117f, 0.777f, 1.8335f, 4.83f, 10.2564f};
static const float curve_vibrato[9] EL_DSP_DATA = {0, 0.0212f, 0.0477f, 0.0736f, 0.104f, 0.1544f, 0.2174f, 0.3205f, 0.4279f};
/* LFO sync divisions in quarter notes: 1/32 ... 4/1 */
static const float divisions[14] EL_DSP_DATA = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3,
                                                1, 1.5f, 2, 4, 8, 16};

static inline float curve_exp(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] * powf_fast(t[i + 1] / t[i], f);
}

static inline float curve_lin(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] + (t[i + 1] - t[i]) * f;
}

static inline float env_seconds(float raw, int segment)
{
    if (segment == 3) return curve_exp(curve_t3, raw);
    if (segment == 4) return curve_exp(curve_t4, raw);
    return 0.354f * dsp_exp2((dsp_clamp(raw, 0, 127) - 64.0f) * 0.1f);
}

/* ---- DCO (MB87123 chop model) ------------------------------------------- */

static const mks_chop_t chop_off EL_DSP_DATA = {0, 0, 0.5f, 0, 0.5f, 0, 0.5f};

static inline int chop_enabled(const mks_chop_t *c) { return c->ramp > 0 || c->m0 > 0; }

static inline float chop_naive(const mks_chop_t *c, float p)
{
    if (c->m0 > 0 && wrap01(p * c->m0) < c->w0) return 0;
    if (c->m1 > 0 && wrap01(p * c->m1) < c->w1) return 0;
    if (c->m2 > 0 && wrap01(p * c->m2) < c->w2) return 0;
    return c->ramp > 0 ? wrap01(p * c->ramp) : 1.0f;
}

static EL_DSP_CODE float chop_render(const mks_chop_t *c, float p, float dt)
{
    /* Fast paths with the same result for the two most common shapes:
     * a plain ramp and a single gate (square / pulse). */
    if (c->m1 <= 0 && c->m2 <= 0) {
        if (c->m0 <= 0) {
            float t = wrap01(p * c->ramp), d = dt * c->ramp;
            return 2.0f * t - 1.0f - polyblep(t, d);
        }
        if (c->ramp <= 0) {
            float t = wrap01(p * c->m0), d = dt * c->m0, w = c->w0;
            float out = t >= w ? 1.0f : -1.0f;
            out += polyblep(wrap01(t - w), d) - polyblep(t, d);
            return out;
        }
    }
    float l0 = c->m0 > 0 ? wrap01(p * c->m0) : 0, l1 = c->m1 > 0 ? wrap01(p * c->m1) : 0;
    float l2 = c->m2 > 0 ? wrap01(p * c->m2) : 0, lr = c->ramp > 0 ? wrap01(p * c->ramp) : 0;
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
                    float h = chop_naive(c, wrap01(edge + 1e-5f)) - chop_naive(c, wrap01(edge - 1e-5f)); \
                    if (h != 0) out += h * polyblep(wrap01(d), dt);                          \
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

static void chop_set(mks_chop_t *c, float ramp, float m0, float w0, float m1, float w1)
{
    c->ramp = ramp; c->m0 = m0; c->w0 = w0; c->m1 = m1; c->w1 = w1; c->m2 = 0; c->w2 = 0.5f;
}

/* DCOSettings.configure: pulse on the pulse phase (1 = main), saw/sub on the
 * master phase (4 = main). Only Pulse 3 and Saw 3 follow PW / PWM. */
static EL_DSP_CODE void dco_configure(mks_part_t *p, int pulse, int saw, int sub, float pw)
{
    switch (pulse) {
    case 1: chop_set(&p->pulse, 0, 1, 0.5f, 0, 0.5f); break;
    case 2: chop_set(&p->pulse, 0, 1, 0.5f, 2, 0.5f); break;
    case 3: chop_set(&p->pulse, 0, 1, 0.5f, 2, pw); break;
    default: p->pulse = chop_off; break;
    }
    switch (saw) {
    case 1: chop_set(&p->saw, 4, 0, 0.5f, 0, 0.5f); break;
    case 2: chop_set(&p->saw, 4, 8, 0.5f, 0, 0.5f); break;
    case 3: chop_set(&p->saw, 4, 8, pw, 0, 0.5f); break;
    case 4: chop_set(&p->saw, 4, 32, 0.5f, 0, 0.5f); break;
    case 5: chop_set(&p->saw, 4, 8, 0.5f, 32, 0.5f); break;
    default: p->saw = chop_off; break;
    }
    switch (sub) {
    case 1: chop_set(&p->sub, 0, 2, 0.5f, 4, 0.5f); break;
    case 2: chop_set(&p->sub, 0, 2, 0.5f, 8, 0.5f); break;
    case 3: chop_set(&p->sub, 0, 2, 0.5f, 16, 0.5f); break;
    case 4: chop_set(&p->sub, 0, 1, 0.5f, 0, 0.5f); break;
    case 5: chop_set(&p->sub, 0, 1, 0.5f, 2, 0.5f); break;
    default: chop_set(&p->sub, 0, 2, 0.5f, 0, 0.5f); break;
    }
}

static inline float dco_tick(const mks_part_t *p, mks_voice_t *v, float raw_dt, float ratio, int sync)
{
    float dt = raw_dt < 0.45f ? raw_dt : 0.45f, dtm = dt * 0.25f;
    v->phase += dtm;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    float main = wrap01(v->phase * 4.0f);
    int wrapped = main < v->last_main;
    v->last_main = main;
    float out = 0;
    if (chop_enabled(&p->pulse)) {
        if (ratio == 1.0f && !sync) {
            v->pulse_phase = main;
            out += chop_render(&p->pulse, main, dt);
        } else {
            float pdt = dt * ratio;
            if (pdt > 0.45f) pdt = 0.45f;
            if (sync && wrapped) {
                float since = main / (dt > 1e-9f ? dt : 1e-9f);
                float before = wrap01(v->pulse_phase + pdt * (1.0f - since));
                v->pulse_phase = wrap01(pdt * since);
                float h = chop_naive(&p->pulse, 0) - chop_naive(&p->pulse, before);
                out += 2.0f * chop_naive(&p->pulse, v->pulse_phase) - 1.0f + h * polyblep(main, dt);
            } else {
                v->pulse_phase += pdt;
                if (v->pulse_phase >= 1.0f) v->pulse_phase = wrap01(v->pulse_phase);
                out += chop_render(&p->pulse, v->pulse_phase, pdt);
            }
        }
    }
    if (chop_enabled(&p->saw)) out += chop_render(&p->saw, v->phase, dtm);
    if (p->sub_level > 0) out += chop_render(&p->sub, v->phase, dtm) * p->sub_level;
    if (p->noise_level > 0) out += xorshift(&v->noise) * p->noise_level;
    return out;
}

/* ---- envelope -------------------------------------------------------------- */

static void env_begin(mks_env_t *e, int stage, float level, float seconds)
{
    float samples = seconds * EL_SAMPLE_RATE;
    e->stage = (uint8_t)stage;
    e->start = e->value;
    e->target = level;
    e->progress = 0;
    e->increment = 1.0f / (samples > 1.0f ? samples : 1.0f);
    e->curve = 1.0f;
    e->curve_decay = dsp_exp(-4.5f * e->increment);
}

static inline float env_next(mks_env_t *e, const mks_env_settings_t *s)
{
    switch (e->stage) {
    case ENV_IDLE: return 0;
    case ENV_SUSTAIN:
        e->value += (s->l3 - e->value) * 0.002f;
        return e->value;
    default:
        e->progress += e->increment;
        if (e->progress >= 1.0f) {
            e->value = e->target;
            switch (e->stage) {
            case ENV_T1: env_begin(e, ENV_T2, s->l2, s->t2); break;
            case ENV_T2: env_begin(e, ENV_T3, s->l3, s->t3); break;
            case ENV_T3: e->stage = ENV_SUSTAIN; break;
            default: e->stage = ENV_IDLE; e->value = 0; break;
            }
        } else {
            float shape;
            if (e->stage == ENV_T1) shape = e->progress;
            else { e->curve *= e->curve_decay; shape = (1.0f - e->curve) * 1.011235f; }
            e->value = e->start + (e->target - e->start) * shape;
        }
        return e->value;
    }
}

/* ---- filters --------------------------------------------------------------- */

static inline void ladder_set(mks_voice_t *v, float cutoff, float resonance)
{
    float fc = dsp_clamp(cutoff, 8.0f, EL_SAMPLE_RATE * 0.45f);
    v->g = dsp_tan(DSP_PI * fc / EL_SAMPLE_RATE);
    v->G = v->g / (1.0f + v->g);
    v->k = resonance * 3.8f;
    v->G2 = v->G * v->G;
    v->G3 = v->G2 * v->G;
    v->G4 = v->G3 * v->G;
    v->inv = 1.0f / (1.0f + v->g);
    v->den = 1.0f / (1.0f + v->k * v->G4);
}

static inline float ladder(mks_voice_t *v, float input)
{
    float G = v->G, G2 = v->G2, G3 = v->G3, G4 = v->G4;
    float sigma = (G3 * v->s1 + G2 * v->s2 + G * v->s3 + v->s4) * v->inv;
    float x = input * (1.0f + 0.55f * v->k) + 1e-20f;
    float y4e = (G4 * x + sigma) * v->den;
    float u = 2.0f * soft_clip(0.5f * (x - v->k * y4e));
    float t = (u - v->s1) * G, y1 = t + v->s1; v->s1 = y1 + t;
    t = (y1 - v->s2) * G; float y2 = t + v->s2; v->s2 = y2 + t;
    t = (y2 - v->s3) * G; float y3 = t + v->s3; v->s3 = y3 + t;
    t = (y3 - v->s4) * G; float y4 = t + v->s4; v->s4 = y4 + t;
    return y4;
}

static void hpf_configure(mks_part_t *p, int mode)
{
    mks_biquad_t *c = &p->hpf;
    p->hpf_mode = (uint8_t)mode;
    p->hpf_on = mode != 1;
    if (mode == 0) {
        /* Bass boost: low shelf +6 dB at 70 Hz. */
        float a = dsp_exp2(6.0f / 40.0f * 3.321928f), w = 2 * DSP_PI * 70.0f / EL_SAMPLE_RATE;
        float cw = dsp_cos(w), sw = dsp_sin(w), alpha = sw / 2 * 1.41421356f, sa = 2 * dsp_sqrt(a) * alpha;
        float a0 = (a + 1) + (a - 1) * cw + sa;
        c->b0 = a * ((a + 1) - (a - 1) * cw + sa) / a0;
        c->b1 = 2 * a * ((a - 1) - (a + 1) * cw) / a0;
        c->b2 = a * ((a + 1) - (a - 1) * cw - sa) / a0;
        c->a1 = -2 * ((a - 1) + (a + 1) * cw) / a0;
        c->a2 = ((a + 1) + (a - 1) * cw - sa) / a0;
    } else if (mode >= 2) {
        float k = dsp_tan(DSP_PI * (mode == 2 ? 225.0f : 700.0f) / EL_SAMPLE_RATE), a0 = 1 + k;
        c->b0 = 1 / a0; c->b1 = -1 / a0; c->b2 = 0; c->a1 = (k - 1) / a0; c->a2 = 0;
    }
}

static inline float biquad(const mks_biquad_t *c, float *z1, float *z2, float x)
{
    float y = c->b0 * x + *z1;
    *z1 = c->b1 * x - c->a1 * y + *z2;
    *z2 = c->b2 * x - c->a2 * y;
    return y;
}

/* ---- chorus (MN3009 BBD, Chorus.swift) ----------------------------------- */

/* CHORUS: OFF, FREE (CHO RATE, as on the MKS-50), and the Juno-60's
 * buttons I, II and I+II with their fixed rates and depths. mode 0 = FREE. */
static EL_DSP_CODE void chorus(mks_chorus_t *c, float *left, float *right, int frames, int enabled, int mode, float rate)
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
        c->buffer[c->write & (MKS_CHORUS_SIZE - 1)] = soft_clip(c->lp2 * 0.8f) * 1.25f;
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
            float frac = pos - (float)i, a = c->buffer[i & (MKS_CHORUS_SIZE - 1)];
            wet[side] = a + (c->buffer[(i + 1) & (MKS_CHORUS_SIZE - 1)] - a) * frac;
        }
        float noise = xorshift(&c->hiss) * 0.00006f;
        c->wl1 += (wet[0] + noise - c->wl1) * lp;
        c->wr1 += (wet[1] - noise - c->wr1) * lp;
        c->wl2 += (c->wl1 - c->wl2) * lp;
        c->wr2 += (c->wr1 - c->wr2) * lp;
        float dry = 1.0f - 0.3f * c->mix;
        left[n] = xl * dry + c->wl2 * 0.75f * c->mix;
        right[n] = xr * dry + c->wr2 * 0.75f * c->mix;
        c->write = (c->write + 1u) & (MKS_CHORUS_SIZE - 1);
    }
}

/* ---- part ------------------------------------------------------------------ */

void mks_part_init(mks_part_t *p, unsigned voices, uint32_t seed)
{
    memset(p, 0, sizeof *p);
    p->voices = (uint8_t)(voices < 1 ? 1 : voices > MKS_MAX_VOICES ? MKS_MAX_VOICES : voices);
    for (unsigned i = 0; i < MKS_MAX_VOICES; ++i) {
        mks_voice_t *v = &p->voice[i];
        v->key = -1;
        v->noise = seed + i * 0x9e3779b9u;
        v->rng = (seed + i) * 2654435761u;
        if (!v->rng) v->rng = 1;
        v->drift_pitch = xorshift(&v->rng);
        v->drift_cutoff = xorshift(&v->rng);
        v->drift_env = xorshift(&v->rng);
        v->lfo_fade = 1;
    }
    p->lfo_rng = seed ^ 0xa5a5a5a5u;
    p->chorus.hiss = 0xb0b0cafeu ^ seed;
    p->chorus.depth = 80;
    p->level = 1;
    p->hpf_mode = 1;
    mks_part_load(p, mks_presets[0].v);
}

EL_DSP_CODE int mks_active(const mks_part_t *p)
{
    for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].active) return 1;
    return p->chorus.mix > 0.0001f && p->silent_blocks < 40;
}

EL_DSP_CODE void mks_all_off(mks_part_t *p)
{
    for (unsigned i = 0; i < MKS_MAX_VOICES; ++i) {
        mks_voice_t *v = &p->voice[i];
        if (v->gate) { v->gate = 0; env_begin(&v->env, ENV_RELEASE, 0, env_seconds(p->v[MP_T4], 4)); }
    }
    p->held_count = 0;
}

EL_DSP_CODE void mks_part_load(mks_part_t *p, const uint8_t v[MP_COUNT])
{
    int mode_changed = p->v[MP_KEY_MODE] != v[MP_KEY_MODE];
    for (unsigned i = 0; i < MP_COUNT; ++i) p->v[i] = v[i];
    if (mode_changed) mks_all_off(p);
    hpf_configure(p, p->v[MP_HPF]);
    p->cutoff_smoothed = p->v[MP_CUTOFF];
}

EL_DSP_CODE void mks_part_set(mks_part_t *p, unsigned param, int value)
{
    if (param >= MP_COUNT) return;
    int mode_changed = param == MP_KEY_MODE && p->v[param] != value;
    p->v[param] = (uint8_t)el_clampi(value, 0, mks_param_max(param));
    if (mode_changed) mks_all_off(p);
    if (param == MP_HPF) hpf_configure(p, p->v[MP_HPF]);
}

static inline float env_scale(const mks_part_t *p, const mks_voice_t *v, float pitch)
{
    float follow = p->v[MP_ENV_KEY] / 127.0f;
    float scale = 1.0f + p->v[MP_DRIFT] / 127.0f * v->drift_env * 0.08f;
    if (follow > 0) scale *= dsp_exp2(-follow * (pitch - 60.0f) / 24.0f);
    return scale;
}

static void env_settings(const mks_part_t *p, const mks_voice_t *v, float pitch, mks_env_settings_t *s)
{
    float k = env_scale(p, v, pitch);
    s->t1 = env_seconds(p->v[MP_T1], 1) * k; s->l1 = p->v[MP_L1] / 127.0f;
    s->t2 = env_seconds(p->v[MP_T2], 2) * k; s->l2 = p->v[MP_L2] / 127.0f;
    s->t3 = env_seconds(p->v[MP_T3], 3) * k; s->l3 = p->v[MP_L3] / 127.0f;
    s->t4 = env_seconds(p->v[MP_T4], 4) * k;
}

static float last_pitch_of(const mks_part_t *p)
{
    uint32_t best = 0; float pitch = -1;
    for (unsigned i = 0; i < p->voices; ++i)
        if (p->voice[i].active && p->voice[i].age >= best) { best = p->voice[i].age; pitch = p->voice[i].pitch; }
    return pitch;
}

static void start_voice(mks_part_t *p, mks_voice_t *v, int note, float velocity, int glide, float from, int retrigger)
{
    mks_env_settings_t s;
    int was_active = v->active;
    v->key = (int16_t)note;
    v->velocity = velocity;
    v->gate = 1;
    v->age = ++p->age;
    v->target_pitch = (float)note + (float)((int)p->v[MP_KEY_SHIFT] - 12);
    v->pitch = glide && from >= 0 ? from : v->target_pitch;
    if (!retrigger && was_active) return;
    v->lfo_fade = p->v[MP_LFO_DELAY] > 0 ? 0.0f : 1.0f;
    if (!was_active) {
        v->s1 = v->s2 = v->s3 = v->s4 = 0;
        v->hz1 = v->hz2 = 0;
        v->hpf_fc = -1;
        v->env.stage = ENV_IDLE;
        v->env.value = 0;
        v->gate_amp = 0;
    }
    v->active = 1;
    env_settings(p, v, v->target_pitch, &s);
    env_begin(&v->env, ENV_T1, s.l1, s.t1);
}

static mks_voice_t *find_voice(mks_part_t *p)
{
    mks_voice_t *best = 0;
    for (unsigned i = 0; i < p->voices; ++i) if (!p->voice[i].active) return &p->voice[i];
    for (unsigned i = 0; i < p->voices; ++i)
        if (!p->voice[i].gate && (!best || p->voice[i].age < best->age)) best = &p->voice[i];
    if (best) return best;
    for (unsigned i = 0; i < p->voices; ++i)
        if (!best || p->voice[i].age < best->age) best = &p->voice[i];
    return best;
}

static int any_gate(const mks_part_t *p)
{
    for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].gate) return 1;
    return 0;
}

EL_DSP_CODE void mks_note_on(mks_part_t *p, int note, float velocity, int legato)
{
    int mode = p->v[MP_KEY_MODE], porta = p->v[MP_PORTA];
    int held = any_gate(p) || legato;
    int glide = porta == 1 || (porta == 2 && held);
    float from = last_pitch_of(p);
    if (p->v[MP_LFO_KEYSYNC] && !held) {
        p->lfo_phase = 0;
        p->lfo_held = xorshift(&p->lfo_rng);
        p->lfo_from = p->lfo_to;
    }
    if (mode == 2 || mode == 3) {
        /* Mono / unison: newest note wins; legato keeps the envelope running
         * when ENV RETRIGGER is LEGATO or the step is tied. */
        if (p->held_count < 8) p->held[p->held_count++] = (int16_t)note;
        int retrigger = !(held && (p->v[MP_ENV_RETRIG] == 1 || legato));
        unsigned count = mode == 3 ? p->voices : 1u;
        for (unsigned i = 0; i < count; ++i) {
            mks_voice_t *v = &p->voice[i];
            v->unison_spread = count > 1 ? -1.0f + 2.0f * (float)i / (float)(count - 1) : 0.0f;
            start_voice(p, v, note, velocity, glide, from, retrigger || !v->active);
        }
        return;
    }
    mks_voice_t *v = 0;
    for (unsigned i = 0; i < p->voices; ++i)
        if (p->voice[i].active && p->voice[i].key == note) { v = &p->voice[i]; break; }
    if (!v) v = find_voice(p);
    v->unison_spread = 0;
    start_voice(p, v, note, velocity, glide, from, 1);
}

static void release(mks_part_t *p, mks_voice_t *v)
{
    mks_env_settings_t s;
    v->gate = 0;
    env_settings(p, v, v->pitch, &s);
    env_begin(&v->env, ENV_RELEASE, 0, s.t4);
}

EL_DSP_CODE void mks_note_off(mks_part_t *p, int note)
{
    int mode = p->v[MP_KEY_MODE];
    if (mode == 2 || mode == 3) {
        unsigned n = 0;
        for (unsigned i = 0; i < p->held_count; ++i) if (p->held[i] != note) p->held[n++] = p->held[i];
        p->held_count = (uint8_t)n;
        unsigned count = mode == 3 ? p->voices : 1u;
        if (n) {
            /* Fall back to the newest still-held note, legato. */
            for (unsigned i = 0; i < count; ++i) {
                mks_voice_t *v = &p->voice[i];
                if (!v->gate) continue;
                v->key = p->held[n - 1];
                v->target_pitch = (float)p->held[n - 1] + (float)((int)p->v[MP_KEY_SHIFT] - 12);
                if (!p->v[MP_PORTA]) v->pitch = v->target_pitch;
            }
            return;
        }
        for (unsigned i = 0; i < count; ++i) if (p->voice[i].gate) release(p, &p->voice[i]);
        return;
    }
    for (unsigned i = 0; i < p->voices; ++i)
        if (p->voice[i].active && p->voice[i].gate && p->voice[i].key == note) release(p, &p->voice[i]);
}

/* Per control block: LFO, PWM, waveforms and shared modulation amounts. */
typedef struct {
    float vibrato, dco_env, pitch_offset, porta_coef, lfo_fade_inc, drift, unison, pulse_detune, pulse_env;
    float cutoff_oct, key_follow, vcf_env, vcf_lfo, resonance, amp, gate_coef, gate_release;
    int dco_env_mode, vcf_env_mode, gate_mode, dynamics, sync, porta;
} block_t;

static EL_DSP_CODE void control(mks_part_t *p, block_t *b, float bpm)
{
    const uint8_t *v = p->v;
    const float n = CONTROL;
    int wrapped = 0;
    /* LFO */
    if (v[MP_LFO_SYNC]) {
        float quarters = divisions[v[MP_LFO_DIV] < 14 ? v[MP_LFO_DIV] : 13];
        p->lfo_phase += (bpm < 20 ? 20 : bpm) / 60.0f / quarters * n / EL_SAMPLE_RATE;
    } else {
        p->lfo_phase += curve_exp(curve_lfo, v[MP_LFO_RATE]) * n / EL_SAMPLE_RATE;
    }
    if (p->lfo_phase >= 1.0f) { p->lfo_phase = wrap01(p->lfo_phase); wrapped = 1; }
    float ph = p->lfo_phase;
    switch (v[MP_LFO_WAVE]) {
    case 1: p->lfo_value = dsp_sin(2 * DSP_PI * ph); break;
    case 2: p->lfo_value = 2 * ph - 1; break;
    case 3: p->lfo_value = 1 - 2 * ph; break;
    case 4: p->lfo_value = ph < 0.5f ? 1.0f : -1.0f; break;
    case 5:
        if (wrapped) p->lfo_held = xorshift(&p->lfo_rng);
        p->lfo_value = p->lfo_held;
        break;
    case 6:
        if (wrapped) { p->lfo_from = p->lfo_to; p->lfo_to = xorshift(&p->lfo_rng); }
        p->lfo_value = p->lfo_from + (p->lfo_to - p->lfo_from) * (0.5f - 0.5f * dsp_cos(DSP_PI * ph));
        break;
    default: p->lfo_value = 1 - 4 * dsp_abs(ph - 0.5f); break;
    }
    /* PW / PWM: rate 0 = manual width set by the depth control. */
    float depth = v[MP_PW] / 128.0f, pw = depth;
    if (v[MP_PWM_RATE]) {
        p->pwm_phase += curve_exp(curve_pwm, v[MP_PWM_RATE]) * n / EL_SAMPLE_RATE;
        if (p->pwm_phase >= 1.0f) p->pwm_phase = wrap01(p->pwm_phase);
        pw = depth * (1 - dsp_abs(2 * p->pwm_phase - 1));
    }
    dco_configure(p, v[MP_PULSE], v[MP_SAW], v[MP_SUB], pw);
    p->sub_level = v[MP_SUB_LEVEL] ? dsp_exp2((float)v[MP_SUB_LEVEL] - 3.0f) : 0.0f;
    p->noise_level = v[MP_NOISE] / 3.0f * 0.5f;
    p->cutoff_smoothed += (v[MP_CUTOFF] - p->cutoff_smoothed) * dsp_min(1, n / (0.01f * EL_SAMPLE_RATE));
    float volume = (v[MP_VOLUME] / 127.0f) * (v[MP_VOLUME] / 127.0f);
    p->volume_smoothed += (volume - p->volume_smoothed) * dsp_min(1, n / (0.02f * EL_SAMPLE_RATE));
    if (p->hpf_mode != v[MP_HPF]) hpf_configure(p, v[MP_HPF]);

    b->dco_env_mode = v[MP_DCO_ENV_MODE];
    b->vcf_env_mode = v[MP_VCF_ENV_MODE];
    b->gate_mode = v[MP_VCA_ENV_MODE] == 1 || v[MP_VCA_ENV_MODE] == 3;
    b->dynamics = v[MP_VCA_ENV_MODE] >= 2;
    float frac = curve_lin(curve_vibrato, v[MP_DCO_LFO]);
    b->vibrato = 12.0f * dsp_log2(1 + frac / 2);
    b->dco_env = (v[MP_DCO_ENV] / 127.0f) * (v[MP_DCO_ENV] / 127.0f) * 24.0f;
    b->pitch_offset = (float)((1 - (int)v[MP_RANGE]) * 12);
    b->porta = v[MP_PORTA];
    float porta_seconds = (v[MP_PORTA_TIME] / 127.0f) * (v[MP_PORTA_TIME] / 127.0f) * 3.0f;
    b->porta_coef = porta_seconds < 0.001f ? 1.0f : 1.0f - dsp_exp(-n / (porta_seconds * EL_SAMPLE_RATE));
    float delay = v[MP_LFO_DELAY] ? dsp_exp(v[MP_LFO_DELAY] / 14.2f - 3.0f) / 10.0f : 0.0f;
    b->lfo_fade_inc = delay < 0.001f ? 1.0f : n / (delay * EL_SAMPLE_RATE);
    b->drift = v[MP_DRIFT] / 127.0f;
    b->unison = powf_fast(v[MP_UNISON_DETUNE] / 127.0f, 1.5f) * 0.5f;
    b->pulse_detune = ((float)v[MP_PULSE_DETUNE] - 64.0f) / 64.0f * 12.0f;
    b->pulse_env = v[MP_PULSE_ENV] / 127.0f * 36.0f;
    b->sync = v[MP_PULSE_SYNC] == 1;
    b->cutoff_oct = p->cutoff_smoothed / 12.0f - 0.878f;
    b->key_follow = v[MP_VCF_KEY] / 127.0f / 12.0f;
    b->vcf_env = v[MP_VCF_ENV] / 12.0f;
    b->vcf_lfo = v[MP_VCF_LFO] / 48.0f;
    b->resonance = v[MP_RESO] / 127.0f;
    b->amp = v[MP_VCA_LEVEL] / 127.0f;
    b->gate_coef = 0.0730f;    /* 1 - exp(-1 / (0.0003 * 44100)) */
    b->gate_release = 0.00189f; /* 1 - exp(-1 / (0.012 * 44100)) */
}

static inline float env_amount(float env, int mode, float velocity)
{
    switch (mode) {
    case 1: return -env;
    case 2: return env * (0.25f + 0.75f * velocity);
    case 3: return -env * (0.25f + 0.75f * velocity);
    default: return env;
    }
}

static EL_DSP_CODE void render_voice(mks_part_t *p, mks_voice_t *v, const block_t *b, float *left, float *right, int n)
{
    if (b->porta) v->pitch += (v->target_pitch - v->pitch) * b->porta_coef;
    else v->pitch = v->target_pitch;
    float pitch = v->pitch, drift_semis = 0, drift_oct = 0;
    if (b->drift > 0) {
        v->wander += xorshift(&v->rng) * 0.02f * (float)n / 16.0f - v->wander * 0.0005f * (float)n;
        v->wander = dsp_clamp(v->wander, -1, 1);
        drift_semis = b->drift * (v->drift_pitch * 0.08f + v->wander * 0.04f);
        drift_oct = b->drift * v->drift_cutoff * 0.15f;
    }
    if (v->lfo_fade < 1.0f) v->lfo_fade = dsp_min(1, v->lfo_fade + b->lfo_fade_inc);
    float lfo = p->lfo_value * v->lfo_fade;
    float env = v->env.value;
    float dco_env = env_amount(env, b->dco_env_mode, v->velocity);
    float vcf_env = env_amount(env, b->vcf_env_mode, v->velocity);
    float semis = pitch + b->pitch_offset + lfo * b->vibrato + dco_env * b->dco_env + drift_semis +
                  v->unison_spread * b->unison;
    float dt = note_hz(semis) / EL_SAMPLE_RATE;
    float pulse_semis = b->pulse_detune + dco_env * b->pulse_env;
    float ratio = pulse_semis == 0 ? 1.0f : dsp_exp2(pulse_semis / 12.0f);
    float octaves = b->cutoff_oct + b->key_follow * (pitch - 60.0f) + vcf_env * b->vcf_env + lfo * b->vcf_lfo + drift_oct;
    ladder_set(v, 20.0f * dsp_exp2(dsp_clamp(octaves, -2, 11)), b->resonance);
    int unison = p->v[MP_KEY_MODE] == 3 && p->voices > 1;
    float amp = b->amp * (b->dynamics ? 0.2f + 0.8f * v->velocity : 1.0f) * (unison ? 0.45f : 1.0f);
    float pan = unison ? v->unison_spread * 0.85f : 0.0f;
    float gl = amp * dsp_min(1, 1 - pan), gr = amp * dsp_min(1, 1 + pan);
    float gate_target = v->gate ? 1.0f : 0.0f, gate_coef = v->gate ? b->gate_coef : b->gate_release;
    /* HPF 225 / 700 Hz: the corner never above the voice's own fundamental,
     * so low notes keep their bass and higher ones sound as before. */
    int hpf_voice = p->hpf_on && p->hpf_mode >= 2;
    if (hpf_voice) {
        float fc = dsp_min(p->hpf_mode == 2 ? 225.0f : 700.0f, dt * EL_SAMPLE_RATE);
        if (dsp_abs(fc - v->hpf_fc) > 0.005f * fc) {
            float k = dsp_tan(DSP_PI * fc / EL_SAMPLE_RATE);
            v->hpf_fc = fc;
            v->hpf_b0 = 1.0f / (1.0f + k);
            v->hpf_a1 = (k - 1.0f) * v->hpf_b0;
        }
    }
    mks_env_settings_t s;
    env_settings(p, v, pitch, &s);
    for (int k = 0; k < n; ++k) {
        float e = env_next(&v->env, &s);
        v->gate_amp += (gate_target - v->gate_amp) * gate_coef;
        float x = dco_tick(p, v, dt, ratio, b->sync) * 0.5f;
        if (hpf_voice) {
            float y = v->hpf_b0 * x + v->hz1;
            v->hz1 = -v->hpf_b0 * x - v->hpf_a1 * y;
            x = y;
        } else if (p->hpf_on) x = biquad(&p->hpf, &v->hz1, &v->hz2, x);
        x = ladder(v, x) * (b->gate_mode ? v->gate_amp : e);
        left[k] += x * gl;
        right[k] += x * gr;
    }
    if (b->gate_mode) { if (!v->gate && v->gate_amp < 0.0001f) v->active = 0; }
    else if (v->env.stage == ENV_IDLE) v->active = 0;
    if (!v->active) v->key = -1;
}

EL_DSP_CODE void mks_render(mks_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm)
{
    static float l[EL_BLOCK], r[EL_BLOCK];
    static block_t b;
    if (frames <= 0) return;
    int any = 0;
    for (unsigned i = 0; i < p->voices; ++i) any |= p->voice[i].active;
    if (!any && p->chorus.mix < 0.0001f) { p->silent_blocks = 40; return; }
    for (int i = 0; i < frames; ++i) l[i] = r[i] = 0;
    for (int pos = 0; pos < frames; pos += CONTROL) {
        int n = frames - pos < CONTROL ? frames - pos : CONTROL;
        control(p, &b, bpm);
        for (unsigned i = 0; i < p->voices; ++i)
            if (p->voice[i].active) render_voice(p, &p->voice[i], &b, l + pos, r + pos, n);
    }
    float g = p->volume_smoothed * 0.5f * p->level;
    for (int i = 0; i < frames; ++i) { l[i] *= g; r[i] *= g; }
    chorus(&p->chorus, l, r, frames, p->v[MP_CHORUS] > 0, p->v[MP_CHORUS] > 0 ? p->v[MP_CHORUS] - 1 : 0,
           curve_exp(curve_chorus, p->v[MP_CHORUS_RATE]));
    float ds = powf_fast(p->v[MP_DELAY_SEND] / 127.0f, 1.5f), rs = powf_fast(p->v[MP_REVERB_SEND] / 127.0f, 1.5f);
    float peak = 0;
    for (int i = 0; i < frames; ++i) {
        left[i] += l[i];
        right[i] += r[i];
        float m = 0.5f * (l[i] + r[i]);
        dly[i] += m * ds;
        rev[i] += m * rs;
        peak = dsp_max(peak, dsp_abs(m));
    }
    p->silent_blocks = any || peak > 1e-5f ? 0 : (uint8_t)(p->silent_blocks < 255 ? p->silent_blocks + 1 : 255);
}

/* ---- display helpers ------------------------------------------------------- */

static const uint8_t max_values[MP_COUNT] = {
    3, 3, 3, 3, 5, 5, 3, 3, 3, 3, 4, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 12,
    3, 127, 2, 127, 24, 127, 1, 127, 127, 127, 6, 1, 13, 1, 1, 127, 127, MKS_CHORDS - 1};

int mks_param_max(unsigned param) { return param < MP_COUNT ? max_values[param] : 0; }

static const char *const names[MP_COUNT] = {
    "DCO EMODE", "VCF EMODE", "VCA MODE", "PULSE", "SAW", "SUB", "RANGE", "SUB LVL", "NOISE", "HPF", "CHORUS",
    "VIBRATO", "PITCH ENV", "DCO AFT", "PW", "PWM RATE", "CUTOFF", "RESO", "VCF LFO", "VCF ENV", "KEY FLW",
    "VCF AFT", "LEVEL", "VCA AFT", "LFO RATE", "LFO DLY", "ATTACK", "L1", "DECAY", "L2", "T3", "SUSTAIN",
    "RELEASE", "ENV KEY", "CHO RATE", "BEND", "KEY MODE", "GLIDE", "PORTA", "VOLUME", "SHIFT", "DETUNE",
    "SYNC", "SYNC ENV", "DRIFT", "UNI DET", "LFO WAVE", "LFO SYNC", "LFO DIV", "LFO KEY", "RETRIG",
    "DELAY", "REVERB", "CHORD"};

const char *mks_param_name(unsigned param) { return param < MP_COUNT ? names[param] : ""; }

const char *mks_param_value_name(unsigned param, int value)
{
    static const char *const env_modes[] = {"ENV", "INV", "ENV-D", "INV-D"};
    static const char *const vca_modes[] = {"ENV", "GATE", "ENV-D", "GATE-D"};
    static const char *const ranges[] = {"4'", "8'", "16'", "32'"};
    static const char *const off_on[] = {"OFF", "ON"};
    static const char *const chorus_modes[] = {"OFF", "FREE", "I", "II", "I+II"};
    static const char *const key_modes[] = {"POLY", "CHORD", "MONO", "UNISON"};
    static const char *const porta[] = {"OFF", "ON", "AUTO"};
    static const char *const waves[] = {"TRI", "SINE", "SAW UP", "SAW DN", "SQUARE", "S&H", "RANDOM"};
    static const char *const divs[] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T",
                                       "1/4", "1/4.", "1/2", "1 BAR", "2 BARS", "4 BARS"};
    static const char *const retrig[] = {"MULTI", "LEGATO"};
    value = el_clampi(value, 0, mks_param_max(param));
    switch (param) {
    case MP_DCO_ENV_MODE: case MP_VCF_ENV_MODE: return env_modes[value];
    case MP_VCA_ENV_MODE: return vca_modes[value];
    case MP_RANGE: return ranges[value];
    case MP_CHORUS: return chorus_modes[value];
    case MP_PULSE_SYNC: case MP_LFO_SYNC: case MP_LFO_KEYSYNC: return off_on[value];
    case MP_KEY_MODE: return key_modes[value];
    case MP_PORTA: return porta[value];
    case MP_LFO_WAVE: return waves[value];
    case MP_LFO_DIV: return divs[value];
    case MP_ENV_RETRIG: return retrig[value];
    case MP_CHORD: return mks_chord_names[value];
    default: return 0;
    }
}
