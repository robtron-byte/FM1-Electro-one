/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * One synth area (rhythm sounds, strings, bass): an MKS-50 sound plus a
 * step sequencer with notes, chords (type + inversion), accents and ties.
 * Live keys can run through an arpeggiator.
 * Each pattern has its own step length, from a 16th up to eight bars, so the
 * strings can run long chord sequences (e.g. 8 steps of one bar each).
 * Audio-owned; the engine applies queued edits. */
#ifndef EL_SYNTH_TRACK_H
#define EL_SYNTH_TRACK_H
#include "synth.h"
#include "arp.h"

enum { ST_STEPS = 32, ST_PATTERNS = 4, ST_NO_CHORD = 0 };   /* slots A-D */
enum { ST_ON = 1, ST_ACCENT = 2, ST_TIE = 4 };      /* step flags */
/* Bits 3-5 of the flags: the step's condition (EL_COND_*, docs/adr/0025). */
#define ST_COND(flags) (((unsigned)(flags) >> 3) & 7u)
#define ST_COND_MASK 0x38u
/* The step's probability (EL_PROBS): bits 6-7 of the flags (low) and of the
 * chord byte (high; the chord uses bits 0-5). */
#define ST_PROB(s) ((((unsigned)(s)->flags >> 6) & 3u) | (((unsigned)(s)->chord >> 6) & 3u) << 2)
#define ST_SET_PROB(s, v) ((s)->flags = (uint8_t)(((s)->flags & 0x3fu) | ((unsigned)(v) & 3u) << 6), \
                           (s)->chord = (uint8_t)(((s)->chord & 0x3fu) | (((unsigned)(v) >> 2) & 3u) << 6))
/* Step lengths: 1/16, 1/8, 1/4, 1/2, 1 bar, 2, 4, 8 bars (in 16ths: 1..128).
 * With 32 steps a pattern can run up to 256 bars (docs/adr/0027). */
enum { ST_RATE_16TH, ST_RATE_8TH, ST_RATE_QUARTER, ST_RATE_HALF, ST_RATE_BAR, ST_RATE_2BARS, ST_RATE_4BARS, ST_RATE_8BARS, ST_RATES };
/* chord byte: low nibble = type (0 single note, 1..8 chord memory slot + 1),
 * bits 4-5 = inversion 0..2 (lowest notes moved up an octave). */
#define ST_CHORD_TYPE(c) ((unsigned)(c) & 15u)
#define ST_CHORD_INV(c) (((unsigned)(c) >> 4) & 3u)

typedef struct {
    uint8_t note;   /* MIDI note */
    uint8_t flags;  /* ST_ON | ST_ACCENT | ST_TIE */
    uint8_t chord;  /* see ST_CHORD_TYPE / ST_CHORD_INV */
    /* A live-recorded STRINGS note (N6, S8): how long it was held, in 32nds
     * (1..255); it sounds that long, over the next steps, beside them. 0: the
     * step plays with the pattern's gate (store version 26). */
    uint8_t len;
} st_step_t;
enum { ST_LONG_NOTES = 8, ST_REC_HELD = 8 };

typedef struct {
    uint8_t length;            /* 1..32 */
    uint8_t gate;              /* note length in percent of a step, 10..100 */
    uint8_t rate;              /* step length, ST_RATE_* */
    st_step_t step[ST_STEPS];
} st_pattern_t;

/* A slot's sound: engine, what the area shows as its sound (factory index
 * or EL_USER_SOUND + slot) and the values. */
typedef struct {
    uint8_t engine, preset;
    uint8_t v[SYN_VALUES];
} st_sound_t;

/* No step switched on. */
static inline int st_pattern_empty(const st_pattern_t *p)
{
    for (unsigned i = 0; i < ST_STEPS; ++i) if (p->step[i].flags & ST_ON) return 0;
    return 1;
}
/* A live take in STRINGS onto an empty pattern (N6, docs/adr/0027): the
 * notes start on the nearest 8th and keep the length they were held; the
 * pattern runs 4 bars of 8ths meanwhile and is cut to the whole bars the
 * take used when REC ends (st_take_end). */
static inline int st_live_grid_needed(const st_pattern_t *p, unsigned area, int live)
{
    return live && (area == MKS_AREA_STRINGS || area == MKS_AREA_BASS) && st_pattern_empty(p);
}
/* The take's grid (N6, N16): STRINGS 8ths (4 bars), BASS 16ths (2 bars);
 * past them the notes move one step coarser (STRINGS quarters, 8 bars; BASS 8ths, 4 bars). */
static inline unsigned st_take_rate(unsigned area) { return area == MKS_AREA_BASS ? ST_RATE_16TH : ST_RATE_8TH; }

