/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P voice engine for FM1-Electro (docs/adr/0039): one part per area, as engine/mks.h.
 *
 * Origin: fm1-synths (github.com/robtron-byte/fm1-synths, commit 2a7b4fa, jx8p/engine/jx.c and jx.h,
 * GPL-3.0-only, robtron-byte), a port of the user's JX-8P iOS app's DSP (github.com/robtron-byte/JX8P,
 * branch cursor/ios-jx8p-grundlage-76ee, commit 9687574: DCO.swift, HardwareCurves.swift,
 * SynthEngine.swift): two DCOs per voice (noise / square / pulse / saw, SYNC1 / SYNC2 / XMOD), mixer with
 * ENV on DCO-2, HPF in 4 steps, 24 dB ladder, VCA (GATE / ENV-2), two ADSR envelopes, LFO with delay and
 * tempo sync, BBD chorus; poly / chord / mono / unison, portamento, voice drift.
 *
 * Changed for FM1-Electro: no shared voice pool over four parts; each area's part has its own voices
 * (budget 1..JX8P_MAX_VOICES) and renders itself (jx8p_render, as mks_render); the parameters are numbered
 * without gaps (JX_*, 68 of them, so a sound fits SYN_VALUES = 69 and the store layout stays); the chord
 * memory is FM1-Electro's (engine/chords.c, the same eight chords as the JX-8P app). */
#ifndef JX8P_H
#define JX8P_H
#include "el_config.h"
#include "jx8p_dsp.h"
#include "chords.h"

enum jx8p_param {
    /* JX-8P tone parameters #11..#22, #26..#56, #58 in their order (the app's ToneData.tone). Switches as
     * positions 0..3, DCO TUNE as steps 0..24 (12 = 0). */
    JX_DCO1_RANGE, JX_DCO1_WAVE, JX_DCO1_TUNE, JX_DCO1_LFO, JX_DCO1_ENV,
    JX_DCO2_RANGE, JX_DCO2_WAVE, JX_DCO2_XMOD, JX_DCO2_TUNE, JX_DCO2_FINE, JX_DCO2_LFO, JX_DCO2_ENV,
    JX_DCO_DYNA, JX_DCO_ENV_MODE,
    JX_MIX_DCO1, JX_MIX_DCO2, JX_MIX_ENV, JX_MIX_DYNA, JX_MIX_ENV_MODE,
    JX_HPF, JX_CUTOFF, JX_RESO, JX_VCF_LFO, JX_VCF_ENV, JX_VCF_KEY, JX_VCF_DYNA, JX_VCF_ENV_MODE,
    JX_VCA_LEVEL, JX_VCA_DYNA, JX_CHORUS, JX_LFO_WAVE, JX_LFO_DELAY, JX_LFO_RATE,
    JX_E1_ATTACK, JX_E1_DECAY, JX_E1_SUSTAIN, JX_E1_RELEASE, JX_E1_KEY,
    JX_E2_ATTACK, JX_E2_DECAY, JX_E2_SUSTAIN, JX_E2_RELEASE, JX_E2_KEY,
    JX_VCA_ENV_MODE,
    JX_TONE_END,                 /* 44: the tone ends here */
    /* patch level (the iOS app's ranges) */
    JX_BENDER = JX_TONE_END,     /* kept for the sounds (the FM-1 has no bender) */
    JX_BEND_LFO, JX_AFTER_VIB, JX_AFTER_BRIL, JX_AFTER_VOL,   /* kept, not used */
    JX_CHORUS_RATE,              /* FREE chorus rate */
    JX_CHORUS_MODE,              /* 0 JX (the tone's CHORUS switch), 1 I+II, 2 FREE */
    JX_KEY_MODE,                 /* 0 poly, 1 chord (poly + chord memory), 2 mono, 3 unison */
    JX_PORTA_TIME, JX_PORTA,     /* porta 0 off, 1 on, 2 auto (legato only) */
    JX_MOD_SENS,                 /* kept */
    JX_VOLUME, JX_KEY_SHIFT,     /* key shift 0..24, 12 = none */
    JX_DRIFT, JX_UNISON_DETUNE,
    JX_LFO_SHAPE,                /* 0 JX (the tone's LFO WAVE), 1..7 the extended shapes */
    JX_LFO_SYNC, JX_LFO_DIV, JX_LFO_KEYSYNC,
    JX_ENV_RETRIG,               /* 0 multi, 1 legato */
    JX_ENV_SNAP,                 /* 0 ORIG, 1 SNAP (shorter lowest envelope times) */
    JX_DELAY_SEND, JX_REVERB_SEND, JX_CHORD,   /* sends to delay / reverb, chord type 0..7 */
    JX_COUNT                     /* 68 */
};

