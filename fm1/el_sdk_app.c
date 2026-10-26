/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Device build only: what FM1 Open's startup expects from its app layer,
 * for the parts FM1-Electro replaces. tools/prepare_fm1.py moves the SDK's
 * own screen, presets, projects, samples, FM library and project transfer
 * out of the build (they would keep the SDK's synth engine, effects and
 * buffers linked: about 100 KB of SRAM). Board drivers, audio output and
 * the MIDI router stay as they are. */
#include "instrument.h"
#include "fm_patch.h"
#include "fm1.h"
#include "el_fm1.h"
#include <stdlib.h>
#include <string.h>

/* Panel and LCD come up as in the SDK's screen; FM1-Electro draws itself. */
void groove_screen_init(void)
{
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
    fm1_screen_init();
}

void groove_screen_control(void) { (void)el_fm1_control(); }

/* No SDK projects: FM1-Electro keeps its own store (el_store.c). */
void groove_project_init(void) {}
void groove_project_poll(void) {}
void groove_project_name(char name[13]) { memcpy(name, "FM1-ELECTRO", 12); }

/* Only allocated if the SDK ever asks for it. */
struct groove_project *groove_project_scratch(void)
{
    static struct groove_project *scratch;
    if (!scratch) scratch = malloc(sizeof *scratch);
    return scratch;
}

/* No sample bank, FM library or project transfer. */
void groove_samples_init(void) {}
void groove_samples_poll(void) {}
void groove_transfer_init(void) {}
void groove_transfer_poll(void) {}
void groove_fm_library_init(void) {}
FM1_DSP_CODE void groove_fm_program(uint8_t patch[156], unsigned program)
{
    (void)program;
    memset(patch, 0, 156);
}
