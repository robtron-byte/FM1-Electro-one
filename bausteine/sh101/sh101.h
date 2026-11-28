/* SPDX-License-Identifier: GPL-3.0-only
 * sh101: SH-101 (monophon, wahlweise 6 Stimmen poly) nach dem Swift-Nachbau „MonoBass“ des Nutzers
 * (Dropbox Programmieren/Musik/REA/SH101/app/Sources/MonoBassCore, Stand 27.11.2026, Beschreibung docs/SOUND-ENGINE.md).
 *
 * Volle Genauigkeit heißt hier: dieselbe Rechnung wie die Vorlage. Die Vorlage rechnet Audio in 32-bit float und nur
 * Hüllkurven, Glide, LFO-Phase und Clipper in double; genau so dieser Port (gleiche Formeln, Konstanten, Reihenfolge).
 * Siehe README.md.
 *
 * Schalter (beim Bauen):
 *   SH101_WITH_POLY    1 = Poly-Modus mit 6 Stimmen (Parameter polymode), 0 = nur eine Stimme (spart Speicher)
 *   SH101_WITH_ARPSEQ  1 = Arpeggiator und Sequencer der Vorlage (96 Schritte), 0 = weg
 */
#ifndef SH101_H
#define SH101_H
#include "baustein.h"
#include <stdint.h>

#ifndef SH101_WITH_POLY
#define SH101_WITH_POLY 1
#endif
#ifndef SH101_WITH_ARPSEQ
#define SH101_WITH_ARPSEQ 1
#endif
#if SH101_WITH_POLY
#define SH101_VOICES 6
#else
#define SH101_VOICES 1
#endif
#define SH101_STEPS 96

/* Parameter in der Reihenfolge der Vorlage (ParamID). Werte im Speicher wie dort: 0..1, ARP_MODE 0..6,
 * ARP_SEQ_SYNC_MODE 0..3. Über baustein_t.set_param immer 0..1 (wird auf den Bereich gestreckt). */
enum {
    SH101_VOLUME, SH101_MASTER_FINE_TUNE, SH101_OCTAVE_TRANSPOSE, SH101_PORTAMENTO_MODE, SH101_PORTAMENTO_INTENSITY,
    SH101_PORTAMENTO_LINEAR, SH101_POLYMODE,
    SH101_LFO_RATE, SH101_LFO_WAVEFORM, SH101_LFO_TRIGGER, SH101_LFO_SYNC, SH101_LFO_INVERTED,
    SH101_DCO_LFO_VALUE, SH101_DCO_LFO_VALUE_SNAP, SH101_DCO_PWM_VALUE, SH101_DCO_PWM_MODE, SH101_DCO_RANGE,
    SH101_PULSE_VOLUME, SH101_SAW_VOLUME, SH101_SUB_OSC_VOLUME, SH101_SUB_OSC_MODE, SH101_NOISE_VOLUME,
    SH101_WHITE_NOISE_ENABLED, SH101_NOISE_FLOOR,
    SH101_FILTER_CUTOFF, SH101_FILTER_RESONANCE, SH101_FILTER_ENVELOPE_VALUE, SH101_FILTER_MODULATION_VALUE,
    SH101_FILTER_KEYBOARD_VALUE, SH101_FILTER_ENVELOPE_VALUE_FULL_RANGE, SH101_FILTER_VOLUME_CORRECTION,
    SH101_FILTER_12DB,
    SH101_VCA_MODE, SH101_ADSR_MODE, SH101_ADSR_ATTACK, SH101_ADSR_DECAY, SH101_ADSR_SUSTAIN, SH101_ADSR_RELEASE,
    SH101_ADSR_DECLICK,
    SH101_CONTROL_BENDER_FILTER, SH101_CONTROL_BENDER_DCO, SH101_CONTROL_BENDER_MODULATION,
    SH101_CONTROL_VELOCITY_VOLUME, SH101_CONTROL_VELOCITY_ENVELOPE,
    SH101_FM_PULSE, SH101_FM_SAW, SH101_FM_SUB_OSC, SH101_FM_NOISE, SH101_FM_INTENSITY,
    SH101_DISTORTION_AMOUNT,
    SH101_ARP_SEQ_SYNC_ENABLED, SH101_ARP_SEQ_TEMPO, SH101_ARP_SEQ_LENGTH, SH101_ARP_ENABLED, SH101_ARP_MODE,
    SH101_ARP_SEQ_SYNC_MODE, SH101_ARP_SEQ_HOLD,
    SH101_SEQ_ENABLED, SH101_ARP_SEQ_SHUFFLE,
    SH101_PITCH_WHEEL, SH101_MODULATION_WHEEL,
    SH101_PARAM_COUNT
};

typedef struct { int note_offset; uint8_t pause, slide, slide_adsr; float velocity, length; } sh101_step_t;

typedef struct { float base, coef, g, value; } sh101_smoother_t;

typedef struct {
    int n0, n1;
    float c[8];
    float m[16];
} sh101_halfband_t;

typedef struct { float buf[16]; int pos; } sh101_blep_t;
typedef struct { float sr, phase, sign; sh101_blep_t blep; } sh101_pulse_t;

