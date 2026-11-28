/* SPDX-License-Identifier: GPL-3.0-only */
/* JX-8P for the M-VAVE FM-1: the panel and the screen (docs/adr/0003). Part of app/jx_app.c's unit; the MKS-50
 * firmware's (../../mks50/app/mk_ui.c, same repository and licence, ../../mks50/docs/adr/0008) with the
 * JX-8P's pages and pictures.
 *
 * One rule for the whole panel, as the FM-1 firmwares most people use have it (SLOOP, OberVave, FM-106):
 *   KNOB 1-4    the four cells at the bottom of the screen, always
 *   SELECT      the page before / the next page (one row of pages; the buttons are its bookmarks)
 *   PRESETS     the sound before / the next sound
 *   ALGORITHM   the part, A to D
 *   a button    its first page; pressed again, its next page
 * The screen is one layout everywhere: the parts and the page's name on top, the page's picture in the
 * middle, the four knob cells below, the page's place in its group at the bottom. */
#include "syn_font.h"

/* ======================================================================================================
 * Drawing: pixel letters and boxes into the platform's strip canvas, moved by (ox, oy) for the bands
 * and the page slide
 * ====================================================================================================== */
static int32_t g_ox;
static void R(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x + g_ox, y, w, h, c); }
static void LN(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { cv_line(x0 + g_ox, y0, x1 + g_ox, y1, c); }
static void FRAME(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    R(x, y, w, 1, c); R(x, y + h - 1, w, 1, c); R(x, y, 1, h, c); R(x + w - 1, y, 1, h, c);
}
/* 5 x 7 letters, a dot s x s pixels, 6 s from letter to letter */
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
/* 3 x 5 letters, 4 s from letter to letter */
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

/* ======================================================================================================
 * Pages
 * ====================================================================================================== */
enum { G_HOME, G_EDIT, G_ENV, G_LFO, G_FX, G_ARP, G_GLO, G_SEL, G_SEQ, NGROUP };
enum { ST_HOME, ST_PARTS, ST_DCO1, ST_DCO2, ST_FILTER, ST_ENV1, ST_ENV2, ST_LFO, ST_NAME, ST_SYS, ST_BROWSE, ST_AMP,
       ST_CHORUS, ST_DELAY, ST_REVERB, ST_ARP, ST_CHORD, ST_VOICE, ST_TEMPO, ST_MIX, ST_DCOMOD, ST_ENVS, ST_SETS, ST_SEQ, ST_LIVE };
enum { K_NONE, K_PARAM, K_LEVEL, K_CHAN, K_CAT, K_FX, K_ARP, K_TEMPO, K_CHORD, K_CLK, K_SEQ, K_SET, K_TEST, K_LIVE };
typedef struct { const char *label; uint8_t kind, param; } kn_t;
typedef struct { uint8_t group, stage; const char *title; kn_t k[4]; } page_t;
#define PRM(l, p) {l, K_PARAM, p}
#define NO {"", K_NONE, 0}
#define FXK(l, p) {l, K_FX, p}
#define ARK(l, p) {l, K_ARP, p}
#define SQK(l, p) {l, K_SEQ, p}
#define LVK(l, p) {l, K_LIVE, p}   /* FM1-Electro-one: Live-Aufnahme (ADR 0053) */
/* the JX-8P's panel in its order (DCO-1, DCO-2, MIXER, VCF, VCA, ENV-1, ENV-2, LFO, CHORUS), then the pages
 * the MKS-50 firmware has besides (../../mks50/docs/adr/0008) */
static const page_t pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", JP_CUTOFF), PRM("RESO", JP_RESO), PRM("ENV", JP_VCF_ENV), PRM("RELEASE", JP_E2_RELEASE)}},
    {G_HOME, ST_PARTS, "MIX", {{"A", K_LEVEL, 0}, {"B", K_LEVEL, 1}, {"C", K_LEVEL, 2}, {"D", K_LEVEL, 3}}},
    {G_EDIT, ST_DCO1, "DCO-1", {PRM("RANGE", JP_DCO1_RANGE), PRM("WAVE", JP_DCO1_WAVE), PRM("TUNE", JP_DCO1_TUNE), PRM("LFO", JP_DCO1_LFO)}},
    {G_EDIT, ST_DCO2, "DCO-2", {PRM("RANGE", JP_DCO2_RANGE), PRM("WAVE", JP_DCO2_WAVE), PRM("X-MOD", JP_DCO2_XMOD), PRM("TUNE", JP_DCO2_TUNE)}},
    {G_EDIT, ST_DCO2, "DCO-2", {PRM("FINE", JP_DCO2_FINE), PRM("LFO", JP_DCO2_LFO), NO, NO}},
    {G_EDIT, ST_DCOMOD, "DCO ENV", {PRM("DCO-1", JP_DCO1_ENV), PRM("DCO-2", JP_DCO2_ENV), PRM("ENV", JP_DCO_ENV_MODE), PRM("DYNA", JP_DCO_DYNA)}},
    {G_EDIT, ST_MIX, "MIXER", {PRM("DCO-1", JP_MIX_DCO1), PRM("DCO-2", JP_MIX_DCO2), PRM("ENV", JP_MIX_ENV), PRM("ENV MD", JP_MIX_ENV_MODE)}},
    {G_EDIT, ST_FILTER, "VCF", {PRM("CUTOFF", JP_CUTOFF), PRM("RESO", JP_RESO), PRM("ENV", JP_VCF_ENV), PRM("HPF", JP_HPF)}},
    {G_EDIT, ST_FILTER, "VCF", {PRM("LFO", JP_VCF_LFO), PRM("KEY", JP_VCF_KEY), PRM("ENV MD", JP_VCF_ENV_MODE), PRM("DYNA", JP_VCF_DYNA)}},
    {G_EDIT, ST_AMP, "VCA", {PRM("LEVEL", JP_VCA_LEVEL), PRM("MODE", JP_VCA_ENV_MODE), PRM("DYNA", JP_VCA_DYNA), PRM("VOLUME", JP_VOLUME)}},
    {G_ENV, ST_ENV1, "ENV-1", {PRM("ATTACK", JP_E1_ATTACK), PRM("DECAY", JP_E1_DECAY), PRM("SUSTAIN", JP_E1_SUSTAIN), PRM("RELEASE", JP_E1_RELEASE)}},
    {G_ENV, ST_ENV2, "ENV-2", {PRM("ATTACK", JP_E2_ATTACK), PRM("DECAY", JP_E2_DECAY), PRM("SUSTAIN", JP_E2_SUSTAIN), PRM("RELEASE", JP_E2_RELEASE)}},
    {G_ENV, ST_ENVS, "ENV KEY", {PRM("1 KEY", JP_E1_KEY), PRM("2 KEY", JP_E2_KEY), PRM("SNAP", JP_ENV_SNAP), PRM("RETRIG", JP_ENV_RETRIG)}},
    {G_LFO, ST_LFO, "LFO", {PRM("RATE", JP_LFO_RATE), PRM("DELAY", JP_LFO_DELAY), PRM("WAVE", JP_LFO_WAVE), PRM("SYNC", JP_LFO_SYNC)}},
    {G_LFO, ST_LFO, "LFO", {PRM("SHAPE", JP_LFO_SHAPE), PRM("KEYSYNC", JP_LFO_KEYSYNC), NO, NO}},
    {G_FX, ST_CHORUS, "CHORUS", {PRM("CHORUS", JP_CHORUS), PRM("MODE", JP_CHORUS_MODE), PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND)}},
    {G_FX, ST_CHORUS, "CHORUS", {PRM("RATE", JP_CHORUS_RATE), NO, NO, NO}},
    {G_FX, ST_DELAY, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("LEVEL", FX_DLY_LEVEL)}},
    {G_FX, ST_DELAY, "DELAY", {FXK("SYNC", FX_DLY_SYNC), FXK("PING", FX_DLY_PING), FXK("MOD", FX_DLY_MOD), NO}},
    {G_FX, ST_REVERB, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("LEVEL", FX_REV_LEVEL)}},
    {G_FX, ST_REVERB, "REVERB", {FXK("TYPE", FX_REV_TYPE), FXK("MOD", FX_REV_MOD), NO, NO}},
    {G_ARP, ST_ARP, "ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCTAVES", ARP_OCT)}},
    {G_ARP, ST_ARP, "ARP", {ARK("GATE", ARP_GATE), ARK("HOLD", ARP_HOLD), {"TEMPO", K_TEMPO, 0}, NO}},
    {G_ARP, ST_CHORD, "CHORD", {{"CHORD", K_CHORD, 0}, PRM("MODE", JP_KEY_MODE), NO, NO}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_SEQ, ST_SEQ, "SEQ", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"SOURCE", K_CLK, 0}, {"SEND", K_CLK, 1}, NO}},
    {G_GLO, ST_PARTS, "MIDI", {{"A", K_CHAN, 0}, {"B", K_CHAN, 1}, {"C", K_CHAN, 2}, {"D", K_CHAN, 3}}},
    {G_GLO, ST_VOICE, "VOICE", {PRM("MODE", JP_KEY_MODE), PRM("GLIDE", JP_PORTA), PRM("TIME", JP_PORTA_TIME), PRM("SHIFT", JP_KEY_SHIFT)}},
    {G_GLO, ST_VOICE, "VOICE", {PRM("UNI DET", JP_UNISON_DETUNE), PRM("DRIFT", JP_DRIFT), NO, NO}},
    {G_GLO, ST_SYS, "SYSTEM", {{"TEST", K_TEST, 0}, {"SOUND", K_TEST, 1}, {"CORES", K_TEST, 2}, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {{"GROUP", K_CAT, 0}, NO, NO, NO}},
    {G_SEL, ST_SETS, "SETS", {{"SET", K_SET, 0}, {"LOAD", K_SET, 1}, NO, NO}},
};
enum { NPAGE = sizeof pages / sizeof pages[0], PAGE_SAVE = NPAGE };

/* Live (../../mks50/docs/adr/0008): a page button HELD puts its most important four values on the knobs at once,
 * whatever page is shown; let go and the knobs are the page's again. TAPPED it opens its pages. */
typedef struct { const char *title; kn_t k[4]; } layer_t;
static const layer_t layers[NGROUP] = {
    [G_EDIT] = {"DCO + MIXER", {PRM("X-MOD", JP_DCO2_XMOD), PRM("2 TUNE", JP_DCO2_TUNE), PRM("MIX 1", JP_MIX_DCO1), PRM("MIX 2", JP_MIX_DCO2)}},
    [G_ENV] = {"ENV-1 / ENV-2", {PRM("1 ATTACK", JP_E1_ATTACK), PRM("1 DECAY", JP_E1_DECAY), PRM("2 ATTACK", JP_E2_ATTACK), PRM("2 RELEASE", JP_E2_RELEASE)}},
    [G_LFO] = {"LFO", {PRM("RATE", JP_LFO_RATE), PRM("DELAY", JP_LFO_DELAY), PRM("PITCH", JP_DCO1_LFO), PRM("FILTER", JP_VCF_LFO)}},
    [G_FX] = {"EFFECTS", {PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND), FXK("TIME", FX_DLY_TIME), PRM("CHORUS", JP_CHORUS)}},
    [G_ARP] = {"ARP + CHORD", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), {"CHORD", K_CHORD, 0}}},
    [G_GLO] = {"LEVELS", {{"A", K_LEVEL, 0}, {"B", K_LEVEL, 1}, {"C", K_LEVEL, 2}, {"D", K_LEVEL, 3}}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};
static uint8_t layer;                           /* the group whose button is held (0: none; G_HOME has no layer) */
static uint8_t layer_used;                      /* a knob was turned while it was held: not a tap */
static uint32_t layer_t0;
static int layer_shown(void) { return layer && (layer_used || fm1_ms - layer_t0 > 220u); }

static uint8_t page, sel, saving;               /* the page shown, the part selected, the save dialog open */
static int8_t oct;
static const kn_t *cells_now(void);
static uint8_t touched = 255;                   /* the knob turned last (its cell is lit for a moment) */
static uint32_t t_touch;
static char msg[32];
static uint32_t msg_t0, msg_until;
static uint8_t msg_warn;
static void msg_set(const char *s, int warn)
{
    str_cpy(msg, s, sizeof msg);
    msg_t0 = fm1_ms;
    msg_until = fm1_ms + 2200u;
    msg_warn = (uint8_t)warn;
}
static void touch_set(void) { set_dirty = 1; t_set = fm1_ms; }

static const kn_t *cells_now(void) { return layer_shown() || (layer && layer_used) ? layers[layer].k : pages[page].k; }

/* the page slide */
static uint8_t tr_from;
static int8_t tr_dir;
static uint32_t tr_t0;
#define TR_MS 150u
static uint8_t grp_last[NGROUP];                /* each group's page shown last (0: none yet): a button brings it back */
static void goto_page(unsigned p, int dir)
{
    if (p == page || p >= NPAGE) return;
    grp_last[pages[p].group] = (uint8_t)(p + 1u);
    tr_from = page; tr_dir = (int8_t)(dir ? dir : p > page ? 1 : -1); tr_t0 = fm1_ms;
    page = (uint8_t)p;
    touched = 255;
}
static void group_button(unsigned g)            /* its page as left (first: its first); on one of its pages already: its next */
{
    unsigned first = NPAGE, next = NPAGE;
    for (unsigned i = 0; i < NPAGE; i++) if (pages[i].group == g) { if (first == NPAGE) first = i; if (i > page && next == NPAGE) next = i; }
    if (pages[page].group != g) {               /* from another group: the page left there (else its first) */
        unsigned last = grp_last[g] ? grp_last[g] - 1u : first;
        goto_page(last < NPAGE && pages[last].group == g ? last : first, 0);
    }
    else goto_page(next < NPAGE ? next : first, 1);
}

