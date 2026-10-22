/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Electro factory grooves and the style-based groove generator.
 * Control task only: these tables stay in Flash. */
#include "drums.h"
#include <string.h>

const char *const dr_style_names[DR_STYLES] = {"ELECTRO", "KRAFT", "ROTHER", "ROBOT", "MIAMI"};

enum { K808, KELECTRO, KMIAMI, KKRAFT };

/* One row per part in BD SD LT MT HT RS CP CB CY OH CH order, then accent.
 * x = hit, r = hit plus a 32nd roll, . = rest. NULL rows are empty. */
typedef struct {
    const char *name;
    uint8_t swing, kit;
    const char *row[DR_PARTS + 1];
} dr_recipe_t;

/* Minimal, classic electro, Kraftwerk- and Miami-bass-style grooves: kick,
 * snare or clap and hats carry everything; accents are sparse. Percussion
 * only where the style needs it (cowbell in PERFECT / MIAMI 2, rim on the
 * MIAMI clap). The song-style grooves are reconstructions, not transcriptions. */
static const dr_recipe_t factory[DR_PATTERNS] = {
    /* BD                  SD                  LT MT HT RS
       CP                  CB                  CY                  OH
       CH                  accent */
    {"ELECTRO", 54, KELECTRO, {
        "x.....x...x.....", "....x.......x...", 0, 0, 0, 0,
        0, 0, 0, 0,
        "x.x.x.x.x.x.x.x.", "x...........x..."}},
    {"BRONX 82", 52, KELECTRO, {
        "x.....x...x..x..", 0, 0, 0, 0, 0,
        "....x.......x...", 0, 0, 0,
        "x.x.x.x.x.x.x.x.", "x.....x........."}},
    {"NUMBERS", 50, KKRAFT, {
        "x..x..x...x.....", "....x.......x...", 0, 0, 0, 0,
        0, 0, 0, 0,
        "xxxxxxxxxxxxxxxx", "x...x...x...x..."}},
    {"TRANS EUR", 50, KKRAFT, {
        "x.......x.......", "....x.......x...", 0, 0, 0, 0,
        0, 0, 0, 0,
        "..x...x...x...x.", "x.......x......."}},
    {"TOUR", 50, KKRAFT, {
        "x.....x.x.......", "....x.......x..x", 0, 0, 0, 0,
        0, 0, 0, 0,
        "x.x.x.x.x.x.x.x.", "x...x.......x..."}},
    {"COMPUTER", 50, KKRAFT, {
        "x..x..x.x.......", "....x.......x...", 0, 0, 0, 0,
        0, 0, 0, 0,
        "x.xxx.xxx.xxx.xx", "x.......x......."}},
    {"ROTHER", 50, KELECTRO, {
        "x...x...x...x...", 0, 0, 0, 0, 0,
        "....x.......x...", 0, 0, "..x...x...x...x.",
        "x...x...x...x...", "x...x...x...x..."}},
    /* Classic booty bass (Luke / 2 Live Crew style): rim doubles the clap,
     * gapped 16th hats for the jerky stop-start feel. */
    {"MIAMI", 50, KMIAMI, {
        "x.....x...x.....", 0, 0, 0, 0, "....x.......x...",
        "....x.......x...", 0, 0, "..x...........x.",
        "x..xx.xxx.xxx..x", "x...x.......x..."}},
    {"DROWNED", 50, K808, {
        "x.....x.x.....x.", "....x.......x...", 0, 0, 0, 0,
        0, 0, 0, 0,
        "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x."}},
    {"EBM", 50, KKRAFT, {
        "x...x...x...x...", "....x.......x...", 0, 0, 0, 0,
        0, 0, 0, 0,
        "..x...x...x...x.", "x...x...x...x..."}},
    /* Car-audio bass (Bass Patrol / Magic Mike style): sparse kicks so the
     * long 808 tail rings like a sub bass, open hats on the offbeats. */
    {"BOOM BASS", 50, KMIAMI, {
        "x......x..x.....", 0, 0, 0, 0, 0,
        "....x.......x...", 0, 0, "..x...x...x...x.",
        "xx.xxx.xxx.xxx.x", "x...x.....x.x..."}},
    {"ROBOT", 50, KKRAFT, {
        "x...x.....x.....", "....x.......x...", 0, 0, 0, "...........x....",
        0, 0, 0, 0,
        "..x...x...x...x.", "x...x...x...x..."}},
    {"BUILD UP", 50, KELECTRO, {
        "x.....x...x.....x.....x...x.....", "....x.......x.......x.......x.xx", 0, 0, 0, 0,
        0, 0, "x...............................", 0,
        "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x...", "x...........x...............x.xx"}},
    /* Electro-bass, two bars (Dynamix II / Maggotron style): the kick plays
     * the bass figure, cowbell answers, snare pickup at the end. */
    {"MIAMI 2", 50, KMIAMI, {
        "x.....x.x.....x.x.....x...x.x...", "....x.......x.......x.......x..x", 0, 0, 0, 0,
        "....x.......x.......x.......x...", "...x.....x.....x...x.....x......", 0, 0,
        "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", "x.......x.......x..............."}},
    /* In the style of "Looking for the Perfect Beat" (Baker / Robie, 1983):
     * straight electro, clap over snare, cowbell figure, open hat pickup. */
    {"PERFECT", 50, KELECTRO, {
        "x.....x...x.....", "....x.......x...", 0, 0, 0, 0,
        "....x.......x...", "x..x..x...x.x...", 0, "..............x.",
        "x.x.x.x.x.x.x...", "x...x.......x..."}},
    /* The same, two bars: busier kick and a tom run into the next bar. */
    {"PERFECT 2", 50, KELECTRO, {
        "x.....x...x.....x.....x.x.x.x.x.", "..............................xx",
        "............................x...", "..........................x.....", "........................x.......", 0,
        "....x.......x.......x.......x...", 0, 0, 0,
        "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", "x...x.......x.......x.......x.xx"}},
};

