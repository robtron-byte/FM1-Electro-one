/* SPDX-License-Identifier: GPL-3.0-only
 * 808 solo: Drum-Machine wie BEAT (dk_beat.h, ADR 0050). Sequencer-Teil übernommen aus FM1-Electro engine/drums.c,
 * Generator und Stile aus engine/drum_factory.c (dort GPL-3.0-only WITH GPL-3.0-linking-exception); angepasst an 17
 * Teile und den Klang dk808 auf zwei Zuständen. Liegt als Klangdatei im SRAM (dsp_*.o); Tabellen, die der Audio-Takt
 * liest, in .dsp_rodata. */
#include <string.h>
#include "dk_beat.h"

#ifdef EL_PLATFORM
#define DSPD __attribute__((section(".dsp_rodata"), aligned(4)))
#define POOL __attribute__((section(".pool"), aligned(16)))
#else                                           /* (Host-Test) */
#define DSPD
#define POOL
#endif
#define SR 44100.0f

db_machine_t db POOL;
dk808_bs_t db_engA POOL;   /* Kern 0 */
dk808_bs_t db_engB POOL;   /* Kern 1 */

const char *const db_part_names[DB_PARTS] = {"BD", "SD", "RS", "CP", "CB", "CY", "OH", "CH", "LG", "CL",
                                              "LT", "HT", "LC", "HC", "MA"};
const uint8_t db_part_dk[DB_PARTS] DSPD = {DK_BD, DK_SD, DK_RS, DK_CP, DK_CB, DK_CY, DK_OH, DK_CH, DK_LG, DK_CL,
                                           DK_LT, DK_HT, DK_LC, DK_HC, DK_MA};
const char *const db_kit_names[DB_KITS] = {"TR-808", "ELECTRO", "MIAMI", "KRAFT"};
static const uint8_t part_notes[DB_PARTS] = {36, 38, 37, 39, 56, 49, 46, 42, 58, 75, 41, 43, 64, 62, 70};

enum { NORMAL_Q7 = 84, ROLL_Q7 = 96 };
#define NORMAL (NORMAL_Q7 / 127.0f)
#define OH_SHORT_MS 90.0f

/* ---- Kits: der Werksklang des DrumKern (TR-808) und drei Abwandlungen wie die Kits von BEAT; Teil, Regler (0 LEVEL,
 * 1..3 die Regler des DrumKern), Wert */
typedef struct { uint8_t part, knob, value; } kit_entry_t;
enum { KIT_ENTRIES = 10 };
static const kit_entry_t kits[DB_KITS][KIT_ENTRIES] = {
    {{255, 0, 0}},
    /* ELECTRO: längere Bassdrum, knackige Snare, kurze Hats */
    {{P_BD, 2, 45}, {P_BD, 3, 60}, {P_SD, 3, 75}, {P_SD, 1, 40}, {P_CH, 2, 35}, {P_OH, 2, 40}, {P_CB, 2, 40}, {255, 0, 0}},
    /* MIAMI: lange, tiefe Bassdrum, dunklere Snare */
    {{P_BD, 2, 90}, {P_BD, 1, 30}, {P_BD, 3, 70}, {P_SD, 1, 15}, {P_SD, 3, 40}, {P_CH, 2, 40}, {255, 0, 0}},
    /* KRAFT: kurze, klickige Bassdrum, hohe Snare, dunkles Becken */
    {{P_BD, 2, 12}, {P_BD, 1, 80}, {P_SD, 1, 70}, {P_SD, 3, 55}, {P_CY, 1, 30}, {P_RS, 1, 70}, {255, 0, 0}},
};
static void kit_factory(db_sound_t *s, unsigned index)
{
    memset(s, 0, sizeof *s);
    for (unsigned p = 0; p < DB_PARTS; ++p) {
        s->v[p][0] = 100;
        for (unsigned k = 0; k < 3u; ++k) s->v[p][1u + k] = dk808_bs_init_kit[DK_BS_KNOB + db_part_dk[p] * 3u + k];
    }
    s->hat_choke = 1;
    if (index >= DB_KITS) return;
    for (unsigned i = 0; i < KIT_ENTRIES && kits[index][i].part != 255; ++i)
        s->v[kits[index][i].part][kits[index][i].knob] = kits[index][i].value;
}

