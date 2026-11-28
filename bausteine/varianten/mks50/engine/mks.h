/* SPDX-License-Identifier: GPL-3.0-only
 * MKS-50 / Alpha Juno voice engine. From FM1-Electro's engine/mks.h
 * (github.com/robtron-byte/FM1-Electro-one, commit e7b3b14; there a port of
 * the MKS-50 iOS app's DSP): MB87123-style DCO (chopped ramp, pulse, sub,
 * noise, PolyBLEP), 4-step HPF, 24 dB zero-delay-feedback ladder, 4-segment
 * envelope, LFO with delay and tempo sync, BBD chorus, poly / mono / unison,
 * portamento, TAL-Pha style pulse detune / sync and voice drift.
 *
 * Changed for this firmware (docs/adr/0002, 0003): the voices are one pool
 * shared by all parts (a part alone gets them all), each part with its own
 * sound and chorus; a block is rendered in two phases that two cores share
 * (the voices, then each part's chorus). The sound's arithmetic is that of
 * FM1-Electro's (host/identity_check.c holds them against each other).
 *
 * The audio interrupt owns the synth; everything else reaches it through the
 * app's queue. */
#ifndef MKS_H
#define MKS_H
#include "mk_config.h"
#include "syn_dsp.h"

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
    MP_DELAY_SEND, MP_REVERB_SEND, MP_CHORD, /* sends to delay / reverb (docs/adr/0009), chord type (app, docs/adr/0010) */
    MP_COUNT
};

enum {
    MKS_PARTS = 4,          /* sounds at once, each with its chorus */
    MKS_POOL_MAX = 8,       /* voices in the pool at most (the build's pool: mks_init) */
    MKS_UNISON_MAX = 6,     /* voices a unison note takes at most (the MKS-50's six) */
    MKS_GUARANTEE = 2,      /* voices a part keeps while others want more */
    MKS_CORES = 2,
    MKS_CONTROL = SYN_CONTROL,   /* frames per control step */
    MKS_SLICES = MKS_BLOCK / MKS_CONTROL,
    MKS_CHORDS = 8, MKS_CHORD_NOTES = 5
};
enum { MKS_AREA_RHYTHM = 1, MKS_AREA_STRINGS = 2, MKS_AREA_BASS = 3 };

typedef struct {
    char name[11];
    uint8_t area;
    uint8_t v[MP_COUNT];
} mks_preset_t;

extern const mks_preset_t mks_presets[];
extern const unsigned mks_preset_count;


typedef struct {
    int16_t key;              /* sounding note, -1 when free */
    uint8_t gate, active;
    uint8_t part;             /* whose sound it plays (kept while free) */
    uint8_t core;             /* which core renders it this block */
    uint32_t age;
    float velocity, pitch, target_pitch, lfo_fade, gate_amp, unison_spread;
    float drift_pitch, drift_cutoff, drift_env, wander;
    uint32_t rng, noise;
    float phase, pulse_phase, last_main;
    syn_ladder_t ld;                /* the ladder */
    float vcf_acc, vcf_y0, vcf_y1;  /* the ladder at half rate (mks_t.vcf_half): its input sum, its last two outputs */
    uint8_t vcf_cnt;
    float hz1, hz2;                 /* HPF state */
    float hpf_fc, hpf_b0, hpf_a1;   /* HPF modes 2, 3: corner at most the voice's fundamental */
    syn_env_t env;
    /* env_settings as last worked out, and what they came from (the knobs,
     * pitch and drift): recomputed only when one of these changes */
    syn_env_settings_t es;
    float es_pitch, es_drift;
    uint8_t es_key[9], es_ok;
} mks_voice_t;

/* What a part's voices share for one control step (worked out before the
 * voices, so both cores read the same). */
typedef struct {
    float vibrato, dco_env, pitch_offset, porta_coef, lfo_fade_inc, drift, unison, pulse_detune, pulse_env;
    float cutoff_oct, key_follow, vcf_env, vcf_lfo, resonance, amp, gate_coef, gate_release;
    int dco_env_mode, vcf_env_mode, gate_mode, dynamics, sync, porta;
    float lfo_value, sub_level, noise_level;
    syn_chop_t pulse, saw, sub;
} mks_slice_t;

