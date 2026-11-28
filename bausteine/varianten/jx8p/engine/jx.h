/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P voice engine for the M-VAVE FM-1: a port of the user's JX-8P iOS app's DSP
 * (github.com/robtron-byte/JX8P, branch cursor/ios-jx8p-grundlage-76ee, commit 9687574: ios/Shared/DSP/DCO.swift
 * JXOscillators, HardwareCurves.swift, SynthEngine.swift renderVoice / renderControlBlock): two DCOs per voice
 * (noise / square / pulse / saw, SYNC1 / SYNC2 / XMOD, PolyBLEP also at the sync jumps), mixer with ENV on
 * DCO-2, 1-pole HPF in 4 steps, 24 dB ladder, VCA (GATE / ENV-2), two ADSR envelopes, LFO with delay and
 * tempo sync, BBD chorus; poly / chord / mono / unison, portamento, voice drift.
 *
 * The JX-8P app is the user's MKS-50 app with another voice; so is this file the MKS-50 firmware's
 * mks50/engine/mks.c with another voice: the pool shared by all parts, the parts, the block in phases two
 * cores share (mks50/docs/adr/0002, 0003) are mks.c's, line for line (jx8p/docs/adr/0001). The DSP blocks
 * every synth shares are in ../../shared/dsp/.
 *
 * Not taken from the app (as in the MKS-50 firmware): pitch bend, mod wheel and aftertouch (the FM-1 has
 * none; the sounds' depths for them are kept), the tunable extra HPF, the distortion, voice shedding. */
#ifndef JX_H
#define JX_H
#include "jx_config.h"
#include "syn_dsp.h"

enum jx_param {
    /* JX-8P tone parameters, index = JX-8P parameter number #11..#58 (APR / IPR order; the iOS app's
     * ToneData.tone). Switches as positions 0..3, DCO TUNE as steps 0..24 (12 = 0). Slots 0..10, 23..25
     * and 57 are not tone parameters (kept 0). */
    JP_DCO1_RANGE = 11, JP_DCO1_WAVE, JP_DCO1_TUNE, JP_DCO1_LFO, JP_DCO1_ENV,
    JP_DCO2_RANGE, JP_DCO2_WAVE, JP_DCO2_XMOD, JP_DCO2_TUNE, JP_DCO2_FINE, JP_DCO2_LFO, JP_DCO2_ENV,
    JP_DCO_DYNA = 26, JP_DCO_ENV_MODE,
    JP_MIX_DCO1, JP_MIX_DCO2, JP_MIX_ENV, JP_MIX_DYNA, JP_MIX_ENV_MODE,
    JP_HPF, JP_CUTOFF, JP_RESO, JP_VCF_LFO, JP_VCF_ENV, JP_VCF_KEY, JP_VCF_DYNA, JP_VCF_ENV_MODE,
    JP_VCA_LEVEL, JP_VCA_DYNA, JP_CHORUS, JP_LFO_WAVE, JP_LFO_DELAY, JP_LFO_RATE,
    JP_E1_ATTACK, JP_E1_DECAY, JP_E1_SUSTAIN, JP_E1_RELEASE, JP_E1_KEY,
    JP_E2_ATTACK, JP_E2_DECAY, JP_E2_SUSTAIN, JP_E2_RELEASE, JP_E2_KEY,
    JP_VCA_ENV_MODE = 58,
    JP_TONE_END,                 /* 59: the tone ends here (SysEx, presets) */
    /* patch level (the iOS app's ranges) */
    JP_BENDER = JP_TONE_END,     /* bender range 0..12 (kept, the FM-1 has no bender) */
    JP_BEND_LFO, JP_AFTER_VIB, JP_AFTER_BRIL, JP_AFTER_VOL,   /* kept for the sounds, not used here */
    JP_CHORUS_RATE,              /* FREE chorus rate */
    JP_CHORUS_MODE,              /* 0 JX (the tone's CHORUS switch), 1 I+II, 2 FREE */
    JP_KEY_MODE,                 /* 0 poly, 1 chord (poly + chord memory), 2 mono, 3 unison */
    JP_PORTA_TIME, JP_PORTA,     /* porta 0 off, 1 on, 2 auto (legato only) */
    JP_MOD_SENS,                 /* kept */
    JP_VOLUME, JP_KEY_SHIFT,     /* key shift 0..24, 12 = none */
    JP_DRIFT, JP_UNISON_DETUNE,
    JP_LFO_SHAPE,                /* 0 JX (the tone's LFO WAVE), 1..7 the extended shapes */
    JP_LFO_SYNC, JP_LFO_DIV, JP_LFO_KEYSYNC,
    JP_ENV_RETRIG,               /* 0 multi, 1 legato */
    JP_ENV_SNAP,                 /* 0 ORIG, 1 SNAP (shorter lowest envelope times) */
    JP_DELAY_SEND, JP_REVERB_SEND, JP_CHORD,   /* sends to delay / reverb, chord type */
    JP_COUNT
};

enum {
    JX_PARTS = 4,           /* sounds at once, each with its chorus */
    JX_POOL_MAX = 8,        /* voices in the pool at most (the build's pool: jx_init) */
    JX_UNISON_MAX = 6,      /* voices a unison note takes at most (the JX-8P's six) */
    JX_GUARANTEE = 2,       /* voices a part keeps while others want more */
    JX_CORES = 2,
    JX_CONTROL = SYN_CONTROL,
    JX_SLICES = JX_BLOCK / JX_CONTROL,
    JX_CHORDS = 8, JX_CHORD_NOTES = 5
};

typedef struct {
    char name[11];
    uint8_t cat;              /* jx_cat_names */
    uint8_t v[JP_COUNT];
} jx_preset_t;

extern const jx_preset_t jx_presets[];
extern const unsigned jx_preset_count;
extern const char *const jx_cat_names[];
extern const unsigned jx_cat_count;

typedef struct {
    int16_t key;              /* sounding note, -1 when free */
    uint8_t gate, active;
    uint8_t part;             /* whose sound it plays (kept while free) */
    uint8_t core;             /* which core renders it this block */
    uint32_t age;
    float velocity, pitch, target_pitch, lfo_fade, gate_amp, unison_spread;
    float drift_pitch, drift_cutoff, drift_env, wander;
    uint32_t rng, noise;
    float phase1, phase2, last1;     /* the two DCOs (JXOscillators) */
    syn_ladder_t ld;
    float hz1, hz2;                  /* HPF state */
    syn_env_t env1, env2;
    /* both envelopes' settings as last worked out, and what they came from (the part's settings
     * generation, pitch and drift): recomputed only when one of these changes */
    syn_env_settings_t es1, es2;
    float es_pitch, es_drift;
    uint32_t es_gen;
    uint8_t es_ok;
} jx_voice_t;

/* What a part's voices share for one control step (worked out before the voices, so both cores read the
 * same): SynthEngine.BlockParams. */
typedef struct {
    int dco_env_mode, mix_env_mode, vcf_env_mode, gate_mode, porta, xmod;
    float dco_dyn, mix_dyn, vcf_dyn, vca_dyn;
    float vibrato1, vibrato2, dco1_env, dco2_env, pitch1, pitch2;
    float mix1, mix2, mix_env;
    float porta_coef, lfo_fade_inc, drift, unison;
    float cutoff_oct, key_follow, vcf_env, vcf_lfo, resonance, amp, gate_coef, gate_release;
    float lfo_value;
    syn_chop_t wave1, wave2;
    uint8_t noise1, noise2;
} jx_slice_t;

typedef struct {
    uint8_t v[JP_COUNT];
    syn_chorus_t chorus;
    float lfo_phase, lfo_value, lfo_held, lfo_from, lfo_to;
    uint32_t lfo_rng;
    float cutoff_smoothed, volume_smoothed, level;
    int16_t held[8];          /* mono: held notes, newest last */
    uint8_t held_count;
    uint8_t silent_blocks;
    uint8_t live;             /* this block: voices or a chorus tail to render */
    uint8_t core;             /* this block: which core mixes it and runs its chorus */
    uint8_t ctl_core;         /* this block: which core works out its control values */
    syn_biquad_t hpf;
    uint8_t hpf_mode;
    /* the two ADSRs for the sound's knobs (before key follow and drift), and a counter that changes with them */
    syn_env_settings_t base1, base2;
    uint32_t env_gen;
    jx_slice_t slice[JX_SLICES];
} jx_part_t;

typedef struct {
    jx_part_t part[JX_PARTS];
    jx_voice_t voice[JX_POOL_MAX];
    uint8_t pool;             /* voices in the pool, 1..JX_POOL_MAX */
    uint8_t cores;            /* 1 or 2: how many share a block (jx_begin) */
    uint8_t extra1;           /* load units core 1 has besides (the app's effects), for jx_plan */
    uint32_t age;
    float acc[JX_CORES][JX_PARTS][2][JX_BLOCK];
    float out[JX_CORES][2][JX_BLOCK];
    float send[JX_CORES][2][JX_BLOCK];
    float send_sum[2][JX_BLOCK];
    uint8_t voices_on[JX_CORES], parts_on[JX_CORES], ctl_on[JX_CORES];
} jx_t;

void jx_init(jx_t *s, unsigned pool, uint32_t seed);

/* ---- audio side (the interrupt, or the control task before it runs); as mks.h ---- */
EL_DSP_CODE void jx_part_load(jx_t *s, unsigned part, const uint8_t v[JP_COUNT]);
EL_DSP_CODE void jx_part_set(jx_t *s, unsigned part, unsigned param, int value);
EL_DSP_CODE void jx_note_on(jx_t *s, unsigned part, int note, float velocity, int legato);
EL_DSP_CODE void jx_note_off(jx_t *s, unsigned part, int note);
EL_DSP_CODE void jx_all_off(jx_t *s, unsigned part);
EL_DSP_CODE int jx_active(const jx_t *s, unsigned part);
EL_DSP_CODE unsigned jx_part_voices(const jx_t *s, unsigned part);

/* One block of JX_BLOCK frames: jx_plan, jx_control(core), jx_voices(core), jx_finish(core), jx_mix
 * (the order and the rules of mks.h). */
EL_DSP_CODE void jx_plan(jx_t *s);
EL_DSP_CODE void jx_control(jx_t *s, unsigned core, float bpm);
EL_DSP_CODE void jx_voices(jx_t *s, unsigned core);
EL_DSP_CODE void jx_finish(jx_t *s, unsigned core);
EL_DSP_CODE void jx_mix(jx_t *s, float *left, float *right);
EL_DSP_CODE void jx_begin(jx_t *s, float bpm);
EL_DSP_CODE void jx_render(jx_t *s, float *left, float *right, float bpm);

/* Display helpers (control task). */
int jx_param_max(unsigned param);
int jx_param_is_tone(unsigned param);
const char *jx_param_name(unsigned param);
float jx_lfo_hz(int rate);                       /* LFO RATE 0..127 in Hz (the engine's curve) */
const char *jx_param_value_name(unsigned param, int value); /* NULL for plain numbers */
#endif
