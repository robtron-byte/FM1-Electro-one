/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * One synth area (rhythm sounds, strings, bass): an MKS-50 sound plus a
 * 16-pattern step sequencer with notes, chords, accents and ties.
 * Audio-owned; the engine applies queued edits. */
#ifndef EL_SYNTH_TRACK_H
#define EL_SYNTH_TRACK_H
#include "mks.h"

enum { ST_STEPS = 32, ST_PATTERNS = 16, ST_NO_CHORD = 0 };
enum { ST_ON = 1, ST_ACCENT = 2, ST_TIE = 4 };      /* step flags */

typedef struct {
    uint8_t note;   /* MIDI note */
    uint8_t flags;  /* ST_ON | ST_ACCENT | ST_TIE */
    uint8_t chord;  /* 0 = single note, 1..8 = chord memory slot + 1 */
} st_step_t;

typedef struct {
    uint8_t length;            /* 1..32 */
    uint8_t gate;              /* note length in percent of a step, 10..100 */
    st_step_t step[ST_STEPS];
} st_pattern_t;

typedef struct {
    mks_part_t part;
    st_pattern_t pattern[ST_PATTERNS];
    uint8_t area, preset, current, pos, playing, muted, recording, level_value;
    uint8_t chord;             /* chord for live keys (0 = none) */
    uint8_t last_note;         /* used when a step is switched on */
    int16_t sounding[MKS_CHORD_NOTES];
    uint8_t sounding_count, tied;
    float gate_countdown, step_frames, since_step;
    volatile uint32_t revision;
} st_track_t;

/* Control task before audio. */
void st_init(st_track_t *t, unsigned area, unsigned voices, uint32_t seed);

/* Audio side. */
EL_DSP_CODE void st_step(st_track_t *t, unsigned pattern, float step_frames);
EL_DSP_CODE void st_rewind(st_track_t *t);
EL_DSP_CODE int st_frames_until(const st_track_t *t, int limit);
EL_DSP_CODE void st_render(st_track_t *t, float *left, float *right, float *dly, float *rev, int frames, float bpm);
/* Live note from the keys or MIDI: plays (with the live chord) and records
 * when armed and running. velocity 0 = note off. */
EL_DSP_CODE void st_live(st_track_t *t, int note, float velocity);
EL_DSP_CODE void st_mute(st_track_t *t, int on);

/* Control side: groove generator for an area (writes a pattern).
 * style: 0 KRAFT, 1 ROTHER, 2 ROBOT, 3 ACID (strings: 0 PAD, 1 STABS, 2 PULSE, 3 SWELL). */
enum { ST_STYLES = 4 };
const char *st_style_name(unsigned area, unsigned style);
void st_generate(st_pattern_t *out, unsigned area, unsigned style, unsigned density, uint32_t seed, unsigned length);
#endif
