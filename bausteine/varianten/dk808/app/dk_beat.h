/* SPDX-License-Identifier: GPL-3.0-only
 * 808 solo: die Drum-Machine wie der Bereich BEAT von FM1-Electro (ADR 0050). Übernommen aus engine/drums.c und
 * engine/drum_factory.c (FM1-Electro, GPL-3.0-only WITH GPL-3.0-linking-exception): Plätze A–D mit Wechsel am Ende
 * des Patterns, bis 32 Steps, Akzent-Zeile, 32tel-Rolls, Swing, Mute/Solo, Fills je Platz, Bedingungen, Wahrscheinlichkeit,
 * Velocity je Step, Kette, Generator nach Stilen. Anders als dort: die 17 Instrumente des DrumKern (dk808) statt der
 * zehn von tr808.c, auf zwei Zustände für zwei Kerne verteilt (dk808_set_run); keine Tonhöhe je Step (dk808 hat keine).
 *
 * Threads: die Bedienung liest (32-Bit-Lesen ist sicher) und ändert nur über db_post / db_post_pattern; der
 * Audio-Takt wendet die Befehle in db_block an (Haken GM_PRE_C1, bevor Kern 1 startet), rechnet die Steps und schlägt
 * beide Zustände an. db_render_k0 rechnet Zustand A auf Kern 0, db_render_k1 Zustand B auf Kern 1. */
#ifndef DK_BEAT_H
#define DK_BEAT_H
#include <stdint.h>
#include "dk808_bs.h"

/* Teile in der Reihenfolge der schwarzen Tasten von BEAT (ZAP und PULSE von FM1-Electro: Laser Gun und Claves), dann
 * die übrigen sieben (GLO + SELECT) */
enum { P_BD, P_SD, P_RS, P_CP, P_CB, P_CY, P_OH, P_CH, P_LG, P_CL, P_LT, P_MT, P_HT, P_LC, P_MC, P_HC, P_MA, DB_PARTS };
enum {
    DB_KEYPARTS = 10,       /* schwarze Tasten 1–10; die 11. ist die Akzent-Zeile */
    DB_STEPS = 32,
    DB_PATTERNS = 4,        /* Plätze A–D */
    DB_KITS = 4,
    DB_STYLES = 16,
    DB_NAME = 10,
    DB_KNOBS = 4,           /* je Teil: LEVEL und die drei Regler des DrumKern */
    DB_FILL_STEPS = 16,
    DB_CHAIN = 4
};
enum { DB_CONDS = 7, DB_PROBS = 10 };   /* 1:1 1:2 2:2 1:4 2:4 3:4 4:4; 100 % .. 10 % */

typedef struct {
    char name[DB_NAME + 1];
    uint8_t length, swing, kit, style;
    uint32_t hit[DB_PARTS], roll[DB_PARTS], accent;
} db_pattern_t;

/* ein Kit: je Teil LEVEL (0..100) und die drei Regler (0..100), dazu die Choke-Schalter */
typedef struct { uint8_t v[DB_PARTS][DB_KNOBS]; uint8_t hat_choke, conga_choke, tom_choke, pad; } db_sound_t;

typedef struct { uint16_t hit[DB_PARTS], roll[DB_PARTS], accent; uint8_t used, pad; } db_fill_t;

enum db_command {
    DB_HIT,         /* a = Teil, b = Velocity 1..127, c = 1 aufnehmen, 2 aufnehmen ohne Klang */
    DB_TOGGLE, DB_ROLL, DB_ACCENT,      /* a = Teil, b = Step */
    DB_CLEAR_PART,  /* a = Teil (DB_PARTS: das ganze Pattern) */
    DB_LENGTH,      /* b = 1..32 */
    DB_SWING,       /* b = 50..75 */
    DB_SELECT,      /* a = Platz; b = 1 sofort, sonst am Ende des Patterns */
    DB_COPY,        /* a = von, b = nach */
    DB_LOAD,        /* a = Platz, c = 1 Bedingungen behalten: das Pattern aus staging */
    DB_MUTE, DB_SOLO,   /* a = Teil, b = an */
    DB_PARAM,       /* a = Teil, b = Regler 0..3, value */
    DB_CHOKE,       /* a = 0 Hats, 1 Congas, 2 Toms, b = an */
    DB_KIT,         /* a = Kit; b = 1: sein Werksklang */
    DB_LEVEL,       /* value = 0..127 */
    DB_RECORD,      /* b = an */
    DB_PANIC,
    DB_COND, DB_PROB, DB_VOL,   /* a = Teil, b = Step, value */
    DB_FILL,        /* b = 0 keiner, 1 im nächsten Takt einmal, 2 jeden Takt, solange gehalten */
    DB_FILL_AUTO,   /* b = 0 aus, sonst alle b Takte */
    DB_FILL_EDIT,   /* b = an */
    DB_FILL_TOGGLE, DB_FILL_ACCENT, DB_FILL_ROLL,   /* a = Teil, b = Step 0..15 */
    DB_CHAIN_SET,   /* b = Glieder 0..4, value = Plätze je 2 Bit */
    DB_AREA_MUTE    /* b = an */
};

