/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P voice engine for FM1-Electro (engine/jx8p.h: what it is, where it comes from, what changed).
 *
 * The voice (osc_tick, control, render_step), the curves and the display helpers are fm1-synths'
 * jx8p/engine/jx.c (github.com/robtron-byte/fm1-synths, commit 2a7b4fa, GPL-3.0-only, robtron-byte), a port
 * of the user's JX-8P iOS app (github.com/robtron-byte/JX8P, commit 9687574), unchanged but for the names
 * (JP_ -> JX_, syn_ -> jxd_, jx_ -> jx8p_). The voices of a part, the block and the sends are FM1-Electro's
 * engine/mks.c (one part per area, its own voices). The audio path uses no libc math: approximations from
 * dsp_math.h. Compiled only with EL_SYNTH_JX (engine/el_config.h, docs/adr/0039). */
#include "el_config.h"
#if EL_SYNTH_JX
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "jx8p.h"
#include "jx8p_dsp_inline.h"
#include <string.h>

enum { CONTROL = JX8P_CONTROL };

/* ---- curves (HardwareCurves.swift) ------------------------------------------------- */

static const float dynamics_amount[4] EL_DSP_DATA = {0, 0.35f, 0.65f, 1};
static const float env_key_amount[4] EL_DSP_DATA = {0, 0.25f, 0.5f, 1};

static inline float unit(float raw) { return dsp_clamp(raw, 0.0f, 127.0f) * (1.0f / 127.0f); }
static inline float snap_scale(float raw) { return 0.25f + 0.75f * dsp_clamp(raw, 0.0f, 24.0f) / 24.0f; }
/* ATTACK: about 1 ms ... 3 s; DECAY / RELEASE: about 3 ms ... 12 s; both exponential. */
static inline float attack_seconds(float raw, int snap) { float s = 0.001f * dsp_exp2(unit(raw) * 11.5f); return snap ? s * snap_scale(raw) : s; }
static inline float decay_seconds(float raw, int snap) { float s = 0.003f * dsp_exp2(unit(raw) * 12.0f); return snap ? s * snap_scale(raw) : s; }
static inline int sw4(int v) { return v < 0 ? 0 : v > 3 ? 3 : v; }

/* SynthEngine.adsr: the JX-8P ADSR on the 4-segment envelope (T1 attack to 1, T2 decay to S, T3 holds S,
 * T4 release). */
static void adsr(jxd_env_settings_t *e, int attack, int decay, int sustain, int release, int snap)
{
    float s = dsp_clamp((float)sustain, 0.0f, 127.0f) / 127.0f;
    e->t1 = attack_seconds((float)attack, snap); e->l1 = 1;
    e->t2 = decay_seconds((float)decay, snap); e->l2 = s;
    e->t3 = 0.001f; e->l3 = s;
    e->t4 = decay_seconds((float)release, snap);
}

/* ---- the two DCOs (JXOscSettings, JXOscillators) ------------------------------------ */


/* JX-8P waveform switch: 0 NOISE, 1 SQUARE, 2 PULSE (about 22 %), 3 SAW. */
static void osc_shape(jxd_chop_t *c, int wave)
{
    switch (wave) {
    case 1: jxd_chop_set(c, 0, 1, 0.5f, 0, 0.5f); break;
    case 2: jxd_chop_set(c, 0, 1, 0.78f, 0, 0.5f); break;
    default: jxd_chop_set(c, 1, 0, 0.5f, 0, 0.5f); break;   /* saw (noise uses the generator) */
    }
}

/* A slave reset at its master's wrap: the jump band-limited on the master's phase (JXOscillators.synced). */
static inline EL_DSP_HELPER float osc_synced(const jxd_chop_t *shape, float *slave, float slave_dt, float master_phase, float master_dt)
{
    float since = master_phase / (master_dt > 1e-9f ? master_dt : 1e-9f);
    float before = jxd_wrap01(*slave - slave_dt * since);
    *slave = jxd_wrap01(slave_dt * since);
    float h = jxd_chop_naive(shape, 0) - jxd_chop_naive(shape, before);
    return 2.0f * jxd_chop_naive(shape, *slave) - 1.0f + h * jxd_polyblep(master_phase, master_dt);
}

