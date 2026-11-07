/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * 808 drum machine: the 11 circuit-informed TR-808 voices of tr808.c, 16 patterns
 * of up to 32 steps with accent, 32nd rolls, mute/solo, live recording,
 * electro factory grooves and a style-based groove generator.
 *
 * Threading: the audio interrupt owns the kit and writes every pattern.
 * The control task only reads (32-bit loads are tear-free) and edits through
 * dr_post(), which queues commands that the audio side applies in order. */
#ifndef EL_DRUMS_H
#define EL_DRUMS_H
#include "el_config.h"
#include "el_queue.h"
#include "tr808.h"

enum {
    DR_PARTS = D8_NUM,      /* BD SD LT HT RS CP CB CY OH CH ZP PU */
    DR_STEPS = 32,
    DR_PITCH_PARTS = 6,     /* parts with a pitch lock: BD SD RS CB ZP PU */
    DR_PATTERNS = 4,        /* slots A-D (each synth area has its own four too) */
    DR_KITS = 5,
    DR_STYLES = 16,
    DR_NAME = 10
};

typedef struct {
    char name[DR_NAME + 1];
    uint8_t length;            /* 1..32 steps */
    uint8_t swing;             /* 50..75, loaded into the transport on change */
    uint8_t kit;               /* kit, switched to when the pattern starts */
    uint8_t style;             /* style it was generated in */
    uint32_t hit[DR_PARTS];    /* bit n = step n */
    uint32_t roll[DR_PARTS];   /* extra 32nd hit halfway through the step */
    uint32_t accent;           /* 808-style global accent row */
} dr_pattern_t;

enum dr_command {
    DR_CMD_HIT,         /* a = part, b = velocity 1..127, c = record flag, value = semitones (pitch lock) */
    DR_CMD_TOGGLE,      /* a = part, b = step: hit on/off */
    DR_CMD_ROLL,        /* a = part, b = step: roll on/off (sets the hit too) */
    DR_CMD_ACCENT,      /* b = step */
    DR_CMD_CLEAR_PART,  /* a = part (DR_PARTS clears the whole pattern) */
    DR_CMD_LENGTH,      /* b = 1..32 */
    DR_CMD_SWING,       /* b = 50..75 */
    DR_CMD_SELECT,      /* a = pattern; b = 1 switches immediately, else at loop end */
    DR_CMD_COPY,        /* a = source pattern, b = destination (with its sound) */
    DR_CMD_LOAD,        /* a = pattern slot, c = 1 keep its step conditions (else cleared):
                           copy the staging pattern into it (another
                           kit: the slot's sound becomes that kit's factory sound) */
    DR_CMD_MUTE,        /* a = part, b = on */
    DR_CMD_SOLO,        /* a = part, b = on */
    DR_CMD_PARAM,       /* a = part (D8_KIT for kit), b = parameter index, value */
    DR_CMD_KIT,         /* a = kit preset (the playing slot's sound becomes its factory sound) */
    DR_CMD_LEVEL,       /* value = 0..127 */
    DR_CMD_RECORD,      /* b = on */
    DR_CMD_PANIC,
    DR_CMD_AREA_MUTE,   /* b = on: silence the whole beat area */
    DR_CMD_COND,        /* a = part, b = step, value = EL_COND_* (the current pattern) */
    DR_CMD_PROB,        /* a = part, b = step, value = 0..EL_PROBS-1 (the current pattern) */
    DR_CMD_PITCH,       /* a = part, b = step, value = -12..12 semitones (docs/adr/0028; tuned parts only) */
    DR_CMD_VOL          /* a = part, b = step, value = 0 as played, 1..7 a fixed level (docs/adr/0028) */
};

/* Harmony family of a style: chord progressions for the strings. */
enum { DR_HARMONY_ROTHER, DR_HARMONY_KRAFT, DR_HARMONY_ELECTRO, DR_HARMONY_EGYPT, DR_HARMONIES };

