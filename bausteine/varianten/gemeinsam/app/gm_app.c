/* SPDX-License-Identifier: GPL-3.0-only */
/* Gemeinsames Gerüst der Solo- und Kombi-Firmwares aus bausteine/ (ADR 0044). Abgeleitet aus fm1-synths
 * (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa): mks50/app/mk_app.c und mk_ui.c (MKS-50-Firmware), die ihrerseits
 * auf der Plattform Dinghy / Felucca (Leo Kuroshita, Hügelton Instruments, mit SLOOPs und FM1-Electros Änderungen,
 * GPL-3.0-only; platform/ORIGIN.md) laufen.
 *
 * Eine Variante (z. B. varianten/sh101/app/sh_app.c) legt fest, was die Firmware spielt, und bindet danach diese Datei ein:
 *
 *   GM_PARTS, GM_NP, GM_NAMEMAX, GM_SOUND_VER     Parts, Parameter je Sound, Namenslänge, Version des Soundformats
 *   gm_params[GM_NP]   {Name, Höchstwert, Farbklasse, Textfunktion}; Werte im Speicher als uint8_t 0..Höchstwert
 *   gm_init_v[GM_NP]   der Grundklang (INIT), auch Füllwert, wenn ein älterer Sound weniger Parameter hat
 *   gm_factory[], GM_NFAC, gm_cat_name[GM_NCAT]   Werkssounds und Gruppen
 *   gm_pages[], gm_layers[NGROUP]                  Seiten (KNOB 1–4 = vier Felder) und die Ebenen gehaltener Tasten
 *   kern_boot, kern_load, kern_set, kern_on, kern_off, kern_all_off, kern_bend, kern_mod, kern_render, kern_voices
 *   kern_stage(stage, part)    eigenes Bild einer Seite (0 = keins, das Gerüst zeigt dann den Seitennamen)
 *
 * Das Gerüst bringt mit: zweiter Kern (Delay und Hall, später Stimmen), Ereignis-Warteschlange zum Audio-Interrupt,
 * MIDI (USB und TRS, Kanal je Part, Pitch Bend, Modulationsrad), Takt (intern / MIDI-Clock, Start/Stop), einen
 * Arpeggiator und den Step-Sequencer wie beim SH-101 (je Part), Werks- und eigene Sounds (Blob 0), den Stand beim
 * Ausschalten (Journal), Lastmessung (Seite SYSTEM) und die Oberfläche nach fm1-synths ADR 0008:
 *   KNOB 1–4  die vier Felder unten      SELECT  Seite      PRESETS  Sound      ALGORITHM  Part
 *   eine Seitentaste tippen: ihre Seiten; halten: ihre vier wichtigsten Werte auf KNOB 1–4
 *   SAVE kurz: speichern; lang: speichern unter   HOME lang: ungespeicherte Änderung zurückholen
 *   PLAY halten: Start/Stop   REC: Schritte aufnehmen   SEQ: Sequencer an/aus   OCT− + OCT+: alle Töne aus */
#ifndef GM_PARTS
#error "Variante: GM_PARTS usw. vor gm_app.c festlegen"
#endif
#define HALF_FRAMES GM_BLOCK
static void app_idle(void);
#define LCD_IDLE app_idle
static void gm_boot(void);
#define APP_BOOT gm_boot
#ifndef GM_BAND_H
#define GM_BAND_H 30u                           /* das Bild in Streifen von so vielen Zeilen (240 / GM_BAND_H ganz) */
#endif
#define CV_MAX (240u * GM_BAND_H)
#include "platform.c"
#include "syn_fx.h"
/* GM_FX 0: eine Variante mit eigenen Effekten (MKS Ultra, ADR 0045) baut syn_fx nicht (spart ~190 kB Pool); die Seiten
 * K_FX zeigen dann nichts, die Effekte bleiben aus, und Kern 1 bleibt für die Variante frei. Standard 1 = wie bisher. */
#ifndef GM_FX
#define GM_FX 1
#endif
#if !GM_FX
#define syn_fx_init() ((void)0)
#define syn_fx_set(p, v) ((void)(p), (void)(v))
#define syn_fx_process(a, b, c, d, n, bpm) ((void)0)
#define syn_fx_max(p) ((void)(p), 0)
#define syn_fx_default(p) ((void)(p), 0)
#define syn_fx_value_name(p, v) ((void)(p), (void)(v), (const char *)0)
#endif
#include "syn_font.h"
#include "gm_theme.h"

#define DSP __attribute__((section(".dsp_text"), noinline))
#define EL_SR 44100.0f

/* ======================================================================================================
 * Der zweite Kern (hal/fm1_cpu1.h, wie fm1-synths mks50): beim Einschalten gestartet, mit einem Probeauftrag geprüft;
 * bei einem Fehler hält die Plattform ihn an und alles läuft auf Kern 0.
 * ====================================================================================================== */
static uint8_t cpu1_ok;
static volatile uint32_t cpu1_test_out;
static uint32_t cpu1_lcg(uint32_t x)
{
    for (uint32_t i = 0; i < 64u; i++) x = x * 1664525u + 1013904223u;
    return x;
}
static void cpu1_selftest(uint32_t arg) { cpu1_test_out = cpu1_lcg(arg); }

/* ======================================================================================================
 * Ereignisse vom Hauptprogramm zum Audio-Interrupt (ein Schreiber, ein Leser)
 * ====================================================================================================== */