typedef struct {
    uint8_t v[MP_COUNT];
    syn_chorus_t chorus;
    float lfo_phase, lfo_value, lfo_held, lfo_from, lfo_to, pwm_phase;
    uint32_t lfo_rng;
    float cutoff_smoothed, volume_smoothed, level;
    int16_t held[8];          /* mono: held notes, newest last */
    uint8_t held_count;
    uint8_t silent_blocks;
    uint8_t live;             /* this block: voices or a chorus tail to render */
    uint8_t core;             /* this block: which core mixes it and runs its chorus */
    uint8_t ctl_core;         /* this block: which core works out its control values */
    /* derived once per control step */
    syn_chop_t pulse, saw, sub;
    float sub_level, noise_level;
    syn_biquad_t hpf;
    uint8_t hpf_on, hpf_mode;
    float base_t[4];          /* T1..T4 in seconds for the sound's knobs (before key follow and drift): worked out when a knob changes, not at every note */
    mks_slice_t slice[MKS_SLICES];
} mks_part_t;

typedef struct {
    mks_part_t part[MKS_PARTS];
    mks_voice_t voice[MKS_POOL_MAX];
    uint8_t pool;             /* voices in the pool, 1..MKS_POOL_MAX */
    uint8_t cores;            /* 1 or 2: how many share a block (mks_begin) */
    uint8_t vcf_half;         /* 1: the ladder at half the sample rate (FM1-Electro's MKS_VCF_DECIM 2), default 0 */
    uint8_t release_hurry;    /* 1: a release that has faded far ends 8 times faster (FM1-Electro's default), default 0 */
    uint8_t extra1;           /* load units core 1 has besides (the app's effects), for mks_plan */
    uint32_t age;
    /* a block's sums: per core and part while the voices render, then per core */
    float acc[MKS_CORES][MKS_PARTS][2][MKS_BLOCK];
    float out[MKS_CORES][2][MKS_BLOCK];
    float send[MKS_CORES][2][MKS_BLOCK];      /* the parts' delay (0) and reverb (1) sends, mono (docs/adr/0009) */
    float send_sum[2][MKS_BLOCK];             /* mks_mix: both cores' sends together */
    uint8_t voices_on[MKS_CORES], parts_on[MKS_CORES], ctl_on[MKS_CORES];   /* this block: how many each core has */
} mks_t;

/* Before the audio starts. pool: voices 1..MKS_POOL_MAX. */
void mks_init(mks_t *s, unsigned pool, uint32_t seed);

/* ---- audio side (the interrupt, or the control task before it runs) ---- */
/* Replace a part's whole sound (stops its voices only if the voice mode changed). */
EL_DSP_CODE void mks_part_load(mks_t *s, unsigned part, const uint8_t v[MP_COUNT]);
EL_DSP_CODE void mks_part_set(mks_t *s, unsigned part, unsigned param, int value);
/* legato: tie from the previous step (mono: glide without retrigger). */
EL_DSP_CODE void mks_note_on(mks_t *s, unsigned part, int note, float velocity, int legato);
EL_DSP_CODE void mks_note_off(mks_t *s, unsigned part, int note);
EL_DSP_CODE void mks_all_off(mks_t *s, unsigned part);
EL_DSP_CODE int mks_active(const mks_t *s, unsigned part);
EL_DSP_CODE unsigned mks_part_voices(const mks_t *s, unsigned part);   /* sounding voices of a part */

/* One block of MKS_BLOCK frames, in this order:
 *   mks_plan             who renders what (core 0 only)
 *   mks_control(core)    each core the control values of its parts   (the cores side by side)
 *   mks_voices(core)     each core its voices            (side by side, after BOTH mks_control)
 *   mks_finish(core)     each core its parts: sum, level, chorus  (side by side, after BOTH mks_voices)
 *   mks_mix              adds the block to left / right (core 0 only, after BOTH mks_finish)
 * bpm drives tempo-synced LFOs. */
EL_DSP_CODE void mks_plan(mks_t *s);
EL_DSP_CODE void mks_control(mks_t *s, unsigned core, float bpm);
EL_DSP_CODE void mks_voices(mks_t *s, unsigned core);
EL_DSP_CODE void mks_finish(mks_t *s, unsigned core);
EL_DSP_CODE void mks_mix(mks_t *s, float *left, float *right);
/* plan and both cores' control values on this core (the host's one-core block) */
EL_DSP_CODE void mks_begin(mks_t *s, float bpm);
/* All four on one core. */
EL_DSP_CODE void mks_render(mks_t *s, float *left, float *right, float bpm);

/* Display helpers (control task). */
int mks_param_max(unsigned param);
const char *mks_param_name(unsigned param);
float mks_lfo_hz(int rate);                       /* LFO RATE 0..127 in Hz (the engine's curve) */
const char *mks_param_value_name(unsigned param, int value); /* NULL for plain numbers */
#endif