#define XMOD_DEPTH 1.5f   /* XMOD: DCO-2 frequency deviation per unit of DCO-1 output */

static inline __attribute__((always_inline)) float osc_tick(const jx8p_slice_t *b, jx8p_voice_t *v, float raw_dt1, float raw_dt2,
                                                           float gain1, float gain2)
{
    float dt1 = raw_dt1 < 0.45f ? raw_dt1 : 0.45f;
    float dt2 = raw_dt2 < 0.45f ? raw_dt2 : 0.45f;
    if (b->xmod == 3) dt2 = dsp_clamp(dt2 * (1.0f + XMOD_DEPTH * v->last1), 0.0f, 0.45f);
    v->phase1 += dt1;
    int wrapped1 = v->phase1 >= 1.0f;
    if (wrapped1) v->phase1 -= 1.0f;
    v->phase2 += dt2;
    int wrapped2 = v->phase2 >= 1.0f;
    if (wrapped2) v->phase2 = jxd_wrap01(v->phase2);
    float o1, o2;
    if (b->xmod == 2 && wrapped2 && !b->noise1) o1 = osc_synced(&b->wave1, &v->phase1, dt1, v->phase2, dt2);
    else o1 = b->noise1 ? jxd_xorshift(&v->noise) : jxd_chop_render(&b->wave1, v->phase1, dt1);
    if (b->xmod == 1 && wrapped1 && !b->noise2) o2 = osc_synced(&b->wave2, &v->phase2, dt2, v->phase1, dt1);
    else o2 = b->noise2 ? jxd_xorshift(&v->noise) : jxd_chop_render(&b->wave2, v->phase2, dt2);
    v->last1 = o1;
    if (b->noise1) o1 *= 0.7f;
    if (b->noise2) o2 *= 0.7f;
    return o1 * gain1 + o2 * gain2;
}

/* ---- HPF: flat, then 1-pole high-passes at about 150 / 330 / 700 Hz ---------------- */

static void hpf_configure(jx8p_part_t *p, int mode)
{
    static const float hz[4] = {0, 150.0f, 330.0f, 700.0f};
    jxd_biquad_t *c = &p->hpf;
    p->hpf_mode = (uint8_t)mode;
    if (mode > 0) {
        float k = dsp_tan(DSP_PI * hz[mode & 3] / EL_SAMPLE_RATE), a0 = 1 + k;
        c->b0 = 1 / a0; c->b1 = -1 / a0; c->b2 = 0; c->a1 = (k - 1) / a0; c->a2 = 0;
    }
}

/* ---- the part and its voices (engine/mks.c) ---------------------------------------- */

static void base_envs(jx8p_part_t *p)
{
    const uint8_t *v = p->v;
    int snap = v[JX_ENV_SNAP] == 1;
    adsr(&p->base1, v[JX_E1_ATTACK], v[JX_E1_DECAY], v[JX_E1_SUSTAIN], v[JX_E1_RELEASE], snap);
    adsr(&p->base2, v[JX_E2_ATTACK], v[JX_E2_DECAY], v[JX_E2_SUSTAIN], v[JX_E2_RELEASE], snap);
    p->env_gen++;
}

EL_CTRL_CODE void jx8p_part_init(jx8p_part_t *p, unsigned voices, uint32_t seed)
{
    memset(p, 0, sizeof *p);
    p->voices = (uint8_t)(voices < 1 ? 1 : voices > JX8P_MAX_VOICES ? JX8P_MAX_VOICES : voices);
    for (unsigned i = 0; i < JX8P_MAX_VOICES; ++i) {
        jx8p_voice_t *v = &p->voice[i];
        v->key = -1;
        v->noise = seed + i * 0x9e3779b9u;
        if (!v->noise) v->noise = 0x9e3779b9u;
        v->rng = (seed + i) * 2654435761u;
        if (!v->rng) v->rng = 1;
        v->drift_pitch = jxd_xorshift(&v->rng);
        v->drift_cutoff = jxd_xorshift(&v->rng);
        v->drift_env = jxd_xorshift(&v->rng);
        v->lfo_fade = 1;
    }
    p->lfo_rng = seed ^ 0xa5a5a5a5u;
    p->chorus.hiss = 0xb0b0cafeu ^ seed;
    p->chorus.depth = 80;
    p->level = 1;
    jx8p_part_load(p, jx8p_presets[0].v);
}

