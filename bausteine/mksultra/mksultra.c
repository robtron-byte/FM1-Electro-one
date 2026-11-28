/* SPDX-License-Identifier: GPL-3.0-only
 * mksultra: Stimme (voice.rs), Parameter (params.rs) und Synth (engine.rs) der Vorlage „MKS Ultra“, nach C übertragen.
 * Siehe mksultra.h und README.md. */
#include "mksultra.h"
#include "mksultra_fx.h"

#define MAX_VOICES 12 /* Stimmen der Vorlage (bestimmt die Reihenfolge der Zufallswerte) */
#define NV MKSULTRA_VOICES
#if NV < 1 || NV > 12
#error "MKSULTRA_VOICES muss zwischen 1 und 12 liegen"
#endif
#define ARP_SEED 0x2a2a2a2a

/* ---------- params.rs ---------- */
typedef struct { const char *id; int is_int; float min, max, def; } pinfo_t;
static const pinfo_t PARAMS[MKSULTRA_PARAMS] = {
    {"Volume", 0, 0.0f, 1.0f, 0.5f},
    {"DcoEnvMode", 1, 0.0f, 3.0f, 0.0f},
    {"VcfEnvMode", 1, 0.0f, 3.0f, 0.0f},
    {"VcaEnvMode", 1, 0.0f, 3.0f, 0.0f},
    {"DcoWavePulse", 1, 0.0f, 4.0f, 0.0f},
    {"DcoWaveSaw", 1, 0.0f, 5.0f, 1.0f},
    {"DcoWaveSub", 1, 0.0f, 5.0f, 0.0f},
    {"DcoRange", 1, 0.0f, 3.0f, 1.0f},
    {"DcoSubLevel", 0, 0.0f, 3.0f, 0.0f},
    {"Noise", 0, 0.0f, 3.0f, 0.0f},
    {"HpfCutoff", 1, 0.0f, 3.0f, 1.0f},
    {"DcoChorusSwitch", 1, 0.0f, 1.0f, 0.0f},
    {"DcoLfoMod", 0, 0.0f, 1.0f, 0.0f},
    {"DcoEnvMod", 0, 0.0f, 1.0f, 0.0f},
    {"DcoAtMod", 0, 0.0f, 1.0f, 0.0f},
    {"DcoPwmDepth", 0, 0.0f, 1.0f, 0.0f},
    {"DcoPwmRate", 0, 0.0f, 1.0f, 0.5f},
    {"VcfCutoff", 0, 0.0f, 1.0f, 1.0f},
    {"VcfResonance", 0, 0.0f, 1.0f, 0.0f},
    {"VcfLfoMod", 0, 0.0f, 1.0f, 0.0f},
    {"VcfEnvMod", 0, 0.0f, 1.0f, 0.0f},
    {"VcfKeyFollow", 0, 0.0f, 1.0f, 0.0f},
    {"VcfAftertouch", 0, 0.0f, 1.0f, 0.0f},
    {"VcaLevel", 0, 0.0f, 1.0f, 1.0f},
    {"VcaAftertouch", 0, 0.0f, 1.0f, 0.0f},
    {"LfoRate", 0, 0.0f, 1.0f, 0.5f},
    {"LfoDelay", 0, 0.0f, 1.0f, 0.0f},
    {"EnvAttackTime", 0, 0.0f, 1.0f, 0.0f},
    {"EnvAttackLevel", 0, 0.0f, 1.0f, 1.0f},
    {"EnvBreakTime", 0, 0.0f, 1.0f, 0.0f},
    {"EnvBreakLevel", 0, 0.0f, 1.0f, 1.0f},
    {"EnvDecayTime", 0, 0.0f, 1.0f, 0.0f},
    {"EnvSustainLevel", 0, 0.0f, 1.0f, 1.0f},
    {"EnvReleaseTime", 0, 0.0f, 1.0f, 0.0f},
    {"EnvKeyFollow", 0, 0.0f, 1.0f, 0.0f},
    {"ChorusRate", 0, 0.0f, 1.0f, 0.5f},
    {"BenderRange", 1, 0.0f, 12.0f, 2.0f},
    {"MasterFineTune", 0, 0.0f, 1.0f, 0.5f},
    {"ModWheelVcfRange", 0, 0.0f, 1.0f, 0.0f},
    {"ModWheelDcoLfo", 0, 0.0f, 1.0f, 0.1f},
    {"ModWheelVcfLfo", 0, 0.0f, 1.0f, 0.0f},
    {"NumberOfVoices", 1, 0.0f, 11.0f, 5.0f},
    {"PortamentoMode", 1, 0.0f, 2.0f, 0.0f},
    {"PortamentoTime", 0, 0.0f, 1.0f, 0.0f},
    {"LfoWaveform", 1, 0.0f, 5.0f, 0.0f},
    {"LfoSync", 1, 0.0f, 1.0f, 0.0f},
    {"LfoTrigger", 1, 0.0f, 1.0f, 0.0f},
    {"LfoInvert", 1, 0.0f, 1.0f, 0.0f},
    {"mpsEnabled", 1, 0.0f, 1.0f, 0.0f},
    {"mpeEnabled", 1, 0.0f, 1.0f, 0.0f},
    {"sysexEnabled", 1, 0.0f, 1.0f, 0.0f},
    {"resonanceBoost", 1, 0.0f, 2.0f, 0.0f},
    {"resonanceBoostFrequency", 1, 0.0f, 1.0f, 0.0f},
    {"specialSawVolume", 0, 0.0f, 1.0f, 1.0f},
    {"specialPulseVolume", 0, 0.0f, 1.0f, 1.0f},
    {"specialPulseSync", 1, 0.0f, 1.0f, 0.0f},
    {"specialPulseTune", 1, -24.0f, 24.0f, 0.0f},
    {"specialPulseFineTune", 0, -1.0f, 1.0f, 0.0f},
    {"specialPulseDcoEnv", 0, -1.0f, 1.0f, 0.0f},
    {"specialPulseDcoLfo", 0, -1.0f, 1.0f, 0.0f},
    {"UnisonoVoices", 0, 0.0f, 1.0f, 0.0f},
    {"UnisonoDetune", 0, 0.0f, 1.0f, 0.2f},
    {"UnisonoSpread", 0, 0.0f, 1.0f, 1.0f},
    {"RoundRobin", 1, 0.0f, 1.0f, 0.0f},
    {"Octave", 1, -2.0f, 2.0f, 0.0f},
    {"NoDcOffset", 1, 0.0f, 1.0f, 0.0f},
    {"arprate", 0, 0.0f, 1.0f, 0.5f},
    {"arprange", 1, 0.0f, 3.0f, 1.0f},
    {"arpmode", 1, 0.0f, 5.0f, 0.0f},
    {"arpsyncmode", 1, 0.0f, 2.0f, 0.0f},
    {"reverbDryWet", 0, 0.0f, 1.0f, 0.0f},
    {"reverbSize", 0, 0.0f, 1.0f, 0.8f},
    {"reverbDelay", 0, 0.0f, 1.0f, 0.0f},
    {"reverbTone", 0, 0.0f, 1.0f, 0.3f},
    {"delayDryWet", 0, 0.0f, 1.0f, 0.0f},
    {"delayTime", 0, 0.0f, 1.0f, 0.3f},
    {"delaySync", 1, 0.0f, 1.0f, 1.0f},
    {"delaySpread", 0, 0.0f, 1.0f, 0.5f},
    {"delayTone", 0, 0.0f, 1.0f, 0.5f},
    {"delayFeedback", 0, 0.0f, 1.0f, 0.2f},
    {"eqBellFreq", 0, 0.0f, 1.0f, 0.5f},
    {"eqBellQ", 0, 0.0f, 1.0f, 0.5f},
    {"eqBellGain", 0, -24.0f, 24.0f, 0.0f},
    {"paramPortamentoTimeMode", 1, 0.0f, 1.0f, 0.0f},
    {"portamentoChordOriginalMode", 1, 0.0f, 1.0f, 0.0f},
};

