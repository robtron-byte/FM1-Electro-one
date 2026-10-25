/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Style-based groove generation. Nothing here is a fixed pattern: every
 * style lists typical alternatives per instrument (classic electro,
 * Kraftwerk, Miami bass, Egyptian Lover and Anthony Rother idioms) and the
 * generator picks and varies them, so each run gives a new groove that
 * stays in the style. The song styles are reconstructions from tempos and
 * descriptions, not transcriptions. Control task only: tables in Flash.
 * Tempos from published analyses (Tunebat, Beatsource, GetSongBPM; 2009
 * remasters): Numbers ~130, Computer World 129, Computer Love 125,
 * Trans-Europe Express 108, Destroy Him My Robots 129, Father 133 BPM. */
#include "drums.h"
#include <string.h>

enum { K808, KELECTRO, KMIAMI, KKRAFT };

/* Rows: BD SD LT HT RS CP CB CY OH CH ZP PU, then accent. Alternatives are
 * separated by '|'; an empty alternative means "not this time".
 * x = hit, r = hit plus a 32nd roll, . = rest. */
const dr_style_t dr_styles[DR_STYLES] = {
    {"ELECTRO", 1120, 54, KELECTRO, 16, 2, 0, 1, DR_HARMONY_ROTHER, {
        "x.....x...x.....|x.....x...x..x..|x.....x.x...x...", "....x.......x...", 0, 0, 0,
        "|....x.......x...", 0, 0, "|..............x.",
        "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.xxx.|xxx.xxx.xxx.xxx.", "|..............x.|", 0, "x...........x...|x...x.......x..."}},
    {"BRONX 82", 1180, 52, KELECTRO, 16, 2, 0, 0, DR_HARMONY_ELECTRO, {
        "x.....x...x..x..|x.....x...x.....", 0, 0, 0, 0,
        "....x.......x...", "|x..x..x...x..x..", 0, 0,
        "x.x.x.x.x.x.x.x.|x.xxx.xxx.xxx.xx", "|...........x....", 0, "x.....x.........|x...x.......x..."}},
    {"NUMBERS", 1300, 50, KKRAFT, 16, 0, 1, 0, DR_HARMONY_KRAFT, {
        "x..x..x...x.....|x..x..x...x..x..", "....x.......x..x|....x.......x...", 0, 0, 0,
        0, 0, 0, 0,
        "xxxxxxxxxxxxxxxx|x.xxx.xxx.xxx.xx", 0, 0, "x...x...x...x..."}},
    {"TRANS EUR", 1080, 50, KKRAFT, 16, 0, 1, 0, DR_HARMONY_KRAFT, {
        "x.......x.......|x.......x.x.....", "....x.......x...", 0, 0, 0,
        0, 0, 0, 0,
        "..x...x...x...x.|x.x.x.x.x.x.x.x.", 0, 0, "x.......x......."}},
    {"TOUR", 1240, 50, KKRAFT, 16, 0, 0, 0, DR_HARMONY_KRAFT, {
        "x.....x.x.......|x.....x.x.....x.", "....x.......x..x|....x.......x...", 0, 0, 0,
        0, 0, 0, 0,
        "x.x.x.x.x.x.x.x.", 0, 0, "x...x.......x..."}},
    {"COMPUTER", 1290, 50, KKRAFT, 16, 2, 2, 0, DR_HARMONY_KRAFT, {
        "x..x..x.x.......|x..x..x...x.....", "....x.......x...", 0, 0, 0,
        0, 0, 0, 0,
        "x.xxx.xxx.xxx.xx|xxxxxxxxxxxxxxxx", 0, 0, "x.......x......."}},
    /* ROTHER: measured on the TR-808 tracks of Anthony Rother's DEKATRON
     * (stems separated with Demucs): 133 BPM, straight, a syncopated electro
     * kick (not four on the floor), clap on 2 and 4, 16th hats with gaps. */
    {"ROTHER", 1330, 50, KELECTRO, 16, 2, 0, 1, DR_HARMONY_ROTHER, {
        "x..x..x...x..x..|x..x..x..x...x..|x..x..xx..x..x..|x.x....x.x......|x..x..x...x...x.",
        "|...x.....x.....x|..x........x...x", 0, 0, "|......x.......x.",
        "....x.......x...", 0, 0, "|..x...x...x...x.|.............x..",
        "x..x..x...x..x..|x.xxx.xxx.xxx.xx|x.x.x.x.x.x.x.x.|xx.xxx.xxx.xxx.x", "|..............x.", 0,
        "x.....x...x.....|x...x...x...x..."}},
    {"MIAMI", 1300, 50, KMIAMI, 16, 2, 0, 1, DR_HARMONY_ELECTRO, {
        "x.....x...x.....|x......x..x.....", 0, 0, 0, "....x.......x...|",
        "....x.......x...", 0, 0, "..x...........x.",
        "x..xx.xxx.xxx..x", 0, "|..x...x...x...x.", "x...x.......x..."}},
    {"MIAMI 2", 1250, 50, KMIAMI, 32, 2, 0, 1, DR_HARMONY_ELECTRO, {
        "x.....x.x.....x.x.....x...x.x...|x.....x.x.....x.x.....x.x...x...",
        "....x.......x.......x.......x..x", 0, 0, 0,
        "....x.......x.......x.......x...", "...x.....x.....x...x.....x......|", 0, 0,
        "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", 0, "|...x.......x.......x.......x....", "x.......x.......x..............."}},
    {"BOOM BASS", 1360, 50, KMIAMI, 16, 2, 1, 1, DR_HARMONY_ELECTRO, {
        "x......x..x.....|x.........x.....", 0, 0, 0, 0,
        "....x.......x...", 0, 0, "..x...x...x...x.",
        "xx.xxx.xxx.xxx.x|x...x...x...x...", 0, 0, "x...x.....x.x..."}},
    {"DROWNED", 1380, 50, K808, 16, 3, 1, 3, DR_HARMONY_ROTHER, {
        "x.....x.x.....x.|x.....x.x...x...", "....x.......x...", 0, 0, 0,
        0, 0, 0, 0,
        "xxxxxxxxxxxxxxxx", "|......x.........", 0, "x.x.x.x.x.x.x.x.|x...x...x...x..."}},
    {"ROBOT", 1200, 50, KKRAFT, 16, 2, 0, 0, DR_HARMONY_KRAFT, {
        "x...x.....x.....|x.........x.x...", "....x.......x...", 0, 0, "...........x....|",
        0, 0, 0, 0,
        "..x...x...x...x.", 0, 0, "x...x...x...x..."}},
    {"PERFECT", 1200, 50, KELECTRO, 16, 2, 0, 1, DR_HARMONY_ELECTRO, {
        "x.....x...x.....", "....x.......x...", 0, 0, 0,
        "....x.......x...", "x..x..x...x.x...|", 0, "..............x.",
        "x.x.x.x.x.x.x...", "|......x.......x.", 0, "x...x.......x..."}},
    /* EGYPT: measured on The Egyptian Lover's 1986 (stems separated with
     * Demucs): ~126 BPM, kick figures from The Internet, Lose Control, Get
     * Down, Cyborg and Freaky Girl, closed hats in 16ths that leave the clap
     * steps free, or open hats in 8ths. */
    {"EGYPT", 1260, 50, KMIAMI, 32, 2, 0, 1, DR_HARMONY_ELECTRO, {
        "x.....x...x.....x.....x...x..x..|x...x.x...x.....x...x.x...x.....|x...x.x...x..xx.x...x.x...x..x..|"
        "x.....x.x..x....x.....x.x..x....|x...x.x.x...x...x...x.x.x...x...|x..x..x...x..x..x..x..x...x..x..",
        "....x.......x.......x.......xxxx",
        "............................x...|", "..............................x.|",
        "...x......x..x.....x......x.....|",
        "....x.......x.......x.......xxxx", "..x...x.x.....x...x...x.x.......|", 0,
        "|||x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.",
        "xxxx..xxxxxx..xxxxxx..xxxxxx..xx|xxxxxxxxxxxxxxxxxxxxxxxxxxxx....|..xx....xx.....x..xx....xx.....x",
        "|..............................x.", "..x...x...x...x...x...x...x...x.|..xx..x...xx..x...xx..x...xx.x.x|x..x..x.x..x..x.x..x..x.x..x..x.", "x...x.x...x.x...x...x.x...x.xxxx"}},
    {"FREAKY", 1280, 50, KELECTRO, 32, 2, 2, 1, DR_HARMONY_ELECTRO, {
        "x.....x...x..x..x.....x...x.x..x", "....x.......x.......x.......x.xx", 0, 0, 0,
        "....x.......x.......x.......xxxx", "x..x..x...x.....x..x..x...x.....|", 0, "..x...x...x...x...x...x...x.....",
        "xx.xxx.xxx.xxx.xxx.xxx.xxx.xxxxx", "|..............x...............x.", "..x...x...x...x...x...x...x...xx|.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x|", "x.x.x.x.x.x.x.x.x.x.x.x.x.x.xxxx"}},
    {"LOVE", 1250, 53, KMIAMI, 32, 2, 1, 0, DR_HARMONY_KRAFT, {
        "x..x..x.x.....x.x..x..x.x..x..x.", "....x.......x.......x.......x...",
        "............................x...|", "..............................x.|", 0,
        "....x.......x..x....x.......x...", 0, "x...............................|", "..............x...............x.",
        "x.x.x.x.x.x.x...x.x.x.x.x.x.x...", 0, "|..x...x...x...x...x...x...x...x.|", "x.......x.......x.......x......."}},
};

