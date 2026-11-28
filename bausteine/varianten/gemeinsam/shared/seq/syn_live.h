/* SPDX-License-Identifier: GPL-3.0-only
 * Live-Aufnahme nach Art des Retrokits RK-008 (FM1-Electro-one, docs/adr/0053, FEATURES S20), ein Modul für alle
 * Firmwares aus bausteine/varianten: das Gerüst gemeinsam/ (SH-101, MKS Ultra, JP-6 solo) und die fm1-synths-Apps von
 * JX-8P und MKS-50. Nur eine Kopfdatei: die App bindet sie nach ihren Noten-Funktionen ein.
 *
 * Verhalten (Wunsch des Nutzers 28.11.2026):
 *   REC tippen: Aufnahme sofort. REC zweimal schnell: ein Takt Einzählen, dann Aufnahme.
 *   Leere Schleife: die erste Aufnahme legt die Länge fest (REC beendet sie), gerundet auf ganze Takte. Danach läuft die
 *   Schleife; nimmt man wieder auf, kommt jeder Durchgang dazu (Overdub). REC übernimmt, STOP (PLAY tippen) verwirft
 *   den laufenden Durchgang.
 *   Quantisiert fest auf 1/16, die Note so lang wie gehalten.
 *   Länge nachträglich in ganzen Takten: verlängern (wiederholen oder leer), kürzen (Anfang oder Ende behalten);
 *   0 Takte = leer.
 *
 * Die App legt vor dem Einbinden fest:
 *   LV_PARTS                      Parts (1 im Gerüst, 4 in den fm1-synths-Apps)
 *   LV_NOTE_ON(p, note, vel127)   Note an den Klang des Parts (im Audio-Interrupt)
 *   LV_NOTE_OFF(p, note)
 *   LV_DSP                        Ablage der Audio-Funktionen (z. B. .dsp_text)
 * und ruft auf:
 *   Audio-Interrupt: lv_block(ticks) je Block mit den Takt-Ticks (24 je Viertel, 0 bei STOP) nach Arp und Sequencer;
 *                    lv_hear(p, note, vel, on) für jede gespielte Note eines Parts (Tasten, MIDI); lv_stop_all()
 *                    beim Transport-STOP, lv_rewind() beim START.
 *   Hauptprogramm:   lv_ui_rec(p, ms), lv_ui_stop(p), lv_ui_set(p, was, wert), lv_status(...), lv_load/lv_image.
 * Gespeichert wird lv_store (die App legt es in einen eigenen Blob; Kennung und Version stehen darin). */
#ifndef SYN_LIVE_H
#define SYN_LIVE_H
#include <stdint.h>
#include "syn_live_set.h"   /* LV_SET_*, LV_GROW_*, LV_CUT_* */
/* (memset: die App bringt es mit, Plattform oder <string.h>) */

#ifndef LV_DSP
#define LV_DSP
#endif
#ifndef LV_EV_MAX
#define LV_EV_MAX 512                   /* Ereignisse je Part höchstens (die App kann weniger nehmen, um Speicher zu sparen) */
#endif
#ifndef LV_RT_ATTR
#define LV_RT_ATTR                      /* Ablage der Laufzeitdaten (z. B. section(".pool"); lv_load_done nullt sie) */
#endif
enum {
    LV_BARS = 16,                       /* höchstens (Ziel des Nutzers 8–16) */
    LV_TPB = 96,                        /* Ticks je Takt (4/4, 24 je Viertel) */
    LV_T16 = 6,                         /* Ticks je 1/16 */
    LV_EV = (4000 - 16 - 8 * LV_PARTS) / (4 * LV_PARTS) > LV_EV_MAX ? LV_EV_MAX : (4000 - 16 - 8 * LV_PARTS) / (4 * LV_PARTS),
    LV_HOLD = 16,                       /* gleichzeitig klingende Noten der Schleife je Part */
    LV_MAGIC = 0x3156494Cu,             /* "LIV1" */
    LV_VER = 1,
    LV_DOUBLE_MS = 400,                 /* zweimal REC innerhalb dieser Zeit: Einzählen */
};
enum { LV_OFF, LV_COUNT, LV_FIRST, LV_OVER };      /* Zustand der Aufnahme */

