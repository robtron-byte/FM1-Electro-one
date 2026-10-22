/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Drum page (more pages follow with the synth engines).
 *
 *   black keys        select + audition a drum part (REC: play it in)
 *   white keys        set/clear the selected part's steps (16 per page)
 *   EDIT + white      accent on that step       SCL + white   32nd roll
 *   EDIT + black      clear that part           GLO + black   mute
 *   HOME + black      solo                      ENV           next sound page
 *   OCT -/+           steps 1-16 / 17-32        PLAY / REC    transport, live record
 *   SAVE              store patterns, kit sound and tempo in Flash
 *   LFO held          groove generator: E3 style, E4 density, E5 kick, white key = new
 *   E1 pattern (switches at loop end; GLO: kit)  E2 tempo (GLO: fine)
 *   E3-E6 sound of the selected part  E7 length (GLO: swing) */
#include "el_ui.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
enum { BACK = 0x0000, PANEL = 0x18e3, GRID = 0x2945, TEXT = 0xef5d, DIM = 0x738e, FAINT = 0x4208,
       ACCENT = 0x07ff, RED = 0xf9a6, AMBER = 0xfd20, GREEN = 0x37e6 };
enum { HOLD_EDIT = 1, HOLD_SCL = 2, HOLD_GLO = 4, HOLD_HOME = 8, HOLD_GEN = 16 };

