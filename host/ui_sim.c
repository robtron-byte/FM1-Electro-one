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

static void hold(unsigned b, int on) { if (on) panel.buttons |= 1u << b; else panel.buttons &= ~(1u << b); run(20); }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    memset(flash, 0xff, sizeof flash);
    el_init(&engine);
    el_ui_init(&ui, &engine);
    ui.save = el_store_save;
    run(100);
    shot(dir, "01_beat");

    tap_button(BTN_PLAY);
    run(900);
    shot(dir, "02_beat_playing");

    tap_button(BTN_SCL);              /* RHYTHM */
    run(400);
    shot(dir, "03_rhythm");

    tap_button(BTN_ENV);              /* STRINGS */
    run(600);
    shot(dir, "04_strings");

    hold(BTN_ARP, 1);                 /* generator: STABS -> PULSE */
    turn(2, 1);
    tap_key(4);
    shot(dir, "05_strings_generator");
    hold(BTN_ARP, 0);

    tap_button(BTN_LFO);              /* BASS */
    tap_button(BTN_LFO);              /* next sound page: ENVELOPE */
    run(300);
    shot(dir, "06_bass_keys");

    tap_button(BTN_SEQ);              /* STEP mode */
    tap_key(4);                       /* white key 3: select (and toggle) step 3 */
    turn(2, 2);                       /* note +2 */
    run(200);
    shot(dir, "07_bass_step");

    tap_button(BTN_HOME);             /* overview */
    hold(BTN_GLO, 1);
    tap_button(BTN_SCL);              /* GLO + RHYTHM: mute it */
    hold(BTN_GLO, 0);
    turn(5, -6);                      /* bass level down */
    run(400);
    shot(dir, "08_home");

    hold(BTN_ARP, 1);                 /* HOME + ARP + white key 3: new groove in slot 3 */
    tap_key(4);
    hold(BTN_ARP, 0);
    run(300);
    shot(dir, "09_new_groove");
    int fresh = engine.drums.current == 2 && engine.drums.pattern[2].style == 2;
    /* The new groove is kept in Flash by itself and comes back after a restart. */
    run(2500);
    static el_engine_t restarted;
    el_init(&restarted);
    fresh = fresh && el_store_load(&restarted) == 0 &&
            !memcmp(&restarted.drums.pattern[2], &engine.drums.pattern[2], sizeof(dr_pattern_t)) &&
            !memcmp(&restarted.track[1].pattern[2], &engine.track[1].pattern[2], sizeof(st_pattern_t));
    printf("store: new groove kept over restart %s\n", fresh ? "ok" : "FAIL");

    tap_button(BTN_PLAY);
    tap_button(BTN_SAVE);             /* SAVE, then white key 1: user slot 17 */
    shot(dir, "10_save_pick");
    tap_key(0);
    run(100);
    shot(dir, "11_saved");
    const unsigned user = DR_STYLE_SLOTS;
    fresh = fresh && !memcmp(engine.drums.pattern[user].hit, engine.drums.pattern[2].hit, sizeof engine.drums.pattern[2].hit);
    static el_engine_t loaded;
    el_init(&loaded);
    int r = el_store_load(&loaded);
    int same = fresh && r == 0 && !memcmp(loaded.drums.pattern, engine.drums.pattern, sizeof loaded.drums.pattern);
    for (unsigned t = 0; t < EL_TRACKS; ++t)
        same = same && !memcmp(loaded.track[t].pattern, engine.track[t].pattern, sizeof loaded.track[t].pattern) &&
               !memcmp(loaded.track[t].part.v, engine.track[t].part.v, MP_COUNT) &&
               loaded.track[t].muted == engine.track[t].muted && loaded.track[t].level_value == engine.track[t].level_value;
    same = same && !memcmp(loaded.fx.v, engine.fx.v, FX_PARAMS);
    if (!same) {
        printf("debug r=%d fresh=%d\n", r, fresh);
        for (unsigned i = 0; i < DR_PATTERNS; ++i) if (memcmp(&loaded.drums.pattern[i], &engine.drums.pattern[i], sizeof(dr_pattern_t))) { const unsigned char *a = (const void *)&loaded.drums.pattern[i], *b = (const void *)&engine.drums.pattern[i]; printf(" drum slot %u differs:", i); for (unsigned k = 0; k < sizeof(dr_pattern_t); ++k) if (a[k] != b[k]) printf(" @%u %02x/%02x", k, a[k], b[k]); printf("\n"); }
        for (unsigned t = 0; t < EL_TRACKS; ++t) {
            for (unsigned i = 0; i < ST_PATTERNS; ++i)
                if (memcmp(&loaded.track[t].pattern[i], &engine.track[t].pattern[i], sizeof(st_pattern_t))) printf(" track %u slot %u differs\n", t, i);
            if (memcmp(loaded.track[t].part.v, engine.track[t].part.v, MP_COUNT)) printf(" track %u sound differs\n", t);
        }
    }
    printf("store: save/load round trip %s\n", same ? "ok" : "FAIL");
    return same ? 0 : 1;
}
