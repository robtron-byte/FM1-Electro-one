/* SPDX-License-Identifier: GPL-3.0-only */
/* 808 solo: Bedienung wie der Bereich BEAT von FM1-Electro (ui/el_ui.c, dort GPL-3.0-only WITH GPL-3.0-linking-exception),
 * Teil der Einheit gm_app.c (GM_UI_FILE). Ablauf und Tasten wie dort:
 *
 *   PLAY            Start / Stop                      REC   Aufnahme (EDIT + REC: Pattern löschen; 1 s: neu aufnehmen)
 *   schwarze 1–10   Instrument wählen und hören (BD SD RS CP CB CY OH CH LG CL), 11 = Akzent-Zeile
 *   weiße 1–16      Steps des Instruments             EDIT + weiß Akzent, GLO + weiß 32tel-Roll
 *   EDIT + schwarz  Instrument löschen                GLO + schwarz stumm; GLO tippen: Mute-Modus
 *   Step halten     PRESETS Durchläufe, ALGORITHM Wahrscheinlichkeit, KNOB 2 Velocity
 *   SELECT          Platz A–D (am Ende des Patterns); GLO + SELECT: Instrument (alle 17) ohne Ton
 *   ALGORITHM       Tempo; GLO: Länge in Takten (1–2); EDIT: Swing
 *   PRESETS         Kit; EDIT + PRESETS: sein Werksklang
 *   KNOB 1–4        LEVEL und die drei Regler des Instruments; FX (Bereichstaste) blättert: SENDS, DIST, DELAY, REVERB
 *   OCT+ / OCT−     Vorschau an/aus / Step-Seite 1–16 und 17–32
 *   SEQ             Fill im nächsten Takt (halten: jeden Takt), GLO + SEQ automatisch 8/16/32, EDIT + SEQ bearbeiten
 *   ARP halten      Generator: KNOB 1 Stil, 2 Stärke, 3 Kick behalten; weiß 1 zweimal Variation, weiß 2 zweimal neu
 *   HOME            Übersicht: weiße 1–4 Plätze A–D (gehalten + weitere: Kette), KNOB 1–4 Pegel, Sends
 *   SAVE            alles speichern (sonst von selbst 2 s nach einer Änderung, wenn gestoppt) */

/* ---- Zeichnen (wie gm_ui.c) */
static void R(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y, w, h, c); }
static int32_t Q_W(const char *t, int32_t s) { int32_t n = (int32_t)str_len(t); return n ? n * 4 * s - s : 0; }
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
static int32_t P_W(const char *t, int32_t s) { int32_t n = (int32_t)str_len(t); return n ? n * 6 * s - s : 0; }
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
static void QC(int32_t cx, int32_t y, int32_t s, const char *t, uint16_t c) { Q(cx - Q_W(t, s) / 2, y, s, t, c); }
static void PC(int32_t cx, int32_t y, int32_t s, const char *t, uint16_t c) { P(cx - P_W(t, s) / 2, y, s, t, c); }

/* Farben wie el_ui.c (RGB565) */
enum { BACK = 0x0000, PANEL = 0x18e3, GRID = 0x2945, TEXT = 0xef5d, DIM = 0x738e, FAINT = 0x4208,
       ACCENTC = 0x07ff, RED = 0xf9a6, AMBER = 0xfd20, GREEN = 0x37e6, BEATC = 0xfb44 };
static const uint16_t part_color[DB_PARTS] = {0xf8a3, 0xfc40, 0xfd8c, 0xfd8c, 0xfd8c, 0x5e7f, 0x5e7f, 0x5e7f, 0x87f0, 0xffff,
                                              0xfc40, 0xfc40, 0xfc40, 0xfd8c, 0xfd8c, 0xfd8c, 0xffff};
