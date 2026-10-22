/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host panel simulator: runs the engine and the UI against stubbed FM1
 * panel calls, plays a scripted session and writes screenshots (PPM) plus
 * the LED state next to each. Usage: ui_sim <output directory> */
#include "el_ui.h"
#include "el_store.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static uint16_t screen[GFX_W * GFX_H];
static uint8_t flash[640 * 1024];
static int leds[41];
static uint32_t clock_ms;

/* Stubs for the SDK panel API. */
uint32_t fm1_millis(void) { return clock_ms; }
void fm1_led_set(unsigned key, int on) { if (key < 41) leds[key] = on; }
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
    return 0;
}
int fm1_flash_write(uint32_t offset, const void *data, size_t bytes)
{
    const uint8_t *p = data;
    if (offset + bytes > sizeof flash) return FM1_FLASH_INVALID;
    for (size_t i = 0; i < bytes; ++i) flash[offset + i] &= p[i]; /* NOR: only 1 -> 0 */
    return 0;
}
int fm1_screen_submit(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t *pixels, size_t count)
{
    if (x + w > GFX_W || y + h > GFX_H || count < (size_t)w * h) return -1;
    for (unsigned r = 0; r < h; ++r) memcpy(screen + (y + r) * GFX_W + x, pixels + r * w, w * 2u);
    return 0;
}

static el_engine_t engine;
static el_ui_t ui;
static struct fm1_controls panel;

/* Advance time: audio blocks plus a UI pass every 5 ms. */
static void run(unsigned ms)
{
    float audio[EL_BLOCK * 2];
    static float carry;
    for (unsigned i = 0; i < ms; ++i) {
        carry += FM1_SAMPLE_RATE / 1000.0f;
        while (carry >= EL_BLOCK) { el_render_float(&engine, audio, EL_BLOCK); carry -= EL_BLOCK; }
        ++clock_ms;
        if (clock_ms % 5u == 0) {
            el_ui_control(&ui, &panel, clock_ms);
            memset(panel.encoders, 0, sizeof panel.encoders);
        }
    }
}

static void tap_button(unsigned b) { panel.buttons |= 1u << b; run(30); panel.buttons &= ~(1u << b); run(30); }
static void tap_key(unsigned k) { panel.keys |= 1u << k; run(30); panel.keys &= ~(1u << k); run(30); }
static void turn(unsigned e, int detents) { panel.encoders[e] = (int16_t)detents; run(10); }

static void shot(const char *dir, const char *name)
{
    char path[512];
    /* Make sure every dirty band reached the "LCD". */
    run(40);
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; ++i) {
        uint16_t c = screen[i];
        unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/%s.leds", dir, name);
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < 41; ++i) fputc(leds[i] ? '1' : '0', f);
    fprintf(f, "\n%u\n%u\n", panel.buttons, panel.keys);
    fclose(f);
    printf("%s\n", name);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    memset(flash, 0xff, sizeof flash);
    el_init(&engine);
    el_ui_init(&ui, &engine);
    ui.save = el_store_save;
    run(100);
    shot(dir, "01_start");

    /* Pick the closed hat (last black key) and start ELECTRO at 112 BPM. */
    tap_key(25);
    turn(1, -1);
    turn(1, -1);
    turn(1, 1);
    tap_button(BTN_PLAY);
    run(700);
    shot(dir, "02_playing_ch");

    /* Mute the snare while holding GLO. */
    panel.buttons |= 1u << BTN_GLO;
    run(20);
    tap_key(3);
    shot(dir, "03_glo_mute");
    panel.buttons &= ~(1u << BTN_GLO);
    run(20);

    /* Groove generator: hold LFO, choose ROTHER, press a white key. */
    panel.buttons |= 1u << BTN_LFO;
    run(20);
    turn(3, 3);
    tap_key(0);
    shot(dir, "04_generator");
    panel.buttons &= ~(1u << BTN_LFO);
    run(300);
    shot(dir, "05_generated");

    /* 32-step BUILD UP pattern (slot 13), queued and played into bar two. */
    tap_key(1); /* select BD */
    turn(0, 12);
    run(2500);
    shot(dir, "06_build_up");

    /* Kick sound page while stopped. */
    tap_button(BTN_PLAY);
    turn(2, 3);
    turn(4, -4);
    shot(dir, "07_kick_sound");

    /* Save twice (both sectors in use), then load into a fresh engine. */
    tap_button(BTN_SAVE);
    dr_post(&engine.drums, DR_CMD_TOGGLE, D8_HT, 3, 0, 0);
    run(20);
    tap_button(BTN_SAVE);
    shot(dir, "08_saved");
    static el_engine_t loaded;
    el_init(&loaded);
    int r = el_store_load(&loaded);
    int same = r == 0 && !memcmp(loaded.drums.pattern, engine.drums.pattern, sizeof loaded.drums.pattern) &&
               drum808_get(&loaded.drums.kit, D8_BD, 0) == drum808_get(&engine.drums.kit, D8_BD, 0) &&
               drum808_get(&loaded.drums.kit, D8_BD, 2) == drum808_get(&engine.drums.kit, D8_BD, 2) &&
               loaded.drums.current == engine.drums.current;
    printf("store: save/load round trip %s\n", same ? "ok" : "FAIL");
    return same ? 0 : 1;
}