typedef struct { uint8_t type, a, b, c; int16_t value; } db_cmd_t;
enum { DB_QUEUE = 64 };

typedef struct {
    db_pattern_t pattern[DB_PATTERNS];
    db_sound_t sound;                       /* ein Kit für alle Plätze (wie BEAT seit 12.11.) */
    db_sound_t kit_mem[DB_KITS];            /* jedes Kit, wie zuletzt eingestellt (N1) */
    uint8_t kit_mem_used, kit_index, level_value, fill_auto;
    uint32_t cond[DB_PATTERNS][3][DB_PARTS];
    uint32_t prob[DB_PATTERNS][4][DB_PARTS];
    uint32_t vol[DB_PATTERNS][3][DB_PARTS];
    db_fill_t fill[DB_PATTERNS];
} db_store_t;                               /* was gespeichert wird */

typedef struct {
    db_store_t s;
    db_cmd_t q[DB_QUEUE];
    volatile uint32_t q_head, q_tail;
    db_pattern_t staging;
    volatile uint32_t staging_posted, staging_taken;
    uint32_t mute, solo;
    uint8_t current, next, pos, playing, recording, muted, pass, fill_req, fill_now, fill_edit;
    uint8_t chain[DB_CHAIN], chain_len, chain_pos;
    uint32_t bars, random, suppress, roll_pending;
    float level, step_frames, since_step, roll_countdown, roll_velocity[DB_PARTS], oh_short;
    uint32_t step_abs, last_tick_step;      /* Takt: Step seit Start, Stand */
    uint8_t was_running, pad[3];
    uint32_t hits_now;                      /* angeschlagene Teile im letzten Step (Anzeige) */
    volatile uint32_t revision;
} db_machine_t;

extern db_machine_t db;                     /* im Pool */
extern dk808_bs_t db_engA, db_engB;         /* Klang: Kern 0, Kern 1 (dist = Anteil in den Distortion-Send) */
#ifndef DK_K1_MASK
#define DK_K1_MASK DK808_METAL_USERS        /* Instrumente auf Kern 1 (Kombis: DK808_MASK_ALL) */
#endif
extern const char *const db_part_names[DB_PARTS];
extern const char *const db_kit_names[DB_KITS];
extern const char *const db_style_names[DB_STYLES];
extern const uint8_t db_part_dk[DB_PARTS];  /* Teil -> Instrument des dk808 */

/* vor dem Audio: Werkszustand (Platz A ein Groove, B–D leer); danach db_apply_sound */
void db_init(void);
/* nach dem Laden eines gespeicherten Stands (Bedienung, vor dem Audio): Zustände und Kit herstellen */
void db_restore(void);
int db_post(uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value);
int db_post_pattern(unsigned slot, const db_pattern_t *p, int keep_cond);

/* Generator (aus drum_factory.c): neuer Groove in einem Stil, Variation, Fill */
void db_generate(db_pattern_t *out, unsigned style, unsigned variation, uint32_t seed, const db_pattern_t *keep);
void db_vary(db_pattern_t *p, unsigned amount, uint32_t seed);
void db_empty_pattern(db_pattern_t *out, const char *name);

/* Audio-Takt (Kern 0, bevor Kern 1 startet): running 1 = der Takt läuft; ticks = Stand in 1/24 Viertel (mit Bruchteil);
 * frames = Länge des Blocks; bpm für die Step-Länge */
void db_block(int running, float ticks, float bpm, unsigned frames);
/* Note von MIDI oder der Live-Schleife: das Teil nach der Drum-Map, aufgenommen, wenn REC an ist */
void db_note(int note, int velocity);
void db_render_k0(float *l, float *r, float *dist, int n);   /* Zustand A */
void db_render_k1(float *l, float *r, float *dist, int n);   /* Zustand B */
int db_voices(void);
int db_part_of_note(int note);
unsigned db_part_note(unsigned part);

/* Lesehilfen für die Bedienung */
static inline unsigned db_bits3(const uint32_t (*c)[DB_PARTS], unsigned planes, unsigned part, unsigned step)
{
    unsigned v = 0;
    for (unsigned k = 0; k < planes; ++k) v |= ((c[k][part] >> step) & 1u) << k;
    return v;
}
static inline unsigned db_cond(unsigned slot, unsigned part, unsigned step) { return db_bits3(db.s.cond[slot & 3u], 3, part, step); }
static inline unsigned db_prob(unsigned slot, unsigned part, unsigned step) { return db_bits3(db.s.prob[slot & 3u], 4, part, step); }
static inline unsigned db_vol(unsigned slot, unsigned part, unsigned step) { return db_bits3(db.s.vol[slot & 3u], 3, part, step); }
int db_vol_percent(unsigned level);
const char *db_knob_name(unsigned part, unsigned knob);   /* "" = kein Regler */
#endif
