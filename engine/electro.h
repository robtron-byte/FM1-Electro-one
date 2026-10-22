/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro engine: transport plus the instruments that follow it.
 * Today: the 808 drum machine. Next: MKS-50 / JP-6 voice engines with
 * arpeggiator, step sequencer, note looper and shared delay/reverb. */
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
    EL_CMD_NOTE,         /* b = note, value = velocity (0 = off); records when armed */
    EL_CMD_STEP_TOGGLE,  /* b = step: on/off with the last played note */
    EL_CMD_STEP_FLAG,    /* b = step, c = ST_ACCENT / ST_TIE: toggle (switches the step on) */
    EL_CMD_STEP_NOTE,    /* b = step, value = semitones to add (switches the step on) */
    EL_CMD_STEP_CHORD,   /* b = step, value = chord 0..8 */
    EL_CMD_STEP_SET,     /* b = step, c = note, value = flags | chord << 8 */
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
    EL_CMD_RECORD_AREA,  /* a = area 0 beat .. 3 bass, b = on; other areas disarm */
    EL_CMD_FX,           /* a = fx parameter, value */
    EL_CMD_MIX,          /* a = 1: mix and master processing on, 0: off (A/B) */
    EL_CMD_SLOT_COPY     /* a = source slot, b = destination: drums and all synth
                          * parts; c = 1 keeps the destination's name */
};

enum { EL_TRACKS = 3, EL_AREAS = 4 };
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
    volatile uint32_t blocks;
    volatile uint32_t slot_copies;       /* bumps after every EL_CMD_SLOT_COPY */
} el_engine_t;

/* Control task, before audio starts. */
void el_init(el_engine_t *e);
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
int el_load_pattern(el_engine_t *e, unsigned track, unsigned slot, const st_pattern_t *p);
/* MIDI routing task: its own queue, so the two tasks never share a producer
 * side. Handles clock, Start/Continue/Stop and drum notes on channel 10. */
int el_midi_input(el_engine_t *e, const uint8_t *message, size_t bytes);
/* Audio interrupt: fill frames of interleaved stereo signed Q23. */
EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames);
/* Same, as float -1..1 interleaved (host tools). */
EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames);
#endif
