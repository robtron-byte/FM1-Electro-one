/* SPDX-License-Identifier: GPL-3.0-only
 * MKS-50 / Alpha Juno voice engine. From FM1-Electro's engine/mks.c
 * (github.com/robtron-byte/FM1-Electro-one, commit e7b3b14, GPL-3.0-only with
 * FM1 Open's linking exception; there a port of the MKS-50 iOS app's DSP:
 * DCO.swift, LadderFilter.swift, Envelope.swift, HardwareCurves.swift,
 * Chorus.swift, SynthEngine.swift). The audio path uses no libc math:
 * approximations from dsp_math.h.
 *
 * Changed here (docs/adr/0002, 0003): one pool of voices for all parts, a
 * block in phases two cores share, the ladder's rate and the faster release
 * end as run-time switches (both off). The oscillators, filters, envelope
 * and chorus are FM1-Electro's, line for line. */
/* Device build: all code and constants of this file run in internal SRAM
 * (safe while Flash is written, and faster). */
#if defined(EL_PLATFORM)
#pragma const_seg(".dsp_rodata")
#endif
#include "mks.h"
#include "dsp_math.h"
#include "syn_dsp_inline.h"
#include "syn_chop_inline.h"
#include <string.h>

enum { CONTROL = MKS_CONTROL };

/* ---- hardware curves (june-21 measurements, see HardwareCurves.swift) ---- */

static const float curve_t3[9] EL_DSP_DATA = {0.09f, 0.184f, 0.266f, 0.474f, 1.032f, 3.03f, 11.864f, 23.102f, 37.58f};
static const float curve_t4[9] EL_DSP_DATA = {0.007f, 0.0564f, 0.2032f, 0.6632f, 1.904f, 5.2f, 11.454f, 23.182f, 39.34f};
static const float curve_pwm[9] EL_DSP_DATA = {0.02f, 0.0436f, 0.1048f, 0.2494f, 0.6535f, 1.5306f, 3.6868f, 8.7365f, 20.429f};
static const float curve_vibrato[9] EL_DSP_DATA = {0, 0.0212f, 0.0477f, 0.0736f, 0.104f, 0.1544f, 0.2174f, 0.3205f, 0.4279f};

static inline float env_seconds(float raw, int segment)
{
    if (segment == 3) return syn_curve_exp(curve_t3, raw);
    if (segment == 4) return syn_curve_exp(curve_t4, raw);
    return 0.354f * dsp_exp2((dsp_clamp(raw, 0, 127) - 64.0f) * 0.1f);
}

/* ---- DCO (MB87123 chop model) ------------------------------------------- */

/* DCOSettings.configure: pulse on the pulse phase (1 = main), saw/sub on the
 * master phase (4 = main). Only Pulse 3 and Saw 3 follow PW / PWM. */
static EL_DSP_CODE void dco_configure(mks_part_t *p, int pulse, int saw, int sub, float pw)
{
    switch (pulse) {
    case 1: syn_chop_set(&p->pulse, 0, 1, 0.5f, 0, 0.5f); break;
    /* Pulse 2 / 3: the main square AND a double-rate one of width w is open
     * only for p >= (1 + w) / 2: one gate of that width, the same wave and
     * edges (the double-rate gate's other edges fall where the main one is
     * shut), without syn_chop_general's seven edge tests a sample. */
    case 2: syn_chop_set(&p->pulse, 0, 1, 0.75f, 0, 0.5f); break;
    case 3: syn_chop_set(&p->pulse, 0, 1, 0.5f + 0.5f * pw, 0, 0.5f); break;
    default: p->pulse = syn_chop_off; break;
    }
    switch (saw) {
    case 1: syn_chop_set(&p->saw, 4, 0, 0.5f, 0, 0.5f); break;
    case 2: syn_chop_set(&p->saw, 4, 8, 0.5f, 0, 0.5f); break;
    case 3: syn_chop_set(&p->saw, 4, 8, pw, 0, 0.5f); break;
    case 4: syn_chop_set(&p->saw, 4, 32, 0.5f, 0, 0.5f); break;
    case 5: syn_chop_set(&p->saw, 4, 8, 0.5f, 32, 0.5f); break;
    default: p->saw = syn_chop_off; break;
    }
    switch (sub) {
    case 1: syn_chop_set(&p->sub, 0, 2, 0.5f, 4, 0.5f); break;
    case 2: syn_chop_set(&p->sub, 0, 2, 0.5f, 8, 0.5f); break;
    case 3: syn_chop_set(&p->sub, 0, 2, 0.5f, 16, 0.5f); break;
    case 4: syn_chop_set(&p->sub, 0, 1, 0.5f, 0, 0.5f); break;
    case 5: syn_chop_set(&p->sub, 0, 1, 0.5f, 2, 0.5f); break;
    default: syn_chop_set(&p->sub, 0, 2, 0.5f, 0, 0.5f); break;
    }
}