static uint8_t ready;
enum { EV_ON, EV_OFF, EV_PANIC, EV_SET, EV_LEVEL, EV_FX, EV_ARP, EV_TEMPO, EV_CLK, EV_SEQ, EV_SEQ_LEN, EV_SEND };
typedef struct { uint8_t type, part, a, b; } gm_ev_t;
#define EVQ 128u
static gm_ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_put(uint32_t type, uint32_t part, uint32_t a, uint32_t b)
{
    uint32_t w = ev_w, n;
    for (n = 0; w - ev_r >= EVQ && n < 200000u; n++) {}
    if (w - ev_r >= EVQ) return;
    evq[w % EVQ] = (gm_ev_t){(uint8_t)type, (uint8_t)part, (uint8_t)a, (uint8_t)b};
    FM1_C1_SYNC();
    ev_w = w + 1u;
}
/* ein ganzer Sound für einen Part: geschrieben, dann markiert; der Audio-Interrupt übernimmt ihn */
static uint8_t pend_v[GM_PARTS][GM_NP];
static volatile uint8_t pend_flag[GM_PARTS];
static volatile uint8_t part_ch[GM_PARTS];      /* MIDI-Kanal je Part: 0 = alle (OMNI), 1..16 */
static float part_level[GM_PARTS], part_send[GM_PARTS][2];

static void gm_boot(void)
{
    kern_boot();
    syn_fx_init();
    for (unsigned p = 0; p < GM_PARTS; p++) { part_ch[p] = (uint8_t)(GM_PARTS == 1 ? 0 : p + 1); part_level[p] = 0.8f; }
    fm1_guard_pc_open();
    int started = fm1_cpu1_start() == 0;
    fm1_guard_enable(FM1_GUARD_PC);
    if (started) {
        cpu1_test_out = 0;
        if (!fm1_cpu1_run(cpu1_selftest, 20261128u) || fm1_cpu1_wait() != 0 || cpu1_test_out != cpu1_lcg(20261128u))
            fm1_cpu1_hold();
        else
            cpu1_ok = 1;
    }
    ready = 1;
}

/* ======================================================================================================
 * Last: Takte je Block (24 je Mikrosekunde, beide Kerne lesen denselben Zeitgeber)
 * ====================================================================================================== */
#define BLOCK_TICKS ((uint32_t)(24000000ull * GM_BLOCK / 44100u))
enum { M_ALL, M_K0, M_K1, M_VOICE, M_FX, M_N };
static struct { uint32_t sum[M_N], peak[M_N], blocks, late; } meas;
static volatile uint32_t c1_fx_t;
static uint32_t block_no;
static DSP void m_add(uint32_t i, uint32_t t)
{
    meas.sum[i] += t;
    if (t > meas.peak[i]) meas.peak[i] = t;
}

/* ======================================================================================================
 * Takt (fm1-synths ADR 0010, 0012): TEMPO oder die MIDI-Clock, Start/Stop
 * ====================================================================================================== */
static volatile float bpm_now = 120.0f;
static float tempo_set = 120.0f;
static uint32_t clk_t_last, clk_t24, clk_n;
static volatile uint8_t clk_running;
static volatile uint32_t clk_ticks;
enum { SRC_AUTO, SRC_INT, SRC_MIDI };
static uint8_t clk_src, clk_send;
static volatile uint8_t xport, xport_req, ext_follow;
static volatile uint16_t tempo_taken;
static float int_acc;

/* Delay und Hall: gespeist von den Sends des Blocks davor, gerechnet auf Kern 1 (oder Kern 0, wenn Kern 1 fehlt) */
static float fx_in[2][GM_BLOCK], fx_wet[2][GM_BLOCK];
static uint8_t fx_on;
/* GM_FX_EXTRA(wl, wr): ein weiterer Send-Effekt der Variante (SH-101: Distortion), läuft mit Delay und Hall (Kern 1,
 * sonst Kern 0) und addiert sein nasses Signal; GM_FX_EXTRA_ON(): ist er gerade an. Standard: keiner. */
#ifndef GM_FX_EXTRA
#define GM_FX_EXTRA(wl, wr) ((void)0)
#define GM_FX_EXTRA_ON() 0
#endif
static DSP void fx_run(void)
{
    for (uint32_t i = 0; i < GM_BLOCK; i++) fx_wet[0][i] = fx_wet[1][i] = 0;
    if (fx_on) syn_fx_process(fx_in[0], fx_in[1], fx_wet[0], fx_wet[1], GM_BLOCK, bpm_now);
    GM_FX_EXTRA(fx_wet[0], fx_wet[1]);
}
static DSP void c1_job(uint32_t blk)
{
    uint32_t t0 = FM1_C1_T4;
    (void)blk;
    fx_run();
    c1_fx_t = FM1_C1_T4 - t0;
    FM1_C1_SYNC();
}

/* ======================================================================================================
 * Arpeggiator je Part (aus fm1-synths mks50, ohne Akkorde): UP DOWN UP/DN RAND ORDER, 1–4 Oktaven, Notenwerte wie das
 * Delay, Gate 5..100 %, HOLD hält das zuletzt Gespielte
 * ====================================================================================================== */
static const float arp_q[FX_DIVS] = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3, 1, 1.5f, 2, 4};
static const uint8_t arp_ticks[FX_DIVS] = {3, 4, 6, 9, 8, 12, 18, 16, 24, 36, 48, 96};
typedef struct {
    uint8_t cfg[ARP_N];
    uint8_t keys[16], nkeys, phys, vel;
    int16_t sounding;
    uint32_t pos, since, gate_len, rng;
    uint8_t fresh;
} arp_t;
static arp_t arp[GM_PARTS];
static volatile uint8_t arp_view[GM_PARTS][24], arp_view_n[GM_PARTS], arp_view_at[GM_PARTS];

