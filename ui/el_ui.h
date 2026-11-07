/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Panel user interface with four areas: BEAT (808), RHYTHM (rhythmic synth
 * sounds), STRINGS (electro strings and pads) and BASS (bass sequences),
 * plus a HOME overview. Reads the FM-1 controls, posts edits to the engine,
 * drives the key/button LEDs and repaints the LCD in 8-row bands.
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
enum { EL_OCT_MIN = -3, EL_OCT_MAX = 2 };   /* the keys' octave per synth area */
enum { UI_KEYS = 27, UI_WHITE = 16, UI_BLACK = 11, UI_ENCODERS = 7, UI_PAGES = 5 };

typedef struct {
    el_engine_t *engine;
    int (*save)(el_engine_t *engine); /* SAVE button; 0 on success */
    /* Erase the copy the next save writes, ahead (docs/adr/0023); may be
     * NULL. Called once the groove has stood still a while after the start
     * and after each save; then SAVE while it plays leaves no gap. */
    int (*prepare)(void);
    uint8_t prep_pending;
    uint32_t still_since;
    /* User sound slots (fm1/el_store.c); may be NULL. */
    int (*sound_get)(unsigned slot, el_sound_t *out);
    int (*sound_put)(unsigned slot, const el_sound_t *in);
    char user_name[EL_USER_SOUNDS][EL_SOUND_NAME + 1];
    uint8_t user_engine[EL_USER_SOUNDS], user_used[EL_USER_SOUNDS], user_cached;
    /* SAVE in the editor: slot and name of the sound being stored. */
    uint8_t sound_save, sound_slot, name_cursor;
    char sound_name[EL_SOUND_NAME + 1];
    uint8_t area, overview, follow, part, beat_layer, arp_tap;
    uint8_t options, opt_item;          /* OPTIONS menu (HOME + SAVE) */
    /* Synth editor (EDIT + area button): section and page per area. */
    uint8_t editor, ed_section[EL_AREAS], ed_page[EL_AREAS];
    uint8_t page[EL_AREAS], step_page[EL_AREAS], step_mode[EL_AREAS], cursor[EL_AREAS];
    int8_t octave[EL_AREAS];
    /* Step record (REC while stopped): new sequence pending, key and note of
     * the last step (legato), TRANSPOSE mode per area (GLO + SEQ). */
    uint8_t rec_fresh, rec_key, rec_note, transpose_mode[EL_AREAS];
    uint8_t punch_key[EL_PUNCHES];      /* key + 1 holding each punch-in (black keys 5-11) */
    uint8_t punch_last;                 /* the punch-in pressed last: its value on the white keys */
    /* A step key held (BEAT, or a synth area in STEP mode; docs/adr/0025):
     * key + 1, area, step, drum part, its condition as last set, whether an
     * encoder turned meanwhile, and whether the step sounded at the press
     * (then it goes off at the release, unless an encoder turned). */
    uint8_t sh_key, sh_area, sh_step, sh_part, sh_cond, sh_used, sh_remove;
    uint8_t sh_prob;                    /* its probability (EL_PROBS; ALGORITHM while held) */
    uint32_t sh_at;                     /* when it went down: the big readout after a moment */
    uint8_t gen_style[EL_AREAS], gen_density[EL_AREAS], gen_keep;
    /* New or varied groove for all areas: posted piece by piece (drums,
     * then the three synth parts), one staging buffer at a time, each into
     * the slot its area plays. */
    uint8_t job_slot[EL_AREAS], job_stage, job_fresh;   /* stage 0 = idle */
    dr_pattern_t job_drums;
    st_pattern_t job_parts[EL_TRACKS];
    /* Generator keys ask first: what, for which area (4 = all), until when. */
    uint8_t ask, ask_area;
    uint32_t ask_until;
    /* New grooves are kept in Flash on their own, shortly after the last
     * generator key press (they survive a restart). */
    uint8_t autosave;
    uint8_t show_pos[4];                  /* the steps the screen shows (0 BEAT, 1-3 the tracks), held for a whole frame */
    uint8_t opt_snap[EL_OPTIONS];         /* the options when OPTIONS opened: save only what changed */
    uint32_t autosave_at;
    uint8_t key_note[UI_KEYS];          /* note sent for each held key, 0xff = none */
    uint8_t key_track[UI_KEYS];
    uint32_t prev_buttons, prev_keys;
    /* Per encoder E3-E6: value just sent, used until the audio side has it. */
    int16_t shadow_value[4];
    uint16_t shadow_param[4];   /* area << 8 | parameter */
    uint32_t shadow_time[4];
    uint32_t knob_at[4];        /* when each of KNOB 1-4 last turned (its speed: edit_knob) */
    const char *notice;
    uint32_t notice_until, notice_id, hold;
    uint64_t leds, leds_dim;   /* lit, and of those dimmed (docs/adr/0020) */
    uint8_t leds_valid;
    uint32_t sig[3];                    /* header, main, footer as last painted */
    uint32_t dirty;                     /* bands waiting to be painted (bit per band) */
    uint8_t paint_next;
    uint32_t now;
    /* Audio load, shown in the header (device only; -1 hides it): percent of
     * a block's time the last half second's slowest block took, and whether
     * blocks were dropped (docs/adr/0019). */
    int16_t load;
    uint8_t load_drops;
    /* CPU test at start (device only; bench[0] == 0: none): tenths of a
     * nanosecond per operation, EL_BENCH_* (docs/adr/0019). */
    uint16_t bench[16];
    /* Average share of a block per render stage (EL_PROF_*), percent;
     * prof_valid 0: none measured (host). */
    uint8_t prof[EL_PROF_STAGES], prof_valid;
    /* Own platform (docs/adr/0021): the slowest audio callback since the
     * start (percent of its time), the slowest synth stage of the last half
     * second (percent of a block, the second core), whether that core still
     * runs, and the callbacks that took longer than their time (a crackle). */
    uint16_t load_max;
    uint8_t c1_peak, cpu1_on, plat;
    uint32_t drops;
    /* Clock registers (device only, ADR 0019): PLL and system clock in MHz
     * as X0X reads them, the system clock source code, the timer clock. */
    uint16_t clk_pll, clk_sys, clk_timer;
    uint8_t clk_src;
    gfx_band_t band;
} el_ui_t;

