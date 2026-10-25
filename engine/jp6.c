/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Port of the JP-6 iOS app's sound engine (VCO.swift, VCF.swift,
 * Envelope.swift, HardwareCurves.swift, LFOShape.swift, Chorus.swift,
 * Effects.swift AcidDistortion, SynthEngine.swift voice path). Arpeggiator,
 * sequencer, looper, stutter, MIDI clock, performance controllers, the
 * app-wide delay / reverb / limiter and its CPU load shedding are left out.
 * The audio path uses no libc math: approximations from dsp_math.h. */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "jp6.h"
#include "dsp_math.h"
#include <string.h>

enum { CONTROL = 16, SILENT_BLOCKS = 40 };
enum { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
enum { W_TRI = 1, W_SAW = 2, W_PULSE = 4, W_SQUARE = 8, W_NOISE = 16, W_WAVES = 15 };

/* ChordMemory.swift defaults (the same chords as mks_chords[0..7]). */
const char *const jp6_chord_names[JP6_CHORDS] = {"m7", "SUS2", "MIN", "5+8", "SUS4", "7TH", "MAJ", "m7b5"};

/* ---- math ---------------------------------------------------------------- */

static inline float wrap01(float x)
{
    int i = (int)x;
    if ((float)i > x) --i;
    return x - (float)i;
}

/* wrap01 for 0 <= x < 2 */
static inline float wrap1(float x) { return x >= 1.0f ? x - 1.0f : x; }

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
static inline uint32_t rng_seed(uint32_t s) { return s ? s : 0x9e3779b9u; }

static inline float xorshift(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n ? n : 0x9e3779b9u;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

/* ---- parameter curves (HardwareCurves.swift) ----------------------------- */

static const float curve_chorus[9] EL_DSP_DATA = {0.01f, 0.0217f, 0.0524f, 0.1243f, 0.3117f, 0.777f, 1.8335f, 4.83f, 10.2564f};
/* LFO sync divisions in quarter notes: 1/32 ... 4/1 */
static const float divisions[14] EL_DSP_DATA = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3,
                                                1, 1.5f, 2, 4, 8, 16};

static inline float unit(float raw) { return dsp_clamp(raw, 0.0f, 127.0f) * (1.0f / 127.0f); }

static inline float curve_exp(const float *t, float raw)
{
    float v = dsp_clamp(raw, 0.0f, 127.0f);
    int i = (int)(v * (1.0f / 16.0f));
    if (i > 7) i = 7;
    float x0 = (float)(i * 16), x1 = i == 7 ? 127.0f : x0 + 16.0f, f = (v - x0) / (x1 - x0);
    return t[i] * powf_fast(t[i + 1] / t[i], f);
}

/* ADSR time: attack 1 ms ... 18 s, decay / release 2 ms ... 20 s, the
 * upper travel stretched (x * (0.75 + 0.25 x)). */
static inline float env_seconds(float raw, int attack)
{
    float x = unit(raw), c = x * (0.75f + 0.25f * x);
    return attack ? 0.001f * dsp_exp2(c * 14.1357093f) : 0.002f * dsp_exp2(c * 13.2877124f);
}

static inline float lfo_hz(float raw) { return 0.03f * dsp_exp2(unit(raw) * 11.7027499f); } /* 0.03 ... 100 Hz */
static inline float lfo_delay_seconds(float raw) { float x = unit(raw); return raw < 0.5f ? 0.0f : 5.0f * x * x; }
static inline float hpf_hz(float raw) { return 20.0f * dsp_exp2(raw * (7.6438562f / 127.0f)); } /* 20 Hz ... 4 kHz */

/* ---- envelope (Envelope.swift) --------------------------------------------- */

typedef struct { float attack, decay, sustain, release; } env_settings_t;

static void env_begin(jp6_env_t *e, int stage, float level, float seconds)
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

static inline void env_gate_off(jp6_env_t *e, const env_settings_t *s)
{
    if (e->stage != ENV_IDLE) env_begin(e, ENV_RELEASE, 0, s->release);
}

/* Linear attack; decay and release (1 - e^(-4.5 p)) / (1 - e^(-4.5)) with
 * an exact finite duration; the sustain level follows live changes. */
static inline float env_next(jp6_env_t *e, const env_settings_t *s)
{
    switch (e->stage) {
    case ENV_IDLE: return 0;
    case ENV_SUSTAIN:
        e->value += (s->sustain - e->value) * 0.002f;
        return e->value;
    default:
        e->progress += e->increment;
        if (e->progress >= 1.0f) {
            e->value = e->target;
            if (e->stage == ENV_ATTACK) env_begin(e, ENV_DECAY, s->sustain, s->decay);
            else if (e->stage == ENV_DECAY) e->stage = ENV_SUSTAIN;
            else { e->stage = ENV_IDLE; e->value = 0; }
        } else {
            float shape;
            if (e->stage == ENV_ATTACK) shape = e->progress;
            else { e->curve *= e->curve_decay; shape = (1.0f - e->curve) * 1.011235f; }
            e->value = e->start + (e->target - e->start) * shape;
        }
        return e->value;
    }
}

/* Base times of ENV-1 (which = 0) or ENV-2 from the panel values. */
static void env_base(const jp6_part_t *p, int which, env_settings_t *s)
{
    const uint8_t *v = p->v + (which ? JP_ENV2_A : JP_ENV1_A);
    s->attack = env_seconds(v[0], 1);
    s->decay = env_seconds(v[1], 0);
    s->sustain = v[2] / 127.0f;
    s->release = env_seconds(v[3], 0);
}

/* KEY follow shortens the times for higher notes, voice drift varies them. */
static void env_settings(const jp6_part_t *p, const jp6_voice_t *v, int which, float pitch,
                         const env_settings_t *base, env_settings_t *s)
{
    float follow = p->v[which ? JP_ENV2_KEY : JP_ENV1_KEY] / 127.0f;
    float scale = 1.0f + p->v[JP_DRIFT] / 127.0f * v->drift_env * 0.08f;
    if (follow > 0) scale *= dsp_exp2(-follow * (pitch - 60.0f) * (1.0f / 24.0f));
    s->attack = base->attack * scale;
    s->decay = base->decay * scale;
    s->sustain = base->sustain;
    s->release = base->release * scale;
}

/* ---- VCOs (VCO.swift) ------------------------------------------------------- */

/* Band-limited sum of the selected waveforms at phase t, bipolar. The
 * triangle stays naive (12 dB/oct harmonics, aliasing far below the rest). */
static inline float vco_shape(unsigned w, float t, float dt, float pw)
{
    float out = 0;
    if (w & W_SAW) out += 2.0f * t - 1.0f - polyblep(t, dt);
    if (w & W_SQUARE) out += (t < 0.5f ? 1.0f : -1.0f) + polyblep(t, dt) - polyblep(wrap1(t + 0.5f), dt);
    if (w & W_PULSE) out += (t < pw ? 1.0f : -1.0f) + polyblep(t, dt) - polyblep(wrap1(t + 1.0f - pw), dt);
    if (w & W_TRI) out += 1.0f - 4.0f * dsp_abs(t - 0.5f);
    return out;
}

/* Naive sum, for the height of a sync reset. */
static inline float vco_naive(unsigned w, float t, float pw)
{
    float out = 0;
    if (w & W_SAW) out += 2.0f * t - 1.0f;
    if (w & W_SQUARE) out += t < 0.5f ? 1.0f : -1.0f;
    if (w & W_PULSE) out += t < pw ? 1.0f : -1.0f;
    if (w & W_TRI) out += 1.0f - 4.0f * dsp_abs(t - 0.5f);
    return out;
}

/* Free-running step: samples since the wrap within this sample (0..1), or -1. */
static inline float vco_advance(float *phase, float dt)
{
    *phase += dt;
    if (*phase >= 1.0f) { *phase -= 1.0f; return *phase / dt; }
    return -1.0f;
}

/* Step of a VCO hard-synced to a master that wrapped `since` samples ago. */
static inline float vco_slave(float *phase, float dt, float since, unsigned w, float pw)
{
    if (since < 0) {
        vco_advance(phase, dt);
        return vco_shape(w, *phase, dt, pw);
    }
    float before = wrap01(*phase + dt * (1.0f - since));
    *phase = dt * since;
    float height = vco_naive(w, 0, pw) - vco_naive(w, before, pw);
    return vco_naive(w, *phase, pw) + 0.5f * height * polyblep(*phase, dt);
}

/* Cross mod source: VCO-2's waveform, or its triangle when VCO-2 is muted. */
static inline float vco_mod_source(unsigned w, float out, float phase)
{
    return (w & W_WAVES) ? out : 1.0f - 4.0f * dsp_abs(phase - 0.5f);
}

/* Shared per render call: waveforms, mixer, sync and the modulation scales. */
typedef struct {
    unsigned wave1, wave2;
    int sync, pwm_source, mod1, mod2, porta, vcf_env2, mode, stereo;
    float level1, level2, offset1, offset2;
    float lfo_semis, env1_semis, crossmod, crossmod_env, env1_sign;
    float porta_coef, lfo_fade_inc, drift, unison_semis, pw_depth;
    float key_follow, vcf_env_oct, vcf_lfo_oct, vcf_velocity, damping, input_gain;
    float amp, tremolo, vca_velocity;
    env_settings_t env[2];
} derived_t;

static inline float vco_tick(jp6_voice_t *v, const derived_t *d, float raw_dt1, float dt2, float pw, float xmod)
{
    float o1, o2;
    if (d->sync == 2) {
        /* VCO-2 is the master: it runs first and restarts VCO-1. */
        float since = vco_advance(&v->phase2, dt2);
        o2 = vco_shape(d->wave2, v->phase2, dt2, pw);
        float xm = xmod > 0 ? dsp_exp2(xmod * dsp_clamp(vco_mod_source(d->wave2, o2, v->phase2), -1.5f, 1.5f)) : 1.0f;
        float dt1 = dsp_clamp(raw_dt1 * xm, 0.0f, 0.45f);
        o1 = vco_slave(&v->phase1, dt1, since, d->wave1, pw);
        v->last2 = o2;
    } else {
        /* Cross mod from VCO-2's previous sample. */
        float xm = xmod > 0 ? dsp_exp2(xmod * dsp_clamp(v->last2, -1.5f, 1.5f)) : 1.0f;
        float dt1 = dsp_clamp(raw_dt1 * xm, 0.0f, 0.45f);
        float since = vco_advance(&v->phase1, dt1);
        o1 = vco_shape(d->wave1, v->phase1, dt1, pw);
        if (d->sync == 1) {
            o2 = vco_slave(&v->phase2, dt2, since, d->wave2, pw);
        } else {
            vco_advance(&v->phase2, dt2);
            o2 = vco_shape(d->wave2, v->phase2, dt2, pw);
        }
        v->last2 = vco_mod_source(d->wave2, o2, v->phase2);
    }
    if (d->wave2 & W_NOISE) o2 += xorshift(&v->noise) * 0.8f;
    return o1 * d->level1 + o2 * d->level2;
}

/* ---- VCF (VCF.swift): IR3109 as two SVF sections in series --------------- */

typedef struct { float g1, g2, h1, h2, r, input_gain, makeup; int hp1, hp2; } vcf_t;

/* LPF: LP -> LP, BPF: HP -> LP (+6 dB makeup), HPF: HP -> HP. Exponential
 * damping 0.85 -> 0.18, input attenuation with resonance, 3 % mismatch. */
static inline void vcf_set(vcf_t *f, float cutoff, const derived_t *d)
{
    const float limit = EL_SAMPLE_RATE * 0.45f;
    f->g1 = dsp_tan(DSP_PI * dsp_clamp(cutoff, 5.0f, limit) * (1.0f / EL_SAMPLE_RATE));
    f->g2 = dsp_tan(DSP_PI * dsp_clamp(cutoff * 1.03f, 5.0f, limit) * (1.0f / EL_SAMPLE_RATE));
    f->r = d->damping;
    f->h1 = 1.0f / (1.0f + 2.0f * f->r * f->g1 + f->g1 * f->g1);
    f->h2 = 1.0f / (1.0f + 2.0f * f->r * f->g2 + f->g2 * f->g2);
    f->input_gain = d->input_gain * 0.5f;
    f->hp1 = d->mode != 0;
    f->hp2 = d->mode == 2;
    f->makeup = d->mode == 1 ? 2.0f : 1.0f;
}

static inline float svf_section(float x, float *a, float *b, float g, float h, float r, int high)
{
    float hp = (x - (2.0f * r + g) * *a - *b) * h;
    float v1 = g * hp, bp = v1 + *a;
    *a = 2.0f * soft_clip((bp + v1) * 0.5f);
    float v2 = g * bp, lp = v2 + *b;
    *b = lp + v2;
    return high ? hp : lp;
}

static inline float vcf(jp6_voice_t *v, const vcf_t *f, float input)
{
    float x = 2.0f * soft_clip(input * f->input_gain);
    float y = svf_section(x, &v->a1, &v->b1, f->g1, f->h1, f->r, f->hp1);
    return f->makeup * svf_section(y, &v->a2, &v->b2, f->g2, f->h2, f->r, f->hp2);
}

/* ---- HPF: 12 dB biquad, Q 0.6, shared coefficients ------------------------ */

/* b0 b1 b2 a1 a2 of the high-pass at hz. */
static void hpf_coefs(float hz, float *c)
{
    float w = 2.0f * DSP_PI * hz / EL_SAMPLE_RATE;
    float cw = dsp_cos(w), alpha = dsp_sin(w) / (2.0f * 0.6f), a0 = 1.0f + alpha;
    c[0] = (1.0f + cw) * 0.5f / a0;
    c[1] = -(1.0f + cw) / a0;
    c[2] = c[0];
    c[3] = -2.0f * cw / a0;
    c[4] = (1.0f - alpha) / a0;
}

static void hpf_configure(jp6_part_t *p, float raw)
{
    float c[5];
    hpf_coefs(hpf_hz(raw), c);
    p->hpf_b0 = c[0]; p->hpf_b1 = c[1]; p->hpf_b2 = c[2]; p->hpf_a1 = c[3]; p->hpf_a2 = c[4];
    p->hpf_coef_at = raw;
}

/* ---- chorus (Chorus.swift: MN3009 BBD, free rate or Juno-60 modes) ------- */

static EL_DSP_CODE void chorus(jp6_chorus_t *c, float *left, float *right, int frames, int enabled, int mode, float rate)
{
    const float lp = 0.6801192f; /* 1 - exp(-2 pi 8000 / 44100) */
    float hz, depth_seconds;
    if (!enabled && c->mix < 0.0001f) return;
    switch (mode) {
    case 1: hz = 0.513f; depth_seconds = 0.00185f; break;
    case 2: hz = 0.863f; depth_seconds = 0.00185f; break;
    case 3: hz = 9.75f; depth_seconds = 0.0002f; break;
    default: hz = rate; depth_seconds = 0.0019f - dsp_min(rate, 8.0f) * 0.0001f; break;
    }
    float target = enabled ? 1.0f : 0.0f, inc = hz / EL_SAMPLE_RATE;
    float center = 0.0035f * EL_SAMPLE_RATE, depth_target = depth_seconds * EL_SAMPLE_RATE;
    for (int n = 0; n < frames; ++n) {
        float xl = left[n], xr = right[n];
        c->lp1 += (0.5f * (xl + xr) - c->lp1) * lp;
        c->lp2 += (c->lp1 - c->lp2) * lp;
        c->buffer[c->write] = soft_clip(c->lp2 * 0.8f) * 1.25f;
        c->mix += (target - c->mix) * 0.001f;
        c->depth += (depth_target - c->depth) * 0.0005f;
        if (c->mix > 0.0001f) {
            c->phase += inc;
            if (c->phase >= 1.0f) c->phase -= 1.0f;
            float tri = 4.0f * dsp_abs(c->phase - 0.5f) - 1.0f;
            float wet[2];
            for (int side = 0; side < 2; ++side) {
                float pos = (float)c->write - (center + (side ? -c->depth : c->depth) * tri);
                int i = (int)pos;
                if ((float)i > pos) --i;
                float frac = pos - (float)i, a = c->buffer[i & (JP6_CHORUS_SIZE - 1)];
                wet[side] = a + (c->buffer[(i + 1) & (JP6_CHORUS_SIZE - 1)] - a) * frac;
            }
            float noise = xorshift(&c->hiss) * 0.00006f;
            c->wl1 += (wet[0] + noise - c->wl1) * lp;
            c->wr1 += (wet[1] - noise - c->wr1) * lp;
            c->wl2 += (c->wl1 - c->wl2) * lp;
            c->wr2 += (c->wr1 - c->wr2) * lp;
            float dry = 1.0f - 0.3f * c->mix;
            left[n] = xl * dry + c->wl2 * 0.75f * c->mix;
            right[n] = xr * dry + c->wr2 * 0.75f * c->mix;
        }
        c->write = (c->write + 1u) & (JP6_CHORUS_SIZE - 1);
    }
}

/* ---- part ------------------------------------------------------------------ */

void jp6_part_init(jp6_part_t *p, unsigned voices, uint32_t seed)
{
    memset(p, 0, sizeof *p);
    p->voices = (uint8_t)(voices < 1 ? 1 : voices > JP6_MAX_VOICES ? JP6_MAX_VOICES : voices);
    for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) {
        jp6_voice_t *v = &p->voice[i];
        uint32_t s = 0x12345678u + i * 7919u + seed * 2654435761u;
        v->key = -1;
        v->noise = rng_seed(s);
        /* Free-running analog VCOs: every voice starts at its own phase. */
        uint32_t r = rng_seed(s * 747796405u + 1u);
        v->phase1 = xorshift(&r) * 0.5f + 0.5f;
        v->phase2 = xorshift(&r) * 0.5f + 0.5f;
        /* Voice calibration: fixed deviations like component tolerances. */
        v->rng = rng_seed(s * 2654435761u);
        v->drift_pitch = xorshift(&v->rng);
        v->drift_cutoff = xorshift(&v->rng);
        v->drift_env = xorshift(&v->rng);
        v->lfo_fade = 1;
        v->pitch = v->target_pitch = 60;
    }
    p->lfo_rng = rng_seed(0x51f15eedu ^ seed);
    p->chorus.hiss = rng_seed(0xb0b0cafeu ^ seed);
    p->chorus.depth = 80;
    p->dist.gain = 1;
    p->dist.mono = 1;
    p->level = 1;
    p->last_pitch = 60;
    p->volume_smoothed = 0.6f;
    p->silent_blocks = SILENT_BLOCKS;
    jp6_part_load(p, jp6_presets[0].v);
}