enum {
    P_Volume, P_DcoEnvMode, P_VcfEnvMode, P_VcaEnvMode, P_DcoWavePulse, P_DcoWaveSaw, P_DcoWaveSub, P_DcoRange,
    P_DcoSubLevel, P_Noise, P_HpfCutoff, P_DcoChorusSwitch, P_DcoLfoMod, P_DcoEnvMod, P_DcoAtMod, P_DcoPwmDepth,
    P_DcoPwmRate, P_VcfCutoff, P_VcfResonance, P_VcfLfoMod, P_VcfEnvMod, P_VcfKeyFollow, P_VcfAftertouch, P_VcaLevel,
    P_VcaAftertouch, P_LfoRate, P_LfoDelay, P_EnvAttackTime, P_EnvAttackLevel, P_EnvBreakTime, P_EnvBreakLevel,
    P_EnvDecayTime, P_EnvSustainLevel, P_EnvReleaseTime, P_EnvKeyFollow, P_ChorusRate, P_BenderRange,
    P_MasterFineTune, P_ModWheelVcfRange, P_ModWheelDcoLfo, P_ModWheelVcfLfo, P_NumberOfVoices, P_PortamentoMode,
    P_PortamentoTime, P_LfoWaveform, P_LfoSync, P_LfoTrigger, P_LfoInvert, P_mpsEnabled, P_mpeEnabled,
    P_sysexEnabled, P_resonanceBoost, P_resonanceBoostFrequency, P_specialSawVolume, P_specialPulseVolume,
    P_specialPulseSync, P_specialPulseTune, P_specialPulseFineTune, P_specialPulseDcoEnv, P_specialPulseDcoLfo,
    P_UnisonoVoices, P_UnisonoDetune, P_UnisonoSpread, P_RoundRobin, P_Octave, P_NoDcOffset, P_arprate, P_arprange,
    P_arpmode, P_arpsyncmode, P_reverbDryWet, P_reverbSize, P_reverbDelay, P_reverbTone, P_delayDryWet, P_delayTime,
    P_delaySync, P_delaySpread, P_delayTone, P_delayFeedback, P_eqBellFreq, P_eqBellQ, P_eqBellGain,
    P_paramPortamentoTimeMode, P_portamentoChordOriginalMode, P_COUNT
};
typedef char p_count_ok[(P_COUNT == MKSULTRA_PARAMS) ? 1 : -1];

static float sanitize(int k, float v)
{
    v = fminr(fmaxr(v, PARAMS[k].min), PARAMS[k].max);
    return PARAMS[k].is_int ? roundf(v) : v;
}

static const double DCO_ENV_TABLE[128] = {
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.489, 0.489, 0.489, 0.489, 0.9878, 0.9878, 0.9878, 0.9878, 1.489, 1.489, 1.489, 1.489,
    1.9868, 1.9868, 1.9868, 1.9868, 2.4857, 2.4857, 2.4857, 2.4857, 2.9862, 2.9862, 2.9862, 2.9862, 3.4915, 3.4915,
    3.4915, 3.4915, 3.992, 3.992, 3.992, 3.992, 4.4919, 4.4919, 4.4919, 4.4919, 4.9868, 4.9868, 4.9868, 4.9868, 5.4897,
    5.4897, 5.4897, 5.4897, 5.9905, 5.9905, 5.9905, 5.9905, 6.49, 6.49, 6.49, 6.49, 6.9908, 6.9908, 6.9908, 6.9908, 7.493,
    7.493, 7.493, 7.9926, 7.9926, 8.4922, 8.4922, 8.9887, 8.9887, 9.4915, 9.4915, 9.9908, 9.9908, 10.4918, 10.4918,
    10.9909, 10.9909, 11.4918, 11.4918, 11.99, 11.99, 12.4918, 12.4918, 12.9989, 12.9989, 13.4918, 13.4918, 13.9895,
    13.9895, 14.4912, 14.4912, 14.9906, 14.9906, 15.4918, 15.9914, 16.4915, 16.9907, 17.4916, 17.9908, 18.4919, 18.9902,
    19.4931, 19.99, 20.4924, 20.9967, 21.4938, 21.9927, 22.4935, 22.9966, 23.4936, 23.9919, 24.4942, 24.9913, 25.4935,
    25.9907, 26.4925, 26.9977, 27.4931, 27.9968, 28.4939, 28.9962, 29.4932, 29.9963, 30.993, 31.4937, 31.4937};

static void prm_default(prm_t *p)
{
    memset(p, 0, sizeof *p);
    for (int k = 0; k < 128; k++) p->tuning[k] = (float)k;
    p->volume_target = 1.0f; p->saw_mode = 1; p->saw_volume = 1.0f; p->pulse_volume = 1.0f; p->bender_range = 2.0f;
    p->mw_dco_lfo = 0.1f; p->vcf_cutoff = 1.0f; p->hpf_mode = 1;
    p->env_t1 = 4.0; p->env_l1 = 1.0; p->env_t2 = 4.0; p->env_l2 = 1.0; p->env_t3 = 4.0; p->env_l3 = 1.0; p->env_t4 = 4.0;
    p->env_t1f = 4.0; p->env_t4f = 4.0;
    p->vca_level = 1.0f; p->vca_at = 1.0f; p->unison_detune = 0.1f; p->unison_spread = 1.0f;
    p->pwm_lfo_rate_cache = -1.0f; p->sample_rate = 44100.0f;
    for (int k = 0; k < 6; k++) p->chord[k] = -1;
}

static void prm_apply(prm_t *p, int id, float v)
{
#define B(x) ((x) >= 0.5f)
    switch (id) {
    case P_Volume: p->volume_target = v * v + v * v; break;
    case P_PortamentoMode: p->porta_mode = f2i(v); break;
    case P_PortamentoTime: p->porta_time = v; break;
    case P_paramPortamentoTimeMode: p->porta_time_mode = B(v); break;
    case P_portamentoChordOriginalMode: p->chord_orig_mono = B(v); break;
    case P_DcoWaveSaw: p->saw_mode = f2i(v); break;
    case P_DcoWavePulse: p->pulse_mode = f2i(v); break;
    case P_DcoPwmRate: p->pwm_rate = v > 1e-4f ? exp2f(10.2f * v + (-10.2f)) : v; break;
    case P_DcoPwmDepth: p->pwm_depth = v; break;
    case P_DcoWaveSub: p->sub_mode = f2i(v); break;
    case P_DcoSubLevel: p->sub_level = 0.33333331f * v; break;
    case P_Noise: p->noise = 0.33333331f * v; break;
    case P_DcoLfoMod: { float t = 0.8f * v + 0.2f; p->dco_lfo_mod = (4.0f * v) * t; break; }
    case P_Octave: p->octave = (float)f2i(v); break;
    case P_NoDcOffset: p->no_dc_offset = B(v); break;
    case P_DcoEnvMode: p->dco_env_mode = f2i(v); break;
    case P_DcoEnvMod: {
        int32_t idx = f2i(floorf(v * 127.0f + 0.5f));
        p->dco_env_mod = (float)DCO_ENV_TABLE[clampi(idx, 0, 127)];
        break;
    }
    case P_DcoAtMod: p->dco_at_mod = v; break;
    case P_specialPulseDcoEnv: { float s = 24.0f * v * v; p->sp_env = v > 0.0f ? s : -s; break; }
    case P_specialPulseDcoLfo: { float s = 24.0f * v * v; p->sp_lfo = v > 0.0f ? s : -s; break; }
    case P_specialPulseTune: p->sp_tune = v; break;
    case P_specialPulseFineTune: p->sp_fine = v; break;
    case P_specialSawVolume: p->saw_volume = v; break;
    case P_specialPulseVolume: p->pulse_volume = v; break;
    case P_specialPulseSync: p->pulse_sync = B(v); break;
    case P_DcoRange: {
        int32_t idx = f2i(v);
        p->dco_range = idx == 0 ? 12.0f : idx == 1 ? 0.0f : idx == 2 ? -12.0f : idx == 3 ? -24.0f : 0.0f;
        break;
    }
    case P_MasterFineTune: p->fine_tune = v + v + (-1.0f); break;
    case P_BenderRange: p->bender_range = v; break;
    case P_ModWheelDcoLfo: p->mw_dco_lfo = v; break;
    case P_VcfCutoff: p->vcf_cutoff = v; break;
    case P_VcfResonance: p->vcf_res = v; break;
    case P_VcfKeyFollow: p->vcf_kf = v; break;
    case P_resonanceBoost: p->res_boost = f2i(v); break;
    case P_resonanceBoostFrequency: p->bass_boost_freq = B(v); break;
    case P_VcfLfoMod: p->vcf_lfo = 0.27f * v; break;
    case P_VcfEnvMode: p->vcf_env_mode = f2i(v); break;
    case P_VcfEnvMod: p->vcf_env_mod = 1.04f * v; break;
    case P_ModWheelVcfRange: p->mw_vcf_range = v; break;
    case P_ModWheelVcfLfo: p->mw_vcf_lfo = 0.27f * v; break;
    case P_VcfAftertouch: p->vcf_at = 0.5f * v; break;
    case P_HpfCutoff: p->hpf_mode = f2i(v); break;
    case P_EnvAttackTime: p->env_t1 = calc_coef(v); p->env_t1f = calc_coef(v + 0.001f); break;
    case P_EnvAttackLevel: p->env_l1 = (double)v; break;
    case P_EnvBreakTime: p->env_t2 = calc_coef(v); break;
    case P_EnvBreakLevel: p->env_l2 = (double)v; break;
    case P_EnvDecayTime: p->env_t3 = calc_coef(v); break;
    case P_EnvSustainLevel: p->env_l3 = (double)v; break;
    case P_EnvReleaseTime: p->env_t4 = calc_coef(v); p->env_t4f = calc_coef(v + 0.001f); break;
    case P_EnvKeyFollow: p->env_kf = v; break;
    case P_VcaEnvMode: p->vca_env_mode = f2i(v); break;
    case P_VcaLevel: p->vca_level = (v * v) * 0.95f + 0.05f; break;
    case P_VcaAftertouch: p->vca_at = v; break;
    case P_ChorusRate: { float c = v * v * v; p->chorus_rate = (c * 39.9f) * (0.9f * c + 0.1f) + 0.1f; break; }
    case P_DcoChorusSwitch: p->chorus_on = B(v); break;
    case P_UnisonoVoices: {
        int32_t n = v > 0.001f ? f2i(floorf(4.0f * v + 3.5f)) : 1;
        p->unison_extra = n - 1;
        break;
    }
    case P_UnisonoDetune: p->unison_detune = v * v; break;
    case P_UnisonoSpread: p->unison_spread = v; break;
    case P_RoundRobin: p->round_robin = B(v); break;
    case P_arpmode: p->arp_active = floorf(v + 0.5f) != 0.0f; break;
    default: break;
    }
#undef B
}

