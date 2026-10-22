/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Jupiter-6 voice engine, ported from the JP-6 iOS app: two VCOs with
 * combinable waveforms (PolyBLEP), sync 1->2 / 2->1, cross mod with ENV-1
 * depth, non-resonant HPF, IR3109-style multimode VCF (two SVF sections:
 * LPF / BPF / HPF), two ADSR envelopes, LFO with delay and tempo sync,
 * poly / mono / unison, portamento, voice drift, per-sound distortion and
 * BBD chorus.
 *
 * Same API shape as engine/mks.h: one jp6_part_t is one sound with its own
 * voices, owned by the audio interrupt. */
#ifndef EL_JP6_H
#define EL_JP6_H
#include "el_config.h"
#include "dist.h"

enum jp6_param {
    /* Tone parameters (JP-6 panel order of the app, without the
     * aftertouch depths: no aftertouch on the FM-1) */
    JP_VCO1_TRI, JP_VCO1_SAW, JP_VCO1_PULSE, JP_VCO1_SQUARE, /* 0/1 each, combinable */
    JP_VCO1_RANGE,    /* 0 32', 1 16', 2 8', 3 4', 4 2' */
    JP_XMOD,          /* VCO-2 -> VCO-1 FM */
    JP_SYNC,          /* 0 off, 1 1->2, 2 2->1 */
    JP_VCO2_TRI, JP_VCO2_SAW, JP_VCO2_PULSE, JP_VCO2_NOISE,
    JP_VCO2_RANGE,    /* 0 LO (sub-audio), 1 32' ... 5 2' */
    JP_VCO2_TUNE,     /* 0..24, 12 = 0 semitones */
    JP_VCO2_FINE,     /* 64 = 0, +-50 cents */
    JP_PWM_DEPTH, JP_PWM_SOURCE, /* source 0 LFO, 1 MAN, 2 ENV-1 */
    JP_VCO_LFO, JP_VCO_ENV,
    JP_VCO_MOD_DEST,  /* 0 VCO-1, 1 VCO-2, 2 both */
    JP_MIX,           /* 0 VCO-1 ... 64 both ... 127 VCO-2 */
    JP_HPF,           /* 0 off, 1..127 = 20 Hz ... 4 kHz */
    JP_VCF_MODE,      /* 0 LPF 24 dB, 1 BPF 12 dB, 2 HPF 24 dB */
    JP_CUTOFF, JP_RESO, JP_VCF_ENV,
    JP_VCF_ENV_SRC,   /* 0 ENV-1, 1 ENV-2 */
    JP_VCF_LFO, JP_VCF_KEY, JP_VCF_VELO,
    JP_VCA_LEVEL, JP_VCA_LFO, JP_VCA_VELO,
    JP_LFO_RATE, JP_LFO_DELAY, JP_LFO_WAVE,
    JP_ENV1_A, JP_ENV1_D, JP_ENV1_S, JP_ENV1_R, JP_ENV1_KEY,
    JP_ENV1_POL,      /* 0 normal, 1 inverted */
    JP_ENV2_A, JP_ENV2_D, JP_ENV2_S, JP_ENV2_R, JP_ENV2_KEY,
    JP_XMOD_ENV,      /* ENV-1 -> cross mod amount */
    /* patch level and extensions (iOS app ranges) */
    JP_KEY_MODE,      /* 0 poly, 1 chord (poly + chord memory), 2 mono, 3 unison */
    JP_PORTA_TIME, JP_PORTA, /* porta 0 off, 1 on, 2 auto (legato only) */
    JP_VOLUME, JP_KEY_SHIFT, /* key shift 0..24, 12 = none */
    JP_DRIFT, JP_UNISON_DETUNE,
    JP_LFO_SYNC, JP_LFO_DIV, JP_LFO_KEYSYNC, JP_ENV_RETRIG, /* 0 multi, 1 legato */
    JP_CHORUS, JP_CHORUS_RATE, JP_CHORUS_MODE, /* mode 0 free rate, 1 I, 2 II, 3 I+II */
    JP_DIST_MODE,     /* 0 off, 1 OD, 2 DIST, 3 RAT, 4 TUBE */
    JP_DIST_DRIVE, JP_DIST_TONE, JP_DIST_LEVEL, JP_DIST_MIX,
    JP_DELAY_SEND, JP_REVERB_SEND, JP_CHORD,
    JP_COUNT
};