EL_DSP_CODE int jp6_active(const jp6_part_t *p)
{
    for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].active) return 1;
    return p->silent_blocks < SILENT_BLOCKS;
}

static void release(jp6_part_t *p, jp6_voice_t *v, const env_settings_t base[2])
{
    env_settings_t s;
    v->gate = 0;
    env_settings(p, v, 0, v->pitch, &base[0], &s);
    env_gate_off(&v->env1, &s);
    env_settings(p, v, 1, v->pitch, &base[1], &s);
    env_gate_off(&v->env2, &s);
}

static void env_bases(const jp6_part_t *p, env_settings_t base[2])
{
    env_base(p, 0, &base[0]);
    env_base(p, 1, &base[1]);
}

EL_DSP_CODE void jp6_all_off(jp6_part_t *p)
{
    env_settings_t base[2];
    env_bases(p, base);
    for (unsigned i = 0; i < JP6_MAX_VOICES; ++i)
        if (p->voice[i].gate) release(p, &p->voice[i], base);
    p->held_count = 0;
}

EL_DSP_CODE void jp6_part_load(jp6_part_t *p, const uint8_t v[JP_COUNT])
{
    int mode_changed = p->v[JP_KEY_MODE] != v[JP_KEY_MODE];
    if (mode_changed) jp6_all_off(p);
    for (unsigned i = 0; i < JP_COUNT; ++i) p->v[i] = (uint8_t)el_clampi(v[i], 0, jp6_param_max(i));
    /* A new sound starts at its own filter settings, not with a sweep. */
    p->cutoff_smoothed = p->v[JP_CUTOFF];
    p->hpf_smoothed = p->v[JP_HPF];
    p->hpf_coef_at = -1;
}