/* ---------- voice.rs ---------- */
typedef struct { float cutoff, detune, attack, decay, release, pan_l, pan_r; } vtd_t;
static void vtd_default(vtd_t *t) { t->cutoff = 0.0f; t->detune = 0.0f; t->attack = 1.0f; t->decay = 1.0f; t->release = 1.0f; t->pan_l = 1.0f; t->pan_r = 1.0f; }
static float vt_cube(float r) { float f = (r + r) + (-1.0f); return ((f * f) * 0.5f) * f; }
static float vt_pan(float p) { return 1.3333334f - 1.3333334f * (p * p); }
static void vtd_from_raw(vtd_t *t, const mksultra_vtune_t *v)
{
    t->cutoff = vt_cube(v->cutoff);
    t->detune = vt_cube(v->detune);
    t->attack = vt_cube(v->attack) + 1.0f;
    t->decay = vt_cube(v->decay) + 1.0f;
    t->release = vt_cube(v->release) + 1.0f;
    t->pan_l = vt_pan(v->pan);
    t->pan_r = vt_pan(1.0f - v->pan);
}

typedef struct {
    int gate, hold;
    float sr;
    dco_t dco;
    dco_t unison[7];
    dsvf_t fa, fb;
    chain_t up_l, up_r, down8_l, down8_r, down4_l, down4_r;
    float porta_sr, porta_cur, porta_step;
    smoother_t cut_sm, press_sm, bend_sm;
    float key, vel, key_scale;
    int slot;
    tpt_t dcblock;
    int vca_snap;
    env_t env_m, env_f, env_g;
    int32_t trigger_key;
    float ekf;
    int32_t note;
    float pressure;
    int active_flag;
} voice_t;

static void voice_set_sr(voice_t *v, float sr, float mod_sum)
{
    v->sr = sr;
    dco_set_sr(&v->dco, sr, mod_sum);
    for (int i = 0; i < 7; i++) dco_set_sr(&v->unison[i], sr, mod_sum);
    sm_set_sr(&v->cut_sm, sr);
    sm_set_sr(&v->press_sm, sr);
    sm_set_sr(&v->bend_sm, sr);
    dsvf_set_sr(&v->fa, sr);
    dsvf_set_sr(&v->fb, sr);
    chain_clear(&v->up_l); chain_clear(&v->up_r); chain_clear(&v->down8_l);
    chain_clear(&v->down8_r); chain_clear(&v->down4_l); chain_clear(&v->down4_r);
    v->porta_sr = sr / 44100.0f;
    env_set_sr(&v->env_m, sr);
    env_set_sr(&v->env_f, sr);
    env_set_sr(&v->env_g, sr);
    tpt_set_sr(&v->dcblock, sr);
}

/* Zieht die Zufallswerte einer Stimme in der Reihenfolge von Voice::new. v == NULL: nur ziehen (Sparfassung). */
static void voice_new(voice_t *v, int index, seeds_t *seeds, const float *up, const float *d8, const float *d4)
{
    dco_t scratch;
    for (int i = 0; i < 7; i++) {
        dco_t *d = v ? &v->unison[i] : &scratch;
        dco_new(d, 1, seeds);
        float r = (float)((uint32_t)next_seed(seeds) & 0x7fffffffu) * 4.656613e-10f;
        d->s2.phase = r;
    }
    dco_new(v ? &v->dco : &scratch, 0, seeds);
    int32_t ks_seed = next_seed(seeds);
    int32_t sa = next_seed(seeds);
    int32_t sb = next_seed(seeds);
    if (!v) return;
    dsvf_new(&v->fa, sa);
    dsvf_new(&v->fb, sb);
    chain_new(&v->up_l, up, 4); chain_new(&v->up_r, up, 4);
    chain_new(&v->down8_l, d8, 8); chain_new(&v->down8_r, d8, 8);
    chain_new(&v->down4_l, d4, 4); chain_new(&v->down4_r, d4, 4);
    v->gate = 0; v->hold = 0; v->sr = 44100.0f;
    v->porta_sr = 1.0f; v->porta_cur = 42.0f; v->porta_step = 0.0f;
    sm_new(&v->cut_sm, 1000.0f);
    sm_new(&v->press_sm, 4.0f);
    sm_new(&v->bend_sm, 1000.0f);
    v->key = 0.0f; v->vel = 0.0f;
    v->key_scale = (float)ks_seed * 4.1909516e-14f + 0.999955f;
    v->slot = index % 6;
    tpt_new(&v->dcblock, 8.0f);
    v->vca_snap = 0;
    env_new(&v->env_m); env_new(&v->env_f); env_new(&v->env_g);
    v->trigger_key = 60; v->ekf = 0.0f; v->note = 60; v->pressure = 0.0f; v->active_flag = 0;
    voice_set_sr(v, 44100.0f, 0.0f);
}

