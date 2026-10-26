/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_fm1.h"
#include "el_ui.h"
#include "el_store.h"

static el_engine_t engine;
static el_ui_t ui;
static volatile uint8_t ready;
static int master_shown = -1;

FM1_AUDIO_CODE int el_fm1_render(int32_t *output, unsigned frames)
{
    if (!output || frames != FM1_BLOCK_FRAMES) return -1;
    if (!ready) {
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) output[i] = 0;
        return 0;
    }
    el_render(&engine, output, frames);
    return 0;
}

int el_fm1_control(void)
{
    struct fm1_controls controls;
    if (!ready) {
        /* groove_screen_init() (el_sdk_app.c) has started the panel and LCD. */
        el_init(&engine);
        /* The last grooves come back from Flash; only on the very first
         * start (empty Flash) the generated factory set plays. */
        el_store_load(&engine);
        el_ui_init(&ui, &engine);
        ui.save = el_store_save;
        ui.sound_get = el_sound_get;
        ui.sound_put = el_sound_put;
        EL_FENCE();
        ready = 1;
    }
    fm1_controls_read(&controls);
    /* MASTER knob: 0..1023 ADC to 0..127, with a little hysteresis. */
    int master = (int)controls.knob * 127 / 1023;
    if (master_shown < 0 || master > master_shown + 1 || master < master_shown - 1 ||
        (master == 0 && master_shown != 0) || (master == 127 && master_shown != 127)) {
        if (el_post(&engine, EL_CMD_MASTER, 0, (int16_t)master) == 0) master_shown = master;
    }
    el_ui_control(&ui, &controls, fm1_millis());
    return 1;
}

void *el_fm1_boot_scratch(size_t bytes)
{
    return !ready && bytes <= sizeof engine ? (void *)&engine : NULL;
}

int el_fm1_midi(unsigned source, const uint8_t *message, size_t bytes)
{
    (void)source;
    if (!ready) return 0;
    return el_midi_input(&engine, message, bytes);
}
