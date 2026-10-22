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

static const dr_recipe_t factory[DR_PATTERNS] = {
    {"ELECTRO", 54, KELECTRO, {
        "x.....x...x.....", "....x.......x...", 0, 0, 0, 0,
        "....x.......x...", "..x.....x.x.....", 0, "..............x.",
        "x.x.x.x.x.x.xr..", "x...x.......x..."}},
    {"BRONX 82", 52, KELECTRO, {
        "x.....x...x..x..", "....x.......x...", 0, 0, 0, 0,
        "....x.......x...", "x..x..x...x..x..", 0, 0,
        "x.xxx.xxx.xxx.xx", "x.....x.....x..."}},
    {"NUMBERS", 50, KKRAFT, {
        "x..x..x...x.....", "....x.......x..x", 0, 0, 0, "..x...x.x.....x.",
        0, "..........x.....", 0, 0,
        "xxxxxxxxxxxxxxxx", "x...x...x...x..."}},
    {"ROTHER", 50, KELECTRO, {
        "x...x...x..x.x..", 0, 0, 0, 0, "......x.......x.",
        "....x.......x...", 0, 0, "..x...x...x...x.",
        "xx.xxx.xxx.xxx.x", "x...x...x...x..."}},
    {"MIAMI", 56, KMIAMI, {
        "x......x..x.....", "....x.......x...", 0, 0, 0, 0,
        "....x.......x...", 0, 0, 0,
        "x.x.x.x.x.x.x.xr", "....x.......x..."}},
    {"BREAKER", 55, K808, {
        "x.x.......x..x..", "....x..x.x..x...", 0, 0, 0, 0,
        0, 0, 0, "......x.........",
        "x.x.x...x.x.x.x.", "....x.......x..."}},
    {"FREESTYL", 54, KELECTRO, {
        "x..x..x.x..x....", "....x.......x...", 0, 0, 0, 0,
        "....x.......x...", "...x.......x....", 0, 0,
        "x.xrx.x.x.xrx.x.", "x...x..x....x..."}},
    {"DROWNED", 50, K808, {
        "x.....x.x.....x.", "....x.......x...", "..........x.....", ".......x.....x..", "...x............", 0,
        0, 0, 0, 0,
        "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x."}},
    {"EBM", 50, KKRAFT, {
        "x...x...x...x...", "....x.......x...", 0, 0, 0, 0,
        "....x.......x...", 0, 0, "..x...x...x...x.",
        0, "x...x...x...x..."}},
    {"TRANS EUR", 50, KKRAFT, {
        "x...x.x...x.x...", "....x.......x...", 0, 0, 0, 0,
        0, "........x.......", 0, 0,
        "x.x.x.x.x.x.x.x.", "x...x...x...x..."}},
    {"HALFTIME", 52, KMIAMI, {
        "x.........x.....", "........x.......", 0, 0, 0, 0,
        "........x.......", 0, 0, "..............x.",
        "x.x.x.xrx.x.xr..", "x.......x......."}},
    {"CLAVES", 54, KKRAFT, {
        "x.....x...x.....", 0, 0, "..x.....x.......", ".....x.....x..x.", "x..x..x...x.x...",
        "....x.......x...", 0, 0, 0,
        "x.x.x.x.x.x.x.x.", "x..x..x...x.x..."}},
    {"BUILD UP", 50, KELECTRO, {
        "x.....x...x.....x.....x...x.....", "....x.......x.......x..x.xrxrrrr", 0, 0, 0, 0,
        "....x.......x.......x...........", 0, "x...............................", 0,
        "x.x.x.x.x.x.x.x.x.x.x.x.x.x.....", "....x.......x.......x.......x.xx"}},
    {"SPACE", 52, K808, {
        "x.......x.x.....", 0, 0, 0, 0, 0,
        "....x.......x...", "x.....x.....x...", "x...............", 0,
        "..x...x...x...x.", "x...x...x...x..."}},
    {"INIT A", 50, K808, {0}},
    {"INIT B", 50, KELECTRO, {0}},
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
    /* ELECTRO: syncopated kick, backbeat, busy hats, cowbell answers. */
    {"9002003050600200", "0000900000009001", 0, 0, 0, "0010000100100010",
     "0000800000008000", "0030002030200300", 0, "0000000200000030",
     "8060806080608060", "9000800030009000"},
    /* KRAFT: rigid and mechanical, claves and tight 16th hats. */
    {"9006006000600000", "0000900000009002", 0, 0, 0, "0040040300400040",
     0, "0000000000300000", 0, 0,
     "8686868686868686", "9000900090009000"},
    /* ROTHER: driving four-to-the-floor electro techno, offbeat open hat. */
    {"9000900090030300", 0, 0, 0, 0, "0000003000000030",
     "0000900000009000", 0, 0, "0090009000900090",
     "6606660666066606", "9000900090009000"},
    /* ROBOT: sparse and broken, toms and rimshots. */
    {"9000003000300000", "0000800000008000", "0000000000300000", "0000000300000300", "0003000000000000", "0020000200020000",
     0, "0000002000000020", 0, 0,
     "7050705070507050", "9000700070009000"},
    /* MIAMI: booming kick on the one and late syncopation, 16th rolls. */
    {"9000000400500300", "0000900000009000", 0, 0, 0, 0,
     "0000900000009000", 0, 0, 0,
     "8080808080808084", "0000900000009000"},
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
    /* Hat rolls on some off-16ths, more with higher density. */
    for (unsigned i = 0; i < p.length; ++i)
        if ((p.hit[D8_CH] & (1u << i)) && (i & 1u) && next_random(&rng) % 1000u < 40u + density * 2u)
            p.roll[D8_CH] |= 1u << i;
    if (keep) {
        p.hit[D8_BD] = keep->hit[D8_BD];
        p.roll[D8_BD] = keep->roll[D8_BD];
    }
    *out = p;
}