enum { ST_CHAIN = 4 };   /* links of a pattern chain (S16) */
typedef struct {
    syn_part_t part;           /* MKS-50 or JP-6, chosen with the sound */
    st_pattern_t pattern[ST_PATTERNS];
    /* Each slot's sound: loaded when the slot starts, edits go to the slot
     * that plays (docs/adr/0024). */
    st_sound_t sound[ST_PATTERNS];
    uint8_t area, preset, current, pos, playing, muted, recording, level_value;
    uint8_t next;              /* slot to switch to at the next bar */
    /* Pattern chain (S16): the slots played one after the other, a pass of
     * each; where the chain stands; hold: st_step_at replays a step. */
    uint8_t chain[ST_CHAIN], chain_len, chain_pos, chain_hold;
    int8_t transpose;          /* added to the notes of the steps (keys in TRANSPOSE) */
    uint8_t chord;             /* chord for live keys (0 = none) */
    uint8_t last_note;         /* used when a step is switched on */
    int16_t sounding[MKS_CHORD_NOTES];
    uint8_t sounding_count, tied, tick;
    uint8_t pass;              /* passes of the current pattern since it started */
    uint32_t rnd;              /* the step probabilities' dice */
    float gate_countdown, step_frames, since_step;
    float sixteenth_frames;    /* as the transport last said */
    float arp_seq_left;        /* STRINGS with ARP: frames the step's chord is arpeggiated still */
    /* Notes of steps with a length (N6): sounding beside the steps, each until its time is up. */
    int16_t long_note[ST_LONG_NOTES];
    float long_left[ST_LONG_NOTES];
    uint8_t long_count;
    /* A live STRINGS take (N6): the keys held, the step each went to and how long it is held. */
    uint8_t rec_note[ST_REC_HELD], rec_step[ST_REC_HELD], rec_count, grow;
    float rec_frames[ST_REC_HELD];
    arp_t arp;
    volatile uint32_t revision;
} st_track_t;

/* Control task before audio. */
void st_init(st_track_t *t, unsigned area, unsigned voices, uint32_t seed);

/* Audio side. */
/* Every transport 16th: step is its absolute number since Start. A slot
 * chosen with next starts on the next bar. */
EL_DSP_CODE void st_step(st_track_t *t, uint32_t step, float sixteenth_frames);
/* Repeat punch-in: play the step at 16th target (wrapped to the length)
 * instead, the playhead going on as if not; no slot change meanwhile. */
EL_DSP_CODE void st_step_at(st_track_t *t, uint32_t step, float sixteenth_frames, unsigned target);
EL_DSP_CODE void st_rewind(st_track_t *t);
/* Switch to slot now and load its sound when another one is loaded. */
EL_DSP_CODE void st_enter(st_track_t *t, unsigned slot);
/* The part's sound (as loaded and edited) into the playing slot; all = 1:
 * into every slot. */
EL_DSP_CODE void st_keep_sound(st_track_t *t, int all);
/* After the sound changed: a sound with chord memory sets the track's chord;
 * a chord that came from the old sound (old = its chord memory) ends when the
 * new one has none, a chord picked by hand stays. */
EL_DSP_CODE void st_follow_chord(st_track_t *t, unsigned old);
/* Pattern chain (S16): links 0..ST_CHAIN (under 2: none), slots 2 bits each. */
EL_DSP_CODE void st_chain(st_track_t *t, unsigned links, unsigned slots);
EL_DSP_CODE int st_frames_until(const st_track_t *t, int limit);
EL_DSP_CODE void st_render(st_track_t *t, float *left, float *right, float *dly, float *rev, int frames, float bpm);
/* Live note from the keys or MIDI: plays (with the live chord) and records
 * when armed and running. velocity 0 = note off. */
EL_DSP_CODE void st_live(st_track_t *t, int note, float velocity);
EL_DSP_CODE void st_mute(st_track_t *t, int on);
EL_DSP_CODE void st_arp_set(st_track_t *t, unsigned param, int value);
/* REC ended for this track: the held keys keep their lengths so far; a take
 * that started on an empty STRINGS pattern is cut to the whole bars it used
 * (1-4, N6). */
EL_DSP_CODE void st_take_end(st_track_t *t);

/* Notes of a step: root plus chord type and inversion (up to MKS_CHORD_NOTES). */
unsigned st_chord_notes(int root, unsigned chord, int16_t *out);
extern const char *const st_rate_names[ST_RATES];

/* Control side: groove generator for an area (writes a pattern).
 * style: 0 KRAFT, 1 ROTHER, 2 ROBOT, 3 ACID, 4 MIAMI; strings: 0 PADS (one chord per
 * bar, 4 bars), 1 SLOW (one chord per 2 bars, 8 bars), 2 PULSE (8th chords,
 * changes every bar), 3 STABS (16th stabs), 4 LONG (one chord per 4 bars, 16 bars). length is ignored for strings. */
/* Variation of a pattern: amount 0..127 changes notes (to ones the
 * pattern already uses or octaves), moves notes by a step, flips ties and
 * accents; strings change inversions and swap chord types. The first note
 * stays. */
void st_vary(st_pattern_t *p, unsigned area, unsigned amount, uint32_t seed);
/* Melodic styles (rhythm, bass): 0 KRAFT, 1 ROTHER, 2 ROBOT, 3 ACID, 4 MIAMI. */
enum { ST_STYLES = 5, ST_STRING_STYLES = 5 };
unsigned st_styles(unsigned area);
const char *st_style_name(unsigned area, unsigned style);
/* harmony: DR_HARMONY_ROTHER / _KRAFT / _ELECTRO / _EGYPT (strings chord family). */
void st_generate(st_pattern_t *out, unsigned area, unsigned style, unsigned density, uint32_t seed, unsigned length,
                 unsigned harmony);
#endif