static DSP void arp_stop_note(arp_t *a, unsigned p)
{
    if (a->sounding >= 0) { kern_off(p, a->sounding); a->sounding = -1; }
}
static DSP int arp_pool(unsigned p, uint8_t *out)
{
    arp_t *a = &arp[p];
    uint8_t base[16];
    int n = a->nkeys, m = 0, o = a->cfg[ARP_OCT] + 1;
    for (int k = 0; k < n; k++) base[k] = a->keys[k];
    if (a->cfg[ARP_MODE] != 4)
        for (int i = 1; i < n; i++) for (int j = i; j > 0 && base[j] < base[j - 1]; j--) { uint8_t t = base[j]; base[j] = base[j - 1]; base[j - 1] = t; }
    for (int oo = 0; oo < o; oo++) for (int i = 0; i < n && m < 64; i++) { int x = base[i] + 12 * oo; if (x < 128) out[m++] = (uint8_t)x; }
    return m;
}
static DSP void arp_step(unsigned p)
{
    arp_t *a = &arp[p];
    uint8_t pool[64];
    int n = arp_pool(p, pool), i;
    arp_stop_note(a, p);
    if (!n) return;
    if (a->fresh) a->pos = 0;
    switch (a->cfg[ARP_MODE]) {
    case 1: i = n - 1 - (int)(a->pos % (uint32_t)n); break;
    case 2: { int per = n > 1 ? 2 * n - 2 : 1, q = (int)(a->pos % (uint32_t)per); i = q < n ? q : per - q; break; }
    case 3:
        if (!a->rng) a->rng = ((fm1_ticks() * 2654435761u) ^ (0x9E3779B9u + p)) | 1u;
        a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5; i = (int)(a->rng % (uint32_t)n); break;
    default: i = (int)(a->pos % (uint32_t)n); break;
    }
    a->pos++;
    a->fresh = 0;
    a->sounding = pool[i];
    kern_on(p, a->sounding, a->vel, 0);
    a->since = 0;
    {
        int m = n < 24 ? n : 24;
        for (int k = 0; k < m; k++) arp_view[p][k] = pool[k];
        arp_view_n[p] = (uint8_t)m; arp_view_at[p] = (uint8_t)(i < 24 ? i : 23);
    }
}
static DSP void arp_key(unsigned p, int note, int vel)
{
    arp_t *a = &arp[p];
    if (vel) {
        if (a->cfg[ARP_HOLD] && a->phys == 0) a->nkeys = 0;
        for (int k = 0; k < a->nkeys; k++) if (a->keys[k] == note) { for (int j = k + 1; j < a->nkeys; j++) a->keys[j - 1] = a->keys[j]; a->nkeys--; break; }
        if (a->nkeys == 16) { for (int j = 1; j < 16; j++) a->keys[j - 1] = a->keys[j]; a->nkeys--; }
        if (a->nkeys == 0) a->fresh = 1;
        a->keys[a->nkeys++] = (uint8_t)note;
        a->vel = (uint8_t)vel;
        if (a->phys < 255) a->phys++;
    } else {
        if (a->phys) a->phys--;
        if (!a->cfg[ARP_HOLD])
            for (int k = 0; k < a->nkeys; k++) if (a->keys[k] == note) { for (int j = k + 1; j < a->nkeys; j++) a->keys[j - 1] = a->keys[j]; a->nkeys--; break; }
        if (!a->nkeys) { arp_stop_note(a, p); arp_view_n[p] = 0; }
    }
}
static DSP void arp_set(unsigned p, unsigned prm, int v)
{
    arp_t *a = &arp[p];
    if (prm >= ARP_N) return;
    if (prm == ARP_ON && !v && a->cfg[ARP_ON]) { arp_stop_note(a, p); a->nkeys = 0; a->phys = 0; arp_view_n[p] = 0; }
    if (prm == ARP_HOLD && !v && a->cfg[ARP_HOLD] && !a->phys) { arp_stop_note(a, p); a->nkeys = 0; arp_view_n[p] = 0; }
    a->cfg[prm] = (uint8_t)v;
}
static DSP void arp_block(unsigned ticks)
{
    for (unsigned p = 0; p < GM_PARTS; p++) {
        arp_t *a = &arp[p];
        uint32_t step;
        if (!a->cfg[ARP_ON] || !a->nkeys) continue;
        step = (uint32_t)(arp_q[a->cfg[ARP_DIV] % FX_DIVS] * 60.0f / bpm_now * EL_SR);
        if (step < GM_BLOCK) step = GM_BLOCK;
        a->gate_len = (uint32_t)((0.05f + 0.95f * a->cfg[ARP_GATE] / 127.0f) * (float)step);
        if (xport == 2) { a->since += GM_BLOCK; if (a->sounding >= 0 && a->since >= a->gate_len) arp_stop_note(a, p); continue; }
        if (a->fresh) { arp_step(p); continue; }
        if (ext_follow || xport == 1) {
            for (unsigned t = 0; t < ticks; t++) if ((clk_ticks - ticks + t + 1u) % arp_ticks[a->cfg[ARP_DIV] % FX_DIVS] == 0) arp_step(p);
        } else if (a->since + GM_BLOCK >= step) arp_step(p);
        a->since += GM_BLOCK;
        if (a->sounding >= 0 && a->since >= a->gate_len) arp_stop_note(a, p);
    }
}