static inline EL_DSP_HELPER float dco_tick(const mks_slice_t *p, mks_voice_t *v, float raw_dt, float ratio, int sync)
{
    float dt = raw_dt < 0.45f ? raw_dt : 0.45f, dtm = dt * 0.25f;
    v->phase += dtm;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    float main = syn_wrap01(v->phase * 4.0f);
    int wrapped = main < v->last_main;
    v->last_main = main;
    float out = 0;
    if (syn_chop_enabled(&p->pulse)) {
        if (ratio == 1.0f && !sync) {
            v->pulse_phase = main;
            out += syn_chop_render(&p->pulse, main, dt);
        } else {
            float pdt = dt * ratio;
            if (pdt > 0.45f) pdt = 0.45f;
            if (sync && wrapped) {
                float since = main / (dt > 1e-9f ? dt : 1e-9f);
                float before = syn_wrap01(v->pulse_phase + pdt * (1.0f - since));
                v->pulse_phase = syn_wrap01(pdt * since);
                float h = syn_chop_naive(&p->pulse, 0) - syn_chop_naive(&p->pulse, before);
                out += 2.0f * syn_chop_naive(&p->pulse, v->pulse_phase) - 1.0f + h * syn_polyblep(main, dt);
            } else {
                v->pulse_phase += pdt;
                if (v->pulse_phase >= 1.0f) v->pulse_phase = syn_wrap01(v->pulse_phase);
                out += syn_chop_render(&p->pulse, v->pulse_phase, pdt);
            }
        }
    }
    if (syn_chop_enabled(&p->saw)) out += syn_chop_render(&p->saw, v->phase, dtm);
    if (p->sub_level > 0) out += syn_chop_render(&p->sub, v->phase, dtm) * p->sub_level;
    if (p->noise_level > 0) out += syn_xorshift(&v->noise) * p->noise_level;
    return out;
}

/* ---- filters --------------------------------------------------------------- */

