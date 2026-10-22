/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Panel user interface: reads the FM-1 controls, posts edits to the engine,
 * drives the key/button LEDs and repaints the LCD in 16-row bands.
 * Control task only. */
#ifndef EL_UI_H
#define EL_UI_H
#include "electro.h"
#include "fm1.h"
#include "gfx.h"

/* Panel buttons as numbered by the FM1 Open firmware. */
enum {
    BTN_OCT_DOWN, BTN_OCT_UP, BTN_FX, BTN_SCL, BTN_ENV, BTN_LFO, BTN_EDIT,
    BTN_GLO, BTN_HOME, BTN_SAVE, BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC, BTN_COUNT
};
enum { UI_KEYS = 27, UI_WHITE = 16, UI_BLACK = 11, UI_ENCODERS = 7 };

typedef struct {
    /* What the screen shows; compared against the last painted state. */
    uint8_t pattern, next, part, step_page, steps_shown, playing, recording, pos;
    uint8_t param_page, length, swing, kit, mode, gen_style, gen_density, gen_keep;
    uint16_t bpm_x10, mute, solo;
    uint32_t revision, notice_id, hold;
} ui_model_t;

typedef struct {
    el_engine_t *engine;
    int (*save)(el_engine_t *engine); /* SAVE button; 0 on success */
    uint8_t part, step_page, follow, param_page;
    uint8_t gen_style, gen_density, gen_keep;
    uint32_t prev_buttons, prev_keys;
    /* Parameter values just sent, used until the audio side has applied them. */
    int16_t shadow_value[DR_PARTS + 1][10];
    uint32_t shadow_time[DR_PARTS + 1][10];
    const char *notice;
    uint32_t notice_until, notice_id;
    uint64_t leds;
    uint8_t leds_valid;
    ui_model_t shown, model;
    uint16_t dirty;       /* bands waiting to be painted */
    uint8_t paint_next;
    gfx_band_t band;
} el_ui_t;

void el_ui_init(el_ui_t *ui, el_engine_t *engine);
/* One control pass: handle input, update LEDs, paint some dirty bands. */
void el_ui_control(el_ui_t *ui, const struct fm1_controls *controls, uint32_t now);
/* Draw one band of the current page (used by the painter and the simulator). */
void el_ui_draw_band(el_ui_t *ui, unsigned band);
#endif