typedef struct {
    const char *name;
    uint16_t bpm_x10;
    uint8_t swing, kit, length;
    uint8_t rhythm_style, strings_style, bass_style, harmony;
    const char *row[DR_PARTS + 1];   /* '|'-separated alternatives */
} dr_style_t;
extern const dr_style_t dr_styles[DR_STYLES];

typedef struct {
    tr808_t kit;
    dr_pattern_t pattern[DR_PATTERNS];
    /* Each slot's sound (kit and knobs): set when the slot starts, edits go
     * to the slot that plays (docs/adr/0024). */
    tr808_sound_t sound[DR_PATTERNS];
    /* Step conditions per slot, part and step (EL_COND_*), as three bit
     * planes: bit n of cond[slot][k][part] is bit k of step n's condition. */
    uint32_t cond[DR_PATTERNS][3][DR_PARTS];
    uint32_t prob[DR_PATTERNS][4][DR_PARTS];   /* the probability (EL_PROBS), four bit planes likewise */
    /* Parameter locks per step (docs/adr/0028), as bit planes as well: the
     * pitch in semitones (-12..12, five planes of two's complement; only the
     * tuned parts, dr_pitch_slot) and a fixed level 1..7 (three planes, 0 =
     * the step plays as the pattern has it). */
    uint32_t pitch[DR_PATTERNS][5][DR_PITCH_PARTS];
    uint32_t vol[DR_PATTERNS][3][DR_PARTS];
    int8_t roll_semis[DR_PARTS];
    uint8_t pass;           /* passes of the current pattern since it started */
    el_queue_t queue;
    float level;            /* output gain */
    float roll_countdown;   /* frames until the pending 32nd rolls fire */
    uint32_t roll_pending;  /* parts waiting for their roll hit */
    float roll_velocity[DR_PARTS];
    uint32_t suppress;      /* parts already played live for the next step */
    dr_pattern_t staging;   /* control writes, then DR_CMD_LOAD copies it in */
    volatile uint32_t staging_posted, staging_taken;
    float step_frames;      /* length of the current step */
    float since_step;       /* frames since the current step fired */
    uint32_t random;
    uint16_t mute, solo;
    uint8_t current, next;  /* next == current when no change is queued */
    uint8_t pos;            /* step within the current pattern that fired last */
    uint8_t playing;        /* 1 after the first step since start */
    uint8_t recording;
    uint8_t muted;          /* whole area muted */
    uint8_t kit_index;
    uint8_t level_value;
    volatile uint32_t revision; /* bumps on every applied edit, for the UI */
} dr_machine_t;

extern const char *const dr_part_names[DR_PARTS];
extern const char *const dr_kit_names[DR_KITS];

/* Control task, before audio starts. Loads the factory patterns and kit 0. */
void dr_init(dr_machine_t *m);
/* The factory sound of kit index (DR_KITS) into s. */
EL_DSP_CODE void dr_kit_sound(tr808_sound_t *s, unsigned index);
/* Control task: queue an edit. Returns 0, or -1 when the queue is full. */
int dr_post(dr_machine_t *m, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value);
/* Control task: replace a whole pattern. Returns 0, -1 queue full, -2 while
 * the previous replacement has not been applied yet (retry next frame). */
int dr_post_pattern(dr_machine_t *m, unsigned slot, const dr_pattern_t *p, int keep_cond);
static inline unsigned dr_prob(const dr_machine_t *m, unsigned slot, unsigned part, unsigned step)
{
    const uint32_t (*c)[DR_PARTS] = m->prob[slot % DR_PATTERNS];
    unsigned v = 0;
    for (unsigned k = 0; k < 4; ++k) v |= ((c[k][part] >> step) & 1u) << k;
    return v;
}
/* Low and high tom (conga) are taken out of the kit (docs/adr/0028): no key,
 * no row, no MIDI note, and they never sound. Their hits in patterns of
 * older versions are removed when a pattern is loaded or generated. */
#define DR_HIDDEN(part) ((part) == D8_LT || (part) == D8_HT)
void dr_strip_hidden(dr_pattern_t *p);