EL_DSP_CODE int jx8p_active(const jx8p_part_t *p)
{
    for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].active) return 1;
    return p->chorus.mix > 0.0001f && p->silent_blocks < 40;
}

/* Both envelopes' settings for a voice at a pitch (SynthEngine.envelopeSettings): key follow in 4 steps
 * (higher notes faster: 2^(-amount) per octave), drift; T3 stays. Worked out again only when the sound's
 * envelope knobs, the pitch or the drift changed. */
static EL_DSP_HELPER void env_settings(const jx8p_part_t *p, jx8p_voice_t *v, float pitch)
{
    if (v->es_ok && v->es_gen == p->env_gen && v->es_pitch == pitch && v->es_drift == v->drift_env) return;
    v->es_ok = 1;
    v->es_gen = p->env_gen;
    v->es_pitch = pitch;
    v->es_drift = v->drift_env;
    float drift = 1.0f + p->v[JX_DRIFT] / 127.0f * v->drift_env * 0.08f;
    for (int which = 1; which <= 2; ++which) {
        float follow = env_key_amount[sw4(p->v[which == 1 ? JX_E1_KEY : JX_E2_KEY])], scale = drift;
        if (follow > 0) scale *= dsp_exp2(-follow * (pitch - 60.0f) / 12.0f);
        jxd_env_settings_t *e = which == 1 ? &v->es1 : &v->es2;
        *e = which == 1 ? p->base1 : p->base2;
        e->t1 *= scale; e->t2 *= scale; e->t4 *= scale;
    }
}

static EL_DSP_HELPER void release(const jx8p_part_t *p, jx8p_voice_t *v)
{
    v->gate = 0;
    env_settings(p, v, v->pitch);
    jxd_env_begin(&v->env1, JXD_ENV_RELEASE, 0, v->es1.t4);
    jxd_env_begin(&v->env2, JXD_ENV_RELEASE, 0, v->es2.t4);
}

EL_DSP_CODE void jx8p_all_off(jx8p_part_t *p)
{
    for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].gate) release(p, &p->voice[i]);
    p->held_count = 0;
}

EL_DSP_CODE void jx8p_part_load(jx8p_part_t *p, const uint8_t v[JX_COUNT])
{
    int mode_changed = p->v[JX_KEY_MODE] != v[JX_KEY_MODE];
    for (unsigned i = 0; i < JX_COUNT; ++i) p->v[i] = v[i];
    if (mode_changed) jx8p_all_off(p);
    hpf_configure(p, sw4(p->v[JX_HPF]));
    base_envs(p);
    p->cutoff_smoothed = p->v[JX_CUTOFF];
}

EL_DSP_CODE void jx8p_part_set(jx8p_part_t *p, unsigned param, int value)
{
    if (param >= JX_COUNT) return;
    int mode_changed = param == JX_KEY_MODE && p->v[param] != value;
    p->v[param] = (uint8_t)el_clampi(value, 0, jx8p_param_max(param));
    if (mode_changed) jx8p_all_off(p);
    if (param == JX_HPF) hpf_configure(p, sw4(p->v[JX_HPF]));
    if ((param >= JX_E1_ATTACK && param <= JX_E2_RELEASE) || param == JX_ENV_SNAP) base_envs(p);
}

static float last_pitch_of(const jx8p_part_t *p)
{
    uint32_t best = 0; float pitch = -1;
    for (unsigned i = 0; i < p->voices; ++i)
        if (p->voice[i].active && p->voice[i].age >= best) { best = p->voice[i].age; pitch = p->voice[i].pitch; }
    return pitch;
}