EL_DSP_CODE void jp6_part_set(jp6_part_t *p, unsigned param, int value)
{
    if (param >= JP_COUNT) return;
    value = el_clampi(value, 0, jp6_param_max(param));
    if (param == JP_KEY_MODE && p->v[param] != value) jp6_all_off(p);
    p->v[param] = (uint8_t)value;
}

static inline float key_shift(const jp6_part_t *p) { return (float)((int)p->v[JP_KEY_SHIFT] - 12); }

static void start_voice(jp6_part_t *p, jp6_voice_t *v, int key, float note, float velocity, int glide,
                        const env_settings_t base[2])
{
    env_settings_t s;
    int was_active = v->active;
    v->key = (int16_t)key;
    v->velocity = velocity;
    v->gate = 1;
    v->age = ++p->age;
    v->target_pitch = note;
    v->pitch = glide ? p->last_pitch : note;
    v->lfo_fade = p->v[JP_LFO_DELAY] > 0 ? 0.0f : 1.0f;
    if (!was_active) {
        v->a1 = v->b1 = v->a2 = v->b2 = 0;
        v->hz1 = v->hz2 = 0;
        v->env1.stage = v->env2.stage = ENV_IDLE;
        v->env1.value = v->env2.value = 0;
    }
    v->active = 1;
    env_settings(p, v, 0, note, &base[0], &s);
    env_begin(&v->env1, ENV_ATTACK, 1, s.attack);
    env_settings(p, v, 1, note, &base[1], &s);
    env_begin(&v->env2, ENV_ATTACK, 1, s.attack);
    p->last_pitch = note;
}