/* ---- the colour of a knob cell: what the control does (the iOS app's Theme.paramColor / sectionColor) */
static uint16_t k_colour(const kn_t *k)
{
    if (k->kind == K_FX) return T_VIOLET;
    if (k->kind == K_TEMPO || k->kind == K_CLK || k->kind == K_SEQ) return T_VIOLET;
    if (k->kind == K_TEST) return T_ORANGE;
    if (k->kind != K_PARAM) return T_SEL;
    switch (k->param) {
    case JP_DCO1_RANGE: case JP_DCO1_WAVE: case JP_DCO1_TUNE: case JP_DCO2_RANGE: case JP_DCO2_WAVE: case JP_DCO2_XMOD:
    case JP_DCO2_TUNE: case JP_DCO2_FINE: case JP_MIX_DCO1: case JP_MIX_DCO2: case JP_UNISON_DETUNE: case JP_DRIFT:
    case JP_KEY_SHIFT:
        return T_BLUE;
    case JP_DCO1_LFO: case JP_DCO2_LFO: case JP_DCO1_ENV: case JP_DCO2_ENV: case JP_DCO_ENV_MODE: case JP_DCO_DYNA:
    case JP_MIX_ENV: case JP_MIX_ENV_MODE: case JP_MIX_DYNA: case JP_VCF_LFO: case JP_VCF_ENV: case JP_VCF_ENV_MODE:
    case JP_VCF_DYNA: case JP_VCA_DYNA: case JP_LFO_RATE: case JP_LFO_DELAY: case JP_LFO_WAVE: case JP_LFO_KEYSYNC:
    case JP_LFO_SHAPE: case JP_LFO_SYNC: case JP_LFO_DIV:
        return T_GREEN;
    case JP_CUTOFF: case JP_RESO: case JP_VCF_KEY: case JP_HPF: return T_ORANGE;
    case JP_CHORUS: case JP_CHORUS_MODE: case JP_CHORUS_RATE: case JP_DELAY_SEND: case JP_REVERB_SEND: return T_VIOLET;
    default: return T_SEL;                      /* envelopes, amp, voice */
    }
}

/* ---- the sets' places (see perf_take below) */
enum { NSET = 8 };
typedef struct {
    uint8_t used;
    char name[NAMEMAX + 1];
    __typeof__(set) s;
    char pname[JX_PARTS][NAMEMAX + 1];
    uint8_t v[JX_PARTS][JP_COUNT];
} perf_t;
static perf_t perfs[NSET], perf_undo;
static uint8_t perf_cur, perf_undo_ok;
_Static_assert(sizeof perfs <= 4096u - 64u, "the sets fit their sector");

/* ---- values of the knob cells */
static const uint8_t arp_max[ARP_N] = {1, 4, FX_DIVS - 1, 3, 127, 1};
static const char *const arp_modes[5] = {"UP", "DOWN", "UP/DN", "RAND", "ORDER"};
static const char *const chord_names[9] = {"OFF", "m7", "SUS2", "MIN", "5+8", "SUS4", "7TH", "MAJ", "m7b5"};
/* LFO RATE turns the note value while LFO SYNC is on (as the delay's TIME with SYNC) */
static unsigned k_prm(const kn_t *k) { return k->param == JP_LFO_RATE && up[sel].v[JP_LFO_SYNC] ? JP_LFO_DIV : k->param; }
static void fmt_hz(char *b, float hz)          /* 0.02 .. 65 Hz in at most 4 characters: 0.24, 9.8, 26 */
{
    int32_t x;
    if (hz < 0.995f) { x = (int32_t)(hz * 100.0f + 0.5f); b[0] = '0'; b[1] = '.'; b[2] = (char)('0' + x / 10); b[3] = (char)('0' + x % 10); b[4] = 0; }
    else if (hz < 9.95f) { x = (int32_t)(hz * 10.0f + 0.5f); b[0] = (char)('0' + x / 10); b[1] = '.'; b[2] = (char)('0' + x % 10); b[3] = 0; }
    else fmt_int(b, (int32_t)(hz + 0.5f));
}
static int k_max(const kn_t *k)
{
    switch (k->kind) {
    case K_PARAM: return jx_param_max(k_prm(k));
    case K_LEVEL: return 127;
    case K_CHAN: return 16;
    case K_CAT: return NCAT - 1;
    case K_FX: return k->param == FX_DLY_TIME && fxv[FX_DLY_SYNC] ? FX_DIVS - 1 : syn_fx_max(k->param);
    case K_ARP: return arp_max[k->param];
    case K_TEMPO: return 200;
    case K_CLK: return k->param ? 1 : 2;
    case K_SEQ: return k->param == SQ_ON ? 1 : k->param == SQ_DIV ? FX_DIVS - 1 : k->param == SQ_SHIFT ? 48 : 127;
    case K_LIVE: return k->param == LV_SET_BARS ? LV_BARS : 1;
    case K_SET: return k->param ? 1 : NSET - 1;
    case K_CHORD: return 8;
    case K_TEST: return k->param == 0 ? 8 : k->param == 1 ? (int)jx_preset_count - 1 : 1;
    default: return 0;
    }
}
static int k_get(const kn_t *k)
{
    switch (k->kind) {
    case K_PARAM: return up[sel].v[k_prm(k)];
    case K_LEVEL: return up[k->param].level;
    case K_CHAN: return up[k->param].chan;
    case K_CAT: return (int)cat_rank(up[sel].cat_view);
    case K_FX: return k->param == FX_DLY_TIME && fxv[FX_DLY_SYNC] ? fxv[FX_DLY_DIV] : fxv[k->param];
    case K_ARP: return arpc[sel][k->param];
    case K_TEMPO: return tempo;
    case K_CLK: return clkv[k->param];
    case K_SEQ: return sq.cfg[sel][k->param];
    case K_LIVE:
        return k->param == LV_SET_ON ? lv_store.p[sel].on : k->param == LV_SET_BARS ? lv_store.p[sel].bars
             : k->param == LV_SET_GROW ? lv_store.p[sel].grow : lv_store.p[sel].cut;
    case K_SET: return k->param ? 0 : perf_cur;
    case K_TEST: return test_v[k->param];
    case K_CHORD: return up[sel].v[JP_KEY_MODE] == 1 ? (up[sel].v[JP_CHORD] & 7) + 1 : 0;
    default: return 0;
    }
}
static void k_text(const kn_t *k, int v, char *b)
{
    const char *n;
    switch (k->kind) {
    case K_PARAM:
        if (k_prm(k) == JP_LFO_RATE) fmt_hz(b, jx_lfo_hz(v));   /* (in Hz: the curve is steep) */
        else if (k->param == JP_CHORD) str_cpy(b, chord_names[(v & 7) + 1], 10);
        else if ((n = jx_param_value_name(k_prm(k), v)) != 0) str_cpy(b, n, 10);
        else if (k->param == JP_DCO2_FINE) { char *p = b; v -= 64; if (v >= 0) *p++ = '+'; fmt_int(p, v); }
        else fmt_int(b, v);
        break;
    case K_CHAN: if (v) fmt_int(b, v); else str_cpy(b, "OMNI", 10); break;
    case K_CAT: str_cpy(b, cat_name[cat_at((unsigned)v)], 10); break;
    case K_FX:
        if (k->param == FX_DLY_TIME && fxv[FX_DLY_SYNC]) str_cpy(b, syn_fx_value_name(FX_DLY_DIV, v), 10);
        else if (k->param == FX_DLY_TIME) {     /* the time in ms */
            float sec = syn_fx_delay_seconds(v, 0, 0, 120.0f);
            if (sec > 0.70f) sec = 0.70f;
            fmt_int(b, (int32_t)(sec * 1000.0f + 0.5f)); str_cpy(b + str_len(b), "MS", 4);
        } else if ((n = syn_fx_value_name(k->param, v)) != 0) str_cpy(b, n, 10);
        else fmt_int(b, v);
        break;
    case K_ARP:
        if (k->param == ARP_ON || k->param == ARP_HOLD) str_cpy(b, v ? "ON" : "OFF", 10);
        else if (k->param == ARP_MODE) str_cpy(b, arp_modes[v % 5], 10);
        else if (k->param == ARP_DIV) str_cpy(b, syn_fx_value_name(FX_DLY_DIV, v), 10);
        else if (k->param == ARP_OCT) fmt_int(b, v + 1);
        else fmt_int(b, v);
        break;
    case K_TEMPO: fmt_int(b, v + 40); break;
    case K_CLK: str_cpy(b, k->param ? (v ? "USB" : "OFF") : v == 1 ? "INT" : v == 2 ? "MIDI" : "AUTO", 10); break;
    case K_CHORD: str_cpy(b, chord_names[v % 9], 10); break;
    case K_LIVE:
        if (k->param == LV_SET_ON) str_cpy(b, v ? "ON" : "OFF", 10);
        else if (k->param == LV_SET_BARS) { if (v) fmt_int(b, v); else str_cpy(b, "EMPTY", 10); }
        else if (k->param == LV_SET_GROW) str_cpy(b, v ? "EMPTY" : "REPEAT", 10);
        else str_cpy(b, v ? "END" : "START", 10);
        break;
    case K_SEQ:
        if (k->param == SQ_ON) str_cpy(b, v ? "ON" : "OFF", 10);
        else if (k->param == SQ_DIV) str_cpy(b, syn_fx_value_name(FX_DLY_DIV, v), 10);
        else if (k->param == SQ_SHIFT) { char *q = b; v -= 24; if (v >= 0) *q++ = '+'; fmt_int(q, v); }
        else fmt_int(b, v);
        break;
    case K_SET:
        if (k->param) str_cpy(b, perfs[perf_cur].used ? "TURN >" : "EMPTY", 10);
        else { char *p = b; if (v + 1 < 10) *p++ = '0'; fmt_int(p, v + 1); }
        break;
    case K_TEST:
        if (k->param == 0) { if (v) fmt_int(b, v); else str_cpy(b, "OFF", 10); }
        else if (k->param == 1) str_cpy(b, jx_presets[v].name, 11);
        else fmt_int(b, v + 1);
        break;
    default: fmt_int(b, v); break;
    }
}
/* ---- sets (../../mks50/docs/adr/0013, docs/adr/0003): the whole setup in one of 8 places (blob 1): every part's sound as it is, unsaved changes
 * included, with its name, channel, level and arp, and the effects, tempo and clock. KNOB 2 on the page SETS: right
 * loads the set chosen with KNOB 1, left brings back what was there before the load. */
static void perf_take(perf_t *r)               /* the setup as it is now */
{
    memset(r, 0, sizeof *r);
    set_pack();
    set.sel = sel; set.oct = oct;
    r->s = set;
    r->used = 1;
    for (unsigned p = 0; p < JX_PARTS; p++) {
        memcpy(r->v[p], up[p].v, JP_COUNT);
        str_cpy(r->pname[p], up[p].name, sizeof r->pname[p]);
    }
}
static void perf_apply(const perf_t *r)         /* as at power-on (app_main), but the part and octave stay */
{
    if (!r->used || r->s.ver != SET_VER) return;
    set = r->s;
    set.sel = sel; set.oct = oct;
    tempo = set.tempo <= 200u ? set.tempo : 80;
    ev_put(EV_TEMPO, 0, tempo, 0);
    clkv[0] = set.clk_src <= 2u ? set.clk_src : 0; clkv[1] = set.clk_send <= 1u ? set.clk_send : 0;
    ev_put(EV_CLK, 0, 0, clkv[0]); ev_put(EV_CLK, 0, 1, clkv[1]);
    for (unsigned i = 0; i < FX_COUNT; i++) { fxv[i] = set.fx[i] <= (unsigned)syn_fx_max(i) ? set.fx[i] : (uint8_t)(i == FX_DLY_TIME ? syn_fx_max(i) : syn_fx_default(i)); ev_put(EV_FX, 0, i, fxv[i]); }
    fx_on = fxv[FX_DLY_LEVEL] || fxv[FX_REV_LEVEL];
    for (unsigned p = 0; p < JX_PARTS; p++) {
        ev_put(EV_PANIC, p, 0, 0);                  /* (nothing hangs from the sounds before) */
        up[p].chan = set.p[p].chan <= 16u ? set.p[p].chan : (uint8_t)(p + 1);
        up[p].level = set.p[p].level > 127u ? 100 : set.p[p].level;
        part_ch[p] = up[p].chan;
        load_sound(p, set.p[p].kind, set.p[p].idx);
        memcpy(up[p].v, r->v[p], JP_COUNT);
        for (unsigned i = 0; i < JP_COUNT; i++) if (up[p].v[i] > jx_param_max(i)) up[p].v[i] = up[p].base[i];
        if (r->pname[p][0]) str_cpy(up[p].name, r->pname[p], sizeof up[p].name);
        mark_dirty(p); push_part(p);
        ev_put(EV_LEVEL, p, up[p].level, 0);
        for (unsigned a = 0; a < ARP_N; a++) { arpc[p][a] = set.p[p].arp[a] <= arp_max[a] ? set.p[p].arp[a] : 0; ev_put(EV_ARP, p, a, arpc[p][a]); }
    }
    touch_set();
}
static void perf_msg(const char *what, unsigned i)   /* "SET 03 LOADED" */
{
    char b[32], *p = put(b, "SET ");
    if (i + 1u < 10u) *p++ = '0';
    fmt_int(p, (int32_t)i + 1); p += str_len(p);
    *p++ = ' '; p = put(p, what); *p = 0;
    msg_set(b, 0);
}