static EL_DSP_HELPER void start_voice(jx8p_part_t *p, jx8p_voice_t *v, int note, float velocity, int glide, float from, int retrigger)
{
    int was_active = v->active;
    v->key = (int16_t)note;
    v->velocity = velocity;
    v->gate = 1;
    v->age = ++p->age;
    v->target_pitch = (float)note + (float)((int)p->v[JX_KEY_SHIFT] - 12);
    v->pitch = glide && from >= 0 ? from : v->target_pitch;
    if (!retrigger && was_active) return;
    v->lfo_fade = p->v[JX_LFO_DELAY] > 0 ? 0.0f : 1.0f;
    if (!was_active) {
        jxd_ladder_reset(&v->ld);
        v->hz1 = v->hz2 = 0;
        v->env1.stage = v->env2.stage = JXD_ENV_IDLE;
        v->env1.value = v->env2.value = 0;
        v->phase1 = v->phase2 = v->last1 = 0;
        v->gate_amp = 0;
    }
    v->active = 1;
    env_settings(p, v, v->target_pitch);
    jxd_env_begin(&v->env1, JXD_ENV_T1, v->es1.l1, v->es1.t1);
    jxd_env_begin(&v->env2, JXD_ENV_T1, v->es2.l1, v->es2.t1);
}

/* A voice of the part: a free one, else the oldest released, else the oldest (mks.c's find_voice). */
static jx8p_voice_t *find_voice(jx8p_part_t *p)
{
    jx8p_voice_t *best = 0;
    for (unsigned i = 0; i < p->voices; ++i) if (!p->voice[i].active) return &p->voice[i];
    for (unsigned i = 0; i < p->voices; ++i)
        if (!p->voice[i].gate && (!best || p->voice[i].age < best->age)) best = &p->voice[i];
    if (best) return best;
    for (unsigned i = 0; i < p->voices; ++i)
        if (!best || p->voice[i].age < best->age) best = &p->voice[i];
    return best;
}

static int any_gate(const jx8p_part_t *p)
{
    for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].gate) return 1;
    return 0;
}

EL_DSP_CODE void jx8p_note_on(jx8p_part_t *p, int note, float velocity, int legato)
{
    int mode = p->v[JX_KEY_MODE], porta = p->v[JX_PORTA];
    int held = any_gate(p) || legato;
    int glide = porta == 1 || (porta == 2 && held);
    float from = last_pitch_of(p);
    if (p->v[JX_LFO_KEYSYNC] && !held) {
        p->lfo_phase = 0;
        p->lfo_held = jxd_xorshift(&p->lfo_rng);
        p->lfo_from = p->lfo_to;
    }
    if (mode == 2 || mode == 3) {
        /* Mono / unison: newest note wins; legato keeps the envelopes running when ENV RETRIG is LEGATO
         * or the step is tied. Unison: every voice of the part (up to six). */
        if (p->held_count < 8) p->held[p->held_count++] = (int16_t)note;
        int retrigger = !(held && (p->v[JX_ENV_RETRIG] == 1 || legato));
        unsigned count = mode == 3 ? p->voices : 1u;
        for (unsigned i = 0; i < count; ++i) {
            jx8p_voice_t *v = &p->voice[i];
            v->unison_spread = count > 1 ? -1.0f + 2.0f * (float)i / (float)(count - 1) : 0.0f;
            start_voice(p, v, note, velocity, glide, from, retrigger || !v->active);
        }
        return;
    }
    jx8p_voice_t *v = 0;
    for (unsigned i = 0; i < p->voices; ++i)
        if (p->voice[i].active && p->voice[i].key == note) { v = &p->voice[i]; break; }
    if (!v) v = find_voice(p);
    v->unison_spread = 0;
    start_voice(p, v, note, velocity, glide, from, 1);
}