/* Free voice first, then the oldest released voice, then the oldest voice. */
static jp6_voice_t *find_voice(jp6_part_t *p)
{
    jp6_voice_t *best = 0;
    for (unsigned i = 0; i < p->voices; ++i) if (!p->voice[i].active) return &p->voice[i];
    for (unsigned i = 0; i < p->voices; ++i)
        if (!p->voice[i].gate && (!best || p->voice[i].age < best->age)) best = &p->voice[i];
    if (best) return best;
    for (unsigned i = 0; i < p->voices; ++i)
        if (!best || p->voice[i].age < best->age) best = &p->voice[i];
    return best;
}

static int any_gate(const jp6_part_t *p)
{
    for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) if (p->voice[i].gate) return 1;
    return 0;
}

/* Mono / unison: one voice group driven by a key stack (last-note
 * priority). MULTI retriggers on every note, LEGATO (or a tied step) only
 * when no key was held. */
static void group_note_on(jp6_part_t *p, int note, float velocity, int tied, int glide, const env_settings_t base[2])
{
    unsigned n = 0;
    for (unsigned i = 0; i < p->held_count; ++i) if (p->held[i] != note) p->held[n++] = p->held[i];
    if (n == 8) { for (unsigned i = 1; i < 8; ++i) p->held[i - 1] = p->held[i]; n = 7; }
    p->held[n++] = (int16_t)note;
    p->held_count = (uint8_t)n;
    int unison = p->v[JP_KEY_MODE] == 3;
    unsigned count = unison ? p->voices : 1u;
    int legato = 0;
    for (unsigned j = 0; j < count; ++j) legato |= p->voice[j].active && p->voice[j].gate;
    int retrigger = p->v[JP_ENV_RETRIG] == 0 && !tied;
    float pitch = (float)note + key_shift(p);
    for (unsigned j = 0; j < p->voices; ++j) {
        jp6_voice_t *v = &p->voice[j];
        if (j >= count) {
            if (v->gate) release(p, v, base);
            continue;
        }
        v->unison = (uint8_t)unison;
        v->unison_spread = unison && count > 1 ? -1.0f + 2.0f * (float)j / (float)(count - 1) : 0.0f;
        if (legato && v->active && v->gate) {
            if (retrigger) {
                /* New attack, but keep gliding from the current pitch. */
                float from = v->pitch, fade = v->lfo_fade;
                start_voice(p, v, note, pitch, velocity, glide, base);
                if (glide) v->pitch = from;
                v->lfo_fade = fade;
            } else {
                v->key = (int16_t)note;
                v->target_pitch = pitch;
                if (!glide) v->pitch = pitch;
            }
        } else {
            float from = p->last_pitch;
            start_voice(p, v, note, pitch, velocity, glide, base);
            if (glide) v->pitch = from;
            p->last_pitch = from;
        }
    }
    p->last_pitch = pitch;
}