/* ======================================================================================================
 * Step-Sequencer je Part wie beim SH-101 (fm1-synths ADR 0015): bis 64 Schritte, je Note, Pause oder Bindung zum
 * nächsten (legato); Aufnahme Schritt für Schritt (REC), Wiedergabe im Raster des Takts (RATE, GATE), transponiert
 * von den Tasten, während er läuft (C4 = 0). Gespeichert in Blob 3, in einer Spielpause.
 * ====================================================================================================== */
enum { SEQ_STEPS = 64, SQ_REST = 1, SQ_TIE = 2 };
enum { SQ_VER = 1 };
static struct {
    uint8_t ver, len[GM_PARTS];
    uint8_t cfg[GM_PARTS][SQ_N];
    uint8_t note[GM_PARTS][SEQ_STEPS], flag[GM_PARTS][SEQ_STEPS];
} sq;
typedef struct { uint8_t pos, tied, fresh; int16_t sounding; uint32_t since, gate_len; } sqrun_t;
static sqrun_t sqr[GM_PARTS];
static volatile uint8_t seq_rec_p, rec_mn[32], rec_mv[32], rec_mw, rec_mr;
static volatile uint8_t sq_view_at[GM_PARTS];
static DSP void seq_stop_note(unsigned p)
{
    if (sqr[p].sounding >= 0) { kern_off(p, sqr[p].sounding); sqr[p].sounding = -1; }
    sqr[p].tied = 0;
}
static DSP void seq_step(unsigned p)
{
    sqrun_t *s = &sqr[p];
    unsigned n = sq.len[p] <= SEQ_STEPS ? sq.len[p] : SEQ_STEPS, i, f;
    int note;
    if (!n) { seq_stop_note(p); return; }
    if (s->fresh) { s->pos = 0; s->fresh = 0; }
    i = s->pos % n;
    s->pos = (uint8_t)((i + 1u) % n);
    f = sq.flag[p][i];
    note = (int)sq.note[p][i] + (int)sq.cfg[p][SQ_SHIFT] - 24;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (f & SQ_REST) seq_stop_note(p);
    else if (s->sounding >= 0 && s->tied) {     /* gebunden: legato, wie das SH-101 (Glide-Modus AUTO gleitet hier) */
        if (note != s->sounding) { kern_on(p, note, 100, 1); kern_off(p, s->sounding); }
        s->sounding = (int16_t)note;
    } else {
        seq_stop_note(p);
        kern_on(p, note, 100, 0);
        s->sounding = (int16_t)note;
    }
    s->tied = (uint8_t)((f & SQ_TIE) && !(f & SQ_REST));
    s->since = 0;
    sq_view_at[p] = (uint8_t)i;
}
static DSP void seq_set(unsigned p, unsigned prm, int v)
{
    if (prm >= SQ_N) return;
    if (prm == SQ_ON && !v) seq_stop_note(p);
    if (prm == SQ_ON && v && !sq.cfg[p][SQ_ON]) { sqr[p].fresh = 1; sqr[p].since = 0; }
    sq.cfg[p][prm] = (uint8_t)v;
}
static DSP void seq_block(unsigned ticks)
{
    for (unsigned p = 0; p < GM_PARTS; p++) {
        sqrun_t *s = &sqr[p];
        uint32_t step;
        unsigned div = sq.cfg[p][SQ_DIV] % FX_DIVS;
        if (!sq.cfg[p][SQ_ON] || !sq.len[p]) continue;
        step = (uint32_t)(arp_q[div] * 60.0f / bpm_now * EL_SR);
        if (step < GM_BLOCK) step = GM_BLOCK;
        s->gate_len = (uint32_t)((0.05f + 0.95f * sq.cfg[p][SQ_GATE] / 127.0f) * (float)step);
        if (xport == 2) { s->since += GM_BLOCK; if (s->sounding >= 0 && s->since >= s->gate_len) seq_stop_note(p); continue; }
        if (s->fresh) { seq_step(p); continue; }
        if (ext_follow || xport == 1) {
            for (unsigned t = 0; t < ticks; t++) if ((clk_ticks - ticks + t + 1u) % arp_ticks[div] == 0) seq_step(p);
        } else if (s->since + GM_BLOCK >= step) seq_step(p);
        s->since += GM_BLOCK;
        if (s->sounding >= 0 && !s->tied && s->since >= s->gate_len) { kern_off(p, s->sounding); s->sounding = -1; }
    }
}

static DSP void play_on(unsigned p, int note, int vel)
{
    if (sq.cfg[p][SQ_ON] && sq.len[p] && seq_rec_p != p + 1u) {   /* der Sequencer läuft: die Taste transponiert */
        int t = note - 60 + 24;
        sq.cfg[p][SQ_SHIFT] = (uint8_t)(t < 0 ? 0 : t > 48 ? 48 : t);
        return;
    }
    if (arp[p].cfg[ARP_ON]) arp_key(p, note, vel ? vel : 1);
    else kern_on(p, note, vel, 0);
}
static DSP void play_off(unsigned p, int note)
{
    if (sq.cfg[p][SQ_ON] && sq.len[p] && seq_rec_p != p + 1u) return;
    if (arp[p].cfg[ARP_ON]) arp_key(p, note, 0);
    else kern_off(p, note);
}
static DSP void xport_start(void)
{
    for (unsigned p = 0; p < GM_PARTS; p++) { sqr[p].fresh = 1; arp[p].pos = 0; if (arp[p].nkeys) arp[p].fresh = 1; }
    clk_ticks = 0;
    int_acc = 0;
    xport = 1;
}
static DSP void xport_stop(void)
{
    for (unsigned p = 0; p < GM_PARTS; p++) { seq_stop_note(p); arp_stop_note(&arp[p], p); }
    xport = 2;
}

