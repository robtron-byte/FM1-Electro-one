/* SPDX-License-Identifier: GPL-3.0-only */
/* Bedienung und Bild des Gerüsts (ADR 0044), Teil der Einheit gm_app.c. Aus fm1-synths mks50/app/mk_ui.c (Commit 2a7b4fa,
 * dort ADR 0008), ohne die Teile, die nur die MKS-50 kennt (Akkorde, Sets, Bilder der MKS-Parameter).
 *
 * Eine Regel für das ganze Gerät: KNOB 1–4 die vier Felder unten, SELECT Seite, PRESETS Sound, ALGORITHM Part, eine
 * Taste ihre erste Seite (noch einmal: ihre nächste), gehalten ihre vier wichtigsten Werte. Bildaufbau überall gleich:
 * oben die Parts mit Sound, in der Mitte das Bild der Seite, unten vier Felder, ganz unten Seite und Oktave. */

/* ---- Zeichnen in die Streifen der Plattform, verschoben um (g_ox) für das Gleiten der Seiten */
static int32_t g_ox;
static void R(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x + g_ox, y, w, h, c); }
static void LN(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { cv_line(x0 + g_ox, y0, x1 + g_ox, y1, c); }
static void FRAME(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    R(x, y, w, 1, c); R(x, y + h - 1, w, 1, c); R(x, y, 1, h, c); R(x + w - 1, y, 1, h, c);
}
static int32_t P_W(const char *t, int32_t s) { int32_t n = (int32_t)str_len(t); return n ? n * 6 * s - s : 0; }
static void P(int32_t x, int32_t y, int32_t s, const char *t, uint16_t c)
{
    for (; *t; t++, x += 6 * s) {
        uint32_t ch = (uint8_t)*t;
        if (ch >= 'a' && ch <= 'z') ch -= 32u;
        if (ch < 32u || ch > 95u) ch = '?';
        const uint8_t *g = PX57[ch - 32u];
        if (y + 7 * s + cv_oy <= 0 || y + cv_oy >= (int32_t)cv_h) continue;
        for (int32_t col = 0; col < 5; col++)
            for (int32_t row = 0; row < 7; row++)
                if (g[col] >> row & 1u) R(x + col * s, y + row * s, s, s, c);
    }
}
static int32_t Q_W(const char *t, int32_t s) { int32_t n = (int32_t)str_len(t); return n ? n * 4 * s - s : 0; }
static void Q(int32_t x, int32_t y, int32_t s, const char *t, uint16_t c)
{
    for (; *t; t++, x += 4 * s) {
        uint32_t ch = (uint8_t)*t;
        if (ch >= 'a' && ch <= 'z') ch -= 32u;
        if (ch < 32u || ch > 95u) ch = '?';
        const uint8_t *g = PX35[ch - 32u];
        if (y + 5 * s + cv_oy <= 0 || y + cv_oy >= (int32_t)cv_h) continue;
        for (int32_t row = 0; row < 5; row++)
            for (int32_t col = 0; col < 3; col++)
                if (g[row] >> (2 - col) & 1u) R(x + col * s, y + row * s, s, s, c);
    }
}
static void PC(int32_t cx, int32_t y, int32_t s, const char *t, uint16_t c) { P(cx - P_W(t, s) / 2, y, s, t, c); }
static void QC(int32_t cx, int32_t y, int32_t s, const char *t, uint16_t c) { Q(cx - Q_W(t, s) / 2, y, s, t, c); }
static void QR(int32_t xr, int32_t y, int32_t s, const char *t, uint16_t c) { Q(xr - Q_W(t, s), y, s, t, c); }

/* ---- Seiten (Tabelle der Variante: gm_pages, gm_layers) */
enum { NPAGE = sizeof gm_pages / sizeof gm_pages[0] };
#define pages gm_pages
#define layers gm_layers
static uint8_t layer, layer_used;
static uint32_t layer_t0;
static int layer_shown(void) { return layer && (layer_used || fm1_ms - layer_t0 > 220u); }
static uint8_t page, sel, saving;
static int8_t oct;
static uint8_t touched = 255;
static uint32_t t_touch;
static char msg[32];
static uint32_t msg_t0, msg_until;
static uint8_t msg_warn;
static void msg_set(const char *s, int warn)
{
    str_cpy(msg, s, sizeof msg);
    msg_t0 = fm1_ms; msg_until = fm1_ms + 2200u; msg_warn = (uint8_t)warn;
}
static void touch_set(void) { t_set = fm1_ms; }
static const kn_t *cells_now(void) { return layer_shown() || (layer && layer_used) ? layers[layer].k : pages[page].k; }
static uint8_t tr_from;
static int8_t tr_dir;
static uint32_t tr_t0;
#define TR_MS 150u
static uint8_t grp_last[NGROUP];
static void goto_page(unsigned p, int dir)
{
    if (p == page || p >= NPAGE) return;
    grp_last[pages[p].group] = (uint8_t)(p + 1u);
    tr_from = page; tr_dir = (int8_t)(dir ? dir : p > page ? 1 : -1); tr_t0 = fm1_ms;
    page = (uint8_t)p;
    touched = 255;
}
static void group_button(unsigned g)
{
    unsigned first = NPAGE, next = NPAGE;
    for (unsigned i = 0; i < NPAGE; i++) if (pages[i].group == g) { if (first == NPAGE) first = i; if (i > page && next == NPAGE) next = i; }
    if (first == NPAGE) return;
    if (pages[page].group != g) {
        unsigned last = grp_last[g] ? grp_last[g] - 1u : first;
        goto_page(last < NPAGE && pages[last].group == g ? last : first, 0);
    } else goto_page(next < NPAGE ? next : first, 1);
}

/* ---- Werte der Reglerfelder */
static const uint8_t arp_max[ARP_N] = {1, 4, FX_DIVS - 1, 3, 127, 1};
static const uint8_t sq_max[SQ_N] = {1, FX_DIVS - 1, 127, 48};
static const char *const arp_modes[5] = {"UP", "DOWN", "UP/DN", "RAND", "ORDER"};
static const char *const div_names[FX_DIVS] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T", "1/4", "1/4.", "1/2", "1/1"};
static uint16_t k_colour(const kn_t *k)
{
    if (k->kind == K_PARAM) return gm_class_colour[gm_params[k->param].cls % 5u];
    if (k->kind == K_FX || k->kind == K_TEMPO || k->kind == K_CLK || k->kind == K_SEQ || k->kind == K_SEND) return T_VIOLET;
    return T_SEL;
}
static int k_max(const kn_t *k)
{
    switch (k->kind) {
    case K_PARAM: return gm_params[k->param].max;
    case K_LEVEL: case K_SEND: return 127;
    case K_CHAN: return 16;
    case K_FX: return syn_fx_max(k->param);
    case K_ARP: return arp_max[k->param % ARP_N];
    case K_TEMPO: return 200;
    case K_CLK: return k->param ? 1 : 2;
    case K_SEQ: return sq_max[k->param % SQ_N];
    default: return 0;
    }
}
static int k_get(const kn_t *k)
{
    switch (k->kind) {
    case K_PARAM: return up[sel].v[k->param];
    case K_LEVEL: return up[k->param < GM_PARTS ? k->param : sel].level;
    case K_CHAN: return up[sel].chan;
    case K_FX: return fxv[k->param];
    case K_ARP: return arpc[sel][k->param % ARP_N];
    case K_TEMPO: return tempo;
    case K_CLK: return clkv[k->param & 1u];
    case K_SEQ: return sq.cfg[sel][k->param % SQ_N];
    case K_SEND: return sendv[sel][k->param & 1u];
    default: return 0;
    }
}
static void k_text(const kn_t *k, int v, char *b)
{
    switch (k->kind) {
    case K_PARAM:
        if (gm_params[k->param].text) gm_params[k->param].text((uint8_t)v, b);
        else fmt_int(b, v);
        return;
    case K_CHAN: if (v) fmt_int(b, v); else str_cpy(b, "OMNI", 8); return;
    case K_FX: { const char *n = syn_fx_value_name(k->param, v); if (n) str_cpy(b, n, 8); else fmt_int(b, v); return; }
    case K_ARP:
        if (k->param == ARP_ON || k->param == ARP_HOLD) str_cpy(b, v ? "ON" : "OFF", 8);
        else if (k->param == ARP_MODE) str_cpy(b, arp_modes[v % 5], 8);
        else if (k->param == ARP_DIV) str_cpy(b, div_names[v % FX_DIVS], 8);
        else if (k->param == ARP_OCT) fmt_int(b, v + 1);
        else fmt_int(b, 5 + v * 95 / 127);
        return;
    case K_TEMPO: fmt_int(b, v + 40); return;
    case K_CLK:
        if (k->param) str_cpy(b, v ? "ON" : "OFF", 8);
        else str_cpy(b, v == 0 ? "AUTO" : v == 1 ? "INT" : "MIDI", 8);
        return;
    case K_SEQ:
        if (k->param == SQ_ON) str_cpy(b, v ? "ON" : "OFF", 8);
        else if (k->param == SQ_DIV) str_cpy(b, div_names[v % FX_DIVS], 8);
        else if (k->param == SQ_SHIFT) { if (v >= 24) { b[0] = '+'; fmt_int(b + 1, v - 24); } else fmt_int(b, v - 24); }
        else fmt_int(b, 5 + v * 95 / 127);
        return;
    default: fmt_int(b, v); return;
    }
}
static void k_turn(const kn_t *k, int32_t d)
{
    int v = k_get(k) + d, mx = k_max(k);
    v = v < 0 ? 0 : v > mx ? mx : v;
    if (v == k_get(k)) return;
    switch (k->kind) {
    case K_PARAM:
        up[sel].v[k->param] = (uint8_t)v;
        ev_put(EV_SET, sel, k->param, (uint32_t)v);
        mark_dirty(sel);
        break;
    case K_LEVEL: { unsigned p = k->param < GM_PARTS ? k->param : sel; up[p].level = (uint8_t)v; ev_put(EV_LEVEL, p, (uint32_t)v, 0); break; }
    case K_CHAN: up[sel].chan = (uint8_t)v; part_ch[sel] = (uint8_t)v; break;
    case K_FX:
        fxv[k->param] = (uint8_t)v; ev_put(EV_FX, 0, k->param, (uint32_t)v);
        fx_on = fxv[FX_DLY_LEVEL] || fxv[FX_REV_LEVEL];
        break;
    case K_ARP: arpc[sel][k->param % ARP_N] = (uint8_t)v; ev_put(EV_ARP, sel, k->param, (uint32_t)v); break;
    case K_TEMPO: tempo = (uint8_t)v; ev_put(EV_TEMPO, 0, (uint32_t)v, 0); break;
    case K_CLK: clkv[k->param & 1u] = (uint8_t)v; ev_put(EV_CLK, 0, k->param & 1u, (uint32_t)v); break;
    case K_SEQ: ev_put(EV_SEQ, sel, k->param, (uint32_t)v); break;   /* (die Audioseite schreibt sq.cfg) */
    case K_SEND: sendv[sel][k->param & 1u] = (uint8_t)v; ev_put(EV_SEND, sel, k->param & 1u, (uint32_t)v); break;
    default: break;
    }
    touch_set();
}
/* Reglerbeschleunigung: der Schritt wächst mit den Rasten je Sekunde (nur bei mehr als 16 Stufen) */
static uint32_t enc_t[8];
static int32_t enc_fast(unsigned id, int32_t e, int range, uint32_t now)
{
    uint32_t dt = now - enc_t[id], n = (uint32_t)(e < 0 ? -e : e), rate, f;
    enc_t[id] = now;
    if (range <= 16 || dt > 250u) return e;
    if (dt < 4u) dt = 4u;
    rate = n * 1000u / dt;
    f = rate < 12u ? 1u : rate < 22u ? 2u : rate < 34u ? 4u : rate < 50u ? 7u : 12u;
    return e * (int32_t)f;
}

/* ---- Speichern: SAVE kurz auf den eigenen Platz des Sounds, lang (oder ein Werkssound) mit Dialog */
static const char CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+.";
static struct { uint8_t cursor, cat, slot; char name[GM_NAMEMAX + 1]; } dlg;
static int own_free(void) { for (unsigned j = 0; j < NOWN; j++) if (!own[j].used) return (int)j; return -1; }
static void own_write(unsigned slot, unsigned p, const char *name, unsigned cat)
{
    own_t *o = &own[slot];
    str_cpy(o->name, name, sizeof o->name);
    o->cat = (uint8_t)cat; o->used = 1;
    memcpy(o->v, up[p].v, GM_NP);
    own_flash();
    up[p].kind = 1; up[p].idx = (uint8_t)slot; up[p].cat = (uint8_t)cat;
    str_cpy(up[p].name, name, sizeof up[p].name);
    memcpy(up[p].base, up[p].v, GM_NP);
    up[p].dirty = 0;
    touch_set();
}
static void dlg_open(unsigned p)
{
    int f = up[p].kind ? up[p].idx : own_free();
    unsigned n = str_len(up[p].name);
    memset(dlg.name, ' ', GM_NAMEMAX); dlg.name[GM_NAMEMAX] = 0;
    memcpy(dlg.name, up[p].name, n > GM_NAMEMAX ? GM_NAMEMAX : n);
    dlg.cursor = 0; dlg.cat = (uint8_t)(up[p].cat < GM_NCAT ? up[p].cat : 0); dlg.slot = (uint8_t)(f < 0 ? 0 : f);
    saving = 1;
}
static void save_short(unsigned p)
{
    if (up[p].kind == 1u && own[up[p].idx].used) { own_write(up[p].idx, p, up[p].name, up[p].cat); msg_set("SAVED", 0); }
    else dlg_open(p);
}
static void dlg_ok(unsigned p)
{
    char n[GM_NAMEMAX + 1];
    int e = GM_NAMEMAX;
    str_cpy(n, dlg.name, sizeof n);
    while (e > 0 && n[e - 1] == ' ') n[--e] = 0;
    if (!n[0]) { msg_set("A NAME, PLEASE", 1); return; }
    own_write(dlg.slot, p, n, dlg.cat);
    saving = 0;
    msg_set("SAVED", 0);
}

/* ---- Last für die Seite SYSTEM */
static uint32_t avg[M_N], peak[M_N], max_all, late_blocks;
static uint32_t pct(uint32_t ticks) { return (ticks * 100u + BLOCK_TICKS / 2u) / BLOCK_TICKS; }

/* ---- Bild */
static int32_t shown_q8[4];
static uint32_t frame_no;
static int8_t scope_shot[240];
static uint8_t voices_now[GM_PARTS];
static uint8_t midi_seen[GM_PARTS];
static uint32_t midi_t[GM_PARTS];
static void draw_header(void)
{
    const int32_t w = 240 / GM_PARTS;
    for (unsigned i = 0; i < GM_PARTS; i++) {
        int32_t x = (int32_t)i * w;
        uint16_t fg = i == sel ? T_BG : T_DIM;
        if (i == sel) R(x + 1, 1, w - 2, 21, GM_ACCENT); else FRAME(x + 1, 1, w - 2, 21, T_LINE);
        if (GM_PARTS > 1) { char s[2] = {(char)('A' + i), 0}; P(x + 5, 4, 2, s, fg); Q(x + 20, 8, 1, up[i].name, fg); }
        else { Q(x + 6, 6, 2, GM_TITLE, fg); QR(x + w - 14, 6, 2, up[i].name, fg); }
        if (up[i].dirty) R(x + w - 9, 4, 4, 4, i == sel ? T_BG : T_WARN);
        if (fm1_ms - midi_t[i] < 150u && midi_t[i]) R(x + w - 9, 14, 5, 5, i == sel ? T_BG : T_LIVE);
        for (unsigned k = 0; k < voices_now[i] && k < 6u; k++) R(x + 3 + (int32_t)k * 9, 24, 7, 2, T_LIVE);
    }
}
static void draw_sound_line(int32_t y)
{
    if (up[sel].dirty) { R(180, y + 2, 6, 6, T_WARN); QR(238, y, 2, "EDITED", T_WARN); }
}
static void stage_name(const page_t *pg) { draw_sound_line(32); PC(120, 76, 4, pg->title, T_LINE); }
static void stage_home(void)
{
    PC(120, 40, 3, up[sel].name, T_INK);
    {
        char b[24], *p = put(b, cat_label(up[sel].cat));
        if (up[sel].kind) p = put(p, "  USER");
        *p = 0;
        QC(120, 68, 2, b, T_DIM);
    }
    for (int32_t x = 0; x < 239; x++) {
        int32_t y0 = 116 - scope_shot[x] / 4, y1 = 116 - scope_shot[x + 1] / 4;
        LN(x, y0, x + 1, y1, T_LIVE);
    }
    draw_sound_line(84);
}
static void stage_sys(void)
{
    char b[40], *p;
    static const char *const nm[3] = {"TOTAL ", "CORE 0", "CORE 1"};
    p = put(b, "V" APP_VER); while (p < b + 19) *p++ = ' ';
    p = put(p, "AVG  PEAK"); *p = 0;
    Q(4, 34, 2, b, T_DIM);
    for (unsigned i = 0; i < 3u; i++) {
        uint32_t pk = pct(peak[i]);
        p = put(b, nm[i]); p = put_n(p, pct(avg[i]), 6); p = put(p, "%"); p = put_n(p, pk, 5); p = put(p, "%"); *p = 0;
        P(4, 52 + (int32_t)i * 20, 2, b, pk >= 88u ? T_WARN : T_INK);
    }
    p = put(b, "SOUND"); p = put_n(p, pct(avg[M_VOICE]), 4); p = put(p, "%  DLY+REV"); p = put_n(p, pct(avg[M_FX]), 3); *put(p, "%") = 0;
    Q(4, 116, 2, b, T_DIM);
    p = put(b, "MAX"); p = put_n(p, pct(max_all), 4); p = put(p, "% LATE "); p = put_n(p, late_blocks, 0);
    p = put(p, " GAP "); p = put_n(p, gaps, 0);
    p = put(p, cpu1_ok ? "  2K" : "  1K!"); *p = 0;
    Q(4, 132, 2, b, late_blocks || gaps || !cpu1_ok ? T_WARN : T_DIM);
}
static void stage_browse(void)
{
    upart_t *u = &up[sel];
    int cnt = (int)cat_count(u->cat_view), pos = cat_pos(u->cat_view, u->kind, u->idx), first;
    QR(236, 30, 2, cat_label(u->cat_view), T_DIM);
    if (pos < 0) pos = 0;
    first = pos - 2; if (first > cnt - 5) first = cnt - 5; if (first < 0) first = 0;
    for (int i = 0; i < 5 && first + i < cnt; i++) {
        uint8_t k, x;
        int32_t y = 40 + i * 22;
        char n[4], *q = n;
        const char *name;
        if (cat_item(u->cat_view, (unsigned)(first + i), &k, &x)) break;
        name = k ? own[x].name : fac_name(x);
        if (first + i + 1 < 10) *q++ = '0';
        fmt_int(q, first + i + 1);
        if (first + i == pos) { R(0, y, 240, 20, T_SEL); Q(6, y + 5, 2, n, T_BG); P(34, y + 3, 2, name, T_BG); }
        else { Q(6, y + 5, 2, n, T_DIM); P(34, y + 3, 2, name, T_INK); }
    }
}
static void stage_arp(void)
{
    unsigned n = arp_view_n[sel], at = arp_view_at[sel];
    draw_sound_line(32);
    if (!arpc[sel][ARP_ON]) { PC(120, 76, 4, "ARP", T_LINE); return; }
    for (unsigned i = 0; i < n && i < 24u; i++) {
        int32_t x = 6 + (int32_t)i * 9, h = 10 + (arp_view[sel][i] % 24) * 3;
        R(x, 140 - h, 7, h, i == at ? T_SEL : T_LINE);
    }
}
static uint8_t rec_on, rec_part, rec_cur, rec_key = 0xff, rec_note = 0xff;
static void stage_seq(void)
{
    unsigned n = rec_on && rec_part == sel ? rec_cur : sq.len[sel], at = sq_view_at[sel];
    char b[24], *p;
    p = put(b, rec_on ? "REC STEP " : "STEPS "); fmt_int(p, (int32_t)(rec_on ? rec_cur + 1u : n));
    Q(4, 32, 2, b, rec_on ? T_WARN : T_DIM);
    int32_t lo = 127, hi = 0;                   /* Höhe der Balken: die Tonhöhe innerhalb des Umfangs der Folge */
    for (unsigned i = 0; i < n && i < SEQ_STEPS; i++) if (!(sq.flag[sel][i] & SQ_REST)) {
        if (sq.note[sel][i] < lo) lo = sq.note[sel][i];
        if (sq.note[sel][i] > hi) hi = sq.note[sel][i];
    }
    for (unsigned i = 0; i < n && i < SEQ_STEPS; i++) {
        int32_t x = 4 + (int32_t)(i % 16u) * 15, y = 50 + (int32_t)(i / 16u) * 24;
        uint8_t f = sq.flag[sel][i];
        uint16_t c = (sq.cfg[sel][SQ_ON] && i == at) ? T_SEL : (f & SQ_REST) ? T_LINE : T_VIOLET;
        if (f & SQ_REST) R(x, y + 16, 12, 2, c);
        else { int32_t h = 4 + (hi > lo ? (sq.note[sel][i] - lo) * 16 / (hi - lo) : 8); R(x, y + 20 - h, 12, h, c); if (f & SQ_TIE) R(x + 12, y + 14, 3, 2, c); }
    }
}
static void stage_tempo(void)
{
    char b[12];
    fmt_int(b, (int32_t)(bpm_now + 0.5f));
    PC(110, 50, 6, b, T_INK);
    Q(4, 108, 2, ext_follow ? "MIDI CLOCK" : "INTERNAL", T_DIM);
    QR(236, 108, 2, xport == 1 ? "RUNNING" : xport == 2 ? "STOPPED" : "", T_DIM);
    if (xport == 1 && clk_ticks % 24u < 6u) R(220, 54, 12, 12, T_VIOLET);
}

/* ---- die Reglerfelder */
static void cell(unsigned k, const char *label, const char *value, int32_t bar_q8, int bar, uint16_t col)
{
    int32_t x = (int32_t)k * 60, cx = x + 30;
    int hot = touched == k && fm1_ms - t_touch < 1200u;
    if (!label[0]) { R(x + 26, 190, 8, 2, T_LINE); return; }
    {
        int32_t ls = str_len(label) > 7u ? 1 : 2;
        if (hot) { R(x + 2, 158, 56, 14, col); QC(cx, 160 + (2 - ls) * 2, ls, label, T_BG); }
        else QC(cx, 160 + (2 - ls) * 2, ls, label, T_DIM);
    }
    if (str_len(value) <= 4u) PC(cx, 180, 2, value, T_INK);
    else if (str_len(value) <= 7u) QC(cx, 182, 2, value, T_INK);
    else QC(cx, 184, 1, value, T_INK);
    if (bar) {
        R(x + 4, 202, 52, 5, T_LINE);
        R(x + 4, 202, bar_q8 * 52 / (256 * 256), 5, col);
    }
}
static void draw_cells(const kn_t *cells, int live)
{
    for (unsigned k = 0; k < 4u; k++) {
        const kn_t *kn = &cells[k];
        char b[12];
        int v = k_get(kn), mx = k_max(kn);
        if (kn->kind == K_NONE) { cell(k, "", "", 0, 0, T_SEL); continue; }
        k_text(kn, v, b);
        cell(k, kn->label, b, live ? shown_q8[k] : (mx ? v * 65536 / mx : 0), mx > 4, k_colour(kn));
    }
}
static void draw_save_cells(void)
{
    char b[12], s[2] = {dlg.name[dlg.cursor], 0};
    fmt_int(b, dlg.cursor + 1);
    cell(0, "CURSOR", b, 0, 0, T_SEL);
    cell(1, "LETTER", s[0] == ' ' ? "_" : s, 0, 0, T_SEL);
    cell(2, "GROUP", cat_label(dlg.cat), 0, 0, T_SEL);
    { char *p = b; if (dlg.slot + 1u < 10u) *p++ = '0'; fmt_int(p, dlg.slot + 1); }
    cell(3, "PLACE", b, 0, 0, T_SEL);
}
static void stage_save(void)
{
    char s[2] = {0, 0};
    for (unsigned i = 0; i < GM_NAMEMAX; i++) {
        int32_t x = 2 + (int32_t)i * 24;
        s[0] = dlg.name[i];
        if (i == dlg.cursor) { R(x - 1, 50, 22, 34, T_SEL); P(x, 53, 4, s, T_BG); }
        else { P(x, 53, 4, s, T_INK); if (s[0] == ' ') R(x, 82, 20, 2, T_LINE); }
    }
    Q(4, 34, 2, "NAME", T_DIM);
    Q(4, 104, 2, own[dlg.slot].used ? "THIS PLACE IS TAKEN: SAVE REPLACES IT" : "THIS PLACE IS FREE", own[dlg.slot].used ? T_WARN : T_DIM);
    Q(4, 124, 2, "SAVE = OK       HOME = CANCEL", T_DIM);
}
static void draw_page(unsigned pi, int live)
{
    const page_t *pg = &pages[pi];
    switch (pg->stage) {
    case ST_HOME: stage_home(); break;
    case ST_SYS: stage_sys(); break;
    case ST_BROWSE: stage_browse(); break;
    case ST_ARP: stage_arp(); break;
    case ST_SEQ: stage_seq(); break;
    case ST_TEMPO: stage_tempo(); break;
    default: if (pg->stage < ST_USER || !kern_stage(pg->stage, sel)) stage_name(pg); break;
    }
    if (live && layer_shown()) {
        R(0, 153, 240, 2, T_ACC); R(0, 212, 240, 2, T_ACC);
        draw_cells(layers[layer].k, 1);
    } else {
        R(0, 154, 240, 1, T_LINE);
        draw_cells(pg->k, live);
    }
}
static void draw_footer(uint32_t now)
{
    unsigned g = pages[page].group, n = 0, at = 0;
    for (unsigned i = 0; i < NPAGE; i++) if (pages[i].group == g) { if (i == page) at = n; n++; }
    if (saving) Q(4, 228, 2, "SAVE AS", T_INK);
    else {
        int32_t x0 = 120 - ((int32_t)n * 14 - 6) / 2;
        if (layer_shown()) { char b[24], *p = put(b, "HOLD: "); p = put(p, layers[layer].title); *p = 0; Q(4, 228, 2, b, T_ACC); }
        else {
            Q(4, 228, 2, pages[page].title, T_INK);
            for (unsigned i = 0; i < n; i++) R(x0 + (int32_t)i * 14, 230, 8, 5, i == at ? T_INK : T_LINE);
        }
        { char b[8], *p = put(b, "OCT"); if (oct >= 0) *p++ = '+'; fmt_int(p, oct); QR(236, 228, 2, b, oct ? T_INK : T_LINE); }
    }
    if (now < msg_until) {
        uint32_t a = now - msg_t0, bb = msg_until - now;
        int32_t up_px = a < 120u ? (int32_t)a * 18 / 120 : bb < 120u ? (int32_t)bb * 18 / 120 : 18;
        R(0, 240 - up_px, 240, 18, msg_warn ? T_WARN : T_INK);
        QC(120, 244 - up_px, 2, msg, T_BG);
    }
}
static void draw_all(uint32_t now)
{
    uint32_t a = now - tr_t0;
    g_ox = 0;
    draw_header();
    if (saving) { stage_save(); R(0, 154, 240, 1, T_LINE); draw_save_cells(); }
    else if (a < TR_MS && tr_dir) {
        int32_t t = (int32_t)(a * 256u / TR_MS), e = 256 - (256 - t) * (256 - t) / 256, off = 240 * e / 256;
        g_ox = -tr_dir * off; draw_page(tr_from, 0);
        g_ox = tr_dir * (240 - off); draw_page(page, 0);
        g_ox = 0;
    } else draw_page(page, 1);
    draw_footer(now);
}
static uint32_t band_sum[8];
static void render(uint32_t now)
{
    idle_keys = 1;
    for (uint32_t b = 0; b < 8u; b++) {
        uint32_t sum = 0x1234567u, n = 240u * 30u;
        cv_begin(240, 30, T_BG);
        cv_oy = -(int32_t)b * 30;
        draw_all(now);
        cv_oy = 0;
        for (uint32_t i = 0; i < n; i++) sum = (sum ^ cv_px[i]) * 16777619u;
        if (sum != band_sum[b]) { band_sum[b] = sum; cv_blit(0, b * 30u); }
        keys_poll();
    }
    lcd_sync();
    idle_keys = 0;
}

/* ======================================================================================================
 * Tasten, Schrittaufnahme (wie das SH-101: jede Taste ein Schritt; gedrückt, während die vorige noch gehalten ist:
 * gebunden; die oberste Taste eine Pause; GLO + Taste: die Note davor einen Schritt länger), Licht
 * ====================================================================================================== */
static uint32_t keys_prev;
static uint8_t held_part[27], held[27];
static uint32_t sq_sum_saved, sq_t_change, sq_sum_seen;
static uint8_t rec_play, rec_mheld[16];
static int seq_rec_key(uint32_t k, uint8_t note, uint32_t keys)
{
    unsigned s = rec_cur, glo = (fm1_in.buttons >> BTN_GLO) & 1u;
    if (s >= SEQ_STEPS) { msg_set("64 STEPS: REC TO FINISH", 0); return 1; }
    if (k == 26u) {
        sq.note[rec_part][s] = 0; sq.flag[rec_part][s] = SQ_REST;
        rec_key = rec_note = 0xff;
        rec_cur++;
        return 1;
    }
    if (glo && s > 0 && rec_note != 0xff) {
        sq.flag[rec_part][s - 1u] |= SQ_TIE;
        sq.note[rec_part][s] = rec_note; sq.flag[rec_part][s] = 0;
        rec_cur++;
        layer_used = 1;
        return 1;
    }
    if (s > 0 && rec_key < 27u && (keys >> rec_key & 1u)) sq.flag[rec_part][s - 1u] |= SQ_TIE;
    sq.note[rec_part][s] = note; sq.flag[rec_part][s] = 0;
    rec_key = (uint8_t)k; rec_note = note;
    rec_cur++;
    return 0;
}
static void seq_rec(int on)
{
    if (on) {
        rec_on = 1; rec_part = sel; rec_cur = 0; rec_key = rec_note = 0xff;
        rec_mr = rec_mw; seq_rec_p = (uint8_t)(sel + 1u);
        rec_play = sq.cfg[sel][SQ_ON];
        if (rec_play) { ev_put(EV_SEQ_LEN, sel, 0, 1); ev_put(EV_SEQ, sel, SQ_SHIFT, 24); }
        else ev_put(EV_SEQ, sel, SQ_ON, 0);
        for (unsigned i = 0; i < NPAGE; i++) if (pages[i].stage == ST_SEQ) { goto_page(i, 0); break; }
        msg_set("REC  KEYS = STEPS  REC = DONE", 0);
        return;
    }
    rec_on = 0; seq_rec_p = 0;
    ev_put(EV_SEQ_LEN, rec_part, rec_cur, rec_play);
    if (rec_cur) {
        char b[24], *q = b;
        ev_put(EV_SEQ, rec_part, SQ_ON, 1);
        fmt_int(q, rec_cur); q += str_len(q); q = put(q, rec_cur == 1u ? " STEP: PLAYING" : " STEPS: PLAYING"); *q = 0;
        msg_set(b, 0);
    } else msg_set("NOTHING RECORDED", 0);
}
static uint32_t sq_sum(void)
{
    const uint8_t *b = (const uint8_t *)&sq;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < sizeof sq; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static void seq_rec_midi(uint8_t note)
{
    unsigned s = rec_cur;
    if (s >= SEQ_STEPS) { msg_set("64 STEPS: REC TO FINISH", 0); return; }
    if ((fm1_in.buttons >> BTN_GLO) & 1u && s > 0 && rec_note != 0xff) {
        sq.flag[rec_part][s - 1u] |= SQ_TIE;
        sq.note[rec_part][s] = rec_note; sq.flag[rec_part][s] = 0;
        rec_cur++;
        layer_used = 1;
        return;
    }
    if (s > 0 && rec_key == 0xfe && rec_note != 0xff && (rec_mheld[rec_note >> 3] >> (rec_note & 7u) & 1u))
        sq.flag[rec_part][s - 1u] |= SQ_TIE;
    sq.note[rec_part][s] = note; sq.flag[rec_part][s] = 0;
    rec_key = 0xfe; rec_note = note;
    rec_cur++;
}
static void keys_poll(void)
{
    uint8_t rec_was = rec_cur;
    while (rec_mr != rec_mw) {
        uint8_t r = rec_mr, nn = rec_mn[r & 31u] & 127u, v = rec_mv[r & 31u];
        rec_mr = (uint8_t)(r + 1u);
        if (v) { if (rec_on) seq_rec_midi(nn); rec_mheld[nn >> 3] |= (uint8_t)(1u << (nn & 7u)); }
        else rec_mheld[nn >> 3] &= (uint8_t)~(1u << (nn & 7u));
    }
    uint32_t keys = fm1_in.notes, down = keys & ~keys_prev, upk = keys_prev & ~keys;
    keys_prev = keys;
    for (uint32_t k = 0; k < 27u && (down | upk); k++) {
        if (down >> k & 1u) {
            held[k] = (uint8_t)(53 + 12 * oct + (int32_t)k); held_part[k] = sel;
            if (rec_on && rec_part == sel && seq_rec_key(k, held[k], keys)) continue;
            ev_put(EV_ON, sel, held[k], 100);
        }
        if (upk >> k & 1u) ev_put(EV_OFF, held_part[k], held[k], 0);
    }
    if (rec_on && rec_play && rec_cur != rec_was) ev_put(EV_SEQ_LEN, rec_part, rec_cur, 1);
}
static void lights_init(void)
{
    for (uint32_t p = 0; p < FM1_NCOL; p++)
        for (uint32_t r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] >= 0) fm1_led_bg[p] |= (uint8_t)(1u << r);
    fm1_led_bg_ns = 8000u;
}
static void lights(void)
{
    unsigned g = pages[page].group;
    for (uint32_t k = 0; k < 27u; k++) fm1_led_key(KEY_LED(k), keys_prev >> k & 1u);
    fm1_led_key(BTN_OCT_DOWN, oct < 0); fm1_led_key(BTN_OCT_UP, oct > 0);
    if (layer) g = layer;
    fm1_led_key(BTN_HOME, !saving && g == G_HOME); fm1_led_key(BTN_EDIT, !saving && g == G_EDIT);
    fm1_led_key(BTN_ENV, !saving && g == G_ENV); fm1_led_key(BTN_LFO, !saving && g == G_LFO);
    fm1_led_key(BTN_FX, !saving && g == G_FX); fm1_led_key(BTN_GLO, !saving && g == G_GLO);
    fm1_led_key(BTN_SEL, !saving && g == G_SEL);
    fm1_led_key(BTN_ARP, !saving && (g == G_ARP || arpc[sel][ARP_ON]));
    fm1_led_key(BTN_SEQ, !saving && (g == G_SEQ || sq.cfg[sel][SQ_ON]));
    fm1_led_key(BTN_REC, rec_on && (fm1_ms / 300u & 1u));
    fm1_led_key(BTN_PLAY, xport == 1 ? clk_ticks % 24u < 4u : arpc[sel][ARP_HOLD]);
    fm1_led_key(BTN_SAVE, saving || (up[sel].dirty && (fm1_ms / 400u & 1u)));
}
static unsigned btn_group(uint32_t i)
{
    return i == BTN_EDIT ? G_EDIT : i == BTN_ENV ? G_ENV : i == BTN_LFO ? G_LFO : i == BTN_FX ? G_FX : i == BTN_GLO ? G_GLO : i == BTN_ARP ? G_ARP : i == BTN_SEQ ? G_SEQ : 0u;
}
static void panic_all(void)
{
    for (unsigned p = 0; p < GM_PARTS; p++) ev_put(EV_PANIC, p, 0, 0);
    msg_set("ALL NOTES OFF", 0);
}

/* ======================================================================================================
 * Stand beim Ausschalten (fm1-synths ADR 0011): ein Journal in den Slots der Plattform, 1,5 s nach der letzten
 * Änderung in einer Spielpause geschrieben, beim Einschalten der neueste ganze Eintrag. Jede Variante hat ihre
 * eigene Kennung GM_JR_MAGIC und Version GM_SET_VER; ein Eintrag mit anderer Länge (älterer Stand) wird übernommen,
 * soweit er reicht.
 * ====================================================================================================== */
#define JR_AT SLOTS_AT
#define JR_SECT 16u
#define JR_REC 512u
#define JR_PER (4096u / JR_REC)
#define JR_N (JR_SECT * JR_PER)
_Static_assert(JR_AT + JR_SECT * 0x1000u <= SLOTS_AT + NSLOT * SLOT_SECT * 0x1000u, "das Journal passt in die Slots");
typedef struct { uint32_t magic, seq; uint16_t len, crc; uint32_t sum; } jr_head_t;
/* Ein Eintrag beschreibt sich selbst: Längen vorn, dann die Einstellungen, dann die Werte je Part. So lädt eine
 * spätere Fassung (längeres `set`, mehr Parameter) einen älteren Eintrag (jr_load). */
typedef struct {
    uint16_t set_len, np;
    uint8_t page, parts;
    __typeof__(set) set;
    uint8_t v[GM_PARTS][GM_NP];
} jr_state_t;
_Static_assert(sizeof(jr_head_t) + sizeof(jr_state_t) <= JR_REC, "ein Stand passt in einen Eintrag");
_Static_assert(__builtin_offsetof(jr_state_t, set) == 6u, "Kopf des Eintrags: 6 Bytes (jr_load)");
static jr_state_t jr_now, jr_saved;
static uint32_t jr_seq, jr_pos, jr_t_change, jr_ok;
static uint8_t jr_clean[JR_SECT];
static uint8_t jr_buf[JR_REC];
static uint16_t jr_crc(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFu;
    while (n--) { c ^= (uint32_t)*p++ << 8; for (int k = 0; k < 8; k++) c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1; }
    return (uint16_t)c;
}
static void jr_take(jr_state_t *st)
{
    memset(st, 0, sizeof *st);
    set_pack();
    set.sel = sel; set.oct = oct;
    st->set = set;
    for (unsigned p = 0; p < GM_PARTS; p++) memcpy(st->v[p], up[p].v, GM_NP);
    st->page = page; st->np = GM_NP; st->set_len = sizeof st->set; st->parts = GM_PARTS;
}
static int jr_head(uint32_t i, jr_head_t *h) { return st_read(JR_AT + (i / JR_PER) * 0x1000u + (i % JR_PER) * JR_REC, h, sizeof *h); }
static int jr_playing(void)
{
    if (!quiet() || silent_blocks < 700u) return 1;
    for (unsigned p = 0; p < GM_PARTS; p++) if ((arp[p].cfg[ARP_ON] && arp[p].nkeys) || (sq.cfg[p][SQ_ON] && sq.len[p])) return 1;
    return 0;
}
static int jr_prog(uint32_t off, const uint8_t *src, uint32_t n)
{
    while (n) {
        uint32_t chunk = 64u - (off & 63u), b0 = *(volatile uint32_t *)&block_no, t0 = fm1_ms;
        if (chunk > n) chunk = n;
        while (*(volatile uint32_t *)&block_no == b0 && fm1_ms - t0 < 5u) {}
        if (fl_write(off, src, chunk)) return -1;
        off += chunk; src += chunk; n -= chunk;
    }
    return 0;
}
static void jr_write(void)
{
    uint32_t sect = jr_pos / JR_PER, off = JR_AT + sect * 0x1000u + (jr_pos % JR_PER) * JR_REC;
    jr_head_t h;
    if (jr_pos % JR_PER == 0 && !jr_clean[sect]) return;
    memcpy(jr_buf + sizeof h, &jr_now, sizeof jr_now);
    h.magic = GM_JR_MAGIC; h.seq = jr_seq + 1u; h.len = sizeof jr_now; h.crc = jr_crc(jr_buf + sizeof h, sizeof jr_now); h.sum = ~h.seq;
    jr_clean[sect] = 0;
    if (!jr_prog(off + sizeof h, jr_buf + sizeof h, sizeof jr_now) && !jr_prog(off, (const uint8_t *)&h, sizeof h)) {
        jr_seq++;
        jr_saved = jr_now;
    }
    jr_pos = (jr_pos + 1u) % JR_N;
}
static void jr_tidy(int may_stall)
{
    for (uint32_t k = 0; k < 3u; k++) {
        uint32_t s = (jr_pos / JR_PER + k) % JR_SECT, took;
        if (k == 0 && jr_pos % JR_PER) continue;
        if (jr_clean[s]) continue;
        if (!may_stall) return;
        fm1_wdt_feed();
        if (!fl_erase4k(JR_AT + s * 0x1000u, &took)) jr_clean[s] = 1;
        return;
    }
}
static int jr_load(jr_state_t *st)
{
    uint32_t best = 0, found = 0;
    static uint8_t page_buf[256];
    if (!flash_ok) return -1;
    for (uint32_t i = 0; i < JR_N; i++) {
        jr_head_t h;
        if (jr_head(i, &h) || h.magic != GM_JR_MAGIC || h.sum != ~h.seq || h.len > JR_REC - sizeof h || h.len < sizeof st->set) continue;
        if (found && (int32_t)(h.seq - jr_seq) <= 0) continue;
        {
            uint32_t off = JR_AT + (i / JR_PER) * 0x1000u + (i % JR_PER) * JR_REC + sizeof h;
            if (st_read(off, jr_buf, h.len) || jr_crc(jr_buf, h.len) != h.crc) continue;
        }
        {   /* die Längen des Eintrags: set so weit es reicht, die Werte je Part mit ihrer Anzahl */
            uint32_t sl = (uint32_t)jr_buf[0] | (uint32_t)jr_buf[1] << 8, np = (uint32_t)jr_buf[2] | (uint32_t)jr_buf[3] << 8;
            uint32_t parts = jr_buf[5], vo = 6u + sl;
            if (vo + parts * np > h.len || parts != GM_PARTS || !np) continue;
            jr_seq = h.seq; best = i; found = 1;
            memset(st, 0, sizeof *st);
            st->set_len = (uint16_t)sl; st->np = (uint16_t)np; st->page = jr_buf[4]; st->parts = (uint8_t)parts;
            memcpy(&st->set, jr_buf + 6, sl < sizeof st->set ? sl : sizeof st->set);
            for (unsigned p = 0; p < GM_PARTS; p++)
                for (unsigned k = 0; k < GM_NP; k++) st->v[p][k] = k < np ? jr_buf[vo + p * np + k] : 0xFFu;   /* (0xFF: aus dem Sound) */
        }
    }
    jr_pos = found ? (best + 1u) % JR_N : 0;
    for (uint32_t sct = 0; sct < JR_SECT; sct++) {
        uint32_t clean = 1;
        for (uint32_t o = 0; o < 0x1000u && clean; o += sizeof page_buf) {
            if (st_read(JR_AT + sct * 0x1000u + o, page_buf, sizeof page_buf)) { clean = 0; break; }
            for (uint32_t b = 0; b < sizeof page_buf; b++) if (page_buf[b] != 0xFFu) { clean = 0; break; }
        }
        jr_clean[sct] = (uint8_t)clean;
    }
    {
        uint32_t sct = jr_pos / JR_PER, from = (jr_pos % JR_PER) * JR_REC, clean = 1;
        for (uint32_t o = from; o < JR_PER * JR_REC && clean; o += 64u) {
            uint32_t n = JR_PER * JR_REC - o < 64u ? JR_PER * JR_REC - o : 64u;
            if (st_read(JR_AT + sct * 0x1000u + o, page_buf, n)) { clean = 0; break; }
            for (uint32_t b = 0; b < n; b++) if (page_buf[b] != 0xFFu) { clean = 0; break; }
        }
        if (!clean || jr_pos % JR_PER == 0) jr_pos = ((sct + 1u) % JR_SECT) * JR_PER;
        if (jr_pos % JR_PER == 0 && !jr_clean[jr_pos / JR_PER]) { uint32_t took; fm1_wdt_feed(); if (!fl_erase4k(JR_AT + (jr_pos / JR_PER) * 0x1000u, &took)) jr_clean[jr_pos / JR_PER] = 1; }
    }
    jr_ok = 1;
    return found ? 0 : -1;
}

/* ======================================================================================================
 * Hauptschleife
 * ====================================================================================================== */
static void app_main(void)
{
    uint32_t t_stats = 0, t_frame = 0, press_t[FM1_NBTN] = {0}, long_done = 0, combo = 0, shot_w = 0;
    static jr_state_t st;
    int have;

    own_load();
    if (!flash_ok || blob_load(3, &sq, sizeof sq) || sq.ver != SQ_VER) {
        memset(&sq, 0, sizeof sq);
        sq.ver = SQ_VER;
        for (unsigned p = 0; p < GM_PARTS; p++) { sq.cfg[p][SQ_DIV] = 2; sq.cfg[p][SQ_GATE] = 80; sq.cfg[p][SQ_SHIFT] = 24; }
    }
    for (unsigned p = 0; p < GM_PARTS; p++) {
        if (sq.len[p] > SEQ_STEPS) sq.len[p] = 0;
        for (unsigned i = 0; i < SQ_N; i++) if (sq.cfg[p][i] > sq_max[i]) sq.cfg[p][i] = 0;
        sq.cfg[p][SQ_ON] = 0;                   /* (beim Einschalten läuft nichts von selbst) */
        sqr[p].sounding = -1; sqr[p].fresh = 1;
    }
    sq_sum_saved = sq_sum_seen = sq_sum();
    have = flash_ok ? jr_load(&st) : -1;
    if (!have && st.set.ver == SET_VER) {
        set = st.set;
        page = st.page < NPAGE ? st.page : 0;
        grp_last[pages[page].group] = (uint8_t)(page + 1u);
    } else {
        have = -1;
        memset(&set, 0, sizeof set);
        set.ver = SET_VER;
        set.tempo = 80;
        for (unsigned p = 0; p < GM_PARTS; p++) {
            static const uint8_t arp_def[ARP_N] = {0, 0, 2, 0, 80, 0};
            set.p[p].idx = 0; set.p[p].chan = (uint8_t)(GM_PARTS == 1 ? 0 : p + 1); set.p[p].level = 100;
            set.p[p].send[0] = set.p[p].send[1] = 40;
            memcpy(set.p[p].arp, arp_def, ARP_N);
        }
        for (unsigned i = 0; i < FX_COUNT; i++) set.fx[i] = (uint8_t)syn_fx_default(i);
        set.fx[FX_DLY_LEVEL] = 0; set.fx[FX_REV_LEVEL] = 0;   /* (Effekte zuerst aus: das SH-101 hat keine) */
    }
    tempo = set.tempo <= 200u ? set.tempo : 80;
    ev_put(EV_TEMPO, 0, tempo, 0);
    clkv[0] = set.clk_src <= 2u ? set.clk_src : 0; clkv[1] = set.clk_send <= 1u ? set.clk_send : 0;
    ev_put(EV_CLK, 0, 0, clkv[0]); ev_put(EV_CLK, 0, 1, clkv[1]);
    for (unsigned i = 0; i < FX_COUNT; i++) { fxv[i] = set.fx[i] <= (unsigned)syn_fx_max(i) ? set.fx[i] : (uint8_t)syn_fx_default(i); ev_put(EV_FX, 0, i, fxv[i]); }
    fx_on = fxv[FX_DLY_LEVEL] || fxv[FX_REV_LEVEL];
    sel = set.sel < GM_PARTS ? set.sel : 0;
    oct = set.oct < -3 || set.oct > 3 ? 0 : set.oct;
    for (unsigned p = 0; p < GM_PARTS; p++) {
        up[p].chan = set.p[p].chan <= 16u ? set.p[p].chan : 0;
        up[p].level = set.p[p].level > 127u ? 100 : set.p[p].level;
        part_ch[p] = up[p].chan;
        load_sound(p, set.p[p].kind, set.p[p].idx);
        if (have == 0) {                        /* die Werte wie beim Ausschalten, geändert oder nicht */
            for (unsigned i = 0; i < GM_NP; i++) {
                uint8_t x = st.v[p][i];
                up[p].v[i] = x <= gm_params[i].max ? x : up[p].base[i];
            }
            mark_dirty(p); push_part(p);
        }
        ev_put(EV_LEVEL, p, up[p].level, 0);
        for (unsigned s = 0; s < 2u; s++) { sendv[p][s] = set.p[p].send[s] & 127u; ev_put(EV_SEND, p, s, sendv[p][s]); }
        for (unsigned a = 0; a < ARP_N; a++) { arpc[p][a] = set.p[p].arp[a] <= arp_max[a] ? set.p[p].arp[a] : 0; ev_put(EV_ARP, p, a, arpc[p][a]); }
    }
    jr_take(&jr_now);
    jr_saved = jr_now;
    if (flash_ok) for (int k = 0; k < 2; k++) jr_tidy(1);
    lcd_fill(0, 0, 240, 240, T_BG);
    lights_init();
    msg_set(APP_NAME " V" APP_VER, 0);

    for (;;) {
        uint32_t now, b, rel, cur;
        int32_t e;
        platform_poll();
        now = fm1_ms;
        keys_poll();
        if (tempo_taken) {
            uint32_t tt = tempo_taken;
            tempo_taken = 0;
            tempo = (uint8_t)((tt + 5u) / 10u - 40u);
            touch_set();
            msg_set("TEMPO KEPT", 0);
        }
        b = fm1_input_edges(&rel);
        cur = fm1_in.buttons;
        for (uint32_t i = 0; i < FM1_NBTN; i++) {
            uint32_t m = 1u << i;
            if (b & m) {
                press_t[i] = now; long_done &= ~m;
                if (i == BTN_OCT_DOWN || i == BTN_OCT_UP) {
                    if ((cur & (1u << BTN_OCT_DOWN)) && (cur & (1u << BTN_OCT_UP))) { combo = 1; panic_all(); }
                } else if (!saving) {
                    unsigned g = btn_group(i);
                    if (i == BTN_SEL) group_button(G_SEL);
                    else if (g) { layer = (uint8_t)g; layer_used = 0; layer_t0 = now; touched = 255; }
                }
            }
            if ((cur & m) && !(long_done & m) && i == BTN_PLAY && now - press_t[i] >= 600u && !saving) {
                long_done |= m;
                if (ext_follow) msg_set("FOLLOWS MIDI CLOCK", 0);
                else { xport_req = xport == 1 ? 2 : 1; msg_set(xport == 1 ? "STOP" : "START", 0); }
            }
            if ((cur & m) && !(long_done & m) && now - press_t[i] >= 1000u && (i == BTN_HOME || i == BTN_SAVE) && !saving) {
                long_done |= m;
                if (i == BTN_HOME) {
                    if (restore[sel].ok) {
                        restore[sel].ok = 0;
                        load_sound(sel, restore[sel].kind, restore[sel].idx);
                        memcpy(up[sel].v, restore[sel].v, GM_NP);
                        str_cpy(up[sel].name, restore[sel].name, sizeof up[sel].name);
                        mark_dirty(sel); push_part(sel);
                        msg_set("EDITS BROUGHT BACK", 0);
                    } else msg_set("NOTHING TO BRING BACK", 0);
                } else dlg_open(sel);
            }
            if (rel & m) {
                int was_long = (long_done & m) != 0;
                long_done &= ~m;
                switch (i) {
                case BTN_OCT_DOWN: case BTN_OCT_UP:
                    if (combo) { if (!(cur & ((1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP)))) combo = 0; }
                    else if (i == BTN_OCT_DOWN && oct > -3) { oct--; touch_set(); }
                    else if (i == BTN_OCT_UP && oct < 3) { oct++; touch_set(); }
                    break;
                case BTN_REC: if (!saving) seq_rec(!rec_on); break;
                case BTN_EDIT: case BTN_ENV: case BTN_LFO: case BTN_FX: case BTN_GLO: case BTN_ARP: case BTN_SEQ:
                    if (layer == btn_group(i)) {
                        if (!layer_used && now - press_t[i] < 400u && !saving) {
                            if (i == BTN_SEQ && pages[page].group == G_SEQ && sq.len[sel]) {   /* auf seiner Seite: an/aus */
                                kn_t k = SQK("SEQ", SQ_ON);
                                k_turn(&k, sq.cfg[sel][SQ_ON] ? -1 : 1);
                                msg_set(sq.cfg[sel][SQ_ON] ? "SEQ ON" : "SEQ OFF", 0);
                            } else group_button(layer);
                        }
                        layer = 0; touched = 255;
                    }
                    break;
                case BTN_PLAY:
                    if (!was_long && !saving) {
                        arpc[sel][ARP_HOLD] ^= 1u; ev_put(EV_ARP, sel, ARP_HOLD, arpc[sel][ARP_HOLD]); touch_set();
                        msg_set(arpc[sel][ARP_HOLD] ? "HOLD ON" : "HOLD OFF", 0);
                    }
                    break;
                case BTN_HOME: if (!was_long) { if (saving) saving = 0; else group_button(G_HOME); } break;
                case BTN_SAVE: if (!was_long) { if (saving) dlg_ok(sel); else save_short(sel); } break;
                default: break;
                }
            }
        }
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0 && !saving && GM_PARTS > 1) {
            if (rec_on) seq_rec(0);
            int s = (int)sel + e;
            sel = (uint8_t)(s < 0 ? 0 : s >= (int)GM_PARTS ? GM_PARTS - 1 : s);
            touch_set();
        }
        if ((e = fm1_enc_take(ENC_SELECT)) != 0 && !saving) {
            int p = (int)page + e;
            p = p < 0 ? 0 : p >= (int)NPAGE ? (int)NPAGE - 1 : p;
            goto_page((unsigned)p, e > 0 ? 1 : -1);
        }
        if ((e = fm1_enc_take(ENC_PRESETS)) != 0 && !saving) { step_sound(sel, e); touch_set(); }
        for (uint32_t k = 0; k < 4u; k++) {
            if ((e = fm1_enc_take(ENC_KNOB1 + k)) == 0) continue;
            touched = (uint8_t)k; t_touch = now;
            if (saving) {
                if (k == 0) { int c = (int)dlg.cursor + e; dlg.cursor = (uint8_t)(c < 0 ? 0 : c >= (int)GM_NAMEMAX ? GM_NAMEMAX - 1 : c); }
                else if (k == 1) {
                    const char *c = find_char(CHARS, dlg.name[dlg.cursor]);
                    int i = c ? (int)(c - CHARS) : 0, n = (int)sizeof CHARS - 1;
                    dlg.name[dlg.cursor] = CHARS[((i + e) % n + n) % n];
                } else if (k == 2) { int c = ((int)dlg.cat + e) % GM_NCAT; dlg.cat = (uint8_t)(c < 0 ? c + GM_NCAT : c); }
                else { int s = (int)dlg.slot + e; dlg.slot = (uint8_t)(s < 0 ? 0 : s >= NOWN ? NOWN - 1 : s); }
            } else {
                const kn_t *kn;
                if (layer) layer_used = 1;
                kn = &cells_now()[k];
                if (kn->kind != K_NONE) k_turn(kn, enc_fast(k, e, k_max(kn), now));
            }
        }
        lights();
        if (now - t_stats >= 500u) {
            uint32_t f = irq_save(), n = meas.blocks;
            for (uint32_t i = 0; i < M_N; i++) {
                avg[i] = n ? meas.sum[i] / n : 0;
                peak[i] = meas.peak[i];
                meas.sum[i] = meas.peak[i] = 0;
            }
            meas.blocks = 0;
            late_blocks = meas.late;
            irq_restore(f);
            if (peak[M_ALL] > max_all) max_all = peak[M_ALL];
            t_stats = now;
        }
        if (now - t_frame >= 33u) {
            t_frame = now;
            frame_no++;
            for (unsigned p = 0; p < GM_PARTS; p++) if (midi_hit[p] != midi_seen[p]) { midi_seen[p] = midi_hit[p]; midi_t[p] = now; }
            for (unsigned p = 0; p < GM_PARTS; p++) voices_now[p] = (uint8_t)kern_voices(p);
            for (unsigned k = 0; k < 4u; k++) {
                const kn_t *kn = &cells_now()[k];
                int mx = k_max(kn);
                int32_t target = mx ? k_get(kn) * 65536 / mx : 0, d = target - shown_q8[k];
                shown_q8[k] = d > -300 && d < 300 ? target : shown_q8[k] + d * 2 / 5;
            }
            if (pages[page].stage == ST_HOME && scope_w != shot_w) {
                uint32_t w = scope_w, start = w - 500u, i;
                int32_t pk = 0;
                shot_w = w;
                for (i = 0; i < 250u; i++)
                    if (scope_buf[(start + i) & 511u] < 0 && scope_buf[(start + i + 1u) & 511u] >= 0) break;
                if (i == 250u) i = 0;
                for (uint32_t x = 0; x < 240u; x++) { int32_t v = scope_buf[(start + i + x) & 511u]; if (v < 0) v = -v; if (v > pk) pk = v; }
                if (pk < 3) pk = 0; else if (pk < 16) pk = 16;
                for (uint32_t x = 0; x < 240u; x++) scope_shot[x] = (int8_t)(pk ? scope_buf[(start + i + x) & 511u] * 120 / pk : 0);
            }
            render(now);
        }
        if (flash_ok && !rec_on) {
            static uint32_t t_sq;
            if (now - t_sq >= 250u) {
                uint32_t h = sq_sum();
                t_sq = now;
                if (h != sq_sum_seen) { sq_sum_seen = h; sq_t_change = now; }
                else if (h != sq_sum_saved && now - sq_t_change >= 1500u && !jr_playing()) {
                    if (!blob_save(3, &sq, sizeof sq)) sq_sum_saved = h;
                }
            }
        }
        if (jr_ok && now - jr_t_change >= 250u) {
            static uint32_t t_check;
            if (now - t_check >= 250u) {
                jr_state_t s2;
                t_check = now;
                jr_take(&s2);
                if (memcmp(&s2, &jr_now, sizeof s2)) { jr_now = s2; jr_t_change = now; }
                else if (memcmp(&jr_now, &jr_saved, sizeof jr_now) && now - jr_t_change >= 1500u && !jr_playing()) jr_write();
                else if (!jr_playing()) jr_tidy(1);
            }
        }
    }
}