/* ---- Stimmen: beide Zustände bekommen jeden Anschlag; jeder spielt nur die Instrumente seiner Maske */
static void v_trigger(unsigned part, float velocity)
{
    int v = (int)(velocity * 127.0f + 0.5f);
    if (part >= DB_PARTS || v <= 0) return;
    if (v > 127) v = 127;
    /* CH und OH auf verschiedenen Zuständen: die geschlossene Hat dämpft die offene auch drüben (Choke des Kerns) */
    if (part == P_CH && db.s.sound.hat_choke) { dk808_choke(&db_engA.k, DK_OH); dk808_choke(&db_engB.k, DK_OH); }
    dk808_trigger(&db_engA.k, db_part_dk[part], v);
    dk808_trigger(&db_engB.k, db_part_dk[part], v);
}
static void v_choke(unsigned part) { dk808_choke(&db_engA.k, db_part_dk[part]); dk808_choke(&db_engB.k, db_part_dk[part]); }
static void v_silence(void) { dk808_bs_all_off(&db_engA); dk808_bs_all_off(&db_engB); }
static void v_param(unsigned part, unsigned knob, unsigned value)
{
    unsigned i = db_part_dk[part];
    if (knob == 0) { dk808_set_level(&db_engA.k, i, value / 100.0); dk808_set_level(&db_engB.k, i, value / 100.0); }
    else if (knob <= 3u && *dk808_knob_name(i, knob - 1u)) {
        dk808_set_knob(&db_engA.k, i, knob - 1u, value / 100.0); dk808_set_knob(&db_engB.k, i, knob - 1u, value / 100.0);
    }
}
static void v_chokes(const db_sound_t *s)
{
    db_engA.k.hat_choke = db_engB.k.hat_choke = s->hat_choke;
    db_engA.k.conga_choke = db_engB.k.conga_choke = s->conga_choke;
    db_engA.k.tom_choke = db_engB.k.tom_choke = s->tom_choke;
}
static void v_apply(const db_sound_t *s)
{
    for (unsigned p = 0; p < DB_PARTS; ++p) for (unsigned k = 0; k < DB_KNOBS; ++k) v_param(p, k, s->v[p][k]);
    v_chokes(s);
}
const char *db_knob_name(unsigned part, unsigned knob)
{
    if (part >= DB_PARTS || knob >= DB_KNOBS) return "";
    return knob == 0 ? "LEVEL" : dk808_knob_name(db_part_dk[part], knob - 1u);
}

/* ---- Befehle (wie el_queue.h) */
int db_post(uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value)
{
    uint32_t h = db.q_head;
    if (h - db.q_tail >= DB_QUEUE) return -1;
    db.q[h & (DB_QUEUE - 1u)] = (db_cmd_t){type, a, b, c, value};
    __sync_synchronize();
    db.q_head = h + 1u;
    return 0;
}
int db_post_pattern(unsigned slot, const db_pattern_t *p, int keep_cond)
{
    if (slot >= DB_PATTERNS || !p) return -1;
    if (db.staging_posted != db.staging_taken) return -2;
    db.staging = *p;
    __sync_synchronize();
    if (db_post(DB_LOAD, (uint8_t)slot, 0, keep_cond ? 1 : 0, 0)) return -1;
    ++db.staging_posted;
    return 0;
}

/* ---- Bitebenen (Bedingung, Wahrscheinlichkeit, Velocity je Step) */
static void set_planes(uint32_t (*c)[DB_PARTS], unsigned planes, unsigned part, unsigned step, unsigned v)
{
    for (unsigned k = 0; k < planes; ++k) {
        uint32_t *w = &c[k][part];
        *w = (v >> k) & 1u ? *w | 1u << step : *w & ~(1u << step);
    }
}
static void clear_locks(unsigned slot, unsigned part, unsigned step)
{
    set_planes(db.s.cond[slot], 3, part, step, 0);
    set_planes(db.s.prob[slot], 4, part, step, 0);
    set_planes(db.s.vol[slot], 3, part, step, 0);
}
static const uint8_t vol_q7[8] DSPD = {0, 19, 36, 51, 66, 84, 106, 127};
static float vol_velocity(unsigned level) { return level && level < 8 ? vol_q7[level] / 127.0f : 0.0f; }
int db_vol_percent(unsigned level) { return level && level < 8 ? (int)(vol_q7[level] * 100 + 63) / 127 : 0; }
static int cond_plays(unsigned cond, unsigned pass)
{
    static const uint8_t every[DB_CONDS] DSPD = {1, 2, 2, 4, 4, 4, 4}, which[DB_CONDS] DSPD = {0, 0, 1, 0, 1, 2, 3};
    return cond >= DB_CONDS || pass % every[cond] == which[cond];
}
static uint32_t rnd(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return *s = n ? n : 0x9e3779b9u;
}
static int prob_plays(unsigned prob, uint32_t r) { return !prob || prob >= DB_PROBS || (int)((r >> 8) % 100u) < (int)(10u - prob) * 10; }
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

/* ---- Fills (N12): der erste Takt des Grooves, zum Ende ein Lauf auf Snare oder Clap */
void db_generate_fill(const db_pattern_t *g, uint32_t seed, db_fill_t *out);
void db_generate_fill(const db_pattern_t *g, uint32_t seed, db_fill_t *out)
{
    static const uint16_t kick[4] DSPD = {0x0211u, 0x0049u, 0x0c49u, 0x1041u};
    static const uint16_t lead[4] DSPD = {0xf544u, 0xf555u, 0xd6a4u, 0xf0a0u};
    static const uint16_t second[4] DSPD = {0x0a00u, 0x0000u, 0x2004u, 0x0f00u};
    static const uint16_t accents[4] DSPD = {0x9100u, 0xc110u, 0x9220u, 0xa101u};
    uint32_t rng = seed ? seed : 0x2545f491u;
    memset(out, 0, sizeof *out);
    unsigned kind = rnd(&rng) % 4u;
    unsigned lv = kind == 1 ? P_CP : P_SD;
    unsigned sv = kind == 3 ? P_RS : (rnd(&rng) & 1u) ? P_CP : P_RS;
    if (sv == lv) sv = P_RS;
    out->hit[P_BD] = (uint16_t)(kick[kind] | 0x0001u);
    out->hit[lv] = lead[kind];
    out->hit[sv] |= second[kind];
    out->hit[P_CH] = (uint16_t)(g->hit[P_CH] & 0x0111u);
    out->hit[P_CY] = (uint16_t)(g->hit[P_CY] & 0x0001u);
    out->roll[lv] = (uint16_t)((rnd(&rng) & 1u) ? 0x4000u : 0x2000u);
    out->hit[lv] |= out->roll[lv];
    if (rnd(&rng) & 1u) out->hit[lv] &= (uint16_t)~(1u << (2u + rnd(&rng) % 6u));
    if (lv == P_CP) out->hit[P_SD] |= 0x8000u;
    out->accent = accents[kind];
    out->used = 1;
}

