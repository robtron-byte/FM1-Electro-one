/* SPDX-License-Identifier: GPL-3.0-only */
/* Sequencer-Zusätze nach den iOS-Apps des Nutzers (JX8P/MKS50 iOS, Shared/DSP/StepSequencer.swift,
 * Shared/Model/SequenceGenerator.swift, Shared/UI/PatternBank.swift; ADR 0052): Akzent und Tie je Schritt, GATE auf der
 * Mehrschritt-Skala, G.VAR (lange Noten aus einem festen Zufallswert je Schritt), Zufallsfolgen in fünf Stilen (NEW,
 * VARY) und eine Bank mit 8 Plätzen, die am Ende der laufenden Folge umschaltet. Reines C ohne Speicherverwaltung, für
 * das Gerät (Audio- und UI-Seite) und den Host-Test test_pat.c.
 *
 * Schritt wie im Gerüst: Note absolut (MIDI) und Flags SP_REST, SP_TIE, SP_ACC. Die Apps speichern Noten relativ zur
 * gehaltenen Taste (−12 … +18); hier relativ zu SP_BASE, das Gerüst transponiert wie bisher mit der Taste. */
#ifndef SYN_PAT_H
#define SYN_PAT_H
#include <stdint.h>

enum { SP_REST = 1, SP_TIE = 2, SP_ACC = 4 };
enum { SP_STEPS = 16, SP_SLOTS = 8, SP_STYLES = 5, SP_BASE = 48 };   /* NEW erzeugt 16 Schritte um C3 */
enum { SP_VEL = 88, SP_VEL_ACC = 127 };          /* Anschlag normal / Akzent (StepSequencer.swift) */

static const char *const sp_style_names[SP_STYLES] = {"KRAFT", "ROTHER", "ROBOT", "ACID", "MIAMI"};

/* ---- Zufall: xorshift32 (Gerät: Startwert aus dem Zeitgeber) */
static inline uint32_t sp_rand(uint32_t *s)
{
    uint32_t x = *s ? *s : 2463534242u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}
static inline unsigned sp_below(uint32_t *s, unsigned n) { return n ? (unsigned)((sp_rand(s) >> 8) % n) : 0u; }
static inline int sp_chance(uint32_t *s, unsigned percent) { return sp_below(s, 100u) < percent; }
#define SP_PICK(s, arr) ((arr)[sp_below((s), (unsigned)(sizeof(arr) / sizeof((arr)[0])))])

/* GATE 0…127 → Länge in Schritten: 0…80 sind 5 … 100 % eines Schritts, darüber ganze Schritte 1 … 8 (die Note klingt über
 * Pausen weiter, bis ihre Länge um ist oder die nächste Note kommt) */
static inline float sp_gate_steps(unsigned v)
{
    if (v > 127u) v = 127u;
    if (v <= 80u) return 0.05f + 0.95f * (float)v * (1.0f / 80.0f);
    return (float)(1u + (14u * (v - 80u) + 47u) / 94u);   /* gerundet wie .rounded() */
}

/* Rohwert eines Schritts wie in den Apps (0 Pause, sonst 1 + (Note + 12) + 32·Akzent + 64·Tie): Grundlage des G.VAR-Hashs */
static inline unsigned sp_raw(uint8_t note, uint8_t flag)
{
    int n;
    if (flag & SP_REST) return 0u;
    n = (int)note - SP_BASE;
    n = n < -12 ? -12 : n > 18 ? 18 : n;
    n = 1 + n + 12 + ((flag & SP_ACC) ? 32 : 0) + ((flag & SP_TIE) ? 64 : 0);
    return (unsigned)(n > 127 ? 127 : n);
}

/* G.VAR (0…127): Länge der Note auf Schritt i in ganzen Schritten (1 = GATE wie eingestellt). Jeder Schritt hat einen
 * festen Zufallswert aus der Folge selbst (FNV über die ersten 16 Schritte), also kommen mit mehr G.VAR nur lange Noten
 * hinzu (zuerst auf den Zählzeiten); nie über das Ende der Folge hinaus */
static inline unsigned sp_hold_steps(unsigned i, unsigned count, unsigned gvar, const uint8_t *note, const uint8_t *flag)
{
    uint64_t h = 0xcbf29ce484222325ull;
    float chance, pick, weight;
    unsigned len;
    if (!gvar || i >= count || (flag[i] & SP_REST)) return 1u;
    for (unsigned k = 0; k < SP_STEPS; k++) h = (h ^ (uint64_t)(k < count ? sp_raw(note[k], flag[k]) : 0u)) * 0x100000001b3ull;
    h = (h ^ (uint64_t)(i + 1u)) * 0x9e3779b97f4a7c15ull;
    h ^= h >> 29;
    chance = (float)(h & 0xFFFFu) * (1.0f / 65536.0f);
    pick = (float)((h >> 16) & 0xFFFFu) * (1.0f / 65536.0f);
    weight = i % 4u == 0u ? 1.0f : i % 2u == 0u ? 0.8f : 0.45f;
    if (!(chance < (float)gvar * (1.0f / 127.0f) * 0.8f * weight)) return 1u;
    len = pick < 0.5f ? 2u : pick < 0.8f ? 3u : 4u;
    return len < count - i ? len : count - i;
}