static uint32_t parse_row(const char *row, uint32_t *roll, unsigned *length)
{
    uint32_t bits = 0;
    if (!row) return 0;
    unsigned n = (unsigned)strlen(row);
    if (n > DR_STEPS) n = DR_STEPS;
    if (n > *length) *length = n;
    for (unsigned i = 0; i < n; ++i) {
        if (row[i] == 'x' || row[i] == 'r') bits |= 1u << i;
        if (row[i] == 'r' && roll) *roll |= 1u << i;
    }
    return bits;
}

void dr_factory_pattern(unsigned slot, dr_pattern_t *out)
{
    memset(out, 0, sizeof *out);
    if (slot >= DR_PATTERNS) slot = DR_PATTERNS - 1;
    const dr_recipe_t *r = &factory[slot];
    unsigned length = 0;
    strncpy(out->name, r->name, DR_NAME);
    for (unsigned p = 0; p < DR_PARTS; ++p) out->hit[p] = parse_row(r->row[p], &out->roll[p], &length);
    out->accent = parse_row(r->row[DR_PARTS], 0, &length);
    out->length = (uint8_t)(length ? length : 16);
    out->swing = r->swing;
    out->kit = r->kit;
}

/* Generator weights per style: one digit per 16th, 0 = never, 9 = always.
 * Same row order as the factory recipes; the last row is the accent. */
