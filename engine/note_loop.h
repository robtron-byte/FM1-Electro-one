/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Note looper of one synth area. Works like the apps' loop recorder (armed,
 * starts with the first note, records exactly 1/2/4/8 bars on the beat
 * clock and plays on without a gap), but loops the notes instead of audio:
 * an audio loop of a few bars would need more RAM than the FM-1 has left.
 * The loop runs freely over the patterns, on the transport's bar grid,
 * with overdub. Positions are 1/48 of a 16th. Audio-owned. */
#ifndef EL_NOTE_LOOP_H
#define EL_NOTE_LOOP_H
#include "el_config.h"

enum { LOOP_EVENTS = 192, LOOP_UNITS = 48, LOOP_BAR_CHOICES = 4 };
enum { LOOP_EMPTY, LOOP_ARMED, LOOP_RECORDING, LOOP_PLAYING, LOOP_OVERDUB, LOOP_STATES };
/* Commands (EL_CMD_TRACK_LOOP b): RECORD steps empty -> armed -> (first
 * note) recording -> playing <-> overdub; armed again cancels. */
enum { LOOP_CMD_RECORD, LOOP_CMD_CLEAR, LOOP_CMD_BARS, LOOP_CMD_PLAY };

typedef struct { uint16_t pos; uint8_t note, velocity; } loop_event_t;   /* velocity 0 = off */
/* Plays a note of the loop (velocity 0 = off). */
typedef void (*loop_play_fn)(void *ctx, int note, int velocity);

typedef struct {
    loop_event_t ev[LOOP_EVENTS];
    uint16_t count, next;
    uint8_t state, bars, muted, clocked;
    uint32_t origin, step;        /* absolute 16ths: loop start, current */
    float since, sixteenth;       /* frames into the current 16th, its length */
    uint8_t sounding[16];         /* notes the loop has started */
    uint8_t recorded[16];         /* recorded note-ons still waiting for their off */
} note_loop_t;

extern const uint8_t loop_bar_choices[LOOP_BAR_CHOICES];
extern const char *const loop_state_names[LOOP_STATES];

void loop_init(note_loop_t *l);
static inline unsigned loop_bars(const note_loop_t *l) { return loop_bar_choices[l->bars % LOOP_BAR_CHOICES]; }
EL_DSP_CODE void loop_command(note_loop_t *l, unsigned cmd, int value, loop_play_fn play, void *ctx);
/* A note the player started or stopped (keys, MIDI or arpeggiator). */
EL_DSP_CODE void loop_note(note_loop_t *l, int note, int velocity);
/* Transport 16th fired. */
EL_DSP_CODE void loop_sync(note_loop_t *l, uint32_t step, float sixteenth_frames, loop_play_fn play, void *ctx);
EL_DSP_CODE int loop_frames_until(const note_loop_t *l, int limit);
EL_DSP_CODE void loop_advance(note_loop_t *l, int frames, loop_play_fn play, void *ctx);
/* Transport stopped or restarted: notes off, a take in progress ends, the
 * loop starts again on bar 1. */
EL_DSP_CODE void loop_rewind(note_loop_t *l, loop_play_fn play, void *ctx);
EL_DSP_CODE void loop_silence(note_loop_t *l, loop_play_fn play, void *ctx);
/* Position in the loop, 0..1 (display). */
float loop_progress(const note_loop_t *l);
#endif