static uint32_t repeat_bits(uint32_t bits, unsigned old, unsigned len)
{
    for (unsigned s = old; s < len && s < 32u; ++s) bits = (bits & ~(1u << s)) | (((bits >> (s % old)) & 1u) << s);
    return bits;
}
static void apply_kit(unsigned index, int factory)
{
    if (index >= DB_KITS) return;
    if (factory) db.s.kit_mem_used &= (uint8_t)~(1u << index);
    if ((db.s.kit_mem_used >> index) & 1u) db.s.sound = db.s.kit_mem[index];
    else kit_factory(&db.s.sound, index);
    db.s.kit_index = (uint8_t)index;
    for (unsigned i = 0; i < DB_PATTERNS; ++i) db.s.pattern[i].kit = (uint8_t)index;
    v_apply(&db.s.sound);
}
static void enter(unsigned index)
{
    db.current = (uint8_t)index;
    db.pass = 0;
}
static int audible(unsigned part) { uint32_t bit = 1u << part; return !(db.mute & bit) && (!db.solo || (db.solo & bit)); }

static void hit(unsigned part, float velocity, int record)
{
    if (part >= DB_PARTS || velocity <= 0.0f) return;
    if (record != 2) v_trigger(part, velocity);
    if (!record || !db.recording || !db.playing) return;
    db_pattern_t *p = &db.s.pattern[db.current];
    unsigned step = db.pos, len = p->length ? p->length : 1u;
    if (db.since_step > 0.5f * db.step_frames) { step = (step + 1u) % len; db.suppress |= 1u << part; }
    p->hit[part] |= 1u << step;
    if (velocity > 0.9f) p->accent |= 1u << step;
    ++db.revision;
}
static void rewind(void)
{
    db.oh_short = 0.0f;
    db.fill_now = 0;
    db.bars = 0;
    db.playing = 0;
    db.pos = 0;
    db.roll_pending = 0;
    db.suppress = 0;
    db.pass = 0;
    if (db.chain_len >= 2) { db.chain_pos = 0; db.next = db.chain[0]; }
    if (db.next != db.current) enter(db.next);
}
static void step(uint32_t sixteenth, float step_frames)
{
    db_pattern_t *p = &db.s.pattern[db.current];
    unsigned pos;
    if (!db.playing) { pos = 0; db.playing = 1; db.pass = 0; }
    else {
        pos = db.pos + 1u;
        if (pos >= p->length) {
            pos = 0;
            ++db.pass;
            if (db.chain_len >= 2) {
                if (db.current == db.chain[db.chain_pos]) db.chain_pos = (uint8_t)((db.chain_pos + 1u) % db.chain_len);
                db.next = db.chain[db.chain_pos];
            }
            if (db.next != db.current) { enter(db.next); p = &db.s.pattern[db.current]; }
        }
    }
    db.pos = (uint8_t)pos;
    db.step_frames = step_frames;
    db.since_step = 0.0f;
    if (sixteenth % DB_FILL_STEPS == 0u) {
        if (sixteenth) ++db.bars;
        unsigned every = db.s.fill_auto;
        db.fill_now = db.fill_req || (every && (db.bars + 1u) % every == 0u);
        if (db.fill_req == 1) db.fill_req = 0;
        db_fill_t *f = &db.s.fill[db.current];
        if (db.fill_now && !f->used) db_generate_fill(p, db.random ^ db.bars, f);
    }
    uint32_t bit = 1u << pos, suppress = db.suppress, played = 0;
    db.suppress = 0;
    const db_fill_t *fill = db.fill_now ? &db.s.fill[db.current] : 0;
    uint32_t fbit = 1u << (sixteenth % DB_FILL_STEPS);
    float velocity = (fill ? (fill->accent & fbit) != 0 : (p->accent & bit) != 0) ? 1.0f : NORMAL;
    db.roll_pending = 0;
    /* B16: Open und Closed Hat auf einem Step bei Choke: die offene allein, kurz abgeschnitten */
    int both = db.s.sound.hat_choke && (fill ? (fill->hit[P_OH] & fill->hit[P_CH] & fbit) : (p->hit[P_OH] & p->hit[P_CH] & bit)) &&
               audible(P_OH) && audible(P_CH);
    if (both) { suppress |= 1u << P_CH; db.oh_short = OH_SHORT_MS * (SR / 1000.0f); }
    for (unsigned part = 0; part < DB_PARTS; ++part) {
        float v = velocity;
        if (fill) {
            if (!(fill->hit[part] & fbit) || !audible(part) || db.muted) continue;
        } else {
            if (!(p->hit[part] & bit) || !audible(part) || db.muted) continue;
            if (!cond_plays(db_cond(db.current, part, pos), db.pass)) continue;
            unsigned prob = db_prob(db.current, part, pos);
            if (prob && !prob_plays(prob, rnd(&db.random))) continue;
            unsigned level = db_vol(db.current, part, pos);
            if (level) v = vol_velocity(level);
        }
        if (!(suppress & (1u << part))) { v_trigger(part, v); played |= 1u << part; }
        if (fill ? (fill->roll[part] & fbit) != 0 : (p->roll[part] & bit) != 0) {
            db.roll_pending |= 1u << part;
            db.roll_velocity[part] = v * (ROLL_Q7 / 127.0f);
        }
    }
    db.hits_now = played;
    db.roll_countdown = 0.5f * step_frames;
}

