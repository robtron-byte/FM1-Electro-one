/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host panel simulator: runs the engine and the UI against stubbed FM1
 * panel calls, plays a scripted session and writes screenshots (PPM) plus
 * the LED state next to each. Usage: ui_sim <output directory> */
#include "el_ui.h"
#include "el_editor.h"
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

    /* BEAT by the panel layout: PRESETS kit, GLO + ALGORITHM length,
     * EDIT + ALGORITHM swing, GLO + SELECT instrument (PULSE too). */
    unsigned kit_before = engine.drums.kit_index;
    turn(6, 1);
    hold(BTN_GLO, 1);
    turn(1, -4);
    turn(0, 20);
    hold(BTN_GLO, 0);
    hold(BTN_EDIT, 1);
    turn(1, 3);
    hold(BTN_EDIT, 0);
    run(100);
    const dr_pattern_t *bp = &engine.drums.pattern[engine.drums.current];
    int beat_ok = engine.drums.kit_index == kit_before + 1 && bp->length == 12 && bp->swing > 50 && ui.part == DR_PARTS - 1;
    printf("layout: beat kit %s, length %u, swing %u, part %s %s\n", dr_kit_names[engine.drums.kit_index], bp->length,
           bp->swing, dr_part_names[ui.part], beat_ok ? "ok" : "FAIL");
    shot(dir, "02b_beat_pulse");
    turn(6, -1);
    hold(BTN_GLO, 1);
    turn(1, 4);
    hold(BTN_GLO, 0);

    tap_button(BTN_SEL);              /* RHYTHM */
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
    tap_button(BTN_SEL);              /* GLO + RHYTHM: mute it */
    hold(BTN_GLO, 0);
    turn(5, -6);                      /* bass level down */
    run(400);
    shot(dir, "08_home");
    hold(BTN_EDIT, 1);                /* HOME + EDIT: distortion sends */
    turn(2, 20);                      /* BEAT send */
    turn(5, 30);                      /* BASS send */
    turn(6, 1);                       /* PRESETS: type TUBE */
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

    tap_button(BTN_SAVE);             /* SAVE twice: everything, HOLD is still on */
    tap_button(BTN_SAVE);
    static el_engine_t again;
    el_init(&again);
    int kept = el_store_load(&again) == 0 && again.track[2].arp.v[ARP_HOLD] == 1 &&
               again.track[2].arp.v[ARP_RATE] == bass->arp.v[ARP_RATE] && again.track[2].arp.v[ARP_GATE] == bass->arp.v[ARP_GATE];
    printf("store: arp settings kept over restart %s\n", kept ? "ok" : "FAIL");
    /* Synth editor: every parameter once (MKS-50 without its aftertouch
     * depths), EDIT + area button opens it, SELECT section, ALGORITHM page,
     * KNOB 1 edits, keys play even in STEP mode, the area button leaves. */
    int ed_ok = 1;
    for (unsigned engine = 0; engine < SYN_ENGINES; ++engine) {
        unsigned count = syn_value_count(engine), seen[128] = {0};
        for (unsigned sct = 0; sct < ed_sections(engine); ++sct) {
            const ed_section_t *sec = ed_section(engine, sct);
            for (unsigned i = 0; i < sec->count; ++i) ++seen[sec->param[i]];
        }
        for (unsigned prm = 0; prm < count; ++prm) {
            int want = !(engine == SYN_MKS && (prm == MP_DCO_AFTER || prm == MP_VCF_AFTER || prm == MP_VCA_AFTER));
            if ((int)seen[prm] != want) { printf("  editor: %s %s seen %u\n", syn_engine_names[engine], syn_param_name(engine, prm), seen[prm]); ed_ok = 0; }
        }
    }
    tap_button(BTN_LFO);                  /* BASS (leaves any other page) */
    if (!ui.step_mode[EL_AREA_BASS]) tap_button(BTN_SEQ);   /* STEP mode on: the editor must still play */
    hold(BTN_EDIT, 1);
    tap_button(BTN_LFO);
    hold(BTN_EDIT, 0);
    run(100);
    ed_ok = ed_ok && ui.editor && ui.area == EL_AREA_BASS && leds[BTN_EDIT];
    shot(dir, "14_editor_first");
    unsigned engine = bass->part.engine;
    turn(0, 1);                           /* SELECT: next section */
    run(50);
    unsigned sec1 = ui.ed_section[EL_AREA_BASS];
    unsigned prm1 = ed_param(engine, sec1, 0, 0);
    int before_v = syn_values(&bass->part)[prm1];
    turn(2, 3);                           /* KNOB 1 */
    run(200);
    int after_v = syn_values(&bass->part)[prm1];
    shot(dir, "15_editor_section2");
    ed_ok = ed_ok && sec1 == 1 && after_v != before_v;
    printf("editor: %s section %s, %s %d -> %d\n", syn_engine_names[engine], ed_section(engine, sec1)->name,
           syn_param_name(engine, prm1), before_v, after_v);
    turn(1, 1);                           /* ALGORITHM: page 2 of the section */
    run(50);
    ed_ok = ed_ok && ui.ed_page[EL_AREA_BASS] == (ed_pages(ed_section(engine, sec1)) > 1 ? 1 : 0);
    st_pattern_t pat_before = bass->pattern[bass->current];
    tap_key(4);                           /* plays, does not switch a step */
    ed_ok = ed_ok && !memcmp(&pat_before, &bass->pattern[bass->current], sizeof pat_before);
    turn(0, 2);                           /* the envelope */
    run(100);
    shot(dir, "16_editor_env");
    hold(BTN_EDIT, 1);                    /* other engine */
    turn(6, 1);
    hold(BTN_EDIT, 0);
    run(300);
    turn(0, -10);
    turn(0, 5);                           /* JP-6: ENVELOPE 1 */
    run(100);
    shot(dir, "17_editor_jp6_env1");
    turn(0, 2);                           /* LFO */
    run(100);
    shot(dir, "18_editor_jp6_lfo");
    turn(0, -4);                          /* VCF */
    run(100);
    shot(dir, "19_editor_jp6_vcf");
    ed_ok = ed_ok && bass->part.engine == SYN_JP6;
    tap_button(BTN_LFO);                  /* the area button leaves */
    ed_ok = ed_ok && !ui.editor;
    printf("editor: all parameters, navigation, keys, leave %s\n", ed_ok ? "ok" : "FAIL");
    return same && arp_ok && kept && beat_ok && ed_ok ? 0 : 1;
}