static uint32_t midi_ticks;
static volatile uint8_t midi_hit[GM_PARTS];
static DSP void midi_to_synth(void)
{
    uint8_t st, d1, d2;
    int src;
    while ((src = midi_get(&st, &d1, &d2)) != 0) {
        uint32_t ty = st & 0xF0u, ch = st & 15u;
        if (st >= 0xF0u) {
            int follow = clk_src != SRC_INT;
            if (clk_send && src == 2) {
                if (st == 0xF2u) midi_out_event(3u | 0xF2u << 8 | (uint32_t)d1 << 16 | (uint32_t)d2 << 24);
                else if (st >= 0xF8u) midi_out_event(0xFu | (uint32_t)st << 8);
            }
            if (st == 0xF8u) {
                uint32_t now = fm1_ticks();
                clk_t_last = now; midi_ticks++;
                if (follow && xport != 2) clk_ticks++;
                if (++clk_n >= 24u) {
                    uint32_t dt = now - clk_t24;
                    clk_n = 0; clk_t24 = now;
                    float b = 60.0f * 24000000.0f / (float)(dt | 1u);   /* (nie 0) */
                    float d = b - bpm_now;
                    if (follow && b > 20.0f && b < 400.0f && (d > 0.003f * bpm_now || d < -0.003f * bpm_now))
                        bpm_now = (float)(int)(b * 10.0f + 0.5f) * 0.1f;
                }
            } else if (!follow) {
            } else if (st == 0xFAu) { clk_n = 0; clk_t24 = fm1_ticks(); xport_start(); }
            else if (st == 0xFBu) xport = 1;
            else if (st == 0xFCu) xport_stop();
            else if (st == 0xF2u) clk_ticks = ((uint32_t)d1 | (uint32_t)d2 << 7) * 6u;
            continue;
        }
        for (uint32_t p = 0; p < GM_PARTS; p++) {
            uint32_t c = part_ch[p];
            if (c && c - 1u != ch) continue;
            midi_hit[p]++;
            if (seq_rec_p == p + 1u && (ty == 0x90u || ty == 0x80u)) {
                uint8_t w = rec_mw;
                rec_mn[w & 31u] = d1; rec_mv[w & 31u] = (uint8_t)(ty == 0x90u ? d2 : 0u); rec_mw = (uint8_t)(w + 1u);
            }
            if (ty == 0x90u && d2) play_on(p, d1, d2);
            else if (ty == 0x80u || ty == 0x90u) play_off(p, d1);
            else if (ty == 0xE0u) kern_bend(p, (float)((int32_t)((uint32_t)d1 | (uint32_t)d2 << 7) - 8192) * (1.0f / 8192.0f));
            else if (ty == 0xB0u && d1 == 1u) kern_mod(p, (float)d2 * (1.0f / 127.0f));
            else if (ty == 0xB0u && (d1 == 123u || d1 == 120u)) { arp_set(p, ARP_ON, arp[p].cfg[ARP_ON]); kern_all_off(p); }
        }
    }
}

static DSP void events_to_synth(void)
{
    for (uint32_t p = 0; p < GM_PARTS; p++)
        if (pend_flag[p]) {
            kern_load(p, pend_v[p]);
            FM1_C1_SYNC();
            pend_flag[p] = 0;
        }
    while (ev_r != ev_w) {
        gm_ev_t e = evq[ev_r % EVQ];
        ev_r++;
        if (e.part >= GM_PARTS) continue;
        switch (e.type) {
        case EV_ON: play_on(e.part, e.a, e.b); break;
        case EV_OFF: play_off(e.part, e.a); break;
        case EV_PANIC:
            seq_stop_note(e.part);
            arp[e.part].nkeys = 0; arp[e.part].phys = 0; arp_stop_note(&arp[e.part], e.part); arp_view_n[e.part] = 0;
            kern_all_off(e.part);
            break;
        case EV_FX: syn_fx_set(e.a, e.b); break;
        case EV_ARP: arp_set(e.part, e.a, e.b); break;
        case EV_TEMPO: tempo_set = (float)(e.a + 40u); break;
        case EV_CLK: if (e.a == 0) clk_src = e.b; else clk_send = e.b; break;
        case EV_SEQ: seq_set(e.part, e.a, e.b); break;
        case EV_SEQ_LEN:
            if (!e.b || !e.a) seq_stop_note(e.part);
            if (!e.b) sqr[e.part].fresh = 1;
            sq.len[e.part] = e.a;
            break;
        case EV_SET: kern_set(e.part, e.a, e.b); break;
        case EV_LEVEL: part_level[e.part] = (float)e.a * (1.0f / 127.0f); break;
        case EV_SEND: part_send[e.part][e.a & 1u] = (float)e.b * (1.0f / 127.0f); break;
        default: break;
        }
    }
}

/* ======================================================================================================
 * Der Klang: der Audio-Interrupt der Plattform, GM_BLOCK Frames
 * ====================================================================================================== */
