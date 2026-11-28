/* SPDX-License-Identifier: GPL-3.0-only
 * dk808: TR-808 aus dem DrumKern (Baustein, docs/adr/0040).
 *
 * Vorlage: DrumKern des Nutzers (Swift), Dropbox `Programmieren/Musik/808 Projekt/app/Sources/DrumKernDSP/`,
 * Voices.swift / DSPMath.swift (Stand 26.11.2026), Ablauf aus Engine.swift (27.11.2026). Formeln, Konstanten und
 * Reihenfolge der Rechnung unverändert, in dk_real wie dort. Ohne Kanalzug, Busse, Effekte und Master.
 *
 * Instrumente lassen sich beim Bauen weglassen: -DDK808_INSTRUMENTS=<Maske>, Bit i = Instrument i (DK_BD …).
 */
#ifndef DK808_H
#define DK808_H
#include "baustein.h"

/* Instrumente (Reihenfolge wie Instrument in Model.swift). Makros, damit #if sie kennt. */
#define DK_BD 0   /* Bass Drum */
#define DK_LG 1   /* Laser Gun */
#define DK_SD 2   /* Snare Drum */
#define DK_RS 3   /* Rim Shot */
#define DK_CP 4   /* Hand Clap */
#define DK_LC 5   /* Low Conga */
#define DK_MC 6   /* Mid Conga */
#define DK_HC 7   /* Hi Conga */
#define DK_LT 8   /* Low Tom */
#define DK_MT 9   /* Mid Tom */
#define DK_HT 10  /* Hi Tom */
#define DK_CH 11  /* Closed Hi Hat */
#define DK_OH 12  /* Open Hi Hat */
#define DK_CY 13  /* Cymbal */
#define DK_CL 14  /* Claves */
#define DK_MA 15  /* Maracas */
#define DK_CB 16  /* Cow Bell */
#define DK_COUNT 17

#define DK808_MASK_ALL 0x1FFFFu
/* Auswahl für die Firmware des Nutzers (28.11.): ohne Laser, Congas, Toms, Claves, Maracas */
#define DK808_MASK_FM1_ELECTRO ((1u << DK_BD) | (1u << DK_SD) | (1u << DK_RS) | (1u << DK_CP) | (1u << DK_CH) | \
                                (1u << DK_OH) | (1u << DK_CY) | (1u << DK_CB))
/* Rechengenauigkeit (docs/adr/0050): 0 = double wie die Swift-Vorlage (Referenz, Standard), 1 = float für das Gerät
 * (Abweichung gegen double: make -C bausteine dk808, Ausgabe "float"). Die Schnittstelle bleibt gleich. */
#ifndef DK808_FLOAT
#define DK808_FLOAT 0
#endif
#if DK808_FLOAT
typedef float dk_real;
#else
typedef double dk_real;
#endif

#ifndef DK808_INSTRUMENTS
#define DK808_INSTRUMENTS DK808_MASK_ALL
#endif
#define DK808_HAS(i) ((DK808_INSTRUMENTS >> (i)) & 1u)
#define DK808_HAS_CONGA (DK808_HAS(DK_LC) || DK808_HAS(DK_MC) || DK808_HAS(DK_HC))
#define DK808_HAS_TOM (DK808_HAS(DK_LT) || DK808_HAS(DK_MT) || DK808_HAS(DK_HT))
#define DK808_HAS_HAT (DK808_HAS(DK_CH) || DK808_HAS(DK_OH))
/* Wer den gemeinsamen Rausch- bzw. Metallgenerator braucht */
#define DK808_NEEDS_NOISE (DK808_HAS(DK_SD) || DK808_HAS(DK_CP) || DK808_HAS(DK_MA) || DK808_HAS_TOM)
#define DK808_NEEDS_METAL (DK808_HAS_HAT || DK808_HAS(DK_CY))

/* Parameter (baustein.h): je Instrument drei Regler 0..1 (Nummer = Instrument * 3 + Regler, Namen und Standard wie
 * im DrumKern), danach drei Choke-Schalter. */
#define DK808_KNOBS 3
#define DK808_P_HAT_CHOKE (DK_COUNT * DK808_KNOBS)
#define DK808_P_CONGA_CHOKE (DK808_P_HAT_CHOKE + 1)
#define DK808_P_TOM_CHOKE (DK808_P_HAT_CHOKE + 2)
#define DK808_PARAMS (DK808_P_HAT_CHOKE + 3)

typedef struct { dk_real p, y; } dk_lp_t;
typedef struct { dk_real p, b, x1, y1; } dk_hp_t;
typedef struct { dk_real b0, b1, b2, a1, a2, x1, x2, y1, y2; } dk_bq_t;
typedef struct { dk_real lo, bp; } dk_svf_t;
typedef struct { dk_real c, x1, y1; } dk_ap_t;
typedef struct { dk_ap_t upA, upB, dnA, dnB; dk_real dnHold; } dk_hb_t;
typedef struct { int quiet, limit; } dk_gate_t;

typedef struct {
    uint32_t w258, w25c;
    dk_lp_t lp2000, lp1;
    dk_bq_t bq;
    dk_hp_t hp1;
    dk_real out10, out138, raw;
} dk_noise_t;

typedef struct {
    dk_real phase[6], inc[6], out;
#if DK808_FLOAT
    uint32_t ph[6], ph_inc[6];   /* float: Phasen ganzzahlig (2^32 = ein Zyklus), sonst läuft float über Sekunden weg */
#endif
} dk_metal_t;