/* ---- Zufallsfolgen (SequenceGenerator.swift): 16 Schritte, note relativ zu SP_BASE */
typedef struct { int8_t rel[SP_STEPS]; uint8_t flag[SP_STEPS]; } sp_gen_t;
static inline void sp_set(sp_gen_t *g, unsigned i, int rel, int acc, int tie)
{
    g->rel[i] = (int8_t)(rel < -12 ? -12 : rel > 18 ? 18 : rel);
    g->flag[i] = (uint8_t)((acc ? SP_ACC : 0) | (tie ? SP_TIE : 0));
}
static inline void sp_rest(sp_gen_t *g, unsigned i) { g->rel[i] = 0; g->flag[i] = SP_REST; }
static inline int sp_euclid(unsigned k, unsigned n, unsigned rot, unsigned i) { return ((i + rot) % n) * k % n < k; }
static inline int sp_phrygian(int n)
{
    static const int8_t deg[12] = {0, 1, 1, 3, 3, 5, 5, 7, 8, 8, 10, 10};
    int o = n >= 0 ? n / 12 : -((11 - n) / 12);
    return 12 * o + deg[n - 12 * o];
}
static const int8_t sp_scale_kraft[] = {0, 0, 12, 7, 3, 10, 5, -5};
static const int8_t sp_scale_rother[] = {0, 0, 0, 12, 12, 1, -2, 7, 3, -12, 10};
static const int8_t sp_scale_robot[] = {0, 7, 12, 15, 3, 10, 17, 5};
static const int8_t sp_scale_acid[] = {0, 0, 12, 3, 7, 10, 5, -2, 8};
static const int8_t sp_scale_miami[] = {0, 0, 0, 0, 5, 7, -5, 3, -2, 12};
static inline int sp_scale_pick(unsigned style, uint32_t *s)
{
    switch (style) {
    case 1: return SP_PICK(s, sp_scale_rother);
    case 2: return SP_PICK(s, sp_scale_robot);
    case 3: return SP_PICK(s, sp_scale_acid);
    case 4: return SP_PICK(s, sp_scale_miami);
    default: return SP_PICK(s, sp_scale_kraft);
    }
}
/* Kraftwerk: gerades 4er- oder 8er-Motiv, wiederholt, die letzte Wiederholung transponiert (Antwort) */
static inline void sp_kraft(sp_gen_t *g, uint32_t *s)
{
    static const int8_t alt3[] = {7, 3, 10}, answers[] = {0, 0, -2, 3, 5, -5};
    unsigned ml = sp_chance(s, 50) ? 4u : 8u, bounce = sp_chance(s, 40);
    sp_gen_t m;
    int answer;
    for (unsigned i = 0; i < ml; i++) {
        if (i == 0) sp_set(&m, i, 0, 1, 0);
        else if (bounce) sp_set(&m, i, sp_chance(s, 25) ? SP_PICK(s, alt3) : (i % 2u ? 12 : 0), 0, 0);
        else if (sp_chance(s, 18)) sp_rest(&m, i);
        else sp_set(&m, i, sp_scale_pick(0, s), i % 4u == 0u, 0);
    }
    answer = SP_PICK(s, answers);
    for (unsigned i = 0; i < SP_STEPS; i++) {
        unsigned k = i % ml, last = i / ml == SP_STEPS / ml - 1u;
        g->rel[i] = m.rel[k]; g->flag[i] = m.flag[k];
        if (last && !(m.flag[k] & SP_REST)) sp_set(g, i, m.rel[k] + answer, m.flag[k] & SP_ACC, 0);
    }
}
/* Anthony Rother: 3–7 Noten im Takt, meist neben der Electro-Kick, Schritte in phrygisch Moll, ab und zu gebunden;
 * oft Ruf und Antwort (zweite Hälfte = erste, das Ende verändert) */
