/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * WebAssembly build of the firmware for the browser panel simulator: the
 * real engine and UI code against an emulated FM1 Open panel API (screen,
 * LEDs, controls, clock, 640 KB of Flash). The page calls sim_control()
 * every few ms with the panel state and sim_render() for audio. */
#include "el_ui.h"
#include "el_store.h"
#include <string.h>

#define EXPORT __attribute__((visibility("default")))

static uint16_t screen[GFX_W * GFX_H];
static uint8_t flash[640 * 1024];
static uint8_t leds[41];
static uint32_t clock_ms, flash_writes;
static el_engine_t engine;
static el_ui_t ui;
static struct fm1_controls panel;
static float audio[EL_BLOCK * 2];

uint32_t fm1_millis(void) { return clock_ms; }
void fm1_led_set(unsigned key, int on) { if (key < 41) leds[key] = (uint8_t)(on != 0); }
int fm1_screen_pending(void) { return 0; }
uint32_t fm1_flash_size(void) { return sizeof flash; }
int fm1_flash_read(uint32_t offset, void *data, size_t bytes)
{
    if (offset + bytes > sizeof flash) return FM1_FLASH_INVALID;
    memcpy(data, flash + offset, bytes);
    return 0;
}
int fm1_flash_erase(uint32_t offset)
{
    if (offset % FM1_FLASH_SECTOR_BYTES || offset >= sizeof flash) return FM1_FLASH_INVALID;
    memset(flash + offset, 0xff, FM1_FLASH_SECTOR_BYTES);
    ++flash_writes;
    return 0;
}
int fm1_flash_write(uint32_t offset, const void *data, size_t bytes)
{
    const uint8_t *p = data;
    if (offset + bytes > sizeof flash) return FM1_FLASH_INVALID;
    for (size_t i = 0; i < bytes; ++i) flash[offset + i] &= p[i];
    ++flash_writes;
    return 0;
}
int fm1_screen_submit(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t *pixels, size_t count)
{
    if (x + w > GFX_W || y + h > GFX_H || count < (size_t)w * h) return -1;
    for (unsigned r = 0; r < h; ++r) memcpy(screen + (y + r) * GFX_W + x, pixels + r * w, w * 2u);
    return 0;
}

/* The Flash image lives in the page's storage between visits: the page
 * fills it before sim_init(restore = 1) and saves it when sim_flash_writes()
 * changes. */
EXPORT uint8_t *sim_flash(void) { return flash; }
EXPORT uint32_t sim_flash_bytes(void) { return sizeof flash; }
EXPORT uint32_t sim_flash_writes(void) { return flash_writes; }
EXPORT uint16_t *sim_screen(void) { return screen; }
EXPORT uint8_t *sim_leds(void) { return leds; }
EXPORT float *sim_audio(void) { return audio; }
EXPORT int sim_block(void) { return EL_BLOCK; }

EXPORT void sim_init(int restore)
{
    if (!restore) memset(flash, 0xff, sizeof flash);
    el_init(&engine);
    el_store_load(&engine);
    el_ui_init(&ui, &engine);
    ui.save = el_store_save;
    ui.sound_get = el_sound_get;
    ui.sound_put = el_sound_put;
}

/* One control pass: panel state since the last call. */
EXPORT void sim_control(uint32_t now, uint32_t buttons, uint32_t keys, int e0, int e1, int e2, int e3, int e4, int e5, int e6,
                        int knob)
{
    clock_ms = now;
    panel.buttons = buttons;
    panel.keys = keys;
    const int e[7] = {e0, e1, e2, e3, e4, e5, e6};
    for (int i = 0; i < 7; ++i) panel.encoders[i] = (int16_t)e[i];
    panel.knob = (uint16_t)knob;
    int master = knob * 127 / 1023;
    static int shown = -1;
    if (master != shown && el_post(&engine, EL_CMD_MASTER, 0, (int16_t)master) == 0) shown = master;
    el_ui_control(&ui, &panel, now);
}

/* Audio: one block of EL_BLOCK interleaved stereo frames into sim_audio(). */
EXPORT void sim_render(void) { el_render_float(&engine, audio, EL_BLOCK); }

/* MIDI in (Web MIDI), 1-3 bytes. */
EXPORT void sim_midi(int b0, int b1, int b2)
{
    uint8_t m[3] = {(uint8_t)b0, (uint8_t)b1, (uint8_t)b2};
    el_midi_input(&engine, m, 3);
}
