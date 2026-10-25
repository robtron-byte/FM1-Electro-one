/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro engine: transport plus the instruments that follow it.
 * The 808 drum machine, three synth areas (MKS-50 / JP-6) with step
 * sequencer and arpeggiator, shared effects and the mix bus. */
#ifndef EL_ELECTRO_H
#define EL_ELECTRO_H
#include "el_config.h"
#include "el_queue.h"
#include "transport.h"
#include "drums.h"
#include "synth_track.h"
#include "fx.h"
#include "mixbus.h"

enum el_command {
    EL_CMD_START,        /* c = 1 when it came from MIDI (follow its clock) */
    EL_CMD_STOP,
    EL_CMD_CONTINUE,
    EL_CMD_BPM,          /* value = BPM x 10 */
    EL_CMD_CLOCK_SOURCE, /* a = EL_CLOCK_INTERNAL / EL_CLOCK_MIDI */
    EL_CMD_MIDI_TICK,    /* one F8 */
    EL_CMD_MASTER,       /* value = 0..127 */
    EL_CMD_DRUM_HIT,     /* a = part, value = velocity 1..127; records when armed */
    /* Synth areas: a = track 0 rhythm, 1 strings, 2 bass (EL_TRACKS). */
    EL_CMD_NOTE,         /* b = note, value = velocity (0 = off); records when armed, c = 1: never */
    EL_CMD_STEP_TOGGLE,  /* b = step: on/off with the last played note */
    EL_CMD_STEP_FLAG,    /* b = step, c = ST_ACCENT / ST_TIE: toggle (switches the step on) */
    EL_CMD_STEP_NOTE,    /* b = step, value = semitones to add (switches the step on) */
    EL_CMD_STEP_CHORD,   /* b = step, value = chord 0..8 */
    EL_CMD_STEP_SET,     /* b = step, c = note, value = flags | chord << 8 */
    EL_CMD_STEP_WRITE,   /* step record: as STEP_SET, and the pattern ends after b */
    EL_CMD_STEP_TIE,     /* b = step, c = 1: tie it into the next step, 0: no tie */
    EL_CMD_TRACK_TRANSPOSE, /* value = semitones added to every step on playback */
    EL_CMD_TRACK_LENGTH, /* b = 1..32 */
    EL_CMD_TRACK_GATE,   /* b = 10..100 */
    EL_CMD_TRACK_PARAM,  /* b = mks parameter, value */
    EL_CMD_TRACK_PRESET, /* b = sound index, c = engine: loads the staged patch */
    EL_CMD_TRACK_PATTERN,/* b = pattern slot: copies the staged pattern */
    EL_CMD_TRACK_MUTE,   /* b = on */
    EL_CMD_TRACK_LEVEL,  /* value = 0..127 */
    EL_CMD_TRACK_CHORD,  /* b = live chord 0..8 */
    EL_CMD_TRACK_RATE,   /* b = step length ST_RATE_* */
    EL_CMD_STEP_INV,     /* b = step, value = inversion 0..2 */
    EL_CMD_TRACK_CLEAR,  /* clear the current pattern */
    EL_CMD_TRACK_ARP,    /* b = ARP_* setting, value */
    EL_CMD_RECORD_AREA,  /* a = area 0 beat .. 3 bass, b = on; other areas disarm */
    EL_CMD_FX,           /* a = fx parameter, value */
    EL_CMD_MIX,          /* a = 1: mix and master processing on, 0: off (A/B) */
    EL_CMD_TRACK_SELECT, /* a = track, b = slot A-D; c = 1 switches now, else at the next bar */
    EL_CMD_MIDI_CHANNEL, /* a = area, value = channel 0..15 or EL_MIDI_OFF */
    EL_CMD_OPTION,       /* a = EL_OPT_*, value */
    EL_CMD_PUNCH         /* a = EL_PUNCH_* bit, b = 1 held / 0 let go */
};
/* Punch-in effects, active while held (HOME, black keys 5-7). */
enum { EL_PUNCH_STUTTER = 1, EL_PUNCH_STUTTER32 = 2, EL_PUNCH_THROW = 4 };

enum { EL_TRACKS = 3, EL_AREAS = 4 };
/* MIDI channel per area (0-based); EL_MIDI_OFF ignores the area's notes.
 * Two areas on one channel both play. */