static void set_param(unsigned p, unsigned prm, int v)
{
    int mx = jx_param_max(prm);
    v = v < 0 ? 0 : v > mx ? mx : v;
    if (up[p].v[prm] == v) return;
    up[p].v[prm] = (uint8_t)v;
    ev_put(EV_SET, p, prm, (uint32_t)v);
    mark_dirty(p);
}
static void k_turn(const kn_t *k, int32_t d)
{
    int v = k_get(k) + d, mx = k_max(k);
    v = v < 0 ? 0 : v > mx ? mx : v;
    switch (k->kind) {
    case K_PARAM: set_param(sel, k_prm(k), v); break;
    case K_LEVEL: up[k->param].level = (uint8_t)v; ev_put(EV_LEVEL, k->param, (uint32_t)v, 0); set_pack(); touch_set(); break;
    case K_CHAN: up[k->param].chan = (uint8_t)v; part_ch[k->param] = (uint8_t)v; set_pack(); touch_set(); break;
    case K_FX: {
        unsigned prm = k->param == FX_DLY_TIME && fxv[FX_DLY_SYNC] ? FX_DLY_DIV : k->param;
        fxv[prm] = (uint8_t)v; ev_put(EV_FX, 0, prm, (uint32_t)v); fx_on = fxv[FX_DLY_LEVEL] || fxv[FX_REV_LEVEL];
        touch_set();
        break;
    }
    case K_ARP: arpc[sel][k->param] = (uint8_t)v; ev_put(EV_ARP, sel, k->param, (uint32_t)v); touch_set(); break;
    case K_TEMPO: tempo = (uint8_t)v; ev_put(EV_TEMPO, 0, (uint32_t)v, 0); touch_set(); break;
    case K_CLK: clkv[k->param] = (uint8_t)v; ev_put(EV_CLK, 0, k->param, (uint32_t)v); touch_set(); break;
    case K_LIVE: lv_ui_set(sel, k->param, (unsigned)v); break;   /* (the audio side writes lv_store) */
    case K_SEQ:
        if (k->param == SQ_ON && v && !sq.len[sel]) { msg_set("NO STEPS YET: PRESS REC", 0); break; }
        ev_put(EV_SEQ, sel, k->param, (uint32_t)v);
        break;
    case K_CHORD:                               /* OFF: POLY; a chord: key mode CHORD and its type */
        if (v) { set_param(sel, JP_KEY_MODE, 1); set_param(sel, JP_CHORD, v - 1); }
        else if (up[sel].v[JP_KEY_MODE] == 1) set_param(sel, JP_KEY_MODE, 0);
        break;
    case K_TEST:                                /* the load test (docs/adr/0002) */
        {   /* TEST: n voices over parts A-C, which get the SOUND when it starts; CORES: one or both */
            unsigned was = test_v[0];
            test_v[k->param] = (uint8_t)v;
            if (k->param == 2) { want_cores = (uint8_t)(v + 1); break; }
            if (k->param == 0 && was == (unsigned)v) break;
            if (test_v[0] && (k->param == 1 || !was)) { for (unsigned p = 0; p < 3u; p++) load_sound(p, 0, test_v[1]); set_pack(); touch_set(); }
            if (k->param == 0 || test_v[0]) ev_put(EV_TEST, 0, test_v[0], 0);
        }
        break;
    case K_SET:
        if (!k->param) { perf_cur = (uint8_t)v; break; }
        if (d > 0) {                            /* right: load it (what was there stays reachable: left) */
            if (!perfs[perf_cur].used) { perf_msg("IS EMPTY", perf_cur); break; }
            perf_take(&perf_undo); perf_undo_ok = 1;
            perf_apply(&perfs[perf_cur]);
            perf_msg("LOADED  < BACK", perf_cur);
        } else if (perf_undo_ok) { perf_apply(&perf_undo); perf_undo_ok = 0; msg_set("BACK TO BEFORE THE LOAD", 0); }
        else msg_set("NOTHING TO BRING BACK", 0);
        break;
    case K_CAT: {
        unsigned c = up[sel].cat_view;
        uint8_t kk, ii;
        for (int32_t n = d < 0 ? -d : d; n > 0; n--) {   /* (the knob stops at the ends: INIT left, EIGENE right) */
            unsigned c2 = next_cat(c, d > 0 ? 1 : -1);
            if (d > 0 ? cat_rank(c2) <= cat_rank(c) : cat_rank(c2) >= cat_rank(c)) break;
            c = c2;
        }
        if (c != up[sel].cat_view && !cat_item(c, 0, &kk, &ii)) { up[sel].cat_view = (uint8_t)c; load_sound(sel, kk, ii); set_pack(); touch_set(); }
        break;
    }
    default: break;
    }
}

/* A knob turned faster moves further (docs/adr/0008): its steps times a factor from how many detents a
 * second it makes. Only where there is a way to go (more than 16 values); lists step one by one. */
static uint32_t enc_t[8];
static int32_t enc_fast(unsigned id, int32_t e, int range, uint32_t now)
{
    uint32_t dt = now - enc_t[id], n = (uint32_t)(e < 0 ? -e : e), rate, f;
    enc_t[id] = now;
    if (range <= 16 || dt > 250u) return e;
    if (dt < 4u) dt = 4u;
    rate = n * 1000u / dt;                      /* detents a second */
    f = rate < 12u ? 1u : rate < 22u ? 2u : rate < 34u ? 4u : rate < 50u ? 7u : 12u;
    return e * (int32_t)f;
}

/* ---- saving (docs/adr/0006) */
static const char CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+.";
static struct { uint8_t cursor, cat, slot, perf; char name[NAMEMAX + 1]; } dlg;   /* perf: a set (else a sound) */
static void flash_own(unsigned slot)
{
    char b[32], *p = b;
    if (!flash_ok || blob_save(0, own, sizeof own)) { msg_set("SAVE FAILED", 1); return; }
    p = put(p, "SAVED: USER "); if (slot + 1u < 10u) *p++ = '0';
    fmt_int(p, (int32_t)slot + 1); msg_set(b, 0);
}
static int own_free(void) { for (unsigned j = 0; j < NOWN; j++) if (!own[j].used) return (int)j; return -1; }
static void own_write(unsigned slot, unsigned p, const char *name, unsigned cat)
{
    upart_t *u = &up[p];
    str_cpy(own[slot].name, name, sizeof own[slot].name);
    own[slot].cat = (uint8_t)cat; own[slot].used = 1;
    memcpy(own[slot].v, u->v, JP_COUNT);
    u->kind = 1; u->idx = (uint8_t)slot; u->cat = (uint8_t)cat; u->cat_view = (uint8_t)cat;
    str_cpy(u->name, name, sizeof u->name);
    memcpy(u->base, u->v, JP_COUNT);
    u->dirty = 0;
    set_pack(); touch_set();
}
static void dlg_open(unsigned p);
static void save_short(unsigned p)
{
    upart_t *u = &up[p];
    int slot;
    if (!u->dirty) { msg_set("NOTHING TO SAVE", 0); return; }
    if (!u->kind && u->cat == CAT_INIT) { dlg_open(p); return; }   /* from INIT: it needs a name and a group first */
    slot = u->kind ? (int)u->idx : own_free();
    if (slot < 0) { msg_set("MEMORY FULL", 1); return; }
    own_write((unsigned)slot, p, u->name, u->cat);
    flash_own((unsigned)slot);
}
static void dlg_open(unsigned p)
{
    upart_t *u = &up[p];
    unsigned i = 0;
    int f = own_free();
    for (; i < NAMEMAX && u->name[i]; i++) dlg.name[i] = u->name[i];
    for (; i < NAMEMAX; i++) dlg.name[i] = ' ';
    dlg.name[NAMEMAX] = 0;
    dlg.cursor = 0;
    dlg.cat = u->cat < NFC ? u->cat : 0;
    dlg.slot = (uint8_t)(u->kind ? u->idx : f >= 0 ? f : 0);
    dlg.perf = 0;
    saving = 1; touched = 255;
}
static void dlg_open_perf(void)                 /* SAVE on the page SETS: the set's name and place */
{
    const char *n = perfs[perf_cur].used ? perfs[perf_cur].name : "SET";
    unsigned i = 0;
    for (; i < NAMEMAX && n[i]; i++) dlg.name[i] = n[i];
    if (!perfs[perf_cur].used && i + 3u <= NAMEMAX) { dlg.name[i++] = ' '; dlg.name[i++] = (char)('0' + (perf_cur + 1u) / 10u); dlg.name[i++] = (char)('0' + (perf_cur + 1u) % 10u); }
    for (; i < NAMEMAX; i++) dlg.name[i] = ' ';
    dlg.name[NAMEMAX] = 0;
    dlg.cursor = 0; dlg.slot = perf_cur; dlg.perf = 1;
    saving = 1; touched = 255;
}
static void dlg_ok(unsigned p)
{
    char n[NAMEMAX + 1];
    unsigned len = NAMEMAX;
    str_cpy(n, dlg.name, sizeof n);
    while (len && n[len - 1] == ' ') n[--len] = 0;
    if (!len) str_cpy(n, "NO NAME", sizeof n);
    if (dlg.perf) {                             /* a set: the setup as it is, in its place (the sound stalls ~50 ms) */
        perf_take(&perfs[dlg.slot]);
        str_cpy(perfs[dlg.slot].name, n, sizeof perfs[dlg.slot].name);
        perf_cur = dlg.slot;
        if (!flash_ok || blob_save(1, perfs, sizeof perfs)) msg_set("SAVE FAILED", 1); else perf_msg("SAVED", dlg.slot);
        saving = 0;
        return;
    }
    own_write(dlg.slot, p, n, dlg.cat);
    flash_own(dlg.slot);
    saving = 0;
}

/* ---- the load numbers (SYSTEM page): what the measuring firmware showed (docs/adr/0003) */
static uint32_t avg[M_N], peak[M_N], max_all, late_blocks;
static uint32_t pct(uint32_t ticks) { return (ticks * 100u + BLOCK_TICKS / 2u) / BLOCK_TICKS; }

/* ======================================================================================================
 * The screen
 * ====================================================================================================== */
static int32_t shown_q8[4];                     /* the bars glide to their values */
static uint32_t frame_no;
static int8_t scope_shot[240];
static uint8_t voices_now[JX_PARTS];
static uint8_t midi_seen[JX_PARTS];                    /* midi_hit as last looked at, and when it changed */
static uint32_t midi_t[JX_PARTS];

static void draw_header(void)               /* the four parts, always: letter, sound, sounding voices, unsaved */
{
    for (unsigned i = 0; i < JX_PARTS; i++) {
        int32_t x = (int32_t)i * 60;
        char s[2] = {(char)('A' + i), 0}, n[11];
        uint16_t fg = i == sel ? T_BG : T_DIM;
        str_cpy(n, up[i].name, sizeof n);
        if (i == sel) R(x + 1, 1, 58, 21, T_SEL); else FRAME(x + 1, 1, 58, 21, T_LINE);
        P(x + 5, 4, 2, s, fg);
        {                                       /* the name: one line of 9, or two, parted at a space */
            unsigned len = str_len(n), cut = len, sp = 0;
            for (unsigned j = 1; j < 9u && j < len; j++) if (n[j] == ' ') sp = j;
            if (len > 9u) cut = sp ? sp : len == 10u ? len : 9u;   /* ten letters without a space: one line, a pixel to the left */
            if (cut == len && len == 10u) Q(x + 19, 8, 1, n, fg);
            else if (cut < len) {
                Q(x + 20, 12, 1, n + cut + (n[cut] == ' '), fg);
                n[cut] = 0;
                Q(x + 20, 4, 1, n, fg);
            } else Q(x + 20, 8, 1, n, fg);
        }
        if (up[i].dirty) R(x + 52, 4, 4, 4, i == sel ? T_BG : T_WARN);
        if (fm1_ms - midi_t[i] < 150u && midi_t[i]) R(x + 50, 14, 6, 6, T_LIVE);   /* MIDI came in for this part */
        for (unsigned k = 0; k < voices_now[i] && k < 6u; k++) R(x + 3 + (int32_t)k * 9, 24, 7, 2, T_LIVE);
    }
}

static void draw_sound_line(int32_t y)          /* on the editing pages: has this sound been changed? */
{
    if (up[sel].dirty) { R(180, y + 2, 6, 6, T_WARN); QR(238, y, 2, "EDITED", T_WARN); }
}

/* ---- the stages */
/* ---- what moves in the pictures: the newest voice's envelope (a dot on its curve) and the LFO's phase (a dot on its
 * wave, running on at its rate while nothing sounds) */
enum { EV_IDLE, EV_T1, EV_T2, EV_T3, EV_SUS, EV_REL };   /* (the engines' envelope stages, syn_dsp.h SYN_ENV_*) */
static void env_dot(uint8_t stage, float prog, const int32_t *xs, const int32_t *ys)
{   /* xs, ys: the corners of the drawn curve (start, ends of T1, T2, T3, the sustain's end, the release's end); the dot
     * sits on the drawn line at the stage's share of its time (the engine's own level bends and starts the release
     * where it is, so it would leave the line) */
    int32_t i, x, y;
    if (prog < 0) prog = 0;
    if (prog > 1) prog = 1;
    switch (stage) {
    case EV_T1: i = 0; break;
    case EV_T2: i = 1; break;
    case EV_T3: i = 2; break;
    case EV_SUS: i = 3; prog = 0.5f; break;
    case EV_REL: i = 4; break;
    default: return;
    }
    x = xs[i] + (int32_t)((float)(xs[i + 1] - xs[i]) * prog);
    y = ys[i] + (int32_t)((float)(ys[i + 1] - ys[i]) * prog);
    R(x - 3, y - 3, 7, 7, T_BG); R(x - 2, y - 2, 5, 5, T_INK);
}
static const float lfo_quarters[14] = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3, 1, 1.5f, 2, 4, 8, 16};
static float lfo_dot_phase(float engine_phase, int live, int sync, unsigned div, float hz)
{
    static float ph;
    static uint32_t t0;
    uint32_t now = fm1_ms, dt = now - t0;
    t0 = now;
    if (dt > 200u) dt = 0;
    if (live) ph = engine_phase;            /* sounding: the LFO itself */
    else {                                  /* silent (the engine's LFO stands): at its rate, or its note value */
        float f = sync ? bpm_now * (1.0f / 60.0f) / lfo_quarters[div < 14u ? div : 13u] : hz;
        ph += f * (float)dt * 0.001f;
        ph -= (float)(int)ph;
    }
    return ph;
}
static const jx_voice_t *jx_voice_now(void)   /* the selected part's newest sounding voice (JX-8P) */
{
    const jx_voice_t *b = 0;
    for (unsigned i = 0; i < synth.pool; i++) { const jx_voice_t *v = &synth.voice[i]; if (v->active && v->part == sel && (!b || v->age > b->age)) b = v; }
    return b;
}
/* the filter as it sounds: the newest voice's ladder coefficient g = tan(pi fc / fs) back to fc, then to the CUTOFF
 * knob's scale (the engines' cutoff_oct), so the picture's line stands where the filter is (envelope, LFO, key) */