static inline int voice_vca_finished(const voice_t *v, const prm_t *p)
{
    return (p->vca_env_mode | 2) == 3 ? env_finished(&v->env_g) : env_finished(&v->env_m);
}
static inline float cutoff_target(const prm_t *p, float press)
{
    return ((p->vcf_cutoff * 1.08f + (-0.0756f)) + p->mod_wheel * p->mw_vcf_range) + press * p->vcf_at;
}
static void voice_retrigger(voice_t *v, float vel)
{
    v->env_m.stage = 0; v->env_f.stage = 0; v->env_g.stage = 0; v->vel = vel;
}
static void voice_note_on(voice_t *v, int32_t note, int32_t slot, float vel, const prm_t *p)
{
    float pp = ((float)note * 0.083333336f + (-5.0f)) * p->env_kf + (-0.75f);
    v->ekf = fastpow2(pp) * 1.681855f;
    int fresh = voice_vca_finished(v, p);
    float press = v->pressure;
    if (fresh) {
        v->cut_sm.y = cutoff_target(p, press);
        float mod_sum = (p->sp_lfo + p->sp_fine) + (p->sp_env + p->sp_tune);
        dco_realign(&v->dco, mod_sum);
        for (int i = 0; i < 7; i++) dco_realign(&v->unison[i], mod_sum);
        v->vel = vel;
        v->bend_sm.y = p->bender_range * p->pitch_wheel;
        v->vca_snap = 1;
        env_reset(&v->env_m); env_reset(&v->env_f); env_reset(&v->env_g);
    }
    v->key = (note >= 0 && note < 128) ? p->tuning[note] : 60.0f;
    int held = v->gate || v->hold;
    switch (p->porta_mode) {
    case 0:
        voice_retrigger(v, vel);
        v->porta_cur = v->key;
        v->cut_sm.y = cutoff_target(p, press);
        break;
    case 1: case 2: {
        if (p->porta_mode == 1 && fresh) v->porta_cur = v->key;
        if (held) {
            if (p->arp_active) voice_retrigger(v, vel);
        } else {
            if (p->porta_mode == 1) v->porta_cur = v->key;
            voice_retrigger(v, vel);
        }
        float scale = p->porta_time_mode ? fabsf(v->porta_cur - v->key) * 0.05f : 1.0f;
        float u = p->porta_time * (-0.1f) + 0.11f;
        u = u * u;
        v->porta_step = ((((u * u) * 10000.0f) * u + 1e-4f) * scale) / v->porta_sr;
        break;
    }
    default: break;
    }
    v->gate = 1;
    v->note = note;
    v->trigger_key = slot;
    v->active_flag = 1;
}
static void voice_release(voice_t *v) { v->gate = 0; v->active_flag = 0; }

static int voice_process(voice_t *v, const prm_t *p, float lfo_value, const vtd_t *vt, const float *tab, float *l, float *r)
{
    if (env_finished(&v->env_m)) return 0;
    int alive = (p->vca_env_mode == 1 || p->vca_env_mode == 3) ? !env_finished(&v->env_g) : 1;
    int gate = v->gate || v->hold;
    float env_f = env_due(&v->env_f, gate)
        ? env_tick(&v->env_f, (double)vt->attack * p->env_t1f, p->env_l1, p->env_t2, p->env_l2,
                   (double)vt->decay * p->env_t3, p->env_l3, (double)vt->release * p->env_t4f, (double)v->ekf)
        : env_hold(&v->env_f);
    float env_m = env_due(&v->env_m, gate)
        ? env_tick(&v->env_m, p->env_t1 * (double)vt->attack, p->env_l1, p->env_t2, p->env_l2,
                   p->env_t3 * (double)vt->decay, p->env_l3, p->env_t4 * (double)vt->release, (double)v->ekf)
        : env_hold(&v->env_m);
    float env_g = env_due(&v->env_g, gate) ? env_tick(&v->env_g, 4.0, 1.0, 1.0, 1.0, 1.0, 1.0, 4.0, 1.0) : env_hold(&v->env_g);

    /* Tonhöhe */
    float target = v->key;
    float g;
    if (p->porta_time <= 1e-4f) g = target;
    else if (p->porta_mode <= 0) { v->porta_cur = target; g = target; }
    else {
        if (v->porta_cur < target) { v->porta_cur += v->porta_step; if (v->porta_cur > target) v->porta_cur = target; }
        if (v->porta_cur > target) { v->porta_cur -= v->porta_step; if (v->porta_cur < target) v->porta_cur = target; }
        g = v->porta_cur;
    }
    float key_pitch = (p->octave * 12.0f + g * v->key_scale) + p->fine_tune;
    float bs = sm_tick(&v->bend_sm, p->bender_range * p->pitch_wheel);
    float press = v->pressure;
    float lfo = lfo_value;
    float depth = press * (p->dco_at_mod * 2.4f) + (p->dco_lfo_mod + p->mw_dco_lfo * (p->mod_wheel * 4.0f));
    float pitch = (((bs + key_pitch) + vt->detune) + p->dco_range) + depth * lfo;
    float e = p->dco_env_mod * env_m;
    switch (p->dco_env_mode) {
    case 0: pitch += e; break;
    case 1: pitch -= e; break;
    case 2: pitch += e * v->vel; break;
    case 3: pitch -= e * v->vel; break;
    default: break;
    }

    /* Oszillatoren bei 2× */
    float s1 = 0.0f, s2 = 0.0f;
    dco_process(&v->dco, &s1, pitch, env_m, lfo, p, tab);
    dco_process(&v->dco, &s2, pitch, env_m, lfo, p, tab);
    int32_t n_uni = p->unison_extra;
    int stereo = n_uni >= 2;
    float l1 = s1, l2 = s2, r1 = s1, r2 = s2;
    if (stereo) {
        float sp = p->unison_spread, dt = p->unison_detune;
        float k = 1.0f - sp * sp;
        l1 = k * s1; l2 = k * s2; r1 = k * s1; r2 = k * s2;
        float sgn = 1.0f, off = 100.0f;
        int nu = n_uni < 7 ? (int)n_uni : 7;
        for (int i = 0; i < nu; i++) {
            off = (sgn > 0.0f ? ((float)i + 1.0f) * dt : off) * sgn;
            float u1 = 0.0f, u2 = 0.0f;
            dco_t *d = &v->unison[i];
            dco_process(d, &u1, pitch + off, env_m, lfo, p, tab);
            dco_process(d, &u2, pitch + off, env_m, lfo, p, tab);
            float gl, gr;
            if (sgn < 0.0f) { gl = 1.0f + 0.5f * sp; gr = 1.0f - sp; }
            else { gl = 1.0f - sp; gr = 1.0f + 0.5f * sp; }
            l1 += u1 * gl; l2 += u1 * gl; r1 += u1 * gr; r2 += u1 * gr;
            sgn = -sgn;
        }
        l1 *= 0.5f; l2 *= 0.5f; r1 *= 0.5f; r2 *= 0.5f;
    }

    /* Cutoff */
    float ct = (p->vcf_cutoff * 1.08f + (-0.0756f)) + p->mod_wheel * p->mw_vcf_range;
    float cs = ((v->cut_sm.y * v->cut_sm.a + ct) + press * p->vcf_at) * v->cut_sm.b;
    v->cut_sm.y = cs;
    float envc;
    switch (p->vcf_env_mode) {
    case 0: envc = p->vcf_env_mod * env_f; break;
    case 1: envc = (-env_f) * p->vcf_env_mod; break;
    case 2: envc = (p->vcf_env_mod * env_f) * v->vel; break;
    case 3: envc = (p->vcf_env_mod * 0.5f) * v->vel; break;
    default: envc = 0.0f; break;
    }
    float c = (vt->cutoff + cs) + (((p->mw_vcf_lfo * p->mod_wheel + p->vcf_lfo) * lfo +
                                    (p->vcf_kf * (key_pitch + (-60.0f))) * 0.007874016f) + envc);

    /* Resonanz und Pegel */
    float res = p->vcf_res, rp, gain_in, drive;
    switch (p->res_boost) {
    case 0: {
        float m = 0.25f < c ? c * (-0.1f) + 1.025f : 1.0f;
        float t = m * res;
        rp = ((t * 0.32f + 0.68f) + ((1.0f - t) * (1.0f - t)) * (-0.68f)) * 0.2725968f;
        gain_in = (1.0f - fminr(res, 0.85f)) * 0.5f;
        drive = 0.5f;
        break;
    }
    case 1:
        rp = ((0.32f * res + 0.68f) + ((1.0f - res) * (1.0f - res)) * (-0.68f)) * 0.32452002f;
        gain_in = (1.0f - fminr(res, 0.8f)) * 1.0f;
        drive = 1.0f;
        break;
    case 2:
        rp = ((0.32f * res + 0.68f) + ((1.0f - res) * (1.0f - res)) * (-0.68f)) * 0.36863998f;
        gain_in = (1.0f - fminr(res, 0.8f)) * 4.0f;
        drive = 4.0f;
        break;
    default: rp = 0.0f; gain_in = 1.0f; drive = 1.0f; break;
    }
    dsvf_set_params(&v->fa, c, rp + 0.8f, p->res_boost == 0);
#if MKSULTRA_VCF2X
    {   /* Filter bei 2× statt 4×: g gilt bei 4× (Grenzfrequenz 4·sr/π·atan g); bei 2× dieselbe Frequenz:
         * tan(2·atan g) = 2g/(1 − g²), begrenzt, wo sie über der halben 2×-Rate läge */
        float g = v->fa.g;
        v->fa.g = g < 0.99f ? (g + g) / (1.0f - g * g) : 99.0f;
    }
#endif
    v->fb.g = v->fa.g; v->fb.r = v->fa.r; v->fb.vintage = v->fa.vintage;

    /* Filter bei 4× */
    float post = drive * 0.1f;
    float xl, xr;
    if (!stereo) {
        float d0 = l1 * gain_in, d1 = l2 * gain_in;
#if MKSULTRA_VCF2X
        float y0 = dsvf_step(&v->fb, 0, dsvf_step(&v->fa, 0, d0 * 0.1f));
        float y1 = dsvf_step(&v->fb, 0, dsvf_step(&v->fa, 0, d1 * 0.1f));
        float a, b;
        chain_down4_paths(&v->down4_l, y0, y1, &a, &b);
#else
        float o[4];
        chain_up4(&v->up_l, d0, &o[0], &o[1]);
        chain_up4(&v->up_l, d1, &o[2], &o[3]);
        for (int i = 0; i < 4; i++) {
            float x = o[i] * 0.1f;
            float a = dsvf_step(&v->fa, 0, x);
            o[i] = dsvf_step(&v->fb, 0, a);
        }
        float h0 = chain_down8(&v->down8_l, o[0], o[1]);
        float h1 = chain_down8(&v->down8_l, o[2], o[3]);
        float a, b;
        chain_down4_paths(&v->down4_l, h0, h1, &a, &b);
#endif
        xl = ((a + b) * 0.5f) / post;
        xl = tpt_hp(&v->dcblock, 0, xl);
        xr = xl;
    } else {
        float inv = 1.0f / post;
        float outv[2];
        for (int ch = 0; ch < 2; ch++) {
            chain_t *up = ch == 0 ? &v->up_l : &v->up_r;
            chain_t *d8 = ch == 0 ? &v->down8_l : &v->down8_r;
            chain_t *d4 = ch == 0 ? &v->down4_l : &v->down4_r;
            float sa = ch == 0 ? l1 : r1, sb = ch == 0 ? l2 : r2;
#if MKSULTRA_VCF2X
            (void)up; (void)d8;
            float y0 = dsvf_step(&v->fb, 1 + ch, dsvf_step(&v->fa, 1 + ch, sa * gain_in * 0.1f));
            float y1 = dsvf_step(&v->fb, 1 + ch, dsvf_step(&v->fa, 1 + ch, sb * gain_in * 0.1f));
            float a, b;
            chain_down4_paths(d4, y0, y1, &a, &b);
#else
            float o[4];
            chain_up4(up, sa * gain_in, &o[0], &o[1]);
            chain_up4(up, sb * gain_in, &o[2], &o[3]);
            for (int i = 0; i < 4; i++) {
                float x = o[i] * 0.1f;
                float a = dsvf_step(&v->fa, 1 + ch, x);
                o[i] = dsvf_step(&v->fb, 1 + ch, a);
            }
            float h0 = chain_down8(d8, o[0], o[1]);
            float h1 = chain_down8(d8, o[2], o[3]);
            float a, b;
            chain_down4_paths(d4, h0, h1, &a, &b);
#endif
            outv[ch] = ((a + b) * 0.5f) * inv;
        }
        xl = tpt_hp(&v->dcblock, 0, outv[0]);
        xr = tpt_hp(&v->dcblock, 1, outv[1]);
    }

    /* VCA */
    int32_t mode = p->vca_env_mode;
    if (mode == 0 || mode == 2) { float gv = env_m * p->vca_level; xl *= gv; xr *= gv; }
    else if (mode == 1 || mode == 3) { float gv = env_g * p->vca_level; xl *= gv; xr *= gv; }
    float dynf = (mode & ~1) == 2 ? v->vel * 0.9f + 0.1f : 1.0f;
    float tgt = p->vca_at * press + dynf;
    if (v->vca_snap) { v->vca_snap = 0; v->press_sm.y = tgt; }
    float y = sm_tick(&v->press_sm, tgt);
    if (alive) {
        float gg = y * 0.25f;
        *l += (gg * xl) * vt->pan_l;
        *r += (xr * gg) * vt->pan_r;
    }
    return 1;
}