/* REPEAT (Punch-in, dr_step_at in FM1-Electro): jedes Sechzehntel spielt den gewählten Step des Takts noch einmal,
 * die Laufmarke geht darunter weiter */
static void step_at(uint32_t sixteenth, float step_frames, unsigned target)
{
    unsigned len = db.s.pattern[db.current].length ? db.s.pattern[db.current].length : 1u;
    unsigned real = !db.playing ? 0u : db.pos + 1u < len ? db.pos + 1u : 0u;
    uint8_t next = db.next, pass = (uint8_t)(db.pass + (db.playing && real == 0)), chain = db.chain_len;
    db.next = db.current;
    db.playing = 1;
    db.pos = (uint8_t)((target % len + len - 1u) % len);
    db.pass = (uint8_t)(target % len == 0 ? pass - 1u : pass);
    db.chain_len = 0;                           /* (ein wiederholter Step bewegt die Kette nicht) */
    step(sixteenth, step_frames);
    db.chain_len = chain;
    db.next = next;
    db.pos = (uint8_t)real;
    db.pass = pass;
}
static void apply(const db_cmd_t *c)
{
    db_pattern_t *p = &db.s.pattern[db.current];
    unsigned part = c->a, st = c->b, cur = db.current;
    switch (c->type) {
    case DB_HIT: hit(part, c->b / 127.0f, c->c); return;
    case DB_TOGGLE:
        if (part < DB_PARTS && st < DB_STEPS) {
            p->hit[part] ^= 1u << st;
            if (!(p->hit[part] & (1u << st))) { p->roll[part] &= ~(1u << st); clear_locks(cur, part, st); }
        }
        break;
    case DB_ROLL:
        if (part < DB_PARTS && st < DB_STEPS) { p->roll[part] ^= 1u << st; if (p->roll[part] & (1u << st)) p->hit[part] |= 1u << st; }
        break;
    case DB_ACCENT: if (st < DB_STEPS) p->accent ^= 1u << st; break;
    case DB_CLEAR_PART:
        if (part < DB_PARTS) {
            p->hit[part] = p->roll[part] = 0;
            for (unsigned k = 0; k < 3; ++k) { db.s.cond[cur][k][part] = 0; db.s.vol[cur][k][part] = 0; }
            for (unsigned k = 0; k < 4; ++k) db.s.prob[cur][k][part] = 0;
        } else {
            for (unsigned i = 0; i < DB_PARTS; ++i) p->hit[i] = p->roll[i] = 0;
            memset(db.s.cond[cur], 0, sizeof db.s.cond[cur]);
            memset(db.s.prob[cur], 0, sizeof db.s.prob[cur]);
            memset(db.s.vol[cur], 0, sizeof db.s.vol[cur]);
            p->accent = 0;
        }
        break;
    case DB_LENGTH: {
        unsigned old = p->length ? p->length : 1u, len = (unsigned)clampi((int)st, 1, DB_STEPS);
        uint32_t fresh = 0;
        for (unsigned s = old; s < len; ++s) fresh |= 1u << s;
        int empty = (p->accent & fresh) == 0;
        for (unsigned i = 0; i < DB_PARTS && empty; ++i) empty = !((p->hit[i] | p->roll[i]) & fresh);
        if (len > old && empty) {
            p->accent = repeat_bits(p->accent, old, len);
            for (unsigned i = 0; i < DB_PARTS; ++i) {
                p->hit[i] = repeat_bits(p->hit[i], old, len); p->roll[i] = repeat_bits(p->roll[i], old, len);
                for (unsigned k = 0; k < 3; ++k) { db.s.cond[cur][k][i] = repeat_bits(db.s.cond[cur][k][i], old, len); db.s.vol[cur][k][i] = repeat_bits(db.s.vol[cur][k][i], old, len); }
                for (unsigned k = 0; k < 4; ++k) db.s.prob[cur][k][i] = repeat_bits(db.s.prob[cur][k][i], old, len);
            }
        }
        p->length = (uint8_t)len;
        if (db.pos >= p->length) db.pos = (uint8_t)(db.pos % p->length);
        break;
    }
    case DB_SWING: p->swing = (uint8_t)clampi((int)st, 50, 75); break;
    case DB_SELECT:
        if (part < DB_PATTERNS) {
            db.next = (uint8_t)part;
            if (st || !db.playing) enter(part);
            if (db.pos >= db.s.pattern[db.current].length) db.pos = 0;
        }
        break;
    case DB_COPY:
        if (part < DB_PATTERNS && st < DB_PATTERNS) {
            db.s.pattern[st] = db.s.pattern[part];
            db.s.fill[st] = db.s.fill[part];
            memcpy(db.s.cond[st], db.s.cond[part], sizeof db.s.cond[st]);
            memcpy(db.s.prob[st], db.s.prob[part], sizeof db.s.prob[st]);
            memcpy(db.s.vol[st], db.s.vol[part], sizeof db.s.vol[st]);
        }
        break;
    case DB_LOAD:
        if (part < DB_PATTERNS) {
            unsigned kit = db.staging.kit;
            db.s.pattern[part] = db.staging;
            if (part == db.current && kit < DB_KITS && kit != db.s.kit_index) apply_kit(kit, 0);
            db.s.pattern[part].kit = db.s.kit_index;
            if (!c->c || !db.s.fill[part].used) db_generate_fill(&db.s.pattern[part], db.random ^ 0x9e3779b9u, &db.s.fill[part]);
            if (!c->c) { memset(db.s.cond[part], 0, sizeof db.s.cond[part]); memset(db.s.prob[part], 0, sizeof db.s.prob[part]); memset(db.s.vol[part], 0, sizeof db.s.vol[part]); }
        }
        __sync_synchronize();
        ++db.staging_taken;
        break;
    case DB_MUTE: if (part < DB_PARTS) db.mute = st ? db.mute | (1u << part) : db.mute & ~(1u << part); break;
    case DB_SOLO: if (part < DB_PARTS) db.solo = st ? db.solo | (1u << part) : db.solo & ~(1u << part); break;
    case DB_PARAM:
        if (part < DB_PARTS && st < DB_KNOBS) {
            db.s.sound.v[part][st] = (uint8_t)clampi(c->value, 0, 100);
            v_param(part, st, db.s.sound.v[part][st]);
            db.s.kit_mem[db.s.kit_index & 3u] = db.s.sound;
            db.s.kit_mem_used |= (uint8_t)(1u << (db.s.kit_index & 3u));
        }
        break;
    case DB_CHOKE:
        if (part == 0) db.s.sound.hat_choke = st ? 1 : 0;
        else if (part == 1) db.s.sound.conga_choke = st ? 1 : 0;
        else db.s.sound.tom_choke = st ? 1 : 0;
        v_chokes(&db.s.sound);
        db.s.kit_mem[db.s.kit_index & 3u] = db.s.sound;
        db.s.kit_mem_used |= (uint8_t)(1u << (db.s.kit_index & 3u));
        break;
    case DB_KIT: apply_kit(part, st == 1); break;
    case DB_LEVEL: db.s.level_value = (uint8_t)clampi(c->value, 0, 127); db.level = db.s.level_value / 127.0f; break;
    case DB_RECORD: db.recording = st ? 1 : 0; break;
    case DB_PANIC: v_silence(); db.roll_pending = 0; db.suppress = 0; break;
    case DB_COND: if (part < DB_PARTS && st < DB_STEPS) set_planes(db.s.cond[cur], 3, part, st, (unsigned)clampi(c->value, 0, DB_CONDS - 1)); break;
    case DB_PROB: if (part < DB_PARTS && st < DB_STEPS) set_planes(db.s.prob[cur], 4, part, st, (unsigned)clampi(c->value, 0, DB_PROBS - 1)); break;
    case DB_VOL: if (part < DB_PARTS && st < DB_STEPS) set_planes(db.s.vol[cur], 3, part, st, (unsigned)clampi(c->value, 0, 7)); break;
    case DB_FILL: db.fill_req = (uint8_t)(st > 2 ? 0 : st); break;
    case DB_FILL_AUTO: db.s.fill_auto = (uint8_t)st; break;
    case DB_FILL_EDIT: {
        db_fill_t *f = &db.s.fill[cur];
        db.fill_edit = st ? 1 : 0;
        if (st && !f->used) {
            memset(f, 0, sizeof *f);
            uint16_t keep = (uint16_t)(p->length < DB_FILL_STEPS ? (1u << p->length) - 1u : 0xffffu);
            for (unsigned i = 0; i < DB_PARTS; ++i) { f->hit[i] = (uint16_t)p->hit[i] & keep; f->roll[i] = (uint16_t)p->roll[i] & keep; }
            f->accent = (uint16_t)p->accent & keep;
            f->used = 1;
        }
        break;
    }
    case DB_FILL_TOGGLE:
        if (part < DB_PARTS && st < DB_FILL_STEPS) {
            db_fill_t *f = &db.s.fill[cur];
            f->hit[part] ^= (uint16_t)(1u << st);
            if (!(f->hit[part] & (1u << st))) f->roll[part] &= (uint16_t)~(1u << st);
            f->used = 1;
        }
        break;
    case DB_FILL_ACCENT: if (st < DB_FILL_STEPS) { db.s.fill[cur].accent ^= (uint16_t)(1u << st); db.s.fill[cur].used = 1; } break;
    case DB_FILL_ROLL:
        if (part < DB_PARTS && st < DB_FILL_STEPS) {
            db_fill_t *f = &db.s.fill[cur];
            f->roll[part] ^= (uint16_t)(1u << st);
            if (f->roll[part] & (1u << st)) f->hit[part] |= (uint16_t)(1u << st);
            f->used = 1;
        }
        break;
    case DB_CHAIN_SET:
        db.chain_len = (uint8_t)(st >= 2 && st <= DB_CHAIN ? st : 0);
        for (unsigned i = 0; i < DB_CHAIN; ++i) db.chain[i] = (uint8_t)(((unsigned)c->value >> (2u * i)) & 3u);
        db.chain_pos = 0;
        if (db.chain_len) db.next = db.chain[0];
        if (db.chain_len && !db.playing && db.next != db.current) enter(db.next);
        break;
    case DB_AREA_MUTE: db.muted = st ? 1 : 0; break;
    case DB_REPEAT: db.repeat_on = st ? 1 : 0; db.repeat_step = (uint8_t)(c->c & 15u); return;
    default: return;
    }
    ++db.revision;
}