enum { JP6_MAX_VOICES = 6, JP6_CHORDS = 8, JP6_CHORUS_SIZE = 512 };
enum { JP6_AREA_RHYTHM = 1, JP6_AREA_STRINGS = 2, JP6_AREA_BASS = 3 };

typedef struct {
    char name[11];
    uint8_t area;
    uint8_t v[JP_COUNT];
} jp6_preset_t;

extern const jp6_preset_t jp6_presets[];
extern const unsigned jp6_preset_count;
/* The app's eight chord memories; the same chords (and order) as
 * mks_chords[0..7], so JP_CHORD + 1 is a valid st chord type. */
extern const char *const jp6_chord_names[JP6_CHORDS];

typedef struct {
    uint8_t stage;
    float value, start, target, progress, increment, curve, curve_decay;
} jp6_env_t;

typedef struct {
    int16_t key;              /* sounding key, -1 when free */
    uint8_t gate, active, unison;
    uint32_t age;
    float velocity, pitch, target_pitch, lfo_fade, unison_spread;
    float drift_pitch, drift_cutoff, drift_env, wander;
    uint32_t rng, noise;
    float phase1, phase2, last2;    /* VCOs */
    float a1, b1, a2, b2;           /* VCF sections */
    float hz1, hz2;                 /* HPF state */
    jp6_env_t env1, env2;
} jp6_voice_t;

/* MN3009-style BBD chorus. 512 samples cover the longest read (3.5 ms
 * centre + 1.9 ms depth = 240 samples at 44.1 kHz). */
typedef struct {
    float buffer[JP6_CHORUS_SIZE];
    unsigned write;
    float phase, lp1, lp2, wl1, wr1, wl2, wr2, mix, depth;
    uint32_t hiss;
} jp6_chorus_t;



typedef struct {
    uint8_t v[JP_COUNT];
    uint8_t voices;           /* voice budget 1..JP6_MAX_VOICES */
    uint8_t held_count, silent_blocks, hpf_on;
    int16_t held[8];          /* mono: held notes, newest last */
    jp6_voice_t voice[JP6_MAX_VOICES];
    jp6_chorus_t chorus;
    el_dist_t dist;
    float lfo_phase, lfo_value, lfo_held, lfo_from, lfo_to;
    uint32_t lfo_rng, age;
    float last_pitch, cutoff_smoothed, volume_smoothed, hpf_smoothed, hpf_coef_at, level;
    float dc_xl, dc_yl, dc_xr, dc_yr;
    float hpf_b0, hpf_b1, hpf_b2, hpf_a1, hpf_a2;
} jp6_part_t;

/* Control task before audio, or audio side. */
void jp6_part_init(jp6_part_t *p, unsigned voices, uint32_t seed);
/* Audio side: replace the whole sound (stops voices only if the key mode changed). */
EL_DSP_CODE void jp6_part_load(jp6_part_t *p, const uint8_t v[JP_COUNT]);
EL_DSP_CODE void jp6_part_set(jp6_part_t *p, unsigned param, int value);
/* legato: tie from the previous step (mono: glide without retrigger). */
EL_DSP_CODE void jp6_note_on(jp6_part_t *p, int note, float velocity, int legato);
EL_DSP_CODE void jp6_note_off(jp6_part_t *p, int note);
EL_DSP_CODE void jp6_all_off(jp6_part_t *p);
/* Adds frames (<= EL_BLOCK) to the buses. bpm drives tempo-synced LFOs. */
EL_DSP_CODE void jp6_render(jp6_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm);
EL_DSP_CODE int jp6_active(const jp6_part_t *p);

/* Display helpers (control task). */
int jp6_param_max(unsigned param);
const char *jp6_param_name(unsigned param);
const char *jp6_param_value_name(unsigned param, int value); /* NULL for plain numbers */
#endif