static inline void sp_rother(sp_gen_t *g, uint32_t *s)
{
    static const uint8_t counts[] = {3, 4, 4, 5, 5, 6, 7};
    static const int8_t moves[] = {0, 0, -1, -1, -1, -1, 1, 5, -5, 7, -7, 3, -3, 2, -2, 4, -4, 12};
    static const int8_t ends[] = {12, -2, 1, 7};
    unsigned cnt = SP_PICK(s, counts), rot = sp_below(s, 16), on = 1u;
    int note = 0;
    for (unsigned i = 0; i < SP_STEPS; i++)
        if (sp_euclid(cnt, 16, rot, i)) {
            unsigned k = i;
            if ((k == 3 || k == 6 || k == 10 || k == 13) && !sp_chance(s, 25)) k = (k + 1u) % 16u;
            on |= 1u << k;
        }
    for (unsigned i = 0; i < SP_STEPS; i++) {
        unsigned off = i % 4u == 2u || i % 4u == 3u;
        if (!(on >> i & 1u)) { sp_rest(g, i); continue; }
        if (i) { int t = note + SP_PICK(s, moves); note = sp_phrygian(t < -5 ? -5 : t > 13 ? 13 : t); }
        sp_set(g, i, note, i == 0 || (off && sp_chance(s, 45)), i < 15u && (on >> (i + 1u) & 1u) && sp_chance(s, 12));
    }
    if (sp_chance(s, 60)) {
        for (unsigned i = 0; i < 8u; i++) { g->rel[i + 8u] = g->rel[i]; g->flag[i + 8u] = g->flag[i]; }
        if (!(g->flag[14] & SP_REST)) sp_set(g, 14, SP_PICK(s, ends), g->flag[14] & SP_ACC, 0);
    }
}
/* Miami / Egyptian Lover: 3–5 Noten auf der Kick-Figur, meist der Grundton, oft 2–5 Schritte gehalten */
static inline void sp_miami(sp_gen_t *g, uint32_t *s)
{
    static const uint8_t fig[5][6] = {{3, 0, 6, 10}, {4, 0, 4, 6, 10}, {4, 0, 6, 8, 11}, {5, 0, 4, 6, 8, 12}, {4, 0, 6, 10, 13}};
    static const int8_t alt[] = {5, 7, -5, 3, -2, 12, 12};
    const uint8_t *f = fig[sp_below(s, 5)];
    for (unsigned i = 0; i < SP_STEPS; i++) sp_rest(g, i);
    for (unsigned n = 0; n < f[0]; n++) {
        unsigned i = f[1 + n], next = n + 1u < f[0] ? f[2 + n] : 16u;
        int note = (i == 0 || sp_chance(s, 75)) ? 0 : SP_PICK(s, alt);
        sp_set(g, i, note, i == 0 || sp_chance(s, 30), 0);
        if (sp_chance(s, 65)) {
            unsigned len = 2u + sp_below(s, 4);
            for (unsigned k = 1; k < len && i + k < next; k++) { g->flag[i + k - 1u] |= SP_TIE; sp_set(g, i + k, note, 0, 0); }
        }
    }
}
/* Roboter-Piepser: sparsam, hoch, weite Sprünge */
static inline void sp_robot(sp_gen_t *g, uint32_t *s)
{
    static const uint8_t ks[] = {4, 5, 6, 7};
    unsigned k = SP_PICK(s, ks), rot = sp_below(s, 4);
    for (unsigned i = 0; i < SP_STEPS; i++)
        if (i == 0 || sp_euclid(k, 16, rot, i)) sp_set(g, i, i ? sp_scale_pick(2, s) : 0, sp_chance(s, 30), 0);
        else sp_rest(g, i);
}
/* Electro-Acid: dicht, Bindungen (Slides) und Akzente, Moll */
static inline void sp_acid(sp_gen_t *g, uint32_t *s)
{
    static const uint8_t ks[] = {11, 12, 13};
    unsigned k = SP_PICK(s, ks);
    for (unsigned i = 0; i < SP_STEPS; i++) {
        if (!(i == 0 || sp_euclid(k, 16, 0, i))) { sp_rest(g, i); continue; }
        sp_set(g, i, i ? sp_scale_pick(3, s) : 0, sp_chance(s, 35), i < 15u && sp_euclid(k, 16, 0, i + 1u) && sp_chance(s, 25));
    }
}
static inline void sp_generate(sp_gen_t *g, unsigned style, uint32_t *s)
{
    switch (style) {
    case 1: sp_rother(g, s); break;
    case 2: sp_robot(g, s); break;
    case 3: sp_acid(g, s); break;
    case 4: sp_miami(g, s); break;
    default: sp_kraft(g, s); break;
    }
}
/* VARY: 1–2 Schritte (nie der erste) in der Tonleiter des Stils ändern; eine Pause wird Note, selten eine Note Pause */
static inline void sp_vary(sp_gen_t *g, unsigned style, uint32_t *s)
{
    unsigned n = 1u + sp_below(s, 2);
    while (n--) {
        unsigned i = 1u + sp_below(s, SP_STEPS - 1u);
        if ((g->flag[i] & SP_REST) || sp_chance(s, 25)) {
            if (g->flag[i] & SP_REST) sp_set(g, i, sp_scale_pick(style, s), 0, 0);
            else sp_rest(g, i);
        } else sp_set(g, i, sp_scale_pick(style, s), g->flag[i] & SP_ACC, g->flag[i] & SP_TIE);
    }
}

/* ---- Bank: 8 Plätze mit der ganzen Folge eines Parts (bis 64 Schritte) und RATE, GATE, G.VAR */
enum { SP_BANK_STEPS = 64, SP_BANK_VER = 1 };
typedef struct {
    uint8_t used, len, div, gate, gvar, rsv[3];
    uint8_t note[SP_BANK_STEPS], flag[SP_BANK_STEPS];
} sp_slot_t;
typedef struct { uint8_t ver, rsv[3]; sp_slot_t s[SP_SLOTS]; } sp_bank_t;

#endif