EL_DSP_CODE void jx8p_note_off(jx8p_part_t *p, int note)
{
    int mode = p->v[JX_KEY_MODE];
    if (mode == 2 || mode == 3) {
        unsigned n = 0;
        for (unsigned i = 0; i < p->held_count; ++i) if (p->held[i] != note) p->held[n++] = p->held[i];
        p->held_count = (uint8_t)n;
        if (n) {
            for (unsigned i = 0; i < p->voices; ++i) {
                jx8p_voice_t *v = &p->voice[i];
                if (!v->gate) continue;
                v->key = p->held[n - 1];
                v->target_pitch = (float)p->held[n - 1] + (float)((int)p->v[JX_KEY_SHIFT] - 12);
                if (!p->v[JX_PORTA]) v->pitch = v->target_pitch;
            }
            return;
        }
        for (unsigned i = 0; i < p->voices; ++i) if (p->voice[i].gate) release(p, &p->voice[i]);
        return;
    }
    for (unsigned i = 0; i < p->voices; ++i) {
        jx8p_voice_t *v = &p->voice[i];
        if (v->active && v->gate && v->key == note) release(p, v);
    }
}

/* ---- per control step (SynthEngine.renderControlBlock, updateLFO) ------------------- */

static EL_DSP_CODE void control(jx8p_part_t *p, jx8p_slice_t *b, float bpm)
{
    const uint8_t *v = p->v;
    const float n = CONTROL;
    int wrapped = 0;
    /* LFO */
    if (v[JX_LFO_SYNC]) {
        float quarters = jxd_divisions[v[JX_LFO_DIV] < 14 ? v[JX_LFO_DIV] : 13];
        p->lfo_phase += (bpm < 20 ? 20 : bpm) / 60.0f / quarters * n / EL_SAMPLE_RATE;
    } else {
        p->lfo_phase += jxd_curve_exp(jxd_curve_lfo, v[JX_LFO_RATE]) * n / EL_SAMPLE_RATE;
    }
    if (p->lfo_phase >= 1.0f) { p->lfo_phase = jxd_wrap01(p->lfo_phase); wrapped = 1; }
    float ph = p->lfo_phase;
    /* SHAPE 0 follows the JX-8P switch: RANDOM (sample & hold), SQUARE, SINE */
    int shape = v[JX_LFO_SHAPE] > 0 ? v[JX_LFO_SHAPE] - 1 : v[JX_LFO_WAVE] == 0 ? 5 : v[JX_LFO_WAVE] == 1 ? 4 : 1;
    switch (shape) {
    case 1: p->lfo_value = dsp_sin(2 * DSP_PI * ph); break;
    case 2: p->lfo_value = 2 * ph - 1; break;
    case 3: p->lfo_value = 1 - 2 * ph; break;
    case 4: p->lfo_value = ph < 0.5f ? 1.0f : -1.0f; break;
    case 5:
        if (wrapped) p->lfo_held = jxd_xorshift(&p->lfo_rng);
        p->lfo_value = p->lfo_held;
        break;
    case 6:
        if (wrapped) { p->lfo_from = p->lfo_to; p->lfo_to = jxd_xorshift(&p->lfo_rng); }
        p->lfo_value = p->lfo_from + (p->lfo_to - p->lfo_from) * (0.5f - 0.5f * dsp_cos(DSP_PI * ph));
        break;
    default: p->lfo_value = 1 - 4 * dsp_abs(ph - 0.5f); break;
    }
    /* the DCOs' shapes */
    osc_shape(&b->wave1, v[JX_DCO1_WAVE]);
    osc_shape(&b->wave2, v[JX_DCO2_WAVE]);
    b->noise1 = v[JX_DCO1_WAVE] == 0;
    b->noise2 = v[JX_DCO2_WAVE] == 0;
    b->xmod = sw4(v[JX_DCO2_XMOD]);
    p->cutoff_smoothed += (v[JX_CUTOFF] - p->cutoff_smoothed) * dsp_min(1, n / (0.01f * EL_SAMPLE_RATE));
    float volume = (v[JX_VOLUME] / 127.0f) * (v[JX_VOLUME] / 127.0f);
    p->volume_smoothed += (volume - p->volume_smoothed) * dsp_min(1, n / (0.02f * EL_SAMPLE_RATE));
    if (p->hpf_mode != v[JX_HPF]) hpf_configure(p, sw4(v[JX_HPF]));

    b->dco_env_mode = sw4(v[JX_DCO_ENV_MODE]);
    b->mix_env_mode = sw4(v[JX_MIX_ENV_MODE]);
    b->vcf_env_mode = sw4(v[JX_VCF_ENV_MODE]);
    b->gate_mode = v[JX_VCA_ENV_MODE] == 0;
    b->dco_dyn = dynamics_amount[sw4(v[JX_DCO_DYNA])];
    b->mix_dyn = dynamics_amount[sw4(v[JX_MIX_DYNA])];
    b->vcf_dyn = dynamics_amount[sw4(v[JX_VCF_DYNA])];
    b->vca_dyn = dynamics_amount[sw4(v[JX_VCA_DYNA])];
    /* vibrato: the DCO LFO depths (mod wheel, bender LFO and aftertouch: none on the FM-1) */
    float u1 = unit(v[JX_DCO1_LFO]), u2 = unit(v[JX_DCO2_LFO]);
    b->vibrato1 = u1 * u1 * 12.0f;
    b->vibrato2 = u2 * u2 * 12.0f;
    float e1 = unit(v[JX_DCO1_ENV]), e2 = unit(v[JX_DCO2_ENV]);
    b->dco1_env = e1 * e1 * 36.0f;
    b->dco2_env = e2 * e2 * 36.0f;
    /* RANGE 0 = 16', 1 = 8', 2 = 4', 3 = 2'; TUNE 0..24 = -12..+12; FINE ±50 cents around 64 */
    b->pitch1 = (float)((sw4(v[JX_DCO1_RANGE]) - 1) * 12 + (int)v[JX_DCO1_TUNE] - 12);
    b->pitch2 = (float)((sw4(v[JX_DCO2_RANGE]) - 1) * 12 + (int)v[JX_DCO2_TUNE] - 12) +
                (dsp_clamp((float)v[JX_DCO2_FINE], 0.0f, 127.0f) - 64.0f) / 128.0f;
    float m1 = unit(v[JX_MIX_DCO1]), m2 = unit(v[JX_MIX_DCO2]);
    b->mix1 = m1 * m1;
    b->mix2 = m2 * m2;
    b->mix_env = v[JX_MIX_ENV] / 127.0f;
    b->porta = v[JX_PORTA] != 0;
    float porta_seconds = (v[JX_PORTA_TIME] / 127.0f) * (v[JX_PORTA_TIME] / 127.0f) * 3.0f;
    b->porta_coef = porta_seconds < 0.001f ? 1.0f : 1.0f - dsp_exp(-n / (porta_seconds * EL_SAMPLE_RATE));
    float delay = v[JX_LFO_DELAY] ? dsp_exp(v[JX_LFO_DELAY] / 14.2f - 3.0f) / 10.0f : 0.0f;
    b->lfo_fade_inc = delay < 0.001f ? 1.0f : n / (delay * EL_SAMPLE_RATE);
    b->drift = v[JX_DRIFT] / 127.0f;
    b->unison = jxd_powf(v[JX_UNISON_DETUNE] / 127.0f, 1.5f) * 0.5f;
    b->cutoff_oct = unit(p->cutoff_smoothed) * 10.3f - 0.3f;
    b->key_follow = v[JX_VCF_KEY] / 127.0f / 12.0f;
    b->vcf_env = unit(v[JX_VCF_ENV]) * 9.0f;
    float l = unit(v[JX_VCF_LFO]);
    b->vcf_lfo = l * l * 4.0f;
    b->resonance = v[JX_RESO] / 127.0f;
    b->amp = v[JX_VCA_LEVEL] / 127.0f;
    b->gate_coef = 0.0730f;    /* 1 - exp(-1 / (0.0003 * 44100)) */
    b->gate_release = 0.00189f; /* 1 - exp(-1 / (0.012 * 44100)) */
    b->lfo_value = p->lfo_value;
}