static float log2_ui(float x)              /* (x > 0: fc is at least 8 Hz) */
{
    union { float f; uint32_t u; } c = {x};
    float e = (float)((int32_t)((c.u >> 23) & 255u) - 127), m;
    c.u = (c.u & 0x7FFFFFu) | 0x3F800000u;
    m = c.f;
    return e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f;
}
static float fc_of_g(float g)              /* atan (g) * fs / pi, g > 0 */
{
    float a = g <= 1.0f ? g / (1.0f + 0.28f * g * g) : 1.5707963f - (1.0f / g) / (1.0f + 0.28f / (g * g));   /* (g > 1 here) */
    return a * (EL_SAMPLE_RATE / 3.14159265f);
}
static int32_t cut_x(float knob)           /* the knob's value (0..127) as the picture's x */
{
    int32_t x = 24 + (int32_t)(knob * 190.0f / 127.0f);
    return x < 0 ? 0 : x > 214 ? 214 : x;   /* (as far as the knob's curve goes: its slope ends at the edge) */
}
static void cut_live(int32_t xl, int32_t xc, int32_t y0, int32_t pk, int32_t yb)
{   /* the low pass where it is now, in green over the knob's (orange): shoulder, peak, slope; opened further than
     * the knob, its flat part runs on over the knob's shoulder */
    int32_t a = xl - 12 > 0 ? xl - 12 : 0;
    if (xl - xc < 2 && xc - xl < 2) return;    /* (unmodulated: the knob's curve is the filter) */
    if (xl > xc) LN(xc - 12 > 0 ? xc - 12 : 0, y0, a, y0, T_GREEN);
    LN(a, y0, xl, y0 - pk, T_GREEN);
    LN(xl, y0 - pk, xl + 22, yb, T_GREEN);
}
static void stage_home(void)
{
    upart_t *u = &up[sel];
    char b[24], *p;
    int pos = cat_pos(u->cat_view, u->kind, u->idx);
    p = put(b, cat_name[u->cat_view]); p = put(p, "  ");
    if (pos + 1 < 10) *p++ = '0';
    fmt_int(p, pos + 1); p += str_len(p); p = put(p, "/"); fmt_int(p, (int32_t)cat_count(u->cat_view));
    Q(4, 34, 2, b, T_DIM);
    draw_sound_line(34);
    P(2, 52, 4, u->name, T_INK);
    /* the scope */
    R(0, 120, 240, 1, T_LINE);
    for (int32_t x = 0; x < 239; x++) {
        int32_t y0 = 120 - scope_shot[x] * 28 / 128, y1 = 120 - scope_shot[x + 1] * 28 / 128;
        LN(x, y0, x + 1, y1, T_LIVE);
    }
}

static void stage_parts(void)                   /* MIX, MIDI: the four parts in a list */
{
    for (unsigned i = 0; i < JX_PARTS; i++) {
        int32_t y = 34 + (int32_t)i * 29;
        char s[12], *p = s;
        s[0] = (char)('A' + i); s[1] = 0;
        if (i == sel) R(0, y, 3, 22, T_SEL);
        P(10, y + 4, 2, s, i == sel ? T_SEL : T_DIM);
        P(32, y + 4, 2, up[i].name, T_INK);
        if (up[i].chan) { p = put(p, "CH"); fmt_int(p, up[i].chan); } else str_cpy(s, "OMNI", sizeof s);
        QR(238, y + 6, 2, s, T_DIM);
        if (up[i].dirty) R(160, y + 8, 6, 6, T_WARN);
    }
}

/* the DCOs' waves as the sound sets them (a preview: the shapes of engine/jx.c without its band limiting):
 * wave 0 NOISE, 1 SQUARE, 2 PULSE, 3 SAW; ph 0..1, out -1..1 */
static float dco_wave(unsigned wave, float ph, uint32_t *rng)
{
    switch (wave) {
    case 0: *rng = *rng * 1103515245u + 12345u; return (float)(int32_t)(*rng >> 16 & 255u) / 128.0f - 1.0f;
    case 1: return ph < 0.5f ? 1.0f : -1.0f;
    case 2: return ph < 0.25f ? 1.0f : -1.0f;
    default: return 1.0f - 2.0f * ph;
    }
}
static float dco_cycles(unsigned range, unsigned tune)       /* cycles in the picture: 16' two, each octave twice as many */
{
    static const float semi[25] = {0.500f, 0.530f, 0.561f, 0.595f, 0.630f, 0.667f, 0.707f, 0.749f, 0.794f, 0.841f,
                                   0.891f, 0.944f, 1.000f, 1.059f, 1.122f, 1.189f, 1.260f, 1.335f, 1.414f, 1.498f,
                                   1.587f, 1.682f, 1.782f, 1.888f, 2.000f};
    return (float)(2u << (range & 3u)) * semi[tune < 25u ? tune : 12u];
}
/* both DCOs over the picture: DCO-2 synced to DCO-1 (SYNC1, SYNC2) or bent by it (XMOD) as the sound sets */
static void dco_trace(const uint8_t *v, float *w1, float *w2)
{
    float c1 = dco_cycles(v[JP_DCO1_RANGE], v[JP_DCO1_TUNE]), c2 = dco_cycles(v[JP_DCO2_RANGE], v[JP_DCO2_TUNE]);
    float p1 = 0, p2 = 0, d1 = c1 / 240.0f, d2 = c2 * (1.0f + ((int)v[JP_DCO2_FINE] - 64) * 0.0005f) / 240.0f;
    uint32_t r1 = 7u, r2 = 99u;
    unsigned x2 = v[JP_DCO2_XMOD];
    for (int32_t x = 0; x < 240; x++) {
        w1[x] = dco_wave(v[JP_DCO1_WAVE], p1, &r1);
        w2[x] = dco_wave(v[JP_DCO2_WAVE], p2, &r2);
        p1 += d1;
        p2 += x2 == 3u ? d2 * (1.0f + 0.8f * w1[x]) : d2;
        if (p1 >= 1.0f) { p1 -= 1.0f; if (x2 == 1u || x2 == 2u) p2 = 0; }
        while (p2 >= 1.0f) p2 -= 1.0f;
    }
}
static void trace(const float *w, int32_t y0, int32_t amp, uint16_t c)
{
    int32_t py = 0;
    for (int32_t x = 0; x < 240; x++) {
        int32_t y = y0 - (int32_t)(w[x] * (float)amp);
        if (x && y != py) R(x, y < py ? y : py, 1, (y < py ? py - y : y - py) + 1, c);
        R(x, y, 1, 2, c);
        py = y;
    }
}
static float w_a[240], w_b[240];
static void stage_dco(int which)                /* 1: DCO-1 lit; 2: DCO-2 lit over DCO-1 dim */
{
    const uint8_t *v = up[sel].v;
    static const char *const xm[4] = {"", "SYNC1", "SYNC2", "X-MOD"};
    draw_sound_line(32);
    dco_trace(v, w_a, w_b);
    R(0, 96, 240, 1, T_LINE);
    if (which == 1) trace(w_a, 96, 30, T_BLUE);
    else {
        trace(w_a, 96, 30, T_LINE);
        trace(w_b, 96, 30, T_BLUE);
        if (v[JP_DCO2_XMOD]) QR(238, 140, 2, xm[v[JP_DCO2_XMOD] & 3u], T_BLUE);
    }
}
static void stage_mix(void)                     /* what the mixer makes of both, with each one's share */
{
    const uint8_t *v = up[sel].v;
    float a = v[JP_MIX_DCO1] / 127.0f, b = v[JP_MIX_DCO2] / 127.0f, n = a + b > 1.0f ? a + b : 1.0f;
    draw_sound_line(32);
    dco_trace(v, w_a, w_b);
    for (int32_t x = 0; x < 240; x++) w_a[x] = (w_a[x] * a + w_b[x] * b) / n;   /* (n >= 1) */
    R(0, 92, 240, 1, T_LINE);
    trace(w_a, 92, 30, T_BLUE);
    R(4, 136, v[JP_MIX_DCO1] * 100 / 127, 4, T_BLUE); Q(4, 142, 1, "DCO-1", T_DIM);
    R(124, 136, v[JP_MIX_DCO2] * 100 / 127, 4, T_BLUE); Q(124, 142, 1, "DCO-2", T_DIM);
}
/* an ADSR of the sound, x0..x0+w, base yb, height h; down: upside down (an ENV MODE "DOWN") */
static void adsr(unsigned e, int32_t x0, int32_t yb, int32_t h, int down, uint16_t c)
{
    const uint8_t *v = up[sel].v;
    unsigned b = e == 1u ? JP_E1_ATTACK : JP_E2_ATTACK;
    int32_t wa = 4 + v[b] * 50 / 127, wd = 4 + v[b + 1] * 50 / 127, ws = 40, wr = 4 + v[b + 3] * 60 / 127;
    int32_t s = v[b + 2] * h / 127, x1 = x0 + wa, x2 = x1 + wd, x3 = x2 + ws, x4 = x3 + wr;
    int32_t top = down ? yb + h : yb - h, ys = down ? yb + s : yb - s;
    LN(x0, yb, x1, top, c); LN(x1, top, x2, ys, c); LN(x2, ys, x3, ys, c); LN(x3, ys, x4, yb, c);
    R(x1 - 1, top - 1, 3, 3, T_ORANGE); R(x2 - 1, ys - 1, 3, 3, T_ORANGE); R(x3 - 1, ys - 1, 3, 3, T_ORANGE);
    {   /* where the newest voice's envelope is (an ADSR: T3 is the step to the sustain, at its start) */
        const jx_voice_t *nv = jx_voice_now();
        const int32_t xs[6] = {x0, x1, x2, x2, x3, x4}, yc[6] = {yb, top, ys, ys, ys, yb};
        if (nv && c != T_DIM) { const syn_env_t *ev = e == 1u ? &nv->env1 : &nv->env2; env_dot(ev->stage, ev->progress, xs, yc); }
    }
}
static void stage_env(unsigned e)
{
    draw_sound_line(32);
    R(0, 140, 240, 1, T_LINE);
    adsr(e, 8, 140, 84, 0, T_SEL);
}
static void stage_envs(void)                    /* both envelopes: ENV-1 lit, ENV-2 dim */
{
    draw_sound_line(32);
    R(0, 140, 240, 1, T_LINE);
    adsr(2, 8, 140, 84, 0, T_DIM);
    adsr(1, 8, 140, 84, 0, T_SEL);
    Q(4, 144, 1, "ENV-1", T_SEL); Q(40, 144, 1, "ENV-2", T_DIM);
}
static void stage_dcomod(void)                  /* the envelope that bends the DCOs' pitch, and how far for each */
{
    const uint8_t *v = up[sel].v;
    unsigned m = v[JP_DCO_ENV_MODE] & 3u;       /* 2 DOWN, 2 UP, 1 DOWN, 1 UP */
    static const char *const nm[4] = {"ENV-2 DOWN", "ENV-2 UP", "ENV-1 DOWN", "ENV-1 UP"};
    draw_sound_line(32);
    R(0, 96, 240, 1, T_LINE);
    adsr(m < 2u ? 2u : 1u, 8, 96, 40, !(m & 1u), T_GREEN);
    Q(4, 34, 2, nm[m], T_DIM);
    R(4, 136, v[JP_DCO1_ENV] * 100 / 127, 4, T_GREEN); Q(4, 142, 1, "DCO-1", T_DIM);
    R(124, 136, v[JP_DCO2_ENV] * 100 / 127, 4, T_GREEN); Q(124, 142, 1, "DCO-2", T_DIM);
}
static void stage_filter(void)                  /* HPF (4 steps), the low pass with its peak, the envelope's reach */
{
    const uint8_t *v = up[sel].v;
    int32_t xc = 24 + v[JP_CUTOFF] * 190 / 127, pk = v[JP_RESO] * 30 / 127, y0 = 96, yb = 140;
    int32_t xh = (int32_t)v[JP_HPF] * 20, xs = xc - 12 > 0 ? xc - 12 : 0;
    draw_sound_line(32);
    R(0, yb, 240, 1, T_LINE);
    if (xh > xs) xh = xs;
    if (xh > 0) LN(0, yb, xh, y0, T_ORANGE);
    LN(xh, y0, xs, y0, T_ORANGE);
    LN(xs, y0, xc, y0 - pk, T_ORANGE);
    LN(xc, y0 - pk, xc + 22, yb, T_ORANGE);
    R(xc, y0 - pk - 3, 1, yb - (y0 - pk) + 3, T_LINE);
    if (v[JP_VCF_ENV]) { int32_t w = v[JP_VCF_ENV] * 60 / 127; R(xc, 146, w, 2, T_GREEN); }
    {   /* where the filter is now (the newest voice), cutoff_oct = knob / 127 * 10.3 - 0.3 */
        const jx_voice_t *nv = jx_voice_now();
        if (nv && nv->ld.g > 0) cut_live(cut_x((log2_ui(fc_of_g(nv->ld.g) * (1.0f / 20.0f)) + 0.3f) * (127.0f / 10.3f)), xc, y0, pk, yb);
    }
}
/* the LFO as the engine computes it (engine control(): 0 triangle, 1 sine, 2 saw up, 3 saw down, 4 square, 5 sample
 * and hold, 6 smooth random), p: 256 a cycle (two cycles drawn), out -32..32 */