/* ---------- engine.rs ---------- */
typedef struct {
    tpt_t lp2k, lp28, hp5;
    smoother_t sm;
    int32_t cnt, n;
    float peak, amp, sr;
} dcemu_t;
static void dcemu_set_sr(dcemu_t *d, float sr)
{
    d->sr = sr;
    tpt_set_sr(&d->lp2k, sr); tpt_set_sr(&d->lp28, sr); tpt_set_sr(&d->hp5, sr);
    sm_set_sr(&d->sm, sr);
    d->n = f2i(1.0f) * f2i(0.0016666667f * sr);
}
static void dcemu_new(dcemu_t *d)
{
    tpt_new(&d->lp2k, 2000.0f); tpt_new(&d->lp28, 28.0f); tpt_new(&d->hp5, 5.0f);
    sm_new(&d->sm, 10.0f);
    d->cnt = 0; d->n = 73; d->peak = 0.0f; d->amp = 0.0f; d->sr = 44100.0f;
    dcemu_set_sr(d, 44100.0f);
}
static inline void dcemu_process(dcemu_t *d, float *l, float *r)
{
    float m = (*r + *l) * 0.5f;
    float lp = tpt_lp(&d->lp2k, 0, m);
    int32_t c = d->cnt;
    d->cnt = (int32_t)((uint32_t)c + 1u);
    float env = ((-2.4f / d->sr) + 1.0f) * d->peak;
    d->peak = env;
    if (2 * d->n <= c) (void)sm_tick(&d->sm, env);
    if (c + 1 < 20 * d->n) {
        if (env < fabsf(lp)) d->peak = fabsf(lp);
        if (d->amp < 1.0f) d->amp = 1.0f;
    }
    float gg = d->amp * d->sm.y;
    float q = 0.6f * gg;
    float lo = tpt_lp(&d->lp28, 0, q);
    float add = gg * (-0.6f) + (q - lo);
    *l += add;
    *r = add + *r;
    *l = tpt_hp(&d->hp5, 0, *l);
    *r = tpt_hp(&d->hp5, 1, *r);
}

#define RECENCY_MAX 256

typedef struct {
    prm_t prm;
    float sr;
    float blep[BLEP_LEN];
    voice_t voices[NV];
    vtd_t vtunings[6];
    int num_voices;
    int recency[RECENCY_MAX];
    int nrec;
    int32_t held[128];
    int nheld;
    int rr;
    int hold;
    lfo_t lfo;
    tpt_t hpf;
    svf_t shelf[2], bell80[2], subsonic[2], eq[2];
    float eq_gain;
    dcemu_t dcemu;
    chorus_t chorus_l, chorus_r;
    delay_t delay;
    reverb_t reverb;
    smoother_t vol;
    float fade, fade_step;
    arp_t arp;
    seeds_t seeds;
    float values[MKSULTRA_PARAMS];
    float bpm;
    int32_t quiet; /* Samples seit dem letzten hörbaren Ausgang (nur für active, nicht Teil der Vorlage) */
    float chorus_mem[2][CHORUS_CAP];
    fxs_t delay_mem[2][DELAY_CAP];
    fxs_t reverb_mem[REVERB_CAP];
} synth_t;

static void synth_set_sr(synth_t *s, float sr);