enum { EL_MIDI_OFF = 16 };
/* Settings made once in the OPTIONS menu (HOME + SAVE), kept in Flash. */
enum {
    EL_OPT_MIDI_CLOCK,  /* 1: follow MIDI clock and Start/Stop, 0: ignore them */
    EL_OPT_MASTER,      /* 1: mix bus and master processing on, 0: off */
    EL_OPT_OUT_LEVEL,   /* 0..127 output level (127 = full) */
    EL_OPT_START,       /* EL_START_*: slots at power-on (where they were, or A) */
    EL_OPT_HINTS,       /* 1: help line at the bottom of the screen */
    /* Master compressor (in the MASTER FX chain, before saturation and limiter). */
    EL_OPT_COMP_THRESHOLD,  /* 0..30: threshold -0 .. -30 dB */
    EL_OPT_COMP_RATIO,      /* index into el_comp_ratios */
    EL_OPT_COMP_ATTACK,     /* index into el_comp_attacks (ms) */
    EL_OPT_COMP_RELEASE,    /* index into el_comp_releases (ms) */
    EL_OPT_COMP_MAKEUP,     /* 0..24: makeup 0 .. +12 dB in half dB */
    EL_OPT_REC_STEP,        /* bit per synth track: REC records step by step (SH-101), else live */
    EL_OPTIONS = 12     /* stored size, room for more */
};
enum { EL_COMP_RATIOS = 8, EL_COMP_ATTACKS = 8, EL_COMP_RELEASES = 8 };
extern const float el_comp_ratios[EL_COMP_RATIOS];
extern const uint16_t el_comp_attacks[EL_COMP_ATTACKS], el_comp_releases[EL_COMP_RELEASES];
/* Largest value of an option. */
int el_option_max(unsigned option);
enum { EL_START_LAST, EL_START_SLOT_A };
enum { EL_AREA_BEAT, EL_AREA_RHYTHM, EL_AREA_STRINGS, EL_AREA_BASS };

typedef struct {
    el_transport_t transport;
    dr_machine_t drums;
    st_track_t track[EL_TRACKS];
    el_fx_t fx;
    el_mixbus_t mix;
    uint8_t staged_patch[SYN_VALUES];      /* control writes, a command copies */
    st_pattern_t staged_pattern;
    volatile uint32_t staged_posted, staged_taken;
    el_queue_t queue;       /* from the control task */
    el_queue_t midi_queue;  /* from the MIDI routing task */
    float master;
    volatile uint8_t midi_channel[EL_AREAS];   /* read by the MIDI routing task */
    volatile uint8_t option[EL_OPTIONS];
    /* Punch-in: held effects, stutter phase (16ths, reset on every step),
     * dry level and how long the reverb throw has been fed. */
    volatile uint8_t punch;
    float stutter_phase, stutter_gain, dry_gain, throw_fed;
    int stutter_sync;
    volatile uint32_t blocks;
} el_engine_t;

/* Control task, before audio starts. */
void el_init(el_engine_t *e);
/* Audio side or before audio starts: the master compressor from the options. */
void el_apply_comp(el_engine_t *e);
/* Control task, before audio starts: generate every style slot (drums and
 * the three synth parts, in the style's harmony) and load the default
 * sounds. User slots are left alone. */
void el_generate_all(el_engine_t *e, uint32_t seed);
/* Control task: the three synth parts (rhythm, strings, bass) that go with
 * a drum groove, from its style. */
void el_generate_parts(const dr_pattern_t *d, uint32_t seed, st_pattern_t out[EL_TRACKS]);
/* Control task. Returns 0, or -1 when the queue is full. */
int el_post(el_engine_t *e, uint8_t type, uint8_t a, int16_t value);
int el_post3(el_engine_t *e, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value);
/* Control task: load a factory sound / a whole pattern into a synth track.
 * Returns 0, -1 queue full, -2 while the previous one is still pending. */
int el_load_preset(el_engine_t *e, unsigned track, unsigned preset);
/* Same with any values (a user sound): id is what the track shows as its
 * sound (factory index, or EL_USER_SOUND + slot). */
int el_load_sound(el_engine_t *e, unsigned track, unsigned id, unsigned engine, const uint8_t *values);
enum { EL_USER_SOUND = 200, EL_USER_SOUNDS = 32, EL_SOUND_NAME = 10 };
/* A user sound slot (stored by fm1/el_store.c). */
typedef struct {
    char name[EL_SOUND_NAME + 1];
    uint8_t engine, used;
    uint8_t v[SYN_VALUES];
} el_sound_t;
int el_load_pattern(el_engine_t *e, unsigned track, unsigned slot, const st_pattern_t *p);
/* MIDI routing task: its own queue, so the two tasks never share a producer
 * side. Handles clock, Start/Continue/Stop and the notes of each area on
 * its channel (midi_channel; default drums 10, RHYTHM 1, STRINGS 2, BASS 3). */
int el_midi_input(el_engine_t *e, const uint8_t *message, size_t bytes);
/* Audio interrupt: fill frames of interleaved stereo signed Q23. */
EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames);
/* Same, as float -1..1 interleaved (host tools). */
EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames);
#endif