static int32_t lfo_rnd(int32_t c) { static const int8_t lv[8] = {-22, 18, -6, 28, -28, 8, 24, -14}; return lv[c & 7]; }   /* (a picture: random levels that show) */
static int32_t lfo_shape(unsigned wave, int32_t p)
{
    int32_t ph = p & 255, t = ph & 127;
    switch (wave) {
    case 1: {   /* sin: Bhaskara's form on each half (error under 0.2 %) */
        int32_t q = t * (128 - t), s = 16 * q * 32 / (5 * 16384 - 4 * q);   /* (the divisor is at least 65536) */
        return ph < 128 ? s : -s;
    }
    case 2: return ph / 4 - 32;
    case 3: return 32 - ph / 4;
    case 4: return ph < 128 ? 32 : -32;
    case 5: return lfo_rnd(p >> 8);              /* one new value a cycle */
    case 6: {                                    /* from one random value to the next, eased (the engine's cosine) */
        int32_t a = lfo_rnd(p >> 8), b = lfo_rnd((p >> 8) + 1), f = ph, e = f * f * (768 - 2 * f) / 65536;   /* 3f^2 - 2f^3 on 0..256 */
        return a + (b - a) * e / 256;
    }
    default: return 32 - (ph > 128 ? ph - 128 : 128 - ph) / 2;   /* 1 - 4 |ph - 0.5|: -1 at 0, +1 at the half */
    }
}
static void stage_lfo(void)
{
    const uint8_t *v = up[sel].v;
    static const uint8_t jx_wave[3] = {5, 4, 1};                /* the tone's LFO WAVE: RANDOM, SQUARE, SINE */
    static const uint8_t shape[8] = {0, 0, 1, 2, 3, 4, 5, 6};   /* LFO SHAPE: JX, TRI, SINE, SAW UP, SAW DN, SQUARE, S&H, RANDOM */
    unsigned w = v[JP_LFO_SHAPE] ? shape[v[JP_LFO_SHAPE] & 7u] : jx_wave[v[JP_LFO_WAVE] < 3u ? v[JP_LFO_WAVE] : 2u];
    int32_t py = 0, dot = (int32_t)(lfo_dot_phase(synth.part[sel].lfo_phase, voices_now[sel] > 0, v[JP_LFO_SYNC], v[JP_LFO_DIV], jx_lfo_hz(v[JP_LFO_RATE])) * 120.0f);   /* (the LFO itself: the picture shows two cycles) */
    draw_sound_line(32);
    R(0, 96, 240, 1, T_LINE);
    for (int32_t x = 0; x < 240; x++) {
        int32_t y = 96 - lfo_shape(w, x * 512 / 240);
        if (x && y != py) R(x, y < py ? y : py, 1, (y < py ? py - y : y - py) + 1, T_GREEN);
        R(x, y, 1, 2, T_GREEN);
        if (x == dot) R(x - 2, y - 2, 5, 5, T_SEL);
        py = y;
    }
    if (!v[JP_LFO_SYNC]) {                       /* free: its rate */
        char b[16];
        fmt_hz(b, jx_lfo_hz(v[JP_LFO_RATE]));
        str_cpy(b + str_len(b), " HZ", 4);
        Q(4, 140, 2, b, T_DIM);
    } else {                                    /* synced: its note value and the tempo it follows */
        char b[32], *p = put(b, "SYNC ");
        p = put(p, jx_param_value_name(JP_LFO_DIV, v[JP_LFO_DIV])); p = put(p, "  ");
        fmt_int(p, (int32_t)(bpm_now + 0.5f)); p += str_len(p);
        p = put(p, ext_follow ? " MIDI" : " BPM");
        if (v[JP_LFO_KEYSYNC]) p = put(p, "  KEY");
        *p = 0;
        Q(4, 140, 2, b, ext_follow ? T_GREEN : T_DIM);
    }
}
static void stage_amp(void)                    /* ENV-2: its shape; GATE: on while held */
{
    const uint8_t *v = up[sel].v;
    if (v[JP_VCA_ENV_MODE] == 0) {
        int32_t h = 30 + v[JP_VCA_LEVEL] * 50 / 127;
        draw_sound_line(32);
        R(0, 140, 240, 1, T_LINE);
        LN(20, 140, 22, 140 - h, T_SEL); LN(22, 140 - h, 200, 140 - h, T_SEL); LN(200, 140 - h, 204, 140, T_SEL);
        Q(26, 144, 1, "GATE", T_DIM);
    } else stage_env(2);
}
static void stage_chorus(void)                 /* the two delay lines' movement, opposite in phase */
{
    const uint8_t *v = up[sel].v;
    unsigned m = v[JP_CHORUS_MODE], c = v[JP_CHORUS];
    int32_t depth = m == 1 ? 16 : m == 2 ? 14 : c == 0 ? 0 : c == 1 ? 18 : 14;
    int32_t speed = m == 1 ? 3 : m == 2 ? 1 + v[JP_CHORUS_RATE] / 20 : c == 2 ? 2 : 1;
    draw_sound_line(32);
    R(0, 96, 240, 1, T_LINE);
    for (int32_t x = 0; x < 240; x += 2) {
        int32_t ph = (int32_t)((uint32_t)(x * 3 + (int32_t)frame_no * speed) & 255u), t = ph < 128 ? ph : 255 - ph;
        int32_t y = (t - 64) * depth / 64;
        R(x, 96 - y, 2, 2, T_VIOLET);
        R(x, 96 + y, 2, 2, T_DIM);
    }
    if (!depth) PC(120, 88, 2, "OFF", T_DIM);
    { char b[24], *p = put(b, "SEND  DLY "); fmt_int(p, v[JP_DELAY_SEND]); p += str_len(p); p = put(p, "  REV "); fmt_int(p, v[JP_REVERB_SEND]); Q(4, 140, 2, b, T_DIM); }
}
static void stage_delay(void)                  /* the echoes: spaced by the time, falling with the feedback */
{
    float sec = syn_fx_delay_seconds(fxv[FX_DLY_TIME], fxv[FX_DLY_SYNC], fxv[FX_DLY_DIV], bpm_now);
    int32_t dx = (int32_t)(sec * 260.0f), h = 88, x = 8, n = 0;
    float fb = fxv[FX_DLY_FB] / 127.0f, a = 1.0f;
    char b[24], *p;
    if (dx < 6) dx = 6;
    if (sec > 0.70f) sec = 0.70f;
    R(0, 140, 240, 1, T_LINE);
    R(x, 140 - h, 6, h, T_SEL);
    for (x += dx; x < 236 && n < 24; x += dx, n++) {
        int32_t hh = (int32_t)(h * a * (fxv[FX_DLY_LEVEL] / 127.0f) + 0.5f), up_ = fxv[FX_DLY_PING] && (n & 1);
        if (hh < 1) break;
        R(x, up_ ? 140 - hh / 2 : 140 - hh, 6, up_ ? hh / 2 : hh, up_ ? T_DIM : T_VIOLET);
        a *= fb < 0.5f ? fb * 0.95f + 0.05f : 0.5f + (fb - 0.5f);
    }
    p = put(b, fxv[FX_DLY_SYNC] ? syn_fx_value_name(FX_DLY_DIV, fxv[FX_DLY_DIV]) : "FREE"); p = put(p, "  ");
    fmt_int(p, (int32_t)(sec * 1000.0f + 0.5f)); p += str_len(p); *put(p, " MS") = 0;
    Q(4, 34, 2, b, T_DIM);
    if (!fxv[FX_DLY_LEVEL]) QR(238, 34, 2, "OFF", T_ORANGE);
}
static void stage_reverb(void)                 /* the tail: pre-delay, then a fall as long as the decay */
{
    int32_t pre = fxv[FX_REV_TYPE] == 0 ? fxv[FX_REV_SIZE] * fxv[FX_REV_SIZE] / 400 : fxv[FX_REV_TYPE] == 1 ? fxv[FX_REV_SIZE] / 12 : 1;
    int32_t len = 30 + fxv[FX_REV_DECAY] * 190 / 127, top = 40 + fxv[FX_REV_LEVEL] * 50 / 127;
    uint32_t r = 12345u;
    R(0, 140, 240, 1, T_LINE);
    R(4, 140 - 90, 4, 90, T_SEL);
    for (int32_t x = 0; x < len && 10 + pre + x < 238; x += 2) {
        int32_t hh = top * (len - x) / len;
        r = r * 1103515245u + 12345u;
        hh = hh * (int32_t)(160u + (r >> 24) % 96u) / 256;
        R(10 + pre + x, 140 - hh, 2, hh, x & 2 ? T_VIOLET : T_DIM);
    }
    Q(4, 34, 2, syn_fx_value_name(FX_REV_TYPE, fxv[FX_REV_TYPE]), T_DIM);
    if (!fxv[FX_REV_LEVEL]) QR(238, 34, 2, "OFF", T_ORANGE);
}
static void stage_arp(void)                    /* the pattern's notes, the one playing lit */
{
    unsigned n = arp_view_n[sel], at = arp_view_at[sel];
    char b[32], *p;
    p = put(b, arpc[sel][ARP_ON] ? "ON  " : "OFF  "); p = put(p, arp_modes[arpc[sel][ARP_MODE] % 5]); p = put(p, "  ");
    p = put(p, syn_fx_value_name(FX_DLY_DIV, arpc[sel][ARP_DIV])); if (arpc[sel][ARP_HOLD]) p = put(p, "  HOLD"); *p = 0;
    Q(4, 34, 2, b, arpc[sel][ARP_ON] ? T_SEL : T_DIM);
    { char t[12]; fmt_int(t, (int32_t)(bpm_now + 0.5f)); str_cpy(t + str_len(t), clk_running ? " MIDI" : " BPM", 6); QR(238, 34, 2, t, T_DIM); }
    R(0, 140, 240, 1, T_LINE);
    if (!n) { PC(120, 84, 2, arpc[sel][ARP_ON] ? "PLAY KEYS" : "ARP OFF", T_LINE); return; }
    {
        int lo = 127, hi = 0;
        int32_t w = 232 / (int32_t)n;
        for (unsigned i = 0; i < n; i++) { if (arp_view[sel][i] < lo) lo = arp_view[sel][i]; if (arp_view[sel][i] > hi) hi = arp_view[sel][i]; }
        for (unsigned i = 0; i < n; i++) {
            int32_t y = hi > lo ? 132 - (arp_view[sel][i] - lo) * 80 / (hi - lo) : 92;
            R(4 + (int32_t)i * w, y, w - 2, 6, i == at ? T_SEL : T_DIM);
        }
    }
}
static void stage_chord(void)                  /* the chord's notes on a keyboard, from C */
{
    int c = up[sel].v[JP_KEY_MODE] == 1 ? up[sel].v[JP_CHORD] & 7 : -1;
    static const int8_t ch[8][5] = {{0, 3, 7, 10, -1}, {0, 2, 7, -1}, {0, 3, 7, -1}, {0, 7, 12, -1},
                                    {0, 5, 7, -1}, {0, 4, 7, 10, -1}, {0, 4, 7, -1}, {0, 3, 6, 10, -1}};
    static const uint8_t black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    int32_t wx = 0;
    uint8_t on[25] = {0};
    if (c >= 0) for (int i = 0; i < 5 && ch[c][i] >= 0; i++) on[ch[c][i]] = 1;
    else on[0] = 1;
    for (int k = 0; k < 25; k++) if (!black[k % 12]) { R(4 + wx * 16, 60, 15, 70, on[k] ? T_SEL : T_LINE); wx++; }
    wx = 0;
    for (int k = 0; k < 25; k++) {
        if (black[k % 12]) R(4 + wx * 16 - 5, 60, 10, 42, on[k] ? T_SEL : T_BG);
        else wx++;
    }
    Q(4, 34, 2, c >= 0 ? "ONE KEY PLAYS THE CHORD" : "CHORD OFF: ONE NOTE A KEY", T_DIM);
}
static void stage_voice(void)                  /* how the six voices play: poly, chord, mono, unison */
{
    int m = up[sel].v[JP_KEY_MODE];
    static const char *const nm[4] = {"POLY", "CHORD", "MONO", "UNISON"};
    draw_sound_line(32);
    for (int i = 0; i < 6; i++) {
        int lit = m == 2 ? i == 0 : 1;
        R(12 + i * 37, 70, 30, 30, lit ? T_BLUE : T_LINE);
        if (m == 3 && i < 5) R(42 + i * 37, 84, 7, 2, T_BLUE);
    }
    PC(120, 116, 2, nm[m & 3], T_SEL);
}
/* the sequence: a step a column, its note's height, a rest a dot below, a tie joined to the next; the step playing
 * lit, while recording the place of the next */
static uint8_t rec_on, rec_part, rec_cur, rec_key = 0xff, rec_note = 0xff;   /* step recording (REC, docs/adr/0015) */
static void stage_seq(void)
{
    unsigned n = rec_on && rec_part == sel ? rec_cur : sq.len[sel], at = sq_view_at[sel], cols = n > 16u ? n : 16u;
    int lo = 127, hi = 0;
    int32_t w = 232 / (int32_t)cols;
    char b[32], *q;
    if (rec_on && rec_part == sel) { q = put(b, "REC  STEP "); fmt_int(q, (int32_t)rec_cur + 1); q += str_len(q); q = put(q, "  TOP KEY: REST"); *q = 0; Q(4, 34, 2, b, T_ACC); }
    else {
        q = b; fmt_int(q, (int32_t)n); q += str_len(q); q = put(q, n == 1u ? " STEP  " : " STEPS  ");
        q = put(q, syn_fx_value_name(FX_DLY_DIV, sq.cfg[sel][SQ_DIV]));
        if (sq.cfg[sel][SQ_SHIFT] != 24) { int t = (int)sq.cfg[sel][SQ_SHIFT] - 24; q = put(q, "  "); if (t >= 0) *q++ = '+'; fmt_int(q, t); q += str_len(q); }
        *q = 0;
        Q(4, 34, 2, b, sq.cfg[sel][SQ_ON] ? T_SEL : T_DIM);
    }
    R(0, 140, 240, 1, T_LINE);
    if (!n) { PC(120, 84, 2, rec_on && rec_part == sel ? "PLAY THE STEPS" : "REC: RECORD STEPS", T_LINE); return; }
    for (unsigned i = 0; i < n; i++) if (!(sq.flag[sel][i] & SQ_REST)) { if (sq.note[sel][i] < lo) lo = sq.note[sel][i]; if (sq.note[sel][i] > hi) hi = sq.note[sel][i]; }
    for (unsigned i = 0; i < n; i++) {
        int32_t x = 4 + (int32_t)i * w, y;
        int on = sq.cfg[sel][SQ_ON] && i == at;   /* (while recording too, when it plays on) */
        if (sq.flag[sel][i] & SQ_REST) { R(x + w / 2 - 1, 134, 2, 2, T_DIM); continue; }
        y = hi > lo ? 128 - (sq.note[sel][i] - lo) * 76 / (hi - lo) : 90;
        R(x, y, (sq.flag[sel][i] & SQ_TIE) ? w : w - 1, 5, on ? T_SEL : T_VIOLET);
        if ((sq.flag[sel][i] & SQ_TIE) && i + 1u < n && !(sq.flag[sel][i + 1u] & SQ_REST)) {   /* tied: joined to the next */
            int32_t y2 = hi > lo ? 128 - (sq.note[sel][i + 1u] - lo) * 76 / (hi - lo) : 90;
            R(x + w - 1, y < y2 ? y : y2, 2, (y < y2 ? y2 - y : y - y2) + 5, T_VIOLET);
        }
    }
    if (rec_on && rec_part == sel && n < SEQ_STEPS) R(4 + (int32_t)n * w, 136, w > 2 ? w - 1 : 1, 3, T_ACC);
}
/* FM1-Electro-one: the live recording (ADR 0053): bar and length large, the state above */
static void stage_live(void)
{
    unsigned bar, bars, st;
    char b[24], *p;
    lv_status(sel, &bar, &bars, &st);
    draw_sound_line(32);
    if (st == LV_COUNT) { PC(120, 70, 4, "COUNT IN", T_WARN); return; }
    p = b;
    if (st == LV_FIRST) { fmt_int(p, (int32_t)bar); p += str_len(p); *p = 0; }
    else if (bars) { fmt_int(p, (int32_t)(bar > bars ? bars : bar)); p += str_len(p); p = put(p, "/"); fmt_int(p, (int32_t)bars); p += str_len(p); *p = 0; }
    else { p = put(p, "EMPTY"); *p = 0; }
    PC(120, 62, 5, b, st != LV_OFF ? T_WARN : bars && lv_store.p[sel].on ? T_INK : T_LINE);
    Q(4, 36, 2, st == LV_FIRST ? "REC: REC = LENGTH" : st == LV_OVER ? "OVERDUB  PLAY = UNDO" : bars ? "BARS  REC = OVERDUB" : "REC = RECORD", st != LV_OFF ? T_WARN : T_DIM);
}
static void stage_tempo(void)                   /* the tempo, where it comes from, the transport, the beat */
{
    char b[24], *p;
    uint32_t grid = ext_follow || xport == 1;
    uint32_t lit = grid ? clk_ticks % 24u < 4u : (fm1_ms % (uint32_t)(60000.0f / bpm_now)) < 90u;
    fmt_int(b, (int32_t)(bpm_now + 0.5f));
    PC(120, 40, 6, b, T_SEL);
    PC(120, 96, 2, ext_follow ? "MIDI CLOCK" : clkv[0] == SRC_MIDI ? "WAITING FOR CLOCK" : "INTERNAL", ext_follow ? T_GREEN : T_DIM);
    p = put(b, xport == 1 ? "PLAY" : xport == 2 ? "STOP" : "FREE");
    if (grid) { uint32_t bar = clk_ticks / 96u + 1u, beat = clk_ticks / 24u % 4u + 1u; p = put(p, "  "); fmt_int(p, (int32_t)bar); p += str_len(p); *p++ = '.'; fmt_int(p, (int32_t)beat); p += str_len(p); }
    *p = 0;
    QC(120, 118, 2, b, xport == 1 ? T_SEL : T_DIM);
    for (int i = 0; i < 4; i++) R(84 + i * 20, 136, 14, 6, grid && (int)(clk_ticks / 24u % 4u) == i ? T_VIOLET : lit && !grid ? T_VIOLET : T_LINE);
    if (clkv[1]) Q(4, 140, 1, "SENDS CLOCK: USB", T_DIM);
}