EL_DSP_CODE void jp6_note_on(jp6_part_t *p, int note, float velocity, int legato)
{
    env_settings_t base[2];
    int porta = p->v[JP_PORTA], held = any_gate(p) || legato;
    int glide = porta == 1 || (porta == 2 && held);
    env_bases(p, base);
    if (p->v[JP_LFO_KEYSYNC] && !held) {
        p->lfo_phase = 0;
        p->lfo_held = xorshift(&p->lfo_rng);
        p->lfo_from = p->lfo_to;
    }
    velocity = dsp_clamp(velocity, 0.0f, 1.0f);
    if (p->v[JP_KEY_MODE] >= 2) { group_note_on(p, note, velocity, legato, glide, base); return; }
    jp6_voice_t *v = 0;
    for (unsigned i = 0; i < p->voices; ++i)
        if (p->voice[i].active && p->voice[i].key == note) { v = &p->voice[i]; break; }
    if (!v) v = find_voice(p);
    v->unison = 0;
    v->unison_spread = 0;
    start_voice(p, v, note, (float)note + key_shift(p), velocity, glide, base);
}

EL_DSP_CODE void jp6_note_off(jp6_part_t *p, int note)
{
    env_settings_t base[2];
    env_bases(p, base);
    if (p->v[JP_KEY_MODE] < 2) {
        for (unsigned i = 0; i < p->voices; ++i)
            if (p->voice[i].active && p->voice[i].gate && p->voice[i].key == note) release(p, &p->voice[i], base);
        return;
    }
    int was_top = p->held_count && p->held[p->held_count - 1] == note;
    unsigned n = 0;
    for (unsigned i = 0; i < p->held_count; ++i) if (p->held[i] != note) p->held[n++] = p->held[i];
    p->held_count = (uint8_t)n;
    if (!was_top) return;
    if (n) {
        /* Return to the previously held key, legato (AUTO glides too). */
        unsigned count = p->v[JP_KEY_MODE] == 3 ? p->voices : 1u;
        float pitch = (float)p->held[n - 1] + key_shift(p);
        for (unsigned j = 0; j < count; ++j) {
            jp6_voice_t *v = &p->voice[j];
            if (!v->active) continue;
            v->key = p->held[n - 1];
            v->target_pitch = pitch;
            if (!p->v[JP_PORTA]) v->pitch = pitch;
        }
        p->last_pitch = pitch;
        return;
    }
    for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) if (p->voice[i].gate) release(p, &p->voice[i], base);
}

/* ---- rendering ------------------------------------------------------------- */

/* Parameter curves, once per render call (the panel only changes between calls). */
static EL_DSP_CODE void derive(const jp6_part_t *p, derived_t *d)
{
    const uint8_t *v = p->v;
    d->wave1 = (v[JP_VCO1_TRI] ? W_TRI : 0u) | (v[JP_VCO1_SAW] ? W_SAW : 0u) | (v[JP_VCO1_PULSE] ? W_PULSE : 0u) |
               (v[JP_VCO1_SQUARE] ? W_SQUARE : 0u);
    d->wave2 = (v[JP_VCO2_TRI] ? W_TRI : 0u) | (v[JP_VCO2_SAW] ? W_SAW : 0u) | (v[JP_VCO2_PULSE] ? W_PULSE : 0u) |
               (v[JP_VCO2_NOISE] ? W_NOISE : 0u);
    /* SOURCE MIX: VCO-1 alone at 0, both at full level in the middle. */
    float mix = v[JP_MIX] / 127.0f;
    d->level1 = dsp_min(1, 2 * (1 - mix));
    d->level2 = dsp_min(1, 2 * mix);
    d->sync = v[JP_SYNC];
    float x = unit(v[JP_VCO_LFO]);
    d->lfo_semis = 12.0f * x * x;
    x = unit(v[JP_VCO_ENV]);
    d->env1_semis = 48.0f * x * x;
    d->crossmod = v[JP_XMOD] / 127.0f;
    d->crossmod_env = v[JP_XMOD_ENV] / 127.0f;
    d->mod1 = v[JP_VCO_MOD_DEST] != 1;
    d->mod2 = v[JP_VCO_MOD_DEST] != 0;
    /* VCO-1 RANGE 32' ... 2'; VCO-2 adds LO (5 octaves below 8') in front. */
    d->offset1 = (float)(((int)v[JP_VCO1_RANGE] - 2) * 12);
    int range2 = v[JP_VCO2_RANGE], octave2 = range2 == 0 ? -60 : (range2 - 3) * 12;
    d->offset2 = (float)(octave2 + (int)v[JP_VCO2_TUNE] - 12) + ((float)v[JP_VCO2_FINE] - 64.0f) / 64.0f * 0.5f;
    d->porta = v[JP_PORTA] != 0;
    x = unit(v[JP_PORTA_TIME]);
    float porta_seconds = x * x * 3.0f;
    d->porta_coef = porta_seconds < 0.001f ? 1.0f : 1.0f - dsp_exp(-(float)CONTROL / (porta_seconds * EL_SAMPLE_RATE));
    float delay = lfo_delay_seconds(v[JP_LFO_DELAY]);
    d->lfo_fade_inc = delay < 0.001f ? 1.0f : (float)CONTROL / (delay * EL_SAMPLE_RATE);
    d->drift = v[JP_DRIFT] / 127.0f;
    x = unit(v[JP_UNISON_DETUNE]);
    d->unison_semis = x * dsp_sqrt(x) * 0.5f;
    d->pwm_source = v[JP_PWM_SOURCE];
    d->pw_depth = v[JP_PWM_DEPTH] / 127.0f;
    d->env1_sign = v[JP_ENV1_POL] ? -1.0f : 1.0f;
    /* Key follow up to 120 %. */
    d->key_follow = v[JP_VCF_KEY] / 127.0f * 1.2f / 12.0f;
    d->vcf_env_oct = v[JP_VCF_ENV] / 12.0f;
    d->vcf_env2 = v[JP_VCF_ENV_SRC] == 1;
    d->vcf_lfo_oct = v[JP_VCF_LFO] / 32.0f;
    d->vcf_velocity = v[JP_VCF_VELO] / 127.0f;
    float res = v[JP_RESO] / 127.0f;
    d->damping = 0.85f * dsp_exp2(res * -2.2394659f); /* 0.85 * (0.18 / 0.85)^res */
    d->input_gain = 1.0f / (1.0f + res);
    d->mode = v[JP_VCF_MODE];
    d->amp = v[JP_VCA_LEVEL] / 127.0f;
    d->tremolo = v[JP_VCA_LFO] / 127.0f;
    d->vca_velocity = v[JP_VCA_VELO] / 127.0f;
    env_bases(p, d->env);
    d->stereo = 0;
    for (unsigned i = 0; i < p->voices; ++i) d->stereo |= p->voice[i].active && p->voice[i].unison;
}