typedef struct {
    dk_hp_t outHP;
    dk_real tonePole, toneState, damping, sweep, A, c1, c2;
    int f1, f2, envDone;
    dk_real r, rRate, E, a, h, e1, e2, e3, excP, excB, hpPrev, negEPrev, low, band;
} dk_bd_t;

typedef struct { dk_real depth, adec, pdec, A, penv, aenv, phase, timer, tail, tailPhase; } dk_lg_t;

typedef struct {
    dk_real f1, f2, f3;
    dk_svf_t svf1, svf2, svf3;
    dk_hp_t hpA, hpB;
    dk_real toneMix, bodyLevel, snapDecay, snapLevel, toneMixS, bodyLevelS, A2;
    int t1, t2;
    dk_real e, eRate, g, nenv, nTarget, nRate;
} dk_sd_t;

typedef struct {
    dk_hp_t hp10k;
    dk_bq_t hp570;
    dk_real f1, f2, q1, q2;
    dk_svf_t svf1, svf2;
    dk_real A, timer, p1, p2, p1Rate, p2Rate, e, eTarget, eRate;
} dk_rs_t;

typedef struct {
    dk_hp_t hp;
    dk_lp_t lp;
    dk_svf_t svfA, svfB;
    dk_real f, tailDecay, A, a, e1, e2;
    int k, done;
} dk_cp_t;

typedef struct {   /* Congas und Toms */
    dk_real f0, q0;
    dk_hp_t hp;
    dk_real f, q, noiseMix;
    dk_svf_t svf;
    dk_real A, dampMul, e, target, rate, nenv;
    int timer;
} dk_drum_t;

typedef struct {
    int open;
    dk_bq_t bp1, bp2, hpf;
    dk_hb_t hb;
    dk_real dec, A, timer, target, e, rate, p;
    int choked;
} dk_hat_t;

typedef struct {
    dk_bq_t bp1, bp2, hpA, hpB, hpLow;
    dk_lp_t lpLow;
    dk_hb_t hbA, hbB, hbOut;
    dk_real d40, d60, d78, tone, A, timer, c, m1, m2, envB, n, r1, r2, rB, rN, tB, pA, pB, pL;
} dk_cy_t;

typedef struct { dk_real f, q; dk_svf_t svf; dk_real e, timer; } dk_cl_t;
typedef struct { dk_bq_t hp; dk_real A, timer, env1, env1Target, env1Rate, e; } dk_ma_t;

typedef struct {
    dk_bq_t hpf;
    dk_hb_t hb;
    dk_real period1, period2, thr1, thr2, ph1, ph2, D, A, timer, e1, e2, t, r1, r2, lp1, lp2;
} dk_cb_t;

enum { DK808_QUEUE = 32 };

typedef struct {
    dk_real R, H, S40, R2, S40_2;   /* RateInfo bei R und bei 2·R */
    dk_real knob[DK_COUNT][DK808_KNOBS];
    int hat_choke, conga_choke, tom_choke;
    uint8_t active[DK_COUNT];
    dk_gate_t gate[DK_COUNT];
    struct { uint8_t inst, vel; } queue[DK808_QUEUE];
    unsigned queued;
#if DK808_NEEDS_NOISE
    dk_noise_t noise;
#endif
#if DK808_NEEDS_METAL
    dk_metal_t metal;
#endif
#if DK808_HAS(DK_BD)
    dk_bd_t bd;
#endif
#if DK808_HAS(DK_LG)
    dk_lg_t lg;
#endif
#if DK808_HAS(DK_SD)
    dk_sd_t sd;
#endif
#if DK808_HAS(DK_RS)
    dk_rs_t rs;
#endif
#if DK808_HAS(DK_CP)
    dk_cp_t cp;
#endif
#if DK808_HAS_CONGA
    dk_drum_t conga[3];
#endif
#if DK808_HAS_TOM
    dk_drum_t tom[3];
#endif
#if DK808_HAS_HAT
    dk_hat_t hat[2];   /* 0 geschlossen, 1 offen */
#endif
#if DK808_HAS(DK_CY)
    dk_cy_t cy;
#endif
#if DK808_HAS(DK_CL)
    dk_cl_t cl;
#endif
#if DK808_HAS(DK_MA)
    dk_ma_t ma;
#endif
#if DK808_HAS(DK_CB)
    dk_cb_t cb;
#endif
} dk808_t;

extern const baustein_t dk808_baustein;

/* Direkter Zugang (ohne baustein_t): */
void dk808_init(dk808_t *d, double sample_rate);
void dk808_trigger(dk808_t *d, unsigned instrument, int velocity);   /* gilt ab dem nächsten render */
void dk808_set_knob(dk808_t *d, unsigned instrument, unsigned knob, double value);
void dk808_render(dk808_t *d, float *left, float *right, int n);    /* addiert, mono auf beide Seiten */
dk_real dk808_tick(dk808_t *d);                                       /* ein Sample, löst wartende Trigger aus */
int dk808_note_instrument(int note);   /* MIDI-Note -> Instrument (Drum-Map des DrumKern), -1 = keins */
const char *dk808_instrument_abbr(unsigned instrument);
const char *dk808_knob_name(unsigned instrument, unsigned knob);   /* "" = kein Regler */
#endif
