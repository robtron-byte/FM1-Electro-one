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

    hold(BTN_EDIT, 1);                /* EDIT + E7: strings from the JP-6 */
    turn(6, 1);
    hold(BTN_EDIT, 0);
    run(300);
    shot(dir, "05b_strings_jp6");
    printf("engine: strings on %s %s\n", syn_engine_names[engine.track[1].part.engine],
           engine.track[1].part.engine == SYN_JP6 ? "ok" : "FAIL");

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
    hold(BTN_EDIT, 1);                /* HOME + EDIT: distortion sends */
    turn(2, 20);                      /* BEAT send */
    turn(5, 30);                      /* BASS send */
    turn(0, 1);                       /* type: TUBE */
    run(300);
    shot(dir, "08b_home_distortion");
    hold(BTN_EDIT, 0);
    printf("dist: send beat %u bass %u type %s %s\n", engine.fx.v[FX_DIST_SEND_BEAT], engine.fx.v[FX_DIST_SEND_BASS],
           fx_dist_mode_name(engine.fx.v[FX_DIST_MODE]),
           engine.fx.v[FX_DIST_SEND_BEAT] > 0 && engine.fx.v[FX_DIST_SEND_BASS] > 0 && engine.fx.v[FX_DIST_MODE] == DIST_TUBE ? "ok" : "FAIL");

    hold(BTN_ARP, 1);                 /* HOME + ARP + white key 3: new groove in slot 3 */
    tap_key(4);
    hold(BTN_ARP, 0);
    run(300);
    shot(dir, "09_new_groove");
    /* The new groove is kept in Flash by itself and comes back after a restart. */
    run(2500);
    int fresh = engine.drums.current == 2 && engine.drums.pattern[2].style == 2;
    static el_engine_t restarted;
    el_init(&restarted);
    int lr = el_store_load(&restarted);
    fresh = fresh && lr == 0 &&
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
               loaded.track[t].part.engine == engine.track[t].part.engine && !memcmp(syn_values(&loaded.track[t].part), syn_values(&engine.track[t].part), syn_value_count(engine.track[t].part.engine)) &&
               loaded.track[t].muted == engine.track[t].muted && loaded.track[t].level_value == engine.track[t].level_value;
    same = same && !memcmp(loaded.fx.v, engine.fx.v, FX_PARAMS);
    if (!same) {
        printf("debug r=%d fresh=%d\n", r, fresh);
        for (unsigned i = 0; i < DR_PATTERNS; ++i) if (memcmp(&loaded.drums.pattern[i], &engine.drums.pattern[i], sizeof(dr_pattern_t))) { const unsigned char *a = (const void *)&loaded.drums.pattern[i], *b = (const void *)&engine.drums.pattern[i]; printf(" drum slot %u differs:", i); for (unsigned k = 0; k < sizeof(dr_pattern_t); ++k) if (a[k] != b[k]) printf(" @%u %02x/%02x", k, a[k], b[k]); printf("\n"); }
        for (unsigned t = 0; t < EL_TRACKS; ++t) {
            for (unsigned i = 0; i < ST_PATTERNS; ++i)
                if (memcmp(&loaded.track[t].pattern[i], &engine.track[t].pattern[i], sizeof(st_pattern_t))) printf(" track %u slot %u differs\n", t, i);
            if (memcmp(syn_values(&loaded.track[t].part), syn_values(&engine.track[t].part), syn_value_count(engine.track[t].part.engine))) printf(" track %u sound differs\n", t);
        }
    }
    printf("store: save/load round trip %s\n", same ? "ok" : "FAIL");

    /* Arpeggiator in BASS: tap ARP, hold two keys. */
    if (!engine.transport.running) tap_button(BTN_PLAY);
    tap_button(BTN_LFO);
    st_track_t *bass = &engine.track[2];
    tap_button(BTN_ARP);
    run(50);
    int arp_ok = bass->arp.v[ARP_ON] == 1 && ui.page[EL_AREA_BASS] == 5 && !ui.step_mode[EL_AREA_BASS];
    panel.keys |= 1u << 0 | 1u << 7;
    run(30);
    uint32_t before = bass->arp.position;
    run(1000);
    shot(dir, "12_bass_arp");
    uint32_t running_notes = bass->arp.position - before;
    arp_ok = arp_ok && bass->arp.count == 2 && running_notes >= 6;
    tap_button(BTN_PLAY);             /* stopped: the arp keeps going on its own */
    before = bass->arp.position;
    run(1000);
    uint32_t free_notes = bass->arp.position - before;
    arp_ok = arp_ok && free_notes >= 6;
    panel.keys = 0;
    run(30);
    arp_ok = arp_ok && bass->arp.count == 0 && bass->arp.sounding < 0;
    hold(BTN_EDIT, 1);                /* EDIT + ARP: HOLD */
    tap_button(BTN_ARP);
    hold(BTN_EDIT, 0);
    tap_key(4);
    run(300);
    arp_ok = arp_ok && bass->arp.v[ARP_HOLD] == 1 && bass->arp.count == 1;
    tap_button(BTN_ARP);              /* off again */
    run(30);
    arp_ok = arp_ok && bass->arp.v[ARP_ON] == 0 && bass->arp.count == 0;
    printf("arp: %u notes in 1 s running, %u stopped, hold, off %s\n", running_notes, free_notes, arp_ok ? "ok" : "FAIL");

    /* Looper: 1 bar, three notes, then it plays; overdub adds one more. */
    hold(BTN_EDIT, 1);                /* empty pattern, so only the loop plays */
    tap_button(BTN_REC);
    hold(BTN_EDIT, 0);
    tap_button(BTN_PLAY);
    tap_button(BTN_LFO);              /* LOOP page */
    turn(2, -3);                      /* 1 bar */
    hold(BTN_GLO, 1);
    tap_button(BTN_REC);              /* arm */
    hold(BTN_GLO, 0);
    int loop_ok = bass->loop.state == LOOP_ARMED && loop_bars(&bass->loop) == 1;
    run(200);
    tap_key(0);
    loop_ok = loop_ok && bass->loop.state == LOOP_RECORDING;
    run(300);
    tap_key(4);
    run(300);
    tap_key(7);
    run(2500);
    loop_ok = loop_ok && bass->loop.state == LOOP_PLAYING && bass->loop.count == 6;
    int heard = 0;
    for (unsigned i = 0; i < 500; ++i) {
        run(5);
        for (unsigned k = 0; k < 16; ++k) heard |= bass->loop.sounding[k] != 0;
    }
    shot(dir, "13_bass_loop");
    hold(BTN_GLO, 1);
    tap_button(BTN_REC);              /* overdub */
    hold(BTN_GLO, 0);
    loop_ok = loop_ok && bass->loop.state == LOOP_OVERDUB;
    tap_key(9);
    hold(BTN_GLO, 1);
    tap_button(BTN_REC);              /* plays again */
    hold(BTN_GLO, 0);
    loop_ok = loop_ok && heard && bass->loop.state == LOOP_PLAYING && bass->loop.count == 8;
    printf("loop: %u events, played back %s, overdub %s\n", bass->loop.count, heard ? "yes" : "no", loop_ok ? "ok" : "FAIL");

    tap_button(BTN_SAVE);             /* SAVE twice: everything, loops included */
    tap_button(BTN_SAVE);
    static el_engine_t again;
    el_init(&again);
    int kept = el_store_load(&again) == 0 && again.track[2].loop.count == bass->loop.count &&
               again.track[2].loop.state == LOOP_PLAYING && loop_bars(&again.track[2].loop) == 1 &&
               again.track[2].arp.v[ARP_HOLD] == 1 && again.track[2].arp.v[ARP_RATE] == bass->arp.v[ARP_RATE];
    for (unsigned k = 0; kept && k < bass->loop.count; ++k)
        kept = again.track[2].loop.ev[k].pos == bass->loop.ev[k].pos && again.track[2].loop.ev[k].note == bass->loop.ev[k].note &&
               !again.track[2].loop.ev[k].velocity == !bass->loop.ev[k].velocity;
    hold(BTN_EDIT, 1);
    hold(BTN_GLO, 1);
    tap_button(BTN_REC);              /* EDIT + GLO + REC: clear */
    hold(BTN_GLO, 0);
    hold(BTN_EDIT, 0);
    kept = kept && bass->loop.state == LOOP_EMPTY && bass->loop.count == 0;
    printf("store: loop and arp kept over restart, clear %s\n", kept ? "ok" : "FAIL");
    return same && arp_ok && loop_ok && kept ? 0 : 1;
}
