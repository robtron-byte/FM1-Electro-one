/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * MKS-50 / Alpha Juno voice engine, ported from the MKS-50 iOS app:
 * MB87123-style DCO (chopped ramp, pulse, sub, noise, PolyBLEP), 4-step HPF,
 * 24 dB zero-delay-feedback ladder, 4-segment envelope, LFO with delay and
 * tempo sync, BBD chorus, poly / mono / unison, portamento, TAL-Pha style
 * pulse detune / sync and voice drift.
 *
 * One mks_part_t is one sound with its own voices. The audio interrupt owns
 * it; the control side changes it through the engine's command queue. */
#ifndef EL_MKS_H
#define EL_MKS_H
#include "el_config.h"

enum mks_param {
    /* MKS-50 tone parameters #0..#35 (SysEx order) */
    MP_DCO_ENV_MODE, MP_VCF_ENV_MODE, MP_VCA_ENV_MODE, MP_PULSE, MP_SAW, MP_SUB, MP_RANGE, MP_SUB_LEVEL,
    MP_NOISE, MP_HPF, MP_CHORUS, MP_DCO_LFO, MP_DCO_ENV, MP_DCO_AFTER, MP_PW, MP_PWM_RATE, MP_CUTOFF,
    MP_RESO, MP_VCF_LFO, MP_VCF_ENV, MP_VCF_KEY, MP_VCF_AFTER, MP_VCA_LEVEL, MP_VCA_AFTER, MP_LFO_RATE,
    MP_LFO_DELAY, MP_T1, MP_L1, MP_T2, MP_L2, MP_T3, MP_L3, MP_T4, MP_ENV_KEY, MP_CHORUS_RATE, MP_BENDER,
    /* patch level and extensions (iOS app ranges) */
    MP_KEY_MODE,      /* 0 poly, 1 chord (poly + chord memory), 2 mono, 3 unison */
    MP_PORTA_TIME, MP_PORTA, /* porta 0 off, 1 on, 2 auto (legato only) */
    MP_VOLUME, MP_KEY_SHIFT, /* key shift 0..24, 12 = none */
    MP_PULSE_DETUNE, MP_PULSE_SYNC, MP_PULSE_ENV, MP_DRIFT, MP_UNISON_DETUNE,
    MP_LFO_WAVE, MP_LFO_SYNC, MP_LFO_DIV, MP_LFO_KEYSYNC, MP_ENV_RETRIG, /* 0 multi, 1 legato */
    MP_DELAY_SEND, MP_REVERB_SEND, MP_CHORD,
    MP_COUNT
};

enum { MKS_MAX_VOICES = 6, MKS_CHORDS = 8, MKS_CHORD_NOTES = 5, MKS_CHORUS_SIZE = 1024 };
enum { MKS_AREA_RHYTHM = 1, MKS_AREA_STRINGS = 2, MKS_AREA_BASS = 3 };

typedef struct {
    char name[11];
    uint8_t area;
    uint8_t v[MP_COUNT];
} mks_preset_t;

extern const mks_preset_t mks_presets[];
extern const unsigned mks_preset_count;
extern const char *const mks_chord_names[MKS_CHORDS];
extern const int8_t mks_chords[MKS_CHORDS][MKS_CHORD_NOTES]; /* offsets, -128 ends */
/* Chord type (1-based, 0 = none) of records up to version 15, when there
 * were 13 (m7 m9 MIN 5+8 4THS 7SUS4 MAJ7 m7b5 MAJ SUS2 ADD9 MAJ9 7), to
 * the nearest of today's eight. */
extern const uint8_t st_chord_v15[14];

typedef struct { float ramp, m0, w0, m1, w1, m2, w2; } mks_chop_t;
typedef struct { float t1, l1, t2, l2, t3, l3, t4; } mks_env_settings_t;
typedef struct { float b0, b1, b2, a1, a2; } mks_biquad_t;

typedef struct {
    uint8_t stage;
    float value, start, target, progress, increment, curve, curve_decay;
} mks_env_t;

typedef struct {
    int16_t key;              /* sounding note, -1 when free */
    uint8_t gate, active;
    uint32_t age;
    float velocity, pitch, target_pitch, lfo_fade, gate_amp, unison_spread;
    float drift_pitch, drift_cutoff, drift_env, wander;
    uint32_t rng, noise;
    float phase, pulse_phase, last_main;
    float s1, s2, s3, s4, g, G, k;  /* ladder */
    float G2, G3, G4, inv, den;     /* its per-block coefficients (ladder_set) */
    float hz1, hz2;                 /* HPF state */
    float hpf_fc, hpf_b0, hpf_a1;   /* HPF modes 2, 3: corner at most the voice's fundamental */
    mks_env_t env;
} mks_voice_t;

typedef struct {
    float buffer[MKS_CHORUS_SIZE];
    unsigned write;
    float phase, lp1, lp2, wl1, wr1, wl2, wr2, mix, depth;
    uint32_t hiss;
} mks_chorus_t;

typedef struct {
    uint8_t v[MP_COUNT];
    uint8_t voices;           /* voice budget 1..MKS_MAX_VOICES */
    mks_voice_t voice[MKS_MAX_VOICES];
    mks_chorus_t chorus;
    float lfo_phase, lfo_value, lfo_held, lfo_from, lfo_to, pwm_phase;
    uint32_t lfo_rng, age;
    float cutoff_smoothed, volume_smoothed, level;
    int16_t held[8];          /* mono: held notes, newest last */
    uint8_t held_count;
    uint8_t silent_blocks;
    /* derived once per control block */
    mks_chop_t pulse, saw, sub;
    float sub_level, noise_level;
    mks_biquad_t hpf;
    uint8_t hpf_on, hpf_mode;
} mks_part_t;

/* Control task before audio, or audio side. */
void mks_part_init(mks_part_t *p, unsigned voices, uint32_t seed);
/* Audio side: replace the whole sound (stops voices only if the voice mode changed). */
EL_DSP_CODE void mks_part_load(mks_part_t *p, const uint8_t v[MP_COUNT]);
EL_DSP_CODE void mks_part_set(mks_part_t *p, unsigned param, int value);
/* legato: tie from the previous step (mono: glide without retrigger). */
EL_DSP_CODE void mks_note_on(mks_part_t *p, int note, float velocity, int legato);
EL_DSP_CODE void mks_note_off(mks_part_t *p, int note);
EL_DSP_CODE void mks_all_off(mks_part_t *p);
/* Adds frames (<= EL_BLOCK) to the buses. bpm drives tempo-synced LFOs. */
EL_DSP_CODE void mks_render(mks_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm);
EL_DSP_CODE int mks_active(const mks_part_t *p);

/* Display helpers (control task). */
int mks_param_max(unsigned param);
const char *mks_param_name(unsigned param);
const char *mks_param_value_name(unsigned param, int value); /* NULL for plain numbers */
#endif