static void apply_param(synth_t *s, int id, float v)
{
    prm_apply(&s->prm, id, v);
    switch (id) {
    case P_NumberOfVoices: {
        int n = clampi(f2i(v) + 1, 1, 12);
        if (n > NV) n = NV;
        if (n != s->num_voices) {
            s->num_voices = n;
            for (int i = 0; i < NV; i++)
                if (!(n == 1 && i == 0)) { s->voices[i].gate = 0; s->voices[i].hold = 0; s->voices[i].active_flag = 0; }
        }
        break;
    }
    case P_LfoRate: lfo_set_rate(&s->lfo, v); break;
    case P_LfoSync: lfo_set_sync(&s->lfo, v >= 0.5f); break;
    case P_LfoDelay: lfo_set_delay(&s->lfo, v); break;
    case P_LfoTrigger: s->lfo.trigger = v >= 0.5f; break;
    case P_LfoInvert: s->lfo.inv = v >= 0.5f ? -1.0f : 1.0f; break;
    case P_LfoWaveform: s->lfo.wave = f2i(v); break;
    case P_eqBellFreq: {
        float v2 = v * v;
        float f = v2 * v2 * 19980.0f + 20.0f;
        for (int k = 0; k < 2; k++) { s->eq[k].f = f; svf_update(&s->eq[k]); }
        break;
    }
    case P_eqBellQ: {
        float q = expf(7.37776f * v) * 0.025f + (-1.6494e-18f);
        for (int k = 0; k < 2; k++) { s->eq[k].q = q; svf_update(&s->eq[k]); }
        break;
    }
    case P_eqBellGain: {
        s->eq_gain = v;
        float a = (float)pow(10.0, (double)v * 0.025);
        for (int k = 0; k < 2; k++) {
            s->eq[k].a_gain = a;
            s->eq[k].sqrt_a = fsqrt(a);
            s->eq[k].enabled = v != 0.0f;
            svf_update(&s->eq[k]);
        }
        break;
    }
    case P_delayDryWet: delay_set_drywet(&s->delay, v); break;
    case P_delayTime: delay_set_time(&s->delay, v); break;
    case P_delaySync: delay_set_sync(&s->delay, v > 0.5f); break;
    case P_delaySpread: delay_set_spread(&s->delay, v); break;
    case P_delayTone: delay_set_tone(&s->delay, v); break;
    case P_delayFeedback: delay_set_feedback(&s->delay, v); break;
    case P_reverbDryWet: reverb_set_drywet(&s->reverb, v); break;
    case P_reverbSize: reverb_set_size(&s->reverb, v); break;
    case P_reverbDelay: reverb_set_delay(&s->reverb, v); break;
    case P_reverbTone: reverb_set_tone(&s->reverb, v); break;
    case P_arpmode: arp_set_mode(&s->arp, f2i(floorf(v + 0.5f))); break;
    case P_arprange: arp_set_range(&s->arp, f2i(floorf(v + 0.5f))); break;
    case P_arprate: arp_set_rate_param(&s->arp, v); break;
    case P_arpsyncmode: arp_set_sync_mode(&s->arp, f2i(floorf(v + 0.5f))); break;
    default: break;
    }
}

static void synth_new(synth_t *s, float sample_rate)
{
    memset(s, 0, sizeof *s);
    seeds_init(&s->seeds, 0x5eed1234u);
    float up[4], d8[8], d4[4];
    hiir_design(4, 0.009999999776482582, up);
    hiir_design(8, 0.009999999776482582, d8);
    hiir_design(4, 0.2549999998882413, d4);
    for (int i = 0; i < MAX_VOICES; i++) voice_new(i < NV ? &s->voices[i] : NULL, i, &s->seeds, up, d8, d4);
    int32_t lfo_seed = next_seed(&s->seeds);
    float a = (float)pow(10.0, 0.15);
    for (int k = 0; k < 2; k++) {
        svf_new(&s->shelf[k], 400.0f, 0.6f, SVF_LSHELF); svf_set_gain(&s->shelf[k], a);
        svf_new(&s->bell80[k], 80.0f, 0.76f, SVF_BELL); svf_set_gain(&s->bell80[k], a);
        svf_new(&s->subsonic[k], 0.8f, 1.0f / 0.70710677f, SVF_HP);
        svf_new(&s->eq[k], 1000.0f, 1.41421f, SVF_BELL);
    }
    reverb_new(&s->reverb, s->reverb_mem, 44100.0f, &s->seeds);
    prm_default(&s->prm);
    s->sr = 44100.0f;
    build_blep_table(s->blep);
    for (int k = 0; k < 6; k++) vtd_default(&s->vtunings[k]);
    s->num_voices = 6 < NV ? 6 : NV;
    lfo_new(&s->lfo, lfo_seed);
    tpt_new(&s->hpf, 0.0f);
    s->eq_gain = 0.0f;
    dcemu_new(&s->dcemu);
    chorus_new(&s->chorus_l, s->chorus_mem[0], 44100.0f, 1.0f);
    chorus_new(&s->chorus_r, s->chorus_mem[1], 44100.0f, 0.5f);
    delay_new(&s->delay, s->delay_mem[0], s->delay_mem[1], 44100.0f);
    sm_new(&s->vol, 1000.0f);
    s->fade = 0.0f;
    s->fade_step = 1.0f / (0.1f * 44100.0f);
    arp_new(&s->arp, ARP_SEED);
    for (int k = 0; k < MKSULTRA_PARAMS; k++) s->values[k] = PARAMS[k].def;
    s->bpm = 120.0f;
    s->prm.pwm_lfo_phase = (float)((uint32_t)next_seed(&s->seeds) & 0x7fffffffu) * 4.656613e-10f;
    for (int k = 0; k < MKSULTRA_PARAMS; k++) apply_param(s, k, s->values[k]);
    synth_set_sr(s, sample_rate);
}

static void synth_set_sr(synth_t *s, float sr)
{
    sr = sr > 0.0f ? sr : 44100.0f;
    if (sr > (float)MKSULTRA_MAX_SR) sr = (float)MKSULTRA_MAX_SR;
    s->sr = sr;
    s->prm.sample_rate = sr;
    for (int k = 0; k < 2; k++) {
        svf_set_sr(&s->shelf[k], sr); svf_set_sr(&s->bell80[k], sr);
        svf_set_sr(&s->subsonic[k], sr); svf_set_sr(&s->eq[k], sr);
    }
    lfo_set_sr(&s->lfo, sr);
    float mod_sum = (s->prm.sp_lfo + s->prm.sp_fine) + (s->prm.sp_env + s->prm.sp_tune);
    for (int i = 0; i < NV; i++) voice_set_sr(&s->voices[i], sr, mod_sum);
    sm_set_sr(&s->vol, sr);
    s->fade = 0.0f;
    s->fade_step = 1.0f / (0.1f * sr);
    chorus_new(&s->chorus_l, s->chorus_mem[0], sr, 1.0f);
    chorus_new(&s->chorus_r, s->chorus_mem[1], sr, 0.5f);
    delay_set_sr(&s->delay, sr);
    reverb_initialize(&s->reverb, sr, &s->seeds);
    dcemu_set_sr(&s->dcemu, sr);
    s->hpf.sr = sr;
    s->hpf.fc = 0.0f;
    s->arp.sr = sr;
}

/* ---------- Stimmenverwaltung ---------- */
static void rec_insert0(synth_t *s, int v)
{
    if (s->nrec >= RECENCY_MAX) s->nrec = RECENCY_MAX - 1; /* nur in Extremfällen: älteste vergessen */
    memmove(&s->recency[1], &s->recency[0], sizeof(int) * (size_t)s->nrec);
    s->recency[0] = v;
    s->nrec++;
}
static int rec_remove(synth_t *s, int pos)
{
    int v = s->recency[pos];
    memmove(&s->recency[pos], &s->recency[pos + 1], sizeof(int) * (size_t)(s->nrec - pos - 1));
    s->nrec--;
    return v;
}
static int any_held(const synth_t *s)
{
    for (int i = 0; i < NV; i++) if (s->voices[i].gate || s->voices[i].hold) return 1;
    return 0;
}
static int get_new_voice(synth_t *s, int32_t note)
{
    int nv = s->num_voices < NV ? s->num_voices : NV;
    if (s->nrec == 0) { rec_insert0(s, 0); return 0; }
    for (int k = 0; k < s->nrec; k++)
        if (s->voices[s->recency[k]].note == note) { int v = rec_remove(s, k); rec_insert0(s, v); return v; }
    for (int i = 0; i < nv; i++)
        if (s->voices[i].note == note) { rec_insert0(s, i); return i; }
    for (int i = 0; i < nv; i++)
        if (voice_vca_finished(&s->voices[i], &s->prm)) { rec_insert0(s, i); return i; }
    for (int k = s->nrec - 1; k >= 0; k--)
        if (!s->voices[s->recency[k]].gate) { int v = rec_remove(s, k); rec_insert0(s, v); return v; }
    int v = s->nrec > 0 ? s->recency[--s->nrec] : 0;
    rec_insert0(s, v);
    return v;
}
static void prune_recency(synth_t *s)
{
    int k2 = 0;
    for (int k = 0; k < s->nrec; k++)
        if (!voice_vca_finished(&s->voices[s->recency[k]], &s->prm)) s->recency[k2++] = s->recency[k];
    s->nrec = k2;
}
static void held_push_front(synth_t *s, int32_t note)
{
    int k2 = 0;
    for (int k = 0; k < s->nheld; k++) if (s->held[k] != note) s->held[k2++] = s->held[k];
    s->nheld = k2;
    if (s->nheld >= 128) s->nheld = 127;
    memmove(&s->held[1], &s->held[0], sizeof(int32_t) * (size_t)s->nheld);
    s->held[0] = note;
    s->nheld++;
}
static void held_remove(synth_t *s, int32_t note)
{
    int k2 = 0;
    for (int k = 0; k < s->nheld; k++) if (s->held[k] != note) s->held[k2++] = s->held[k];
    s->nheld = k2;
}
static int next_rr(synth_t *s)
{
    s->rr = (s->rr + 1) % s->num_voices;
    rec_insert0(s, s->rr);
    return s->rr;
}