enum { HOLD_EDIT = 1, HOLD_GLO = 2, HOLD_GEN = 4 };
enum { UI_WHITE = 16, UI_BLACK = 11 };
static const uint8_t white_keys[UI_WHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[UI_BLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int key_white(unsigned key) { for (unsigned i = 0; i < UI_WHITE; ++i) if (white_keys[i] == key) return (int)i; return -1; }
static int key_black(unsigned key) { for (unsigned i = 0; i < UI_BLACK; ++i) if (black_keys[i] == key) return (int)i; return -1; }
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

/* ---- Einstellungen (gespeichert mit den Pattern) */
enum { PG_PART, PG_SENDS, PG_DIST, PG_DELAY, PG_REVERB, NPG };
static struct {
    uint16_t bpm_x10;
    uint8_t dly_send, rev_send, dist_send, dist_mode, dist_drive, dist_tone, dist_level, level;
    uint8_t fx[FX_COUNT];
    uint8_t clk_src, clk_send, gen_style, gen_density, gen_keep, preview_off;
} us;
static struct {
    uint32_t now, buttons_prev, keys_prev;
    uint8_t part, acc_lane, page, overview, mute_mode, step_page, fill_edit, hold;
    uint8_t glo_tap, arp_tap, edit_tap, ask, seq_held_fill;
    uint32_t ask_until, seq_at, rec_at;
    uint8_t rec_renewed;
    /* der gehaltene Step */
    uint8_t sh_key, sh_step, sh_part, sh_cond, sh_prob, sh_vol, sh_used, sh_remove, sh_show_prob, sh_lock;
    /* Kette in HOME */
    uint8_t chain_key, chain_count, chain_build[DB_CHAIN];
    const char *notice;
    uint32_t notice_until;
    uint8_t touched;
    uint32_t touched_at;
    uint8_t autosave;
    uint32_t autosave_at, changed_at, saved_rev, seen_rev;
} ui;
static void notice(const char *t) { ui.notice = t; ui.notice_until = ui.now + 1500u; }
static void keep_later(void) { ui.autosave = 1; ui.autosave_at = ui.now + 2000u; }

static void push_fx(unsigned i) { ev_put(EV_FX, 0, i, us.fx[i]); fx_on = us.fx[FX_DLY_LEVEL] || us.fx[FX_REV_LEVEL]; }
static void push_settings(void)
{
    ev_put(EV_TEMPO, 0, (uint32_t)clampi((us.bpm_x10 + 5) / 10 - 40, 0, 200), 0);
    ev_put(EV_CLK, 0, 0, us.clk_src); ev_put(EV_CLK, 0, 1, us.clk_send);
    for (unsigned i = 0; i < FX_COUNT; i++) push_fx(i);
    ev_put(EV_SEND, 0, 0, us.dly_send); ev_put(EV_SEND, 0, 1, us.rev_send);
    ev_put(EV_LEVEL, 0, us.level, 0);
    dist_v[0] = us.dist_mode; dist_v[1] = us.dist_drive; dist_v[2] = us.dist_tone; dist_v[3] = us.dist_level;
    db_engA.dist = db_engB.dist = us.dist_send * (1.0f / 127.0f);
}

/* ---- Speichern: Blob 1 = Kennung, Einstellungen, Pattern, Kit, Fills; Blob 2 = Bedingungen, Wahrscheinlichkeit, Velocity */
#define DK_BLOB_MAGIC 0x31424B44u               /* "DKB1" */
typedef struct { uint32_t magic; uint16_t ver, size1, size2, pad; } dk_head_t;
enum { DK_BLOB_VER = 1 };
static int dk_save(void)
{
    uint8_t *b = (uint8_t *)cv_px;              /* (die Zeichenfläche als Puffer: nicht während render) */
    uint32_t n = 0;
    dk_head_t h = {DK_BLOB_MAGIC, DK_BLOB_VER, (uint16_t)sizeof us, (uint16_t)sizeof db.s, 0};
    _Static_assert(sizeof(dk_head_t) + sizeof us + sizeof db.s.pattern + sizeof db.s.sound + sizeof db.s.kit_mem + 4u +
                   sizeof db.s.fill <= 4000u, "Blob 1 fasst Einstellungen, Pattern, Kits und Fills");
    _Static_assert(sizeof db.s.cond + sizeof db.s.prob + sizeof db.s.vol + sizeof(dk_head_t) <= 4000u, "Blob 2 fasst die Step-Werte");
    _Static_assert(CV_MAX * 2u >= 4096u, "die Zeichenfläche fasst einen Blob");
    if (!flash_ok) return -1;
    memcpy(b + n, &h, sizeof h); n += sizeof h;
    memcpy(b + n, &us, sizeof us); n += sizeof us;
    memcpy(b + n, db.s.pattern, sizeof db.s.pattern); n += sizeof db.s.pattern;
    memcpy(b + n, &db.s.sound, sizeof db.s.sound); n += sizeof db.s.sound;
    memcpy(b + n, db.s.kit_mem, sizeof db.s.kit_mem); n += sizeof db.s.kit_mem;
    b[n++] = db.s.kit_mem_used; b[n++] = db.s.kit_index; b[n++] = db.s.level_value; b[n++] = db.s.fill_auto;
    memcpy(b + n, db.s.fill, sizeof db.s.fill); n += sizeof db.s.fill;
    if (blob_save(1, b, n)) return -1;
    n = 0;
    memcpy(b + n, &h, sizeof h); n += sizeof h;
    memcpy(b + n, db.s.cond, sizeof db.s.cond); n += sizeof db.s.cond;
    memcpy(b + n, db.s.prob, sizeof db.s.prob); n += sizeof db.s.prob;
    memcpy(b + n, db.s.vol, sizeof db.s.vol); n += sizeof db.s.vol;
    if (blob_save(2, b, n)) return -1;
    ui.saved_rev = db.revision;
    return 0;
}
static int dk_load(void)
{
    uint8_t *b = (uint8_t *)cv_px;
    dk_head_t h;
    uint32_t n = 0;
    if (!flash_ok || blob_load(1, b, 4000u)) return -1;
    memcpy(&h, b, sizeof h);
    if (h.magic != DK_BLOB_MAGIC || h.ver != DK_BLOB_VER || h.size1 != sizeof us || h.size2 != sizeof db.s) return -1;
    n = sizeof h;
    memcpy(&us, b + n, sizeof us); n += sizeof us;
    memcpy(db.s.pattern, b + n, sizeof db.s.pattern); n += sizeof db.s.pattern;
    memcpy(&db.s.sound, b + n, sizeof db.s.sound); n += sizeof db.s.sound;
    memcpy(db.s.kit_mem, b + n, sizeof db.s.kit_mem); n += sizeof db.s.kit_mem;
    db.s.kit_mem_used = b[n++]; db.s.kit_index = b[n++]; db.s.level_value = b[n++]; db.s.fill_auto = b[n++];
    memcpy(db.s.fill, b + n, sizeof db.s.fill);
    if (!blob_load(2, b, 4000u)) {
        memcpy(&h, b, sizeof h);
        if (h.magic == DK_BLOB_MAGIC && h.ver == DK_BLOB_VER) {
            n = sizeof h;
            memcpy(db.s.cond, b + n, sizeof db.s.cond); n += sizeof db.s.cond;
            memcpy(db.s.prob, b + n, sizeof db.s.prob); n += sizeof db.s.prob;
            memcpy(db.s.vol, b + n, sizeof db.s.vol);
        }
    }
    return 0;
}
static void settings_default(void)
{
    memset(&us, 0, sizeof us);
    us.bpm_x10 = 1200; us.level = 100; us.dist_drive = 60; us.dist_tone = 70; us.dist_level = 64;
    for (unsigned i = 0; i < FX_COUNT; i++) us.fx[i] = (uint8_t)syn_fx_default(i);
    us.fx[FX_DLY_LEVEL] = us.fx[FX_REV_LEVEL] = 0;
    us.gen_style = 6; us.gen_density = 64;
}
static void settings_check(void)
{
    if (us.bpm_x10 < 400 || us.bpm_x10 > 2400) us.bpm_x10 = 1200;
    for (unsigned i = 0; i < FX_COUNT; i++) if (us.fx[i] > (unsigned)syn_fx_max(i)) us.fx[i] = (uint8_t)(i == FX_DLY_TIME ? syn_fx_max(i) : syn_fx_default(i));
    if (us.clk_src > 2) us.clk_src = 0;
    if (us.clk_send > 1) us.clk_send = 0;
    if (us.dist_mode > 3) us.dist_mode = 0;
    if (us.gen_style >= DB_STYLES) us.gen_style = 6;
    us.dly_send &= 127; us.rev_send &= 127; us.dist_send &= 127; us.dist_drive &= 127; us.dist_tone &= 127;
    us.dist_level &= 127; us.level &= 127; us.gen_density &= 127;
}

/* ---- Regler: die Seite der Bereichstaste (FX) */
typedef struct { const char *name; int value, max; char text[10]; uint16_t color; } knob_t;
static const char *const page_names[NPG] = {"", "SENDS", "DIST", "DELAY", "REVERB"};
static knob_t knob_of(unsigned i)
{
    knob_t k = {"", 0, 0, {0}, TEXT};
    unsigned pg = ui.page;
    if (ui.overview) {
        static const char *const n[4] = {"LEVEL", "DLY SEND", "REV SEND", "DST SEND"};
        const uint8_t v[4] = {us.level, us.dly_send, us.rev_send, us.dist_send};
        k.name = n[i]; k.value = v[i]; k.max = 127; k.color = BEATC;
    } else if (pg == PG_PART) {
        k.name = db_knob_name(ui.part, i);
        if (!*k.name) return k;
        k.value = db.s.sound.v[ui.part][i]; k.max = 100; k.color = part_color[ui.part];
    } else if (pg == PG_SENDS) {
        static const char *const n[4] = {"DLY SEND", "REV SEND", "DST SEND", "LEVEL"};
        const uint8_t v[4] = {us.dly_send, us.rev_send, us.dist_send, us.level};
        k.name = n[i]; k.value = v[i]; k.max = 127; k.color = ACCENTC;
    } else if (pg == PG_DIST) {
        static const char *const n[4] = {"MODE", "DRIVE", "TONE", "LEVEL"};
        static const char *const modes[4] = {"TUBE", "OD", "DIST", "RAT"};
        const uint8_t v[4] = {us.dist_mode, us.dist_drive, us.dist_tone, us.dist_level};
        k.name = n[i]; k.value = v[i]; k.max = i ? 127 : 3; k.color = AMBER;
        if (!i) str_cpy(k.text, modes[v[0] & 3u], sizeof k.text);
    } else {
        static const char *const nd[4] = {"TIME", "FEEDBACK", "TONE", "LEVEL"}, *const nr[4] = {"SIZE", "DECAY", "DAMP", "LEVEL"};
        static const uint8_t pd[4] = {FX_DLY_TIME, FX_DLY_FB, FX_DLY_TONE, FX_DLY_LEVEL}, pr[4] = {FX_REV_SIZE, FX_REV_DECAY, FX_REV_DAMP, FX_REV_LEVEL};
        unsigned p = pg == PG_DELAY ? pd[i] : pr[i];
        k.name = pg == PG_DELAY ? nd[i] : nr[i]; k.value = us.fx[p]; k.max = syn_fx_max(p); k.color = ACCENTC;
        if (p == FX_DLY_TIME) {
            float sec = syn_fx_delay_seconds(k.value, us.fx[FX_DLY_SYNC], us.fx[FX_DLY_DIV], bpm_now);
            fmt_int(k.text, (int32_t)(sec * 1000.0f + 0.5f)); str_cpy(k.text + str_len(k.text), " MS", 4);
        }
    }
    if (!k.text[0]) fmt_int(k.text, k.value);
    return k;
}
static void knob_set(unsigned i, int v)
{
    unsigned pg = ui.page;
    knob_t k = knob_of(i);
    if (!*k.name) return;
    v = clampi(v, 0, k.max);
    if (ui.overview || pg == PG_SENDS) {
        unsigned w = ui.overview ? i : (i == 3u ? 0u : i + 1u);   /* (0 LEVEL, 1 DLY, 2 REV, 3 DST) */
        if (w == 0) { us.level = (uint8_t)v; ev_put(EV_LEVEL, 0, us.level, 0); }
        else if (w == 1) { us.dly_send = (uint8_t)v; ev_put(EV_SEND, 0, 0, us.dly_send); }
        else if (w == 2) { us.rev_send = (uint8_t)v; ev_put(EV_SEND, 0, 1, us.rev_send); }
        else { us.dist_send = (uint8_t)v; db_engA.dist = db_engB.dist = v * (1.0f / 127.0f); }
    } else if (pg == PG_PART) db_post(DB_PARAM, ui.part, (uint8_t)i, 0, (int16_t)v);
    else if (pg == PG_DIST) {
        uint8_t *t[4] = {&us.dist_mode, &us.dist_drive, &us.dist_tone, &us.dist_level};
        *t[i] = (uint8_t)v; dist_v[i] = (uint8_t)v;
    } else {
        static const uint8_t pd[4] = {FX_DLY_TIME, FX_DLY_FB, FX_DLY_TONE, FX_DLY_LEVEL}, pr[4] = {FX_REV_SIZE, FX_REV_DECAY, FX_REV_DAMP, FX_REV_LEVEL};
        unsigned p = pg == PG_DELAY ? pd[i] : pr[i];
        us.fx[p] = (uint8_t)v; push_fx(p);
    }
    ui.touched = (uint8_t)i; ui.touched_at = ui.now;
    keep_later();
}
static uint32_t knob_t_last[4];
static void knob_turn(unsigned i, int d)
{
    knob_t k = knob_of(i);
    if (!*k.name || !d) return;
    int step = d;
    if (k.max > 16) {   /* schnell gedreht: größere Schritte (wie el_ui.c edit_knob) */
        uint32_t gap = ui.now - knob_t_last[i];
        int a = d < 0 ? -d : d, mult = a >= 3 ? 3 : a == 2 ? 2 : gap < 40u ? 3 : gap < 90u ? 2 : 1;
        if (k.max > 64 && mult > 1) ++mult;
        step = d * mult;
    }
    knob_t_last[i] = ui.now;
    knob_set(i, k.value + step);
}

/* ---- Generator (ARP gehalten): zweimal dieselbe Taste */
enum { ASK_NONE, ASK_VARY, ASK_NEW };
static int confirmed(unsigned ask, const char *q)
{
    if (ui.ask == ask && (int32_t)(ui.now - ui.ask_until) < 0) { ui.ask = ASK_NONE; return 1; }
    ui.ask = (uint8_t)ask; ui.ask_until = ui.now + 3000u;
    notice(q); ui.notice_until = ui.ask_until;
    return 0;
}
static void generator_key(unsigned key)
{
    int w = key_white(key);
    static db_pattern_t g;
    if (w != 0 && w != 1) { ui.ask = ASK_NONE; return; }
    if (w == 0) {
        if (!confirmed(ASK_VARY, "VARY THIS PATTERN? KEY 1 AGAIN")) return;
        unsigned amount = 24u + us.gen_density * 3u / 4u;
        uint32_t seed = ui.now * 2654435761u + 977u;
        for (unsigned t = 0; t < 8u; t++, seed = seed * 2654435761u + 1u) {
            g = db.s.pattern[db.current];
            db_vary(&g, amount, seed);
            if (memcmp(&g, &db.s.pattern[db.current], sizeof g)) break;
        }
        if (db_post_pattern(db.current, &g, 1) == 0) { notice("VARIED"); keep_later(); } else notice("BUSY, TRY AGAIN");
        return;
    }
    if (!confirmed(ASK_NEW, "NEW PATTERN? KEY 2 AGAIN")) return;
    db_generate(&g, us.gen_style, us.gen_density, ui.now * 2654435761u + key * 40503u + 1u, us.gen_keep ? &db.s.pattern[db.current] : 0);
    g.length = db.s.pattern[db.current].length;
    if (db_post_pattern(db.current, &g, 0) == 0) { notice("NEW BEAT"); keep_later(); } else notice("BUSY, TRY AGAIN");
}

/* ---- der gehaltene Step (ADR 0025, 0028 in FM1-Electro) */
static const char *const cond_names[DB_CONDS] = {"1:1", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4"};
static void step_hold(unsigned key, unsigned st, unsigned part, int on)
{
    ui.sh_key = (uint8_t)(key + 1u); ui.sh_step = (uint8_t)st; ui.sh_part = (uint8_t)part;
    ui.sh_cond = (uint8_t)(on ? db_cond(db.current, part, st) : 0u);
    ui.sh_prob = (uint8_t)(on ? db_prob(db.current, part, st) : 0u);
    ui.sh_vol = (uint8_t)(on ? db_vol(db.current, part, st) : 0u);
    ui.sh_show_prob = ui.sh_prob && !ui.sh_cond;
    ui.sh_used = 0; ui.sh_remove = (uint8_t)(on != 0); ui.sh_lock = 0;
}
static void step_hold_release(uint32_t keys)
{
    if (!ui.sh_key || (keys >> (ui.sh_key - 1u) & 1u)) return;
    if (ui.sh_remove && !ui.sh_used) db_post(DB_TOGGLE, ui.sh_part, ui.sh_step, 0, 0);
    ui.sh_key = 0;
}

/* ---- Tasten in BEAT (el_ui.c keys_beat) */
static void keys_beat(unsigned key, unsigned hold)
{
    const db_pattern_t *p = &db.s.pattern[db.current];
    int b = key_black(key), w = key_white(key);
    int part = b >= 0 && b < DB_KEYPARTS ? b : -1;   /* (die Reihenfolge der Teile ist die der schwarzen Tasten) */
    if (b == DB_KEYPARTS && !(hold & (HOLD_GLO | HOLD_EDIT))) { ui.acc_lane = 1; notice("ACCENT: STEP KEYS SET ACCENTS"); return; }
    if (part >= 0) {
        ui.acc_lane = 0;
        if ((hold & HOLD_GLO) || (ui.mute_mode && !(hold & HOLD_EDIT))) db_post(DB_MUTE, (uint8_t)part, !(db.mute >> part & 1u), 0, 0);
        else if (hold & HOLD_EDIT) { db_post(DB_CLEAR_PART, (uint8_t)part, 0, 0, 0); notice("PART CLEARED"); }
        else {
            ui.part = (uint8_t)part; ui.page = PG_PART;
            if (!us.preview_off) db_post(DB_HIT, (uint8_t)part, 100, 1, 0);
            else if (db.recording) db_post(DB_HIT, (uint8_t)part, 100, 2, 0);
        }
        return;
    }
    if (w < 0) return;
    if (ui.fill_edit) {
        if ((hold & HOLD_EDIT) || (ui.acc_lane && !(hold & HOLD_GLO))) db_post(DB_FILL_ACCENT, 0, (uint8_t)w, 0, 0);
        else if (hold & HOLD_GLO) db_post(DB_FILL_ROLL, ui.part, (uint8_t)w, 0, 0);
        else db_post(DB_FILL_TOGGLE, ui.part, (uint8_t)w, 0, 0);
        return;
    }
    unsigned s = (unsigned)w + ui.step_page * 16u;
    if (s >= p->length) return;
    if ((hold & HOLD_EDIT) || (ui.acc_lane && !(hold & HOLD_GLO))) db_post(DB_ACCENT, 0, (uint8_t)s, 0, 0);
    else if (hold & HOLD_GLO) db_post(DB_ROLL, ui.part, (uint8_t)s, 0, 0);
    else {
        int on = (p->hit[ui.part] >> s) & 1u;
        step_hold(key, s, ui.part, on);
        if (!on) db_post(DB_TOGGLE, ui.part, (uint8_t)s, 0, 0);
    }
}
/* HOME: weiße 1–4 die Plätze; einer gehalten und weitere dazu: eine Kette (S16), ein leerer bekommt eine Kopie */
static int slot_empty(unsigned slot) { for (unsigned p = 0; p < DB_PARTS; ++p) if (db.s.pattern[slot].hit[p]) return 0; return 1; }
static void post_chain(unsigned links, const uint8_t *slots)
{
    unsigned packed = 0;
    for (unsigned i = 0; i < links; ++i) packed |= (slots[i] & 3u) << (2u * i);
    db_post(DB_CHAIN_SET, 0, (uint8_t)links, 0, (int16_t)packed);
}
static void home_slot_key(unsigned slot)
{
    static char text[40];
    if (!ui.chain_key) {
        ui.chain_key = (uint8_t)(slot + 1u); ui.chain_count = 0;
        if (db.chain_len) post_chain(0, db.chain);
        db_post(DB_SELECT, (uint8_t)slot, 0, 0, 0);
        return;
    }
    unsigned held = ui.chain_key - 1u;
    if (!ui.chain_count) { ui.chain_build[0] = (uint8_t)held; ui.chain_count = 1; }
    if (ui.chain_count >= DB_CHAIN) { notice("CHAIN FULL: 4 PATTERNS"); return; }
    int copied = slot != held && slot_empty(slot) && !slot_empty(held);
    if (copied) db_post(DB_COPY, (uint8_t)held, (uint8_t)slot, 0, 0);
    ui.chain_build[ui.chain_count++] = (uint8_t)slot;
    post_chain(ui.chain_count, ui.chain_build);
    char *q = text;
    if (copied) { *q++ = (char)('A' + held); q = put(q, " COPIED TO "); *q++ = (char)('A' + slot); q = put(q, ", "); }
    q = put(q, "CHAIN ");
    for (unsigned i = 0; i < ui.chain_count; ++i) { if (i) *q++ = '>'; *q++ = (char)('A' + ui.chain_build[i]); }
    *q = 0;
    notice(text);
}

static uint32_t keys_seen;
static void keys_poll(void)
{
    uint32_t keys = fm1_in.notes, down = keys & ~keys_seen;
    unsigned hold = ui.hold;
    keys_seen = keys;
    step_hold_release(keys);
    if (ui.chain_key && (!ui.overview || !(keys >> white_keys[ui.chain_key - 1u] & 1u))) ui.chain_key = 0;
    if (down) { ui.glo_tap = ui.arp_tap = ui.edit_tap = 0; }
    for (unsigned key = 0; key < 27u; key++) {
        if (!(down >> key & 1u)) continue;
        if (hold & HOLD_GEN) { if (key_white(key) >= 0) generator_key(key); }
        else if (ui.overview) {
            int w = key_white(key), b = key_black(key);
            if (w >= 0 && w < DB_PATTERNS) home_slot_key((unsigned)w);
            else if (b == 0) db_post(DB_AREA_MUTE, 0, !db.muted, 0, 0);
        } else keys_beat(key, hold);
        keep_later();
    }
}

/* ---- Licht (el_ui.c update_leds, Bereich BEAT) */
static void lights(void)
{
    const db_pattern_t *p = &db.s.pattern[db.current];
    int playing = xport == 1 && db.playing, slow = (ui.now / 400u) & 1u, fast = (ui.now / 125u) & 1u;
    unsigned hold = ui.hold;
    for (uint32_t i = 0; i < 14u; i++) { fm1_led_key(i, 1); fm1_led_key_glow(i, 0); }
    if (playing) fm1_led_key(BTN_PLAY, (db.pos & 3u) < 2u); else fm1_led_key_glow(BTN_PLAY, 1), fm1_led_key(BTN_PLAY, 0);
    if (db.recording) fm1_led_key(BTN_REC, fast); else { fm1_led_key(BTN_REC, 0); fm1_led_key_glow(BTN_REC, 1); }
    if ((hold & HOLD_GEN) && slow) fm1_led_key(BTN_ARP, 0);
    if (ui.mute_mode && !ui.overview && slow) fm1_led_key(BTN_GLO, 0);
    if (!ui.overview && ((db.fill_now && playing && fast) || (ui.fill_edit && slow))) fm1_led_key(BTN_SEQ, 0);
    {   /* die Bereichstasten: FX (BEAT) voll, die anderen und HOME gedimmt */
        static const uint8_t ab[4] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO};
        for (unsigned a = 0; a < 4u; a++) if (a || ui.overview) { fm1_led_key(ab[a], 0); fm1_led_key_glow(ab[a], 1); }
        if (!ui.overview) { fm1_led_key(BTN_HOME, 0); fm1_led_key_glow(BTN_HOME, 1); }
    }
    if (us.preview_off) { fm1_led_key(BTN_OCT_UP, 0); fm1_led_key_glow(BTN_OCT_UP, 1); }
    if (p->length > 16 && ui.step_page && slow) fm1_led_key(BTN_OCT_DOWN, 0);
    for (uint32_t k = 0; k < 27u; k++) { fm1_led_key(KEY_LED(k), 0); fm1_led_key_glow(KEY_LED(k), 0); }
    if (ui.overview) {
        unsigned now = db.current, next = db.next;
        if (db.chain_len) for (unsigned i = 0; i < db.chain_len; ++i) {
            fm1_led_key(KEY_LED(white_keys[db.chain[i]]), db.chain[i] == now);
            fm1_led_key_glow(KEY_LED(white_keys[db.chain[i]]), 1);
        } else {
            fm1_led_key(KEY_LED(white_keys[now]), 1);
            if (next != now && (ui.now / 250u) & 1u) fm1_led_key(KEY_LED(white_keys[next]), 1);
        }
        fm1_led_key(KEY_LED(black_keys[0]), !db.muted);
        return;
    }
    for (unsigned i = 0; i < UI_WHITE; ++i) {
        unsigned s = i + ui.step_page * 16u;
        if (s >= p->length && !ui.fill_edit) continue;
        int on;
        int acc = (hold & HOLD_EDIT) || (ui.acc_lane && !(hold & HOLD_GLO));
        if (ui.fill_edit) {
            const db_fill_t *f = &db.s.fill[db.current];
            on = acc ? (f->accent >> i) & 1u : (hold & HOLD_GLO) ? (f->roll[ui.part] >> i) & 1u : (f->hit[ui.part] >> i) & 1u;
        } else on = acc ? (p->accent >> s) & 1u : (hold & HOLD_GLO) ? (p->roll[ui.part] >> s) & 1u : (p->hit[ui.part] >> s) & 1u;
        if (playing && db.pos == s) on = !on;
        if (on) fm1_led_key(KEY_LED(white_keys[i]), 1);
        else if (i % 4u == 0) fm1_led_key_glow(KEY_LED(white_keys[i]), 1);   /* die Zählzeiten 1 5 9 13 schwach */
    }
    for (unsigned k = 0; k < DB_KEYPARTS; ++k) {
        uint32_t id = KEY_LED(black_keys[k]);
        if (ui.mute_mode && !(hold & HOLD_GLO)) {
            if (db.mute >> k & 1u) fm1_led_key(id, slow);
            else { fm1_led_key(id, p->hit[k] != 0); if (!p->hit[k]) fm1_led_key_glow(id, 1); }
            continue;
        }
        fm1_led_key(id, (hold & HOLD_GLO) ? !(db.mute >> k & 1u) : (k == ui.part && !ui.acc_lane));
    }
    fm1_led_key(KEY_LED(black_keys[DB_KEYPARTS]), ui.acc_lane);
}

/* ---- Bild */
enum { HEADER_H = 30, GRID_X = 22, GRID_Y = 33, ROW = 8, CELL = 13, FOOT_Y = 186 };
static void draw_header(void)
{
    char t[24], *q;
    int playing = xport == 1 && db.playing;
    R(0, 0, 240, HEADER_H, PANEL);
    R(1, 0, 70, 15, BEATC);
    Q(5, 4, 1, "TR-808", BACK);
    t[0] = (char)('A' + db.next); t[1] = 0;
    P(46, 4, 1, t, db.next != db.current ? AMBER : BACK);
    if (db.chain_len) { q = t; for (unsigned i = 0; i < db.chain_len; ++i) { if (i) *q++ = '>'; *q++ = (char)('A' + db.chain[i]); } *q = 0; Q(80, 5, 1, t, AMBER); }
    if (db.recording) R(60, 4, 6, 6, RED);
    if (db.muted) Q(120, 5, 1, "MUTE", RED);
    P(4, 19, 1, db.s.pattern[db.next].name, TEXT);
    q = t; fmt_int(q, (int32_t)(us.bpm_x10 / 10u)); q += str_len(q); *q = 0;
    P(140, 19, 1, t, TEXT);
    Q(140 + P_W(t, 1) + 3, 21, 1, "BPM", DIM);
    P(206, 19, 1, playing ? "PLAY" : "STOP", playing ? GREEN : DIM);
    Q(206, 5, 1, db_kit_names[db.s.kit_index & 3u], DIM);
}
static void draw_grid(void)
{
    const db_pattern_t *p = &db.s.pattern[db.current];
    static db_pattern_t fv;
    if (ui.fill_edit) {   /* der Fill (N12): sein Takt statt des Grooves */
        const db_fill_t *f = &db.s.fill[db.current];
        fv = *p; fv.length = DB_FILL_STEPS;
        for (unsigned i = 0; i < DB_PARTS; ++i) { fv.hit[i] = f->hit[i]; fv.roll[i] = f->roll[i]; }
        fv.accent = f->accent;
        p = &fv; ui.step_page = 0;
    }
    unsigned base = ui.step_page * 16u, shown = p->length > base ? p->length - base : 0;
    int playing = xport == 1 && db.playing;
    unsigned pos = playing ? db.pos : 0xffu;
    if (ui.fill_edit && pos != 0xffu) pos %= DB_FILL_STEPS;
    if (shown > 16) shown = 16;
    for (unsigned part = 0; part < DB_PARTS; ++part) {
        int y = GRID_Y + (int)part * ROW;
        int muted = (db.mute >> part & 1u) || db.muted, selp = part == ui.part && !ui.acc_lane;
        if (y + ROW + cv_oy <= 0 || y + cv_oy >= (int32_t)cv_h) continue;
        if (selp) R(0, y - 1, GRID_X - 2, ROW, 0x3186);
        Q(2, y + 1, 1, db_part_names[part], muted ? FAINT : selp ? TEXT : DIM);
        if (db.mute >> part & 1u) Q(13, y + 1, 1, "M", RED);
        for (unsigned i = 0; i < 16; ++i) {
            int x = GRID_X + (int)i * CELL;
            unsigned s = base + i;
            if (i >= shown) { R(x + 5, y + 2, 2, 2, PANEL); continue; }
            R(x, y, CELL - 2, ROW - 2, s == pos ? 0x52aa : (i & 3u) == 0 ? 0x31a6 : GRID);
            if (p->hit[part] >> s & 1u) {
                uint16_t c = muted ? DIM : part_color[part];
                if (p->accent >> s & 1u) R(x, y, CELL - 2, ROW - 2, c);
                else R(x + 2, y + 1, CELL - 6, ROW - 4, c);
                if (p->roll[part] >> s & 1u) R(x + 4, y + 2, 3, 2, BACK);
                if (!ui.fill_edit && (db_cond(db.current, part, s) || db_prob(db.current, part, s))) R(x, y, 3, 1, TEXT);
                if (!ui.fill_edit && db_vol(db.current, part, s)) R(x, y + ROW - 3, 3, 1, AMBER);
            }
        }
    }
    {
        int y = GRID_Y + DB_PARTS * ROW + 1;
        if (ui.acc_lane) R(0, y - 1, GRID_X - 2, ROW, 0x3186);
        Q(2, y + 1, 1, "AC", ui.acc_lane ? TEXT : DIM);
        for (unsigned i = 0; i < shown; ++i) if (p->accent >> (base + i) & 1u) R(GRID_X + (int)i * CELL, y + 1, CELL - 2, ROW - 4, AMBER);
    }
}
static void draw_home(void)
{
    static const char *const sl[4] = {"A", "B", "C", "D"};
    QC(120, 40, 2, "PATTERN", DIM);
    for (unsigned s = 0; s < DB_PATTERNS; s++) {
        int x = 12 + (int)s * 56, inchain = 0;
        for (unsigned i = 0; i < db.chain_len; i++) inchain |= db.chain[i] == s;
        uint16_t c = s == db.current ? BEATC : s == db.next ? AMBER : inchain ? DIM : FAINT;
        R(x, 60, 48, 48, c);
        PC(x + 24, 72, 3, sl[s], s == db.current ? BACK : TEXT);
        Q(x + 2, 112, 1, slot_empty(s) ? "EMPTY" : db.s.pattern[s].name, DIM);
    }
    QC(120, 140, 1, "WHITE 1-4: PATTERN  HELD + MORE: CHAIN", DIM);
    QC(120, 152, 1, "BLACK 1: BEAT MUTE", DIM);
}
static void draw_footer(void)
{
    char t[24];
    R(0, FOOT_Y - 2, 240, 1, FAINT);
    if (ui.sh_key) {   /* der gehaltene Step: Durchläufe, Wahrscheinlichkeit, Velocity */
        char *q = put(t, "STEP "); fmt_int(q, ui.sh_step + 1); q += str_len(q); *q = 0;
        Q(4, FOOT_Y + 2, 1, t, TEXT);
        Q(4, FOOT_Y + 14, 1, "PRESETS", DIM); P(4, FOOT_Y + 22, 2, cond_names[ui.sh_cond % DB_CONDS], ui.sh_show_prob ? DIM : TEXT);
        Q(84, FOOT_Y + 14, 1, "ALGO", DIM);
        q = t; fmt_int(q, ui.sh_prob ? (int32_t)(10u - ui.sh_prob) * 10 : 100); q += str_len(q); *q++ = '%'; *q = 0;
        P(84, FOOT_Y + 22, 2, t, ui.sh_show_prob ? TEXT : DIM);
        Q(164, FOOT_Y + 14, 1, "KNOB 2 VEL", DIM);
        if (ui.sh_vol) { q = t; fmt_int(q, db_vol_percent(ui.sh_vol)); q += str_len(q); *q++ = '%'; *q = 0; } else str_cpy(t, "AS IS", sizeof t);
        P(164, FOOT_Y + 22, 2, t, ui.sh_lock == 2 ? TEXT : DIM);
        return;
    }
    if (ui.hold & HOLD_GEN) {
        static const char *const keep[2] = {"NEW", "KEEP"};
        Q(4, FOOT_Y + 2, 1, "GENERATOR: WHITE 1 VARY, WHITE 2 NEW", AMBER);
        Q(4, FOOT_Y + 14, 1, "STYLE", DIM); P(4, FOOT_Y + 22, 1, db_style_names[us.gen_style % DB_STYLES], TEXT);
        Q(84, FOOT_Y + 14, 1, "AMOUNT", DIM); fmt_int(t, us.gen_density); P(84, FOOT_Y + 22, 2, t, TEXT);
        Q(164, FOOT_Y + 14, 1, "KICK", DIM); P(164, FOOT_Y + 22, 2, keep[us.gen_keep & 1u], TEXT);
        return;
    }
    {
        const char *title = ui.overview ? "HOME" : ui.page == PG_PART ? db_part_names[ui.part] : page_names[ui.page];
        Q(4, FOOT_Y + 1, 1, title, ui.page == PG_PART && !ui.overview ? part_color[ui.part] : AMBER);
        if (ui.fill_edit) Q(60, FOOT_Y + 1, 1, "FILL EDIT", AMBER);
        if (ui.mute_mode) Q(110, FOOT_Y + 1, 1, "MUTE", RED);
        if (!us.preview_off) {} else Q(140, FOOT_Y + 1, 1, "NO PREVIEW", DIM);
        if (db.s.pattern[db.current].swing > 50) { char *q = put(t, "SW "); fmt_int(q, db.s.pattern[db.current].swing); Q(200, FOOT_Y + 1, 1, t, DIM); }
    }
    for (unsigned i = 0; i < 4u; i++) {
        knob_t k = knob_of(i);
        int x = (int)i * 60;
        if (!*k.name) continue;
        uint16_t c = ui.touched == i && ui.now - ui.touched_at < 1500u ? TEXT : k.color;
        Q(x + 4, FOOT_Y + 10, 1, k.name, DIM);
        P(x + 4, FOOT_Y + 19, str_len(k.text) > 4 ? 1 : 2, k.text, c);
        if (k.max) R(x + 4, FOOT_Y + 36, 52 * k.value / k.max, 3, c);
    }
}
static void draw_all(void)
{
    draw_header();
    if (ui.overview) draw_home(); else draw_grid();
    draw_footer();
    if ((int32_t)(ui.now - ui.notice_until) < 0 && ui.notice) {
        R(0, 222, 240, 18, TEXT);
        QC(120, 228, 1, ui.notice, BACK);
    }
}
enum { DK_BANDS = 240u / GM_BAND_H };
static uint32_t band_sum[DK_BANDS];
static void render(void)
{
    idle_keys = 1;
    for (uint32_t b = 0; b < DK_BANDS; b++) {
        uint32_t sum = 0x1234567u, n = 240u * GM_BAND_H;
        cv_begin(240, GM_BAND_H, BACK);
        cv_oy = -(int32_t)(b * GM_BAND_H);
        draw_all();
        cv_oy = 0;
        for (uint32_t i = 0; i < n; i++) sum = (sum ^ cv_px[i]) * 16777619u;
        if (sum != band_sum[b]) { band_sum[b] = sum; cv_blit(0, b * GM_BAND_H); }
        keys_poll();
    }
    lcd_sync();
    idle_keys = 0;
}

/* ---- Knöpfe und Drehregler (el_ui.c handle_input, Bereich BEAT) */
static void buttons(uint32_t pressed, uint32_t released, uint32_t cur)
{
    unsigned hold = (cur >> BTN_EDIT & 1u ? HOLD_EDIT : 0u) | (cur >> BTN_GLO & 1u ? HOLD_GLO : 0u) | (cur >> BTN_ARP & 1u ? HOLD_GEN : 0u);
    ui.hold = (uint8_t)hold;
    if ((pressed & ~(1u << BTN_ARP)) || (released & (1u << BTN_ARP))) ui.ask = ASK_NONE;
    if (pressed & (1u << BTN_GLO)) ui.glo_tap = 1;
    if (pressed & ~(1u << BTN_GLO)) ui.glo_tap = 0;
    if ((released & (1u << BTN_GLO)) && ui.glo_tap) {
        ui.glo_tap = 0;
        if (!ui.overview) { ui.mute_mode = (uint8_t)!ui.mute_mode; notice(ui.mute_mode ? "MUTE MODE: KEYS MUTE / UNMUTE" : "MUTE MODE OFF"); }
    }
    if (pressed & (1u << BTN_FX)) {
        if (hold & HOLD_GLO) db_post(DB_AREA_MUTE, 0, !db.muted, 0, 0);
        else if (ui.overview) { ui.overview = 0; ui.page = PG_PART; }
        else ui.page = (uint8_t)((ui.page + 1u) % NPG);
    }
    if (pressed & ((1u << BTN_SEL) | (1u << BTN_ENV) | (1u << BTN_LFO))) notice("808 SOLO: ONE AREA (FX)");
    if (pressed & (1u << BTN_HOME)) { ui.overview = (uint8_t)!ui.overview; if (ui.fill_edit) { ui.fill_edit = 0; db_post(DB_FILL_EDIT, 0, 0, 0, 0); } }
    if (pressed & (1u << BTN_PLAY)) {
        if (ext_follow) notice("FOLLOWS MIDI CLOCK");
        else xport_req = xport == 1 ? 2 : 1;
    }
    if (pressed & (1u << BTN_REC)) {
        if (hold & HOLD_EDIT) { db_post(DB_CLEAR_PART, DB_PARTS, 0, 0, 0); notice("PATTERN CLEARED"); }
        else {
            db_post(DB_RECORD, 0, !db.recording, 0, 0);
            ui.rec_at = ui.now ? ui.now : 1u; ui.rec_renewed = 0;
            if (!db.recording && xport != 1) notice("LIVE REC: PLAY TO START");
        }
        keep_later();
    }
    if (!(cur >> BTN_REC & 1u)) ui.rec_at = 0;
    else if (ui.rec_at && !ui.rec_renewed && !ui.overview && ui.now - ui.rec_at >= 1000u) {
        ui.rec_renewed = 1;
        db_post(DB_CLEAR_PART, DB_PARTS, 0, 0, 0); db_post(DB_RECORD, 0, 1, 0, 0);
        notice("NEW TAKE: PATTERN CLEARED, REC ON");
    }
    if (pressed & (1u << BTN_SAVE)) notice(dk_save() == 0 ? "SAVED" : "SAVE FAILED");
    /* SEQ: Fill im nächsten Takt (gehalten: jeden Takt), GLO + SEQ automatisch, EDIT + SEQ bearbeiten (N12) */
    if ((pressed & (1u << BTN_SEQ)) && (hold & HOLD_GLO)) {
        static const uint8_t every[4] = {0, 8, 16, 32};
        unsigned k = 0;
        while (k < 4u && every[k] != db.s.fill_auto) ++k;
        unsigned nx = every[(k + 1u) % 4u];
        if (db_post(DB_FILL_AUTO, 0, (uint8_t)nx, 0, 0) == 0)
            notice(nx == 8 ? "FILL EVERY 8 BARS" : nx == 16 ? "FILL EVERY 16 BARS" : nx == 32 ? "FILL EVERY 32 BARS" : "FILL AUTO OFF");
    } else if ((pressed & (1u << BTN_SEQ)) && (hold & HOLD_EDIT) && !ui.overview) {
        ui.fill_edit = (uint8_t)!ui.fill_edit;
        db_post(DB_FILL_EDIT, 0, ui.fill_edit, 0, 0);
        notice(ui.fill_edit ? "FILL EDIT: STEP KEYS 1-16" : "FILL EDIT OFF");
    } else if (pressed & (1u << BTN_SEQ)) {
        if (db_post(DB_FILL, 0, 1, 0, 0) == 0) notice("FILL IN THE NEXT BAR");
        ui.seq_at = ui.now ? ui.now : 1u; ui.seq_held_fill = 0;
    }
    if ((cur >> BTN_SEQ & 1u) && ui.seq_at && !ui.seq_held_fill && ui.now - ui.seq_at >= 300u && db_post(DB_FILL, 0, 2, 0, 0) == 0) ui.seq_held_fill = 1;
    if (!(cur >> BTN_SEQ & 1u) && ui.seq_at) { if (ui.seq_held_fill) db_post(DB_FILL, 0, 0, 0, 0); ui.seq_at = 0; ui.seq_held_fill = 0; }
    if (pressed & (1u << BTN_OCT_UP)) { us.preview_off = (uint8_t)!us.preview_off; notice(us.preview_off ? "DRUM PREVIEW OFF" : "DRUM PREVIEW ON"); }
    if ((pressed & (1u << BTN_OCT_DOWN)) && db.s.pattern[db.current].length > 16 && !ui.fill_edit) ui.step_page = (uint8_t)!ui.step_page;
    if ((cur & (1u << BTN_OCT_DOWN)) && (cur & (1u << BTN_OCT_UP)) && (pressed & ((1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP)))) {
        db_post(DB_PANIC, 0, 0, 0, 0); notice("ALL NOTES OFF");
    }
    if (pressed) keep_later();
}
static void encoders(void)
{
    int32_t sel_e = fm1_enc_take(ENC_SELECT), algo = fm1_enc_take(ENC_ALGORITHM), pre = fm1_enc_take(ENC_PRESETS), k[4];
    unsigned hold = ui.hold;
    for (unsigned i = 0; i < 4u; i++) k[i] = fm1_enc_take(ENC_KNOB1 + i);
    if (sel_e || algo || pre || k[0] || k[1] || k[2] || k[3]) { ui.glo_tap = ui.arp_tap = 0; keep_later(); }
    if (ui.sh_key) {   /* der gehaltene Step: PRESETS Durchläufe, ALGORITHM Wahrscheinlichkeit, KNOB 2 Velocity */
        if (sel_e || algo || pre || k[0] || k[1] || k[2] || k[3]) ui.sh_used = 1;
        if (k[1]) {
            int from = ui.sh_vol ? (int)ui.sh_vol : ((db.s.pattern[db.current].accent >> ui.sh_step) & 1u) ? 7 : 5;
            int level = clampi(from + (k[1] > 0 ? 1 : -1), 1, 7);
            if (db_post(DB_VOL, ui.sh_part, ui.sh_step, 0, (int16_t)level) == 0) {
                ui.sh_vol = (uint8_t)level;
                db_post(DB_HIT, ui.sh_part, (uint8_t)(db_vol_percent((unsigned)level) * 127 / 100), 0, 0);
            }
            ui.sh_lock = 2; k[1] = 0;
        }
        if (algo) {
            unsigned prob = (unsigned)clampi((int)ui.sh_prob - algo, 0, DB_PROBS - 1);
            if (db_post(DB_PROB, ui.sh_part, ui.sh_step, 0, (int16_t)prob) == 0) ui.sh_prob = (uint8_t)prob;
            ui.sh_show_prob = 1; ui.sh_lock = 0; algo = 0;
        }
        if (pre) {
            unsigned cond = (unsigned)clampi((int)ui.sh_cond + pre, 0, DB_CONDS - 1);
            if (db_post(DB_COND, ui.sh_part, ui.sh_step, 0, (int16_t)cond) == 0) ui.sh_cond = (uint8_t)cond;
            ui.sh_show_prob = 0; ui.sh_lock = 0; pre = 0;
        }
        k[0] = 0;   /* (Tonhöhe je Step: dk808 kennt keine; KNOB 1 ruht) */
    }
    if (sel_e) {
        if ((hold & HOLD_GLO) && !ui.overview) {   /* GLO + SELECT: das Instrument (alle 17), ohne es zu spielen */
            ui.part = (uint8_t)clampi((int)ui.part + sel_e, 0, DB_PARTS - 1); ui.page = PG_PART; ui.acc_lane = 0;
        } else db_post(DB_SELECT, (uint8_t)clampi((int)db.next + sel_e, 0, DB_PATTERNS - 1), 0, 0, 0);
    }
    if (algo) {
        const db_pattern_t *p = &db.s.pattern[db.current];
        if (ui.overview || !(hold & (HOLD_GLO | HOLD_EDIT))) {
            us.bpm_x10 = (uint16_t)clampi(us.bpm_x10 + algo * ((hold & HOLD_GLO) ? 1 : 10), 400, 2400);
            ev_put(EV_TEMPO, 0, (uint32_t)clampi((us.bpm_x10 + 5) / 10 - 40, 0, 200), 0);
            if (ext_follow) notice("FOLLOWS MIDI CLOCK");
        } else if (hold & HOLD_GLO) {   /* die Länge in ganzen Takten (N11): 1 oder 2 */
            int bars = clampi(((int)p->length + 15) / 16 + (algo > 0 ? 1 : -1), 1, 2);
            db_post(DB_LENGTH, 0, (uint8_t)(bars * 16), 0, 0);
            notice(bars == 1 ? "LENGTH 1 BAR" : "LENGTH 2 BARS");
        } else {
            db_post(DB_SWING, 0, (uint8_t)clampi(p->swing + algo, 50, 75), 0, 0);
        }
    }
    if (hold & HOLD_GEN) {
        if (k[0]) us.gen_style = (uint8_t)clampi(us.gen_style + k[0], 0, DB_STYLES - 1);
        if (k[1]) us.gen_density = (uint8_t)clampi(us.gen_density + k[1] * 4, 0, 127);
        if (k[2]) us.gen_keep = k[2] > 0;
    } else if (ui.overview && (hold & HOLD_EDIT)) {   /* HOME + EDIT: Distortion (wie FM1-Electro) */
        if (k[0]) knob_set(3, us.dist_send + k[0] * 2);
        if (pre) { us.dist_mode = (uint8_t)clampi(us.dist_mode + (pre > 0 ? 1 : -1), 0, 3); dist_v[0] = us.dist_mode; }
        pre = 0;
    } else {
        for (unsigned i = 0; i < 4u; i++) knob_turn(i, k[i]);
    }
    if (pre) {
        if (hold & HOLD_EDIT) { if (db_post(DB_KIT, db.s.kit_index, 1, 0, 0) == 0) notice("KIT: FACTORY SOUND"); }
        else db_post(DB_KIT, (uint8_t)clampi((int)db.s.kit_index + (pre > 0 ? 1 : -1), 0, DB_KITS - 1), 0, 0, 0);
    }
}

static void app_main(void)
{
    uint32_t t_frame = 0;
    db_init();
    settings_default();
    if (dk_load() == 0) settings_check();
    db_restore();
    push_settings();
    ui.saved_rev = ui.seen_rev = db.revision;
    ui.part = P_BD;
    lcd_fill(0, 0, 240, 240, BACK);
    for (uint32_t p = 0; p < FM1_NCOL; p++)   /* (die Hintergrundbeleuchtung der Tasten wie im Gerüst) */
        for (uint32_t r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] >= 0) fm1_led_bg[p] |= (uint8_t)(1u << r);
    fm1_led_bg_ns = 8000u;
    notice(GM_TITLE " V" APP_VER);
    for (;;) {
        uint32_t rel, pressed;
        platform_poll();
        ui.now = fm1_ms;
        if (tempo_taken) { uint32_t tt = tempo_taken; tempo_taken = 0; us.bpm_x10 = (uint16_t)tt; notice("TEMPO KEPT"); }
        pressed = fm1_input_edges(&rel);
        buttons(pressed, rel, fm1_in.buttons);
        keys_poll();
        encoders();
        lights();
        if (db.revision != ui.seen_rev) { ui.seen_rev = db.revision; ui.changed_at = ui.now; }
        if (ui.now - t_frame >= 33u) { t_frame = ui.now; render(); }
        /* von selbst speichern: 2 s nach einer Änderung, aber nur gestoppt (das Löschen des Flash setzt den Klang aus) */
        if (ui.autosave && (int32_t)(ui.now - ui.autosave_at) >= 0 && xport != 1 && db.staging_posted == db.staging_taken &&
            db.q_head == db.q_tail) {
            ui.autosave = 0;
            if (dk_save() != 0) notice("SAVE FAILED");
        }
    }
}