static void hpf_configure(mks_part_t *p, int mode)
{
    syn_biquad_t *c = &p->hpf;
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

/* ---- parts and the pool ------------------------------------------------------ */

void mks_init(mks_t *s, unsigned pool, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->pool = (uint8_t)(pool < 1 ? 1 : pool > MKS_POOL_MAX ? MKS_POOL_MAX : pool);
    s->cores = 1;
    for (unsigned i = 0; i < MKS_POOL_MAX; ++i) {
        mks_voice_t *v = &s->voice[i];
        v->key = -1;
        v->noise = seed + i * 0x9e3779b9u;
        v->rng = (seed + i) * 2654435761u;
        if (!v->rng) v->rng = 1;
        v->drift_pitch = syn_xorshift(&v->rng);
        v->drift_cutoff = syn_xorshift(&v->rng);
        v->drift_env = syn_xorshift(&v->rng);
        v->lfo_fade = 1;
    }
    for (unsigned i = 0; i < MKS_PARTS; ++i) {
        mks_part_t *p = &s->part[i];
        uint32_t ps = seed + i * 0x632be5abu;   /* (part 0: the seed itself, as FM1-Electro's one part) */
        p->lfo_rng = ps ^ 0xa5a5a5a5u;
        p->chorus.hiss = 0xb0b0cafeu ^ ps;
        p->chorus.depth = 80;
        p->level = 1;
        p->hpf_mode = 1;
        mks_part_load(s, i, mks_presets[0].v);
    }
}

/* A part's voices are the pool's that carry its number and sound, lowest
 * first (with one part: FM1-Electro's order). */
#define PART_VOICES(s, pi, v) \
    for (mks_voice_t *v = (s)->voice, *v##_end = (s)->voice + (s)->pool; v < v##_end; ++v) if (v->part == (pi))

EL_DSP_CODE unsigned mks_part_voices(const mks_t *s, unsigned pi)
{
    unsigned n = 0;
    for (unsigned i = 0; i < s->pool; ++i) n += s->voice[i].active && s->voice[i].part == pi;
    return n;
}

EL_DSP_CODE int mks_active(const mks_t *s, unsigned pi)
{
    if (pi >= MKS_PARTS) return 0;
    if (mks_part_voices(s, pi)) return 1;
    return s->part[pi].chorus.mix > 0.0001f && s->part[pi].silent_blocks < 40;
}

EL_DSP_CODE void mks_all_off(mks_t *s, unsigned pi)
{
    if (pi >= MKS_PARTS) return;
    mks_part_t *p = &s->part[pi];
    PART_VOICES(s, pi, v)
        if (v->gate) { v->gate = 0; syn_env_begin(&v->env, SYN_ENV_RELEASE, 0, env_seconds(p->v[MP_T4], 4)); }
    p->held_count = 0;
}

static void base_times(mks_part_t *p)
{
    for (unsigned i = 0; i < 4u; ++i) p->base_t[i] = env_seconds(p->v[MP_T1 + (i == 1 ? 2 : i == 2 ? 4 : i == 3 ? 6 : 0)], (int)i + 1);
}

EL_DSP_CODE void mks_part_load(mks_t *s, unsigned pi, const uint8_t v[MP_COUNT])
{
    if (pi >= MKS_PARTS) return;
    mks_part_t *p = &s->part[pi];
    int mode_changed = p->v[MP_KEY_MODE] != v[MP_KEY_MODE];
    for (unsigned i = 0; i < MP_COUNT; ++i) p->v[i] = v[i];
    if (mode_changed) mks_all_off(s, pi);
    hpf_configure(p, p->v[MP_HPF]);
    base_times(p);
    p->cutoff_smoothed = p->v[MP_CUTOFF];
}

EL_DSP_CODE void mks_part_set(mks_t *s, unsigned pi, unsigned param, int value)
{
    if (pi >= MKS_PARTS || param >= MP_COUNT) return;
    mks_part_t *p = &s->part[pi];
    int mode_changed = param == MP_KEY_MODE && p->v[param] != value;
    p->v[param] = (uint8_t)el_clampi(value, 0, mks_param_max(param));
    if (mode_changed) mks_all_off(s, pi);
    if (param == MP_HPF) hpf_configure(p, p->v[MP_HPF]);
    if (param == MP_T1 || param == MP_T2 || param == MP_T3 || param == MP_T4) base_times(p);
}

static inline float env_scale(const mks_part_t *p, const mks_voice_t *v, float pitch)
{
    float follow = p->v[MP_ENV_KEY] / 127.0f;
    float scale = 1.0f + p->v[MP_DRIFT] / 127.0f * v->drift_env * 0.08f;
    if (follow > 0) scale *= dsp_exp2(-follow * (pitch - 60.0f) / 24.0f);
    return scale;
}

static EL_DSP_HELPER void env_settings(const mks_part_t *p, mks_voice_t *v, float pitch, syn_env_settings_t *s)
{
    /* Worked out again only when the knobs, the pitch or the drift changed
     * (else the same numbers as before, without exp2 / log2 every block). */
    static const uint8_t keys[9] EL_DSP_DATA = {MP_T1, MP_L1, MP_T2, MP_L2, MP_T3, MP_L3, MP_T4, MP_ENV_KEY, MP_DRIFT};
    syn_env_settings_t *m = &v->es;
    int same = v->es_ok && v->es_pitch == pitch && v->es_drift == v->drift_env;
    for (unsigned i = 0; i < 9u && same; ++i) same = v->es_key[i] == p->v[keys[i]];
    if (!same) {
        for (unsigned i = 0; i < 9u; ++i) v->es_key[i] = p->v[keys[i]];
        v->es_pitch = pitch;
        v->es_drift = v->drift_env;
        v->es_ok = 1;
        float k = env_scale(p, v, pitch);
        m->t1 = p->base_t[0] * k; m->l1 = p->v[MP_L1] / 127.0f;
        m->t2 = p->base_t[1] * k; m->l2 = p->v[MP_L2] / 127.0f;
        m->t3 = p->base_t[2] * k; m->l3 = p->v[MP_L3] / 127.0f;
        m->t4 = p->base_t[3] * k;
    }
    s->t1 = m->t1; s->l1 = m->l1; s->t2 = m->t2; s->l2 = m->l2;
    s->t3 = m->t3; s->l3 = m->l3; s->t4 = m->t4;
}

static float last_pitch_of(const mks_t *s, unsigned pi)
{
    uint32_t best = 0; float pitch = -1;
    for (unsigned i = 0; i < s->pool; ++i) {
        const mks_voice_t *v = &s->voice[i];
        if (v->part == pi && v->active && v->age >= best) { best = v->age; pitch = v->pitch; }
    }
    return pitch;
}

static EL_DSP_HELPER void start_voice(mks_t *s, unsigned pi, mks_voice_t *v, int note, float velocity, int glide, float from, int retrigger)
{
    mks_part_t *p = &s->part[pi];
    syn_env_settings_t es;
    int was_active = v->active;
    v->part = (uint8_t)pi;
    v->key = (int16_t)note;
    v->velocity = velocity;
    v->gate = 1;
    v->age = ++s->age;
    v->target_pitch = (float)note + (float)((int)p->v[MP_KEY_SHIFT] - 12);
    v->pitch = glide && from >= 0 ? from : v->target_pitch;
    if (!retrigger && was_active) return;
    v->lfo_fade = p->v[MP_LFO_DELAY] > 0 ? 0.0f : 1.0f;
    if (!was_active) {
        syn_ladder_reset(&v->ld);
        v->hz1 = v->hz2 = 0;
        v->hpf_fc = -1;
        v->env.stage = SYN_ENV_IDLE;
        v->env.value = 0;
        v->gate_amp = 0;
    }
    v->active = 1;
    env_settings(p, v, v->target_pitch, &es);
    syn_env_begin(&v->env, SYN_ENV_T1, es.l1, es.t1);
}

/* A voice for part pi from the pool (docs/adr/0002): a free one; else the
 * oldest that was let go, whoever's; else the oldest held one of the part
 * that has the most, where a part keeps MKS_GUARANTEE voices against the
 * others (a part alone: its own oldest, as FM1-Electro's). */
static mks_voice_t *pool_take(mks_t *s, unsigned pi)
{
    mks_voice_t *best = 0;
    unsigned count[MKS_PARTS] = {0}, victim = pi;
    for (unsigned i = 0; i < s->pool; ++i) if (!s->voice[i].active) return &s->voice[i];
    for (unsigned i = 0; i < s->pool; ++i)
        if (!s->voice[i].gate && (!best || s->voice[i].age < best->age)) best = &s->voice[i];
    if (best) return best;
    for (unsigned i = 0; i < s->pool; ++i) count[s->voice[i].part < MKS_PARTS ? s->voice[i].part : 0]++;
    if (count[pi] < MKS_GUARANTEE) {
        /* under its share: from whoever has the most */
        unsigned most = 0;
        for (unsigned q = 0; q < MKS_PARTS; ++q)
            if (q != pi && count[q] > most) { most = count[q]; victim = q; }
    } else {
        /* at or over it: its own, unless another part has more than this one and more than its share */
        for (unsigned q = 0; q < MKS_PARTS; ++q)
            if (count[q] > MKS_GUARANTEE && count[q] > count[victim]) victim = q;
    }
    for (unsigned i = 0; i < s->pool; ++i)
        if (s->voice[i].part == victim && (!best || s->voice[i].age < best->age)) best = &s->voice[i];
    if (!best) best = &s->voice[0];
    return best;
}

static int any_gate(const mks_t *s, unsigned pi)
{
    for (unsigned i = 0; i < s->pool; ++i) if (s->voice[i].part == pi && s->voice[i].gate) return 1;
    return 0;
}

/* The voices a mono (1) or unison note of part pi plays on: those it has
 * (lowest first), then free ones, for unison up to MKS_UNISON_MAX; never
 * fewer than one (taken from the pool as any note). */
static unsigned note_voices(mks_t *s, unsigned pi, unsigned want, mks_voice_t **list)
{
    unsigned n = 0;
    if (want == 1) {
        /* mono: the voice that holds its note; else the one it played last */
        mks_voice_t *best = 0;
        PART_VOICES(s, pi, v) if (v->active && v->gate) { list[0] = v; return 1; }
        PART_VOICES(s, pi, v) if (v->active && (!best || v->age >= best->age)) best = v;
        list[0] = best ? best : pool_take(s, pi);
        return 1;
    }
    PART_VOICES(s, pi, v) if (v->active && n < want) list[n++] = v;
    for (unsigned i = 0; i < s->pool && n < want; ++i) if (!s->voice[i].active) list[n++] = &s->voice[i];
    if (n > 1) {   /* (lowest first, so the spread across them stays as it was) */
        for (unsigned i = 1; i < n; ++i)
            for (unsigned j = i; j > 0 && list[j] < list[j - 1]; --j) { mks_voice_t *t = list[j]; list[j] = list[j - 1]; list[j - 1] = t; }
    }
    if (!n) list[n++] = pool_take(s, pi);
    return n;
}

EL_DSP_CODE void mks_note_on(mks_t *s, unsigned pi, int note, float velocity, int legato)
{
    if (pi >= MKS_PARTS) return;
    mks_part_t *p = &s->part[pi];
    int mode = p->v[MP_KEY_MODE], porta = p->v[MP_PORTA];
    int held = any_gate(s, pi) || legato;
    int glide = porta == 1 || (porta == 2 && held);
    float from = last_pitch_of(s, pi);
    if (p->v[MP_LFO_KEYSYNC] && !held) {
        p->lfo_phase = 0;
        p->lfo_held = syn_xorshift(&p->lfo_rng);
        p->lfo_from = p->lfo_to;
    }
    if (mode == 2 || mode == 3) {
        /* Mono / unison: newest note wins; legato keeps the envelope running
         * when ENV RETRIGGER is LEGATO or the step is tied. */
        mks_voice_t *list[MKS_UNISON_MAX];
        if (p->held_count < 8) p->held[p->held_count++] = (int16_t)note;
        int retrigger = !(held && (p->v[MP_ENV_RETRIG] == 1 || legato));
        unsigned count = note_voices(s, pi, mode == 3 ? MKS_UNISON_MAX : 1u, list);
        for (unsigned i = 0; i < count; ++i) {
            mks_voice_t *v = list[i];
            int mine = v->active && v->part == pi;
            v->unison_spread = count > 1 ? -1.0f + 2.0f * (float)i / (float)(count - 1) : 0.0f;
            start_voice(s, pi, v, note, velocity, glide, from, retrigger || !mine);
        }
        return;
    }
    mks_voice_t *v = 0;
    for (unsigned i = 0; i < s->pool; ++i)
        if (s->voice[i].part == pi && s->voice[i].active && s->voice[i].key == note) { v = &s->voice[i]; break; }
    if (!v) v = pool_take(s, pi);
    v->unison_spread = 0;
    start_voice(s, pi, v, note, velocity, glide, from, 1);
}

static EL_DSP_HELPER void release(mks_part_t *p, mks_voice_t *v)
{
    syn_env_settings_t s;
    v->gate = 0;
    env_settings(p, v, v->pitch, &s);
    syn_env_begin(&v->env, SYN_ENV_RELEASE, 0, s.t4);
}

EL_DSP_CODE void mks_note_off(mks_t *s, unsigned pi, int note)
{
    if (pi >= MKS_PARTS) return;
    mks_part_t *p = &s->part[pi];
    int mode = p->v[MP_KEY_MODE];
    if (mode == 2 || mode == 3) {
        unsigned n = 0;
        for (unsigned i = 0; i < p->held_count; ++i) if (p->held[i] != note) p->held[n++] = p->held[i];
        p->held_count = (uint8_t)n;
        if (n) {
            /* Fall back to the newest still-held note, legato. */
            PART_VOICES(s, pi, v) {
                if (!v->gate) continue;
                v->key = p->held[n - 1];
                v->target_pitch = (float)p->held[n - 1] + (float)((int)p->v[MP_KEY_SHIFT] - 12);
                if (!p->v[MP_PORTA]) v->pitch = v->target_pitch;
            }
            return;
        }
        PART_VOICES(s, pi, v) if (v->gate) release(p, v);
        return;
    }
    PART_VOICES(s, pi, v)
        if (v->active && v->gate && v->key == note) release(p, v);
}

/* Per control step: LFO, PWM, waveforms and shared modulation amounts. */
static EL_DSP_CODE void control(mks_part_t *p, mks_slice_t *b, float bpm)
{
    const uint8_t *v = p->v;
    const float n = CONTROL;
    int wrapped = 0;
    /* LFO */
    if (v[MP_LFO_SYNC]) {
        float quarters = syn_divisions[v[MP_LFO_DIV] < 14 ? v[MP_LFO_DIV] : 13];
        p->lfo_phase += (bpm < 20 ? 20 : bpm) / 60.0f / quarters * n / EL_SAMPLE_RATE;
    } else {
        p->lfo_phase += syn_curve_exp(syn_curve_lfo, v[MP_LFO_RATE]) * n / EL_SAMPLE_RATE;
    }
    if (p->lfo_phase >= 1.0f) { p->lfo_phase = syn_wrap01(p->lfo_phase); wrapped = 1; }
    float ph = p->lfo_phase;
    switch (v[MP_LFO_WAVE]) {
    case 1: p->lfo_value = dsp_sin(2 * DSP_PI * ph); break;
    case 2: p->lfo_value = 2 * ph - 1; break;
    case 3: p->lfo_value = 1 - 2 * ph; break;
    case 4: p->lfo_value = ph < 0.5f ? 1.0f : -1.0f; break;
    case 5:
        if (wrapped) p->lfo_held = syn_xorshift(&p->lfo_rng);
        p->lfo_value = p->lfo_held;
        break;
    case 6:
        if (wrapped) { p->lfo_from = p->lfo_to; p->lfo_to = syn_xorshift(&p->lfo_rng); }
        p->lfo_value = p->lfo_from + (p->lfo_to - p->lfo_from) * (0.5f - 0.5f * dsp_cos(DSP_PI * ph));
        break;
    default: p->lfo_value = 1 - 4 * dsp_abs(ph - 0.5f); break;
    }
    /* PW / PWM: rate 0 = manual width set by the depth control. */
    float depth = v[MP_PW] / 128.0f, pw = depth;
    if (v[MP_PWM_RATE]) {
        p->pwm_phase += syn_curve_exp(curve_pwm, v[MP_PWM_RATE]) * n / EL_SAMPLE_RATE;
        if (p->pwm_phase >= 1.0f) p->pwm_phase = syn_wrap01(p->pwm_phase);
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
    float frac = syn_curve_lin(curve_vibrato, v[MP_DCO_LFO]);
    b->vibrato = 12.0f * dsp_log2(1 + frac / 2);
    b->dco_env = (v[MP_DCO_ENV] / 127.0f) * (v[MP_DCO_ENV] / 127.0f) * 24.0f;
    b->pitch_offset = (float)((1 - (int)v[MP_RANGE]) * 12);
    b->porta = v[MP_PORTA];
    float porta_seconds = (v[MP_PORTA_TIME] / 127.0f) * (v[MP_PORTA_TIME] / 127.0f) * 3.0f;
    b->porta_coef = porta_seconds < 0.001f ? 1.0f : 1.0f - dsp_exp(-n / (porta_seconds * EL_SAMPLE_RATE));
    float delay = v[MP_LFO_DELAY] ? dsp_exp(v[MP_LFO_DELAY] / 14.2f - 3.0f) / 10.0f : 0.0f;
    b->lfo_fade_inc = delay < 0.001f ? 1.0f : n / (delay * EL_SAMPLE_RATE);
    b->drift = v[MP_DRIFT] / 127.0f;
    b->unison = syn_powf(v[MP_UNISON_DETUNE] / 127.0f, 1.5f) * 0.5f;
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
    /* what the voices of this step read (they may run on the other core) */
    b->lfo_value = p->lfo_value;
    b->sub_level = p->sub_level;
    b->noise_level = p->noise_level;
    b->pulse = p->pulse;
    b->saw = p->saw;
    b->sub = p->sub;
}

static inline EL_DSP_HELPER float env_amount(float env, int mode, float velocity)
{
    switch (mode) {
    case 1: return -env;
    case 2: return env * (0.25f + 0.75f * velocity);
    case 3: return -env * (0.25f + 0.75f * velocity);
    default: return env;
    }
}

/* One voice, one control step (n frames), added to left / right. decim: a
 * constant in each caller (render_full, render_half). */
static inline __attribute__((always_inline)) void render_step(const mks_part_t *p, mks_voice_t *v, const mks_slice_t *b,
                                                              float *left, float *right, int n, int unison, const int decim)
{
    if (b->porta) v->pitch += (v->target_pitch - v->pitch) * b->porta_coef;
    else v->pitch = v->target_pitch;
    float pitch = v->pitch, drift_semis = 0, drift_oct = 0;
    if (b->drift > 0) {
        v->wander += syn_xorshift(&v->rng) * 0.02f * (float)n / 16.0f - v->wander * 0.0005f * (float)n;
        v->wander = dsp_clamp(v->wander, -1, 1);
        drift_semis = b->drift * (v->drift_pitch * 0.08f + v->wander * 0.04f);
        drift_oct = b->drift * v->drift_cutoff * 0.15f;
    }
    if (v->lfo_fade < 1.0f) v->lfo_fade = dsp_min(1, v->lfo_fade + b->lfo_fade_inc);
    float lfo = b->lfo_value * v->lfo_fade;
    float env = v->env.value;
    float dco_env = env_amount(env, b->dco_env_mode, v->velocity);
    float vcf_env = env_amount(env, b->vcf_env_mode, v->velocity);
    float semis = pitch + b->pitch_offset + lfo * b->vibrato + dco_env * b->dco_env + drift_semis +
                  v->unison_spread * b->unison;
    float dt = syn_note_hz(semis) / EL_SAMPLE_RATE;
    float pulse_semis = b->pulse_detune + dco_env * b->pulse_env;
    float ratio = pulse_semis == 0 ? 1.0f : dsp_exp2(pulse_semis / 12.0f);
    float octaves = b->cutoff_oct + b->key_follow * (pitch - 60.0f) + vcf_env * b->vcf_env + lfo * b->vcf_lfo + drift_oct;
    syn_ladder_set(&v->ld, 20.0f * dsp_exp2(dsp_clamp(octaves, -2, 11)), b->resonance, decim);
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
    syn_env_settings_t s;
    env_settings(p, v, pitch, &s);
    for (int k = 0; k < n; ++k) {
        float e = syn_env_next(&v->env, &s);
        v->gate_amp += (gate_target - v->gate_amp) * gate_coef;
        float x = dco_tick(b, v, dt, ratio, b->sync) * 0.5f;
        if (hpf_voice) {
            float y = v->hpf_b0 * x + v->hz1;
            v->hz1 = -v->hpf_b0 * x - v->hpf_a1 * y;
            x = y;
        } else if (p->hpf_on) x = syn_biquad(&p->hpf, &v->hz1, &v->hz2, x);
        if (decim > 1) {
            v->vcf_acc += x;
            if (++v->vcf_cnt >= decim) {
                v->vcf_y0 = v->vcf_y1;
                v->vcf_y1 = syn_ladder(&v->ld, v->vcf_acc * (1.0f / decim));
                v->vcf_acc = 0;
                v->vcf_cnt = 0;
            }
            x = v->vcf_y0 + (v->vcf_y1 - v->vcf_y0) * ((float)(v->vcf_cnt + 1) * (1.0f / decim));
            x *= b->gate_mode ? v->gate_amp : e;
        } else {
            x = syn_ladder(&v->ld, x) * (b->gate_mode ? v->gate_amp : e);
        }
        left[k] += x * gl;
        right[k] += x * gr;
    }
    if (b->gate_mode) { if (!v->gate && v->gate_amp < 0.0001f) v->active = 0; }
    else if (v->env.stage == SYN_ENV_IDLE) v->active = 0;
    if (!v->active) v->key = -1;
}

/* One voice, the whole block (its control steps in turn, until it ends). */
#define RENDER_VOICE(NAME, DECIM)                                                                   \
    static EL_DSP_CODE void NAME(const mks_part_t *p, mks_voice_t *v, float *left, float *right, int unison) \
    {                                                                                               \
        for (int k = 0; k < MKS_SLICES && v->active; ++k)                                           \
            render_step(p, v, &p->slice[k], left + k * CONTROL, right + k * CONTROL, CONTROL, unison, DECIM); \
    }
RENDER_VOICE(render_full, 1)
RENDER_VOICE(render_half, 2)

/* ---- a block ------------------------------------------------------------------- */

EL_DSP_CODE void mks_plan(mks_t *s)
{
    unsigned cores = s->cores > 1 ? 2u : 1u;
    s->voices_on[0] = s->voices_on[1] = 0;
    s->parts_on[0] = s->parts_on[1] = 0;
    s->ctl_on[0] = s->ctl_on[1] = 0;
    for (unsigned pi = 0; pi < MKS_PARTS; ++pi) s->part[pi].live = 0;
    /* the voices, the parts' control values, sums and choruses, each to the
     * core with the less to do (measured on the device, docs/adr/0003: a
     * voice ~14 % of a block, a chorus ~6, a part's control values ~4, and
     * core 0 besides this function, the mix and the output ~9). Units:
     * voice 10, chorus 5, control 3, core 0's own work 9. */
    unsigned load[2] = {cores > 1 ? 9u : 0u, cores > 1 ? s->extra1 : 0u};
    for (unsigned i = 0; i < s->pool; ++i) {
        mks_voice_t *v = &s->voice[i];
        if (!v->active) continue;
        if (v->part >= MKS_PARTS) { v->active = 0; continue; }
        v->core = (uint8_t)(cores > 1 && load[1] <= load[0] ? 1u : 0u);
        load[v->core] += 10u;
        s->voices_on[v->core]++;
        s->part[v->part].live = 1;
    }
    for (unsigned pi = 0; pi < MKS_PARTS; ++pi) {
        mks_part_t *p = &s->part[pi];
        if (!p->live && p->chorus.mix < 0.0001f) { p->silent_blocks = 40; continue; }
        if (!p->live) p->live = 2;   /* only its chorus's tail */
        p->ctl_core = (uint8_t)(cores > 1 && load[1] <= load[0] ? 1u : 0u);
        load[p->ctl_core] += 3u;
        s->ctl_on[p->ctl_core]++;
    }
    for (unsigned pi = 0; pi < MKS_PARTS; ++pi) {
        mks_part_t *p = &s->part[pi];
        if (!p->live) continue;
        p->core = (uint8_t)(cores > 1 && load[1] <= load[0] ? 1u : 0u);
        load[p->core] += 5u;
        s->parts_on[p->core]++;
        for (unsigned c = 0; c < cores; ++c)
            for (unsigned i = 0; i < MKS_BLOCK; ++i) s->acc[c][pi][0][i] = s->acc[c][pi][1][i] = 0;
    }
    for (unsigned c = 0; c < MKS_CORES; ++c)
        for (unsigned i = 0; i < MKS_BLOCK; ++i) s->out[c][0][i] = s->out[c][1][i] = s->send[c][0][i] = s->send[c][1][i] = 0;
}

/* The parts are independent of each other here: each core takes its own. */
EL_DSP_CODE void mks_control(mks_t *s, unsigned core, float bpm)
{
    if (core >= MKS_CORES || !s->ctl_on[core]) return;
    for (unsigned pi = 0; pi < MKS_PARTS; ++pi) {
        mks_part_t *p = &s->part[pi];
        if (!p->live || p->ctl_core != core) continue;
        for (unsigned k = 0; k < MKS_SLICES; ++k) control(p, &p->slice[k], bpm);
    }
}

EL_DSP_CODE void mks_begin(mks_t *s, float bpm)
{
    mks_plan(s);
    mks_control(s, 0, bpm);
    mks_control(s, 1, bpm);
}

EL_DSP_CODE void mks_voices(mks_t *s, unsigned core)
{
    if (core >= MKS_CORES || !s->voices_on[core]) return;
    for (unsigned i = 0; i < s->pool; ++i) {
        mks_voice_t *v = &s->voice[i];
        if (!v->active || v->core != core) continue;
        const mks_part_t *p = &s->part[v->part];
        int unison = p->v[MP_KEY_MODE] == 3 && s->pool > 1;
        if (s->vcf_half) render_half(p, v, s->acc[core][v->part][0], s->acc[core][v->part][1], unison);
        else render_full(p, v, s->acc[core][v->part][0], s->acc[core][v->part][1], unison);
    }
}

EL_DSP_CODE void mks_finish(mks_t *s, unsigned core)
{
    if (core >= MKS_CORES || !s->parts_on[core]) return;
    for (unsigned pi = 0; pi < MKS_PARTS; ++pi) {
        mks_part_t *p = &s->part[pi];
        if (!p->live || p->core != core) continue;
        float *l = s->acc[0][pi][0], *r = s->acc[0][pi][1];
        if (s->cores > 1) {
            const float *l1 = s->acc[1][pi][0], *r1 = s->acc[1][pi][1];
            for (int i = 0; i < MKS_BLOCK; ++i) { l[i] += l1[i]; r[i] += r1[i]; }
        }
        float g = p->volume_smoothed * 0.5f * p->level;
        for (int i = 0; i < MKS_BLOCK; ++i) { l[i] *= g; r[i] *= g; }
        syn_chorus(&p->chorus, l, r, MKS_BLOCK, p->v[MP_CHORUS] > 0, p->v[MP_CHORUS] > 0 ? p->v[MP_CHORUS] - 1 : 0,
               syn_curve_exp(syn_curve_chorus, p->v[MP_CHORUS_RATE]));
        float *ol = s->out[core][0], *orr = s->out[core][1], *sd = s->send[core][0], *sr = s->send[core][1];
        float peak = 0;
        /* the sends, as FM1-Electro's mks_render had them: the mono mix times (send / 127)^1.5 */
        float ds = syn_powf(p->v[MP_DELAY_SEND] / 127.0f, 1.5f), rs = syn_powf(p->v[MP_REVERB_SEND] / 127.0f, 1.5f);
        for (int i = 0; i < MKS_BLOCK; ++i) {
            float m = 0.5f * (l[i] + r[i]);
            ol[i] += l[i];
            orr[i] += r[i];
            sd[i] += m * ds;
            sr[i] += m * rs;
            peak = dsp_max(peak, dsp_abs(m));
        }
        p->silent_blocks = p->live == 1 || peak > 1e-5f ? 0 : (uint8_t)(p->silent_blocks < 255 ? p->silent_blocks + 1 : 255);
    }
}

EL_DSP_CODE void mks_mix(mks_t *s, float *left, float *right)
{
    for (int i = 0; i < MKS_BLOCK; ++i) s->send_sum[0][i] = s->send_sum[1][i] = 0;
    for (unsigned c = 0; c < (s->cores > 1 ? 2u : 1u); ++c) {
        if (!s->parts_on[c]) continue;
        for (int i = 0; i < MKS_BLOCK; ++i) { s->send_sum[0][i] += s->send[c][0][i]; s->send_sum[1][i] += s->send[c][1][i]; }
    }
    for (unsigned c = 0; c < (s->cores > 1 ? 2u : 1u); ++c) {
        if (!s->parts_on[c]) continue;
        const float *l = s->out[c][0], *r = s->out[c][1];
        for (int i = 0; i < MKS_BLOCK; ++i) { left[i] += l[i]; right[i] += r[i]; }
    }
    if (s->release_hurry) {
        /* FM1-Electro's MKS_RELEASE_HURRY: a release that has faded far
         * finishes 8 times faster (the voice is free sooner); with every
         * voice busy already from -30 dB, else from -46 dB. */
        unsigned busy = 0;
        for (unsigned i = 0; i < s->pool; ++i) busy += s->voice[i].active;
        float limit = busy >= s->pool ? 0.03f : 0.005f;
        for (unsigned i = 0; i < s->pool; ++i) {
            syn_env_t *e = &s->voice[i].env;
            if (s->voice[i].active && e->stage == SYN_ENV_RELEASE && !e->hurry && e->value < limit) {
                e->hurry = 1;
                e->increment *= 8.0f;
                e->curve_decay = dsp_exp(-4.5f * e->increment);
            }
        }
    }
}

EL_DSP_CODE void mks_render(mks_t *s, float *left, float *right, float bpm)
{
    mks_begin(s, bpm);
    for (unsigned c = 0; c < MKS_CORES; ++c) mks_voices(s, c);
    for (unsigned c = 0; c < MKS_CORES; ++c) mks_finish(s, c);
    mks_mix(s, left, right);
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
    default: return 0;
    }
}

float mks_lfo_hz(int rate) { return syn_curve_exp(syn_curve_lfo, (float)el_clampi(rate, 0, 127)); }
