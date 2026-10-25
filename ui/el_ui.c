/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Four-area panel, laid out after the FM-1's front:
 *
 *   MASTER   SELECT              screen         KNOB 1  KNOB 2  KNOB 3  KNOB 4
 *   PRESETS  ALGORITHM                          FX   SEL  ENV  LFO  EDIT GLO
 *   OCT -    OCT +                              HOME SAVE ARP  SEQ  PLAY REC  (PLAY = PLAY/STOP)
 *   27 keys in two rows: 11 black ones (3-2-3-2-1) above 16 white ones
 *
 * The left column is structure: SELECT the pattern, ALGORITHM tempo (GLO:
 * pattern length, EDIT: swing / note length / step length), PRESETS the
 * sound (BEAT: the kit). The four knobs are the sound, each above the area
 * button it belongs to on the HOME pages. EDIT and GLO are the two shifts.
 *
 * Everywhere
 *   FX / SEL / ENV / LFO  areas BEAT / RHYTHM / STRINGS / BASS; press the
 *                         active one again for the next sound page
 *   GLO + area button     mute / unmute that area
 *   HOME                  overview: KNOB 1-4 levels (GLO: delay time, feedback, reverb
 *                         size, decay), PRESETS reverb (GLO: delay) return, white keys
 *                         pick patterns, OCT -/+ bank 1-16 / 17-24, black keys 1-4 mute.
 *                         Slots 1-16 hold a generated groove in their style; ARP +
 *                         white key = new groove for that slot. New grooves are kept in
 *                         Flash by themselves and survive a restart.
 *   HOME + EDIT           distortion: KNOB 1-4 send of BEAT / RHYTHM / STRINGS / BASS,
 *                         PRESETS type (OFF OD DIST RAT TUBE), ALGORITHM drive, SELECT tone
 *   PLAY / REC            transport / record into the current area (EDIT + REC clears it)
 *   ARP held              generator: KNOB 1 style, KNOB 2 density, white key = new part
 *   ARP tapped            RHYTHM / STRINGS / BASS: arpeggiator on / off (EDIT + ARP: HOLD)
 *   SAVE                  then white key 1-8: copy the playing groove to user slot
 *                         17-24 and store everything in Flash (SAVE again: store only)
 *   SELECT                pattern (all areas)
 *   ALGORITHM             tempo; GLO: pattern length; EDIT: see the areas (HOME + GLO: fine tempo)
 *
 * BEAT    black keys pick + play a drum (BD SD LT HT RS CP CB CY OH CH ZP; SEQ:
 *         second layer with PULSE on black key 1), white keys set steps.
 *         EDIT + white accent, GLO + white roll, EDIT + black clear, GLO + black mute.
 *         KNOB 1-4 drum sound, PRESETS kit, GLO + SELECT instrument (PULSE too),
 *         EDIT + ALGORITHM swing.
 * RHYTHM / STRINGS / BASS
 *   KEYS  (SEQ off) all keys play the sound, OCT -/+ octave. REC while running
 *         records live, REC while stopped enters notes step by step (EDIT + key: rest).
 *         STRINGS: GLO + white key 1-14 picks the chord (1 = single notes).
 *   STEP  (SEQ on) white keys switch steps on/off and select them, EDIT + white
 *         accent, GLO + white tie, OCT -/+ steps 1-16 / 17-32, GLO + SELECT step.
 *         KNOB 1 note, 2 octave, 3 chord (STRINGS) or accent, 4 inversion
 *         (STRINGS) or tie.
 *   KNOB 1-4 sound page, PRESETS sound (MKS-50 sounds, then JP-6; EDIT +
 *   PRESETS switches the engine), EDIT + ALGORITHM note length (STRINGS: step
 *   length 1/16 .. 2 bars for long chord sequences). After the five sound
 *   pages: ARP (mode, rate, octaves, gate).
 *   EDIT + area button: synth editor for that area, every parameter of the
 *   engine in the sections of the original panel (EDIT LED on). SELECT
 *   section, ALGORITHM page, KNOB 1-4 the four knobs of the page, PRESETS
 *   sound, keys play; the sequencer keeps playing. The area button again
 *   (or HOME, SEQ) leaves it. */
#include "el_ui.h"
#include "el_editor.h"
#include "dsp_math.h"
#include <stdio.h>
#include <string.h>

#define BIT(n) (1u << (n))
enum { BACK = 0x0000, PANEL = 0x18e3, GRID = 0x2945, TEXT = 0xef5d, DIM = 0x738e, FAINT = 0x4208,
       ACCENT = 0x07ff, RED = 0xf9a6, AMBER = 0xfd20, GREEN = 0x37e6 };
enum { HOLD_EDIT = 1, HOLD_GLO = 2, HOLD_GEN = 4 };
/* Encoders as FM1 Open numbers them: SELECT right of the MASTER pot,
 * PRESETS under MASTER, ALGORITHM under SELECT, KNOB 1-4 right of the
 * screen above the area buttons. */
enum { ENC_SELECT = 0, ENC_ALGORITHM = 1, ENC_KNOB1 = 2, ENC_PRESETS = 6 };
/* Synth areas: the sound pages, then the arpeggiator. */
enum { PAGE_ARP = UI_PAGES, SYNTH_PAGES };
enum { PARAM_ARP = 0xe0 };
enum { HEADER_H = 31, GRID_X = 28, GRID_Y = 34, CELL = 12, ACC_Y = GRID_Y + DR_PARTS * CELL + 2, FOOT_Y = 194 };
enum { ROLL_Y = 52, ROLL_H = 118 };

static const uint8_t white_keys[UI_WHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[UI_BLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const uint8_t area_buttons[EL_AREAS] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO};
static const char *const area_names[EL_AREAS] = {"BEAT", "RHYTHM", "STRINGS", "BASS"};
static const char *const area_tabs[EL_AREAS] = {"BEAT", "RHYT", "STRG", "BASS"};
static const uint16_t area_color[EL_AREAS] = {0xfb44, 0xfee0, 0xb4bf, 0x3f3b};
static const uint16_t part_color[DR_PARTS] = {
    0xf8a3, 0xfc40, 0xfee0, 0xfee0, 0xfd8c, 0xfd8c, 0xfd8c, 0x5e7f, 0x5e7f, 0x5e7f, 0x87f0, 0xffff};
/* Sound pages of the synth areas (E3-E6). */
static const char *const synth_page_names[SYNTH_PAGES] = {"FILTER", "ENVELOPE", "OSC", "MOD", "FX", "ARP"};
static const uint8_t arp_page[4] = {ARP_MODE, ARP_RATE, ARP_OCTAVES, ARP_GATE};
static const uint8_t synth_pages[SYN_ENGINES][UI_PAGES][4] = {
    {{MP_CUTOFF, MP_RESO, MP_VCF_ENV, MP_VCF_LFO}, {MP_T1, MP_T2, MP_L3, MP_T4},
     {MP_SAW, MP_PULSE, MP_SUB_LEVEL, MP_DCO_ENV}, {MP_LFO_RATE, MP_DCO_LFO, MP_PORTA_TIME, MP_PULSE_DETUNE},
     {MP_CHORUS, MP_DELAY_SEND, MP_REVERB_SEND, MP_VOLUME}},
    {{JP_CUTOFF, JP_RESO, JP_VCF_ENV, JP_VCF_LFO}, {JP_ENV2_A, JP_ENV2_D, JP_ENV2_S, JP_ENV2_R},
     {JP_MIX, JP_XMOD, JP_PWM_DEPTH, JP_VCO2_FINE}, {JP_LFO_RATE, JP_VCO_LFO, JP_PORTA_TIME, JP_ENV1_D},
     {JP_CHORUS, JP_DELAY_SEND, JP_REVERB_SEND, JP_VOLUME}}};
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
    ui->rec_key = ui->rec_note = 0xff;
    ui->octave[EL_AREA_STRINGS] = -1;
    ui->octave[EL_AREA_BASS] = -2;
    for (unsigned a = 0; a < EL_AREAS; ++a) ui->gen_density[a] = 64;
    ui->gen_style[EL_AREA_BEAT] = 6;   /* ROTHER */
    ui->gen_style[EL_AREA_RHYTHM] = 2;
    ui->gen_style[EL_AREA_STRINGS] = 1;
    ui->gen_style[EL_AREA_BASS] = 1;
    memset(ui->key_note, 0xff, sizeof ui->key_note);
    ui->dirty = (uint16_t)((1u << GFX_BANDS) - 1u);
    for (unsigned i = 0; i < 3; ++i) ui->sig[i] = 0xffffffffu;
}

static st_track_t *track_of(el_ui_t *ui) { return &ui->engine->track[ui->area - 1u]; }
static int synth_area(const el_ui_t *ui) { return !ui->overview && ui->area != EL_AREA_BEAT; }
static int in_editor(const el_ui_t *ui) { return ui->editor && synth_area(ui); }

/* Editor position, kept inside the sections and pages of the engine. */
static void ed_position(el_ui_t *ui, unsigned *section, unsigned *page)
{
    unsigned engine = track_of(ui)->part.engine, n = ed_sections(engine);
    if (ui->ed_section[ui->area] >= n) ui->ed_section[ui->area] = (uint8_t)(n - 1u);
    unsigned pages = ed_pages(ed_section(engine, ui->ed_section[ui->area]));
    if (ui->ed_page[ui->area] >= pages) ui->ed_page[ui->area] = (uint8_t)(pages - 1u);
    *section = ui->ed_section[ui->area];
    *page = ui->ed_page[ui->area];
}

/* ---- parameters behind E3-E6 ------------------------------------------- */

typedef struct { const char *name; int value, max; const char *text; uint16_t color; } knob_t;

/* LFO SYNC on: the LFO RATE knob sets and shows the note value (LFO DIV),
 * as in the apps. */
static unsigned synced_rate(const syn_part_t *part, unsigned param)
{
    int jp = part->engine == SYN_JP6;
    if (param != (jp ? JP_LFO_RATE : MP_LFO_RATE) || !syn_values(part)[jp ? JP_LFO_SYNC : MP_LFO_SYNC]) return param;
    return jp ? JP_LFO_DIV : MP_LFO_DIV;
}