enum { EL_BENCH_LOOP, EL_BENCH_INT, EL_BENCH_FADD, EL_BENCH_FMUL, EL_BENCH_FMUL4, EL_BENCH_FDIV, EL_BENCH_DENORM,
       EL_BENCH_RLOOP,   /* the empty loop again, run from SRAM like the audio code */
       EL_BENCH_RMUL,    /* the FMUL chain from SRAM */
       EL_BENCH_LDSEQ,   /* a float load per step, through 8 KB in order */
       EL_BENCH_LDRND,   /* a dependent load per step, jumping through 64 KB (latency) */
       EL_BENCH_CALL,    /* a call of a small function per step, from SRAM */
       EL_BENCH_RIADD,   /* integer add chain from SRAM */
       EL_BENCH_RIMUL,   /* integer multiply-add chain from SRAM */
       EL_BENCH_RMUL64,  /* 32 x 32 -> 64 bit multiply, the high word kept (Q31), chained, from SRAM */
       EL_BENCH_RFADD,   /* float add chain from SRAM */
       EL_BENCH_RIMUL4,  /* four independent integer multiply-adds from SRAM */
       EL_BENCH_KINDS };
void el_ui_init(el_ui_t *ui, el_engine_t *engine);
/* One control pass: handle input, update LEDs, paint some dirty bands. */
void el_ui_control(el_ui_t *ui, const struct fm1_controls *controls, uint32_t now);
/* Draw one band of the current page (used by the painter and the simulator). */
void el_ui_draw_band(el_ui_t *ui, unsigned band);
#endif