/* Per control block: LFO, smoothed controls, HPF. Returns the cutoff in
 * octaves above 20 Hz (before key follow and modulation). */
static EL_DSP_CODE float control(jp6_part_t *p, float bpm)
{
    const uint8_t *v = p->v;
    const float n = CONTROL;
    int wrapped = 0;
    if (v[JP_LFO_SYNC]) {
        float quarters = divisions[v[JP_LFO_DIV] < 14 ? v[JP_LFO_DIV] : 13];
        p->lfo_phase += (bpm < 20 ? 20 : bpm) / 60.0f / quarters * n / EL_SAMPLE_RATE;
    } else {
        p->lfo_phase += lfo_hz(v[JP_LFO_RATE]) * n / EL_SAMPLE_RATE;
    }
    if (p->lfo_phase >= 1.0f) { p->lfo_phase = wrap01(p->lfo_phase); wrapped = 1; }
    float ph = p->lfo_phase;
    switch (v[JP_LFO_WAVE]) {
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
    p->cutoff_smoothed += (v[JP_CUTOFF] - p->cutoff_smoothed) * (n / (0.01f * EL_SAMPLE_RATE));
    float volume = (v[JP_VOLUME] / 127.0f) * (v[JP_VOLUME] / 127.0f);
    p->volume_smoothed += (volume - p->volume_smoothed) * (n / (0.02f * EL_SAMPLE_RATE));
    /* HPF: 0 = off, else 20 Hz ... 4 kHz, smoothed. */
    float hpf = v[JP_HPF];
    if (hpf < 0.5f && p->hpf_smoothed < 0.5f) {
        p->hpf_on = 0;
        p->hpf_smoothed = 0;
    } else {
        p->hpf_smoothed += (hpf - p->hpf_smoothed) * (n / (0.02f * EL_SAMPLE_RATE));
        p->hpf_on = 1;
        float raw = dsp_max(p->hpf_smoothed, 0.5f);
        if (dsp_abs(raw - p->hpf_coef_at) > 0.01f) hpf_configure(p, raw);
    }
    /* Cutoff 5 Hz ... 30 kHz (about 10 steps per octave). */
    return p->cutoff_smoothed * 0.0988248f - 2.0f;
}

static EL_DSP_CODE void render_voice(jp6_part_t *p, jp6_voice_t *v, const derived_t *d, float cutoff_oct,
                                     float *left, float *right, int n)
{
    if (d->porta) v->pitch += (v->target_pitch - v->pitch) * d->porta_coef;
    else v->pitch = v->target_pitch;
    float pitch = v->pitch, drift_semis = 0, drift_oct = 0;
    if (d->drift > 0) {
        v->wander += xorshift(&v->rng) * 0.02f * (float)n / 16.0f - v->wander * 0.0005f * (float)n;
        v->wander = dsp_clamp(v->wander, -1, 1);
        drift_semis = d->drift * (v->drift_pitch * 0.08f + v->wander * 0.04f);
        drift_oct = d->drift * v->drift_cutoff * 0.15f;
    }
    if (v->lfo_fade < 1.0f) v->lfo_fade = dsp_min(1, v->lfo_fade + d->lfo_fade_inc);
    float lfo = p->lfo_value * v->lfo_fade;
    float velocity = v->velocity;
    /* ENV-1 (with its polarity) modulates pitch, PWM and the VCF; ENV-2
     * drives the VCA and can drive the VCF. */
    float env1 = v->env1.value * d->env1_sign, env2 = v->env2.value;
    float mod = lfo * d->lfo_semis + env1 * d->env1_semis;
    float base = pitch + drift_semis + v->unison_spread * d->unison_semis;
    float dt1 = note_hz(base + d->offset1 + (d->mod1 ? mod : 0)) * (1.0f / EL_SAMPLE_RATE);
    float dt2 = dsp_clamp(note_hz(base + d->offset2 + (d->mod2 ? mod : 0)) * (1.0f / EL_SAMPLE_RATE), 0.0f, 0.45f);
    /* Pulse width 50 % (depth 0) down to 5 %; LFO / ENV-1 sweep it within the depth. */
    float pw_amount = d->pw_depth;
    if (d->pwm_source == 0) pw_amount = d->pw_depth * (0.5f + 0.5f * lfo);
    else if (d->pwm_source == 2) pw_amount = d->pw_depth * dsp_abs(env1);
    float pw = 0.5f - 0.45f * dsp_clamp(pw_amount, 0, 1);
    float vcf_env = (d->vcf_env2 ? env2 : env1) * (1.0f - d->vcf_velocity + d->vcf_velocity * velocity);
    float octaves = cutoff_oct + d->key_follow * (pitch - 60.0f) + vcf_env * d->vcf_env_oct + lfo * d->vcf_lfo_oct + drift_oct;
    vcf_t f;
    vcf_set(&f, 20.0f * dsp_exp2(dsp_clamp(octaves, -2, 11)), d);
    /* Cross mod: manual setting plus ENV-1 (a VCA fed by the knob and ENV-1). */
    float xmod = dsp_clamp(d->crossmod + d->crossmod_env * v->env1.value, 0, 1);
    xmod = xmod < 0.004f ? 0.0f : 4.0f * xmod * dsp_sqrt(xmod);
    float dynamics = 1.0f - d->vca_velocity + d->vca_velocity * velocity;
    float tremolo = 1.0f - d->tremolo * (0.5f - 0.5f * lfo);
    float amp = d->amp * dynamics * tremolo * (v->unison ? 0.45f : 1.0f);
    float pan = v->unison ? v->unison_spread * 0.85f : 0.0f;
    float gl = amp * dsp_min(1, 1 - pan), gr = amp * dsp_min(1, 1 + pan);
    env_settings_t s1, s2;
    env_settings(p, v, 0, pitch, &d->env[0], &s1);
    env_settings(p, v, 1, pitch, &d->env[1], &s2);
    float b0 = p->hpf_b0, b1 = p->hpf_b1, b2 = p->hpf_b2, a1 = p->hpf_a1, a2 = p->hpf_a2;
    int hpf_on = p->hpf_on;
    /* The HPF corner never above the voice's lower VCO, so low notes keep
     * their bass; notes above the corner use the shared coefficients. */
    float fc = 0.85f * dsp_min(dt1, dt2) * EL_SAMPLE_RATE;   /* 12 dB, Q 0.6: about -3 dB at f0 */
    if (hpf_on && fc < hpf_hz(dsp_max(p->hpf_smoothed, 0.5f))) {
        if (dsp_abs(fc - v->hpf_fc) > 0.005f * fc) { hpf_coefs(fc, v->hpf_c); v->hpf_fc = fc; }
        b0 = v->hpf_c[0]; b1 = v->hpf_c[1]; b2 = v->hpf_c[2]; a1 = v->hpf_c[3]; a2 = v->hpf_c[4];
    }
    for (int k = 0; k < n; ++k) {
        env_next(&v->env1, &s1);
        float e = env_next(&v->env2, &s2);
        float x = vco_tick(v, d, dt1, dt2, pw, xmod) * 0.3f;
        if (hpf_on) {
            float y = b0 * x + v->hz1;
            v->hz1 = b1 * x - a1 * y + v->hz2;
            v->hz2 = b2 * x - a2 * y;
            x = y;
        }
        x = vcf(v, &f, x) * e;
        left[k] += x * gl;
        right[k] += x * gr;
    }
    if (v->env2.stage == ENV_IDLE) { v->active = 0; v->key = -1; }
}

EL_DSP_CODE void jp6_render(jp6_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm)
{
    static float l[EL_BLOCK], r[EL_BLOCK];
    static derived_t d;
    if (frames <= 0) return;
    if (frames > EL_BLOCK) frames = EL_BLOCK;
    int any = 0;
    for (unsigned i = 0; i < p->voices; ++i) any |= p->voice[i].active;
    /* Idle once no voice sounds and the chorus / distortion tails are out. */
    if (!any && p->silent_blocks >= SILENT_BLOCKS) return;
    derive(p, &d);
    for (int i = 0; i < frames; ++i) l[i] = r[i] = 0;
    for (int pos = 0; pos < frames; pos += CONTROL) {
        int n = frames - pos < CONTROL ? frames - pos : CONTROL;
        float cutoff_oct = control(p, bpm);
        for (unsigned i = 0; i < p->voices; ++i)
            if (p->voice[i].active) render_voice(p, &p->voice[i], &d, cutoff_oct, l + pos, r + pos, n);
    }
    /* DC blocker and volume, then the per-sound distortion and chorus. */
    float g = p->volume_smoothed * 0.5f;
    for (int i = 0; i < frames; ++i) {
        float yl = l[i] - p->dc_xl + 0.9995f * p->dc_yl, yr = r[i] - p->dc_xr + 0.9995f * p->dc_yr;
        p->dc_xl = l[i]; p->dc_yl = yl;
        p->dc_xr = r[i]; p->dc_yr = yr;
        l[i] = yl * g;
        r[i] = yr * g;
    }
    dist_process(&p->dist, l, r, frames, p->v[JP_DIST_MODE], p->v[JP_DIST_DRIVE], p->v[JP_DIST_TONE],
                 p->v[JP_DIST_LEVEL], p->v[JP_DIST_MIX], d.stereo);
    chorus(&p->chorus, l, r, frames, p->v[JP_CHORUS], p->v[JP_CHORUS_MODE], curve_exp(curve_chorus, p->v[JP_CHORUS_RATE]));
    float level = p->level;
    float ds = powf_fast(p->v[JP_DELAY_SEND] / 127.0f, 1.5f) * level, rs = powf_fast(p->v[JP_REVERB_SEND] / 127.0f, 1.5f) * level;
    for (int i = 0; i < frames; ++i) {
        left[i] += l[i] * level;
        right[i] += r[i] * level;
        float m = 0.5f * (l[i] + r[i]);
        dly[i] += m * ds;
        rev[i] += m * rs;
    }
    if (any) p->silent_blocks = 0;
    else if (++p->silent_blocks >= SILENT_BLOCKS) {
        /* Settle the DC blocker so the next note starts clean. */
        p->dc_xl = p->dc_yl = p->dc_xr = p->dc_yr = 0;
    }
}

/* ---- display helpers ------------------------------------------------------- */

static const uint8_t max_values[JP_COUNT] = {
    1, 1, 1, 1, 4, 127, 2,                  /* VCO-1, X-MOD, SYNC */
    1, 1, 1, 1, 5, 24, 127,                 /* VCO-2 */
    127, 2, 127, 127, 2, 127, 127,          /* PWM, VCO MOD, MIX, HPF */
    2, 127, 127, 127, 1, 127, 127, 127,     /* VCF */
    127, 127, 127, 127, 127, 6,             /* VCA, LFO */
    127, 127, 127, 127, 127, 1, 127, 127, 127, 127, 127, 127, /* ENV-1, ENV-2, X-MOD ENV */
    3, 127, 2, 127, 24, 127, 127,           /* key mode ... unison detune */
    1, 13, 1, 1, 1, 127, 3,                 /* LFO sync, retrigger, chorus */
    4, 127, 127, 127, 127,                  /* distortion */
    127, 127, JP6_CHORDS - 1};

int jp6_param_max(unsigned param) { return param < JP_COUNT ? max_values[param] : 0; }

static const char *const names[JP_COUNT] = {
    "VCO1 TRI", "VCO1 SAW", "VCO1 PLS", "VCO1 SQR", "VCO1 RNG", "X-MOD", "SYNC",
    "VCO2 TRI", "VCO2 SAW", "VCO2 PLS", "NOISE", "VCO2 RNG", "TUNE", "FINE",
    "PW/PWM", "PWM SRC", "VCO LFO", "PITCH ENV", "MOD DEST", "MIX", "HPF",
    "VCF MODE", "CUTOFF", "RESO", "VCF ENV", "ENV SRC", "VCF LFO", "KEY FLW", "VCF VELO",
    "LEVEL", "VCA LFO", "VCA VELO", "LFO RATE", "LFO DLY", "LFO WAVE",
    "ENV1 A", "ENV1 D", "ENV1 S", "ENV1 R", "ENV1 KEY", "ENV1 POL",
    "ENV2 A", "ENV2 D", "ENV2 S", "ENV2 R", "ENV2 KEY", "X-MOD ENV",
    "KEY MODE", "GLIDE", "PORTA", "VOLUME", "SHIFT", "DRIFT", "UNI DET",
    "LFO SYNC", "LFO DIV", "LFO KEY", "RETRIG", "CHORUS", "CHO RATE", "CHO MODE",
    "DIST", "DRIVE", "DIST TONE", "DIST LVL", "DIST MIX",
    "DELAY", "REVERB", "CHORD"};

const char *jp6_param_name(unsigned param) { return param < JP_COUNT ? names[param] : ""; }

const char *jp6_param_value_name(unsigned param, int value)
{
    static const char *const off_on[] = {"OFF", "ON"};
    static const char *const range1[] = {"32'", "16'", "8'", "4'", "2'"};
    static const char *const range2[] = {"LO", "32'", "16'", "8'", "4'", "2'"};
    static const char *const sync[] = {"OFF", "1>2", "2>1"};
    static const char *const semis[] = {"-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0",
                                        "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11", "+12"};
    static const char *const pwm_src[] = {"LFO", "MAN", "ENV-1"};
    static const char *const dest[] = {"VCO-1", "VCO-2", "BOTH"};
    static const char *const vcf_modes[] = {"LPF", "BPF", "HPF"};
    static const char *const env_src[] = {"ENV-1", "ENV-2"};
    static const char *const polarity[] = {"NORM", "INV"};
    static const char *const waves[] = {"TRI", "SINE", "SAW UP", "SAW DN", "SQUARE", "S&H", "RANDOM"};
    static const char *const key_modes[] = {"POLY", "CHORD", "MONO", "UNISON"};
    static const char *const porta[] = {"OFF", "ON", "AUTO"};
    static const char *const divs[] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T",
                                       "1/4", "1/4.", "1/2", "1/1", "2/1", "4/1"};
    static const char *const retrig[] = {"MULTI", "LEGATO"};
    static const char *const chorus_modes[] = {"FREE", "I", "II", "I+II"};
    static const char *const dist_modes[] = {"OFF", "OD", "DIST", "RAT", "TUBE"};
    value = el_clampi(value, 0, jp6_param_max(param));
    switch (param) {
    case JP_VCO1_TRI: case JP_VCO1_SAW: case JP_VCO1_PULSE: case JP_VCO1_SQUARE:
    case JP_VCO2_TRI: case JP_VCO2_SAW: case JP_VCO2_PULSE: case JP_VCO2_NOISE:
    case JP_LFO_SYNC: case JP_LFO_KEYSYNC: case JP_CHORUS: return off_on[value];
    case JP_VCO1_RANGE: return range1[value];
    case JP_VCO2_RANGE: return range2[value];
    case JP_SYNC: return sync[value];
    case JP_VCO2_TUNE: case JP_KEY_SHIFT: return semis[value];
    case JP_PWM_SOURCE: return pwm_src[value];
    case JP_VCO_MOD_DEST: return dest[value];
    case JP_HPF: return value ? 0 : off_on[0];
    case JP_VCF_MODE: return vcf_modes[value];
    case JP_VCF_ENV_SRC: return env_src[value];
    case JP_ENV1_POL: return polarity[value];
    case JP_LFO_WAVE: return waves[value];
    case JP_KEY_MODE: return key_modes[value];
    case JP_PORTA: return porta[value];
    case JP_LFO_DIV: return divs[value];
    case JP_ENV_RETRIG: return retrig[value];
    case JP_CHORUS_MODE: return chorus_modes[value];
    case JP_DIST_MODE: return dist_modes[value];
    case JP_CHORD: return jp6_chord_names[value];
    default: return 0;
    }
}