/* ---- Audio-Takt: Befehle, Steps (mit Swing), Rolls, das kurze Open Hat */
void db_block(int running, float ticks, float bpm, unsigned frames)
{
    while (db.q_tail != db.q_head) {
        uint32_t t = db.q_tail;
        db_cmd_t c = db.q[t & (DB_QUEUE - 1u)];
        __sync_synchronize();
        db.q_tail = t + 1u;
        apply(&c);
    }
    if (running && !db.was_running) { rewind(); db.step_abs = 0; }
    if (!running && db.was_running) rewind();
    db.was_running = (uint8_t)running;
    db.fired = 0;
    if (running) {
        const db_pattern_t *p = &db.s.pattern[db.current];
        float share = (float)clampi(p->swing, 50, 75) * 0.01f, b = bpm < 20.0f ? 20.0f : bpm;
        float pair = 2.0f * SR * 60.0f / (4.0f * b);
        for (int n = 0; n < 2; ++n) {   /* (höchstens zwei Steps je Block) */
            uint32_t k = db.step_abs;
            float due = 6.0f * (float)k + ((k & 1u) ? 12.0f * share - 6.0f : 0.0f);
            if (ticks + 0.001f < due) break;
            if (db.repeat_on) step_at(k, pair * ((k & 1u) ? 1.0f - share : share), db.repeat_step);
            else step(k, pair * ((k & 1u) ? 1.0f - share : share));
            db.fired = 1; db.fired_step = k;
            db.step_abs = k + 1u;
        }
    }
    db.since_step += (float)frames;
    if (db.oh_short > 0.0f) {
        db.oh_short -= (float)frames;
        if (db.oh_short <= 0.0f) { db.oh_short = 0.0f; v_choke(P_OH); }
    }
    if (db.roll_pending) {
        db.roll_countdown -= (float)frames;
        if (db.roll_countdown <= 0.0f) {
            for (unsigned part = 0; part < DB_PARTS; ++part)
                if (db.roll_pending & (1u << part)) v_trigger(part, db.roll_velocity[part]);
            db.roll_pending = 0;
        }
    }
}
void db_note(int note, int velocity)
{
    int part = db_part_of_note(note);
    if (part >= 0 && velocity > 0) hit((unsigned)part, velocity / 127.0f, 1);
}
void db_render_k0(float *l, float *r, float *dist, int n)
{
    dk808_bs_render(&db_engA, l, r, dist, n);
    for (int i = 0; i < n; ++i) { l[i] *= db.level; r[i] *= db.level; if (dist) dist[i] *= db.level; }
}
void db_render_k1(float *l, float *r, float *dist, int n)
{
    dk808_bs_render(&db_engB, l, r, dist, n);
    for (int i = 0; i < n; ++i) { l[i] *= db.level; r[i] *= db.level; if (dist) dist[i] *= db.level; }
}
int db_voices(void) { return dk808_bs_voices(&db_engA) + dk808_bs_voices(&db_engB); }
unsigned db_part_note(unsigned part) { return part < DB_PARTS ? part_notes[part] : 0; }
int db_part_of_note(int note)
{
    for (unsigned p = 0; p < DB_PARTS; ++p) if (part_notes[p] == note) return (int)p;
    int i = dk808_note_instrument(note);   /* sonst die Drum-Map des DrumKern */
    if (i >= 0) for (unsigned p = 0; p < DB_PARTS; ++p) if (db_part_dk[p] == (unsigned)i) return (int)p;
    return -1;
}

