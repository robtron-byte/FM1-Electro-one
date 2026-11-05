/* SPDX-License-Identifier: GPL-3.0-only */
/* FM1-Electro on its own platform (platform/, docs/adr/0021): what the platform
 * side (el_app.c, one unit with platform.c) calls of FM1-Electro (el_glue.c,
 * built apart: the platform's names BTN_* / ENC_* are FM1-Electro's too). */
#ifndef EL_GLUE_H
#define EL_GLUE_H
#include <stddef.h>
#include <stdint.h>
#include "fm1.h"

/* Main loop: engine, store and UI once, before the first render. */
void el_glue_start(void);
/* Main loop: one pass of the UI (keys, knobs, LEDs, screen). */
void el_glue_control(const struct fm1_controls *controls, uint32_t now_ms);
/* Audio interrupt: adds frames stereo frames (L R ..., Q15, out already zero). */
void el_glue_render(int32_t *out, unsigned frames);
/* Main loop: a complete MIDI message from USB or the TRS jack. */
void el_glue_midi(const uint8_t *message, size_t bytes);

/* From the platform side: its free-running 24 MHz counter. */
uint32_t el_platform_ticks(void);
#endif