/* Index of a part among the pitch locks (the tuned parts BD SD RS CB ZP PU), or -1. */
static inline int dr_pitch_slot(unsigned part)
{
    switch (part) {
    case D8_BD: return 0;
    case D8_SD: return 1;
    case D8_RS: return 2;
    case D8_CB: return 3;
    case D8_ZP: return 4;
    case D8_PU: return 5;
    default: return -1;
    }
}
/* A step's pitch lock in semitones (0 = none, or a part that is not tuned). */
static inline int dr_pitch(const dr_machine_t *m, unsigned slot, unsigned part, unsigned step)
{
    int s = dr_pitch_slot(part);
    if (s < 0) return 0;
    const uint32_t (*c)[DR_PITCH_PARTS] = m->pitch[slot % DR_PATTERNS];
    unsigned v = 0;
    for (unsigned k = 0; k < 5; ++k) v |= ((c[k][s] >> step) & 1u) << k;
    return (int)v >= 16 ? (int)v - 32 : (int)v;
}
/* A step's fixed level 1..7, 0 = as played (normal or accent). */
static inline unsigned dr_vol(const dr_machine_t *m, unsigned slot, unsigned part, unsigned step)
{
    const uint32_t (*c)[DR_PARTS] = m->vol[slot % DR_PATTERNS];
    return ((c[0][part] >> step) & 1u) | ((c[1][part] >> step) & 1u) << 1 | ((c[2][part] >> step) & 1u) << 2;
}
/* The velocity of a fixed level (1..7): quiet ghost notes up to the accent
 * (5 is the normal hit); the percentage the panel shows. */
float dr_vol_velocity(unsigned level);
int dr_vol_percent(unsigned level);

/* A step's condition (EL_COND_*). */
static inline unsigned dr_cond(const dr_machine_t *m, unsigned slot, unsigned part, unsigned step)
{
    const uint32_t (*c)[DR_PARTS] = m->cond[slot % DR_PATTERNS];
    return ((c[0][part] >> step) & 1u) | ((c[1][part] >> step) & 1u) << 1 | ((c[2][part] >> step) & 1u) << 2;
}

/* Control task helpers (read Flash tables; never call from audio). */
void dr_empty_pattern(dr_pattern_t *out, const char *name);
/* Generate a groove in a style (dr_styles). variation 0..127 adds ghost
 * kicks, moved hats and pickups; seed any value. When keep is non-NULL its
 * bass drum and length are kept. */
void dr_generate(dr_pattern_t *out, unsigned style, unsigned density, uint32_t seed,
                 const dr_pattern_t *keep);
/* Variation of a pattern: amount 0..127 moves, drops and adds hits and
 * accents; the downbeat kick and the backbeat stay. */
void dr_vary(dr_pattern_t *p, unsigned amount, uint32_t seed);

/* Audio side. Apply queued edits once per block. */
EL_DSP_CODE void dr_process_commands(dr_machine_t *m);
/* A transport step fired; step_frames is its length (for rolls). */
EL_DSP_CODE void dr_step(dr_machine_t *m, uint32_t step, float step_frames);
/* Repeat punch-in: play step target (wrapped to the length) instead, the
 * playhead going on as if not; no slot change meanwhile. */
EL_DSP_CODE void dr_step_at(dr_machine_t *m, uint32_t step, float step_frames, unsigned target);
/* Transport stopped or restarted: rewind to the pattern start. */
EL_DSP_CODE void dr_rewind(dr_machine_t *m);
/* Frames until the next internal event (rolls), or limit. */
EL_DSP_CODE int dr_frames_until(const dr_machine_t *m, int limit);
/* Render frames (added): kick alone, the rest of the kit in stereo, sends
 * to the hall, the gated reverb and the delay. */
EL_DSP_CODE void dr_render(dr_machine_t *m, float *kick, float *left, float *right,
                           float *rev, float *gate, float *dly, int frames);
/* Trigger a part from MIDI or a pad; record into the pattern when armed. */
EL_DSP_CODE void dr_hit(dr_machine_t *m, unsigned part, float velocity, int record, int semitones);

/* General MIDI drum note for a part, and the part for a note (or -1). */
unsigned dr_part_note(unsigned part);
int dr_note_part(unsigned note);
#endif