/* ---- Generator (drum_factory.c): Stile mit Alternativen je Instrument, daraus jedes Mal ein neuer Groove */
enum { K808, KELECTRO, KMIAMI, KKRAFT };
enum { DR_HARMONY_ROTHER, DR_HARMONY_KRAFT, DR_HARMONY_ELECTRO, DR_HARMONY_EGYPT };
enum { STYLE_ROWS = 13 };
typedef struct {
    const char *name;
    uint16_t bpm_x10;
    uint8_t swing, kit, length;
    uint8_t rhythm_style, strings_style, bass_style, harmony;
    const char *row[STYLE_ROWS];
} db_style_t;
#define DB_STYLES_N DB_STYLES
static const db_style_t styles[DB_STYLES] = {
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
    /* RECKLESS: measured on DJ Reckless' Reckless & der Bassklan (stems
     * separated with Demucs): German Miami bass at ~124 BPM, straight; kick
     * on 1 and 7 with pushes on 11-15, clap on the backbeat with snare
     * fills, closed 16ths that leave the backbeat free, 808 bass mostly on
     * the root, octave and fifth stabs. */
    {"RECKLESS", 1245, 50, KMIAMI, 32, 2, 0, 4, DR_HARMONY_EGYPT, {
        "x.....x...x...x.x.....x...x..xx.|x...x.x...x.x.x.x...x.x...x.xx..|x.....x.x.......x.....x...x.x.x.|"
        "x......x..xx..x.x.....xx.....x..|x.x...x...x.xxx.x.....x...x...x.|x.......x...x...x.......x...x.x.",
        "....x.......x.......x.......x...|....x.......x.......x.....x.x.xx|....x.x.....x.......x.x.....x..x", 0, 0, 0,
        "....x.......x.......x.......x...", "...x.....x.....x...x.....x......|", 0, "|..............x...............x.",
        "xxxx..xxxxxx..xxxxxx..xxxxxx..xx|xxxx.xxxxxxx.xxxxxxx.xxxxxxx.xxx|x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", 0,
        "|...x.......x.......x.......x....", "x...x.......x...x...x.......x..."}},
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
    {"EGYPT", 1260, 50, KMIAMI, 32, 2, 0, 4, DR_HARMONY_EGYPT, {
        "x.....x...x.....x.....x...x..x..|x...x.x...x.....x...x.x...x.....|x...x.x...x..xx.x...x.x...x..x..|"
        "x.....x.x..x....x.....x.x..x....|x...x.x.x...x...x...x.x.x...x...|x..x..x...x..x..x..x..x...x..x..",
        "....x.......x.......x.......xxxx",
        "............................x...|", "..............................x.|",
        "...x......x..x.....x......x.....|",
        "....x.......x.......x.......xxxx", "..x...x.x.....x...x...x.x.......|", 0,
        "|||x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.",
        "xxxx..xxxxxx..xxxxxx..xxxxxx..xx|xxxxxxxxxxxxxxxxxxxxxxxxxxxx....|..xx....xx.....x..xx....xx.....x",
        "|..............................x.", "..x...x...x...x...x...x...x...x.|..xx..x...xx..x...xx..x...xx.x.x|x..x..x.x..x..x.x..x..x.x..x..x.", "x...x.x...x.x...x...x.x...x.xxxx"}},
    {"FREAKY", 1280, 50, KELECTRO, 32, 2, 2, 4, DR_HARMONY_EGYPT, {
        "x.....x...x..x..x.....x...x.x..x", "....x.......x.......x.......x.xx", 0, 0, 0,
        "....x.......x.......x.......xxxx", "x..x..x...x.....x..x..x...x.....|", 0, "..x...x...x...x...x...x...x.....",
        "xx.xxx.xxx.xxx.xxx.xxx.xxx.xxxxx", "|..............x...............x.", "..x...x...x...x...x...x...x...xx|.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x|", "x.x.x.x.x.x.x.x.x.x.x.x.x.x.xxxx"}},
    {"LOVE", 1250, 53, KMIAMI, 32, 2, 1, 0, DR_HARMONY_KRAFT, {
        "x..x..x.x.....x.x..x..x.x..x..x.", "....x.......x.......x.......x...",
        "............................x...|", "..............................x.|", 0,
        "....x.......x..x....x.......x...", 0, "x...............................|", "..............x...............x.",
        "x.x.x.x.x.x.x...x.x.x.x.x.x.x...", 0, "|..x...x...x...x...x...x...x...x.|", "x.......x.......x.......x......."}},
};
const char *const db_style_names[DB_STYLES] = {"ELECTRO", "BRONX 82", "NUMBERS", "TRANS EUR", "TOUR", "COMPUTER", "ROTHER",
    "MIAMI", "RECKLESS", "BOOM BASS", "DROWNED", "ROBOT", "PERFECT", "EGYPT", "FREAKY", "LOVE"};