enum { JX8P_MAX_VOICES = 6, JX8P_UNISON_MAX = 6, JX8P_CONTROL = JXD_CONTROL };

typedef struct {
    char name[11];
    uint8_t area;             /* 1 RHYTHM, 2 STRINGS, 3 BASS (SYN_AREA_*) */
    uint8_t v[JX_COUNT];
} jx8p_preset_t;

extern const jx8p_preset_t jx8p_presets[];
extern const unsigned jx8p_preset_count;

typedef struct {
    int16_t key;              /* sounding note, -1 when free */
    uint8_t gate, active;
    uint32_t age;
    float velocity, pitch, target_pitch, lfo_fade, gate_amp, unison_spread;
    float drift_pitch, drift_cutoff, drift_env, wander;
    uint32_t rng, noise;
    float phase1, phase2, last1;     /* the two DCOs */
    jxd_ladder_t ld;
    float hz1, hz2;                  /* HPF state */
    jxd_env_t env1, env2;
    /* both envelopes' settings as last worked out, and what they came from (the part's settings
     * generation, pitch and drift): recomputed only when one of these changes */
    jxd_env_settings_t es1, es2;
    float es_pitch, es_drift;
    uint32_t es_gen;
    uint8_t es_ok;
} jx8p_voice_t;

/* What a part's voices share for one control step (SynthEngine.BlockParams). */
typedef struct {
    int dco_env_mode, mix_env_mode, vcf_env_mode, gate_mode, porta, xmod;
    float dco_dyn, mix_dyn, vcf_dyn, vca_dyn;
    float vibrato1, vibrato2, dco1_env, dco2_env, pitch1, pitch2;
    float mix1, mix2, mix_env;
    float porta_coef, lfo_fade_inc, drift, unison;
    float cutoff_oct, key_follow, vcf_env, vcf_lfo, resonance, amp, gate_coef, gate_release;
    float lfo_value;
    jxd_chop_t wave1, wave2;
    uint8_t noise1, noise2;
} jx8p_slice_t;

typedef struct {
    uint8_t v[JX_COUNT];
    uint8_t voices;           /* voice budget 1..JX8P_MAX_VOICES */
    jx8p_voice_t voice[JX8P_MAX_VOICES];
    jxd_chorus_t chorus;
    float lfo_phase, lfo_value, lfo_held, lfo_from, lfo_to;
    uint32_t lfo_rng, age;
    float cutoff_smoothed, volume_smoothed, level;
    int16_t held[8];          /* mono: held notes, newest last */
    uint8_t held_count;
    uint8_t silent_blocks;
    jxd_biquad_t hpf;
    uint8_t hpf_mode;
    /* the two ADSRs for the sound's knobs (before key follow and drift), and a counter that changes with them */
    jxd_env_settings_t base1, base2;
    uint32_t env_gen;
} jx8p_part_t;

/* Control task before audio, or audio side. */
void jx8p_part_init(jx8p_part_t *p, unsigned voices, uint32_t seed);
/* Audio side: replace the whole sound (stops voices only if the key mode changed). */
EL_DSP_CODE void jx8p_part_load(jx8p_part_t *p, const uint8_t v[JX_COUNT]);
EL_DSP_CODE void jx8p_part_set(jx8p_part_t *p, unsigned param, int value);
/* legato: tie from the previous step (mono: glide without retrigger). */
EL_DSP_CODE void jx8p_note_on(jx8p_part_t *p, int note, float velocity, int legato);
EL_DSP_CODE void jx8p_note_off(jx8p_part_t *p, int note);
EL_DSP_CODE void jx8p_all_off(jx8p_part_t *p);
/* Adds frames (<= EL_BLOCK) to the buses. bpm drives tempo-synced LFOs. */
EL_DSP_CODE void jx8p_render(jx8p_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm);
EL_DSP_CODE int jx8p_active(const jx8p_part_t *p);

/* Value ranges (both sides); names for the display (control task, engine/jx8p_names.c). */
int jx8p_param_max(unsigned param);
const char *jx8p_param_name(unsigned param);
const char *jx8p_param_value_name(unsigned param, int value); /* NULL for plain numbers */
#endif