static float mix_l[GM_BLOCK], mix_r[GM_BLOCK], part_l[GM_BLOCK], part_r[GM_BLOCK];
static int8_t scope_buf[512];
static volatile uint32_t scope_w;
#ifndef GM_OUT_GAIN
#define GM_OUT_GAIN (32767.0f * 0.6f)
#endif
static volatile uint32_t gaps, last_start, silent_blocks;
static DSP void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0, t, wait = 0, started = 0, loud = 0, tv;
    if (!ready || n != GM_BLOCK) return;
    t0 = fm1_ticks();
    if (last_start && t0 - last_start > BLOCK_TICKS + BLOCK_TICKS / 2u) gaps++;
    last_start = t0;
    midi_ticks = 0;
    midi_to_synth();
    events_to_synth();
    clk_running = (uint8_t)(fm1_ticks() - clk_t_last < 24000u * 250u && clk_t_last);
    {
        uint8_t was = ext_follow;
        ext_follow = (uint8_t)(clk_src == SRC_MIDI || (clk_src == SRC_AUTO && clk_running));
        if (was && !ext_follow) {
            float b = bpm_now < 40.0f ? 40.0f : bpm_now > 240.0f ? 240.0f : bpm_now;
            tempo_set = b;
            tempo_taken = (uint16_t)(b * 10.0f + 0.5f);
        }
    }
    {
        unsigned ticks = midi_ticks;
        if (!ext_follow) {
            float tick_len = EL_SR * 60.0f / (tempo_set * 24.0f);   /* (tempo_set >= 40) */
            bpm_now = tempo_set;
            if (xport_req == 1u) { xport_start(); if (clk_send) midi_out_event(0xFu | 0xFAu << 8); }
            else if (xport_req == 2u) { xport_stop(); if (clk_send) midi_out_event(0xFu | 0xFCu << 8); }
            xport_req = 0;
            ticks = 0;
            int_acc += (float)GM_BLOCK;
            while (int_acc >= tick_len) {
                int_acc -= tick_len;
                ticks++;
                if (xport != 2) clk_ticks++;
                if (clk_send) midi_out_event(0xFu | 0xF8u << 8);
            }
        } else xport_req = 0;
        arp_block(xport == 2 ? 0 : ticks);
        seq_block(xport == 2 ? 0 : ticks);
    }
    block_no++;
    c1_fx_t = 0;
    if ((fx_on || GM_FX_EXTRA_ON()) && cpu1_ok && fm1_c1_on) started = (uint32_t)fm1_cpu1_run(c1_job, block_no);
    /* die Parts auf Kern 0, während Kern 1 die Effekte des Blocks davor rechnet */
    t = fm1_ticks();
    for (uint32_t i = 0; i < GM_BLOCK; i++) mix_l[i] = mix_r[i] = 0;
    float sd = 0, sr = 0;
    for (unsigned p = 0; p < GM_PARTS; p++) { sd += part_send[p][0] * part_level[p]; sr += part_send[p][1] * part_level[p]; }
    for (uint32_t i = 0; i < GM_BLOCK; i++) fx_in[0][i] = fx_in[1][i] = 0;
    for (unsigned p = 0; p < GM_PARTS; p++) {
        float lv = part_level[p], s0 = part_send[p][0] * lv, s1 = part_send[p][1] * lv;
        for (uint32_t i = 0; i < GM_BLOCK; i++) part_l[i] = part_r[i] = 0;
        kern_render(p, part_l, part_r, GM_BLOCK);
        for (uint32_t i = 0; i < GM_BLOCK; i++) {
            float m = (part_l[i] + part_r[i]) * 0.5f;
            mix_l[i] += part_l[i] * lv; mix_r[i] += part_r[i] * lv;
            fx_in[0][i] += m * s0; fx_in[1][i] += m * s1;
        }
    }
    (void)sd; (void)sr;
    tv = fm1_ticks() - t;
    if (started) {
        t = fm1_ticks();
        if (fm1_cpu1_wait() != 0) { cpu1_ok = 0; fm1_cpu1_hold(); fm1_cpu1_bar(); }
        wait = fm1_ticks() - t;
    } else {
        t = fm1_ticks();
        fx_run();
        c1_fx_t = fm1_ticks() - t;
    }
    for (uint32_t i = 0; i < GM_BLOCK; i++) {
        float l = (mix_l[i] + fx_wet[0][i]) * GM_OUT_GAIN, r = (mix_r[i] + fx_wet[1][i]) * GM_OUT_GAIN;
        l = l > 32767.0f ? 32767.0f : l < -32767.0f ? -32767.0f : l;
        r = r > 32767.0f ? 32767.0f : r < -32767.0f ? -32767.0f : r;
        out[2u * i] += (int32_t)l;
        out[2u * i + 1u] += (int32_t)r;
        scope_buf[(scope_w + i) & 511u] = (int8_t)((int32_t)l >> 8);
        if (l > 3.0f || l < -3.0f || r > 3.0f || r < -3.0f) loud = 1;
    }
    silent_blocks = loud ? 0u : silent_blocks + 1u;
    scope_w += GM_BLOCK;
    t = fm1_ticks() - t0;
    m_add(M_ALL, t);
    m_add(M_K0, t - wait);
    m_add(M_K1, started ? c1_fx_t : 0);
    m_add(M_VOICE, tv);
    m_add(M_FX, c1_fx_t);
    meas.blocks++;
    if (t > BLOCK_TICKS) meas.late++;
}