static const char *const weights[DR_STYLES][DR_PARTS + 1] = {
    /* Minimal on purpose: kick, snare or clap and hats; percussion is a rare
     * single colour note. Rows: BD SD LT MT HT RS CP CB CY OH CH accent. */
    /* ELECTRO: syncopated kick, backbeat, straight 8th hats. */
    {"9000003000500000", "0000900000009000", 0, 0, 0, 0,
     0, 0, 0, "0000000000000020",
     "8080808080808080", "9000000000009000"},
    /* KRAFT: rigid and mechanical, tight 16th hats, the odd syncopation. */
    {"9002006000600000", "0000900000009001", 0, 0, 0, 0,
     0, 0, 0, 0,
     "8484848484848484", "9000900090009000"},
    /* ROTHER: four to the floor, clap backbeat, offbeat open hat. */
    {"9000900090009020", 0, 0, 0, 0, 0,
     "0000900000009000", 0, 0, "0080008000800080",
     "6000600060006000", "9000900090009000"},
    /* ROBOT: sparse and broken, one rim colour at most. */
    {"9000003000300000", "0000800000008000", 0, 0, 0, "0000000001000000",
     0, 0, 0, 0,
     "0060006000600060", "9000700070009000"},
    /* MIAMI: booming kick on the one and late syncopation. */
    {"9000000400500000", "0000900000009000", 0, 0, 0, 0,
     0, 0, 0, 0,
     "8080808080808080", "0000900000009000"},
};

static uint32_t next_random(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return *s = n ? n : 0x9e3779b9u;
}

static uint32_t roll_row(const char *row, unsigned density, uint32_t *rng, unsigned offset, unsigned count)
{
    uint32_t bits = 0;
    if (!row) return 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned w = (unsigned)(row[i & 15] - '0');
        if (!w) continue;
        /* Density 64 keeps the written odds; 0 halves them, 127 doubles them. */
        unsigned chance = w == 9 ? 1000u : w * 111u * (64u + density) / 128u;
        if (next_random(rng) % 1000u < chance) bits |= 1u << (offset + i);
    }
    return bits;
}

void dr_generate(dr_pattern_t *out, unsigned style, unsigned density, uint32_t seed,
                 const dr_pattern_t *keep)
{
    if (style >= DR_STYLES) style = 0;
    if (density > 127) density = 127;
    uint32_t rng = seed ? seed : 0x1234567u;
    dr_pattern_t p;
    memset(&p, 0, sizeof p);
    p.length = keep ? keep->length : 16;
    p.swing = style == DR_STYLE_MIAMI ? 56 : style == DR_STYLE_ELECTRO ? 54 : 50;
    p.kit = style == DR_STYLE_MIAMI ? KMIAMI : style == DR_STYLE_KRAFT ? KKRAFT : style == DR_STYLE_ROBOT ? K808 : KELECTRO;
    strncpy(p.name, dr_style_names[style], DR_NAME);
    /* Generate 16 steps, then let each further 16 vary the first ones. */
    for (unsigned part = 0; part <= DR_PARTS; ++part) {
        const char *row = weights[style][part];
        uint32_t first = roll_row(row, density, &rng, 0, 16), bits = first;
        for (unsigned bar = 16; bar < p.length; bar += 16) {
            uint32_t fresh = roll_row(row, density, &rng, bar, 16);
            uint32_t mask = 0;
            for (unsigned i = 0; i < 16; ++i)
                if (next_random(&rng) % 4u == 0) mask |= 1u << i;
            bits |= ((first & ~mask) << bar) | (fresh & (mask << bar));
        }
        if (p.length < DR_STEPS) bits &= (1u << p.length) - 1u;
        if (part == DR_PARTS) p.accent = bits;
        else p.hit[part] = bits;
    }
    /* Closed and open hat on the same step: the open hat wins. */
    p.hit[D8_CH] &= ~p.hit[D8_OH];
    /* A hat roll now and then, only with high density. */
    for (unsigned i = 0; i < p.length; ++i)
        if ((p.hit[D8_CH] & (1u << i)) && (i & 1u) && density > 96 && next_random(&rng) % 1000u < 30u)
            p.roll[D8_CH] |= 1u << i;
    if (keep) {
        p.hit[D8_BD] = keep->hit[D8_BD];
        p.roll[D8_BD] = keep->roll[D8_BD];
    }
    *out = p;
}
