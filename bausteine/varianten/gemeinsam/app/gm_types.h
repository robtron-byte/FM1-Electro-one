/* SPDX-License-Identifier: GPL-3.0-only
 * Typen, mit denen eine Variante ihre Parameter, Sounds und Seiten beschreibt (ADR 0044). Die Variante bindet diese
 * Datei vor ihren Tabellen ein und gm_app.c danach. */
#ifndef GM_TYPES_H
#define GM_TYPES_H
#include <stdint.h>
#include "syn_fx.h"                             /* FX_* (Seiten der Effekte) */
#include "seq/syn_live_set.h"                    /* LV_SET_* (Seite LOOP, Live-Aufnahme ADR 0053) */

/* Frames je Audioblock: 1,45 ms (fm1-synths ADR 0004); eine Quelle: SYN_BLOCK aus tools/build.py (GM_BLOCK=…) */
#ifdef SYN_BLOCK
#define GM_BLOCK ((uint32_t)SYN_BLOCK)
#else
#define GM_BLOCK 64u
#endif

typedef struct {
    const char *label;                          /* höchstens 7 Zeichen, sonst kleiner */
    uint8_t max;                                /* Werte 0..max; max <= 4: Schalter (kein Balken) */
    uint8_t cls;                                /* Farbklasse C_SRC, C_MOD, C_FLT, C_AMP, C_FX (gm_theme.h) */
    void (*text)(uint8_t v, char *b);           /* Anzeige (NULL: die Zahl) */
} gm_param_t;


enum { C_SRC, C_MOD, C_FLT, C_AMP, C_FX };      /* Farbklasse eines Parameters (gm_theme.h) */
/* Arpeggiator und Sequencer des Gerüsts (Seiten der Variante) */
enum { ARP_ON, ARP_MODE, ARP_DIV, ARP_OCT, ARP_GATE, ARP_HOLD, ARP_N };
enum { SQ_ON, SQ_DIV, SQ_GATE, SQ_SHIFT, SQ_N };
/* Seitengruppen = Tasten */
enum { G_HOME, G_EDIT, G_ENV, G_LFO, G_FX, G_ARP, G_GLO, G_SEL, G_SEQ, NGROUP };
/* Bilder der Seiten; ab ST_USER zeichnet die Variante (kern_stage) */
enum { ST_NAME, ST_HOME, ST_SYS, ST_BROWSE, ST_ARP, ST_SEQ, ST_TEMPO, ST_FX, ST_PARTS, ST_LIVE, ST_USER };   /* ST_LIVE: Live-Aufnahme (ADR 0053) */
/* was ein Reglerfeld einstellt */
enum { K_NONE, K_PARAM, K_LEVEL, K_CHAN, K_FX, K_ARP, K_TEMPO, K_CLK, K_SEQ, K_SEND, K_LIVE,
       K_ENGINE, K_CHANP, K_EXTRA };   /* K_ENGINE/K_CHANP: Klangerzeuger/MIDI-Kanal des Parts param; K_EXTRA: GM_SET_EXTRA */   /* K_LIVE: LV_SET_* (syn_live.h) */
typedef struct { const char *label; uint8_t kind, param; } kn_t;
typedef struct { uint8_t group, stage; const char *title; kn_t k[4]; } page_t;
typedef struct { const char *title; kn_t k[4]; } layer_t;
#define PRM(l, p) {l, K_PARAM, p}
#define NO {"", K_NONE, 0}
#define FXK(l, p) {l, K_FX, p}
#define ARK(l, p) {l, K_ARP, p}
#define SQK(l, p) {l, K_SEQ, p}
#define LVK(l, p) {l, K_LIVE, p}
#endif
