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
static uint32_t flash_bytes = sizeof flash;   /* the partition size reported */
static int leds[41];
static uint32_t clock_ms;

/* Stubs for the SDK panel API. */
uint32_t fm1_millis(void) { return clock_ms; }
void fm1_led_set(unsigned key, int on) { if (key < 41) leds[key] = on; }
int fm1_screen_pending(void) { return 0; }
uint32_t fm1_flash_size(void) { return flash_bytes; }
int fm1_flash_read(uint32_t offset, void *data, size_t bytes)
{
    if (offset + bytes > flash_bytes) return FM1_FLASH_INVALID;
    memcpy(data, flash + offset, bytes);
    return 0;
}
static unsigned erases;   /* (the store test: a prepared save erases nothing) */
int fm1_flash_erase(uint32_t offset)
{
    ++erases;
    if (offset % FM1_FLASH_SECTOR_BYTES || offset >= flash_bytes) return FM1_FLASH_INVALID;
    memset(flash + offset, 0xff, FM1_FLASH_SECTOR_BYTES);
    return 0;
}
int fm1_flash_write(uint32_t offset, const void *data, size_t bytes)
{
    const uint8_t *p = data;
    if (offset + bytes > flash_bytes) return FM1_FLASH_INVALID;
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
    ui.sound_get = el_sound_get;
    ui.sound_put = el_sound_put;
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
    for (int i = 0; i < 4; ++i) turn(6, -1);   /* PRESETS: type OFF (one a turn): nothing can be sent (docs/adr/0031) */
    turn(2, 20);                      /* BEAT send */
    turn(5, 30);                      /* BASS send */
    run(300);
    shot(dir, "08a_home_dist_off");
    int send_locked = engine.fx.v[FX_DIST_MODE] == DIST_OFF && engine.fx.v[FX_DIST_SEND_BEAT] == 0 && engine.fx.v[FX_DIST_SEND_BASS] == 0;
    for (int i = 0; i < 4; ++i) turn(6, 1);    /* PRESETS: type TUBE */
    turn(2, 20);                      /* BEAT send */
    turn(5, 30);                      /* BASS send */
    run(300);
    shot(dir, "08b_home_distortion");
    hold(BTN_EDIT, 0);
    printf("dist: send beat %u bass %u type %s, locked while OFF %s\n", engine.fx.v[FX_DIST_SEND_BEAT], engine.fx.v[FX_DIST_SEND_BASS],
           fx_dist_mode_name(engine.fx.v[FX_DIST_MODE]),
           send_locked && engine.fx.v[FX_DIST_SEND_BEAT] > 0 && engine.fx.v[FX_DIST_SEND_BASS] > 0 && engine.fx.v[FX_DIST_MODE] == DIST_TUBE ? "ok" : "FAIL");

    hold(BTN_HOME, 1);                /* HOME held + SAVE: OPTIONS */
    tap_button(BTN_SAVE);
    hold(BTN_HOME, 0);
    int menu = ui.options && ui.overview;
    turn(0, 2);                       /* SELECT: MIDI CH STRINGS */
    turn(2, 3);                       /* CH 2 -> CH 5 */
    turn(0, -2);                      /* MIDI CH BEAT */
    turn(6, 20);                      /* PRESETS: CH 10 -> OFF */
    turn(1, 6);                       /* ALGORITHM: COMP THRESHOLD */
    turn(2, 6);                       /* -14 -> -20 dB */
    turn(0, 1);                       /* COMP RATIO */
    turn(2, 3);                       /* 1.8:1 -> 4:1 */
    turn(1, 4);                       /* COMP MIX */
    turn(2, -4);                      /* 100 % -> 80 % compressed, 20 % of the dry signal */
    int mix_ok = engine.option[EL_OPT_COMP_DRY] == 4;
    turn(1, 2);                       /* OUTPUT LEVEL */
    turn(3, -10);                     /* 127 -> 107 */
    turn(0, 2);                       /* HELP LINE */
    run(1500);                        /* the groove plays: the compressor works */
    int compressing = engine.mix.gain_reduction_db < 0.0f;
    turn(4, -1);                      /* off */
    run(300);
    shot(dir, "08c_options");
    menu = menu && mix_ok && engine.mix.master_comp.dry > 0.19f && engine.mix.master_comp.dry < 0.21f && ui.opt_item == 15 && !engine.option[EL_OPT_HINTS] && engine.option[EL_OPT_OUT_LEVEL] == 107 &&
           engine.mix.master_comp.threshold_db == -20.0f && engine.mix.master_comp.ratio == 4.0f && compressing;
    printf("comp: threshold %.0f dB, ratio %.1f:1, gain reduction %.1f dB %s\n", engine.mix.master_comp.threshold_db,
           engine.mix.master_comp.ratio, engine.mix.gain_reduction_db, menu ? "ok" : "FAIL");
    tap_button(BTN_HOME);             /* done: stored */
    menu = menu && !ui.options && ui.overview;
    {
        const uint8_t on_new[3] = {0x94, 77, 100}, off_new[3] = {0x84, 77, 0}, on_old[3] = {0x91, 78, 100};
        int routed = engine.midi_channel[EL_AREA_STRINGS] == 4 && engine.midi_channel[EL_AREA_BEAT] == EL_MIDI_OFF;
        routed = routed && el_midi_input(&engine, on_new, 3) == 1;
        run(50);
        routed = routed && engine.track[1].last_note == 77;
        routed = routed && el_midi_input(&engine, on_old, 3) == 0;   /* channel 2 now plays nothing */
        el_midi_input(&engine, off_new, 3);
        run(50);
        routed = routed && engine.track[1].last_note == 77;
        /* Stored only when the groove stops (a flash erase would stop the
         * sound while it plays), then kept over a restart. */
        static el_engine_t kept;
        el_init(&kept);
        int playing = engine.transport.running;
        routed = routed && playing && (el_store_load(&kept) != 0 || kept.option[EL_OPT_OUT_LEVEL] != 107);
        tap_button(BTN_PLAY);             /* STOP: now it is stored */
        run(300);
        el_init(&kept);
        routed = routed && el_store_load(&kept) == 0 && kept.option[EL_OPT_OUT_LEVEL] == 107 && !kept.option[EL_OPT_HINTS] &&
                 kept.mix.master_comp.threshold_db == -20.0f && kept.mix.master_comp.ratio == 4.0f &&
                 kept.midi_channel[EL_AREA_STRINGS] == 4 && kept.midi_channel[EL_AREA_BEAT] == EL_MIDI_OFF &&
                 kept.midi_channel[EL_AREA_RHYTHM] == 0 && kept.midi_channel[EL_AREA_BASS] == 2;
        printf("options: menu, STRINGS on CH %u, BEAT %s, level %u, help off, routed and kept over restart %s\n",
               engine.midi_channel[EL_AREA_STRINGS] + 1u, engine.midi_channel[EL_AREA_BEAT] == EL_MIDI_OFF ? "OFF" : "on",
               engine.option[EL_OPT_OUT_LEVEL], menu && routed ? "ok" : "FAIL");
        tap_button(BTN_PLAY);             /* and on again */
        run(100);
        el_post(&engine, EL_CMD_MIDI_CHANNEL, EL_AREA_STRINGS, 1);   /* back to the defaults */
        el_post(&engine, EL_CMD_MIDI_CHANNEL, EL_AREA_BEAT, 9);
        el_post(&engine, EL_CMD_OPTION, EL_OPT_OUT_LEVEL, 127);
        el_post(&engine, EL_CMD_OPTION, EL_OPT_HINTS, 1);
        el_post(&engine, EL_CMD_OPTION, EL_OPT_COMP_THRESHOLD, 14);
        el_post(&engine, EL_CMD_OPTION, EL_OPT_COMP_RATIO, 2);
        run(50);
    }

    /* HOME: white keys pick the slot per area (5-8 RHYTHM, 9-12 STRINGS). */
    static const unsigned white[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    tap_key(white[5]);                /* RHYTHM B */
    tap_key(white[10]);               /* STRINGS C */
    run(4000);                        /* switched at the next bar */
    int slots = engine.drums.current == 0 && engine.track[0].current == 1 && engine.track[1].current == 2 &&
                engine.track[2].current == 0;
    /* ARP + white key 2: new groove in every area, only on the second press. */
    dr_pattern_t groove = engine.drums.pattern[0];
    hold(BTN_ARP, 1);
    tap_key(white[1]);
    run(300);
    shot(dir, "09_ask_new_groove");
    int asked = !memcmp(&groove, &engine.drums.pattern[0], sizeof groove) && ui.ask;
    tap_key(white[1]);
    hold(BTN_ARP, 0);
    run(300);
    shot(dir, "09_new_groove");
    int fresh = asked && memcmp(&groove, &engine.drums.pattern[0], sizeof groove) != 0;
    /* ARP + key 1 in STRINGS: a variation of slot C, again only when asked twice. */
    tap_button(BTN_ENV);
    st_pattern_t strings = engine.track[1].pattern[2];
    hold(BTN_ARP, 1);
    tap_key(white[0]);
    int still = !memcmp(&strings, &engine.track[1].pattern[2], sizeof strings);
    tap_button(BTN_OCT_UP);           /* another button: the question is gone */
    tap_key(white[0]);
    still = still && !memcmp(&strings, &engine.track[1].pattern[2], sizeof strings);
    tap_key(white[0]);
    hold(BTN_ARP, 0);
    run(300);
    int varied = still && memcmp(&strings, &engine.track[1].pattern[2], sizeof strings) != 0 &&
                 !memcmp(&engine.track[1].pattern[0], &engine.track[1].pattern[0], sizeof strings);
    printf("slots: per area A-D, new groove and variation after a second press %s\n", slots && fresh && varied ? "ok" : "FAIL");
    if (!(slots && fresh && varied)) printf("debug slots %d (%u %u %u %u) asked %d fresh %d still %d varied %d\n", slots, engine.drums.current, engine.track[0].current, engine.track[1].current, engine.track[2].current, asked, fresh, still, varied);
    tap_button(BTN_HOME);
    /* Generated parts are kept in Flash by themselves (once the groove
     * stops) and come back after a restart. */
    run(2500);
    int was_running = engine.transport.running;
    if (was_running) { tap_button(BTN_PLAY); run(300); }
    static el_engine_t restarted;
    el_init(&restarted);
    int lr = el_store_load(&restarted);
    fresh = fresh && lr == 0 &&
            !memcmp(&restarted.drums.pattern[0], &engine.drums.pattern[0], sizeof(dr_pattern_t)) &&
            !memcmp(&restarted.track[1].pattern[2], &engine.track[1].pattern[2], sizeof(st_pattern_t)) &&
            restarted.track[0].current == 1 && restarted.track[1].current == 2;
    printf("store: new groove and slots kept over restart %s\n", fresh ? "ok" : "FAIL");
    if (was_running) { tap_button(BTN_PLAY); run(100); }

    tap_button(BTN_PLAY);
    tap_button(BTN_SAVE);             /* SAVE: everything */
    run(100);
    shot(dir, "11_saved");
    static el_engine_t loaded;
    el_init(&loaded);
    int r = el_store_load(&loaded);
    int same = r == 0 && !memcmp(loaded.drums.pattern, engine.drums.pattern, sizeof loaded.drums.pattern);
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

    /* FM1-Electro's own Flash layout (tools/package_fm1.py): a user
     * partition smaller than FM1 Open's; grooves and sounds in its last
     * eight sectors, apart from each other. */
    {
        static uint8_t kept_flash[sizeof flash];
        memcpy(kept_flash, flash, sizeof flash);
        memset(flash, 0xff, sizeof flash);
        flash_bytes = 384u * 1024u;
        uint32_t top = flash_bytes - 8u * FM1_FLASH_SECTOR_BYTES;
        int own = el_store_address(0) == top + 2u * FM1_FLASH_SECTOR_BYTES && el_store_address(1) == top + 5u * FM1_FLASH_SECTOR_BYTES &&
                  el_sound_address(0) == top && el_sound_address(1) == top + FM1_FLASH_SECTOR_BYTES;
        el_sound_t snd = {0}, back;
        memcpy(snd.name, "OWN", 4);
        snd.engine = engine.track[2].part.engine;
        memcpy(snd.v, syn_values(&engine.track[2].part), syn_value_count(snd.engine));
        own = own && el_store_save(&engine) == 0 && el_sound_put(0, &snd) == 0 && el_store_save(&engine) == 0;
        static el_engine_t small;
        el_init(&small);
        own = own && el_store_load(&small) == 0 && !memcmp(small.drums.pattern, engine.drums.pattern, sizeof small.drums.pattern) &&
              el_sound_get(0, &back) == 0 && !strcmp(back.name, "OWN");
        for (uint32_t i = 0; i < top; ++i) own = own && flash[i] == 0xff;   /* nothing below its area */
        flash_bytes = sizeof flash;
        memcpy(flash, kept_flash, sizeof flash);
        printf("store: own Flash layout (small partition) %s\n", own ? "ok" : "FAIL");
    }

    /* HOME: black keys 5 and 6 punch in the stutter and the reverb throw
     * while held; black 8 (repeat) starts with white key 2, step 2, and
     * changes no slot; black 9-11 are delay, bitcrusher, phaser. */
    if (!ui.overview) tap_button(BTN_HOME);
    unsigned slot_before = engine.drums.current;
    panel.keys |= 1u << 10; run(30);          /* black 5 */
    int punched = engine.punch == EL_PUNCH_STUTTER;
    panel.keys |= 1u << 13; run(30);          /* black 6 as well */
    punched = punched && engine.punch == (EL_PUNCH_STUTTER | EL_PUNCH_THROW) && engine.fx.throw_on;
    shot(dir, "11a_punch");
    panel.keys &= ~(1u << 10 | 1u << 13); run(60);
    punched = punched && engine.punch == 0 && !engine.fx.throw_on;
    panel.keys |= 1u << 17; run(30);          /* black 8: repeat, waiting for its step */
    punched = punched && engine.punch == 0;
    panel.keys |= 1u << 2; run(30);           /* white 2: step 2, repeat starts */
    panel.keys &= ~(1u << 2); run(30);
    punched = punched && engine.punch == EL_PUNCH_REPEAT && engine.punch_div[3] == 1 && engine.punch_div[0] == 3;
    panel.keys &= ~(1u << 17); run(60);
    panel.keys |= 1u << 20 | 1u << 22 | 1u << 25; run(30);   /* black 9-11 */
    punched = punched && engine.punch == (EL_PUNCH_DELAY | EL_PUNCH_CRUSH | EL_PUNCH_PHASER);
    panel.keys &= ~(1u << 20 | 1u << 22 | 1u << 25); run(60);
    punched = punched && engine.punch == 0 && engine.drums.current == slot_before;
    printf("punch keys: held on, let go off %s\n", punched ? "ok" : "FAIL");

    /* Step record in RHYTHM (SH-101 / KeyStep): stopped, REC, keys. */
    if (engine.transport.running) tap_button(BTN_PLAY);
    tap_button(BTN_SEL);
    if (ui.step_mode[EL_AREA_RHYTHM]) tap_button(BTN_SEQ);
    int live_default = !(engine.option[EL_OPT_REC_STEP] & 1);
    hold(BTN_GLO, 1); tap_button(BTN_REC); hold(BTN_GLO, 0);   /* REC mode: SH-101 */
    run(50);
    int switched = (engine.option[EL_OPT_REC_STEP] & 1) && !engine.track[0].recording;
    tap_button(BTN_REC);
    panel.keys |= 1u << 4; run(30);           /* A3 ... */
    panel.keys |= 1u << 6; run(30);           /* ... B3 while A3 is held: legato */
    panel.keys &= ~(1u << 4 | 1u << 6); run(30);
    hold(BTN_GLO, 1); tap_key(9); hold(BTN_GLO, 0);    /* B3 one more step */
    tap_key(UI_KEYS - 1u);                              /* the last key: a rest */
    tap_key(7);                                         /* C4 */
    run(100);
    shot(dir, "11b_step_rec");
    tap_button(BTN_REC);
    run(100);
    {
        const st_pattern_t *p = &engine.track[0].pattern[engine.track[0].current];
        const st_step_t *st = p->step;
        int ok = p->length == 5 && st[0].note == 57 && (st[0].flags & (ST_ON | ST_TIE)) == (ST_ON | ST_TIE) &&
                 st[1].note == 59 && (st[1].flags & (ST_ON | ST_TIE)) == (ST_ON | ST_TIE) &&
                 st[2].note == 59 && (st[2].flags & (ST_ON | ST_TIE)) == ST_ON && !(st[3].flags & ST_ON) &&
                 st[4].note == 60 && (st[4].flags & ST_ON) && !engine.track[0].recording && live_default && switched;
        /* TRANSPOSE: GLO + SEQ, then D4 moves the sequence up a tone. */
        hold(BTN_GLO, 1); tap_button(BTN_SEQ); hold(BTN_GLO, 0);
        tap_key(9);
        run(50);
        int moved = ui.transpose_mode[EL_AREA_RHYTHM] && engine.track[0].transpose == 2 && p->step[0].note == 57;
        hold(BTN_GLO, 1); tap_button(BTN_SEQ); hold(BTN_GLO, 0);
        run(50);
        moved = moved && !ui.transpose_mode[EL_AREA_RHYTHM] && engine.track[0].transpose == 0;
        /* GLO + REC back to LIVE: running, REC, a key lands in the pattern
         * and the sequence keeps its length. */
        hold(BTN_GLO, 1); tap_button(BTN_REC); hold(BTN_GLO, 0);
        tap_button(BTN_PLAY);
        run(200);
        tap_button(BTN_REC);
        tap_key(11);                  /* E4 = 64 */
        run(50);
        tap_button(BTN_REC);
        tap_button(BTN_PLAY);
        int live = 0;
        for (unsigned i = 0; i < p->length; ++i) live |= p->step[i].note == 64 && (p->step[i].flags & ST_ON);
        live = live && p->length == 5 && !(engine.option[EL_OPT_REC_STEP] & 1);
        printf("rec modes: GLO + REC switches LIVE / SH-101; live take keeps the length %s\n", live ? "ok" : "FAIL");
        printf("steprec: 5 steps, legato tie, hold, rest, length from the steps; transpose +2 %s\n", ok && moved ? "ok" : "FAIL");
        if (!(ok && moved)) {
            printf("debug len %u:", p->length);
            for (unsigned i = 0; i < 6; ++i) printf(" %u/%x", st[i].note, st[i].flags);
            printf(" transpose %d\n", engine.track[0].transpose);
        }
    }

    /* STRINGS (docs/adr/0027): a live take on an empty pattern records in
     * 8ths, a step take keeps the step length; steps run up to 8 bars. */
    {
        tap_button(BTN_ENV);
        st_track_t *str = &engine.track[1];
        engine.option[EL_OPT_REC_STEP] &= (uint8_t)~2u;   /* live */
        hold(BTN_EDIT, 1); tap_button(BTN_REC); hold(BTN_EDIT, 0);   /* clear the pattern */
        el_post3(&engine, EL_CMD_TRACK_RATE, 1, ST_RATE_BAR, 0, 0);
        run(20);
        tap_button(BTN_REC);
        run(20);
        int live_8th = str->recording && str->pattern[str->current].rate == ST_RATE_8TH && str->pattern[str->current].length == ST_STEPS;
        tap_button(BTN_REC);
        run(20);
        engine.option[EL_OPT_REC_STEP] |= 2u;   /* step */
        el_post3(&engine, EL_CMD_TRACK_RATE, 1, ST_RATE_BAR, 0, 0);
        run(20);
        tap_button(BTN_REC);
        run(20);
        int step_kept = str->recording && str->pattern[str->current].rate == ST_RATE_BAR;
        tap_button(BTN_REC);
        run(20);
        engine.option[EL_OPT_REC_STEP] &= (uint8_t)~2u;
        /* A step of 8 bars lasts 128 sixteenths. */
        static st_track_t slow;
        st_init(&slow, MKS_AREA_STRINGS, 3, 5);
        st_pattern_t *sp = &slow.pattern[0];
        sp->rate = ST_RATE_8BARS;
        sp->length = 2;
        sp->step[0].flags = sp->step[1].flags = ST_ON;
        unsigned moves = 0, first = 0, second = 0;
        for (unsigned i = 0; i < 256; ++i) {
            unsigned before = slow.pos;
            st_step(&slow, i, 1000.0f);
            if (i && slow.pos != before) { ++moves; if (moves == 1) first = i; else second = i; }
        }
        int long_ok = moves == 1 && first == 128;   /* the second change would come at 256 */
        printf("strings: live take in 8ths, step take keeps its steps, 8-bar steps change at 128 sixteenths %s\n",
               live_8th && step_kept && long_ok ? "ok" : "FAIL");
        if (!(live_8th && step_kept && long_ok))
            printf("debug live_8th %d step_kept %d long_ok %d (moves %u first %u second %u)\n", live_8th, step_kept, long_ok, moves, first, second);
    }

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
    /* In the editor EDIT blinks (docs/adr/0020): it changes within 0.5 s. */
    int edit_was = leds[BTN_EDIT], edit_changed = 0;
    for (int i = 0; i < 10 && !edit_changed; ++i) { run(50); edit_changed = leds[BTN_EDIT] != edit_was; }
    ed_ok = ed_ok && ui.editor && ui.area == EL_AREA_BASS && edit_changed;
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
    run(20);
    /* Outside it every function button is lit (ADR 0020), EDIT too. */
    ed_ok = ed_ok && !ui.editor && leds[BTN_EDIT] && leds[BTN_FX] && leds[BTN_GLO] && leds[BTN_HOME];
    printf("editor: all parameters, navigation, keys, leave %s\n", ed_ok ? "ok" : "FAIL");
    /* Area buttons: the chosen one full, the others dimmed (2); at the end
     * of the keys' octave its OCT button blinks, the other stays lit. */
    int led_ok = ui.area == EL_AREA_BASS && leds[BTN_LFO] == 1 && leds[BTN_FX] == 2 && leds[BTN_SEL] == 2 &&
                 leds[BTN_ENV] == 2 && leds[BTN_HOME] == 2 && leds[BTN_GLO] == 1;
    int step_was = ui.step_mode[EL_AREA_BASS], oct_was = ui.octave[EL_AREA_BASS];
    if (step_was) tap_button(BTN_SEQ);
    int down_seen[3] = {0}, up_seen[3] = {0};
    for (int i = 0; i < 20 && !down_seen[0]; ++i) { run(50); down_seen[leds[BTN_OCT_DOWN]] = 1; }
    led_ok = led_ok && !down_seen[0];              /* not at the end: lit */
    for (int i = 0; i < 6; ++i) tap_button(BTN_OCT_DOWN);
    down_seen[0] = down_seen[1] = 0;
    for (int i = 0; i < 20; ++i) { run(50); down_seen[leds[BTN_OCT_DOWN]] = 1; up_seen[leds[BTN_OCT_UP]] = 1; }
    led_ok = led_ok && ui.octave[EL_AREA_BASS] == EL_OCT_MIN && down_seen[0] && down_seen[1] && !up_seen[0];
    for (int i = ui.octave[EL_AREA_BASS]; i < oct_was; ++i) tap_button(BTN_OCT_UP);
    if (step_was) tap_button(BTN_SEQ);
    run(20);
    led_ok = led_ok && ui.octave[EL_AREA_BASS] == oct_was && ui.step_mode[EL_AREA_BASS] == step_was;
    tap_button(BTN_HOME);
    run(20);
    led_ok = led_ok && ui.overview && leds[BTN_HOME] == 1 && leds[BTN_FX] == 2 && leds[BTN_LFO] == 2;
    tap_button(BTN_LFO);
    run(20);
    printf("leds: area buttons dimmed, octave end blinks %s\n", led_ok ? "ok" : "FAIL");
    /* User sounds: SAVE in the editor, rename, slot 3, SAVE stores; PRESETS
     * finds it before the factory sounds, GLO + PRESETS straight away; another button cancels; the groove
     * keeps pointing at it over a restart. */
    hold(BTN_EDIT, 1);
    tap_button(BTN_LFO);
    hold(BTN_EDIT, 0);
    turn(2, 4);                           /* change a knob: a sound of our own */
    run(200);
    tap_button(BTN_SAVE);
    int us_ok = ui.sound_save == 1;
    turn(6, -40);                         /* PRESETS: slot 1 */
    turn(6, 2);                           /* slot 3 */
    turn(2, -20);                         /* KNOB 1: first letter */
    turn(3, 1);                           /* KNOB 2: next letter */
    run(100);
    char typed = ui.sound_name[0];
    shot(dir, "20_sound_save");
    tap_button(BTN_SAVE);
    run(400);
    el_sound_t got;
    int gr = el_sound_get(2, &got);
    us_ok = us_ok && !ui.sound_save && gr == 0 && got.used && got.engine == bass->part.engine && got.name[0] == typed &&
            !memcmp(got.v, syn_values(&bass->part), syn_value_count(got.engine)) && bass->preset == EL_USER_SOUND + 2;
    shot(dir, "21_sound_saved");
    turn(6, 1);                           /* the user sounds come first: on to the factory sounds */
    run(300);
    us_ok = us_ok && bass->preset < EL_USER_SOUND;
    turn(6, 200);                         /* to the end of the factory sounds */
    run(300);
    us_ok = us_ok && bass->preset < EL_USER_SOUND;
    hold(BTN_GLO, 1);
    turn(6, 1);                           /* GLO + PRESETS: straight to the user sounds */
    hold(BTN_GLO, 0);
    run(300);
    us_ok = us_ok && bass->preset == EL_USER_SOUND + 2;
    turn(6, 1);                           /* on to the first factory sound */
    run(300);
    us_ok = us_ok && bass->preset < EL_USER_SOUND;
    turn(6, -1);                          /* one click left of it: the user sound */
    run(300);
    us_ok = us_ok && bass->preset == EL_USER_SOUND + 2;
    tap_button(BTN_SAVE);                 /* cancel with another button */
    tap_button(BTN_OCT_UP);
    us_ok = us_ok && !ui.sound_save && ui.editor;
    tap_button(BTN_LFO);                  /* leave the editor, store the groove */
    tap_button(BTN_SAVE);
    tap_button(BTN_SAVE);
    static el_engine_t reboot;
    el_init(&reboot);
    us_ok = us_ok && el_store_load(&reboot) == 0 && reboot.track[2].preset == EL_USER_SOUND + 2 &&
            !memcmp(syn_values(&reboot.track[2].part), got.v, syn_value_count(got.engine));
    printf("sounds: user slot 3 \"%s\" (%s) stored, found, cancel, restart %s\n", got.name, syn_engine_names[got.engine],
           us_ok ? "ok" : "FAIL");

    /* Own factory sounds (docs/adr/0027): after both engines' sounds, found by
     * name, in the area they were made for, with the values of the table. */
    {
        unsigned base = mks_preset_count + jp6_preset_count;
        int ex_ok = syn_extra_count >= 20 && syn_sound_count() == base + syn_extra_count && base + syn_extra_count < EL_USER_SOUND;
        unsigned in_area[4] = {0, 0, 0, 0};
        for (unsigned i = 0; i < syn_extra_count; ++i) {
            const syn_extra_t *x = &syn_extras[i];
            int id = syn_sound_find(x->engine, x->name);
            ex_ok = ex_ok && id == (int)(base + i) && syn_sound_area((unsigned)id) == x->area && syn_sound_engine((unsigned)id) == x->engine &&
                    !strcmp(syn_sound_name((unsigned)id), x->name) && syn_sound_values((unsigned)id) == x->v && x->area >= 1 && x->area <= 3;
            if (x->area <= 3) ++in_area[x->area];
        }
        ex_ok = ex_ok && in_area[1] >= 6 && in_area[2] >= 6 && in_area[3] >= 6;
        printf("extras: %u own sounds (rhythm %u, strings %u, bass %u) after the factory ones %s\n", syn_extra_count, in_area[1],
               in_area[2], in_area[3], ex_ok ? "ok" : "FAIL");
    }

    /* A sound per slot (docs/adr/0024): A's knobs come back after B, also
     * after a restart; a version 20 record gives every slot its one sound. */
    int ps_ok = 1;
    {
        static el_engine_t ps, back, v20;
        float audio[EL_BLOCK * 2];
        el_init(&ps);
        el_generate_all(&ps, 7);
        dr_machine_t *m = &ps.drums;
        st_track_t *st = &ps.track[1];
        dr_post(m, DR_CMD_PARAM, D8_BD, 1, 0, 100);              /* A: BD tune 100 */
        el_post3(&ps, EL_CMD_TRACK_PARAM, 1, MP_CUTOFF, 0, 17);  /* A: strings cutoff 17 */
        el_render_float(&ps, audio, EL_BLOCK);
        ps_ok = ps_ok && tr808_get(&m->kit, D8_BD, 1) == 100 && syn_values(&st->part)[MP_CUTOFF] == 17;
        dr_post(m, DR_CMD_SELECT, 1, 1, 0, 0);
        el_post3(&ps, EL_CMD_TRACK_SELECT, 1, 1, 1, 0);
        el_render_float(&ps, audio, EL_BLOCK);
        ps_ok = ps_ok && m->current == 1 && st->current == 1 &&
                tr808_get(&m->kit, D8_BD, 1) == 64 && syn_values(&st->part)[MP_CUTOFF] != 17;
        dr_post(m, DR_CMD_PARAM, D8_SD, 0, 0, 33);               /* B: SD level 33 */
        el_render_float(&ps, audio, EL_BLOCK);
        dr_post(m, DR_CMD_SELECT, 0, 1, 0, 0);
        el_post3(&ps, EL_CMD_TRACK_SELECT, 1, 0, 1, 0);
        el_render_float(&ps, audio, EL_BLOCK);
        ps_ok = ps_ok && tr808_get(&m->kit, D8_BD, 1) == 100 && tr808_get(&m->kit, D8_SD, 0) != 33 &&
                syn_values(&st->part)[MP_CUTOFF] == 17;
        int sv = el_store_save(&ps);
        el_init(&back);
        int lb = el_store_load(&back);
        ps_ok = ps_ok && sv == 0 && lb == 0 && back.drums.current == 0 &&
                tr808_get(&back.drums.kit, D8_BD, 1) == 100 && syn_values(&back.track[1].part)[MP_CUTOFF] == 17 &&
                tr808_sound_get(&back.drums.sound[1], D8_SD, 0) == 33 && tr808_sound_get(&back.drums.sound[1], D8_BD, 1) == 64 &&
                back.track[1].sound[1].v[MP_CUTOFF] != 17 && back.track[1].sound[0].v[MP_CUTOFF] == 17;
        int dv = el_store_test_as_v20();
        el_init(&v20);
        int l20 = el_store_load(&v20);
        ps_ok = ps_ok && dv == 0 && l20 == 0;
        for (unsigned i = 0; i < DR_PATTERNS; ++i)
            ps_ok = ps_ok && tr808_sound_get(&v20.drums.sound[i], D8_BD, 1) == 100 && v20.track[1].sound[i].v[MP_CUTOFF] == 17;
        if (!ps_ok) printf("debug sv=%d lb=%d dv=%d l20=%d\n", sv, lb, dv, l20);
    }
    printf("slots: a sound per slot, back from B, restart, version 20 record %s\n", ps_ok ? "ok" : "FAIL");

    /* Step conditions (docs/adr/0025): the playback, Repeat and the panel
     * (a step held, PRESETS turned). */
    int sc_ok = 1;
    {
        static el_engine_t ce;
        el_init(&ce);
        dr_machine_t *m = &ce.drums;
        dr_pattern_t *p = &m->pattern[m->current];
        p->length = 4;
        for (unsigned i = 0; i < DR_PARTS; ++i) p->hit[i] = p->roll[i] = 0;
        p->hit[D8_BD] = 1u;                                   /* BD on step 1 */
        dr_post(m, DR_CMD_COND, D8_BD, 0, 0, EL_COND_2_2);
        dr_process_commands(m);
        unsigned played = 0;
        for (unsigned pass = 0; pass < 6; ++pass)
            for (unsigned st = 0; st < 4; ++st) {
                m->kit.v[D8_BD].age = 12345u;
                dr_step(m, pass * 4u + st, 1000.0f);
                if (st == 0 && m->kit.v[D8_BD].age == 0) played |= 1u << pass;
            }
        sc_ok = sc_ok && played == 0x2au;                    /* passes 2, 4, 6 */
        /* Repeat on step 1: the playhead's pass decides. */
        dr_rewind(m);
        unsigned rep = 0;
        for (unsigned n = 0; n < 8; ++n) {
            m->kit.v[D8_BD].age = 12345u;
            dr_step_at(m, n, 1000.0f, 0);
            if (m->kit.v[D8_BD].age == 0) rep |= 1u << n;
        }
        sc_ok = sc_ok && rep == 0xf0u;                       /* the second pass of the playhead */
        /* Synth: a 1:4 step plays on the first of every four passes. */
        st_track_t *t = &ce.track[0];
        st_pattern_t *sp = &t->pattern[t->current];
        sp->length = 2;
        sp->rate = ST_RATE_16TH;
        sp->step[0].flags = ST_ON;
        sp->step[1].flags = 0;
        el_post3(&ce, EL_CMD_STEP_COND, 0, 0, 0, EL_COND_1_4);
        float audio[EL_BLOCK * 2];
        el_render_float(&ce, audio, EL_BLOCK);
        sc_ok = sc_ok && ST_COND(sp->step[0].flags) == EL_COND_1_4;
        st_rewind(t);
        unsigned sp_played = 0;
        for (unsigned pass = 0; pass < 8; ++pass) {
            st_step(t, pass * 2u, 1000.0f);
            if (t->sounding_count) sp_played |= 1u << pass;
            st_step(t, pass * 2u + 1u, 1000.0f);
        }
        sc_ok = sc_ok && sp_played == 0x11u;                 /* passes 1 and 5 */
        /* Probability: 50 % plays about half the passes (dice), 100 % all. */
        dr_rewind(m);
        dr_post(m, DR_CMD_COND, D8_BD, 0, 0, EL_COND_ALWAYS);
        dr_post(m, DR_CMD_PROB, D8_BD, 0, 0, 5);
        dr_process_commands(m);
        unsigned hits = 0;
        for (unsigned pass = 0; pass < 400; ++pass)
            for (unsigned st = 0; st < 4; ++st) {
                m->kit.v[D8_BD].age = 12345u;
                dr_step(m, pass * 4u + st, 1000.0f);
                if (st == 0 && m->kit.v[D8_BD].age == 0) ++hits;
            }
        sc_ok = sc_ok && hits > 150 && hits < 250 && el_prob_percent(5) == 50;
        el_post3(&ce, EL_CMD_STEP_PROB, 0, 0, 0, 7);           /* synth step: 30 % */
        el_post3(&ce, EL_CMD_STEP_CHORD, 0, 0, 0, 2);          /* (a chord keeps it) */
        el_render_float(&ce, audio, EL_BLOCK);
        sc_ok = sc_ok && ST_PROB(&sp->step[0]) == 7 && ST_CHORD_TYPE(sp->step[0].chord) == 2 && ST_COND(sp->step[0].flags) == EL_COND_1_4;
        if (!sc_ok) printf("debug prob: hits %u of 400, synth prob %u\n", hits, ST_PROB(&sp->step[0]));
        if (!sc_ok) printf("debug cond: drums %02x repeat %02x synth %02x\n", played, rep, sp_played);
    }
    {
        /* The panel: BEAT, BD, step 15 held, PRESETS one detent: 1:2, and the
         * step stays on; held again without a turn: off, condition gone. */
        el_engine_t *sim = ui.engine;   /* (a local `engine` above hides the simulator's) */
        if (sim->transport.running) { tap_button(BTN_PLAY); run(100); }
        ui.overview = 0; ui.editor = 0; ui.area = EL_AREA_BEAT; ui.part = D8_BD; ui.step_page[0] = 0;
        dr_machine_t *m = &sim->drums;
        dr_post(m, DR_CMD_LENGTH, 0, 16, 0, 0);
        run(30);
        if ((m->pattern[m->current].hit[D8_BD] >> 14) & 1u) { tap_key(24); run(30); }
        panel.keys |= 1u << 24; run(30);
        panel.encoders[6] = 1; run(10);
        unsigned cond_right = dr_cond(m, m->current, D8_BD, 14);   /* right: 1:2 */
        panel.encoders[1] = -5; run(300);   /* ALGORITHM: 50 %, the passes stay; the big readout */
        unsigned on = (m->pattern[m->current].hit[D8_BD] >> 14) & 1u, cond = dr_cond(m, m->current, D8_BD, 14);
        unsigned prob = dr_prob(m, m->current, D8_BD, 14);
        shot(dir, "22_step_cond");
        panel.keys &= ~(1u << 24); run(30);
        unsigned kept_on = (m->pattern[m->current].hit[D8_BD] >> 14) & 1u;
        tap_key(24); run(30);
        unsigned off = !((m->pattern[m->current].hit[D8_BD] >> 14) & 1u) && dr_cond(m, m->current, D8_BD, 14) == 0;
        int panel_ok = on && cond_right == EL_COND_1_2 && cond == EL_COND_1_2 && prob == 5 && kept_on && off && dr_prob(m, m->current, D8_BD, 14) == 0;
        if (!panel_ok) printf("debug panel: on %u cond %u kept %u off %u\n", on, cond, kept_on, off);
        sc_ok = sc_ok && panel_ok;
    }
    {
        /* Erased ahead (docs/adr/0023): the save that follows erases
         * nothing and loads back; without, it erases. */
        el_engine_t *sim = ui.engine;
        int pr = el_store_prepare();
        unsigned before = erases;
        int sv = el_store_save(sim);
        unsigned during = erases - before;
        static el_engine_t back;
        el_init(&back);
        int lb = el_store_load(&back);
        before = erases;
        el_store_save(sim);   /* not prepared: the older copy holds the save before */
        unsigned unprepared = erases - before;
        int prep_ok = pr == 0 && sv == 0 && during == 0 && lb == 0 && unprepared > 0 &&
                      !memcmp(back.drums.pattern, sim->drums.pattern, sizeof back.drums.pattern);
        if (!prep_ok) printf("debug prepare: %d %d erases %u load %d unprepared %u\n", pr, sv, during, lb, unprepared);
        sc_ok = sc_ok && prep_ok;
        printf("store: erased ahead, the save erases nothing %s\n", prep_ok ? "ok" : "FAIL");
    }
    printf("steps: conditions 2:2 / 1:4, probability, under Repeat, held step + PRESETS / ALGORITHM %s\n", sc_ok ? "ok" : "FAIL");

    /* Drum parameter locks and the kit without toms (docs/adr/0028). */
    {
        int lk_ok = 1;
        el_engine_t *sim = ui.engine;
        dr_machine_t *m = &sim->drums;
        if (sim->transport.running) { tap_button(BTN_PLAY); run(100); }
        ui.overview = 0; ui.editor = 0; ui.area = EL_AREA_BEAT; ui.part = D8_BD; ui.step_page[0] = 0;
        dr_post(m, DR_CMD_LENGTH, 0, 16, 0, 0);
        run(30);
        const unsigned step = 14, key = 24;                                        /* white key 24 = step 15 */
        if (!((m->pattern[m->current].hit[D8_BD] >> step) & 1u)) { tap_key(key); run(30); }
        /* hold the step: KNOB 1 three semitones up, KNOB 2 three velocity steps up */
        panel.keys |= 1u << key; run(30);
        panel.encoders[2] = 3; run(10);
        shot(dir, "24_step_pitch");
        panel.encoders[3] = 1; run(10); panel.encoders[3] = 1; run(10); panel.encoders[3] = 5; run(10);   /* (one step a turn, whatever the count) */
        shot(dir, "23_step_locks");
        int pitch = dr_pitch(m, m->current, D8_BD, step), level = (int)dr_vol(m, m->current, D8_BD, step);
        panel.encoders[2] = -20; run(10);                                      /* far down: stops at -12 */
        int low = dr_pitch(m, m->current, D8_BD, step);
        panel.encoders[2] = 9; run(10);
        panel.keys &= ~(1u << key); run(30);
        lk_ok = lk_ok && pitch == 3 && level == 3 && low == -12 && dr_pitch(m, m->current, D8_BD, step) == -3 &&
                ((m->pattern[m->current].hit[D8_BD] >> step) & 1u);                   /* the step stays on */
        /* an instrument without a pitch takes the level but no pitch */
        ui.part = D8_CH;
        if (!((m->pattern[m->current].hit[D8_CH] >> step) & 1u)) { tap_key(key); run(30); }
        panel.keys |= 1u << key; run(30);
        panel.encoders[2] = 4; run(10);
        panel.encoders[3] = -1; run(10); panel.encoders[3] = 1; run(10); panel.encoders[3] = 1; run(10);
        panel.keys &= ~(1u << key); run(30);
        lk_ok = lk_ok && dr_pitch(m, m->current, D8_CH, step) == 0 && dr_vol(m, m->current, D8_CH, step) == 2;
        /* the hit: pitch and level reach the kit; a step switched off forgets them */
        dr_rewind(m);
        m->pos = (uint8_t)((step + m->pattern[m->current].length - 1u) % m->pattern[m->current].length);
        m->playing = 1;
        dr_step(m, 0, 1000.0f);
        lk_ok = lk_ok && m->kit.shift_semis[D8_BD] == -3 && m->kit.shift[D8_BD] < 0.85f && m->kit.v[D8_BD].volts > 0.0f &&
                m->kit.v[D8_BD].accent == 0.0f;
        float normal_volts = 0;
        {
            static tr808_t kit;
            tr808_init(&kit);
            tr808_trigger_pitch(&kit, D8_BD, 0.66f, 0);
            normal_volts = kit.v[D8_BD].volts;
        }
        lk_ok = lk_ok && m->kit.v[D8_BD].volts < normal_volts * 0.8f;               /* level 3 of 7: quieter than a normal hit */
        /* sounds one octave up: twice the zero crossings of the bass drum's body */
        {
            static tr808_t k0, k1;
            unsigned c[2];
            tr808_t *ks[2] = {&k0, &k1};
            for (unsigned v = 0; v < 2; ++v) {
                tr808_init(ks[v]);
                tr808_trigger_pitch(ks[v], D8_BD, 0.9f, v ? 12 : 0);
                static float kick[EL_BLOCK], l[EL_BLOCK], r[EL_BLOCK], rv[EL_BLOCK], g[EL_BLOCK], d[EL_BLOCK];
                float last = 0;
                c[v] = 0;
                for (unsigned f = 0; f < 44100u / 4u; f += EL_BLOCK) {
                    memset(kick, 0, sizeof kick); memset(l, 0, sizeof l); memset(r, 0, sizeof r);
                    memset(rv, 0, sizeof rv); memset(g, 0, sizeof g); memset(d, 0, sizeof d);
                    tr808_render(ks[v], kick, l, r, rv, g, d, EL_BLOCK);
                    for (unsigned i = 0; i < EL_BLOCK; ++i) {
                        float y = kick[i] + l[i];
                        if (f + i > 2000u && ((y > 0) != (last > 0))) ++c[v];
                        last = y;
                    }
                }
            }
            lk_ok = lk_ok && c[0] > 8 && c[1] * 10 > c[0] * 17 && c[1] * 10 < c[0] * 23;   /* x2 within 15 % */
            if (!lk_ok) printf("debug locks: crossings %u / %u\n", c[0], c[1]);
        }
        dr_post(m, DR_CMD_TOGGLE, D8_BD, (uint8_t)step, 0, 0);
        run(30);
        lk_ok = lk_ok && dr_pitch(m, m->current, D8_BD, step) == 0 && dr_vol(m, m->current, D8_BD, step) == 0;
        /* stored and loaded again, copied with the slot, cleared with the part */
        dr_post(m, DR_CMD_TOGGLE, D8_SD, 3, 0, 0);
        dr_post(m, DR_CMD_PITCH, D8_SD, 3, 0, 7);
        dr_post(m, DR_CMD_VOL, D8_SD, 3, 0, 6);
        run(30);
        static el_engine_t again2;
        el_init(&again2);
        lk_ok = lk_ok && el_store_save(sim) == 0 && el_store_load(&again2) == 0 &&
                !memcmp(again2.drums.pitch, m->pitch, sizeof m->pitch) && !memcmp(again2.drums.vol, m->vol, sizeof m->vol) &&
                dr_pitch(&again2.drums, again2.drums.current, D8_SD, 3) == 7 && dr_vol(&again2.drums, again2.drums.current, D8_SD, 3) == 6;
        unsigned slot = m->current, other = (slot + 1u) % DR_PATTERNS;
        dr_post(m, DR_CMD_COPY, (uint8_t)slot, (uint8_t)other, 0, 0);
        run(30);
        lk_ok = lk_ok && dr_pitch(m, other, D8_SD, 3) == 7 && dr_vol(m, other, D8_SD, 3) == 6;
        dr_post(m, DR_CMD_CLEAR_PART, D8_SD, 0, 0, 0);
        run(30);
        lk_ok = lk_ok && dr_pitch(m, slot, D8_SD, 3) == 0 && dr_vol(m, slot, D8_SD, 3) == 0 && dr_pitch(m, other, D8_SD, 3) == 7;
        /* no toms: no key, no MIDI note, no sound, no hits left in a pattern */
        lk_ok = lk_ok && dr_note_part(41) < 0 && dr_note_part(43) < 0 && dr_note_part(48) < 0 && dr_note_part(36) == D8_BD;
        dr_pattern_t g;
        dr_generate(&g, 6, 40, 12345u, NULL);
        int toms = 0;
        for (unsigned part = 0; part < DR_PARTS; ++part) if (DR_HIDDEN(part) && (g.hit[part] || g.roll[part])) toms = 1;
        static dr_pattern_t dirty;
        dirty = g;
        dirty.hit[D8_LT] = dirty.hit[D8_HT] = 0xffu;
        dr_post_pattern(m, other, &dirty, 0);
        run(30);
        toms = toms || m->pattern[other].hit[D8_LT] || m->pattern[other].hit[D8_HT];
        dr_hit(m, D8_LT, 1.0f, 0, 0);
        toms = toms || m->kit.v[D8_LT].active;
        lk_ok = lk_ok && !toms;
        /* the black keys: the third is the rim shot, then zap and pulse */
        tap_key(5); run(30);
        lk_ok = lk_ok && ui.part == D8_RS;
        tap_key(20); run(30);
        lk_ok = lk_ok && ui.part == D8_ZP;
        tap_key(22); run(30);
        lk_ok = lk_ok && ui.part == D8_PU;
        ui.part = D8_BD;
        printf("locks: step pitch and velocity (KNOB 1 / KNOB 2), stored, copied, cleared; no toms or congas %s\n", lk_ok ? "ok" : "FAIL");
    /* COMP MIX is stored and comes back; a record from before it (version 20) with CLUB
     * keeps its 30 % of the dry signal (docs/adr/0031). */
    {
        el_engine_t *sim = ui.engine;
        static el_engine_t again3, legacy;
        el_post(sim, EL_CMD_OPTION, EL_OPT_COMP_RATIO, EL_COMP_CLUB);
        el_post(sim, EL_CMD_OPTION, EL_OPT_COMP_DRY, 9);
        run(30);
        int cm_ok = el_store_save(sim) == 0;
        el_init(&again3);
        cm_ok = cm_ok && el_store_load(&again3) == 0 && again3.option[EL_OPT_COMP_DRY] == 9 && again3.mix.master_comp.dry > 0.44f &&
                again3.mix.master_comp.dry < 0.46f && again3.mix.master_comp.club;
        el_post(sim, EL_CMD_OPTION, EL_OPT_COMP_DRY, 0);
        run(30);
        cm_ok = cm_ok && el_store_save(sim) == 0 && el_store_test_as_v20() == 0;
        el_init(&legacy);
        cm_ok = cm_ok && el_store_load(&legacy) == 0 && legacy.option[EL_OPT_COMP_DRY] == 6 && legacy.mix.master_comp.club;
        el_post(sim, EL_CMD_OPTION, EL_OPT_COMP_RATIO, 5);
        run(30);
        el_store_save(sim);
        printf("comp mix: stored (version 25), an older CLUB record keeps its 30 %% dry %s\n", cm_ok ? "ok" : "FAIL");
    }
    }
    return same && arp_ok && kept && beat_ok && ed_ok && led_ok && us_ok && ps_ok && sc_ok ? 0 : 1;
}