/* JX-8P ENV MODE: 0 ENV-2 inverted, 1 ENV-2, 2 ENV-1 inverted, 3 ENV-1; times the DYNAMICS factor. */
static inline EL_DSP_HELPER float env_amount(float e1, float e2, int mode, float dyn, float velocity)
{
    float e = mode == 0 ? -e2 : mode == 1 ? e2 : mode == 2 ? -e1 : e1;
    return e * (1.0f - dyn + dyn * velocity);
}

/* One voice, one control step (SynthEngine.renderVoice). */
static inline __attribute__((always_inline)) void render_step(const jx8p_part_t *p, jx8p_voice_t *v, const jx8p_slice_t *b,
                                                              float *left, float *right, int n, int unison)
{
    if (b->porta) v->pitch += (v->target_pitch - v->pitch) * b->porta_coef;
    else v->pitch = v->target_pitch;
    float pitch = v->pitch, drift_semis = 0, drift_oct = 0;
    if (b->drift > 0) {
        v->wander += jxd_xorshift(&v->rng) * 0.02f * (float)n / 16.0f - v->wander * 0.0005f * (float)n;
        v->wander = dsp_clamp(v->wander, -1, 1);
        drift_semis = b->drift * (v->drift_pitch * 0.08f + v->wander * 0.04f);
        drift_oct = b->drift * v->drift_cutoff * 0.15f;
    }
    if (v->lfo_fade < 1.0f) v->lfo_fade = dsp_min(1, v->lfo_fade + b->lfo_fade_inc);
    float lfo = b->lfo_value * v->lfo_fade;
    float e1 = v->env1.value, e2 = v->env2.value, vel = v->velocity;
    float dco_env = env_amount(e1, e2, b->dco_env_mode, b->dco_dyn, vel);
    float vcf_env = env_amount(e1, e2, b->vcf_env_mode, b->vcf_dyn, vel);
    float mix_env = env_amount(e1, e2, b->mix_env_mode, b->mix_dyn, vel);
    float common = pitch + drift_semis + v->unison_spread * b->unison;
    float dt1 = jxd_note_hz(common + b->pitch1 + lfo * b->vibrato1 + dco_env * b->dco1_env) / EL_SAMPLE_RATE;
    float dt2 = jxd_note_hz(common + b->pitch2 + lfo * b->vibrato2 + dco_env * b->dco2_env) / EL_SAMPLE_RATE;
    float gain1 = b->mix1, gain2 = dsp_clamp(b->mix2 + mix_env * b->mix_env, 0.0f, 1.5f);
    float octaves = b->cutoff_oct + b->key_follow * (pitch - 60.0f) + vcf_env * b->vcf_env + lfo * b->vcf_lfo + drift_oct;
    jxd_ladder_set(&v->ld, 20.0f * dsp_exp2(dsp_clamp(octaves, -2, 11)), b->resonance, 1);
    float amp = b->amp * (1.0f - b->vca_dyn + b->vca_dyn * vel) * (unison ? 0.45f : 1.0f);
    float pan = unison ? v->unison_spread * 0.85f : 0.0f;
    float gl = amp * dsp_min(1, 1 - pan), gr = amp * dsp_min(1, 1 + pan);
    float gate_target = v->gate ? 1.0f : 0.0f, gate_coef = v->gate ? b->gate_coef : b->gate_release;
    int hpf_on = p->hpf_mode > 0;
    env_settings(p, v, pitch);
    for (int k = 0; k < n; ++k) {
        (void)jxd_env_next(&v->env1, &v->es1);
        float e = jxd_env_next(&v->env2, &v->es2);
        v->gate_amp += (gate_target - v->gate_amp) * gate_coef;
        float x = osc_tick(b, v, dt1, dt2, gain1, gain2) * 0.5f;
        if (hpf_on) x = jxd_biquad(&p->hpf, &v->hz1, &v->hz2, x);
        x = jxd_ladder(&v->ld, x) * (b->gate_mode ? v->gate_amp : e);
        left[k] += x * gl;
        right[k] += x * gr;
    }
    if (b->gate_mode) { if (!v->gate && v->gate_amp < 0.0001f) v->active = 0; }
    else if (v->env2.stage == JXD_ENV_IDLE) v->active = 0;
    if (!v->active) v->key = -1;
}