static void stage_name(const page_t *pg)        /* pages without a picture: their name, large and quiet */
{
    draw_sound_line(32);
    PC(120, 76, 4, pg->title, T_LINE);
}
static void stage_sys(void)
{
    char b[40], *p;
    static const char *const nm[3] = {"TOTAL ", "CORE 0", "CORE 1"};
    p = put(b, "V" APP_VER); while (p < b + 19) *p++ = ' ';   /* the version, then the columns */
    p = put(p, "AVG  PEAK"); *p = 0;
    Q(4, 34, 2, b, T_DIM);
    for (unsigned i = 0; i < 3u; i++) {
        uint32_t pk = pct(peak[i]);
        p = put(b, nm[i]); p = put_n(p, pct(avg[i]), 6); p = put(p, "%"); p = put_n(p, pk, 5); p = put(p, "%"); *p = 0;
        P(4, 52 + (int32_t)i * 20, 2, b, pk >= 88u ? T_WARN : T_INK);
    }
    p = put(b, "VOICES  K0"); p = put_n(p, pct(avg[M_V0]), 3); p = put(p, "%  K1"); p = put_n(p, pct(avg[M_V1]), 3); *put(p, "%") = 0;
    Q(4, 112, 2, b, T_DIM);
    p = put(b, "CHORUS"); p = put_n(p, pct(avg[M_C0]) + pct(avg[M_C1]), 4); p = put(p, "%  DLY+REV"); p = put_n(p, pct(avg[M_FX]), 3); *put(p, "%") = 0;
    Q(4, 126, 2, b, T_DIM);
    p = put(b, "MAX"); p = put_n(p, pct(max_all), 4); p = put(p, "% LATE "); p = put_n(p, late_blocks, 0);
    p = put(p, " GAP "); p = put_n(p, gaps, 0);   /* (late interrupts: heard as clicks) */
    p = put(p, synth.cores > 1 ? "  2K" : cpu1_ok ? "  1K" : "  1K!"); *p = 0;
    Q(4, 140, 2, b, late_blocks || gaps || !cpu1_ok ? T_WARN : T_DIM);
}
static void stage_browse(void)
{
    upart_t *u = &up[sel];
    int cnt = (int)cat_count(u->cat_view), pos = cat_pos(u->cat_view, u->kind, u->idx), first;
    if (pos < 0) pos = 0;
    first = pos - 2; if (first > cnt - 5) first = cnt - 5; if (first < 0) first = 0;
    for (int i = 0; i < 5 && first + i < cnt; i++) {
        uint8_t k, x;
        int32_t y = 32 + i * 23;
        char n[4], *q = n;
        const char *name;
        if (cat_item(u->cat_view, (unsigned)(first + i), &k, &x)) break;
        name = k ? own[x].name : fac_name(x);
        if (first + i + 1 < 10) *q++ = '0';
        fmt_int(q, first + i + 1);
        if (first + i == pos) { R(0, y, 240, 20, T_SEL); Q(6, y + 5, 2, n, T_BG); P(34, y + 3, 2, name, T_BG); if (k) QR(234, y + 5, 2, "USER", T_BG); }
        else { Q(6, y + 5, 2, n, T_DIM); P(34, y + 3, 2, name, T_INK); if (k) QR(234, y + 5, 2, "USER", T_DIM); }
    }
}
static void stage_sets(void)                    /* the 8 places: number, name, tempo */
{
    int first = (int)perf_cur - 2;
    if (first > NSET - 5) first = NSET - 5;
    if (first < 0) first = 0;
    for (int i = 0; i < 5; i++) {
        unsigned x = (unsigned)(first + i);
        int32_t y = 32 + i * 23, on = x == perf_cur;
        char n[4], t[12], *q = n;
        if (x + 1u < 10u) *q++ = '0';
        fmt_int(q, (int32_t)x + 1);
        if (on) R(0, y, 240, 20, T_SEL);
        Q(6, y + 5, 2, n, on ? T_BG : T_DIM);
        if (perfs[x].used) {
            P(34, y + 3, 2, perfs[x].name, on ? T_BG : T_INK);
            fmt_int(t, perfs[x].s.tempo + 40); str_cpy(t + str_len(t), " BPM", 5);
            QR(234, y + 5, 2, t, on ? T_BG : T_DIM);
        } else Q(34, y + 5, 2, "EMPTY", on ? T_BG : T_LINE);
    }
}
static void stage_save(void)
{
    char s[2] = {0, 0};
    for (unsigned i = 0; i < NAMEMAX; i++) {
        int32_t x = 2 + (int32_t)i * 24;
        s[0] = dlg.name[i];
        if (i == dlg.cursor) { R(x - 1, 50, 22, 34, T_SEL); P(x, 53, 4, s, T_BG); }
        else { P(x, 53, 4, s, T_INK); if (s[0] == ' ') R(x, 82, 20, 2, T_LINE); }
    }
    Q(4, 34, 2, "NAME", T_DIM);
    {
        int taken = dlg.perf ? perfs[dlg.slot].used : own[dlg.slot].used;
        Q(4, 104, 2, taken ? "THIS PLACE IS TAKEN: SAVE REPLACES IT" : "THIS PLACE IS FREE", taken ? T_WARN : T_DIM);
    }
    if (dlg.perf) Q(4, 140, 2, "4 PARTS+MIDI+ARPS+FX+TEMPO", T_DIM);
    Q(4, 124, 2, "SAVE = OK       HOME = CANCEL", T_DIM);
}