static char *put(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *put_n(char *p, uint32_t v, uint32_t width)
{
    char b[10];
    uint32_t n = 0;
    do { b[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 10u);
    for (uint32_t i = n; i < width; i++) *p++ = ' ';
    while (n) *p++ = b[--n];
    return p;
}
static const char *find_char(const char *s, char c) { for (; *s; s++) if (*s == c) return s; return 0; }
static void keys_poll(void);
static uint8_t idle_keys;
static void app_idle(void) { if (idle_keys) keys_poll(); }

/* ======================================================================================================
 * Sounds: Werkssounds der Variante und eigene (Blob 0, mit Formatversion: ältere Stände werden übernommen, fehlende
 * Parameter kommen aus dem Grundklang). Ein Part hält eine Arbeitskopie; `base` ist der Stand beim Laden.
 * ====================================================================================================== */
#ifndef GM_NOWN
#define GM_NOWN 40                              /* Plätze für eigene Sounds (weniger, wenn ein Sound viele Parameter hat) */
#endif
enum { NOWN = GM_NOWN, CAT_OWN = GM_NCAT, CAT_INIT = GM_NCAT + 1, NCAT_ALL = GM_NCAT + 2 };
#define FAC_N (GM_NFAC + 1u)                    /* die Werkssounds, dann INIT (zuletzt, damit die anderen ihre Nummer behalten) */
/* GM_FAC_FUNCS: die Variante liefert die Werkssounds über gm_fac_name/gm_fac_v/gm_fac_cat (i < GM_NFAC) statt der Tabelle
 * gm_factory, z. B. aus Presets im Flash (MKS Ultra). gm_fac_v darf in einen eigenen Puffer schreiben (sofort kopiert). */
#ifdef GM_FAC_FUNCS
static const char *fac_name(unsigned i) { return i < GM_NFAC ? gm_fac_name(i) : "INIT"; }
static const uint8_t *fac_v(unsigned i) { return i < GM_NFAC ? gm_fac_v(i) : gm_init_v; }
static unsigned fac_cat(unsigned i) { return i < GM_NFAC ? gm_fac_cat(i) : CAT_INIT; }
#else
static const char *fac_name(unsigned i) { return i < GM_NFAC ? gm_factory[i].name : "INIT"; }
static const uint8_t *fac_v(unsigned i) { return i < GM_NFAC ? gm_factory[i].v : gm_init_v; }
static unsigned fac_cat(unsigned i) { return i < GM_NFAC ? gm_factory[i].cat : CAT_INIT; }
#endif
static const char *cat_label(unsigned c) { return c < GM_NCAT ? gm_cat_name[c] : c == CAT_OWN ? "OWN" : "INIT"; }
#define FAC_AT(j) (((j) + FAC_N - 1u) % FAC_N)

typedef struct { char name[GM_NAMEMAX + 1]; uint8_t cat, used; uint8_t v[GM_NP]; } own_t;
typedef struct { uint32_t magic; uint16_t ver, np; } own_head_t;
#define OWN_MAGIC 0x4E574F47u                   /* "GOWN" */
/* Blob 0 hat immer dieselbe Länge (die Plattform lädt nur genau die gespeicherte Länge): so lädt eine spätere Fassung
 * mit mehr Parametern die älteren Sounds (Kopf: Formatversion, Anzahl Parameter je Sound) */
#define OWN_BYTES 4000u
static union { struct { own_head_t h; own_t s[NOWN]; } d; uint8_t raw[OWN_BYTES]; } ownb;
#define own (ownb.d.s)
_Static_assert(sizeof ownb == OWN_BYTES && OWN_BYTES <= SECTOR_MAX, "die eigenen Sounds passen in einen Blob");

typedef struct {
    uint8_t kind, idx, chan, level, cat_view, cat, dirty;
    char name[GM_NAMEMAX + 1];
    uint8_t v[GM_NP], base[GM_NP];
} upart_t;
static upart_t up[GM_PARTS];
static struct { uint8_t ok, kind, idx; char name[GM_NAMEMAX + 1]; uint8_t v[GM_NP]; } restore[GM_PARTS];

static unsigned cat_count(unsigned c)
{
    unsigned n = 0;
    if (c != CAT_OWN) for (unsigned i = 0; i < FAC_N; i++) n += fac_cat(i) == c;
    for (unsigned j = 0; j < NOWN; j++) n += own[j].used && (c == CAT_OWN || own[j].cat == c);
    return n;
}
static int cat_item(unsigned c, unsigned n, uint8_t *kind, uint8_t *idx)
{
    if (c != CAT_OWN) for (unsigned j = 0; j < FAC_N; j++)
        if (fac_cat(FAC_AT(j)) == c && n-- == 0) { *kind = 0; *idx = (uint8_t)FAC_AT(j); return 0; }
    for (unsigned j = 0; j < NOWN; j++)
        if (own[j].used && (c == CAT_OWN || own[j].cat == c) && n-- == 0) { *kind = 1; *idx = (uint8_t)j; return 0; }
    return -1;
}
static int cat_pos(unsigned c, unsigned kind, unsigned idx)
{
    unsigned n = cat_count(c);
    for (unsigned i = 0; i < n; i++) {
        uint8_t k, x;
        if (!cat_item(c, i, &k, &x) && k == kind && x == idx) return (int)i;
    }
    return -1;
}
static unsigned cat_order(unsigned r) { return r == 0 ? CAT_INIT : r <= GM_NCAT ? r - 1u : CAT_OWN; }   /* INIT, Gruppen, OWN */
static unsigned next_cat(unsigned c, int dir)
{
    unsigned r = 0;
    for (unsigned i = 0; i < NCAT_ALL; i++) if (cat_order(i) == c) r = i;
    for (unsigned n = 0; n < NCAT_ALL; n++) {
        r = (r + NCAT_ALL + (unsigned)dir) % NCAT_ALL;
        if (cat_count(cat_order(r))) return cat_order(r);
    }
    return c;
}
static void push_part(unsigned p)
{
    uint32_t n;
    for (n = 0; pend_flag[p] && n < 200000u; n++) {}
    for (uint32_t i = 0; i < GM_NP; i++) pend_v[p][i] = up[p].v[i];
    FM1_C1_SYNC();
    pend_flag[p] = 1;
}
static void mark_dirty(unsigned p) { up[p].dirty = memcmp(up[p].v, up[p].base, GM_NP) != 0; }
static void load_sound(unsigned p, unsigned kind, unsigned idx)
{
    upart_t *u = &up[p];
    const uint8_t *src;
    const char *name;
    if (u->dirty) {
        restore[p].ok = 1; restore[p].kind = u->kind; restore[p].idx = u->idx;
        memcpy(restore[p].v, u->v, GM_NP); memcpy(restore[p].name, u->name, sizeof u->name);
    }
    if (kind == 1u && (idx >= NOWN || !own[idx].used)) kind = 0, idx = 0;
    if (kind == 0u && idx >= FAC_N) idx = 0;
    if (kind) { src = own[idx].v; name = own[idx].name; u->cat = own[idx].cat; }
    else { src = fac_v(idx); name = fac_name(idx); u->cat = (uint8_t)fac_cat(idx); }
    u->kind = (uint8_t)kind; u->idx = (uint8_t)idx;
    memcpy(u->base, src, GM_NP); memcpy(u->v, src, GM_NP);
    str_cpy(u->name, name, sizeof u->name);
    if (!kind || u->cat_view != CAT_OWN) u->cat_view = u->cat;
    u->dirty = 0;
    push_part(p);
}
static void step_sound(unsigned p, int32_t e)   /* links: der Sound davor, rechts: der nächste; am Ende die nächste Gruppe */
{
    unsigned c = up[p].cat_view, k = up[p].kind, i = up[p].idx;
    int dir = e > 0 ? 1 : -1;
    for (int32_t n = e < 0 ? -e : e; n > 0; n--) {
        int pos = cat_pos(c, k, i), cnt = (int)cat_count(c);
        uint8_t kk, ii;
        if (pos < 0) pos = dir > 0 ? -1 : cnt;
        pos += dir;
        if (pos >= cnt) { c = next_cat(c, 1); pos = 0; }
        else if (pos < 0) { c = next_cat(c, -1); pos = (int)cat_count(c) - 1; }
        if (cat_item(c, (unsigned)pos, &kk, &ii)) break;
        k = kk; i = ii;
    }
    up[p].cat_view = (uint8_t)c;
    if (k != up[p].kind || i != up[p].idx) load_sound(p, k, i);
}
/* eigene Sounds aus Blob 0 laden; ein älteres Format (weniger Parameter) wird ergänzt */
static void own_load(void)
{
    static uint8_t raw[OWN_BYTES];
    own_head_t h;
    memset(&ownb, 0, sizeof ownb);
    if (!flash_ok || blob_load(0, raw, OWN_BYTES)) return;
    memcpy(&h, raw, sizeof h);
    if (h.magic != OWN_MAGIC || !h.ver || !h.np || h.np > 255u) return;   /* (h.ver: GM_SOUND_VER der Fassung, die schrieb) */
    {
        uint32_t old = GM_NAMEMAX + 3u + h.np;  /* ein Sound im gespeicherten Format */
        for (unsigned j = 0; j < NOWN && sizeof h + (j + 1u) * old <= OWN_BYTES; j++) {
            const uint8_t *s = raw + sizeof h + j * old;
            memcpy(own[j].name, s, GM_NAMEMAX + 1u);
            own[j].name[GM_NAMEMAX] = 0;
            own[j].cat = s[GM_NAMEMAX + 1u]; own[j].used = s[GM_NAMEMAX + 2u];
            for (unsigned i = 0; i < GM_NP; i++) {
                uint8_t x = i < h.np ? s[GM_NAMEMAX + 3u + i] : gm_init_v[i];
                own[j].v[i] = x <= gm_params[i].max ? x : gm_init_v[i];
            }
            if (own[j].used && (own[j].cat >= GM_NCAT || !own[j].name[0])) own[j].used = 0;
        }
    }
}
static void own_flash(void)
{
    ownb.d.h.magic = OWN_MAGIC; ownb.d.h.ver = GM_SOUND_VER; ownb.d.h.np = GM_NP;
    _Static_assert(sizeof(own_t) == GM_NAMEMAX + 3u + GM_NP, "own_t ohne Füllbytes (Formatumwandlung in own_load)");
    blob_save(0, &ownb, OWN_BYTES);
}

/* ======================================================================================================
 * Einstellungen (alles außer den Sounds): im Journal, nach jeder Änderung
 * ====================================================================================================== */
static struct {
    uint8_t ver, sel; int8_t oct; uint8_t tempo;
    struct { uint8_t kind, idx, chan, level, send[2]; uint8_t arp[ARP_N]; } p[GM_PARTS];
    uint8_t fx[FX_COUNT];
    uint8_t clk_src, clk_send;
} set;
enum { SET_VER = 1 };
static uint32_t t_set;
static uint8_t fxv[FX_COUNT], arpc[GM_PARTS][ARP_N], sendv[GM_PARTS][2], tempo = 80, clkv[2];
static int quiet(void)
{
    for (unsigned p = 0; p < GM_PARTS; p++) if (kern_voices(p)) return 0;
    return 1;
}
static void set_pack(void)
{
    set.ver = SET_VER;
    set.tempo = tempo;
    for (unsigned p = 0; p < GM_PARTS; p++) {
        set.p[p].kind = up[p].kind; set.p[p].idx = up[p].idx; set.p[p].chan = up[p].chan; set.p[p].level = up[p].level;
        memcpy(set.p[p].arp, arpc[p], ARP_N); memcpy(set.p[p].send, sendv[p], 2);
    }
    memcpy(set.fx, fxv, FX_COUNT);
    set.clk_src = clkv[0]; set.clk_send = clkv[1];
}

#include "gm_ui.c"