/* Ein Ereignis: Beginn und Länge in 1/16, Note, Velocity (Bit 7: im laufenden Durchgang aufgenommen). */
typedef struct { uint8_t at, len, note, vel; } lv_ev_t;
typedef struct {
    uint32_t magic;
    uint8_t ver, parts, ev_max, pad;
    struct { uint8_t bars, on, grow, cut; uint16_t n, pad; } p[LV_PARTS];
    lv_ev_t ev[LV_PARTS][LV_EV];
} lv_store_t;
_Static_assert(sizeof(lv_store_t) <= 4096u - 64u, "Live-Aufnahme passt in einen Blob");

/* LV_STORE_ATTR: wo der Speicher liegt (z. B. section(".pool"), wenn das RAM knapp ist; der Pool wird beim Start nicht
 * genullt, lv_load_done legt ihn immer ganz an) */
#ifndef LV_STORE_ATTR
#define LV_STORE_ATTR
#endif
static lv_store_t lv_store LV_STORE_ATTR;
/* Laufzeit (Audio-Interrupt) */
static struct {
    uint32_t pos[LV_PARTS];             /* Ticks in der Schleife */
    uint8_t hold_note[LV_PARTS][LV_HOLD];
    uint16_t hold_left[LV_PARTS][LV_HOLD];
    uint8_t hold_n[LV_PARTS];
    uint32_t elapsed;                   /* erster Durchgang: Ticks seit dem Start */
    uint32_t count;                     /* Einzählen: Ticks übrig */
    uint16_t key_t[LV_HOLD];            /* gehaltene Tasten beim Aufnehmen: Tick des Anschlags, Note, Velocity */
    uint8_t key_n[LV_HOLD], key_v[LV_HOLD], keys;
    uint32_t shown_pos[LV_PARTS];       /* für die Anzeige */
    uint32_t rec_ms;                    /* Hauptprogramm: wann REC die Aufnahme begann (Doppeltippen) */
    uint32_t saved;                     /* Hauptprogramm: lv_sum des zuletzt gespeicherten Stands */
    struct { uint8_t cmd, part, a, b; } q[16];   /* Befehle vom Hauptprogramm (ein Schreiber, ein Leser) */
    volatile uint32_t qw, qr;
    volatile uint8_t state, part;       /* LV_OFF.. und welcher Part aufnimmt */
    volatile uint8_t ready;             /* erst nach lv_load_done; liegt lv im Pool, setzt die App es vor dem Audio auf 0 (lv_boot) */
} lv LV_RT_ATTR;
#define lv_state (lv.state)
#define lv_part (lv.part)
#define lv_ready (lv.ready)
/* vor dem Start des Audio-Interrupts (nötig, wenn lv im Pool liegt: der wird nicht genullt) */
static void lv_boot(void) { lv.ready = 0; }
/* Befehle vom Hauptprogramm (ein Schreiber, ein Leser) */
enum { LVC_REC, LVC_COUNT, LVC_COMMIT, LVC_DISCARD, LVC_SET };
static void lv_put(unsigned cmd, unsigned part, unsigned a, unsigned b)
{
    uint32_t w = lv.qw;
    if (!lv_ready) return;
    if (w - lv.qr >= 16u) return;
    lv.q[w & 15u].cmd = (uint8_t)cmd; lv.q[w & 15u].part = (uint8_t)part; lv.q[w & 15u].a = (uint8_t)a; lv.q[w & 15u].b = (uint8_t)b;
    __sync_synchronize();
    lv.qw = w + 1u;
}

