/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Four-area panel.
 *
 * Everywhere
 *   FX / SCL / ENV / LFO  areas BEAT / RHYTHM / STRINGS / BASS; press the
 *                         active one again for the next sound page
 *   GLO + area button     mute / unmute that area
 *   HOME                  overview (white keys pick patterns, black keys 1-4 mute areas)
 *   PLAY / REC            transport / record into the current area (EDIT + REC clears it)
 *   ARP held              generator: E3 style, E4 density, white key = new part
 *   SAVE                  store everything in Flash
 *   E1 pattern (all areas)   E2 tempo (GLO: fine)
 *
 * BEAT    black keys pick + play a drum, white keys set steps.
 *         EDIT + white accent, GLO + white roll, EDIT + black clear, GLO + black mute.
 *         E3-E6 drum sound, E7 length (GLO swing), GLO + E1 kit.
 * RHYTHM / STRINGS / BASS
 *   KEYS  (SEQ off) all keys play the sound, OCT -/+ octave. REC while running
 *         records live, REC while stopped enters notes step by step (EDIT + key: rest).
 *         STRINGS: GLO + white key 1-9 picks the chord (1 = single notes).
 *   STEP  (SEQ on) white keys switch steps on/off and select them, EDIT + white
 *         accent, GLO + white tie, OCT -/+ steps 1-16 / 17-32.
 *         E3 note, E4 octave, E5 chord (STRINGS) or accent, E6 tie.
 *   E3-E6 sound page, E7 sound (GLO length), GLO + E1 note length. */
#include "el_ui.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
enum { BACK = 0x0000, PANEL = 0x18e3, GRID = 0x2945, TEXT = 0xef5d, DIM = 0x738e, FAINT = 0x4208,
       ACCENT = 0x07ff, RED = 0xf9a6, AMBER = 0xfd20, GREEN = 0x37e6 };
enum { HOLD_EDIT = 1, HOLD_GLO = 2, HOLD_GEN = 4 };
enum { HEADER_H = 31, GRID_X = 28, GRID_Y = 34, CELL = 13, ACC_Y = GRID_Y + DR_PARTS * CELL + 2, FOOT_Y = 194 };
enum { ROLL_Y = 52, ROLL_H = 118 };