typedef struct {
    float sr, inv_sr, freq, p0, p1, p2;
    sh101_pulse_t pulse, sub, sub2;
    sh101_blep_t saw;
    float saw_phase, out_pulse, out_saw, out_sub, out_noise;
    float wander, wander_coef, wander_g;
    int32_t seed;
    float noise_a, n1, n2;
} sh101_vco_t;

typedef struct {
    int state;
    double value, k, fall, last_a, last_d, last_r, c_a, c_d, c_r;
    int skip_fall;
    double start_level;
} sh101_env_t;

typedef struct {
    float sr, limit, s0, s1, s2, s3, u_prev, fm_smoothed;
    sh101_smoother_t note_s;
    sh101_halfband_t up1, up2, down1, down2;
} sh101_ladder_t;

typedef struct {
    float sr;
    sh101_vco_t vco;
    sh101_ladder_t filter;
    sh101_env_t vca_env, filter_env;
    sh101_smoother_t cutoff_s, vel_s, bend_s;
    double porta_current, porta_unit, porta_last_i, porta_exp, porta_lin;
    int porta_last_linear, porta_active;
    float hp_a, hp_s;
    int active, released, legato_from_release, no_retrigger, snap_velocity;
    int note;
    float note_pitch, velocity, pitch;
    int tail, tail_length;
    uint64_t last_on;
} sh101_voice_t;

/* Werte, die alle Stimmen lesen (Shared der Vorlage). */
typedef struct {
    float pwm; int pwm_mode, sub_mode;
    float pulse_vol, saw_vol, sub_vol, noise_vol;
    int white_noise, noise_floor;
    float range_semis, octave_semis, fine_tune, dco_lfo;
    float cutoff, resonance, env_amount, filter_lfo, keyboard, keyboard_s;
    int volume_correction, filter12;
    float fm_intensity_s, fm_intensity; int fm_source;
    float attack, decay, sustain, release, attack_s, decay_s, sustain_s;
    int vca_env_mode, declick, adsr_mode;
    float bender_filter, bender_dco, bender_mod, pitch_wheel, mod_wheel, vel_volume, vel_envelope;
    int porta_mode; float porta_intensity; int porta_linear;
    float lfo, lfo_tri, lfo_sine, drift;
    int lfo_wrapped;
} sh101_shared_t;

typedef struct {
    float sample_rate;
    double tempo;            /* BPM für synchronisierte Raten */
    float values[SH101_PARAM_COUNT];
    uint8_t dirty[SH101_PARAM_COUNT];
    int any_dirty;
    sh101_shared_t s;
    sh101_voice_t voices[SH101_VOICES];
    int voice_count;
    uint64_t counter;
    sh101_smoother_t sm_a, sm_d, sm_s, sm_fm, sm_kbd, sm_vol;
    float volume;
    float d_ph0, d_ph1, d_ph2, d_rate;
    double lfo_phase, lfo_inc, lfo_hz, sh_value;
    int lfo_wave; float lfo_invert;
    int32_t lfo_seed; float lfo_n1, lfo_n2, lfo_noise_a;
    float lfo_rate_param; int lfo_sync, lfo_trigger;
    int lfo_gate_armed, lfo_gate_done;
    float drive;
    sh101_halfband_t dist_up1, dist_up2, dist_up3, dist_dn1, dist_dn2, dist_dn3;
    double adaa_prev;
    uint32_t rng;            /* Zufall für S&H, LFO-Trigger, Random-Arp (Vorlage: Systemzufall) */
    int held[128], held_n;
#if SH101_WITH_ARPSEQ
    int arp_notes[128], arp_n;
    int arp_sorted[256], arp_sorted_n;
    int arp_index;
    sh101_step_t steps[SH101_STEPS];
    int seq_length, step_index, current_step;
    double clock_phase, step_time, clock_inc;
    float arp_rate_param;
    int shuffle_pending; double shuffle_time; int step_parity;
    int gate_closed, seq_first, seq_record, record_index, current_step_display;
#endif
} sh101_t;

/* Direkter Zugang (zusätzlich zu baustein_t) */
void sh101_init(sh101_t *e, double sample_rate);
void sh101_set(sh101_t *e, unsigned param, float raw);   /* Wert im Bereich der Vorlage (0..max) */
float sh101_get(const sh101_t *e, unsigned param);
void sh101_note_on(sh101_t *e, int note, float velocity); /* velocity 0..1, 0 = aus */
void sh101_note_off(sh101_t *e, int note);
void sh101_all_notes_off(sh101_t *e);
void sh101_pitch_bend(sh101_t *e, float bend);            /* -1..1 */
void sh101_mod_wheel(sh101_t *e, float mod);              /* 0..1 */
void sh101_prepare(sh101_t *e);                           /* Parameter übernehmen (Blockanfang) */
float sh101_sample(sh101_t *e);                           /* ein Sample, mono */
float sh101_param_max(unsigned param);
float sh101_param_default(unsigned param);
const char *sh101_param_key(unsigned param);              /* Attributname der Presetdateien */
#if SH101_WITH_ARPSEQ
void sh101_set_step(sh101_t *e, int index, const sh101_step_t *step);
#endif

extern const baustein_t sh101_baustein;
#endif
