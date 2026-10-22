/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * 808 drum machine: 11 parts from the DaisySP/Plaits 808 model, 16 patterns
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
#include "el_d808_names.h"
#include "drum808_state.h"

enum {
    DR_PARTS = D8_NUM,      /* BD SD LT MT HT RS CP CB CY OH CH */
    DR_STEPS = 32,
    DR_PATTERNS = 16,
    DR_KITS = 4,
    DR_STYLES = 5,
    DR_NAME = 10
};

typedef struct {
    char name[DR_NAME + 1];
    uint8_t length;            /* 1..32 steps */
    uint8_t swing;             /* 50..75, loaded into the transport on change */
    uint8_t kit;               /* suggested kit, applied when loaded from the factory */
    uint32_t hit[DR_PARTS];    /* bit n = step n */
    uint32_t roll[DR_PARTS];   /* extra 32nd hit halfway through the step */
    uint32_t accent;           /* 808-style global accent row */
} dr_pattern_t;

enum dr_command {
    DR_CMD_HIT,         /* a = part, b = velocity 1..127, c = record flag */
    DR_CMD_TOGGLE,      /* a = part, b = step: hit on/off */
    DR_CMD_ROLL,        /* a = part, b = step: roll on/off (sets the hit too) */
    DR_CMD_ACCENT,      /* b = step */
    DR_CMD_CLEAR_PART,  /* a = part (DR_PARTS clears the whole pattern) */
    DR_CMD_LENGTH,      /* b = 1..32 */
    DR_CMD_SWING,       /* b = 50..75 */
    DR_CMD_SELECT,      /* a = pattern; b = 1 switches immediately, else at loop end */
    DR_CMD_COPY,        /* a = source pattern, b = destination */
    DR_CMD_LOAD,        /* a = pattern slot: copy the staging pattern into it */
    DR_CMD_MUTE,        /* a = part, b = on */
    DR_CMD_SOLO,        /* a = part, b = on */
    DR_CMD_PARAM,       /* a = part (D8_KIT for kit), b = parameter index, value */
    DR_CMD_KIT,         /* a = kit preset */
    DR_CMD_LEVEL,       /* value = 0..127 */
    DR_CMD_RECORD,      /* b = on */
    DR_CMD_PANIC,
    DR_CMD_AREA_MUTE    /* b = on: silence the whole beat area */
};

enum { DR_STYLE_ELECTRO, DR_STYLE_KRAFT, DR_STYLE_ROTHER, DR_STYLE_ROBOT, DR_STYLE_MIAMI };

typedef struct {
    drum808_t kit;
    dr_pattern_t pattern[DR_PATTERNS];
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
extern const char *const dr_style_names[DR_STYLES];

/* Control task, before audio starts. Loads the factory patterns and kit 0. */
void dr_init(dr_machine_t *m);
/* Control task: queue an edit. Returns 0, or -1 when the queue is full. */
int dr_post(dr_machine_t *m, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value);
/* Control task: replace a whole pattern. Returns 0, -1 queue full, -2 while
 * the previous replacement has not been applied yet (retry next frame). */
int dr_post_pattern(dr_machine_t *m, unsigned slot, const dr_pattern_t *p);

/* Control task helpers (read Flash tables; never call from audio). */
void dr_factory_pattern(unsigned slot, dr_pattern_t *out);
/* Build a groove in the given style. density 0..127; seed any value.
 * When keep is non-NULL its bass drum and length are kept. */
void dr_generate(dr_pattern_t *out, unsigned style, unsigned density, uint32_t seed,
                 const dr_pattern_t *keep);

/* Audio side. Apply queued edits once per block. */
EL_DSP_CODE void dr_process_commands(dr_machine_t *m);
/* A transport step fired; step_frames is its length (for rolls). */
EL_DSP_CODE void dr_step(dr_machine_t *m, uint32_t step, float step_frames);
/* Transport stopped or restarted: rewind to the pattern start. */
EL_DSP_CODE void dr_rewind(dr_machine_t *m);
/* Frames until the next internal event (rolls), or limit. */
EL_DSP_CODE int dr_frames_until(const dr_machine_t *m, int limit);
/* Render frames into the stereo bus and the reverb/delay sends (added). */
EL_DSP_CODE void dr_render(dr_machine_t *m, float *left, float *right, float *rev, float *dly, int frames);
/* Trigger a part from MIDI or a pad; record into the pattern when armed. */
EL_DSP_CODE void dr_hit(dr_machine_t *m, unsigned part, float velocity, int record);

/* General MIDI drum note for a part, and the part for a note (or -1). */
unsigned dr_part_note(unsigned part);
int dr_note_part(unsigned note);
#endif