static const uint8_t white_keys[UI_WHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[UI_BLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const uint8_t area_buttons[EL_AREAS] = {BTN_FX, BTN_SCL, BTN_ENV, BTN_LFO};
static const char *const area_names[EL_AREAS] = {"BEAT", "RHYTHM", "STRINGS", "BASS"};
static const char *const area_tabs[EL_AREAS] = {"BEAT", "RHYT", "STRG", "BASS"};
static const uint16_t area_color[EL_AREAS] = {0xfb44, 0xfee0, 0xb4bf, 0x3f3b};
static const uint16_t part_color[DR_PARTS] = {
    0xf8a3, 0xfc40, 0xfee0, 0xfee0, 0xfee0, 0xfd8c, 0xfd8c, 0xfd8c, 0x5e7f, 0x5e7f, 0x5e7f};
/* Sound pages of the synth areas (E3-E6). */
static const char *const synth_page_names[UI_PAGES] = {"FILTER", "ENVELOPE", "OSC", "MOD", "FX"};
static const uint8_t synth_pages[UI_PAGES][4] = {
    {MP_CUTOFF, MP_RESO, MP_VCF_ENV, MP_VCF_LFO}, {MP_T1, MP_T2, MP_L3, MP_T4},
    {MP_SAW, MP_PULSE, MP_SUB_LEVEL, MP_DCO_ENV}, {MP_LFO_RATE, MP_DCO_LFO, MP_PORTA_TIME, MP_PULSE_DETUNE},
    {MP_CHORUS, MP_DELAY_SEND, MP_REVERB_SEND, MP_VOLUME}};
static const char *const note_names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static int key_white(unsigned key)
{
    for (unsigned i = 0; i < UI_WHITE; ++i) if (white_keys[i] == key) return (int)i;
    return -1;
}

static int key_black(unsigned key)
{
    for (unsigned i = 0; i < UI_BLACK; ++i) if (black_keys[i] == key) return (int)i;
    return -1;
}

static void notice(el_ui_t *ui, const char *text)
{
    ui->notice = text;
    ui->notice_until = ui->now + 1500u;
    ++ui->notice_id;
}

static void note_name(char *out, size_t size, int note)
{
    snprintf(out, size, "%s%d", note_names[note % 12], note / 12 - 1);
}

void el_ui_init(el_ui_t *ui, el_engine_t *engine)
{
    memset(ui, 0, sizeof *ui);
    ui->engine = engine;
    ui->follow = 1;
    ui->octave[EL_AREA_STRINGS] = -1;
    ui->octave[EL_AREA_BASS] = -2;
    for (unsigned a = 0; a < EL_AREAS; ++a) ui->gen_density[a] = 64;
    ui->gen_style[EL_AREA_BEAT] = DR_STYLE_ROTHER;
    ui->gen_style[EL_AREA_RHYTHM] = 2;
    ui->gen_style[EL_AREA_STRINGS] = 1;
    ui->gen_style[EL_AREA_BASS] = 1;
    memset(ui->key_note, 0xff, sizeof ui->key_note);
    ui->dirty = (uint16_t)((1u << GFX_BANDS) - 1u);
    for (unsigned i = 0; i < 3; ++i) ui->sig[i] = 0xffffffffu;
}

static st_track_t *track_of(el_ui_t *ui) { return &ui->engine->track[ui->area - 1u]; }
static int synth_area(const el_ui_t *ui) { return !ui->overview && ui->area != EL_AREA_BEAT; }

/* ---- parameters behind E3-E6 ------------------------------------------- */

typedef struct { const char *name; int value, max; const char *text; uint16_t color; } knob_t;

static int raw_value(el_ui_t *ui, unsigned slot, unsigned *param_out)
{
    el_engine_t *e = ui->engine;
    unsigned param;
    int value;
    if (ui->overview) {
        param = (ui->hold & HOLD_GLO) ? 100u + slot : 200u + slot;
        if (param >= 200u) value = slot == 0 ? e->drums.level_value : e->track[slot - 1u].level_value;
        else value = e->fx.v[slot == 0 ? FX_DELAY_DIV : slot == 1 ? FX_DELAY_FB : slot == 2 ? FX_REVERB_SIZE : FX_REVERB_DECAY];
    } else if (ui->area == EL_AREA_BEAT) {
        param = ui->page[0] * 4u + slot;
        value = drum808_get(&e->drums.kit, ui->part, (int)param);
        param |= (unsigned)ui->part << 4;
    } else {
        param = synth_pages[ui->page[ui->area]][slot];
        value = track_of(ui)->part.v[param];
    }
    *param_out = param;
    uint16_t key = (uint16_t)((ui->overview ? 4u : ui->area) << 8 | (param & 0xffu));
    if (ui->shadow_param[slot] == key && ui->now - ui->shadow_time[slot] < 150u) value = ui->shadow_value[slot];
    return value;
}

static knob_t knob(el_ui_t *ui, unsigned slot)
{
    knob_t k = {0, 0, 0, 0, TEXT};
    unsigned param;
    k.value = raw_value(ui, slot, &param);
    if (ui->overview) {
        static const char *const fx_names[4] = {"DLY TIME", "FEEDBACK", "REV SIZE", "DECAY"};
        k.name = (ui->hold & HOLD_GLO) ? fx_names[slot] : area_tabs[slot];
        k.max = (ui->hold & HOLD_GLO) && slot == 0 ? 11 : 127;
        if ((ui->hold & HOLD_GLO) && slot == 0) k.text = fx_delay_division_name(k.value);
        k.color = (ui->hold & HOLD_GLO) ? ACCENT : area_color[slot];
    } else if (ui->area == EL_AREA_BEAT) {
        const fm1_param_t *d = drum808_param(ui->part, (int)(param & 15u));
        if (!d) return k;
        k.name = d->name;
        k.max = d->max;
        if (d->names) k.text = d->names[el_clampi(k.value, 0, d->max)];
        k.color = part_color[ui->part];
    } else {
        k.name = mks_param_name(param);
        k.max = mks_param_max(param);
        k.text = mks_param_value_name(param, k.value);
        k.color = area_color[ui->area];
    }
    return k;
}

static void edit_knob(el_ui_t *ui, unsigned slot, int delta)
{
    el_engine_t *e = ui->engine;
    unsigned param;
    if (!delta) return;
    knob_t k = knob(ui, slot);
    if (!k.name) return;
    int step = k.max > 16 ? delta * (delta > 1 || delta < -1 ? 2 : 1) : delta;
    int value = el_clampi(raw_value(ui, slot, &param) + step, 0, k.max), r;
    if (ui->overview) {
        if (param >= 200u)
            r = slot == 0 ? dr_post(&e->drums, DR_CMD_LEVEL, 0, 0, 0, (int16_t)value)
                          : el_post3(e, EL_CMD_TRACK_LEVEL, (uint8_t)(slot - 1u), 0, 0, (int16_t)value);
        else
            r = el_post(e, EL_CMD_FX, (uint8_t)(slot == 0 ? FX_DELAY_DIV : slot == 1 ? FX_DELAY_FB : slot == 2 ? FX_REVERB_SIZE : FX_REVERB_DECAY),
                        (int16_t)value);
    } else if (ui->area == EL_AREA_BEAT) {
        r = dr_post(&e->drums, DR_CMD_PARAM, ui->part, (uint8_t)(param & 15u), 0, (int16_t)value);
    } else {
        r = el_post3(e, EL_CMD_TRACK_PARAM, (uint8_t)(ui->area - 1u), (uint8_t)param, 0, (int16_t)value);
    }
    if (r == 0) {
        ui->shadow_param[slot] = (uint16_t)((ui->overview ? 4u : ui->area) << 8 | (param & 0xffu));
        ui->shadow_value[slot] = (int16_t)value;
        ui->shadow_time[slot] = ui->now;
    }
}

/* ---- input ------------------------------------------------------------------ */

static unsigned hold_mask(uint32_t buttons)
{
    return ((buttons & BIT(BTN_EDIT)) ? HOLD_EDIT : 0) | ((buttons & BIT(BTN_GLO)) ? HOLD_GLO : 0) |
           ((buttons & BIT(BTN_ARP)) ? HOLD_GEN : 0);
}

static int area_muted(const el_ui_t *ui, unsigned area)
{
    return area == EL_AREA_BEAT ? ui->engine->drums.muted : ui->engine->track[area - 1u].muted;
}

static void set_area_mute(el_ui_t *ui, unsigned area, int on)
{
    if (area == EL_AREA_BEAT) dr_post(&ui->engine->drums, DR_CMD_AREA_MUTE, 0, (uint8_t)on, 0, 0);
    else el_post3(ui->engine, EL_CMD_TRACK_MUTE, (uint8_t)(area - 1u), (uint8_t)on, 0, 0);
}

static void generate(el_ui_t *ui, unsigned key)
{
    el_engine_t *e = ui->engine;
    dr_machine_t *m = &e->drums;
    uint32_t seed = ui->now * 2654435761u + key * 40503u + 1u;
    if (ui->area == EL_AREA_BEAT) {
        dr_pattern_t g;
        const dr_pattern_t *p = &m->pattern[m->current];
        dr_generate(&g, ui->gen_style[0], ui->gen_density[0], seed, ui->gen_keep ? p : NULL);
        g.length = p->length;
        if (dr_post_pattern(m, m->current, &g) == 0) notice(ui, "NEW BEAT");
        return;
    }
    st_track_t *t = track_of(ui);
    st_pattern_t g;
    st_generate(&g, ui->area, ui->gen_style[ui->area], ui->gen_density[ui->area], seed, t->pattern[t->current].length);
    /* Keep the key of the pattern: move the new part to the old root. */
    const st_pattern_t *old = &t->pattern[t->current];
    for (unsigned i = 0; i < old->length; ++i)
        if (old->step[i].flags & ST_ON) {
            int shift = ((int)old->step[i].note - (int)g.step[0].note) % 12;
            if (shift > 6) shift -= 12;
            if (shift < -6) shift += 12;
            for (unsigned s = 0; s < ST_STEPS; ++s) g.step[s].note = (uint8_t)el_clampi(g.step[s].note + shift, 0, 127);
            break;
        }
    if (el_load_pattern(e, ui->area - 1u, t->current, &g) == 0) notice(ui, "NEW PART");
}

static void browse_preset(el_ui_t *ui, int delta)
{
    st_track_t *t = track_of(ui);
    int index = t->preset, dir = delta > 0 ? 1 : -1, moves = delta > 0 ? delta : -delta;
    while (moves--) {
        int i = index;
        do i += dir; while (i >= 0 && i < (int)mks_preset_count && mks_presets[i].area != ui->area);
        if (i < 0 || i >= (int)mks_preset_count) break;
        index = i;
    }
    if (index != t->preset) el_load_preset(ui->engine, ui->area - 1u, (unsigned)index);
}

static void keys_beat(el_ui_t *ui, unsigned key, unsigned hold)
{
    dr_machine_t *m = &ui->engine->drums;
    const dr_pattern_t *p = &m->pattern[m->current];
    int part = key_black(key), step = key_white(key);
    if (part >= 0) {
        if (hold & HOLD_GLO) dr_post(m, DR_CMD_MUTE, (uint8_t)part, !(m->mute & BIT(part)), 0, 0);
        else if (hold & HOLD_EDIT) { dr_post(m, DR_CMD_CLEAR_PART, (uint8_t)part, 0, 0, 0); notice(ui, "PART CLEARED"); }
        else {
            if (ui->part != part) ui->page[0] = 0;
            ui->part = (uint8_t)part;
            dr_post(m, DR_CMD_HIT, (uint8_t)part, 100, 1, 0);
        }
        return;
    }
    unsigned s = (unsigned)step + ui->step_page[0] * 16u;
    if (s >= p->length) return;
    if (hold & HOLD_EDIT) dr_post(m, DR_CMD_ACCENT, 0, (uint8_t)s, 0, 0);
    else if (hold & HOLD_GLO) dr_post(m, DR_CMD_ROLL, ui->part, (uint8_t)s, 0, 0);
    else dr_post(m, DR_CMD_TOGGLE, ui->part, (uint8_t)s, 0, 0);
}

static void keys_synth_down(el_ui_t *ui, unsigned key, unsigned hold)
{
    el_engine_t *e = ui->engine;
    st_track_t *t = track_of(ui);
    uint8_t track = (uint8_t)(ui->area - 1u);
    const st_pattern_t *p = &t->pattern[t->current];
    int step = key_white(key);
    if (ui->step_mode[ui->area]) {
        if (step < 0) return;
        unsigned s = (unsigned)step + ui->step_page[ui->area] * 16u;
        if (s >= p->length) return;
        ui->cursor[ui->area] = (uint8_t)s;
        if (hold & HOLD_EDIT) el_post3(e, EL_CMD_STEP_FLAG, track, (uint8_t)s, ST_ACCENT, 0);
        else if (hold & HOLD_GLO) el_post3(e, EL_CMD_STEP_FLAG, track, (uint8_t)s, ST_TIE, 0);
        else el_post3(e, EL_CMD_STEP_TOGGLE, track, (uint8_t)s, 0, 0);
        return;
    }
    if (ui->area == EL_AREA_STRINGS && (hold & HOLD_GLO) && step >= 0 && step <= MKS_CHORDS) {
        el_post3(e, EL_CMD_TRACK_CHORD, track, (uint8_t)step, 0, 0);
        notice(ui, step ? mks_chord_names[step - 1] : "SINGLE NOTES");
        return;
    }
    int note = el_clampi(53 + (int)key + 12 * ui->octave[ui->area], 0, 127);
    int stopped_entry = t->recording && !e->transport.running;
    if (stopped_entry && (hold & HOLD_EDIT)) {
        el_post3(e, EL_CMD_STEP_SET, track, ui->cursor[ui->area], (uint8_t)note, 0);
        ui->cursor[ui->area] = (uint8_t)((ui->cursor[ui->area] + 1u) % p->length);
        return;
    }
    if (el_post3(e, EL_CMD_NOTE, track, (uint8_t)note, 0, 100) == 0) {
        ui->key_note[key] = (uint8_t)note;
        ui->key_track[key] = track;
    }
    if (stopped_entry) {
        el_post3(e, EL_CMD_STEP_SET, track, ui->cursor[ui->area], (uint8_t)note, (int16_t)(ST_ON | t->chord << 8));
        ui->cursor[ui->area] = (uint8_t)((ui->cursor[ui->area] + 1u) % p->length);
    }
}

static void release_keys(el_ui_t *ui, uint32_t released)
{
    for (unsigned key = 0; key < UI_KEYS; ++key)
        if ((released & BIT(key)) && ui->key_note[key] != 0xff) {
            if (el_post3(ui->engine, EL_CMD_NOTE, ui->key_track[key], ui->key_note[key], 0, 0) == 0) ui->key_note[key] = 0xff;
        }
}

static void handle_input(el_ui_t *ui, const struct fm1_controls *c)
{
    el_engine_t *e = ui->engine;
    dr_machine_t *m = &e->drums;
    uint32_t pressed = c->buttons & ~ui->prev_buttons, keys_down = c->keys & ~ui->prev_keys;
    unsigned hold = hold_mask(c->buttons);
    ui->hold = hold;

    for (unsigned a = 0; a < EL_AREAS; ++a) {
        if (!(pressed & BIT(area_buttons[a]))) continue;
        if (hold & HOLD_GLO) {
            set_area_mute(ui, a, !area_muted(ui, a));
            continue;
        }
        if (!ui->overview && ui->area == a) {
            unsigned pages = a == EL_AREA_BEAT ? ((unsigned)drum808_nparams(ui->part) + 3u) / 4u : UI_PAGES;
            ui->page[a] = (uint8_t)((ui->page[a] + 1u) % pages);
        } else {
            release_keys(ui, ~0u);
            ui->area = (uint8_t)a;
            ui->overview = 0;
        }
    }
    if (pressed & BIT(BTN_HOME)) ui->overview = !ui->overview;
    if (pressed & BIT(BTN_PLAY)) {
        el_post(e, e->transport.running ? EL_CMD_STOP : EL_CMD_START, 0, 0);
        ui->follow = 1;
    }
    if (pressed & BIT(BTN_REC)) {
        if (hold & HOLD_EDIT) {
            if (ui->area == EL_AREA_BEAT) dr_post(m, DR_CMD_CLEAR_PART, DR_PARTS, 0, 0, 0);
            else el_post3(e, EL_CMD_TRACK_CLEAR, (uint8_t)(ui->area - 1u), 0, 0, 0);
            notice(ui, "PATTERN CLEARED");
        } else {
            int armed = ui->area == EL_AREA_BEAT ? m->recording : track_of(ui)->recording;
            el_post3(e, EL_CMD_RECORD_AREA, ui->area, (uint8_t)!armed, 0, 0);
            ui->cursor[ui->area] = 0;
        }
    }
    if ((pressed & BIT(BTN_SAVE)) && ui->save) notice(ui, ui->save(e) == 0 ? "SAVED" : "SAVE FAILED");
    if ((pressed & BIT(BTN_SEQ)) && synth_area(ui)) {
        release_keys(ui, ~0u);
        ui->step_mode[ui->area] = !ui->step_mode[ui->area];
    }
    if (pressed & (BIT(BTN_OCT_DOWN) | BIT(BTN_OCT_UP))) {
        int up = (pressed & BIT(BTN_OCT_UP)) != 0;
        if (synth_area(ui) && !ui->step_mode[ui->area]) {
            release_keys(ui, ~0u);
            ui->octave[ui->area] = (int8_t)el_clampi(ui->octave[ui->area] + (up ? 1 : -1), -3, 2);
        } else if (!ui->overview) {
            unsigned length = ui->area == EL_AREA_BEAT ? m->pattern[m->current].length
                                                       : track_of(ui)->pattern[track_of(ui)->current].length;
            ui->step_page[ui->area] = (uint8_t)(up && length > 16);
            ui->follow = 0;
        }
    }

    release_keys(ui, ui->prev_keys & ~c->keys);
    for (unsigned key = 0; key < UI_KEYS; ++key) {
        if (!(keys_down & BIT(key))) continue;
        if (ui->overview) {
            int w = key_white(key), b = key_black(key);
            if (w >= 0) dr_post(m, DR_CMD_SELECT, (uint8_t)w, 0, 0, 0);
            else if (b >= 0 && b < EL_AREAS) set_area_mute(ui, (unsigned)b, !area_muted(ui, (unsigned)b));
        } else if ((hold & HOLD_GEN) && key_white(key) >= 0) {
            generate(ui, key);
        } else if (ui->area == EL_AREA_BEAT) {
            keys_beat(ui, key, hold);
        } else {
            keys_synth_down(ui, key, hold);
        }
    }

    const int16_t *enc = c->encoders;
    if (enc[0]) {
        if ((hold & HOLD_GLO) && ui->area == EL_AREA_BEAT && !ui->overview)
            dr_post(m, DR_CMD_KIT, (uint8_t)el_clampi((int)m->kit_index + enc[0], 0, DR_KITS - 1), 0, 0, 0);
        else if ((hold & HOLD_GLO) && synth_area(ui)) {
            const st_pattern_t *p = &track_of(ui)->pattern[track_of(ui)->current];
            el_post3(e, EL_CMD_TRACK_GATE, (uint8_t)(ui->area - 1u), (uint8_t)el_clampi(p->gate + enc[0] * 5, 10, 100), 0, 0);
        } else {
            dr_post(m, DR_CMD_SELECT, (uint8_t)el_clampi((int)m->next + enc[0], 0, DR_PATTERNS - 1), 0, 0, 0);
        }
    }
    if (enc[1]) {
        int bpm = (int)(e->transport.bpm * 10.0f + 0.5f);
        el_post(e, EL_CMD_BPM, 0, (int16_t)el_clampi(bpm + enc[1] * ((hold & HOLD_GLO) ? 1 : 10), 300, 3000));
    }
    if (hold & HOLD_GEN) {
        unsigned a = ui->overview ? 0 : ui->area, styles = a == EL_AREA_BEAT ? DR_STYLES : ST_STYLES;
        if (enc[2]) ui->gen_style[a] = (uint8_t)el_clampi(ui->gen_style[a] + enc[2], 0, (int)styles - 1);
        if (enc[3]) ui->gen_density[a] = (uint8_t)el_clampi(ui->gen_density[a] + enc[3] * 4, 0, 127);
        if (enc[4] && a == EL_AREA_BEAT) ui->gen_keep = enc[4] > 0;
    } else if (synth_area(ui) && ui->step_mode[ui->area]) {
        uint8_t track = (uint8_t)(ui->area - 1u), s = ui->cursor[ui->area];
        if (enc[2]) el_post3(e, EL_CMD_STEP_NOTE, track, s, 0, enc[2]);
        if (enc[3]) el_post3(e, EL_CMD_STEP_NOTE, track, s, 0, (int16_t)(enc[3] * 12));
        if (enc[4]) {
            if (ui->area == EL_AREA_STRINGS) {
                const st_step_t *st = &track_of(ui)->pattern[track_of(ui)->current].step[s];
                el_post3(e, EL_CMD_STEP_CHORD, track, s, 0, (int16_t)el_clampi(st->chord + enc[4], 0, MKS_CHORDS));
            } else el_post3(e, EL_CMD_STEP_FLAG, track, s, ST_ACCENT, 0);
        }
        if (enc[5]) el_post3(e, EL_CMD_STEP_FLAG, track, s, ST_TIE, 0);
    } else {
        for (unsigned i = 0; i < 4; ++i) edit_knob(ui, i, enc[2 + i]);
    }
    if (enc[6]) {
        if (ui->overview) {
            el_post(e, EL_CMD_FX, (hold & HOLD_GLO) ? FX_DELAY_RETURN : FX_REVERB_RETURN,
                    (int16_t)el_clampi(e->fx.v[(hold & HOLD_GLO) ? FX_DELAY_RETURN : FX_REVERB_RETURN] + enc[6] * 2, 0, 127));
        } else if (ui->area == EL_AREA_BEAT) {
            const dr_pattern_t *p = &m->pattern[m->current];
            if (hold & HOLD_GLO) dr_post(m, DR_CMD_SWING, 0, (uint8_t)el_clampi(p->swing + enc[6], 50, 75), 0, 0);
            else dr_post(m, DR_CMD_LENGTH, 0, (uint8_t)el_clampi(p->length + enc[6], 1, DR_STEPS), 0, 0);
        } else if (hold & HOLD_GLO) {
            const st_pattern_t *p = &track_of(ui)->pattern[track_of(ui)->current];
            el_post3(e, EL_CMD_TRACK_LENGTH, (uint8_t)(ui->area - 1u), (uint8_t)el_clampi(p->length + enc[6], 1, ST_STEPS), 0, 0);
        } else {
            browse_preset(ui, enc[6]);
        }
    }
    ui->prev_buttons = c->buttons;
    ui->prev_keys = c->keys;
}

/* ---- what is on screen: signatures per region ------------------------------ */

static void mix(uint32_t *h, uint32_t v) { *h = (*h ^ v) * 16777619u; }

static unsigned shown_step_page(el_ui_t *ui, unsigned length, unsigned pos, int playing)
{
    unsigned page = ui->step_page[ui->area];
    if (ui->follow && playing) page = pos / 16u;
    if (length <= 16) page = 0;
    ui->step_page[ui->area] = (uint8_t)page;
    return page;
}

static void signatures(el_ui_t *ui, uint32_t sig[3])
{
    el_engine_t *e = ui->engine;
    const dr_machine_t *m = &e->drums;
    int playing = e->transport.running && m->playing;
    for (unsigned i = 0; i < 3; ++i) sig[i] = 2166136261u;
    mix(&sig[0], ui->area | ui->overview << 3 | (uint32_t)playing << 4);
    mix(&sig[0], m->current | (uint32_t)m->next << 8 | (uint32_t)m->kit_index << 16);
    mix(&sig[0], (uint32_t)(e->transport.bpm * 10.0f + 0.5f));
    for (unsigned a = 0; a < EL_AREAS; ++a)
        mix(&sig[0], (uint32_t)area_muted(ui, a) | (uint32_t)(a ? e->track[a - 1].recording : m->recording) << 1);
    mix(&sig[1], ui->area | ui->overview << 3 | (uint32_t)playing << 4 | (uint32_t)ui->part << 8);
    mix(&sig[1], (uint32_t)(playing ? m->pos : 0xff) | (uint32_t)m->current << 8);
    if (ui->overview) {
        mix(&sig[1], m->revision | m->mute << 8);
        for (unsigned t = 0; t < EL_TRACKS; ++t) {
            mix(&sig[1], e->track[t].revision ^ e->track[t].preset << 20 ^ e->track[t].level_value << 12);
            mix(&sig[1], e->track[t].pos);
        }
        mix(&sig[1], m->level_value);
    } else if (ui->area == EL_AREA_BEAT) {
        const dr_pattern_t *p = &m->pattern[m->current];
        mix(&sig[1], m->revision | (uint32_t)m->mute << 8 | (uint32_t)m->solo << 20);
        mix(&sig[1], shown_step_page(ui, p->length, m->pos, playing) | (uint32_t)p->length << 4 | (uint32_t)p->swing << 12);
    } else {
        st_track_t *t = track_of(ui);
        const st_pattern_t *p = &t->pattern[t->current];
        mix(&sig[1], t->revision ^ (uint32_t)t->preset << 20);
        mix(&sig[1], (uint32_t)(playing ? t->pos : 0xff) | (uint32_t)t->current << 8 | (uint32_t)t->chord << 16);
        mix(&sig[1], shown_step_page(ui, p->length, t->pos, playing) | (uint32_t)p->length << 4 | (uint32_t)p->gate << 12);
        mix(&sig[1], ui->step_mode[ui->area] | (uint32_t)ui->cursor[ui->area] << 4 | (uint32_t)(ui->octave[ui->area] + 8) << 12);
        for (unsigned s = 0; s < p->length; ++s)
            mix(&sig[1], p->step[s].note | (uint32_t)p->step[s].flags << 8 | (uint32_t)p->step[s].chord << 16);
    }
    mix(&sig[2], ui->area | ui->overview << 3 | ui->hold << 4 | (uint32_t)ui->part << 8 | (uint32_t)ui->page[ui->area] << 16);
    mix(&sig[2], ((ui->notice && (int32_t)(ui->now - ui->notice_until) < 0) ? ui->notice_id : 0));
    for (unsigned i = 0; i < 4; ++i) { knob_t k = knob(ui, i); mix(&sig[2], (uint32_t)k.value); }
    unsigned a = ui->overview ? 0 : ui->area;
    mix(&sig[2], ui->gen_style[a] | (uint32_t)ui->gen_density[a] << 8 | (uint32_t)ui->gen_keep << 16);
    if (synth_area(ui)) mix(&sig[2], ui->step_mode[ui->area] | (uint32_t)track_of(ui)->chord << 4 | (uint32_t)ui->cursor[ui->area] << 8);
}

static void mark_dirty(el_ui_t *ui)
{
    uint32_t sig[3];
    static const uint16_t bands[3] = {0x0003, 0x0ffe, 0x7000};
    signatures(ui, sig);
    for (unsigned i = 0; i < 3; ++i)
        if (sig[i] != ui->sig[i]) { ui->dirty |= bands[i]; ui->sig[i] = sig[i]; }
}

/* ---- LEDs ---------------------------------------------------------------------- */

static void update_leds(el_ui_t *ui)
{
    el_engine_t *e = ui->engine;
    const dr_machine_t *m = &e->drums;
    int playing = e->transport.running && m->playing;
    uint64_t leds = 0;
    if (playing) leds |= 1ull << BTN_PLAY;
    if (m->recording || e->track[0].recording || e->track[1].recording || e->track[2].recording) leds |= 1ull << BTN_REC;
    if (ui->overview) leds |= 1ull << BTN_HOME;
    else leds |= 1ull << area_buttons[ui->area];
    if (ui->hold & HOLD_GEN) leds |= 1ull << BTN_ARP;
    if (synth_area(ui) && ui->step_mode[ui->area]) leds |= 1ull << BTN_SEQ;
    if (ui->overview) {
        leds |= 1ull << (14u + white_keys[m->current]);
        if (m->next != m->current && (ui->now / 250u) & 1u) leds |= 1ull << (14u + white_keys[m->next]);
        for (unsigned a = 0; a < EL_AREAS; ++a)
            if (!area_muted(ui, a)) leds |= 1ull << (14u + black_keys[a]);
    } else if (ui->area == EL_AREA_BEAT) {
        const dr_pattern_t *p = &m->pattern[m->current];
        unsigned page = ui->step_page[0];
        if (p->length > 16) leds |= 1ull << (page ? BTN_OCT_UP : BTN_OCT_DOWN);
        for (unsigned i = 0; i < UI_WHITE; ++i) {
            unsigned s = i + page * 16u;
            if (s >= p->length) continue;
            int on = (ui->hold & HOLD_EDIT) ? (p->accent >> s) & 1u :
                     (ui->hold & HOLD_GLO) ? (p->roll[ui->part] >> s) & 1u : (p->hit[ui->part] >> s) & 1u;
            if (playing && m->pos == s) on = !on;
            if (on) leds |= 1ull << (14u + white_keys[i]);
        }
        for (unsigned part = 0; part < UI_BLACK; ++part) {
            int on = (ui->hold & HOLD_GLO) ? !(m->mute & BIT(part)) : part == ui->part;
            if (on) leds |= 1ull << (14u + black_keys[part]);
        }
    } else {
        st_track_t *t = track_of(ui);
        const st_pattern_t *p = &t->pattern[t->current];
        int tplay = playing && t->playing;
        if (ui->step_mode[ui->area]) {
            unsigned page = ui->step_page[ui->area];
            if (p->length > 16) leds |= 1ull << (page ? BTN_OCT_UP : BTN_OCT_DOWN);
            for (unsigned i = 0; i < UI_WHITE; ++i) {
                unsigned s = i + page * 16u;
                if (s >= p->length) continue;
                int on = (ui->hold & HOLD_EDIT) ? (p->step[s].flags & ST_ACCENT) != 0 :
                         (ui->hold & HOLD_GLO) ? (p->step[s].flags & ST_TIE) != 0 : (p->step[s].flags & ST_ON) != 0;
                if (tplay && t->pos == s) on = !on;
                if (on) leds |= 1ull << (14u + white_keys[i]);
            }
        } else {
            /* KEYS: light the keys of the notes the sequence is playing. */
            int base = 53 + 12 * ui->octave[ui->area];
            for (unsigned i = 0; i < t->sounding_count; ++i) {
                int k = t->sounding[i] - base;
                if (k >= 0 && k < UI_KEYS) leds |= 1ull << (14u + (unsigned)k);
            }
            for (unsigned k = 0; k < UI_KEYS; ++k) if (ui->key_note[k] != 0xff) leds |= 1ull << (14u + k);
        }
    }
    uint64_t changed = ui->leds_valid ? leds ^ ui->leds : ~0ull;
    for (unsigned i = 0; i < 14u + UI_KEYS; ++i)
        if (changed & (1ull << i)) fm1_led_set(i, (int)((leds >> i) & 1u));
    ui->leds = leds;
    ui->leds_valid = 1;
}

/* ---- drawing ------------------------------------------------------------------ */

static void draw_header(el_ui_t *ui, gfx_band_t *b)
{
    el_engine_t *e = ui->engine;
    const dr_machine_t *m = &e->drums;
    char text[40], bpm[12];
    gfx_fill(b, 0, 0, GFX_W, HEADER_H, PANEL);
    for (unsigned a = 0; a < EL_AREAS; ++a) {
        int x = (int)a * 60, active = !ui->overview && ui->area == a, muted = area_muted(ui, a);
        uint16_t color = muted ? DIM : area_color[a];
        if (active) gfx_fill(b, x + 1, 0, 58, 15, color);
        else gfx_fill(b, x + 1, 13, 58, 2, color);
        gfx_text(b, x + 6, 3, area_tabs[a], active ? BACK : color, 1);
        if (muted) gfx_text(b, x + 46, 3, "M", active ? BACK : RED, 1);
        else if (a ? e->track[a - 1].recording : m->recording) gfx_fill(b, x + 48, 4, 6, 6, RED);
    }
    snprintf(text, sizeof text, "%02u", m->current + 1u);
    int x = gfx_text(b, 4, 19, text, ACCENT, 1);
    if (m->next != m->current) {
        snprintf(text, sizeof text, ">%02u", m->next + 1u);
        x = gfx_text(b, x, 19, text, AMBER, 1);
    }
    gfx_text(b, x + 6, 19, m->pattern[m->next].name, TEXT, 1);
    unsigned x10 = (unsigned)(e->transport.bpm * 10.0f + 0.5f);
    if (x10 % 10) snprintf(bpm, sizeof bpm, "%u.%u", x10 / 10, x10 % 10);
    else snprintf(bpm, sizeof bpm, "%u", x10 / 10);
    gfx_text(b, 150, 19, bpm, TEXT, 1);
    gfx_text(b, 186, 19, ui->overview ? "HOME" : "", AMBER, 1);
    int playing = e->transport.running && m->playing;
    gfx_text(b, 212, 19, playing ? "PLAY" : "STOP", playing ? GREEN : DIM, 1);
}

static void draw_beat(el_ui_t *ui, gfx_band_t *b)
{
    const dr_machine_t *m = &ui->engine->drums;
    const dr_pattern_t *p = &m->pattern[m->current];
    unsigned base = ui->step_page[0] * 16u, shown = p->length > base ? p->length - base : 0;
    int playing = ui->engine->transport.running && m->playing;
    unsigned pos = playing ? m->pos : 0xffu;
    if (shown > 16) shown = 16;
    for (unsigned part = 0; part < DR_PARTS; ++part) {
        int y = GRID_Y + (int)part * CELL;
        if (!gfx_touches(b, y, CELL)) continue;
        int muted = (m->mute >> part) & 1u || m->muted;
        if (part == ui->part) gfx_fill(b, 0, y - 1, GRID_X - 2, CELL, 0x3186);
        gfx_text(b, 3, y + 2, dr_part_names[part], muted ? FAINT : part == ui->part ? TEXT : DIM, 1);
        if ((m->mute >> part) & 1u) gfx_text(b, 17, y + 2, "M", RED, 1);
        for (unsigned i = 0; i < 16; ++i) {
            int x = GRID_X + (int)i * CELL;
            unsigned s = base + i;
            if (i >= shown) { gfx_fill(b, x + 4, y + 4, 3, 3, PANEL); continue; }
            gfx_fill(b, x, y, CELL - 2, CELL - 2, s == pos ? 0x52aa : (i & 3u) == 0 ? 0x31a6 : GRID);
            if ((p->hit[part] >> s) & 1u) {
                uint16_t c = muted ? DIM : part_color[part];
                if ((p->accent >> s) & 1u) gfx_fill(b, x, y, CELL - 2, CELL - 2, c);
                else gfx_fill(b, x + 2, y + 2, CELL - 6, CELL - 6, c);
                if ((p->roll[part] >> s) & 1u) gfx_fill(b, x + 4, y + 4, 3, 3, BACK);
            }
        }
    }
    if (gfx_touches(b, ACC_Y, 8)) {
        gfx_text(b, 3, ACC_Y, "AC", DIM, 1);
        for (unsigned i = 0; i < shown; ++i)
            if ((p->accent >> (base + i)) & 1u) gfx_fill(b, GRID_X + (int)i * CELL, ACC_Y + 1, CELL - 2, 5, AMBER);
    }
}

static void draw_synth(el_ui_t *ui, gfx_band_t *b)
{
    el_engine_t *e = ui->engine;
    st_track_t *t = track_of(ui);
    const st_pattern_t *p = &t->pattern[t->current];
    uint16_t color = t->muted ? DIM : area_color[ui->area];
    char text[48];
    int playing = e->transport.running && t->playing;
    unsigned base = ui->step_page[ui->area] * 16u, pos = playing ? t->pos : 0xffu;
    unsigned shown = p->length > base ? p->length - base : 0;
    if (shown > 16) shown = 16;
    gfx_text(b, 4, 35, mks_presets[t->preset].name, color, 2);
    if (ui->step_mode[ui->area]) snprintf(text, sizeof text, "STEP%s", p->length > 16 ? (base ? " 2/2" : " 1/2") : "");
    else snprintf(text, sizeof text, "KEYS O%+d", ui->octave[ui->area]);
    gfx_text(b, GFX_W - 4 - gfx_text_width(text, 1), 39, text, ui->step_mode[ui->area] ? AMBER : DIM, 1);

    /* Piano roll: the pattern's note range, at least one octave. */
    int lo = 127, hi = 0;
    for (unsigned s = 0; s < p->length; ++s)
        if (p->step[s].flags & ST_ON) {
            int top = p->step[s].note;
            if (p->step[s].chord) top += 14;
            if (p->step[s].note < lo) lo = p->step[s].note;
            if (top > hi) hi = top;
        }
    if (lo > hi) { lo = t->last_note - 6; hi = t->last_note + 6; }
    if (hi - lo < 12) { int mid = (lo + hi) / 2; lo = mid - 6; hi = mid + 6; }
    int range = hi - lo + 1;
    if (gfx_touches(b, ROLL_Y, ROLL_H)) {
        gfx_fill(b, GRID_X, ROLL_Y, 16 * CELL - 2, ROLL_H, 0x0861);
        for (int n = lo; n <= hi; ++n)
            if (n % 12 == 0) {
                int y = ROLL_Y + ROLL_H - 1 - (n - lo) * ROLL_H / range;
                gfx_fill(b, GRID_X, y, 16 * CELL - 2, 1, FAINT);
                note_name(text, sizeof text, n);
                gfx_text(b, 2, y - 3, text, DIM, 1);
            }
        for (unsigned i = 0; i < shown; ++i) {
            unsigned s = base + i;
            int x = GRID_X + (int)i * CELL;
            if (s == pos) gfx_fill(b, x, ROLL_Y, CELL - 2, ROLL_H, 0x2945);
            if (ui->step_mode[ui->area] && s == ui->cursor[ui->area]) gfx_frame(b, x - 1, ROLL_Y, CELL, ROLL_H, AMBER);
            const st_step_t *st = &p->step[s];
            if (!(st->flags & ST_ON)) continue;
            int16_t notes[MKS_CHORD_NOTES];
            unsigned count = 1;
            notes[0] = st->note;
            if (st->chord) {
                count = 0;
                for (unsigned k = 0; k < MKS_CHORD_NOTES && mks_chords[st->chord - 1][k] != -128; ++k)
                    notes[count++] = (int16_t)(st->note + mks_chords[st->chord - 1][k]);
            }
            for (unsigned k = 0; k < count; ++k) {
                int y = ROLL_Y + ROLL_H - 1 - (notes[k] - lo) * ROLL_H / range - 3;
                int w = (st->flags & ST_TIE) ? CELL : CELL - 2;
                /* Root bright when accented, chord tones and plain notes dimmer. */
                uint16_t c = k == 0 && (st->flags & ST_ACCENT) ? color : (uint16_t)((color >> 1) & 0x7bef);
                gfx_fill(b, x, y, w, 5, c);
            }
        }
    }
    /* Step / chord info line. */
    if (ui->step_mode[ui->area]) {
        const st_step_t *st = &p->step[ui->cursor[ui->area]];
        char n[8];
        note_name(n, sizeof n, st->note);
        snprintf(text, sizeof text, "STEP %02u  %s %s%s%s", ui->cursor[ui->area] + 1u, (st->flags & ST_ON) ? n : "---",
                 st->chord ? mks_chord_names[st->chord - 1] : "", (st->flags & ST_ACCENT) ? " ACC" : "",
                 (st->flags & ST_TIE) ? " TIE" : "");
    } else {
        snprintf(text, sizeof text, "LEN %u  GATE %u%%%s%s", p->length, p->gate,
                 ui->area == EL_AREA_STRINGS ? "  CHORD " : "",
                 ui->area == EL_AREA_STRINGS ? (t->chord ? mks_chord_names[t->chord - 1] : "OFF") : "");
    }
    gfx_text(b, 4, 176, text, TEXT, 1);
    if (t->recording) gfx_text(b, 200, 176, e->transport.running ? "LIVE" : "STEP", RED, 1);
}

static void draw_overview(el_ui_t *ui, gfx_band_t *b)
{
    el_engine_t *e = ui->engine;
    const dr_machine_t *m = &e->drums;
    int playing = e->transport.running && m->playing;
    for (unsigned a = 0; a < EL_AREAS; ++a) {
        int y = GRID_Y + (int)a * 36;
        if (!gfx_touches(b, y, 36)) continue;
        int muted = area_muted(ui, a);
        uint16_t color = muted ? DIM : area_color[a];
        const char *sound = a ? mks_presets[e->track[a - 1].preset].name : dr_kit_names[m->kit_index];
        unsigned level = a ? e->track[a - 1].level_value : m->level_value;
        gfx_text(b, 4, y + 2, area_names[a], color, 1);
        gfx_text(b, 60, y + 2, sound, muted ? DIM : TEXT, 1);
        if (muted) gfx_text(b, 140, y + 2, "MUTE", RED, 1);
        gfx_fill(b, 170, y + 3, 64, 5, GRID);
        gfx_fill(b, 170, y + 3, (int)(level * 64u / 127u), 5, color);
        unsigned length, pos;
        if (a == 0) { length = m->pattern[m->current].length; pos = m->pos; }
        else { length = e->track[a - 1].pattern[e->track[a - 1].current].length; pos = e->track[a - 1].pos; }
        unsigned base = playing && length > 16 ? (pos / 16u) * 16u : 0;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned s = base + i;
            int x = 4 + (int)i * 14, cy = y + 14;
            if (s >= length) continue;
            int on = 0, accent = 0;
            if (a == 0) {
                const dr_pattern_t *p = &m->pattern[m->current];
                for (unsigned part = 0; part < DR_PARTS; ++part) on |= (p->hit[part] >> s) & 1u;
                accent = (p->accent >> s) & 1u;
            } else {
                const st_step_t *st = &e->track[a - 1].pattern[e->track[a - 1].current].step[s];
                on = (st->flags & ST_ON) != 0;
                accent = (st->flags & ST_ACCENT) != 0;
            }
            gfx_fill(b, x, cy, 12, 17, playing && s == pos ? 0x52aa : GRID);
            if (on) gfx_fill(b, x + 2, accent ? cy + 2 : cy + 6, 8, accent ? 13 : 9, color);
        }
    }
    gfx_text(b, 4, 180, "WHITE KEYS: PATTERN  BLACK 1-4: MUTE", DIM, 1);
}

static void draw_footer(el_ui_t *ui, gfx_band_t *b)
{
    char value[24];
    gfx_fill(b, 0, FOOT_Y - 4, GFX_W, GFX_H - FOOT_Y + 4, PANEL);
    unsigned a = ui->overview ? 0 : ui->area;
    if (ui->hold & HOLD_GEN) {
        int beat = a == EL_AREA_BEAT;
        gfx_text(b, 4, FOOT_Y, "STYLE", DIM, 1);
        gfx_text(b, 4, FOOT_Y + 12, beat ? dr_style_names[ui->gen_style[a]] : st_style_name(a, ui->gen_style[a]), ACCENT, 1);
        gfx_text(b, 64, FOOT_Y, "DENS", DIM, 1);
        snprintf(value, sizeof value, "%u", ui->gen_density[a]);
        gfx_text(b, 64, FOOT_Y + 12, value, ACCENT, 2);
        if (beat) {
            gfx_text(b, 124, FOOT_Y, "KICK", DIM, 1);
            gfx_text(b, 124, FOOT_Y + 12, ui->gen_keep ? "KEEP" : "NEW", ACCENT, 2);
        }
        gfx_text(b, 4, GFX_H - 12, "GENERATOR: PRESS A WHITE KEY", AMBER, 1);
        return;
    }
    if (synth_area(ui) && ui->step_mode[ui->area]) {
        static const char *const labels[4] = {"NOTE", "OCTAVE", "CHORD", "TIE"};
        for (unsigned i = 0; i < 4; ++i)
            gfx_text(b, 4 + (int)i * 59, FOOT_Y, i == 2 && ui->area != EL_AREA_STRINGS ? "ACCENT" : labels[i], DIM, 1);
        gfx_text(b, 4, FOOT_Y + 12, "E3-E6 EDIT THE STEP", area_color[ui->area], 1);
    } else {
        for (unsigned i = 0; i < 4; ++i) {
            knob_t k = knob(ui, i);
            if (!k.name) continue;
            int x = 4 + (int)i * 59;
            gfx_text(b, x, FOOT_Y, k.name, DIM, 1);
            if (k.text) gfx_text(b, x, FOOT_Y + 14, k.text, k.color, 1);
            else { snprintf(value, sizeof value, "%d", k.value); gfx_text(b, x, FOOT_Y + 12, value, k.color, 2); }
        }
    }
    const char *hint = (ui->notice && (int32_t)(ui->now - ui->notice_until) < 0) ? ui->notice : 0;
    char line[48];
    if (!hint) {
        if (ui->overview) hint = (ui->hold & HOLD_GLO) ? "GLO: FX   E7 DELAY RETURN" : "LEVELS   E7 REVERB  GLO: FX";
        else if (ui->area == EL_AREA_BEAT) {
            const dr_pattern_t *p = &ui->engine->drums.pattern[ui->engine->drums.current];
            unsigned pages = ((unsigned)drum808_nparams(ui->part) + 3u) / 4u;
            snprintf(line, sizeof line, "%s %u/%u KIT %s SW%u L%u%s", dr_part_names[ui->part], ui->page[0] + 1u, pages,
                     dr_kit_names[ui->engine->drums.kit_index], p->swing, p->length,
                     p->length > 16 ? (ui->step_page[0] ? " 2/2" : " 1/2") : "");
            hint = (ui->hold & HOLD_EDIT) ? "EDIT: KEY=ACCENT  BLACK=CLEAR" :
                   (ui->hold & HOLD_GLO) ? "GLO: KEY=ROLL BLACK=MUTE E1=KIT" : line;
        } else {
            snprintf(line, sizeof line, "%s %u/%u  E7 SOUND  SEQ=STEP", synth_page_names[ui->page[ui->area]],
                     ui->page[ui->area] + 1u, UI_PAGES);
            hint = (ui->hold & HOLD_GLO) ? (ui->step_mode[ui->area] ? "GLO: KEY=TIE  E7=LENGTH" :
                                            ui->area == EL_AREA_STRINGS ? "GLO: KEY 1-9 CHORD  E1=GATE" : "GLO: E1=GATE  E7=LENGTH") :
                   (ui->hold & HOLD_EDIT) ? "EDIT: KEY=ACCENT/REST" : line;
        }
    }
    gfx_text(b, 4, GFX_H - 12, hint, hint == ui->notice ? AMBER : DIM, 1);
}

void el_ui_draw_band(el_ui_t *ui, unsigned band)
{
    gfx_band_t *b = &ui->band;
    gfx_begin(b, band, BACK);
    if (gfx_touches(b, 0, HEADER_H)) draw_header(ui, b);
    if (gfx_touches(b, HEADER_H, FOOT_Y - 4 - HEADER_H)) {
        if (ui->overview) draw_overview(ui, b);
        else if (ui->area == EL_AREA_BEAT) draw_beat(ui, b);
        else draw_synth(ui, b);
    }
    if (gfx_touches(b, FOOT_Y - 4, GFX_H)) draw_footer(ui, b);
}

static void paint(el_ui_t *ui)
{
    uint32_t started = fm1_millis();
    for (unsigned visited = 0; visited < GFX_BANDS && ui->dirty; ++visited) {
        int pending = fm1_screen_pending();
        if (pending > 0) break;
        if (pending < 0) ui->dirty = (uint16_t)((1u << GFX_BANDS) - 1u);
        unsigned band = ui->paint_next;
        ui->paint_next = (uint8_t)((band + 1u) % GFX_BANDS);
        if (!(ui->dirty & BIT(band))) continue;
        el_ui_draw_band(ui, band);
        if (fm1_screen_submit(0, band * GFX_BAND, GFX_W, GFX_BAND, ui->band.px, GFX_W * GFX_BAND) != 0) break;
        ui->dirty &= (uint16_t)~BIT(band);
        if (fm1_millis() - started >= 4u) break;
    }
}

void el_ui_control(el_ui_t *ui, const struct fm1_controls *controls, uint32_t now)
{
    ui->now = now;
    handle_input(ui, controls);
    mark_dirty(ui);
    update_leds(ui);
    paint(ui);
}