static int vm_note_on5(synth_t *s, int32_t note, int32_t slot, float vel, int mono)
{
    lfo_note_on(&s->lfo, any_held(s));
    int chord_off = s->prm.chord[0] < 0;
    int played = -1;
    if (chord_off) {
        if (mono) {
            held_push_front(s, note);
            voice_note_on(&s->voices[0], note, slot, vel, &s->prm);
            played = 0;
        } else {
            int v = s->prm.round_robin ? next_rr(s) : get_new_voice(s, note);
            voice_note_on(&s->voices[v], note, slot, vel, &s->prm);
            played = v;
            prune_recency(s);
        }
    } else if (!s->prm.chord_orig_mono) {
        for (int k = 0; k < 6; k++) {
            int32_t c = s->prm.chord[k];
            if (c < 0) continue;
            int32_t n = clampi(c + note - 60, 0, 127);
            int v = s->prm.round_robin ? next_rr(s) : get_new_voice(s, n);
            voice_note_on(&s->voices[v], n, slot, vel, &s->prm);
            played = v;
        }
        prune_recency(s);
    } else {
        held_push_front(s, note);
        int vi = 0;
        for (int k = 0; k < 6; k++) {
            int32_t c = s->prm.chord[k];
            if (c < 0) continue;
            int32_t n = clampi(c + note - 60, 0, 127);
            if (vi < NV) voice_note_on(&s->voices[vi], n, note, vel, &s->prm);
            vi++;
        }
        prune_recency(s);
    }
    return played;
}
static void vm_reset_all(synth_t *s)
{
    int start = s->num_voices == 1 ? 1 : 0;
    for (int i = start; i < NV; i++) { s->voices[i].gate = 0; s->voices[i].hold = 0; s->voices[i].active_flag = 0; }
    s->nheld = 0;
}
static void arp_step(synth_t *s)
{
    noteinfo_t e;
    if (arp_tick(&s->arp, &e) != 2) return;
    vm_reset_all(s);
    int played = vm_note_on5(s, clampi(e.note, 0, 127), e.orig, e.vel, 1);
    int lv = s->arp.last_voice;
    if (played >= 0 && lv >= 0 && played != lv) s->voices[played].cut_sm.y = s->voices[lv].cut_sm.y;
    s->arp.last_voice = played;
}
static void vm_note_off(synth_t *s, int32_t note)
{
    int mono = s->num_voices == 1;
    int chord_off = s->prm.chord[0] < 0;
    if (!chord_off && s->prm.chord_orig_mono) {
        held_remove(s, note);
        if (s->nheld == 0) {
            for (int i = 0; i < NV; i++) voice_release(&s->voices[i]);
        } else {
            int32_t top = s->held[0];
            int vi = 0;
            for (int k = 0; k < 6; k++) {
                int32_t c = s->prm.chord[k];
                if (c < 0) continue;
                int32_t n = clampi(c + top - 60, 0, 127);
                if (vi < NV) {
                    float vel = s->voices[vi].vel;
                    if (s->voices[vi].note != n) voice_note_on(&s->voices[vi], n, top, vel, &s->prm);
                }
                vi++;
            }
        }
        return;
    }
    if (mono) {
        held_remove(s, note);
        if (s->nheld == 0) voice_release(&s->voices[0]);
        else {
            int32_t top = s->held[0];
            if (s->voices[0].note != top) voice_note_on(&s->voices[0], top, top, s->voices[0].vel, &s->prm);
        }
    } else {
        for (int i = 0; i < NV; i++) if (s->voices[i].trigger_key == note) voice_release(&s->voices[i]);
    }
}

static inline float flush_sub(float x) { return fpclassify(x) == FP_SUBNORMAL ? 0.0f : x; }

/* Stimmen eines Samples (Arpeggiator, LFO-Wert, alle Stimmen), danach LFO und PWM-LFO fürs nächste Sample. */
static void synth_voices(synth_t *s, float *ol, float *or_)
{
    arp_step(s);
    float l = 0.0f, r = 0.0f;
    float lfo_val = lfo_value(&s->lfo);
    for (int k = 0; k < NV; k++) {
        voice_t *v = &s->voices[k];
        (void)voice_process(v, &s->prm, lfo_val, &s->vtunings[v->slot], s->blep, &l, &r);
    }
    lfo_tick(&s->lfo);
    float q = s->prm.pwm_rate * 20.0f;
    if (q != s->prm.pwm_lfo_rate_cache) { s->prm.pwm_lfo_rate_cache = q; s->prm.pwm_lfo_inc = q / s->sr; }
    float ph = s->prm.pwm_lfo_phase + s->prm.pwm_lfo_inc;
    if (!(ph < 1.0f)) ph -= 1.0f;
    s->prm.pwm_lfo_phase = ph;
    s->prm.pwm_lfo_out = pwm_lfo_shape(ph);
    *ol = l;
    *or_ = r;
}
/* Master-Kette eines Samples (hängt nicht von LFO und Stimmenzustand ab, nur von der Summe der Stimmen). */
static void synth_master(synth_t *s, float *ol, float *or_)
{
    float l = *ol, r = *or_;
    switch (s->prm.hpf_mode) {
    case 0:
        if (!s->prm.bass_boost_freq) { l = svf_tick(&s->shelf[0], l); r = svf_tick(&s->shelf[1], r); }
        else { l = svf_tick(&s->bell80[0], l); r = svf_tick(&s->bell80[1], r); }
        break;
    case 1: case 2: case 3: {
        float fc, pfc;
        if (s->prm.hpf_mode == 1) { fc = 3.0f; pfc = 9.424778f; }
        else if (s->prm.hpf_mode == 2) { fc = 200.0f; pfc = 628.31854f; }
        else { fc = 800.0f; pfc = 2513.2742f; }
        if (s->hpf.fc != fc) tpt_set_engine_hpf(&s->hpf, fc, pfc);
        l = tpt_hp(&s->hpf, 0, l);
        r = tpt_hp(&s->hpf, 1, r);
        break;
    }
    default: break;
    }
    if (!s->prm.no_dc_offset) dcemu_process(&s->dcemu, &l, &r);
    l = svf_tick(&s->subsonic[0], l);
    r = svf_tick(&s->subsonic[1], r);
    if (s->fade < 1.0f) { s->fade += s->fade_step; if (s->fade > 1.0f) s->fade = 1.0f; }
    float fd = s->fade;
    l *= fd + fd;
    r *= fd + fd;
    if (s->prm.chorus_on) {
        float rate = s->prm.chorus_rate;
        chorus_set_rate(&s->chorus_l, rate);
        chorus_set_rate(&s->chorus_r, rate);
        float wl = chorus_process(&s->chorus_l, l);
        float wr = chorus_process(&s->chorus_r, r);
        l = wl + l;
        r = wr + r;
    }
    if (s->eq[0].enabled && s->eq_gain != 0.0f) { l = svf_tick(&s->eq[0], l); r = svf_tick(&s->eq[1], r); }
    delay_process(&s->delay, &l, &r);
    reverb_process(&s->reverb, &l, &r);
    float vs = sm_tick(&s->vol, s->prm.volume_target);
    float gv = vs * fd;
    l *= gv;
    r *= gv;
    *ol = flush_sub(l);
    *or_ = flush_sub(r);
    if (fabsf(l) + fabsf(r) > 1e-6f) s->quiet = 0;
    else if (s->quiet < INT32_MAX) s->quiet++;
}
#if !MKSULTRA_BLOCKWISE
static void synth_tick(synth_t *s, float *ol, float *or_)
{
    synth_voices(s, ol, or_);
    synth_master(s, ol, or_);
}
#endif