static void lv_reset(void)
{
    memset(&lv_store, 0, sizeof lv_store);
    lv_store.magic = LV_MAGIC; lv_store.ver = LV_VER; lv_store.parts = LV_PARTS; lv_store.ev_max = (uint8_t)(LV_EV > 255 ? 255 : LV_EV);
    for (unsigned p = 0; p < LV_PARTS; p++) lv_store.p[p].on = 1;
}
/* Nach dem Laden aus dem Blob (rc != 0: nichts da): ungültige Stände werden leer. */
static void lv_load_done(int rc)
{
    int ok = !rc && lv_store.magic == LV_MAGIC && lv_store.ver == LV_VER && lv_store.parts == LV_PARTS;
    if (ok)
        for (unsigned p = 0; p < LV_PARTS; p++)
            if (lv_store.p[p].bars > LV_BARS || lv_store.p[p].n > LV_EV) ok = 0;
    if (!ok) lv_reset();
    memset(&lv, 0, sizeof lv);          /* (die Laufzeit kann im Pool liegen: der wird beim Start nicht genullt) */
    __sync_synchronize();
    lv_ready = 1;
}
static uint32_t lv_sum(void)
{
    const uint8_t *b = (const uint8_t *)&lv_store;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < sizeof lv_store; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

/* ---- Audio-Interrupt ------------------------------------------------------------------------------------------- */

static LV_DSP void lv_hold_off(unsigned p)
{
    for (unsigned i = 0; i < lv.hold_n[p]; i++) LV_NOTE_OFF(p, lv.hold_note[p][i]);
    lv.hold_n[p] = 0;
}
static LV_DSP void lv_stop_all(void) { if (lv_ready) for (unsigned p = 0; p < LV_PARTS; p++) lv_hold_off(p); }
static LV_DSP void lv_rewind(void) { for (unsigned p = 0; p < LV_PARTS; p++) lv.pos[p] = 0; }

static LV_DSP void lv_add(unsigned p, uint32_t at16, uint32_t len16, unsigned note, unsigned vel)
{
    uint16_t n = lv_store.p[p].n;
    if (n >= LV_EV) return;
    if (len16 < 1u) len16 = 1u;
    if (len16 > 255u) len16 = 255u;
    lv_store.ev[p][n] = (lv_ev_t){(uint8_t)at16, (uint8_t)len16, (uint8_t)note, (uint8_t)(0x80u | (vel & 0x7Fu))};
    lv_store.p[p].n = (uint16_t)(n + 1u);
}
/* die Stelle in der Schleife (Ticks), an der aufgenommen wird */
static LV_DSP uint32_t lv_now(unsigned p) { return lv_state == LV_FIRST ? lv.elapsed : lv.pos[p]; }
static LV_DSP void lv_close(unsigned p, unsigned k)       /* gehaltene Taste k (Liste) loslassen: ein Ereignis */
{
    uint32_t t0 = lv.key_t[k], t1 = lv_now(p), len = lv_store.p[p].bars * (uint32_t)LV_TPB;
    uint32_t dur = lv_state == LV_FIRST || t1 >= t0 ? t1 - t0 : t1 + len - t0;
    uint32_t at16 = (t0 + LV_T16 / 2u) / LV_T16, len16 = (dur + LV_T16 / 2u) / LV_T16;
    if (lv_state != LV_FIRST && len) at16 %= len / LV_T16;
    lv_add(p, at16, len16, lv.key_n[k], lv.key_v[k]);
    lv.keys--;
    lv.key_t[k] = lv.key_t[lv.keys]; lv.key_n[k] = lv.key_n[lv.keys]; lv.key_v[k] = lv.key_v[lv.keys];
}
static LV_DSP void lv_hear(unsigned p, unsigned note, unsigned vel, int on)
{
    unsigned k;
    if (!lv_ready) return;
    if (note > 127u || p != lv_part || (lv_state != LV_FIRST && lv_state != LV_OVER)) return;
    for (k = 0; k < lv.keys && lv.key_n[k] != note; k++) {}
    if (k < lv.keys) lv_close(p, k);    /* (dieselbe Taste noch einmal: die vorige endet) */
    if (!on || lv.keys >= LV_HOLD) return;
    lv.key_t[lv.keys] = (uint16_t)lv_now(p); lv.key_n[lv.keys] = (uint8_t)note; lv.key_v[lv.keys] = (uint8_t)(vel ? vel : 100);
    lv.keys++;
}
static LV_DSP void lv_close_held(unsigned p) { while (lv.keys) lv_close(p, lv.keys - 1u); }
static LV_DSP void lv_commit_flags(unsigned p)
{
    for (unsigned i = 0; i < lv_store.p[p].n; i++) lv_store.ev[p][i].vel &= 0x7Fu;
}
static LV_DSP void lv_drop_new(unsigned p)
{
    unsigned k = 0;
    for (unsigned i = 0; i < lv_store.p[p].n; i++) if (!(lv_store.ev[p][i].vel & 0x80u)) lv_store.ev[p][k++] = lv_store.ev[p][i];
    lv_store.p[p].n = (uint16_t)k;
}
/* Länge ändern (ganze Takte): länger = wiederholen oder leer, kürzer = Anfang oder Ende behalten; 0 = leer */
static LV_DSP void lv_set_bars(unsigned p, unsigned bars)
{
    unsigned old = lv_store.p[p].bars, n = lv_store.p[p].n, k;
    uint32_t o16 = old * 16u, n16;
    if (bars > LV_BARS) bars = LV_BARS;
    lv_hold_off(p);
    if (!bars) { lv_store.p[p].bars = 0; lv_store.p[p].n = 0; lv.pos[p] = 0; return; }
    n16 = bars * 16u;
    if (old && bars > old && lv_store.p[p].grow == LV_GROW_REPEAT) {
        for (uint32_t rep = o16; rep < n16; rep += o16)
            for (unsigned i = 0; i < n && lv_store.p[p].n < LV_EV; i++) {
                lv_ev_t e = lv_store.ev[p][i];
                if (e.at + rep < n16) { e.at = (uint8_t)(e.at + rep); lv_store.ev[p][lv_store.p[p].n++] = e; }
            }
    } else if (old && bars < old) {
        uint32_t from = lv_store.p[p].cut == LV_CUT_END ? o16 - n16 : 0u;
        for (unsigned i = k = 0; i < n; i++) {
            lv_ev_t e = lv_store.ev[p][i];
            if (e.at < from || e.at >= from + n16) continue;
            e.at = (uint8_t)(e.at - from);
            lv_store.ev[p][k++] = e;
        }
        lv_store.p[p].n = (uint16_t)k;
    }
    lv_store.p[p].bars = (uint8_t)bars;
    lv.pos[p] %= n16 * LV_T16;
}
static LV_DSP void lv_start_rec(unsigned p)
{
    lv_part = (uint8_t)p;
    lv.keys = 0;
    if (!lv_store.p[p].bars) { lv_store.p[p].n = 0; lv.elapsed = 0; lv_state = LV_FIRST; }
    else { lv_store.p[p].on = 1; lv_state = LV_OVER; }
}
static LV_DSP void lv_commit(void)
{
    unsigned p = lv_part;
    if (lv_state == LV_FIRST) {
        unsigned bars = (lv.elapsed + LV_TPB / 2u) / LV_TPB;
        uint32_t n16;
        lv_close_held(p);
        if (bars < 1u) bars = 1u;
        if (bars > LV_BARS) bars = LV_BARS;
        n16 = bars * 16u;
        for (unsigned i = 0; i < lv_store.p[p].n; i++) lv_store.ev[p][i].at = (uint8_t)(lv_store.ev[p][i].at % n16);
        lv_store.p[p].bars = (uint8_t)bars; lv_store.p[p].on = 1;
        lv.pos[p] = 0;                  /* die Schleife beginnt jetzt */
    } else if (lv_state == LV_OVER) lv_close_held(p);
    lv_commit_flags(p);
    lv_state = LV_OFF;
}
static LV_DSP void lv_discard(void)
{
    unsigned p = lv_part;
    if (lv_state == LV_FIRST) lv_store.p[p].n = 0;
    else lv_drop_new(p);
    lv.keys = 0;
    lv_state = LV_OFF;
}
static LV_DSP void lv_tick(unsigned p)
{
    uint32_t len = lv_store.p[p].bars * (uint32_t)LV_TPB;
    for (unsigned i = 0; i < lv.hold_n[p];) {   /* die klingenden Noten der Schleife: aus nach ihrer Länge */
        if (--lv.hold_left[p][i]) { i++; continue; }
        LV_NOTE_OFF(p, lv.hold_note[p][i]);
        lv.hold_note[p][i] = lv.hold_note[p][lv.hold_n[p] - 1u]; lv.hold_left[p][i] = lv.hold_left[p][lv.hold_n[p] - 1u];
        lv.hold_n[p]--;
    }
    if (!len) return;
    if (lv.pos[p] % LV_T16 == 0u && lv_store.p[p].on) {
        uint32_t s16 = lv.pos[p] / LV_T16;
        for (unsigned i = 0; i < lv_store.p[p].n; i++) {
            lv_ev_t *e = &lv_store.ev[p][i];
            if (e->at != s16 || (e->vel & 0x80u)) continue;   /* (eben aufgenommen: erst im nächsten Durchgang) */
            if (lv.hold_n[p] >= LV_HOLD) break;
            LV_NOTE_ON(p, e->note, e->vel);
            lv.hold_note[p][lv.hold_n[p]] = e->note; lv.hold_left[p][lv.hold_n[p]] = (uint16_t)(e->len * LV_T16 - 1u); lv.hold_n[p]++;
        }
    }
    if (++lv.pos[p] >= len) {
        lv.pos[p] = 0;
        if (lv_state == LV_OVER && lv_part == p) lv_commit_flags(p);   /* ein Durchgang fertig: er bleibt */
    }
}
static LV_DSP void lv_block(unsigned ticks)
{
    if (!lv_ready) return;
    while (lv.qr != lv.qw) {
        struct { uint8_t cmd, part, a, b; } c;
        c.cmd = lv.q[lv.qr & 15u].cmd; c.part = lv.q[lv.qr & 15u].part; c.a = lv.q[lv.qr & 15u].a; c.b = lv.q[lv.qr & 15u].b;
        lv.qr++;
        if (c.part >= LV_PARTS) continue;
        switch (c.cmd) {
        case LVC_REC: if (lv_state != LV_OFF) lv_discard(); lv_start_rec(c.part); break;
        case LVC_COUNT:
            if (lv_state != LV_OFF) lv_discard();
            lv_part = c.part; lv.count = LV_TPB; lv_state = LV_COUNT;
            break;
        case LVC_COMMIT: if (lv_state == LV_COUNT) lv_state = LV_OFF; else lv_commit(); break;
        case LVC_DISCARD: if (lv_state == LV_COUNT) lv_state = LV_OFF; else lv_discard(); break;
        case LVC_SET:
            if (c.a == LV_SET_ON) { lv_store.p[c.part].on = (uint8_t)(c.b != 0); if (!c.b) lv_hold_off(c.part); }
            else if (c.a == LV_SET_BARS) { if (lv_state != LV_OFF && lv_part == c.part) lv_discard(); lv_set_bars(c.part, c.b); }
            else if (c.a == LV_SET_GROW) lv_store.p[c.part].grow = (uint8_t)(c.b != 0);
            else if (c.a == LV_SET_CUT) lv_store.p[c.part].cut = (uint8_t)(c.b != 0);
            break;
        }
    }
    for (unsigned t = 0; t < ticks; t++) {
        if (lv_state == LV_COUNT && !--lv.count) lv_start_rec(lv_part);
        else if (lv_state == LV_FIRST && ++lv.elapsed >= (uint32_t)LV_BARS * LV_TPB) lv_commit();
        for (unsigned p = 0; p < LV_PARTS; p++) lv_tick(p);
    }
    for (unsigned p = 0; p < LV_PARTS; p++) lv.shown_pos[p] = lv_state == LV_FIRST && lv_part == p ? lv.elapsed : lv.pos[p];
}

/* ---- Hauptprogramm --------------------------------------------------------------------------------------------- */

/* REC getippt. Rückgabe: 1 Aufnahme beginnt, 2 Einzählen, 0 übernommen */
static int lv_ui_rec(unsigned p, uint32_t ms)
{
    if (lv_state != LV_OFF && lv_part == p) {
        if (ms - lv.rec_ms < LV_DOUBLE_MS && lv_state != LV_COUNT) { lv_put(LVC_COUNT, p, 0, 0); return 2; }
        lv_put(LVC_COMMIT, p, 0, 0);
        return 0;
    }
    lv.rec_ms = ms;
    lv_put(LVC_REC, p, 0, 0);
    return 1;
}
static void lv_ui_stop(unsigned p) { lv_put(LVC_DISCARD, p, 0, 0); }
static void lv_ui_set(unsigned p, unsigned what, unsigned v) { lv_put(LVC_SET, p, what, v); }
static int lv_busy(void) { return lv_state != LV_OFF; }
/* Anzeige: Takt (1..) und Länge in Takten; Zustand */
static void lv_status(unsigned p, unsigned *bar, unsigned *bars, unsigned *state)
{
    uint32_t pos = lv.shown_pos[p];
    *bars = lv_store.p[p].bars;
    *state = lv_part == p ? lv_state : LV_OFF;
    *bar = (unsigned)(pos / LV_TPB) + 1u;
    if (*state == LV_COUNT) *bar = 0;
}
#endif