/* CHORUS switch OFF / I / II; CHORUS MODE: JX (the switch), I+II, FREE (own rate). */
static inline int chorus_mode(const uint8_t *v)
{
    switch (v[JX_CHORUS_MODE]) {
    case 1: return 3;
    case 2: return 0;
    default: return v[JX_CHORUS] == 2 ? 2 : 1;
    }
}

/* ---- a block (mks_render) ------------------------------------------------------------ */

EL_DSP_CODE void jx8p_render(jx8p_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm)
{
    static float l[EL_BLOCK], r[EL_BLOCK];
    static jx8p_slice_t b;
    if (frames <= 0) return;
    int any = 0;
    for (unsigned i = 0; i < p->voices; ++i) any |= p->voice[i].active;
    if (!any && p->chorus.mix < 0.0001f) { p->silent_blocks = 40; return; }
    int unison = p->v[JX_KEY_MODE] == 3 && p->voices > 1;
    for (int i = 0; i < frames; ++i) l[i] = r[i] = 0;
    for (int pos = 0; pos < frames; pos += CONTROL) {
        int n = frames - pos < CONTROL ? frames - pos : CONTROL;
        control(p, &b, bpm);
        for (unsigned i = 0; i < p->voices; ++i)
            if (p->voice[i].active) render_step(p, &p->voice[i], &b, l + pos, r + pos, n, unison);
    }
    float g = p->volume_smoothed * 0.5f * p->level;
    for (int i = 0; i < frames; ++i) { l[i] *= g; r[i] *= g; }
    jxd_chorus(&p->chorus, l, r, frames, p->v[JX_CHORUS] > 0, chorus_mode(p->v),
               jxd_curve_exp(jxd_curve_chorus, p->v[JX_CHORUS_RATE]));
    float ds = jxd_powf(p->v[JX_DELAY_SEND] / 127.0f, 1.5f), rs = jxd_powf(p->v[JX_REVERB_SEND] / 127.0f, 1.5f);
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

/* ---- value ranges (the audio side clamps with them too; names: engine/jx8p_names.c) ---- */

int jx8p_param_is_tone(unsigned p) { return p < JX_TONE_END; }

int jx8p_param_max(unsigned p)
{
    if (p >= JX_COUNT) return 0;
    switch (p) {
    case JX_DCO1_RANGE: case JX_DCO1_WAVE: case JX_DCO2_RANGE: case JX_DCO2_WAVE: case JX_DCO2_XMOD:
    case JX_DCO_DYNA: case JX_DCO_ENV_MODE: case JX_MIX_DYNA: case JX_MIX_ENV_MODE: case JX_HPF:
    case JX_VCF_DYNA: case JX_VCF_ENV_MODE: case JX_VCA_DYNA: case JX_E1_KEY: case JX_E2_KEY: return 3;
    case JX_CHORUS: case JX_LFO_WAVE: case JX_CHORUS_MODE: return 2;
    case JX_VCA_ENV_MODE: return 1;
    case JX_DCO1_TUNE: case JX_DCO2_TUNE: case JX_KEY_SHIFT: return 24;
    case JX_BENDER: return 12;
    case JX_KEY_MODE: return 3;
    case JX_PORTA: return 2;
    case JX_LFO_SHAPE: return 7;
    case JX_LFO_DIV: return 13;
    case JX_LFO_SYNC: case JX_LFO_KEYSYNC: case JX_ENV_RETRIG: case JX_ENV_SNAP: return 1;
    case JX_CHORD: return MKS_CHORDS - 1;
    default: return 127;
    }
}

#endif
