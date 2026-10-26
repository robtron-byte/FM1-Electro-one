/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Glue between FM1 Open's application hooks and FM1-Electro. The prepare
 * script inserts one call into each of two FM1 Open files:
 *   app/board/audio.c    el_fm1_render() instead of fm1_engine_render()
 *   app/midi_router.c    el_fm1_midi() at the top of fm1_app_midi_received()
 * and el_sdk_app.c's groove_screen_control() calls el_fm1_control(). */
#ifndef EL_FM1_H
#define EL_FM1_H
#include "fm1.h"

FM1_AUDIO_CODE int el_fm1_render(int32_t *output, unsigned frames);
/* Returns 1 when FM1-Electro handled the panel this pass. */
int el_fm1_control(void);
/* Returns 1 when the message was consumed. */
int el_fm1_midi(unsigned source, const uint8_t *message, size_t bytes);
/* Before the first el_fm1_control() the engine's memory is unused (el_init
 * clears it): lends up to its size to the SDK's startup, else NULL. */
void *el_fm1_boot_scratch(size_t bytes);
#endif