/* ---------- öffentliche Schnittstelle ---------- */
void mksultra_init(void *st, float sample_rate) { synth_new((synth_t *)st, sample_rate); }

void mksultra_set_param(void *st, int id, float v)
{
    synth_t *s = (synth_t *)st;
    if (id < 0 || id >= MKSULTRA_PARAMS) return;
    v = sanitize(id, v);
    s->values[id] = v;
    apply_param(s, id, v);
}
float mksultra_get_param(const void *st, int id) { return (id >= 0 && id < MKSULTRA_PARAMS) ? ((const synth_t *)st)->values[id] : 0.0f; }
const char *mksultra_param_id(int id) { return (id >= 0 && id < MKSULTRA_PARAMS) ? PARAMS[id].id : ""; }
void mksultra_param_range(int id, float *mn, float *mx, float *df, int *is_int)
{
    if (id < 0 || id >= MKSULTRA_PARAMS) return;
    if (mn) *mn = PARAMS[id].min;
    if (mx) *mx = PARAMS[id].max;
    if (df) *df = PARAMS[id].def;
    if (is_int) *is_int = PARAMS[id].is_int;
}
int mksultra_param_find(const char *id)
{
    for (int k = 0; k < MKSULTRA_PARAMS; k++) if (strcmp(PARAMS[k].id, id) == 0) return k;
    return -1;
}

void mksultra_load_preset(void *st, const mksultra_preset_t *p)
{
    synth_t *s = (synth_t *)st;
    float version = p->version > 0 ? (float)p->version : 1.0f;
    for (int k = 0; k < MKSULTRA_PARAMS; k++) {
        float x = (p->have[k / 32] >> (k % 32)) & 1u ? p->v[k] : PARAMS[k].def;
        if (version < 8.0f && k == P_resonanceBoost) x += 1.0f;
        x = sanitize(k, x);
        s->values[k] = x;
        apply_param(s, k, x);
    }
    for (int k = 0; k < 128; k++) s->prm.tuning[k] = k < p->ntuning ? p->tuning[k] : (float)k;
    for (int k = 0; k < 6; k++) s->prm.chord[k] = k < p->nchord ? (int32_t)p->chord[k] : -1;
    for (int k = 0; k < 6; k++) {
        if (k < p->nvt) vtd_from_raw(&s->vtunings[k], &p->vt[k]);
        else vtd_default(&s->vtunings[k]);
    }
}
int mksultra_preset_find(const char *path)
{
    for (int k = 0; k < mksultra_preset_count; k++) if (strcmp(mksultra_presets[k].path, path) == 0) return k;
    return -1;
}

void mksultra_note_on(void *st, int note, int vel)
{
    synth_t *s = (synth_t *)st;
    if (note < 0 || note > 127) return;
    if (vel <= 0) { mksultra_note_off(st, note); return; }
    if (vel > 127) vel = 127;
    float v = (float)vel * (1.0f / 127.0f);
    if (s->prm.arp_active) arp_key_on(&s->arp, note, v);
    else (void)vm_note_on5(s, note, note, v, s->num_voices == 1);
    s->dcemu.cnt = 0;
}
void mksultra_note_off(void *st, int note)
{
    synth_t *s = (synth_t *)st;
    if (note < 0 || note > 127) return;
    if (s->prm.arp_active) { if (arp_key_off(&s->arp, note)) vm_reset_all(s); return; }
    vm_note_off(s, note);
}
void mksultra_pitch_bend(void *st, float v) { ((synth_t *)st)->prm.pitch_wheel = fminr(fmaxr(v, -1.0f), 1.0f); }
void mksultra_mod_wheel(void *st, float v) { ((synth_t *)st)->prm.mod_wheel = fminr(fmaxr(v, 0.0f), 1.0f); }
void mksultra_aftertouch(void *st, float v)
{
    synth_t *s = (synth_t *)st;
    v = fminr(fmaxr(v, 0.0f), 1.0f);
    s->prm.aftertouch = v;
    for (int i = 0; i < NV; i++) s->voices[i].pressure = v;
}
void mksultra_sustain(void *st, int on)
{
    synth_t *s = (synth_t *)st;
    s->hold = on;
    if (!s->prm.arp_active) for (int i = 0; i < NV; i++) s->voices[i].hold = on;
    arp_set_hold(&s->arp, on);
}
void mksultra_all_notes_off(void *st)
{
    synth_t *s = (synth_t *)st;
    s->nheld = 0;
    arp_reset_all(&s->arp);
    for (int i = 0; i < NV; i++) { s->voices[i].gate = 0; s->voices[i].hold = 0; }
}
void mksultra_set_tempo(void *st, float bpm)
{
    synth_t *s = (synth_t *)st;
    s->bpm = bpm;
    lfo_set_bpm(&s->lfo, bpm);
    delay_set_bpm(&s->delay, bpm);
    arp_set_bpm(&s->arp, bpm);
}
void mksultra_process(void *st, float *l, float *r, int n)
{
    synth_t *s = (synth_t *)st;
#if MKSULTRA_BLOCKWISE
    mksultra_voices(st, l, r, n);               /* (in zwei Durchgängen: bitgleich, siehe mksultra.h) */
    mksultra_master(st, l, r, n);
    (void)s;
#else
    for (int i = 0; i < n; i++) synth_tick(s, &l[i], &r[i]);
#endif
}
void mksultra_voices(void *st, float *l, float *r, int n)
{
    synth_t *s = (synth_t *)st;
    for (int i = 0; i < n; i++) synth_voices(s, &l[i], &r[i]);
}
void mksultra_master(void *st, float *l, float *r, int n)
{
    synth_t *s = (synth_t *)st;
    for (int i = 0; i < n; i++) synth_master(s, &l[i], &r[i]);
}
int mksultra_active_voices(const void *st)
{
    const synth_t *s = (const synth_t *)st;
    int c = 0;
    for (int i = 0; i < NV; i++) if (!env_finished(&s->voices[i].env_m)) c++;
    return c;
}

/* ---------- Baustein-Schnittstelle (baustein.h) ---------- */
static const char *b_pname(unsigned p) { return mksultra_param_id((int)p); }
static void b_init(void *s, double rate) { mksultra_init(s, (float)rate); }
static void b_note_on(void *s, int note, int vel) { mksultra_note_on(s, note, vel); }
static void b_note_off(void *s, int note) { mksultra_note_off(s, note); }
static void b_set_param(void *s, unsigned p, double v)
{
    if (p >= MKSULTRA_PARAMS) return;
    float x = (float)v;
    mksultra_set_param(s, (int)p, PARAMS[p].min + x * (PARAMS[p].max - PARAMS[p].min));
}
static void b_render(void *s, float *l, float *r, int n)
{
    float bl[64], br[64];
    while (n > 0) {
        int k = n < 64 ? n : 64;
        mksultra_process(s, bl, br, k);
        for (int i = 0; i < k; i++) { l[i] += bl[i]; r[i] += br[i]; }
        l += k; r += k; n -= k;
    }
}
/* Klingt noch etwas? Stimmen aktiv oder Ausgang vor Kurzem hörbar (Delay bis 4 s, Reverb): so schneidet ein Adapter
 * den Nachklang der Effekte nicht ab. */
static int b_active(const void *st)
{
    const synth_t *s = (const synth_t *)st;
    if (mksultra_active_voices(st) > 0) return 1;
    float lim = s->sr * (s->delay.wet > 1e-5f ? 4.5f : 0.5f);
    return (float)s->quiet < lim;
}

const baustein_t mksultra_baustein = {"mksultra", "Alpha Juno / MKS-50 Ultra (TAL-Pha-Nachbau)", sizeof(synth_t),
                                      MKSULTRA_PARAMS, b_pname, b_init, b_note_on, b_note_off, b_set_param, b_render,
                                      b_active};