static int raw_value(el_ui_t *ui, unsigned slot, unsigned *param_out)
{
    el_engine_t *e = ui->engine;
    unsigned param;
    int value;
    if (ui->overview && (ui->hold & HOLD_EDIT)) {
        param = 300u + slot;                     /* distortion sends per area */
        value = e->fx.v[FX_DIST_SEND_BEAT + slot];
    } else if (ui->overview) {
        param = (ui->hold & HOLD_GLO) ? 100u + slot : 200u + slot;
        if (param >= 200u) value = slot == 0 ? e->drums.level_value : e->track[slot - 1u].level_value;
        else value = e->fx.v[slot == 0 ? FX_DELAY_DIV : slot == 1 ? FX_DELAY_FB : slot == 2 ? FX_REVERB_SIZE : FX_REVERB_DECAY];
    } else if (ui->area == EL_AREA_BEAT) {
        param = ui->page[0] * 4u + slot;
        value = tr808_get(&e->drums.kit, ui->part, (int)param);
        param |= (unsigned)ui->part << 4;
    } else if (in_editor(ui)) {
        const syn_part_t *part = &track_of(ui)->part;
        unsigned section, page;
        ed_position(ui, &section, &page);
        param = ed_param(part->engine, section, page, slot);
        if (param != ED_NONE) param = synced_rate(part, param);
        value = param == ED_NONE ? 0 : syn_values(part)[param];
    } else if (ui->page[ui->area] == PAGE_ARP) {
        param = PARAM_ARP + arp_page[slot];
        value = track_of(ui)->arp.v[arp_page[slot]];
    } else {
        const syn_part_t *part = &track_of(ui)->part;
        param = synced_rate(part, synth_pages[part->engine][ui->page[ui->area]][slot]);
        value = syn_values(part)[param];
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
    if (ui->overview && (ui->hold & HOLD_EDIT)) {
        static const char *const send_names[4] = {"DST BEAT", "DST RHYT", "DST STRG", "DST BASS"};
        k.name = send_names[slot];
        k.max = 127;
        k.color = area_color[slot];
    } else if (ui->overview) {
        static const char *const fx_names[4] = {"DLY TIME", "FEEDBACK", "REV SIZE", "DECAY"};
        k.name = (ui->hold & HOLD_GLO) ? fx_names[slot] : area_tabs[slot];
        k.max = (ui->hold & HOLD_GLO) && slot == 0 ? 11 : 127;
        if ((ui->hold & HOLD_GLO) && slot == 0) k.text = fx_delay_division_name(k.value);
        k.color = (ui->hold & HOLD_GLO) ? ACCENT : area_color[slot];
    } else if (ui->area == EL_AREA_BEAT) {
        const fm1_param_t *d = tr808_param(ui->part, (int)(param & 15u));
        if (!d) return k;
        k.name = d->name;
        k.max = d->max;
        if (d->names) k.text = d->names[el_clampi(k.value, 0, d->max)];
        k.color = part_color[ui->part];
    } else if (in_editor(ui) && param == ED_NONE) {
        return k;
    } else if (!in_editor(ui) && param >= PARAM_ARP && param < PARAM_ARP + ARP_PARAMS) {
        static const char *const octaves[4] = {"1 OCT", "2 OCT", "3 OCT", "4 OCT"};
        unsigned a = param - PARAM_ARP;
        static const char *const names[ARP_PARAMS] = {"ARP", "MODE", "RATE", "OCTAVES", "GATE", "HOLD"};
        k.name = names[a];
        k.max = arp_param_max(a);
        k.text = a == ARP_MODE ? arp_mode_names[k.value % ARP_MODES] : a == ARP_RATE ? arp_rate_names[k.value % ARP_RATES] :
                 a == ARP_OCTAVES ? octaves[k.value & 3] : 0;
        k.color = area_color[ui->area];
    } else {
        unsigned engine = track_of(ui)->part.engine;
        k.name = syn_param_name(engine, param);
        /* The rate knob in sync mode: called RATE, showing the note value. */
        int jp = engine == SYN_JP6;
        if (param == (jp ? JP_LFO_DIV : MP_LFO_DIV) && syn_values(&track_of(ui)->part)[jp ? JP_LFO_SYNC : MP_LFO_SYNC])
            k.name = syn_param_name(engine, jp ? JP_LFO_RATE : MP_LFO_RATE);
        k.max = syn_param_max(engine, param);
        k.text = syn_param_value_name(engine, param, k.value);
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
    int value = raw_value(ui, slot, &param) + step, r;
    value = el_clampi(value, param == PARAM_ARP + ARP_GATE ? 5 : 0, k.max);
    if (ui->overview && param >= 300u) {
        r = el_post(e, EL_CMD_FX, (uint8_t)(FX_DIST_SEND_BEAT + slot), (int16_t)value);
    } else if (ui->overview) {
        if (param >= 200u)
            r = slot == 0 ? dr_post(&e->drums, DR_CMD_LEVEL, 0, 0, 0, (int16_t)value)
                          : el_post3(e, EL_CMD_TRACK_LEVEL, (uint8_t)(slot - 1u), 0, 0, (int16_t)value);
        else
            r = el_post(e, EL_CMD_FX, (uint8_t)(slot == 0 ? FX_DELAY_DIV : slot == 1 ? FX_DELAY_FB : slot == 2 ? FX_REVERB_SIZE : FX_REVERB_DECAY),
                        (int16_t)value);
    } else if (ui->area == EL_AREA_BEAT) {
        r = dr_post(&e->drums, DR_CMD_PARAM, ui->part, (uint8_t)(param & 15u), 0, (int16_t)value);
    } else if (!in_editor(ui) && param >= PARAM_ARP) {
        r = el_post3(e, EL_CMD_TRACK_ARP, (uint8_t)(ui->area - 1u), (uint8_t)(param - PARAM_ARP), 0, (int16_t)value);
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

static void keep_later(el_ui_t *ui)
{
    ui->autosave = 1;
    ui->autosave_at = ui->now + 2000u;
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
        if (dr_post_pattern(m, m->current, &g) == 0) { notice(ui, "NEW BEAT"); keep_later(ui); }
        return;
    }
    st_track_t *t = track_of(ui);
    st_pattern_t g;
    /* Strings follow the chord family of the groove's style. */
    unsigned harmony = dr_styles[m->pattern[m->current].style % DR_STYLES].harmony;
    st_generate(&g, ui->area, ui->gen_style[ui->area], ui->gen_density[ui->area], seed, t->pattern[t->current].length, harmony);
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
    if (el_load_pattern(e, ui->area - 1u, t->current, &g) == 0) { notice(ui, "NEW PART"); keep_later(ui); }
}

/* Slot an area plays now, and the one it switches to. */
static unsigned area_slot(const el_ui_t *ui, unsigned a, int next)
{
    const el_engine_t *e = ui->engine;
    if (a == EL_AREA_BEAT) return next ? e->drums.next : e->drums.current;
    return next ? e->track[a - 1u].next : e->track[a - 1u].current;
}

static void select_slot(el_ui_t *ui, unsigned a, unsigned slot)
{
    if (slot >= DR_PATTERNS) return;
    if (a == EL_AREA_BEAT) dr_post(&ui->engine->drums, DR_CMD_SELECT, (uint8_t)slot, 0, 0, 0);
    else el_post3(ui->engine, EL_CMD_TRACK_SELECT, (uint8_t)(a - 1u), (uint8_t)slot, 0, 0);
}

/* Variation of the pattern an area plays (amount from the generator's
 * second knob). */
static void vary(el_ui_t *ui, unsigned a, uint32_t seed, dr_pattern_t *drums, st_pattern_t *part)
{
    el_engine_t *e = ui->engine;
    unsigned amount = 24u + ui->gen_density[a] * 3u / 4u;
    /* A variation that changes nothing would look like a broken key: try
     * again with other dice (a short or empty pattern may stay as it is). */
    for (unsigned tries = 0; tries < 8; ++tries, seed = seed * 2654435761u + 1u) {
        if (a == EL_AREA_BEAT) {
            *drums = e->drums.pattern[e->drums.current];
            dr_vary(drums, amount, seed);
            if (memcmp(drums, &e->drums.pattern[e->drums.current], sizeof *drums)) return;
        } else {
            const st_track_t *t = &e->track[a - 1u];
            *part = t->pattern[t->current];
            st_vary(part, a, amount, seed);
            if (memcmp(part, &t->pattern[t->current], sizeof *part)) return;
        }
    }
}

/* Whole groove, every area in the slot it plays: a variation of each, or
 * all new in the generator's beat style. Posted by run_jobs over the next
 * passes. */
static void groove_job(el_ui_t *ui, int fresh)
{
    if (ui->job_stage) return;
    uint32_t seed = ui->now * 2654435761u + 40503u;
    if (fresh) {
        dr_generate(&ui->job_drums, ui->gen_style[EL_AREA_BEAT], 24u + ui->gen_density[EL_AREA_BEAT] / 4u, seed, NULL);
        el_generate_parts(&ui->job_drums, seed ^ 0x5bd1e995u, ui->job_parts);
    } else {
        for (unsigned a = 0; a < EL_AREAS; ++a)
            vary(ui, a, seed + 7919u * a, &ui->job_drums, a ? &ui->job_parts[a - 1u] : NULL);
    }
    for (unsigned a = 0; a < EL_AREAS; ++a) ui->job_slot[a] = (uint8_t)area_slot(ui, a, 0);
    ui->job_fresh = (uint8_t)fresh;
    ui->job_stage = 1;
}

static void run_jobs(el_ui_t *ui)
{
    el_engine_t *e = ui->engine;
    while (ui->job_stage) {
        unsigned st = ui->job_stage;
        int r = st == 1 ? dr_post_pattern(&e->drums, ui->job_slot[0], &ui->job_drums)
                        : el_load_pattern(e, st - 2u, ui->job_slot[st - 1u], &ui->job_parts[st - 2u]);
        if (r) break;   /* staging busy: next pass */
        if (++ui->job_stage > 1u + EL_TRACKS) {
            ui->job_stage = 0;
            notice(ui, ui->job_fresh ? "NEW GROOVE" : "GROOVE VARIED");
            keep_later(ui);
        }
    }
    if (ui->autosave && !ui->job_stage && (int32_t)(ui->now - ui->autosave_at) >= 0 &&
        e->drums.staging_posted == e->drums.staging_taken && e->staged_posted == e->staged_taken) {
        ui->autosave = 0;
        if (ui->save && ui->save(e) != 0) notice(ui, "SAVE FAILED");
    }
}

/* Generator keys need a second press: the first only asks. */
enum { ASK_NONE, ASK_VARY, ASK_NEW };

static int confirmed(el_ui_t *ui, unsigned ask, const char *question)
{
    int again = ui->ask == ask && ui->ask_area == (ui->overview ? 4u : ui->area) && (int32_t)(ui->now - ui->ask_until) < 0;
    if (again) { ui->ask = ASK_NONE; return 1; }
    ui->ask = (uint8_t)ask;
    ui->ask_area = (uint8_t)(ui->overview ? 4u : ui->area);
    ui->ask_until = ui->now + 3000u;
    notice(ui, question);
    ui->notice_until = ui->ask_until;
    return 0;
}

/* ARP held + white key 1: variation, key 2: new (in HOME for all areas). */
static void generator_key(el_ui_t *ui, unsigned key)
{
    int w = key_white(key);
    if (w != 0 && w != 1) { ui->ask = ASK_NONE; return; }
    int all = ui->overview;
    if (w == 0) {
        if (!confirmed(ui, ASK_VARY, all ? "VARY ALL AREAS? KEY 1 AGAIN" : "VARY THIS PATTERN? KEY 1 AGAIN")) return;
        if (all) { groove_job(ui, 0); return; }
        dr_pattern_t d;
        st_pattern_t p;
        vary(ui, ui->area, ui->now * 2654435761u + 977u, &d, &p);
        int r = ui->area == EL_AREA_BEAT ? dr_post_pattern(&ui->engine->drums, ui->engine->drums.current, &d)
                                         : el_load_pattern(ui->engine, ui->area - 1u, track_of(ui)->current, &p);
        if (r == 0) { notice(ui, "VARIED"); keep_later(ui); } else notice(ui, "BUSY, TRY AGAIN");
        return;
    }
    if (!confirmed(ui, ASK_NEW, all ? "NEW GROOVE, ALL AREAS? KEY 2 AGAIN" : "NEW PATTERN? KEY 2 AGAIN")) return;
    if (all) groove_job(ui, 1);
    else generate(ui, key);
}

/* ---- sounds: factory ones, then the user slots ----------------------------- */

/* Names of the user slots, read once from Flash (and after each store). */
static void user_cache(el_ui_t *ui)
{
    if (ui->user_cached) return;
    ui->user_cached = 1;
    for (unsigned i = 0; i < EL_USER_SOUNDS; ++i) {
        el_sound_t s;
        ui->user_used[i] = 0;
        if (!ui->sound_get || ui->sound_get(i, &s) != 0 || !s.used) continue;
        memcpy(ui->user_name[i], s.name, sizeof ui->user_name[i]);
        ui->user_engine[i] = s.engine;
        ui->user_used[i] = 1;
    }
}

/* What the display shows for a sound id: factory name or "U05 NAME". */
static const char *sound_label(el_ui_t *ui, unsigned id)
{
    static char label[EL_SOUND_NAME + 6];
    if (id < EL_USER_SOUND) return syn_sound_name(id);
    user_cache(ui);
    unsigned slot = id - EL_USER_SOUND;
    snprintf(label, sizeof label, "U%02u %s", slot + 1u, slot < EL_USER_SOUNDS && ui->user_used[slot] ? ui->user_name[slot] : "?");
    return label;
}

static unsigned sound_engine_of(el_ui_t *ui, unsigned id)
{
    if (id < EL_USER_SOUND) return syn_sound_engine(id);
    user_cache(ui);
    return ui->user_engine[(id - EL_USER_SOUND) % EL_USER_SOUNDS];
}

static int load_sound_id(el_ui_t *ui, unsigned id)
{
    unsigned track = ui->area - 1u;
    if (id < EL_USER_SOUND) return el_load_preset(ui->engine, track, id);
    el_sound_t s;
    if (!ui->sound_get || ui->sound_get(id - EL_USER_SOUND, &s) != 0 || !s.used) return -1;
    return el_load_sound(ui->engine, track, id, s.engine, s.v);
}

/* One step through the area's factory sounds (MKS-50, then JP-6), then the
 * user slots (all of them, whatever area they were made in). */
static unsigned next_sound(el_ui_t *ui, unsigned id, int dir)
{
    int count = (int)syn_sound_count();
    user_cache(ui);
    if (id < EL_USER_SOUND) {
        int i = (int)id;
        do i += dir; while (i >= 0 && i < count && syn_sound_area((unsigned)i) != ui->area);
        if (i >= 0 && i < count) return (unsigned)i;
        if (dir > 0)
            for (unsigned u = 0; u < EL_USER_SOUNDS; ++u) if (ui->user_used[u]) return EL_USER_SOUND + u;
        return id;
    }
    for (int u = (int)(id - EL_USER_SOUND) + dir; u >= 0 && u < EL_USER_SOUNDS; u += dir)
        if (ui->user_used[u]) return EL_USER_SOUND + (unsigned)u;
    if (dir < 0)
        for (int i = count - 1; i >= 0; --i) if (syn_sound_area((unsigned)i) == ui->area) return (unsigned)i;
    return id;
}

/* PRESETS steps through the sounds; EDIT + PRESETS jumps to the other
 * engine's first factory sound. */
static void browse_preset(el_ui_t *ui, int delta, int other_engine)
{
    st_track_t *t = track_of(ui);
    unsigned id = t->preset;
    int count = (int)syn_sound_count(), dir = delta > 0 ? 1 : -1, moves = delta > 0 ? delta : -delta;
    if (other_engine) {
        unsigned want = sound_engine_of(ui, id) == SYN_MKS ? SYN_JP6 : SYN_MKS;
        for (int i = 0; i < count; ++i)
            if (syn_sound_engine((unsigned)i) == want && syn_sound_area((unsigned)i) == ui->area) { id = (unsigned)i; break; }
        moves = 0;
    }
    while (moves--) {
        unsigned n = next_sound(ui, id, dir);
        if (n == id) break;
        id = n;
    }
    if (id != t->preset && load_sound_id(ui, id) == 0 && other_engine)
        notice(ui, syn_engine_names[sound_engine_of(ui, id)]);
}

/* SAVE in the editor: pick a user slot (PRESETS or SELECT), name it (KNOB 1
 * cursor, KNOB 2 letter), SAVE again stores it; any other button cancels. */
static const char name_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-#&/.+";

static void sound_save_start(el_ui_t *ui)
{
    const st_track_t *t = track_of(ui);
    user_cache(ui);
    unsigned slot = 0;
    if (t->preset >= EL_USER_SOUND) slot = (t->preset - EL_USER_SOUND) % EL_USER_SOUNDS;
    else while (slot + 1u < EL_USER_SOUNDS && ui->user_used[slot]) ++slot;
    const char *base = t->preset >= EL_USER_SOUND && ui->user_used[slot] ? ui->user_name[slot] : syn_sound_name(t->preset);
    memset(ui->sound_name, ' ', EL_SOUND_NAME);
    ui->sound_name[EL_SOUND_NAME] = 0;
    for (unsigned i = 0; i < EL_SOUND_NAME && base[i]; ++i) {
        char c = base[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        ui->sound_name[i] = strchr(name_chars, c) ? c : ' ';
    }
    ui->sound_slot = (uint8_t)slot;
    ui->name_cursor = 0;
    ui->sound_save = 1;
}

static void sound_save_store(el_ui_t *ui)
{
    st_track_t *t = track_of(ui);
    el_sound_t s;
    memset(&s, 0, sizeof s);
    memcpy(s.name, ui->sound_name, EL_SOUND_NAME);
    for (int i = EL_SOUND_NAME - 1; i >= 0 && s.name[i] == ' '; --i) s.name[i] = 0;   /* no trailing blanks */
    s.engine = t->part.engine;
    s.used = 1;
    memcpy(s.v, syn_values(&t->part), syn_value_count(s.engine));
    ui->sound_save = 0;
    if (!ui->sound_put || ui->sound_put(ui->sound_slot, &s) != 0) { notice(ui, "SOUND SAVE FAILED"); return; }
    ui->user_cached = 0;   /* names read again */
    load_sound_id(ui, EL_USER_SOUND + ui->sound_slot);   /* the area now shows the slot */
    notice(ui, "SOUND SAVED");
}

static void sound_save_edit(el_ui_t *ui, const int16_t *enc)
{
    int slot_turn = enc[0] + enc[6];
    if (slot_turn) ui->sound_slot = (uint8_t)el_clampi((int)ui->sound_slot + slot_turn, 0, EL_USER_SOUNDS - 1);
    if (enc[2]) ui->name_cursor = (uint8_t)el_clampi((int)ui->name_cursor + enc[2], 0, EL_SOUND_NAME - 1);
    if (enc[3]) {
        int n = (int)sizeof name_chars - 1;
        const char *at = strchr(name_chars, ui->sound_name[ui->name_cursor]);
        int i = at ? (int)(at - name_chars) : 0;
        i = ((i + enc[3]) % n + n) % n;
        ui->sound_name[ui->name_cursor] = name_chars[i];
    }
}

/* Black key -> drum part. 11 keys for 12 parts: BD .. ZAP on the black
 * keys, SEQ in BEAT switches to the second layer with PULSE on key 1. */
static int beat_part(const el_ui_t *ui, int black)
{
    int part = black < 0 ? -1 : black + (ui->beat_layer ? UI_BLACK : 0);
    return part >= 0 && part < DR_PARTS && (ui->beat_layer || black < UI_BLACK) ? part : -1;
}

static void keys_beat(el_ui_t *ui, unsigned key, unsigned hold)
{
    dr_machine_t *m = &ui->engine->drums;
    const dr_pattern_t *p = &m->pattern[m->current];
    int part = beat_part(ui, key_black(key)), step = key_white(key);
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

/* Step record while stopped, as on the SH-101 and the KeyStep: every key
 * writes the next step and the sequence ends after it; a key pressed while
 * the one before is still held ties the two (legato / slide). EDIT + key
 * writes a rest, GLO + key holds the note before for one more step. The
 * first key after REC starts a new sequence. */
/* REC mode of the area (GLO + REC): step by step like the SH-101, or live. */
static int rec_step_mode(const el_ui_t *ui)
{
    return synth_area(ui) && (ui->engine->option[EL_OPT_REC_STEP] >> (ui->area - 1u)) & 1u;
}

static void step_record(el_ui_t *ui, unsigned key, int note, unsigned hold)
{
    el_engine_t *e = ui->engine;
    st_track_t *t = track_of(ui);
    uint8_t track = (uint8_t)(ui->area - 1u);
    unsigned step = ui->rec_fresh ? 0u : ui->cursor[ui->area];
    if (step >= ST_STEPS) { notice(ui, "32 STEPS: REC TO FINISH"); return; }
    if (ui->rec_fresh) { el_post3(e, EL_CMD_TRACK_CLEAR, track, 0, 0, 0); ui->rec_fresh = 0; }
    if (hold & HOLD_EDIT) {
        el_post3(e, EL_CMD_STEP_WRITE, track, (uint8_t)step, (uint8_t)note, 0);
        ui->rec_key = ui->rec_note = 0xff;   /* a rest: nothing to tie to */
    } else if ((hold & HOLD_GLO) && step > 0 && ui->rec_note != 0xff) {
        el_post3(e, EL_CMD_STEP_TIE, track, (uint8_t)(step - 1u), 1, 0);
        el_post3(e, EL_CMD_STEP_WRITE, track, (uint8_t)step, ui->rec_note, (int16_t)(ST_ON | t->chord << 8));
    } else {
        if (step > 0 && ui->rec_key != 0xff) el_post3(e, EL_CMD_STEP_TIE, track, (uint8_t)(step - 1u), 1, 0);
        el_post3(e, EL_CMD_STEP_WRITE, track, (uint8_t)step, (uint8_t)note, (int16_t)(ST_ON | t->chord << 8));
        if (el_post3(e, EL_CMD_NOTE, track, (uint8_t)note, 1, 100) == 0) {   /* hear it, not live-recorded */
            ui->key_note[key] = (uint8_t)note;
            ui->key_track[key] = track;
        }
        ui->rec_key = (uint8_t)key;
        ui->rec_note = (uint8_t)note;
    }
    ui->cursor[ui->area] = (uint8_t)(step + 1u);
    ui->step_page[ui->area] = (uint8_t)(step / 16u);
}

static void keys_synth_down(el_ui_t *ui, unsigned key, unsigned hold)
{
    el_engine_t *e = ui->engine;
    st_track_t *t = track_of(ui);
    uint8_t track = (uint8_t)(ui->area - 1u);
    const st_pattern_t *p = &t->pattern[t->current];
    int step = key_white(key);
    if (ui->step_mode[ui->area] && !ui->editor) {   /* the editor always plays */
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
    if (ui->transpose_mode[ui->area]) {
        /* TRANSPOSE (KeyStep): the key moves the whole sequence, C4 = 0. */
        int shift = note - 60;
        el_post3(e, EL_CMD_TRACK_TRANSPOSE, track, 0, 0, (int16_t)shift);
        static char text[20];
        snprintf(text, sizeof text, "TRANSPOSE %+d", shift);
        notice(ui, text);
        return;
    }
    if (t->recording && rec_step_mode(ui)) {
        step_record(ui, key, note, hold);
        return;
    }
    (void)p;
    if (el_post3(e, EL_CMD_NOTE, track, (uint8_t)note, 0, 100) == 0) {
        ui->key_note[key] = (uint8_t)note;
        ui->key_track[key] = track;
    }
}

static void release_keys(el_ui_t *ui, uint32_t released)
{
    for (unsigned key = 0; key < UI_KEYS; ++key)
        if ((released & BIT(key)) && ui->key_note[key] != 0xff) {
            if (el_post3(ui->engine, EL_CMD_NOTE, ui->key_track[key], ui->key_note[key], 0, 0) == 0) ui->key_note[key] = 0xff;
        }
}

/* Tap on ARP: arpeggiator of the area on / off; with EDIT: HOLD. Keys held
 * now are let go first, so no note hangs between the two paths. */
static void toggle_arp(el_ui_t *ui, int hold)
{
    if (!synth_area(ui)) return;
    st_track_t *t = track_of(ui);
    unsigned param = hold ? ARP_HOLD : ARP_ON;
    int on = !t->arp.v[param];
    release_keys(ui, ~0u);
    if (el_post3(ui->engine, EL_CMD_TRACK_ARP, (uint8_t)(ui->area - 1u), (uint8_t)param, 0, (int16_t)on)) return;
    if (hold) notice(ui, on ? "ARP HOLD ON" : "ARP HOLD OFF");
    else {
        notice(ui, on ? "ARP ON" : "ARP OFF");
        if (on && ui->step_mode[ui->area]) ui->step_mode[ui->area] = 0;   /* the keys play the arp */
        if (on) ui->page[ui->area] = PAGE_ARP;
    }
}

/* ---- OPTIONS (HOME + SAVE): settings made once ----------------------------- */

enum { OPT_MIDI_BEAT, OPT_MIDI_BASS = OPT_MIDI_BEAT + 3, OPT_CLOCK, OPT_MASTER, OPT_THRESHOLD, OPT_RATIO, OPT_ATTACK,
       OPT_RELEASE, OPT_MAKEUP, OPT_LEVEL, OPT_START, OPT_HINTS, OPT_ITEMS };
static const char *const opt_names[OPT_ITEMS] = {"MIDI CH BEAT", "MIDI CH RHYTHM", "MIDI CH STRINGS", "MIDI CH BASS",
                                                 "MIDI CLOCK", "MASTER FX", "  COMP THRESHOLD", "  COMP RATIO", "  COMP ATTACK",
                                                 "  COMP RELEASE", "  COMP MAKEUP", "OUTPUT LEVEL", "POWER-ON", "HELP LINE"};
/* Engine option behind each menu line (the MIDI lines use midi_channel). */
static const uint8_t opt_option[OPT_ITEMS] = {0, 0, 0, 0, EL_OPT_MIDI_CLOCK, EL_OPT_MASTER, EL_OPT_COMP_THRESHOLD, EL_OPT_COMP_RATIO,
                                              EL_OPT_COMP_ATTACK, EL_OPT_COMP_RELEASE, EL_OPT_COMP_MAKEUP, EL_OPT_OUT_LEVEL,
                                              EL_OPT_START, EL_OPT_HINTS};

static int opt_value(const el_ui_t *ui, unsigned item, int *max)
{
    const el_engine_t *e = ui->engine;
    if (item <= OPT_MIDI_BASS) { *max = EL_MIDI_OFF; return e->midi_channel[item - OPT_MIDI_BEAT]; }
    *max = el_option_max(opt_option[item]);
    return e->option[opt_option[item]];
}

static void opt_text(const el_ui_t *ui, unsigned item, char *out, size_t size)
{
    int max, v = opt_value(ui, item, &max);
    if (item <= OPT_MIDI_BASS) { if (v >= EL_MIDI_OFF) snprintf(out, size, "OFF"); else snprintf(out, size, "%d", v + 1); }
    else if (item == OPT_CLOCK) snprintf(out, size, v ? "FOLLOW" : "IGNORE");
    else if (item == OPT_START) snprintf(out, size, v == EL_START_SLOT_A ? "SLOTS A" : "LAST");
    else if (item == OPT_LEVEL) snprintf(out, size, "%d", v);
    else if (item == OPT_THRESHOLD) snprintf(out, size, "-%d DB", v);
    else if (item == OPT_RATIO) {
        int r10 = (int)(el_comp_ratios[v % EL_COMP_RATIOS] * 10.0f + 0.5f);
        if (r10 == 10) snprintf(out, size, "OFF (1:1)");
        else if (r10 % 10) snprintf(out, size, "%d.%d:1", r10 / 10, r10 % 10);
        else snprintf(out, size, "%d:1", r10 / 10);
    }
    else if (item == OPT_ATTACK) snprintf(out, size, "%u MS", el_comp_attacks[v % EL_COMP_ATTACKS]);
    else if (item == OPT_RELEASE) snprintf(out, size, "%u MS", el_comp_releases[v % EL_COMP_RELEASES]);
    else if (item == OPT_MAKEUP) snprintf(out, size, "+%d.%d DB", v / 2, v % 2 ? 5 : 0);
    else snprintf(out, size, v ? "ON" : "OFF");
}

static void options_input(el_ui_t *ui, const struct fm1_controls *c, uint32_t pressed)
{
    el_engine_t *e = ui->engine;
    const int16_t *enc = c->encoders;
    if (pressed & (BIT(BTN_HOME) | BIT(BTN_SAVE))) {
        ui->options = 0;
        notice(ui, ui->save && ui->save(e) == 0 ? "OPTIONS SAVED" : "OPTIONS SET");
        return;
    }
    int move = enc[ENC_SELECT] + enc[ENC_ALGORITHM];
    if (move) ui->opt_item = (uint8_t)el_clampi((int)ui->opt_item + move, 0, OPT_ITEMS - 1);
    int turn = enc[ENC_PRESETS];
    for (unsigned k = 0; k < 4; ++k) turn += enc[ENC_KNOB1 + k];
    if (!turn) return;
    unsigned item = ui->opt_item;
    int max, v = opt_value(ui, item, &max);
    v = el_clampi(v + (max > 32 ? turn * 2 : turn), 0, max);
    if (item <= OPT_MIDI_BASS) el_post(e, EL_CMD_MIDI_CHANNEL, (uint8_t)(item - OPT_MIDI_BEAT), (int16_t)v);
    else el_post(e, EL_CMD_OPTION, opt_option[item], (int16_t)v);
}

static void handle_input(el_ui_t *ui, const struct fm1_controls *c)
{
    el_engine_t *e = ui->engine;
    dr_machine_t *m = &e->drums;
    uint32_t pressed = c->buttons & ~ui->prev_buttons, keys_down = c->keys & ~ui->prev_keys;
    unsigned hold = hold_mask(c->buttons);
    ui->hold = hold;
    /* A pending generator question goes with any other button or ARP let go. */
    if ((pressed & ~BIT(BTN_ARP)) || (ui->prev_buttons & ~c->buttons & BIT(BTN_ARP))) ui->ask = ASK_NONE;
    if (ui->options) {
        options_input(ui, c, pressed);
        ui->prev_buttons = c->buttons;
        ui->prev_keys = c->keys;
        return;
    }
    if ((pressed & BIT(BTN_SAVE)) && (c->buttons & BIT(BTN_HOME))) {
        /* HOME held + SAVE: OPTIONS. HOME's own press switched the overview;
         * undo that so the menu returns where it was opened. */
        if (!(pressed & BIT(BTN_HOME))) ui->overview = !ui->overview;
        release_keys(ui, ~0u);
        ui->options = 1;
        ui->opt_item = 0;
        ui->prev_buttons = c->buttons;
        ui->prev_keys = c->keys;
        return;
    }
    if (ui->sound_save && !in_editor(ui)) ui->sound_save = 0;
    if (ui->sound_save && (pressed & ~BIT(BTN_SAVE))) {
        ui->sound_save = 0;
        notice(ui, "SOUND SAVE CANCELLED");
        pressed = 0;   /* the cancelling press does nothing else */
    }
    if (ui->sound_save) keys_down = 0;   /* naming: the keys rest */
    /* ARP tapped (nothing else used while it was down): arpeggiator. */
    if (pressed & BIT(BTN_ARP)) ui->arp_tap = 1;
    if ((pressed & ~BIT(BTN_ARP)) || keys_down) ui->arp_tap = 0;
    for (unsigned i = 0; i < UI_ENCODERS; ++i) if (c->encoders[i]) ui->arp_tap = 0;
    if ((ui->prev_buttons & ~c->buttons & BIT(BTN_ARP)) && ui->arp_tap) {
        ui->arp_tap = 0;
        toggle_arp(ui, (hold & HOLD_EDIT) != 0);
    }

    for (unsigned a = 0; a < EL_AREAS; ++a) {
        if (!(pressed & BIT(area_buttons[a]))) continue;
        if (hold & HOLD_GLO) {
            set_area_mute(ui, a, !area_muted(ui, a));
            continue;
        }
        if (hold & HOLD_EDIT) {
            /* EDIT + area button: the synth editor of that area, again: back. */
            if (a == EL_AREA_BEAT) { notice(ui, "EDITOR: RHYTHM STRINGS BASS"); continue; }
            int leave = in_editor(ui) && ui->area == a;
            release_keys(ui, ~0u);
            ui->area = (uint8_t)a;
            ui->overview = 0;
            ui->editor = (uint8_t)!leave;
            continue;
        }
        if (in_editor(ui) && ui->area == a) {
            ui->editor = 0;
        } else if (!ui->overview && ui->area == a) {
            unsigned pages = a == EL_AREA_BEAT ? ((unsigned)tr808_nparams(ui->part) + 3u) / 4u : SYNTH_PAGES;
            ui->page[a] = (uint8_t)((ui->page[a] + 1u) % pages);
        } else {
            release_keys(ui, ~0u);
            ui->area = (uint8_t)a;
            ui->overview = 0;
            if (a == EL_AREA_BEAT) ui->editor = 0;
        }
    }
    if (pressed & BIT(BTN_HOME)) { ui->overview = !ui->overview; ui->editor = 0; }
    if (pressed & BIT(BTN_PLAY)) {
        el_post(e, e->transport.running ? EL_CMD_STOP : EL_CMD_START, 0, 0);
        ui->follow = 1;
    }
    if ((pressed & BIT(BTN_REC)) && (hold & HOLD_GLO)) {
        /* GLO + REC: how this area records, kept with the settings. */
        if (synth_area(ui)) {
            int bit = 1 << (ui->area - 1u), step = !(e->option[EL_OPT_REC_STEP] & bit);
            el_post(e, EL_CMD_OPTION, EL_OPT_REC_STEP, (int16_t)(step ? e->option[EL_OPT_REC_STEP] | bit : e->option[EL_OPT_REC_STEP] & ~bit));
            el_post3(e, EL_CMD_RECORD_AREA, ui->area, 0, 0, 0);   /* a running take ends */
            notice(ui, step ? "REC MODE: SH-101 STEP" : "REC MODE: LIVE");
            keep_later(ui);
        } else notice(ui, "BEAT RECORDS LIVE");
        pressed &= ~BIT(BTN_REC);
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
            ui->rec_key = ui->rec_note = 0xff;
            ui->rec_fresh = (uint8_t)(!armed && rec_step_mode(ui));
            if (ui->rec_fresh) notice(ui, "STEP REC: KEYS WRITE A NEW SEQUENCE");
            else if (!armed && !e->transport.running) notice(ui, "LIVE REC: PLAY TO START");
        }
    }
    if ((pressed & BIT(BTN_SAVE)) && in_editor(ui) && ui->sound_put) {
        if (ui->sound_save) sound_save_store(ui);
        else sound_save_start(ui);
    } else if ((pressed & BIT(BTN_SAVE)) && ui->save) {
        notice(ui, ui->save(e) == 0 ? "SAVED" : "SAVE FAILED");   /* everything, all slots */
    }
    if ((pressed & BIT(BTN_SEQ)) && !ui->overview && ui->area == EL_AREA_BEAT) {
        ui->beat_layer = !ui->beat_layer;
        notice(ui, ui->beat_layer ? "BLACK KEY 1: PULSE" : "BLACK KEYS: BD .. ZAP");
    }
    if ((pressed & BIT(BTN_SEQ)) && synth_area(ui) && (hold & HOLD_GLO)) {
        /* GLO + SEQ: TRANSPOSE, the keys move the sequence (KeyStep). */
        release_keys(ui, ~0u);
        ui->transpose_mode[ui->area] = (uint8_t)!ui->transpose_mode[ui->area];
        if (!ui->transpose_mode[ui->area]) el_post3(e, EL_CMD_TRACK_TRANSPOSE, (uint8_t)(ui->area - 1u), 0, 0, 0);
        notice(ui, ui->transpose_mode[ui->area] ? "TRANSPOSE ON: KEYS MOVE THE SEQUENCE" : "TRANSPOSE OFF");
        pressed &= ~BIT(BTN_SEQ);
    }
    if ((pressed & BIT(BTN_SEQ)) && synth_area(ui)) {
        release_keys(ui, ~0u);
        ui->editor = 0;
        ui->step_mode[ui->area] = !ui->step_mode[ui->area];
    }
    if (pressed & (BIT(BTN_OCT_DOWN) | BIT(BTN_OCT_UP))) {
        int up = (pressed & BIT(BTN_OCT_UP)) != 0;
        if (ui->overview) {
            /* HOME: nothing on OCT */
        } else if (synth_area(ui) && !ui->step_mode[ui->area]) {
            release_keys(ui, ~0u);
            ui->octave[ui->area] = (int8_t)el_clampi(ui->octave[ui->area] + (up ? 1 : -1), -3, 2);
        } else if (!ui->overview) {
            unsigned length = ui->area == EL_AREA_BEAT ? m->pattern[m->current].length
                                                       : track_of(ui)->pattern[track_of(ui)->current].length;
            ui->step_page[ui->area] = (uint8_t)(up && length > 16);
            ui->follow = 0;
        }
    }

    if (ui->rec_key != 0xff && (ui->prev_keys & ~c->keys & BIT(ui->rec_key))) ui->rec_key = 0xff;
    /* Punch-in effects end when their key is let go, wherever the UI is. */
    for (unsigned i = 0; i < 2; ++i) {
        static const uint8_t fx[2] = {EL_PUNCH_STUTTER, EL_PUNCH_THROW};
        if (ui->punch_key[i] && !(c->keys & BIT(ui->punch_key[i] - 1u)) && el_post3(e, EL_CMD_PUNCH, fx[i], 0, 0, 0) == 0)
            ui->punch_key[i] = 0;
    }
    release_keys(ui, ui->prev_keys & ~c->keys);
    for (unsigned key = 0; key < UI_KEYS; ++key) {
        if (!(keys_down & BIT(key))) continue;
        if (hold & HOLD_GEN) {
            if (ui->overview || key_white(key) >= 0) generator_key(ui, key);
        } else if (ui->overview) {
            /* White keys 1-4 BEAT A-D, 5-8 RHYTHM, 9-12 STRINGS, 13-16 BASS. */
            int w = key_white(key), b = key_black(key);
            if (w >= 0) select_slot(ui, (unsigned)w / 4u, (unsigned)w % 4u);
            else if (b >= 0 && b < EL_AREAS) set_area_mute(ui, (unsigned)b, !area_muted(ui, (unsigned)b));
            else if (b >= 4 && b <= 5) {
                /* Black keys 5 and 6: punch-in effects while held. */
                static const uint8_t fx[2] = {EL_PUNCH_STUTTER, EL_PUNCH_THROW};
                static const char *const names[2] = {"STUTTER", "REVERB THROW"};
                if (el_post3(e, EL_CMD_PUNCH, fx[b - 4], 1, 0, 0) == 0) ui->punch_key[b - 4] = (uint8_t)(key + 1u);
                notice(ui, names[b - 4]);
            }
        } else if (ui->area == EL_AREA_BEAT) {
            keys_beat(ui, key, hold);
        } else {
            keys_synth_down(ui, key, hold);
        }
    }

    /* Encoders by where they sit: the left column is structure (SELECT
     * pattern, ALGORITHM tempo and length, PRESETS sound or kit), the four
     * knobs on the right are the sound. */
    if (ui->sound_save) {
        sound_save_edit(ui, c->encoders);
        ui->prev_buttons = c->buttons;
        ui->prev_keys = c->keys;
        return;
    }
    const int16_t *enc = c->encoders;
    const int select = enc[ENC_SELECT], algo = enc[ENC_ALGORITHM], presets = enc[ENC_PRESETS];
    int dist_page = ui->overview && (hold & HOLD_EDIT);
    if (dist_page) {
        /* HOME + EDIT: KNOB 1-4 send per area, PRESETS type, ALGORITHM drive, SELECT tone. */
        const uint8_t *fv = e->fx.v;
        if (presets) el_post(e, EL_CMD_FX, FX_DIST_MODE, (int16_t)el_clampi(fv[FX_DIST_MODE] + (presets > 0 ? 1 : -1), 0, DIST_MODES - 1));
        if (algo) el_post(e, EL_CMD_FX, FX_DIST_DRIVE, (int16_t)el_clampi(fv[FX_DIST_DRIVE] + algo * 2, 0, 127));
        if (select) el_post(e, EL_CMD_FX, FX_DIST_TONE, (int16_t)el_clampi(fv[FX_DIST_TONE] + select * 2, 0, 127));
    }
    int editor_nav = in_editor(ui) && !(hold & (HOLD_GLO | HOLD_EDIT | HOLD_GEN));
    if (select && editor_nav) {
        unsigned n = ed_sections(track_of(ui)->part.engine);
        ui->ed_section[ui->area] = (uint8_t)el_clampi((int)ui->ed_section[ui->area] + select, 0, (int)n - 1);
        ui->ed_page[ui->area] = 0;
    } else if (select && !dist_page) {
        if ((hold & HOLD_GLO) && !ui->overview && ui->area == EL_AREA_BEAT) {
            /* GLO + SELECT: the instrument, PULSE included, without playing it. */
            ui->part = (uint8_t)el_clampi((int)ui->part + select, 0, DR_PARTS - 1);
            ui->page[0] = 0;
            ui->beat_layer = ui->part >= UI_BLACK;
        } else if ((hold & HOLD_GLO) && synth_area(ui) && ui->step_mode[ui->area]) {
            /* GLO + SELECT in STEP mode: the step being edited. */
            const st_pattern_t *p = &track_of(ui)->pattern[track_of(ui)->current];
            ui->cursor[ui->area] = (uint8_t)el_clampi((int)ui->cursor[ui->area] + select, 0, p->length - 1);
            ui->step_page[ui->area] = (uint8_t)(ui->cursor[ui->area] / 16u);
            ui->follow = 0;
        } else {
            /* Slot A-D of this area; in HOME of all four together. */
            for (unsigned a = 0; a < EL_AREAS; ++a)
                if (ui->overview || a == ui->area)
                    select_slot(ui, a, (unsigned)el_clampi((int)area_slot(ui, a, 1) + select, 0, DR_PATTERNS - 1));
        }
    }
    if (algo && editor_nav) {
        unsigned section, page;
        ed_position(ui, &section, &page);
        unsigned pages = ed_pages(ed_section(track_of(ui)->part.engine, section));
        ui->ed_page[ui->area] = (uint8_t)el_clampi((int)page + algo, 0, (int)pages - 1);
    } else if (algo && !dist_page) {
        int bpm = (int)(e->transport.bpm * 10.0f + 0.5f);
        if (ui->overview || !(hold & (HOLD_GLO | HOLD_EDIT))) {
            /* Tempo; in HOME, GLO + ALGORITHM is the fine tempo. */
            el_post(e, EL_CMD_BPM, 0, (int16_t)el_clampi(bpm + algo * ((hold & HOLD_GLO) ? 1 : 10), 300, 3000));
        } else if (ui->area == EL_AREA_BEAT) {
            const dr_pattern_t *p = &m->pattern[m->current];
            if (hold & HOLD_GLO) dr_post(m, DR_CMD_LENGTH, 0, (uint8_t)el_clampi(p->length + algo, 1, DR_STEPS), 0, 0);
            else dr_post(m, DR_CMD_SWING, 0, (uint8_t)el_clampi(p->swing + algo, 50, 75), 0, 0);
        } else {
            const st_pattern_t *p = &track_of(ui)->pattern[track_of(ui)->current];
            uint8_t track = (uint8_t)(ui->area - 1u);
            if (hold & HOLD_GLO)
                el_post3(e, EL_CMD_TRACK_LENGTH, track, (uint8_t)el_clampi(p->length + algo, 1, ST_STEPS), 0, 0);
            else if (ui->area == EL_AREA_STRINGS)   /* step length 1/16 .. 2 bars, for long chord sequences */
                el_post3(e, EL_CMD_TRACK_RATE, track, (uint8_t)el_clampi(p->rate + (algo > 0 ? 1 : -1), 0, ST_RATES - 1), 0, 0);
            else
                el_post3(e, EL_CMD_TRACK_GATE, track, (uint8_t)el_clampi(p->gate + algo * 5, 10, 100), 0, 0);
        }
    }
    if (hold & HOLD_GEN) {
        unsigned a = ui->overview ? 0 : ui->area, styles = a == EL_AREA_BEAT ? DR_STYLES : st_styles(a);
        if (enc[ENC_KNOB1]) ui->gen_style[a] = (uint8_t)el_clampi(ui->gen_style[a] + enc[ENC_KNOB1], 0, (int)styles - 1);
        if (enc[ENC_KNOB1 + 1]) ui->gen_density[a] = (uint8_t)el_clampi(ui->gen_density[a] + enc[ENC_KNOB1 + 1] * 4, 0, 127);
        if (enc[ENC_KNOB1 + 2] && a == EL_AREA_BEAT) ui->gen_keep = enc[ENC_KNOB1 + 2] > 0;
    } else if (synth_area(ui) && ui->step_mode[ui->area] && !ui->editor) {
        uint8_t track = (uint8_t)(ui->area - 1u), s = ui->cursor[ui->area];
        const int16_t *k = enc + ENC_KNOB1;
        if (k[0]) el_post3(e, EL_CMD_STEP_NOTE, track, s, 0, k[0]);
        if (k[1]) el_post3(e, EL_CMD_STEP_NOTE, track, s, 0, (int16_t)(k[1] * 12));
        if (k[2]) {
            if (ui->area == EL_AREA_STRINGS) {
                const st_step_t *st = &track_of(ui)->pattern[track_of(ui)->current].step[s];
                el_post3(e, EL_CMD_STEP_CHORD, track, s, 0, (int16_t)el_clampi((int)ST_CHORD_TYPE(st->chord) + k[2], 0, MKS_CHORDS));
            } else el_post3(e, EL_CMD_STEP_FLAG, track, s, ST_ACCENT, 0);
        }
        if (k[3]) {
            if (ui->area == EL_AREA_STRINGS) {
                const st_step_t *st = &track_of(ui)->pattern[track_of(ui)->current].step[s];
                el_post3(e, EL_CMD_STEP_INV, track, s, 0, (int16_t)el_clampi((int)ST_CHORD_INV(st->chord) + (k[3] > 0 ? 1 : -1), 0, 2));
            } else el_post3(e, EL_CMD_STEP_FLAG, track, s, ST_TIE, 0);
        }
    } else {
        for (unsigned i = 0; i < 4; ++i) edit_knob(ui, i, enc[ENC_KNOB1 + i]);
    }
    if (presets && !dist_page) {
        if (ui->overview) {
            el_post(e, EL_CMD_FX, (hold & HOLD_GLO) ? FX_DELAY_RETURN : FX_REVERB_RETURN,
                    (int16_t)el_clampi(e->fx.v[(hold & HOLD_GLO) ? FX_DELAY_RETURN : FX_REVERB_RETURN] + presets * 2, 0, 127));
        } else if (ui->area == EL_AREA_BEAT) {
            /* The beat's "sound" is its kit, as a synth area's is its sound. */
            dr_post(m, DR_CMD_KIT, (uint8_t)el_clampi((int)m->kit_index + (presets > 0 ? 1 : -1), 0, DR_KITS - 1), 0, 0, 0);
        } else {
            browse_preset(ui, presets, (hold & HOLD_EDIT) != 0);
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
    for (unsigned a = 1; a < EL_AREAS; ++a) mix(&sig[0], area_slot(ui, a, 0) | area_slot(ui, a, 1) << 4 | a << 8);
    mix(&sig[0], (uint32_t)(e->transport.bpm * 10.0f + 0.5f));
    for (unsigned a = 0; a < EL_AREAS; ++a)
        mix(&sig[0], (uint32_t)area_muted(ui, a) | (uint32_t)(a ? e->track[a - 1].recording : m->recording) << 1);
    mix(&sig[1], ui->area | ui->overview << 3 | (uint32_t)playing << 4 | (uint32_t)ui->part << 8);
    mix(&sig[1], ui->options | (uint32_t)ui->opt_item << 1);
    if (ui->options) {
        for (unsigned i = 0; i < EL_AREAS; ++i) mix(&sig[1], e->midi_channel[i]);
        for (unsigned i = 0; i < EL_OPTIONS; ++i) mix(&sig[1], e->option[i] | i << 8);
        mix(&sig[1], (uint32_t)(int)(-e->mix.gain_reduction_db * 6.0f));
    }
    mix(&sig[1], (uint32_t)(playing ? m->pos : 0xff) | (uint32_t)m->current << 8);
    if (ui->overview) {
        mix(&sig[1], m->revision | m->mute << 8);
        for (unsigned a = 0; a < EL_AREAS; ++a) mix(&sig[1], area_slot(ui, a, 0) | area_slot(ui, a, 1) << 4 | a << 8);
        for (unsigned t = 0; t < EL_TRACKS; ++t) {
            mix(&sig[1], e->track[t].revision ^ e->track[t].preset << 20 ^ e->track[t].level_value << 12);
            mix(&sig[1], e->track[t].pos);
        }
        mix(&sig[1], m->level_value);
    } else if (ui->area == EL_AREA_BEAT) {
        const dr_pattern_t *p = &m->pattern[m->current];
        mix(&sig[1], m->revision | (uint32_t)m->mute << 8 | (uint32_t)m->solo << 20);
        mix(&sig[1], shown_step_page(ui, p->length, m->pos, playing) | (uint32_t)p->length << 4 | (uint32_t)p->swing << 12);
    } else if (in_editor(ui)) {
        st_track_t *t = track_of(ui);
        unsigned section, page;
        ed_position(ui, &section, &page);
        mix(&sig[1], 0x5eed0000u | section | page << 4 | (uint32_t)t->part.engine << 8 | (uint32_t)t->preset << 12 | (uint32_t)t->muted << 24);
        mix(&sig[1], ui->sound_save | (uint32_t)ui->sound_slot << 1 | (uint32_t)ui->name_cursor << 8 | (uint32_t)ui->user_cached << 16);
        for (unsigned i = 0; i < EL_SOUND_NAME; ++i) mix(&sig[1], (uint8_t)ui->sound_name[i]);
        const ed_section_t *sec = ed_section(t->part.engine, section);
        for (unsigned i = 0; i < sec->count; ++i) mix(&sig[1], syn_values(&t->part)[sec->param[i]]);
        /* The filter picture follows the LFO: repaint in 1/16 steps of it. */
        if (sec->kind == ED_VCF && ed_vcf_lfo_octaves(&t->part) > 0.0f)
            mix(&sig[1], (uint32_t)(int)((ed_lfo_value(&t->part) + 1.0f) * 8.0f));
    } else {
        st_track_t *t = track_of(ui);
        const st_pattern_t *p = &t->pattern[t->current];
        mix(&sig[1], t->revision ^ (uint32_t)t->preset << 20);
        mix(&sig[1], (uint32_t)(playing ? t->pos : 0xff) | (uint32_t)t->current << 8 | (uint32_t)t->chord << 16);
        mix(&sig[1], shown_step_page(ui, p->length, t->pos, playing) | (uint32_t)p->length << 4 | (uint32_t)p->gate << 12 | (uint32_t)p->rate << 24);
        mix(&sig[1], ui->step_mode[ui->area] | (uint32_t)ui->cursor[ui->area] << 4 | (uint32_t)(ui->octave[ui->area] + 8) << 12);
        mix(&sig[1], t->arp.v[ARP_ON] | (uint32_t)t->arp.v[ARP_HOLD] << 1);
        for (unsigned s = 0; s < p->length; ++s)
            mix(&sig[1], p->step[s].note | (uint32_t)p->step[s].flags << 8 | (uint32_t)p->step[s].chord << 16);
    }
    mix(&sig[2], ui->area | ui->overview << 3 | ui->hold << 4 | (uint32_t)ui->part << 8 | (uint32_t)ui->page[ui->area] << 16 |
                 (uint32_t)ui->editor << 24 | (uint32_t)ui->ed_section[ui->area] << 25);
    mix(&sig[2], ui->ed_page[ui->area] | (uint32_t)ui->sound_save << 8 | (uint32_t)ui->name_cursor << 9 |
                 (uint32_t)ui->options << 20 | (uint32_t)e->option[EL_OPT_HINTS] << 21);
    if (synth_area(ui))
        mix(&sig[2], ui->transpose_mode[ui->area] | (uint32_t)(uint8_t)track_of(ui)->transpose << 8 | (uint32_t)ui->rec_fresh << 16 |
                     (uint32_t)track_of(ui)->recording << 17 | (uint32_t)e->transport.running << 18 |
                     (uint32_t)e->option[EL_OPT_REC_STEP] << 19);
    if (ui->sound_save) mix(&sig[2], (uint8_t)ui->sound_name[ui->name_cursor]);
    mix(&sig[2], ui->engine->fx.v[FX_DIST_MODE] | (uint32_t)ui->engine->fx.v[FX_DIST_TONE] << 8 | (uint32_t)ui->engine->fx.v[FX_DIST_DRIVE] << 16);
    mix(&sig[2], ((ui->notice && (int32_t)(ui->now - ui->notice_until) < 0) ? ui->notice_id : 0) | (uint32_t)ui->ask << 30);
    for (unsigned i = 0; i < 4; ++i) { knob_t k = knob(ui, i); mix(&sig[2], (uint32_t)k.value); }
    unsigned a = ui->overview ? 0 : ui->area;
    mix(&sig[2], ui->gen_style[a] | (uint32_t)ui->gen_density[a] << 8 | (uint32_t)ui->gen_keep << 16);
    if (synth_area(ui))
        mix(&sig[2], ui->step_mode[ui->area] | (uint32_t)track_of(ui)->chord << 4 | (uint32_t)ui->cursor[ui->area] << 8 |
                     (uint32_t)track_of(ui)->arp.v[ARP_HOLD] << 20);
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
    if (synth_area(ui) && track_of(ui)->arp.v[ARP_ON]) leds |= 1ull << BTN_ARP;
    if (ui->overview) leds |= 1ull << BTN_HOME;
    else leds |= 1ull << area_buttons[ui->area];
    if (ui->hold & HOLD_GEN) leds |= 1ull << BTN_ARP;
    if ((synth_area(ui) && ui->step_mode[ui->area] && !ui->editor) || (!ui->overview && ui->area == EL_AREA_BEAT && ui->beat_layer))
        leds |= 1ull << BTN_SEQ;
    if (in_editor(ui)) leds |= 1ull << BTN_EDIT;
    if (ui->sound_save && (ui->now / 250u) & 1u) leds |= 1ull << BTN_SAVE;
    if (ui->overview) {
        /* The slot each area plays; the one it switches to blinks. */
        for (unsigned a = 0; a < EL_AREAS; ++a) {
            unsigned now = area_slot(ui, a, 0), next = area_slot(ui, a, 1);
            leds |= 1ull << (14u + white_keys[a * 4u + now]);
            if (next != now && (ui->now / 250u) & 1u) leds |= 1ull << (14u + white_keys[a * 4u + next]);
        }
        for (unsigned a = 0; a < EL_AREAS; ++a)
            if (!area_muted(ui, a)) leds |= 1ull << (14u + black_keys[a]);
        if (e->punch & EL_PUNCH_STUTTER) leds |= 1ull << (14u + black_keys[4]);
        if (e->punch & EL_PUNCH_THROW) leds |= 1ull << (14u + black_keys[5]);
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
        for (unsigned k = 0; k < UI_BLACK; ++k) {
            int part = beat_part(ui, (int)k);
            if (part < 0) continue;
            int on = (ui->hold & HOLD_GLO) ? !(m->mute & BIT(part)) : part == ui->part;
            if (on) leds |= 1ull << (14u + black_keys[k]);
        }
    } else {
        st_track_t *t = track_of(ui);
        const st_pattern_t *p = &t->pattern[t->current];
        int tplay = playing && t->playing;
        if (ui->step_mode[ui->area] && !ui->editor) {
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
        char slot[2] = {(char)('A' + area_slot(ui, a, 1)), 0};   /* slot it plays (or switches to) */
        gfx_text(b, x + 34, 3, slot, active ? BACK : area_slot(ui, a, 1) != area_slot(ui, a, 0) ? AMBER : color, 1);
        if (muted) gfx_text(b, x + 46, 3, "M", active ? BACK : RED, 1);
        else if (a ? e->track[a - 1].recording : m->recording) gfx_fill(b, x + 48, 4, 6, 6, RED);
    }
    (void)text;
    gfx_text(b, 4, 19, m->pattern[m->next].name, TEXT, 1);   /* the beat's style */
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
        gfx_text(b, 3, y + 1, dr_part_names[part], muted ? FAINT : part == ui->part ? TEXT : DIM, 1);
        if ((m->mute >> part) & 1u) gfx_text(b, 17, y + 1, "M", RED, 1);
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
    gfx_text(b, 4, 35, sound_label(ui, t->preset), color, 2);
    gfx_text(b, 134, 39, syn_engine_names[t->part.engine], DIM, 1);
    if (ui->step_mode[ui->area]) snprintf(text, sizeof text, "STEP%s", p->length > 16 ? (base ? " 2/2" : " 1/2") : "");
    else snprintf(text, sizeof text, "%s O%+d", t->arp.v[ARP_ON] ? (t->arp.v[ARP_HOLD] ? "ARP-H" : "ARP") : "KEYS",
                  ui->octave[ui->area]);
    gfx_text(b, GFX_W - 4 - gfx_text_width(text, 1), 39, text, ui->step_mode[ui->area] || t->arp.v[ARP_ON] ? AMBER : DIM, 1);

    /* Piano roll: the pattern's note range, at least one octave. */
    int lo = 127, hi = 0;
    for (unsigned s = 0; s < p->length; ++s)
        if (p->step[s].flags & ST_ON) {
            int top = p->step[s].note;
            if (ST_CHORD_TYPE(p->step[s].chord)) top += 26;
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
        /* Few long steps (chord sequences) get wide columns with the chord name. */
        int col = shown && shown < 16 ? (16 * CELL) / (int)shown : CELL;
        for (unsigned i = 0; i < shown; ++i) {
            unsigned s = base + i;
            int x = GRID_X + (int)i * col;
            if (s == pos) gfx_fill(b, x, ROLL_Y, col - 2, ROLL_H, 0x2945);
            if (ui->step_mode[ui->area] && s == ui->cursor[ui->area]) gfx_frame(b, x - 1, ROLL_Y, col, ROLL_H, AMBER);
            const st_step_t *st = &p->step[s];
            if (!(st->flags & ST_ON)) continue;
            int16_t notes[MKS_CHORD_NOTES];
            unsigned count = st_chord_notes(st->note, st->chord, notes);
            for (unsigned k = 0; k < count; ++k) {
                int y = ROLL_Y + ROLL_H - 1 - (notes[k] - lo) * ROLL_H / range - 3;
                int w = (st->flags & ST_TIE) ? col : col - 2;
                /* Root bright when accented, chord tones and plain notes dimmer. */
                uint16_t c = k == 0 && (st->flags & ST_ACCENT) ? color : (uint16_t)((color >> 1) & 0x7bef);
                gfx_fill(b, x, y, w, 5, c);
            }
            if (col >= 30 && ST_CHORD_TYPE(st->chord)) {
                char label[12];
                note_name(label, sizeof label, st->note);
                gfx_text(b, x + 3, ROLL_Y + 3, label, color, 1);
                gfx_text(b, x + 3, ROLL_Y + 13, mks_chord_names[ST_CHORD_TYPE(st->chord) - 1], DIM, 1);
            }
        }
    }
    /* Step / chord info line. */
    if (ui->step_mode[ui->area]) {
        const st_step_t *st = &p->step[ui->cursor[ui->area]];
        char n[8];
        note_name(n, sizeof n, st->note);
        unsigned type = ST_CHORD_TYPE(st->chord), inv = ST_CHORD_INV(st->chord);
        static const char *const inv_names[3] = {"", " INV1", " INV2"};
        snprintf(text, sizeof text, "STEP %02u %s %s%s%s%s", ui->cursor[ui->area] + 1u, (st->flags & ST_ON) ? n : "---",
                 type ? mks_chord_names[type - 1] : "", type ? inv_names[inv] : "",
                 (st->flags & ST_ACCENT) ? " ACC" : "", (st->flags & ST_TIE) ? " TIE" : "");
    } else if (ui->area == EL_AREA_STRINGS) {
        /* Long sequences: show the step length and the whole duration. */
        static const uint8_t ticks[ST_RATES] = {1, 2, 4, 8, 16, 32};
        unsigned sixteenths = p->length * ticks[p->rate < ST_RATES ? p->rate : 0];
        snprintf(text, sizeof text, "%u x %s = %u BARS  CHORD %s", p->length, st_rate_names[p->rate < ST_RATES ? p->rate : 0],
                 (sixteenths + 15u) / 16u, t->chord ? mks_chord_names[ST_CHORD_TYPE(t->chord) - 1] : "OFF");
    } else {
        snprintf(text, sizeof text, "LEN %u  GATE %u%%", p->length, p->gate);
    }
    gfx_text(b, 4, 176, text, TEXT, 1);
    /* REC mode, red while recording. */
    int step = rec_step_mode(ui);
    gfx_text(b, step ? 182 : 194, 176, step ? "REC 101" : "REC LIVE", t->recording ? RED : DIM, 1);
}

/* OPTIONS: one line per setting, the chosen one highlighted. */
static void draw_options(el_ui_t *ui, gfx_band_t *b)
{
    if (gfx_touches(b, 34, 10)) {
        gfx_text(b, 4, 34, "OPTIONS", AMBER, 1);
        /* Master compressor gain reduction, 0 .. -12 dB. */
        float gr = ui->engine->mix.enabled ? -ui->engine->mix.gain_reduction_db : 0.0f;
        int w = (int)(dsp_min(gr, 12.0f) * 6.0f);
        gfx_text(b, 100, 34, "COMP GR", DIM, 1);
        gfx_fill(b, 150, 35, 72, 6, GRID);
        if (w > 0) gfx_fill(b, 222 - w, 35, w, 6, AMBER);
    }
    for (unsigned i = 0; i < OPT_ITEMS; ++i) {
        int y = 46 + (int)i * 10;
        if (!gfx_touches(b, y - 1, 10)) continue;
        int on = i == ui->opt_item;
        char value[16];
        opt_text(ui, i, value, sizeof value);
        if (on) gfx_fill(b, 0, y - 1, GFX_W, 10, PANEL);
        uint16_t c = i <= OPT_MIDI_BASS ? area_color[i] : TEXT;
        gfx_text(b, 6, y, opt_names[i], on ? c : DIM, 1);
        gfx_text(b, 150, y, value, on ? ACCENT : TEXT, 1);
    }
}

/* Synth editor: sound and engine, then the section tabs and picture. */
static void draw_editor(el_ui_t *ui, gfx_band_t *b)
{
    st_track_t *t = track_of(ui);
    uint16_t color = t->muted ? DIM : area_color[ui->area];
    unsigned section, page;
    ed_position(ui, &section, &page);
    if (gfx_touches(b, 35, 16)) {
        gfx_text(b, 4, 35, sound_label(ui, t->preset), color, 2);
        const char *engine = syn_engine_names[t->part.engine];
        gfx_text(b, GFX_W - 4 - gfx_text_width(engine, 1), 39, engine, AMBER, 1);
    }
    if (!ui->sound_save) {
        ed_draw(b, &t->part, section, page, color, 56, FOOT_Y - 6 - 56);
        return;
    }
    /* Storing the sound: slot, what it replaces, the name with its cursor. */
    char text[32];
    user_cache(ui);
    unsigned slot = ui->sound_slot;
    if (gfx_touches(b, 60, 10)) gfx_text(b, 4, 60, "SAVE SOUND TO USER SLOT", AMBER, 1);
    snprintf(text, sizeof text, "U%02u", slot + 1u);
    if (gfx_touches(b, 78, 24)) gfx_text(b, 4, 78, text, color, 3);
    if (ui->user_used[slot]) snprintf(text, sizeof text, "REPLACES %s", ui->user_name[slot]);
    else snprintf(text, sizeof text, "EMPTY");
    if (gfx_touches(b, 108, 10)) gfx_text(b, 4, 108, text, ui->user_used[slot] ? RED : DIM, 1);
    if (gfx_touches(b, 128, 22)) {
        gfx_fill(b, 2, 126, GFX_W - 4, 22, PANEL);
        gfx_text(b, 6, 130, ui->sound_name, TEXT, 2);
        gfx_fill(b, 6 + (int)ui->name_cursor * 12, 147, 10, 2, AMBER);
    }
    if (gfx_touches(b, 160, 10)) gfx_text(b, 4, 160, syn_engine_names[t->part.engine], DIM, 1);
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
        const char *sound = a ? sound_label(ui, e->track[a - 1].preset) : dr_kit_names[m->kit_index];
        unsigned level = a ? e->track[a - 1].level_value : m->level_value;
        gfx_text(b, 4, y + 2, area_names[a], color, 1);
        char slot[4] = {(char)('A' + area_slot(ui, a, 0)), 0, 0, 0};
        if (area_slot(ui, a, 1) != area_slot(ui, a, 0)) { slot[1] = '>'; slot[2] = (char)('A' + area_slot(ui, a, 1)); }
        gfx_text(b, 52, y + 2, slot, AMBER, 1);
        gfx_text(b, 74, y + 2, sound, muted ? DIM : TEXT, 1);
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
    gfx_text(b, 4, 180, "WHITE: SLOTS  BLACK 5 STUTTER  6 HALL", DIM, 1);
}

static void draw_footer(el_ui_t *ui, gfx_band_t *b)
{
    char value[24];
    gfx_fill(b, 0, FOOT_Y - 4, GFX_W, GFX_H - FOOT_Y + 4, PANEL);
    if (ui->options) {
        gfx_text(b, 4, FOOT_Y, "SELECT / ALGO: SETTING", DIM, 1);
        gfx_text(b, 4, FOOT_Y + 12, "PRESETS / KNOBS: VALUE", DIM, 1);
        gfx_text(b, 4, GFX_H - 12, "HOME OR SAVE: DONE", AMBER, 1);
        return;
    }
    unsigned a = ui->overview ? 0 : ui->area;
    if (ui->hold & HOLD_GEN) {
        int beat = a == EL_AREA_BEAT;
        gfx_text(b, 4, FOOT_Y, "STYLE", DIM, 1);
        gfx_text(b, 4, FOOT_Y + 12, beat ? dr_styles[ui->gen_style[a]].name : st_style_name(a, ui->gen_style[a]), ACCENT, 1);
        gfx_text(b, 64, FOOT_Y, "AMOUNT", DIM, 1);
        snprintf(value, sizeof value, "%u", ui->gen_density[a]);
        gfx_text(b, 64, FOOT_Y + 12, value, ACCENT, 2);
        if (beat) {
            gfx_text(b, 124, FOOT_Y, "KICK", DIM, 1);
            gfx_text(b, 124, FOOT_Y + 12, ui->gen_keep ? "KEEP" : "NEW", ACCENT, 2);
        }
        int asking = ui->ask && (int32_t)(ui->now - ui->ask_until) < 0 && ui->notice;
        gfx_text(b, 4, GFX_H - 12, asking ? ui->notice : ui->overview ? "ALL AREAS: KEY 1 VARY, KEY 2 NEW" : "KEY 1 VARIATION, KEY 2 NEW", AMBER, 1);
        return;
    }
    if (in_editor(ui) && ui->sound_save) {
        static const char *const labels[4] = {"CURSOR", "LETTER", "", ""};
        for (unsigned i = 0; i < 2; ++i) gfx_text(b, 4 + (int)i * 59, FOOT_Y, labels[i], DIM, 1);
        gfx_text(b, 180, FOOT_Y, "PRESETS", DIM, 1);
        gfx_text(b, 180, FOOT_Y + 12, "SLOT", area_color[ui->area], 1);
        char pos[8];
        snprintf(pos, sizeof pos, "%u", ui->name_cursor + 1u);
        gfx_text(b, 4, FOOT_Y + 12, pos, area_color[ui->area], 2);
        char letter[2] = {ui->sound_name[ui->name_cursor] == ' ' ? '_' : ui->sound_name[ui->name_cursor], 0};
        gfx_text(b, 63, FOOT_Y + 12, letter, area_color[ui->area], 2);
    } else if (synth_area(ui) && ui->step_mode[ui->area] && !ui->editor) {
        static const char *const labels[4] = {"NOTE", "OCTAVE", "CHORD", "INVERS"};
        static const char *const other[4] = {"NOTE", "OCTAVE", "ACCENT", "TIE"};
        for (unsigned i = 0; i < 4; ++i)
            gfx_text(b, 4 + (int)i * 59, FOOT_Y, ui->area == EL_AREA_STRINGS ? labels[i] : other[i], DIM, 1);
        gfx_text(b, 4, FOOT_Y + 12, "KNOB 1-4 EDIT THE STEP", area_color[ui->area], 1);
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
    if (!hint && synth_area(ui) && !ui->overview && !in_editor(ui) && track_of(ui)->recording && rec_step_mode(ui)) {
        snprintf(line, sizeof line, "STEP REC %u/32  EDIT=REST GLO=TIE", ui->rec_fresh ? 0u : ui->cursor[ui->area]);
        hint = line;
    } else if (!hint && synth_area(ui) && !ui->overview && ui->transpose_mode[ui->area]) {
        snprintf(line, sizeof line, "TRANSPOSE %+d  C4=0  GLO+SEQ=OFF", track_of(ui)->transpose);
        hint = line;
    }
    if (!hint) {
        if (ui->overview && (ui->hold & HOLD_EDIT)) {
            const uint8_t *fv = ui->engine->fx.v;
            snprintf(line, sizeof line, "PRESETS %s ALGO DRV %u SEL TONE %u", fx_dist_mode_name(fv[FX_DIST_MODE]),
                     fv[FX_DIST_DRIVE], fv[FX_DIST_TONE]);
            hint = line;
        } else if (ui->overview) hint = (ui->hold & HOLD_GLO) ? "GLO: FX  PRESETS=DELAY ALGO=FINE BPM" : "LEVELS PRESETS=REV GLO=FX EDIT=DIST";
        else if (ui->area == EL_AREA_BEAT) {
            const dr_pattern_t *p = &ui->engine->drums.pattern[ui->engine->drums.current];
            unsigned pages = ((unsigned)tr808_nparams(ui->part) + 3u) / 4u;
            snprintf(line, sizeof line, "%s %u/%u KIT %s SW%u L%u%s", dr_part_names[ui->part], ui->page[0] + 1u, pages,
                     dr_kit_names[ui->engine->drums.kit_index], p->swing, p->length,
                     p->length > 16 ? (ui->step_page[0] ? " 2/2" : " 1/2") : "");
            hint = (ui->hold & HOLD_EDIT) ? "EDIT: ACCENT/CLEAR  ALGO=SWING" :
                   (ui->hold & HOLD_GLO) ? "GLO: ROLL/MUTE SEL=PART ALGO=LENGTH" : line;
        } else if (in_editor(ui) && ui->sound_save) {
            hint = "SAVE=STORE  OTHER BUTTON=CANCEL";
        } else if (in_editor(ui)) {
            hint = (ui->hold & HOLD_EDIT) ? "EDIT: PRESETS=OTHER ENGINE  AREA=BACK" : "SEL=SECTION ALGO=PAGE SAVE=SOUND";
        } else {
            unsigned page = ui->page[ui->area];
            const st_track_t *t = track_of(ui);
            if (page == PAGE_ARP)
                snprintf(line, sizeof line, "ARP %s%s  TAP ARP  EDIT+ARP=HOLD", t->arp.v[ARP_ON] ? "ON" : "OFF",
                         t->arp.v[ARP_HOLD] ? " HOLD" : "");
            else
                snprintf(line, sizeof line, "%s %u/%u  PRESETS=SOUND SEQ=STEP", synth_page_names[page], page + 1u, SYNTH_PAGES);
            hint = (ui->hold & HOLD_GLO) ? (ui->step_mode[ui->area] ? "GLO: KEY=TIE SEL=STEP ALGO=LENGTH" :
                                            ui->area == EL_AREA_STRINGS ? "GLO: KEY 1-9=CHORD ALGO=LENGTH" : "GLO: ALGO=PATTERN LENGTH") :
                   (ui->hold & HOLD_EDIT) ? (ui->area == EL_AREA_STRINGS ? "EDIT: ACC/REST ALGO=STEP LEN" : "EDIT: ACC/REST ALGO=GATE") : line;
        }
    }
    if (hint == ui->notice || ui->engine->option[EL_OPT_HINTS]) gfx_text(b, 4, GFX_H - 12, hint, hint == ui->notice ? AMBER : DIM, 1);
}

void el_ui_draw_band(el_ui_t *ui, unsigned band)
{
    gfx_band_t *b = &ui->band;
    gfx_begin(b, band, BACK);
    if (gfx_touches(b, 0, HEADER_H)) draw_header(ui, b);
    if (gfx_touches(b, HEADER_H, FOOT_Y - 4 - HEADER_H)) {
        if (ui->options) draw_options(ui, b);
        else if (ui->overview) draw_overview(ui, b);
        else if (ui->area == EL_AREA_BEAT) draw_beat(ui, b);
        else if (in_editor(ui)) draw_editor(ui, b);
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
    run_jobs(ui);
    mark_dirty(ui);
    update_leds(ui);
    paint(ui);
}