static uint32_t next_random(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return *s = n ? n : 0x9e3779b9u;
}

/* Choose one of the '|' alternatives and parse it. */
static uint32_t pick_row(const char *row, uint32_t *roll, uint32_t *rng)
{
    if (!row) return 0;
    unsigned count = 1;
    for (const char *c = row; *c; ++c) count += *c == '|';
    unsigned want = next_random(rng) % count, index = 0, step = 0;
    uint32_t bits = 0;
    for (const char *c = row; *c; ++c) {
        if (*c == '|') { ++index; step = 0; continue; }
        if (index != want) continue;
        if (step < DR_STEPS && (*c == 'x' || *c == 'r')) bits |= 1u << step;
        if (step < DR_STEPS && *c == 'r' && roll) *roll |= 1u << step;
        ++step;
    }
    return bits;
}

void dr_empty_pattern(dr_pattern_t *out, const char *name)
{
    memset(out, 0, sizeof *out);
    strncpy(out->name, name, DR_NAME);
    out->length = 16;
    out->swing = 50;
}

void dr_generate(dr_pattern_t *out, unsigned style, unsigned variation, uint32_t seed,
                 const dr_pattern_t *keep)
{
    if (style >= DR_STYLES) style = 0;
    if (variation > 127) variation = 127;
    const dr_style_t *s = &dr_styles[style];
    uint32_t rng = seed ? seed : 0x1234567u;
    dr_pattern_t p;
    memset(&p, 0, sizeof p);
    strncpy(p.name, s->name, DR_NAME);
    p.length = s->length;
    p.swing = s->swing;
    p.kit = s->kit;
    p.style = (uint8_t)style;
    for (unsigned part = 0; part < DR_PARTS; ++part) p.hit[part] = pick_row(s->row[part], &p.roll[part], &rng);
    p.accent = pick_row(s->row[DR_PARTS], 0, &rng);
    uint32_t mask = p.length >= 32 ? 0xffffffffu : (1u << p.length) - 1u;
    /* Variation, kept musical: a ghost kick on an off-16th, a hat moved or
     * dropped, an open-hat pickup at the end of the bar. Downbeats, the
     * backbeat and the style's figure stay. */
    unsigned chance = variation * 4u;   /* per mille, up to ~50 % */
    if (next_random(&rng) % 1000u < chance) {
        unsigned step = 1u + 2u * (next_random(&rng) % (p.length / 2u));
        if (step < p.length && !(p.hit[D8_BD] & (5u << (step - 1u)))) p.hit[D8_BD] |= 1u << step;
    }
    if (next_random(&rng) % 1000u < chance) {
        unsigned step = next_random(&rng) % p.length;
        if (step & 1u) p.hit[D8_CH] ^= 1u << step;
    }
    if (!p.hit[D8_OH] && next_random(&rng) % 1000u < chance / 2u) p.hit[D8_OH] |= 1u << (p.length - 2u);
    for (unsigned part = 0; part < DR_PARTS; ++part) { p.hit[part] &= mask; p.roll[part] &= mask; }
    p.accent &= mask;
    /* Closed and open hat on the same step: the open hat wins. */
    p.hit[D8_CH] &= ~p.hit[D8_OH];
    if (keep) {
        p.hit[D8_BD] = keep->hit[D8_BD];
        p.roll[D8_BD] = keep->roll[D8_BD];
        p.length = keep->length;
    }
    *out = p;
}
