/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * One synth area (rhythm sounds, strings, bass): an MKS-50 sound plus a
 * step sequencer with notes, chords (type + inversion), accents and ties.
 * Live keys can run through an arpeggiator.
 * Each pattern has its own step length, from a 16th up to two bars, so the
 * strings can run long chord sequences (e.g. 8 steps of one bar each).
 * Audio-owned; the engine applies queued edits. */
#ifndef EL_SYNTH_TRACK_H
#define EL_SYNTH_TRACK_H
#include "synth.h"
#include "arp.h"

enum { ST_STEPS = 32, ST_PATTERNS = 24, ST_NO_CHORD = 0 };
enum { ST_ON = 1, ST_ACCENT = 2, ST_TIE = 4 };      /* step flags */
/* Step lengths: 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars (in 16ths: 1..32). */
enum { ST_RATE_16TH, ST_RATE_8TH, ST_RATE_QUARTER, ST_RATE_HALF, ST_RATE_BAR, ST_RATE_2BARS, ST_RATES };
/* chord byte: low nibble = type (0 single note, 1..8 chord memory slot + 1),
 * bits 4-5 = inversion 0..2 (lowest notes moved up an octave). */
#define ST_CHORD_TYPE(c) ((unsigned)(c) & 15u)
#define ST_CHORD_INV(c) (((unsigned)(c) >> 4) & 3u)

typedef struct {
    uint8_t note;   /* MIDI note */
    uint8_t flags;  /* ST_ON | ST_ACCENT | ST_TIE */
    uint8_t chord;  /* see ST_CHORD_TYPE / ST_CHORD_INV */
} st_step_t;

typedef struct {
    uint8_t length;            /* 1..32 */
    uint8_t gate;              /* note length in percent of a step, 10..100 */
    uint8_t rate;              /* step length, ST_RATE_* */
    st_step_t step[ST_STEPS];
} st_pattern_t;

typedef struct {
    syn_part_t part;           /* MKS-50 or JP-6, chosen with the sound */
    st_pattern_t pattern[ST_PATTERNS];
    uint8_t area, preset, current, pos, playing, muted, recording, level_value;
    uint8_t chord;             /* chord for live keys (0 = none) */
    uint8_t last_note;         /* used when a step is switched on */
    int16_t sounding[MKS_CHORD_NOTES];
    uint8_t sounding_count, tied, tick;
    float gate_countdown, step_frames, since_step;
    arp_t arp;
    volatile uint32_t revision;
} st_track_t;

/* Control task before audio. */
void st_init(st_track_t *t, unsigned area, unsigned voices, uint32_t seed);

/* Audio side. */
/* Every transport 16th: step is its absolute number since Start. */
EL_DSP_CODE void st_step(st_track_t *t, unsigned pattern, uint32_t step, float sixteenth_frames);
EL_DSP_CODE void st_rewind(st_track_t *t);
EL_DSP_CODE int st_frames_until(const st_track_t *t, int limit);
EL_DSP_CODE void st_render(st_track_t *t, float *left, float *right, float *dly, float *rev, int frames, float bpm);
/* Live note from the keys or MIDI: plays (with the live chord) and records
 * when armed and running. velocity 0 = note off. */
EL_DSP_CODE void st_live(st_track_t *t, int note, float velocity);
EL_DSP_CODE void st_mute(st_track_t *t, int on);
EL_DSP_CODE void st_arp_set(st_track_t *t, unsigned param, int value);

/* Notes of a step: root plus chord type and inversion (up to MKS_CHORD_NOTES). */
unsigned st_chord_notes(int root, unsigned chord, int16_t *out);
extern const char *const st_rate_names[ST_RATES];

/* Control side: groove generator for an area (writes a pattern).
 * style: 0 KRAFT, 1 ROTHER, 2 ROBOT, 3 ACID, 4 MIAMI; strings: 0 PADS (one chord per
 * bar, 4 bars), 1 SLOW (one chord per 2 bars, 8 bars), 2 PULSE (8th chords,
 * changes every bar), 3 STABS (16th stabs). length is ignored for strings. */
/* Melodic styles (rhythm, bass): 0 KRAFT, 1 ROTHER, 2 ROBOT, 3 ACID, 4 MIAMI. */
enum { ST_STYLES = 5, ST_STRING_STYLES = 4 };
unsigned st_styles(unsigned area);
const char *st_style_name(unsigned area, unsigned style);
/* harmony: DR_HARMONY_ROTHER / _KRAFT / _ELECTRO (strings chord family). */
void st_generate(st_pattern_t *out, unsigned area, unsigned style, unsigned density, uint32_t seed, unsigned length,
                 unsigned harmony);
#endif