/* die Zeilen der Stile (BD SD LT HT RS CP CB CY OH CH ZP PU): hier spielen auch die Toms wieder; ZP = Laser Gun, PU = Claves */
static const uint8_t style_part[STYLE_ROWS - 1] = {P_BD, P_SD, P_LT, P_HT, P_RS, P_CP, P_CB, P_CY, P_OH, P_CH, P_LG, P_CL};

static uint32_t pick_row(const char *row, uint32_t *roll, uint32_t *rng)
{
    if (!row) return 0;
    unsigned count = 1;
    for (const char *c = row; *c; ++c) count += *c == '|';
    unsigned want = rnd(rng) % count, index = 0, st = 0;
    uint32_t bits = 0;
    for (const char *c = row; *c; ++c) {
        if (*c == '|') { ++index; st = 0; continue; }
        if (index != want) continue;
        if (st < DB_STEPS && (*c == 'x' || *c == 'r')) bits |= 1u << st;
        if (st < DB_STEPS && *c == 'r' && roll) *roll |= 1u << st;
        ++st;
    }
    return bits;
}
void db_empty_pattern(db_pattern_t *out, const char *name)
{
    memset(out, 0, sizeof *out);
    for (unsigned i = 0; i < DB_NAME && name[i]; ++i) out->name[i] = name[i];
    out->length = 16;
    out->swing = 50;
}
static int anchor(unsigned part, unsigned st)
{
    if (part == P_BD) return st % 16u == 0;
    if (part == P_SD || part == P_CP) return st % 8u == 4;
    return 0;
}
void db_vary(db_pattern_t *p, unsigned amount, uint32_t seed)
{
    uint32_t rng = seed ? seed : 0x1234567u;
    unsigned chance = (amount > 127 ? 127 : amount) * 3u;
    unsigned length = p->length ? p->length : 16;
    uint32_t mask = length >= 32 ? 0xffffffffu : (1u << length) - 1u;
    for (unsigned part = 0; part < DB_PARTS; ++part) {
        uint32_t hits = p->hit[part], out = 0;
        if (!hits) continue;
        for (unsigned s = 0; s < length; ++s) {
            if (!((hits >> s) & 1u)) continue;
            if (anchor(part, s) || rnd(&rng) % 1000u >= chance) { out |= 1u << s; continue; }
            unsigned r = rnd(&rng) % 3u;
            if (r == 0) continue;
            unsigned to = r == 1 ? (s + 1u) % length : (s + length - 1u) % length;
            if ((hits | out) & (1u << to)) out |= 1u << s;
            else { out |= 1u << to; if ((p->roll[part] >> s) & 1u) p->roll[part] = (p->roll[part] & ~(1u << s)) | 1u << to; }
        }
        if (part != P_CL && rnd(&rng) % 1000u < chance) {
            unsigned s = rnd(&rng) % length;
            if (part == P_BD) s |= 1u;
            if (s < length && !((out >> s) & 1u) && !(part == P_BD && (out & ((s ? 1u << (s - 1u) : 0u) | 1u << ((s + 1u) % length)))))
                out |= 1u << s;
        }
        p->hit[part] = out & mask;
        p->roll[part] &= p->hit[part];
    }
    if (rnd(&rng) % 1000u < chance) p->accent ^= 1u << (rnd(&rng) % length);
    p->accent &= mask;
    p->hit[P_CH] &= ~p->hit[P_OH];
}
void db_generate(db_pattern_t *out, unsigned style, unsigned variation, uint32_t seed, const db_pattern_t *keep)
{
    if (style >= DB_STYLES) style = 0;
    if (variation > 127) variation = 127;
    const db_style_t *s = &styles[style];
    uint32_t rng = seed ? seed : 0x1234567u;
    db_pattern_t p;
    db_empty_pattern(&p, s->name);
    p.length = s->length;
    p.swing = s->swing;
    p.kit = s->kit;
    p.style = (uint8_t)style;
    for (unsigned row = 0; row < STYLE_ROWS - 1u; ++row) {
        uint32_t roll = 0, h = pick_row(s->row[row], &roll, &rng);
        p.hit[style_part[row]] = h;
        p.roll[style_part[row]] = roll;
    }
    p.accent = pick_row(s->row[STYLE_ROWS - 1u], 0, &rng);
    uint32_t mask = p.length >= 32 ? 0xffffffffu : (1u << p.length) - 1u;
    unsigned chance = variation * 4u;
    if (rnd(&rng) % 1000u < chance) {
        unsigned st = 1u + 2u * (rnd(&rng) % (p.length / 2u));
        if (st < p.length && !(p.hit[P_BD] & (5u << (st - 1u)))) p.hit[P_BD] |= 1u << st;
    }
    if (rnd(&rng) % 1000u < chance) { unsigned st = rnd(&rng) % p.length; if (st & 1u) p.hit[P_CH] ^= 1u << st; }
    if (!p.hit[P_OH] && rnd(&rng) % 1000u < chance / 2u) p.hit[P_OH] |= 1u << (p.length - 2u);
    for (unsigned part = 0; part < DB_PARTS; ++part) { p.hit[part] &= mask; p.roll[part] &= mask; }
    p.accent &= mask;
    p.hit[P_CH] &= ~p.hit[P_OH];
    if (keep) { p.hit[P_BD] = keep->hit[P_BD]; p.roll[P_BD] = keep->roll[P_BD]; p.length = keep->length; }
    *out = p;
}