/* Key bit to white-key step index / black-key part, or 0xff. */
static const uint8_t white_keys[UI_WHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[UI_BLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const uint16_t part_color[DR_PARTS] = {
    0xf8a3, 0xfc40, 0xfee0, 0xfee0, 0xfee0, 0xfd8c, 0xfd8c, 0xfd8c, 0x5e7f, 0x5e7f, 0x5e7f};

/* Layout */
enum { GRID_X = 28, GRID_Y = 34, CELL = 13, ACC_Y = GRID_Y + DR_PARTS * CELL + 2, FOOT_Y = 194 };

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

static void notice(el_ui_t *ui, const char *text, uint32_t now)
{
    ui->notice = text;
    ui->notice_until = now + 1500u;
    ++ui->notice_id;
}

void el_ui_init(el_ui_t *ui, el_engine_t *engine)
{
    memset(ui, 0, sizeof *ui);
    ui->engine = engine;
    ui->follow = 1;
    ui->gen_style = DR_STYLE_ROTHER;
    ui->gen_density = 64;
    ui->gen_keep = 0;
    ui->dirty = (uint16_t)((1u << GFX_BANDS) - 1u);
    memset(&ui->shown, 0xff, sizeof ui->shown);
}

static int param_value(el_ui_t *ui, unsigned part, unsigned p, uint32_t now)
{
    if (now - ui->shadow_time[part][p] < 120u) return ui->shadow_value[part][p];
    return drum808_get(&ui->engine->drums.kit, (int)part, (int)p);
}

static void edit_param(el_ui_t *ui, unsigned part, unsigned p, int delta, uint32_t now)
{
    const fm1_param_t *d = drum808_param((int)part, (int)p);
    if (!d || !delta) return;
    /* Coarse steps for 0..127 controls, single steps for switches. */
    int step = d->max > 16 ? delta * (delta > 1 || delta < -1 ? 2 : 1) : delta;
    int value = el_clampi(param_value(ui, part, p, now) + step, 0, d->max);
    if (dr_post(&ui->engine->drums, DR_CMD_PARAM, (uint8_t)part, (uint8_t)p, 0, (int16_t)value) == 0) {
        ui->shadow_value[part][p] = (int16_t)value;
        ui->shadow_time[part][p] = now;
    }
}

static unsigned hold_mask(uint32_t buttons)
{
    return ((buttons & BIT(BTN_EDIT)) ? HOLD_EDIT : 0) | ((buttons & BIT(BTN_SCL)) ? HOLD_SCL : 0) |
           ((buttons & BIT(BTN_GLO)) ? HOLD_GLO : 0) | ((buttons & BIT(BTN_HOME)) ? HOLD_HOME : 0) |
           ((buttons & BIT(BTN_LFO)) ? HOLD_GEN : 0);
}

static void handle_input(el_ui_t *ui, const struct fm1_controls *c, uint32_t now)
{
    el_engine_t *e = ui->engine;
    dr_machine_t *m = &e->drums;
    const dr_pattern_t *p = &m->pattern[m->current];
    uint32_t pressed = c->buttons & ~ui->prev_buttons, keys_down = c->keys & ~ui->prev_keys;
    unsigned hold = hold_mask(c->buttons);

    if (pressed & BIT(BTN_PLAY)) {
        el_post(e, e->transport.running ? EL_CMD_STOP : EL_CMD_START, 0, 0);
        ui->follow = 1;
    }
    if ((pressed & BIT(BTN_REC)) && (hold & HOLD_EDIT)) {
        dr_post(m, DR_CMD_CLEAR_PART, DR_PARTS, 0, 0, 0);
        notice(ui, "PATTERN CLEARED", now);
    } else if (pressed & BIT(BTN_REC)) dr_post(m, DR_CMD_RECORD, 0, (uint8_t)!m->recording, 0, 0);
    if (pressed & BIT(BTN_OCT_DOWN)) { ui->step_page = 0; ui->follow = 0; }
    if ((pressed & BIT(BTN_OCT_UP)) && p->length > 16) { ui->step_page = 1; ui->follow = 0; }
    if (pressed & BIT(BTN_ENV)) {
        unsigned pages = ((unsigned)drum808_nparams(ui->part) + 3u) / 4u;
        ui->param_page = (uint8_t)((ui->param_page + 1u) % (pages ? pages : 1u));
    }
    if (pressed & BIT(BTN_SEQ)) ui->follow = 1;
    if ((pressed & BIT(BTN_SAVE)) && ui->save)
        notice(ui, ui->save(e) == 0 ? "SAVED" : "SAVE FAILED", now);

    for (unsigned key = 0; key < UI_KEYS; ++key) {
        if (!(keys_down & BIT(key))) continue;
        int part = key_black(key), step = key_white(key);
        if (part >= 0) {
            if (hold & HOLD_GLO) dr_post(m, DR_CMD_MUTE, (uint8_t)part, !(m->mute & BIT(part)), 0, 0);
            else if (hold & HOLD_HOME) dr_post(m, DR_CMD_SOLO, (uint8_t)part, !(m->solo & BIT(part)), 0, 0);
            else if (hold & HOLD_EDIT) { dr_post(m, DR_CMD_CLEAR_PART, (uint8_t)part, 0, 0, 0); notice(ui, "PART CLEARED", now); }
            else {
                if (ui->part != part) ui->param_page = 0;
                ui->part = (uint8_t)part;
                dr_post(m, DR_CMD_HIT, (uint8_t)part, 100, 1, 0);
            }
        } else if (step >= 0) {
            if (hold & HOLD_GEN) {
                dr_pattern_t g;
                dr_generate(&g, ui->gen_style, ui->gen_density, now * 2654435761u + key,
                            ui->gen_keep ? p : NULL);
                g.length = p->length;
                if (dr_post_pattern(m, m->current, &g) == 0) {
                    dr_post(m, DR_CMD_KIT, g.kit, 0, 0, 0);
                    notice(ui, "NEW GROOVE", now);
                }
                continue;
            }
            unsigned s = (unsigned)step + ui->step_page * 16u;
            if (s >= p->length) continue;
            if (hold & HOLD_EDIT) dr_post(m, DR_CMD_ACCENT, 0, (uint8_t)s, 0, 0);
            else if (hold & HOLD_SCL) dr_post(m, DR_CMD_ROLL, ui->part, (uint8_t)s, 0, 0);
            else dr_post(m, DR_CMD_TOGGLE, ui->part, (uint8_t)s, 0, 0);
        }
    }

    const int16_t *enc = c->encoders;
    if (enc[0] && (hold & HOLD_GLO)) {
        dr_post(m, DR_CMD_KIT, (uint8_t)el_clampi((int)m->kit_index + enc[0], 0, DR_KITS - 1), 0, 0, 0);
    } else if (enc[0]) {
        int target = el_clampi((int)m->next + enc[0], 0, DR_PATTERNS - 1);
        dr_post(m, DR_CMD_SELECT, (uint8_t)target, 0, 0, 0);
        if (!e->transport.running) ui->step_page = 0;
    }
    if (enc[1]) {
        int bpm = (int)(e->transport.bpm * 10.0f + 0.5f);
        bpm = el_clampi(bpm + enc[1] * ((hold & HOLD_GLO) ? 1 : 10), 300, 3000);
        el_post(e, EL_CMD_BPM, 0, (int16_t)bpm);
    }
    if (hold & HOLD_GEN) {
        if (enc[2]) ui->gen_style = (uint8_t)el_clampi(ui->gen_style + enc[2], 0, DR_STYLES - 1);
        if (enc[3]) ui->gen_density = (uint8_t)el_clampi(ui->gen_density + enc[3] * 4, 0, 127);
        if (enc[4]) ui->gen_keep = enc[4] > 0;
    } else {
        for (unsigned i = 0; i < 4; ++i)
            edit_param(ui, ui->part, ui->param_page * 4u + i, enc[2 + i], now);
    }
    if (enc[6]) {
        if (hold & HOLD_GLO) dr_post(m, DR_CMD_SWING, 0, (uint8_t)el_clampi(p->swing + enc[6], 50, 75), 0, 0);
        else dr_post(m, DR_CMD_LENGTH, 0, (uint8_t)el_clampi(p->length + enc[6], 1, DR_STEPS), 0, 0);
    }
    ui->prev_buttons = c->buttons;
    ui->prev_keys = c->keys;
}

static void build_model(el_ui_t *ui, uint32_t hold, uint32_t now)
{
    el_engine_t *e = ui->engine;
    dr_machine_t *m = &e->drums;
    const dr_pattern_t *p = &m->pattern[m->current];
    ui_model_t *v = &ui->model;
    if (ui->follow && e->transport.running && m->playing) ui->step_page = (uint8_t)(m->pos / 16u);
    if (p->length <= 16) ui->step_page = 0;
    memset(v, 0, sizeof *v);
    v->pattern = m->current;
    v->next = m->next;
    v->part = ui->part;
    v->step_page = ui->step_page;
    v->steps_shown = (uint8_t)(p->length > ui->step_page * 16u ? p->length - ui->step_page * 16u : 0);
    if (v->steps_shown > 16) v->steps_shown = 16;
    v->playing = e->transport.running && m->playing;
    v->recording = m->recording;
    v->pos = v->playing ? m->pos : 0xff;
    v->param_page = ui->param_page;
    v->length = p->length;
    v->swing = p->swing;
    v->kit = m->kit_index;
    v->bpm_x10 = (uint16_t)(e->transport.bpm * 10.0f + 0.5f);
    v->mute = m->mute;
    v->solo = m->solo;
    v->revision = m->revision;
    v->gen_style = ui->gen_style;
    v->gen_density = ui->gen_density;
    v->gen_keep = ui->gen_keep;
    v->hold = hold;
    v->notice_id = (ui->notice && (int32_t)(now - ui->notice_until) < 0) ? ui->notice_id : 0;
    /* Sound values can change from encoders; fold them into the revision. */
    for (unsigned i = 0; i < 4; ++i) v->revision += (uint32_t)param_value(ui, ui->part, ui->param_page * 4u + i, now) << (i * 7);
}

/* Which bands does a change touch? Header 0-1, grid 2-11, footer 12-14. */
static void mark_dirty(el_ui_t *ui)
{
    const ui_model_t *a = &ui->model, *b = &ui->shown;
    uint16_t header = 0x0003, grid = 0x0ffe, footer = 0x7000;
    if (a->pattern != b->pattern || a->next != b->next || a->playing != b->playing || a->recording != b->recording ||
        a->length != b->length || a->swing != b->swing || a->kit != b->kit || a->bpm_x10 != b->bpm_x10 ||
        a->step_page != b->step_page)
        ui->dirty |= header;
    if (a->pattern != b->pattern || a->part != b->part || a->step_page != b->step_page || a->pos != b->pos ||
        a->mute != b->mute || a->solo != b->solo || a->revision != b->revision || a->steps_shown != b->steps_shown)
        ui->dirty |= grid;
    if (a->part != b->part || a->param_page != b->param_page || a->revision != b->revision || a->hold != b->hold ||
        a->notice_id != b->notice_id || a->gen_style != b->gen_style || a->gen_density != b->gen_density ||
        a->gen_keep != b->gen_keep || a->kit != b->kit)
        ui->dirty |= footer;
    ui->shown = *a;
}

static void update_leds(el_ui_t *ui)
{
    const ui_model_t *v = &ui->model;
    const dr_machine_t *m = &ui->engine->drums;
    const dr_pattern_t *p = &m->pattern[m->current];
    uint64_t leds = 0;
    if (v->playing) leds |= 1ull << BTN_PLAY;
    if (v->recording) leds |= 1ull << BTN_REC;
    leds |= 1ull << BTN_SEQ;
    if (v->hold & HOLD_GEN) leds |= 1ull << BTN_LFO;
    if (p->length > 16) leds |= 1ull << (v->step_page ? BTN_OCT_UP : BTN_OCT_DOWN);
    for (unsigned i = 0; i < UI_WHITE; ++i) {
        unsigned s = i + v->step_page * 16u;
        if (s >= p->length) continue;
        int on;
        if (v->hold & HOLD_EDIT) on = (p->accent >> s) & 1u;
        else if (v->hold & HOLD_SCL) on = (p->roll[v->part] >> s) & 1u;
        else on = (p->hit[v->part] >> s) & 1u;
        if (v->playing && m->pos == s) on = !on; /* running light */
        if (on) leds |= 1ull << (14u + white_keys[i]);
    }
    for (unsigned part = 0; part < UI_BLACK; ++part) {
        int on;
        if (v->hold & HOLD_GLO) on = !(m->mute & BIT(part));
        else if (v->hold & HOLD_HOME) on = (m->solo >> part) & 1u;
        else on = part == v->part;
        if (on) leds |= 1ull << (14u + black_keys[part]);
    }
    uint64_t changed = ui->leds_valid ? leds ^ ui->leds : ~0ull;
    for (unsigned i = 0; i < 14u + UI_KEYS; ++i)
        if (changed & (1ull << i)) fm1_led_set(i, (int)((leds >> i) & 1u));
    ui->leds = leds;
    ui->leds_valid = 1;
}

static void decimal_bpm(char *out, unsigned x10)
{
    if (x10 % 10) snprintf(out, 8, "%u.%u", x10 / 10, x10 % 10);
    else snprintf(out, 8, "%u", x10 / 10);
}

static void draw_header(el_ui_t *ui, gfx_band_t *b)
{
    const ui_model_t *v = &ui->shown;
    const dr_machine_t *m = &ui->engine->drums;
    char text[32], bpm[8];
    gfx_fill(b, 0, 0, GFX_W, 30, PANEL);
    snprintf(text, sizeof text, "%02u", v->pattern + 1u);
    int x = gfx_text(b, 4, 3, text, ACCENT, 2);
    if (v->next != v->pattern) {
        snprintf(text, sizeof text, ">%02u", v->next + 1u);
        x = gfx_text(b, x, 3, text, AMBER, 2);
    }
    gfx_text(b, x + 8, 3, m->pattern[v->next].name, TEXT, 2);
    decimal_bpm(bpm, v->bpm_x10);
    gfx_text(b, GFX_W - 4 - gfx_text_width(bpm, 2), 3, bpm, TEXT, 2);
    snprintf(text, sizeof text, "KIT %s  SW%u  LEN%u", dr_kit_names[v->kit], v->swing, v->length);
    gfx_text(b, 4, 21, text, DIM, 1);
    if (v->length > 16) gfx_text(b, 160, 21, v->step_page ? "2/2" : "1/2", AMBER, 1);
    if (v->recording) gfx_text(b, 186, 21, "REC", RED, 1);
    gfx_text(b, 212, 21, v->playing ? "PLAY" : "STOP", v->playing ? GREEN : DIM, 1);
}

static void draw_grid(el_ui_t *ui, gfx_band_t *b)
{
    const ui_model_t *v = &ui->shown;
    const dr_pattern_t *p = &ui->engine->drums.pattern[v->pattern];
    unsigned base = v->step_page * 16u;
    for (unsigned part = 0; part < DR_PARTS; ++part) {
        int y = GRID_Y + (int)part * CELL;
        if (!gfx_touches(b, y, CELL)) continue;
        int muted = (v->mute >> part) & 1u || (v->solo && !((v->solo >> part) & 1u));
        if (part == v->part) gfx_fill(b, 0, y - 1, GRID_X - 2, CELL, 0x3186);
        gfx_text(b, 3, y + 2, dr_part_names[part], muted ? FAINT : part == v->part ? TEXT : DIM, 1);
        if ((v->mute >> part) & 1u) gfx_text(b, 17, y + 2, "M", RED, 1);
        else if ((v->solo >> part) & 1u) gfx_text(b, 17, y + 2, "S", GREEN, 1);
        for (unsigned i = 0; i < 16; ++i) {
            int x = GRID_X + (int)i * CELL;
            unsigned s = base + i;
            if (i >= v->steps_shown) { gfx_fill(b, x + 4, y + 4, 3, 3, PANEL); continue; }
            uint16_t bg = (i & 3u) == 0 ? 0x31a6 : GRID;
            if (s == v->pos) bg = 0x52aa;
            gfx_fill(b, x, y, CELL - 2, CELL - 2, bg);
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
        for (unsigned i = 0; i < v->steps_shown; ++i)
            if ((p->accent >> (base + i)) & 1u) gfx_fill(b, GRID_X + (int)i * CELL, ACC_Y + 1, CELL - 2, 5, AMBER);
    }
    if (v->pos != 0xff && v->pos >= base && v->pos < base + 16u)
        gfx_fill(b, GRID_X + (int)(v->pos - base) * CELL, GRID_Y - 4, CELL - 2, 2, TEXT);
}

static void draw_footer(el_ui_t *ui, gfx_band_t *b, uint32_t now)
{
    const ui_model_t *v = &ui->shown;
    char value[16];
    gfx_fill(b, 0, FOOT_Y - 4, GFX_W, GFX_H - FOOT_Y + 4, PANEL);
    if (v->hold & HOLD_GEN) {
        const char *labels[4] = {"STYLE", "DENS", "KICK", ""};
        for (unsigned i = 0; i < 3; ++i) {
            int x = 4 + (int)i * 60;
            gfx_text(b, x, FOOT_Y, labels[i], DIM, 1);
            if (i == 0) snprintf(value, sizeof value, "%s", dr_style_names[v->gen_style]);
            else if (i == 1) snprintf(value, sizeof value, "%u", v->gen_density);
            else snprintf(value, sizeof value, "%s", v->gen_keep ? "KEEP" : "NEW");
            gfx_text(b, x, FOOT_Y + 12, value, ACCENT, i == 0 ? 1 : 2);
        }
        gfx_text(b, 4, GFX_H - 12, "GENERATOR: PRESS A WHITE KEY", AMBER, 1);
        return;
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned index = v->param_page * 4u + i;
        const fm1_param_t *d = drum808_param(v->part, (int)index);
        int x = 4 + (int)i * 59;
        if (!d) continue;
        int val = param_value(ui, v->part, index, now);
        gfx_text(b, x, FOOT_Y, d->name, DIM, 1);
        if (d->names) snprintf(value, sizeof value, "%s", d->names[el_clampi(val, 0, d->max)]);
        else snprintf(value, sizeof value, "%d", val);
        gfx_text(b, x, FOOT_Y + 12, value, part_color[v->part], d->names ? 1 : 2);
    }
    const char *hint = v->notice_id ? ui->notice :
                       (v->hold & HOLD_EDIT) ? "EDIT: KEY=ACCENT  BLACK=CLEAR" :
                       (v->hold & HOLD_SCL) ? "SCL: KEY=32ND ROLL" :
                       (v->hold & HOLD_GLO) ? "GLO: BLACK=MUTE E1=KIT E7=SWING" :
                       (v->hold & HOLD_HOME) ? "HOME: BLACK=SOLO" : 0;
    if (hint) gfx_text(b, 4, GFX_H - 12, hint, v->notice_id ? AMBER : TEXT, 1);
    else {
        char line[48];
        unsigned pages = ((unsigned)drum808_nparams(v->part) + 3u) / 4u;
        snprintf(line, sizeof line, "%s  SOUND %u/%u  ENV=PAGE  LFO=GEN", dr_part_names[v->part],
                 v->param_page + 1u, pages);
        gfx_text(b, 4, GFX_H - 12, line, DIM, 1);
    }
}

static uint32_t paint_now;

void el_ui_draw_band(el_ui_t *ui, unsigned band)
{
    gfx_band_t *b = &ui->band;
    gfx_begin(b, band, BACK);
    if (gfx_touches(b, 0, 30)) draw_header(ui, b);
    if (gfx_touches(b, GRID_Y - 4, ACC_Y + 8 - (GRID_Y - 4))) draw_grid(ui, b);
    if (gfx_touches(b, FOOT_Y - 4, GFX_H)) draw_footer(ui, b, paint_now);
}

static void paint(el_ui_t *ui, uint32_t now)
{
    uint32_t started = fm1_millis();
    paint_now = now;
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
    handle_input(ui, controls, now);
    build_model(ui, hold_mask(controls->buttons), now);
    mark_dirty(ui);
    update_leds(ui);
    paint(ui, now);
}
