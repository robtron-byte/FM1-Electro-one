/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Panel user interface with four areas: BEAT (808), RHYTHM (rhythmic synth
 * sounds), STRINGS (electro strings and pads) and BASS (bass sequences),
 * plus a HOME overview. Reads the FM-1 controls, posts edits to the engine,
 * drives the key/button LEDs and repaints the LCD in 16-row bands.
 * Control task only. */
#ifndef EL_UI_H
#define EL_UI_H
#include "electro.h"
#include "fm1.h"
#include "gfx.h"

/* Panel buttons as numbered by the FM1 Open firmware. */
enum {
    BTN_OCT_DOWN, BTN_OCT_UP, BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT,
    BTN_GLO, BTN_HOME, BTN_SAVE, BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC, BTN_COUNT
};
enum { UI_KEYS = 27, UI_WHITE = 16, UI_BLACK = 11, UI_ENCODERS = 7, UI_PAGES = 5 };

typedef struct {
    el_engine_t *engine;
    int (*save)(el_engine_t *engine); /* SAVE button; 0 on success */
    /* User sound slots (fm1/el_store.c); may be NULL. */
    int (*sound_get)(unsigned slot, el_sound_t *out);
    int (*sound_put)(unsigned slot, const el_sound_t *in);
    char user_name[EL_USER_SOUNDS][EL_SOUND_NAME + 1];
    uint8_t user_engine[EL_USER_SOUNDS], user_used[EL_USER_SOUNDS], user_cached;
    /* SAVE in the editor: slot and name of the sound being stored. */
    uint8_t sound_save, sound_slot, name_cursor;
    char sound_name[EL_SOUND_NAME + 1];
    uint8_t area, overview, follow, part, bank, beat_layer, arp_tap;
    /* Synth editor (EDIT + area button): section and page per area. */
    uint8_t editor, ed_section[EL_AREAS], ed_page[EL_AREAS];
    uint8_t page[EL_AREAS], step_page[EL_AREAS], step_mode[EL_AREAS], cursor[EL_AREAS];
    int8_t octave[EL_AREAS];
    uint8_t gen_style[EL_AREAS], gen_density[EL_AREAS], gen_keep;
    /* New groove for a whole slot: posted piece by piece (drums, then the
     * three synth parts), one staging buffer at a time. */
    uint8_t job_slot, job_stage;        /* stage 0 = idle */
    dr_pattern_t job_drums;
    st_pattern_t job_parts[EL_TRACKS];
    /* SAVE: pick a user slot with a white key, wait for the copy, store. */
    uint8_t save_pick, save_pending;
    uint32_t save_copies;
    /* New grooves are kept in Flash on their own, shortly after the last
     * generator key press (they survive a restart). */
    uint8_t autosave;
    uint32_t autosave_at;
    uint8_t key_note[UI_KEYS];          /* note sent for each held key, 0xff = none */
    uint8_t key_track[UI_KEYS];
    uint32_t prev_buttons, prev_keys;
    /* Per encoder E3-E6: value just sent, used until the audio side has it. */
    int16_t shadow_value[4];
    uint16_t shadow_param[4];   /* area << 8 | parameter */
    uint32_t shadow_time[4];
    const char *notice;
    uint32_t notice_until, notice_id, hold;
    uint64_t leds;
    uint8_t leds_valid;
    uint32_t sig[3];                    /* header, main, footer as last painted */
    uint16_t dirty;                     /* bands waiting to be painted */
    uint8_t paint_next;
    uint32_t now;
    gfx_band_t band;
} el_ui_t;

void el_ui_init(el_ui_t *ui, el_engine_t *engine);
/* One control pass: handle input, update LEDs, paint some dirty bands. */
void el_ui_control(el_ui_t *ui, const struct fm1_controls *controls, uint32_t now);
/* Draw one band of the current page (used by the painter and the simulator). */
void el_ui_draw_band(el_ui_t *ui, unsigned band);
#endif