/* ---- Start */
void db_init(void)
{
    memset(&db, 0, sizeof db);
    db.random = 0x2545f491u;
    db_generate(&db.s.pattern[0], 6 /* ROTHER */, 0, 0x2545f491u, 0);
    for (unsigned i = 1; i < DB_PATTERNS; ++i) {
        db_empty_pattern(&db.s.pattern[i], db.s.pattern[0].name);
        db.s.pattern[i].kit = db.s.pattern[0].kit;
        db.s.pattern[i].swing = db.s.pattern[0].swing;
        db.s.pattern[i].style = db.s.pattern[0].style;
    }
    db.s.level_value = 100;
    db.s.kit_index = db.s.pattern[0].kit;
    kit_factory(&db.s.sound, db.s.kit_index);
    db_generate_fill(&db.s.pattern[0], 0x2545f491u, &db.s.fill[0]);
}
void db_restore(void)
{
    uint32_t keep = db.random;
    dk808_bs_init(&db_engA, SR);
    dk808_bs_init(&db_engB, SR);
    dk808_set_run(&db_engA.k, DK808_MASK_ALL & ~(uint32_t)(DK_K1_MASK));
    dk808_set_run(&db_engB.k, (uint32_t)(DK_K1_MASK));
    for (unsigned i = 0; i < DB_PATTERNS; ++i) {   /* (gespeicherte Werte prüfen: Länge 1..32, Swing) */
        db_pattern_t *p = &db.s.pattern[i];
        if (!p->length || p->length > DB_STEPS) p->length = 16;
        if (p->swing < 50 || p->swing > 75) p->swing = 50;
        p->name[DB_NAME] = 0;
    }
    if (db.s.kit_index >= DB_KITS) db.s.kit_index = 0;
    if (db.s.level_value > 127) db.s.level_value = 100;
    for (unsigned p = 0; p < DB_PARTS; ++p) for (unsigned k = 0; k < DB_KNOBS; ++k) if (db.s.sound.v[p][k] > 100) db.s.sound.v[p][k] = 100;
    db.level = db.s.level_value / 127.0f;
    db.current = db.next = 0;
    db.random = keep ? keep : 0x2545f491u;
    v_apply(&db.s.sound);
    dk808_bs_set_raw(&db_engA, DK_BS_DIST, 0);
    dk808_bs_set_raw(&db_engB, DK_BS_DIST, 0);
}