/* ---- the knob cells */
static void cell(unsigned k, const char *label, const char *value, int32_t bar_q8, int bar, uint16_t col)
{
    int32_t x = (int32_t)k * 60, cx = x + 30;
    int hot = touched == k && fm1_ms - t_touch < 1200u;
    if (!label[0]) { R(x + 26, 190, 8, 2, T_LINE); return; }
    {
        int32_t ls = str_len(label) > 7u ? 1 : 2;   /* (more than 7 letters would touch the next cell) */
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
    if (dlg.perf) cell(2, "", "", 0, 0, T_SEL); else cell(2, "GROUP", cat_name[dlg.cat], 0, 0, T_SEL);
    { char *p = b; if (dlg.slot + 1u < 10u) *p++ = '0'; fmt_int(p, dlg.slot + 1); }
    cell(3, "PLACE", b, 0, 0, T_SEL);
}

static void draw_page(unsigned pi, int live)
{
    const page_t *pg = &pages[pi];
    switch (pg->stage) {
    case ST_HOME: stage_home(); break;
    case ST_PARTS: stage_parts(); break;
    case ST_DCO1: stage_dco(1); break;
    case ST_DCO2: stage_dco(2); break;
    case ST_MIX: stage_mix(); break;
    case ST_DCOMOD: stage_dcomod(); break;
    case ST_FILTER: stage_filter(); break;
    case ST_ENV1: stage_env(1); break;
    case ST_ENV2: stage_env(2); break;
    case ST_ENVS: stage_envs(); break;
    case ST_LFO: stage_lfo(); break;
    case ST_SYS: stage_sys(); break;
    case ST_BROWSE: stage_browse(); break;
    case ST_AMP: stage_amp(); break;
    case ST_CHORUS: stage_chorus(); break;
    case ST_DELAY: stage_delay(); break;
    case ST_REVERB: stage_reverb(); break;
    case ST_ARP: stage_arp(); break;
    case ST_CHORD: stage_chord(); break;
    case ST_VOICE: stage_voice(); break;
    case ST_TEMPO: stage_tempo(); break;
    case ST_SEQ: stage_seq(); break;
    case ST_LIVE: stage_live(); break;
    case ST_SETS: stage_sets(); break;
    default: stage_name(pg); break;
    }
    if (live && layer_shown()) {                /* a held button's four values, framed so it reads as a layer */
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
    if (saving) Q(4, 228, 2, dlg.perf ? "SAVE SET" : "SAVE AS", T_INK);
    if (!saving) {
        int32_t x0 = 120 - ((int32_t)n * 14 - 6) / 2;
        if (layer_shown()) { char b[24], *p = put(b, "HOLD: "); p = put(p, layers[layer].title); *p = 0; Q(4, 228, 2, b, T_ACC); }
        else Q(4, 228, 2, pages[page].title, T_INK);
        if (!layer_shown()) for (unsigned i = 0; i < n; i++) R(x0 + (int32_t)i * 14, 230, 8, 5, i == at ? T_INK : T_LINE);
        { char b[8], *p = put(b, "OCT"); if (oct >= 0) *p++ = '+'; fmt_int(p, oct); QR(236, 228, 2, b, oct ? T_INK : T_LINE); }
    }
    if (now < msg_until) {                      /* a message slides up over the footer */
        uint32_t a = now - msg_t0, b = msg_until - now;
        int32_t up_px = a < 120u ? (int32_t)a * 18 / 120 : b < 120u ? (int32_t)b * 18 / 120 : 18;
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
    else if (a < TR_MS && tr_dir) {             /* the page slides in from the side it lies on (ease out) */
        int32_t t = (int32_t)(a * 256u / TR_MS), e = 256 - (256 - t) * (256 - t) / 256, off = 240 * e / 256;
        g_ox = -tr_dir * off; draw_page(tr_from, 0);
        g_ox = tr_dir * (240 - off); draw_page(page, 0);
        g_ox = 0;
    } else draw_page(page, 1);
    draw_footer(now);
}

/* the screen in 8 bands of 30 lines; a band goes to the display only when it has changed */
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
        for (uint32_t i = 0; i < n; i++) sum = (sum ^ cv_px[i]) * 16777619u;   /* (FNV: a rotate-and-xor sum let equal changes 32 pixels apart cancel) */
        if (sum != band_sum[b]) { band_sum[b] = sum; cv_blit(0, b * 30u); }
        keys_poll();
    }
    lcd_sync();
    idle_keys = 0;
}

/* ======================================================================================================
 * The keys and the lights
 * ====================================================================================================== */
static uint32_t keys_prev;
static uint8_t held_part[27], held[27];
/* step recording as on the SH-101 (docs/adr/0015): every key is the next step; pressed while the key before is still
 * held it ties to it (legato); the top key is a rest; GLO + a key holds the note before one step more. Returns 1
 * when the key makes no sound. */
static uint32_t sq_sum_saved, sq_t_change, sq_sum_seen, lv_sum_saved;
static uint8_t rec_play, rec_mheld[16];         /* it plays on while recording (SEQ was on); MIDI notes held */
static uint8_t rec_old_len, rec_old_shift;       /* REC without a step keeps the old sequence (0.2.8) */
static int seq_rec_key(uint32_t k, uint8_t note, uint32_t keys)
{
    unsigned s = rec_cur, glo = (fm1_in.buttons >> BTN_GLO) & 1u;
    if (s >= SEQ_STEPS) { msg_set("64 STEPS: REC TO FINISH", 0); return 1; }
    if (k == 26u) {                              /* the top key: a rest */
        sq.note[rec_part][s] = 0; sq.flag[rec_part][s] = SQ_REST;
        rec_key = rec_note = 0xff;
        rec_cur++;
        return 1;
    }
    if (glo && s > 0 && rec_note != 0xff) {     /* GLO + key: the note before, one step longer */
        sq.flag[rec_part][s - 1u] |= SQ_TIE;
        sq.note[rec_part][s] = rec_note; sq.flag[rec_part][s] = 0;
        rec_cur++;
        layer_used = 1;                         /* (GLO was a modifier, not a tap) */
        return 1;
    }
    if (s > 0 && rec_key < 27u && (keys >> rec_key & 1u)) sq.flag[rec_part][s - 1u] |= SQ_TIE;   /* legato */
    sq.note[rec_part][s] = note; sq.flag[rec_part][s] = 0;
    rec_key = (uint8_t)k; rec_note = note;
    rec_cur++;
    return 0;
}
static void seq_rec(int on)                     /* REC: start (the selected part) or finish */
{
    if (on) {
        rec_on = 1; rec_part = sel; rec_cur = 0; rec_key = rec_note = 0xff;
        rec_mr = rec_mw; seq_rec_p = (uint8_t)(sel + 1u);
        rec_play = sq.cfg[sel][SQ_ON];
        rec_old_len = sq.len[sel]; rec_old_shift = sq.cfg[sel][SQ_SHIFT];
        if (rec_play) {                         /* as FM1-Electro: the new sequence plays while it grows */
            ev_put(EV_SEQ_LEN, sel, 0, 1);
            ev_put(EV_SEQ, sel, SQ_SHIFT, 24);  /* (as recorded) */
        } else ev_put(EV_SEQ, sel, SQ_ON, 0);
        for (unsigned i = 0; i < NPAGE; i++) if (pages[i].stage == ST_SEQ) { goto_page(i, 0); break; }
        msg_set("REC  KEYS = STEPS  REC = DONE", 0);
        return;
    }
    rec_on = 0; seq_rec_p = 0;
    if (!rec_cur) {                             /* nothing recorded: the old sequence stays (clear: EDIT + REC) */
        ev_put(EV_SEQ_LEN, rec_part, rec_old_len, rec_play);
        ev_put(EV_SEQ, rec_part, SQ_SHIFT, rec_old_shift);
        msg_set(rec_old_len ? "NOTHING RECORDED: SEQ KEPT" : "NOTHING RECORDED", 0);
        return;
    }
    ev_put(EV_SEQ_LEN, rec_part, rec_cur, rec_play);
    if (rec_cur) {
        char b[24], *q = b;
        ev_put(EV_SEQ, rec_part, SQ_ON, 1);
        fmt_int(q, rec_cur); q += str_len(q); q = put(q, rec_cur == 1u ? " STEP: PLAYING" : " STEPS: PLAYING"); *q = 0;
        msg_set(b, 0);
    } else msg_set("NOTHING RECORDED", 0);
}
static uint32_t sq_sum(void)                    /* (FNV of the sequences: changed since saved?) */
{
    const uint8_t *b = (const uint8_t *)&sq;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < sizeof sq; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static void seq_rec_midi(uint8_t note)         /* a MIDI note while recording: a step as a key (no rest: the top key) */
{
    unsigned s = rec_cur;
    if (s >= SEQ_STEPS) { msg_set("64 STEPS: REC TO FINISH", 0); return; }
    if ((fm1_in.buttons >> BTN_GLO) & 1u && s > 0 && rec_note != 0xff) {   /* GLO + note: the note before, one step longer */
        sq.flag[rec_part][s - 1u] |= SQ_TIE;
        sq.note[rec_part][s] = rec_note; sq.flag[rec_part][s] = 0;
        rec_cur++;
        layer_used = 1;
        return;
    }
    if (s > 0 && rec_key == 0xfe && rec_note != 0xff && (rec_mheld[rec_note >> 3] >> (rec_note & 7u) & 1u))
        sq.flag[rec_part][s - 1u] |= SQ_TIE;    /* legato */
    sq.note[rec_part][s] = note; sq.flag[rec_part][s] = 0;
    rec_key = 0xfe; rec_note = note;            /* (0xfe: from MIDI) */
    rec_cur++;
}
static void keys_poll(void)                     /* the notes: often (also while the screen draws), so a key never waits for a frame */
{
    uint8_t rec_was = rec_cur;
    while (rec_mr != rec_mw) {                  /* MIDI notes of the part that records */
        uint8_t r = rec_mr, n = rec_mn[r & 31u] & 127u, v = rec_mv[r & 31u];
        rec_mr = (uint8_t)(r + 1u);
        if (v) { if (rec_on) seq_rec_midi(n); rec_mheld[n >> 3] |= (uint8_t)(1u << (n & 7u)); }
        else rec_mheld[n >> 3] &= (uint8_t)~(1u << (n & 7u));
    }
    uint32_t keys = fm1_in.notes, down = keys & ~keys_prev, upk = keys_prev & ~keys;
    keys_prev = keys;
    for (uint32_t k = 0; k < 27u && (down | upk); k++) {
        if (down >> k & 1u) {
            held[k] = (uint8_t)(53 + 12 * oct + (int32_t)k); held_part[k] = sel;
            if (rec_on && rec_part == sel && seq_rec_key(k, held[k], keys)) continue;   /* (a rest or a held note: nothing sounds) */
            ev_put(EV_ON, sel, held[k], 100);
        }
        if (upk >> k & 1u) ev_put(EV_OFF, held_part[k], held[k], 0);
    }
    if (rec_on && rec_play && rec_cur != rec_was) ev_put(EV_SEQ_LEN, rec_part, rec_cur, 1);   /* it plays the new step */
}

/* every key and button glows at half light so it can be found; what is active is fully lit (the
 * platform's backlight layer, as FM1-Electro's docs/adr/0020 uses it: a pulse of 8 us a scan frame) */
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
    if (layer) g = layer;                       /* a held button: its light, not the page's */
    fm1_led_key(BTN_HOME, !saving && g == G_HOME); fm1_led_key(BTN_EDIT, !saving && g == G_EDIT);
    fm1_led_key(BTN_ENV, !saving && g == G_ENV); fm1_led_key(BTN_LFO, !saving && g == G_LFO);
    fm1_led_key(BTN_FX, !saving && g == G_FX); fm1_led_key(BTN_GLO, !saving && g == G_GLO);
    fm1_led_key(BTN_SEL, !saving && g == G_SEL);
    fm1_led_key(BTN_ARP, !saving && (g == G_ARP || arpc[sel][ARP_ON]));
    fm1_led_key(BTN_SEQ, !saving && (g == G_SEQ || sq.cfg[sel][SQ_ON]));
    fm1_led_key(BTN_REC, (rec_on || lv_busy()) && (fm1_ms / 300u & 1u));   /* recording: REC blinks */
    /* PLAY: while the transport runs it flashes on the beat; else it shows the arp's HOLD for the part */
    fm1_led_key(BTN_PLAY, xport == 1 ? clk_ticks % 24u < 4u : arpc[sel][ARP_HOLD]);
    fm1_led_key(BTN_SAVE, saving || (up[sel].dirty && (fm1_ms / 400u & 1u)));   /* unsaved: SAVE blinks */
}

static unsigned btn_group(uint32_t i)
{
    return i == BTN_EDIT ? G_EDIT : i == BTN_ENV ? G_ENV : i == BTN_LFO ? G_LFO : i == BTN_FX ? G_FX : i == BTN_GLO ? G_GLO : i == BTN_ARP ? G_ARP : i == BTN_SEQ ? G_SEQ : 0u;
}
static void panic_all(void)
{
    for (unsigned p = 0; p < JX_PARTS; p++) ev_put(EV_PANIC, p, 0, 0);
    msg_set("ALL NOTES OFF", 0);
}

/* ======================================================================================================
 * The state at power-off (docs/adr/0011): everything the player has set (the parts' sounds with their
 * unsaved changes, channels, levels, arps, the effects, tempo, part, octave, page) goes into a journal in
 * the flash 1.5 s after the last change, also while playing. A record is written in pieces of at most
 * 64 bytes, each right after an audio block (the interrupts are off for ~0.2 ms a piece, the next block
 * has 1.45 ms); a sector is erased (~50 ms, the sound would stop) only when everything has been silent
 * for a second. At power-on the newest whole record comes back.
 * ====================================================================================================== */
#define JR_AT SLOTS_AT                          /* the platform's slots (store.h): 16 of their 32 sectors */
#define JR_SECT 16u
#define JR_REC 512u
#define JR_PER (4096u / JR_REC)                 /* 8 records a sector */
#define JR_N (JR_SECT * JR_PER)
#define JR_MAGIC 0x5453584Au                    /* "JXST" */
_Static_assert(JR_AT + JR_SECT * 0x1000u <= SLOTS_AT + NSLOT * SLOT_SECT * 0x1000u, "the journal fits the slots");
typedef struct { uint32_t magic, seq; uint16_t len, crc; uint32_t sum; } jr_head_t;
typedef struct {
    __typeof__(set) set;
    uint8_t v[JX_PARTS][JP_COUNT];
    uint8_t page;
} jr_state_t;
_Static_assert(sizeof(jr_head_t) + sizeof(jr_state_t) <= JR_REC, "a state fits a record");
static jr_state_t jr_now, jr_saved;
static uint32_t jr_seq, jr_pos, jr_t_change, jr_ok;
static uint8_t jr_clean[JR_SECT];               /* the sector holds nothing yet (erased) */
static uint8_t jr_buf[JR_REC];

static uint16_t jr_crc(const uint8_t *p, uint32_t n)    /* CRC-16/CCITT */
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
    for (unsigned p = 0; p < JX_PARTS; p++) memcpy(st->v[p], up[p].v, JP_COUNT);
    st->page = page;
}
static int jr_head(uint32_t i, jr_head_t *h) { return st_read(JR_AT + (i / JR_PER) * 0x1000u + (i % JR_PER) * JR_REC, h, sizeof *h); }
static int jr_playing(void)                     /* a voice, an arp, or an effect's tail */
{
    if (!quiet() || silent_blocks < 700u) return 1;
    for (unsigned p = 0; p < JX_PARTS; p++) if (arp[p].cfg[ARP_ON] && arp[p].nkeys) return 1;
    for (unsigned p = 0; p < LV_PARTS; p++) if (lv_store.p[p].on && lv_store.p[p].bars && xport != 2) return 1;   /* (a loop plays) */
    if (lv_busy()) return 1;
    return 0;
}
/* write after the next audio block, so the interrupts are off between blocks, never across one */
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
    if (jr_pos % JR_PER == 0 && !jr_clean[sect]) return;     /* not erased yet: when it is (jr_tidy) */
    memcpy(jr_buf + sizeof h, &jr_now, sizeof jr_now);
    h.magic = JR_MAGIC; h.seq = jr_seq + 1u; h.len = sizeof jr_now; h.crc = jr_crc(jr_buf + sizeof h, sizeof jr_now); h.sum = ~h.seq;
    jr_clean[sect] = 0;
    /* the state first, the head last: a record cut by the power has no head and is never read */
    if (!jr_prog(off + sizeof h, jr_buf + sizeof h, sizeof jr_now) && !jr_prog(off, (const uint8_t *)&h, sizeof h)) {
        jr_seq++;
        jr_saved = jr_now;
    }
    jr_pos = (jr_pos + 1u) % JR_N;              /* (a failed record is skipped, never written over) */
}
static void jr_tidy(int may_stall)              /* keep the sectors ahead erased */
{
    for (uint32_t k = 0; k < 3u; k++) {         /* (k 0: the sector the next record opens, if it opens one) */
        uint32_t s = (jr_pos / JR_PER + k) % JR_SECT, took;
        if (k == 0 && jr_pos % JR_PER) continue;
        if (jr_clean[s]) continue;
        if (!may_stall) return;
        fm1_wdt_feed();
        if (!fl_erase4k(JR_AT + s * 0x1000u, &took)) jr_clean[s] = 1;
        return;                                 /* one a call */
    }
}
static int jr_load(jr_state_t *st)              /* power-on: the newest whole record; 0 = found */
{
    uint32_t best = 0, found = 0;
    static uint8_t page_buf[256];
    if (!flash_ok) return -1;
    for (uint32_t i = 0; i < JR_N; i++) {
        jr_head_t h;
        if (jr_head(i, &h) || h.magic != JR_MAGIC || h.sum != ~h.seq || h.len != sizeof *st) continue;
        if (found && (int32_t)(h.seq - jr_seq) <= 0) continue;
        {
            uint32_t off = JR_AT + (i / JR_PER) * 0x1000u + (i % JR_PER) * JR_REC + sizeof h;
            if (st_read(off, jr_buf, h.len) || jr_crc(jr_buf, h.len) != h.crc) continue;
        }
        jr_seq = h.seq; best = i; found = 1;
        memcpy(st, jr_buf, sizeof *st);
    }
    jr_pos = found ? (best + 1u) % JR_N : 0;
    for (uint32_t sct = 0; sct < JR_SECT; sct++) {   /* which sectors are still empty */
        uint32_t clean = 1;
        for (uint32_t o = 0; o < 0x1000u && clean; o += sizeof page_buf) {
            if (st_read(JR_AT + sct * 0x1000u + o, page_buf, sizeof page_buf)) { clean = 0; break; }
            for (uint32_t b = 0; b < sizeof page_buf; b++) if (page_buf[b] != 0xFFu) { clean = 0; break; }
        }
        jr_clean[sct] = (uint8_t)clean;
    }
    {   /* the rest of the sector being written: still empty? else go on in the next one */
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
 * The main loop
 * ====================================================================================================== */
static unsigned preset_named(const char *n)     /* a factory sound by its name (the first when none has it) */
{
    for (unsigned i = 0; i < jx_preset_count; i++) {
        const char *a = jx_presets[i].name, *b = n;
        while (*a && *a == *b) a++, b++;
        if (*a == *b) return i;
    }
    return 0;
}
static void app_main(void)
{
    uint32_t t_stats = 0, t_frame = 0, press_t[FM1_NBTN] = {0}, long_done = 0, combo = 0, shot_w = 0;

    /* the categories: the factory's (as many as the build expects), then the own sounds */
    for (unsigned c = 0; c < NFC; c++) cat_name[c] = c < jx_cat_count ? jx_cat_names[c] : "-";
    cat_name[CAT_OWN] = "EIGENE";
    /* the sounds and the parts as they were left */
    if (!flash_ok || blob_load(0, own, sizeof own)) memset(own, 0, sizeof own);
    if (!flash_ok || blob_load(1, perfs, sizeof perfs)) memset(perfs, 0, sizeof perfs);
    if (!flash_ok || blob_load(3, &sq, sizeof sq) || sq.ver != SQ_VER) {   /* the sequences (docs/adr/0015) */
        memset(&sq, 0, sizeof sq);
        sq.ver = SQ_VER;
        for (unsigned p = 0; p < JX_PARTS; p++) { sq.cfg[p][SQ_DIV] = 2; sq.cfg[p][SQ_GATE] = 80; sq.cfg[p][SQ_SHIFT] = 24; }
    }
    for (unsigned p = 0; p < JX_PARTS; p++) {
        if (sq.len[p] > SEQ_STEPS) sq.len[p] = 0;
        for (unsigned i = 0; i < SQ_N; i++) if (sq.cfg[p][i] > (i == SQ_ON ? 1u : i == SQ_DIV ? FX_DIVS - 1u : i == SQ_SHIFT ? 48u : 127u)) sq.cfg[p][i] = 0;
        if (!sq.len[p]) sq.cfg[p][SQ_ON] = 0;
        sqr[p].sounding = -1; sqr[p].fresh = 1;
    }
    sq_sum_saved = sq_sum_seen = sq_sum();
    lv_load_done(flash_ok ? blob_load(4, &lv_store, sizeof lv_store) : -1);   /* the live loops (ADR 0053): none yet = empty */
    for (unsigned p = 0; p < LV_PARTS; p++) lv_store.p[p].on = 0;   /* (nothing plays by itself at power-on) */
    lv_sum_saved = lv_sum();
    for (unsigned i = 0; i < NSET; i++) if (perfs[i].used && (perfs[i].s.ver != SET_VER || !perfs[i].name[0])) perfs[i].used = 0;
    for (unsigned j = 0; j < NOWN; j++) if (own[j].used && (own[j].cat >= NFC || !own[j].name[0])) own[j].used = 0;
    {
        static jr_state_t st;
        int have = flash_ok ? jr_load(&st) : -1;
        if (!have && st.set.ver == SET_VER) set = st.set;
        else have = -1;
        if (have < 0) memset(&set, 0, sizeof set);
        jr_saved = st;
        if (have == 0) {                         /* the page and the unsaved changes, after the sounds below */
            page = st.page < NPAGE ? st.page : 0;
            grp_last[pages[page].group] = (uint8_t)(page + 1u);
        }
        jr_now.page = (uint8_t)(have == 0 ? 1 : 0);   /* (a flag for below: restore the sounds' values) */
    }
    if (set.ver != SET_VER) {
        static const uint8_t arp_def[ARP_N] = {0, 0, 2, 0, 80, 0};
        memset(&set, 0, sizeof set);
        set.ver = SET_VER;
        set.tempo = 80;
        for (unsigned p = 0; p < JX_PARTS; p++) {
            static const char *const first[JX_PARTS] = {"POLY SYNTH", "E. BASS", "STRINGS 1", "PIANO 1"};
            set.p[p].idx = (uint8_t)preset_named(first[p]); set.p[p].chan = (uint8_t)(p + 1); set.p[p].level = 100;
            memcpy(set.p[p].arp, arp_def, ARP_N);
        }
        for (unsigned i = 0; i < FX_COUNT; i++) set.fx[i] = (uint8_t)syn_fx_default(i);
    }
    tempo = set.tempo <= 200u ? set.tempo : 80;
    ev_put(EV_TEMPO, 0, tempo, 0);
    clkv[0] = set.clk_src <= 2u ? set.clk_src : 0; clkv[1] = set.clk_send <= 1u ? set.clk_send : 0;
    ev_put(EV_CLK, 0, 0, clkv[0]); ev_put(EV_CLK, 0, 1, clkv[1]);
    for (unsigned i = 0; i < FX_COUNT; i++) { fxv[i] = set.fx[i] <= (unsigned)syn_fx_max(i) ? set.fx[i] : (uint8_t)(i == FX_DLY_TIME ? syn_fx_max(i) : syn_fx_default(i)); ev_put(EV_FX, 0, i, fxv[i]); }
    fx_on = fxv[FX_DLY_LEVEL] || fxv[FX_REV_LEVEL];
    sel = set.sel < JX_PARTS ? set.sel : 0;
    oct = set.oct < -3 || set.oct > 3 ? 0 : set.oct;
    for (unsigned p = 0; p < JX_PARTS; p++) {
        up[p].chan = set.p[p].chan <= 16u ? set.p[p].chan : (uint8_t)(p + 1);
        up[p].level = set.p[p].level > 127u ? 100 : set.p[p].level;
        part_ch[p] = up[p].chan;
        load_sound(p, set.p[p].kind, set.p[p].idx);
        if (jr_now.page) {                      /* the values as they were, changed or not */
            memcpy(up[p].v, jr_saved.v[p], JP_COUNT);
            for (unsigned i = 0; i < JP_COUNT; i++) if (up[p].v[i] > jx_param_max(i)) up[p].v[i] = up[p].base[i];
            mark_dirty(p); push_part(p);
        }
        ev_put(EV_LEVEL, p, up[p].level, 0);
        for (unsigned a = 0; a < ARP_N; a++) { arpc[p][a] = set.p[p].arp[a] <= arp_max[a] ? set.p[p].arp[a] : 0; ev_put(EV_ARP, p, a, arpc[p][a]); }
    }
    jr_take(&jr_now);
    jr_saved = jr_now;                          /* what was restored needs no new record */
    if (flash_ok) for (int k = 0; k < 2; k++) jr_tidy(1);   /* (silent at power-on: erase ahead now) */
    lcd_fill(0, 0, 240, 240, T_BG);
    lights_init();
    msg_set(APP_NAME " V" APP_VER, 0);           /* which firmware this is, for a moment */

    for (;;) {
        uint32_t now, b, rel, cur;
        int32_t e;
        platform_poll();
        now = fm1_ms;
        keys_poll();
        if (tempo_taken) {                      /* the clock's last tempo became TEMPO (docs/adr/0012) */
            char t[24], *q;
            uint32_t tt = tempo_taken;
            tempo_taken = 0;
            tempo = (uint8_t)((tt + 5u) / 10u - 40u);
            touch_set();
            q = put(t, "TEMPO KEPT: "); fmt_int(q, (int32_t)(tt / 10u)); q += str_len(q);
            if (tt % 10u) { *q++ = '.'; *q++ = (char)('0' + tt % 10u); }
            *q = 0;
            msg_set(t, 0);
        }

        /* the buttons: a page button acts when pressed; HOME and SAVE tell short from long (1 s);
         * OCT- and OCT+ together: all notes off */
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
                long_done |= m;                 /* PLAY held: the transport (docs/adr/0012) */
                if (ext_follow) msg_set("FOLLOWS MIDI CLOCK", 0);
                else { xport_req = xport == 1 ? 2 : 1; msg_set(xport == 1 ? "STOP" : clkv[1] ? "START  (SENT TO USB)" : "START", 0); }
            }
            if ((cur & m) && !(long_done & m) && now - press_t[i] >= 1000u && (i == BTN_HOME || i == BTN_SAVE) && !saving) {
                long_done |= m;
                if (i == BTN_HOME) {            /* the changes the last sound change put aside */
                    if (restore[sel].ok) {
                        restore[sel].ok = 0;
                        load_sound(sel, restore[sel].kind, restore[sel].idx);
                        memcpy(up[sel].v, restore[sel].v, JP_COUNT);
                        str_cpy(up[sel].name, restore[sel].name, sizeof up[sel].name);
                        mark_dirty(sel); push_part(sel);
                        msg_set("EDITS BROUGHT BACK", 0);
                    } else msg_set("NOTHING TO BRING BACK", 0);
                } else if (pages[page].stage == ST_SETS) dlg_open_perf(); else dlg_open(sel);
            }
            if (rel & m) {
                int was_long = (long_done & m) != 0;
                long_done &= ~m;
                switch (i) {
                case BTN_OCT_DOWN: case BTN_OCT_UP:
                    if (combo) { if (!(cur & ((1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP)))) combo = 0; }
                    else if (i == BTN_OCT_DOWN && oct > -3) { oct--; set.oct = oct; touch_set(); }
                    else if (i == BTN_OCT_UP && oct < 3) { oct++; set.oct = oct; touch_set(); }
                    break;
                case BTN_REC:
                    if (saving) break;
                    if (!rec_on && pages[page].stage == ST_SEQ && (cur >> BTN_EDIT & 1u)) {   /* EDIT + REC: clear (0.2.8) */
                        ev_put(EV_SEQ, sel, SQ_ON, 0); ev_put(EV_SEQ_LEN, sel, 0, 0);
                        layer_used = 1;
                        msg_set("SEQ CLEARED", 0);
                        break;
                    }
                    if (rec_on || pages[page].stage == ST_SEQ) { seq_rec(!rec_on); break; }   /* SEQ page: steps as before */
                    {   /* else the live recording after the RK-008 (ADR 0053): tap now, twice with a count-in, REC keeps */
                        int r = lv_ui_rec(sel, now);
                        if (r && xport == 2) xport_req = 1;   /* (stopped: the clock runs again) */
                        if (r) for (unsigned pi = 0; pi < NPAGE; pi++) if (pages[pi].stage == ST_LIVE) { goto_page(pi, 0); break; }
                        msg_set(r == 2 ? "COUNT IN" : r ? "REC  REC = DONE  PLAY = UNDO" : "LOOP KEPT", 0);
                    }
                    break;
                case BTN_EDIT: case BTN_ENV: case BTN_LFO: case BTN_FX: case BTN_GLO: case BTN_ARP: case BTN_SEQ:
                    if (layer == btn_group(i)) {    /* let go: a tap opens its pages, a hold was its layer */
                        if (!layer_used && now - press_t[i] < 400u && !saving) group_button(layer);
                        layer = 0; touched = 255;
                    }
                    break;
                case BTN_PLAY:                  /* tapped: the arp's HOLD for the selected part */
                    if (!was_long && !saving && lv_busy()) { lv_ui_stop(sel); msg_set("PASS DROPPED", 0); break; }   /* STOP (ADR 0053) */
                    if (!was_long && !saving) {
                        arpc[sel][ARP_HOLD] ^= 1u; ev_put(EV_ARP, sel, ARP_HOLD, arpc[sel][ARP_HOLD]); touch_set();
                        msg_set(arpc[sel][ARP_HOLD] ? "HOLD ON" : "HOLD OFF", 0);
                    }
                    break;
                case BTN_HOME: if (!was_long) { if (saving) saving = 0; else group_button(G_HOME); } break;
                case BTN_SAVE: if (!was_long) { if (saving) dlg_ok(sel); else if (pages[page].stage == ST_SETS) dlg_open_perf(); else save_short(sel); } break;
                default: break;
                }
            }
        }

        /* the encoders */
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0 && !saving) {
            if (rec_on) seq_rec(0);
            int s = (int)sel + e;
            sel = (uint8_t)(s < 0 ? 0 : s >= (int)JX_PARTS ? JX_PARTS - 1 : s);
            set.sel = sel; touch_set();
        }
        if ((e = fm1_enc_take(ENC_SELECT)) != 0 && !saving) {
            int p = (int)page + e;
            p = p < 0 ? 0 : p >= (int)NPAGE ? (int)NPAGE - 1 : p;
            goto_page((unsigned)p, e > 0 ? 1 : -1);
        }
        if ((e = fm1_enc_take(ENC_PRESETS)) != 0 && !saving) {
            step_sound(sel, e);
            set_pack(); touch_set();
        }
        for (uint32_t k = 0; k < 4u; k++) {
            if ((e = fm1_enc_take(ENC_KNOB1 + k)) == 0) continue;
            touched = (uint8_t)k; t_touch = now;
            if (saving) {
                if (k == 0) { int c = (int)dlg.cursor + e; dlg.cursor = (uint8_t)(c < 0 ? 0 : c >= (int)NAMEMAX ? NAMEMAX - 1 : c); }
                else if (k == 1) {
                    const char *c = find_char(CHARS, dlg.name[dlg.cursor]);
                    int i = c ? (int)(c - CHARS) : 0, n = (int)sizeof CHARS - 1;
                    dlg.name[dlg.cursor] = CHARS[((i + e) % n + n) % n];
                } else if (k == 2 && !dlg.perf) { int c = ((int)dlg.cat + e) % NFC; dlg.cat = (uint8_t)(c < 0 ? c + NFC : c); }
                else if (k == 3) { int s = (int)dlg.slot + e, n = dlg.perf ? NSET : NOWN; dlg.slot = (uint8_t)(s < 0 ? 0 : s >= n ? n - 1 : s); }
            } else {
                const kn_t *kn;
                if (layer) layer_used = 1;
                kn = &cells_now()[k];
                if (kn->kind != K_NONE) k_turn(kn, enc_fast(k, e, k_max(kn), now));
            }
        }
        lights();

        /* the load numbers, twice a second */
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

        /* a frame every 33 ms */
        if (now - t_frame >= 33u) {
            t_frame = now;
            frame_no++;
            for (unsigned p = 0; p < JX_PARTS; p++) if (midi_hit[p] != midi_seen[p]) { midi_seen[p] = midi_hit[p]; midi_t[p] = now; }
            for (unsigned p = 0; p < JX_PARTS; p++) voices_now[p] = (uint8_t)jx_part_voices(&synth, p);
            for (unsigned k = 0; k < 4u; k++) {     /* the bars glide */
                const kn_t *kn = &cells_now()[k];
                int mx = k_max(kn);
                int32_t target = mx ? k_get(kn) * 65536 / mx : 0, d = target - shown_q8[k];
                shown_q8[k] = d > -300 && d < 300 ? target : shown_q8[k] + d * 2 / 5;
            }
            if (pages[page].stage == ST_HOME && scope_w != shot_w) {   /* the scope: from a rising zero crossing, so it stands still */
                uint32_t w = scope_w, start = w - 500u, i;
                shot_w = w;
                for (i = 0; i < 250u; i++)
                    if (scope_buf[(start + i) & 511u] < 0 && scope_buf[(start + i + 1u) & 511u] >= 0) break;
                if (i == 250u) i = 0;
                {   /* as tall as the picture allows (a quiet note still shows its wave: up to 7.5 times); next to
                     * silence (the chorus's hiss, the last bit of a tail) a still line, not noise blown up */
                    int32_t pk = 0;
                    for (uint32_t x = 0; x < 240u; x++) { int32_t v = scope_buf[(start + i + x) & 511u]; if (v < 0) v = -v; if (v > pk) pk = v; }
                    if (pk < 3) pk = 0; else if (pk < 16) pk = 16;
                    for (uint32_t x = 0; x < 240u; x++) scope_shot[x] = (int8_t)(pk ? scope_buf[(start + i + x) & 511u] * 120 / pk : 0);
                }
            }
            render(now);
        }

        /* the parts, 2 s after the last change when nothing sounds (a save stalls the sound for ~50 ms) */
        /* the state at power-off: 1.5 s after the last change, also while playing (docs/adr/0011) */
        if (flash_ok && !rec_on) {               /* the sequences: 1.5 s after a change, in a pause (an erase stops the sound) */
            static uint32_t t_sq;
            if (now - t_sq >= 250u) {
                uint32_t h = sq_sum();
                t_sq = now;
                if (h != sq_sum_seen) { sq_sum_seen = h; sq_t_change = now; }
                else if (h != sq_sum_saved && now - sq_t_change >= 1500u && !jr_playing()) {
                    if (!blob_save(3, &sq, sizeof sq)) sq_sum_saved = h;
                }
                if (!lv_busy() && !jr_playing()) {   /* the live loops (blob 4), in a pause */
                    uint32_t hl = lv_sum();
                    if (hl != lv_sum_saved && !blob_save(4, &lv_store, sizeof lv_store)) lv_sum_saved = hl;
                }
            }
        }
        if (jr_ok && now - jr_t_change >= 250u) {
            static uint32_t t_check;
            if (now - t_check >= 250u) {
                jr_state_t st;
                t_check = now;
                jr_take(&st);
                if (memcmp(&st, &jr_now, sizeof st)) { jr_now = st; jr_t_change = now; }
                else if (memcmp(&jr_now, &jr_saved, sizeof jr_now) && now - jr_t_change >= 1500u && !jr_playing()) jr_write();   /* (only in a pause: a flash write holds the interrupts off, a block would come late: a click) */
                else if (!jr_playing()) jr_tidy(1);
            }
        }
        set_dirty = 0;
    }
}
