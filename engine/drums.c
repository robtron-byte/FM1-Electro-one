/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "drums.h"
#include <string.h>

#ifndef DR_OH_SHORT_MS
#define DR_OH_SHORT_MS 90.0f   /* B16: how long the open hat sounds when the closed one is on its step */
#endif

const char *const dr_part_names[DR_PARTS] = {"BD", "SD", "RS", "CP", "CB", "CY", "OH", "CH", "ZP", "PU"};
const char *const dr_kit_names[DR_KITS] = {"TR-808", "ELECTRO", "MIAMI", "KRAFT", "PO-12"};

enum { DR_NORMAL_VELOCITY_Q7 = 84, DR_ROLL_SCALE_Q7 = 96 };
#define DR_NORMAL (DR_NORMAL_VELOCITY_Q7 / 127.0f)

/* Kit presets: part, parameter index (as listed by tr808_param), value,
 * applied on top of the stock circuit. TR-808 is the untouched machine. */
typedef struct { uint8_t part, param, value; } dr_kit_entry_t;
enum { DR_KIT_ENTRIES = 16 };
static const dr_kit_entry_t kits[DR_KITS][DR_KIT_ENTRIES] EL_DSP_DATA = {
    /* TR-808: stock. The clap stays dry in every kit (docs/adr/0013). */
    {{255, 0, 0}},
    /* ELECTRO (Anthony Rother style): longer kick with a little drive,
     * snappy snare in the non-linear reverb, tight hats. */
    {{D8_BD, 2, 82}, {D8_BD, 3, 42}, {D8_BD, 7, 14}, {D8_SD, 2, 72}, {D8_SD, 3, 88}, {D8_SD, 6, 44},
     {D8_CP, 0, 108}, {D8_CH, 2, 52}, {D8_OH, 1, 34}, {D8_CB, 0, 84}, {D8_RS, 0, 92},
     {D8_RS, 5, 30}, {D8_KIT, 1, 110}, {255, 0, 0}},
    /* MIAMI: booming long kick, a bit lower and driven, darker snare. */
    {{D8_BD, 1, 58}, {D8_BD, 2, 116}, {D8_BD, 0, 112}, {D8_BD, 7, 28}, {D8_BD, 3, 30},
     {D8_SD, 2, 40}, {D8_SD, 3, 76}, {D8_CH, 2, 44}, {255, 0, 0}},
    /* KRAFT: short clicky kick, high tight snare, claves, darker cymbal. */
    {{D8_BD, 2, 34}, {D8_BD, 3, 86}, {D8_SD, 1, 74}, {D8_SD, 2, 84}, {D8_SD, 3, 52}, {D8_SD, 6, 52},
     {D8_RS, 3, 1}, {D8_CH, 2, 40}, {D8_CY, 1, 34}, {D8_CB, 1, 70}, {D8_KIT, 2, 1}, {255, 0, 0}},
    /* PO-12: the pocket operator's sounds (resynthesised, engine/po12.c),
     * knobs at the centre = as recorded. */
    {{D8_KIT, 3, 1}, {D8_OH, 1, 64}, {D8_ZP, 4, 64}, {255, 0, 0}},
};

/* ZP on 67 (high agogo), PU on 75 (claves) */
static const uint8_t part_notes[DR_PARTS] = {36, 38, 37, 39, 56, 49, 46, 42, 67, 75};

unsigned dr_part_note(unsigned part) { return part < DR_PARTS ? part_notes[part] : 0; }

int dr_note_part(unsigned note)
{
    for (unsigned p = 0; p < DR_PARTS; ++p)
        if (part_notes[p] == note) return (int)p;
    /* Common alternates: 35 kick, 40 snare, 43/45/47/50 toms, 44 pedal hat, 51 ride. */
    switch (note) {
    case 35: return D8_BD;
    case 40: return D8_SD;
    /* (the tom notes 43 45 47 50 have no part any more) */
    case 44: return D8_CH;
    case 51: case 57: return D8_CY;
    default: return -1;
    }
}

/* The factory kits' sounds, worked out once before audio (dr_init): a kit
 * change in the audio interrupt only copies (the parameter tables are read
 * from the flash, slow there; MAX 66 -> 80 % on a kit change, strict
 * emulator). */
static tr808_sound_t kit_sounds[DR_KITS];
static uint8_t kit_sounds_ok;
static void kit_sounds_init(void)
{
    if (kit_sounds_ok) return;
    for (unsigned k = 0; k < DR_KITS; ++k) {
        tr808_sound_default(&kit_sounds[k]);
        for (unsigned i = 0; i < DR_KIT_ENTRIES && kits[k][i].part != 255; ++i)
            tr808_sound_set(&kit_sounds[k], kits[k][i].part, kits[k][i].param, kits[k][i].value);
    }
    kit_sounds_ok = 1;
}

EL_DSP_CODE void dr_kit_sound(tr808_sound_t *s, unsigned index)
{
    const uint8_t *from = (const uint8_t *)&kit_sounds[index < DR_KITS ? index : 0];
    uint8_t *to = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; ++i) to[i] = from[i];   /* (no library memcpy: it is in the flash) */
}

EL_DSP_CODE void dr_kit_recall(const dr_machine_t *m, tr808_sound_t *s, unsigned index)
{
    if (index < DR_KITS && (m->kit_mem_used >> index) & 1u) {
        const uint8_t *from = (const uint8_t *)&m->kit_mem[index];
        uint8_t *to = (uint8_t *)s;
        for (unsigned i = 0; i < sizeof *s; ++i) to[i] = from[i];
    } else dr_kit_sound(s, index);
}

/* The playing slot takes kit index as last set (factory: its factory sound, the settings forgotten). */
static EL_DSP_CODE void apply_kit(dr_machine_t *m, unsigned index, int factory)
{
    if (index >= DR_KITS) return;
    if (factory) m->kit_mem_used &= (uint8_t)~(1u << index);
    dr_kit_recall(m, &m->sound[m->current], index);
    tr808_sound_apply(&m->kit, &m->sound[m->current]);
    m->kit_index = (uint8_t)index;
}

void dr_init(dr_machine_t *m)
{
    kit_sounds_init();
    memset(m, 0, sizeof *m);
    el_queue_init(&m->queue);
    /* Slot A a groove to start from, B-D empty in its kit and swing. */
    dr_generate(&m->pattern[0], 6 /* ROTHER */, 0, 0x2545f491u, NULL);
    for (unsigned i = 1; i < DR_PATTERNS; ++i) {
        dr_empty_pattern(&m->pattern[i], m->pattern[0].name);
        m->pattern[i].kit = m->pattern[0].kit;
        m->pattern[i].swing = m->pattern[0].swing;
        m->pattern[i].style = m->pattern[0].style;
    }
    m->level_value = 100;
    m->level = 100 / 127.0f;
    m->random = 0x2545f491u;
    tr808_init(&m->kit);
    apply_kit(m, m->pattern[0].kit, 0);
    dr_generate_fill(&m->pattern[0], 0x2545f491u, &m->fill[0]);
    for (unsigned i = 1; i < DR_PATTERNS; ++i) m->sound[i] = m->sound[0];
}

int dr_post(dr_machine_t *m, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value)
{
    el_cmd_t cmd = {type, a, b, c, value};
    return el_queue_push(&m->queue, cmd);
}

int dr_post_pattern(dr_machine_t *m, unsigned slot, const dr_pattern_t *p, int keep_cond)
{
    if (slot >= DR_PATTERNS || !p) return -1;
    if (m->staging_posted != m->staging_taken) return -2;
    m->staging = *p;
    EL_FENCE();
    if (dr_post(m, DR_CMD_LOAD, (uint8_t)slot, 0, keep_cond ? 1 : 0, 0)) return -1;
    ++m->staging_posted;
    return 0;
}

/* A pattern brings its sound along: its kit (a Miami groove gets the boom
 * kick) with the knobs as they were set in it (docs/adr/0024). */
static EL_DSP_CODE void set_cond(dr_machine_t *m, unsigned slot, unsigned part, unsigned step, unsigned cond)
{
    for (unsigned k = 0; k < 3; ++k) {
        uint32_t *w = &m->cond[slot][k][part];
        *w = (cond >> k) & 1u ? *w | 1u << step : *w & ~(1u << step);
    }
}

static EL_DSP_CODE void set_prob(dr_machine_t *m, unsigned slot, unsigned part, unsigned step, unsigned prob)
{
    for (unsigned k = 0; k < 4; ++k) {
        uint32_t *w = &m->prob[slot][k][part];
        *w = (prob >> k) & 1u ? *w | 1u << step : *w & ~(1u << step);
    }
}

static EL_DSP_CODE void set_pitch(dr_machine_t *m, unsigned slot, unsigned part, unsigned step, int semis)
{
    int s = dr_pitch_slot(part);
    if (s < 0) return;
    unsigned v = (unsigned)el_clampi(semis, -12, 12) & 31u;
    for (unsigned k = 0; k < 5; ++k) {
        uint32_t *w = &m->pitch[slot][k][s];
        *w = (v >> k) & 1u ? *w | 1u << step : *w & ~(1u << step);
    }
}

static EL_DSP_CODE void set_vol(dr_machine_t *m, unsigned slot, unsigned part, unsigned step, unsigned level)
{
    for (unsigned k = 0; k < 3; ++k) {
        uint32_t *w = &m->vol[slot][k][part];
        *w = (level >> k) & 1u ? *w | 1u << step : *w & ~(1u << step);
    }
}

/* Every lock of a step: it forgets them when it goes off. */
static EL_DSP_CODE void clear_locks(dr_machine_t *m, unsigned slot, unsigned part, unsigned step)
{
    set_cond(m, slot, part, step, 0);
    set_prob(m, slot, part, step, 0);
    set_pitch(m, slot, part, step, 0);
    set_vol(m, slot, part, step, 0);
}

/* The velocities of the fixed levels 1..7 and their percentages. */
static const uint8_t vol_q7[8] EL_DSP_DATA = {0, 19, 36, 51, 66, 84, 106, 127};
float dr_vol_velocity(unsigned level) { return level && level < 8 ? vol_q7[level] / 127.0f : 0.0f; }
int dr_vol_percent(unsigned level) { return level && level < 8 ? (int)(vol_q7[level] * 100 + 63) / 127 : 0; }

static EL_DSP_CODE uint32_t fill_random(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return *s = n ? n : 0x9e3779b9u;
}

/* Fills (N12): the groove's first bar, its end turned into a run on the
 * snare (or clap): four kinds, chosen by the seed. The closed hat makes way
 * for the run, the kick keeps its downbeat. */
EL_DSP_CODE void dr_generate_fill(const dr_pattern_t *g, uint32_t seed, dr_fill_t *out)
{
    /* Feedback 09.11.: the fill goes through the whole bar, not only its end.
     * Four kinds, each a bar that builds up (sparse at the start, densest at
     * the end), the groove's kick on the one and its cymbal kept; the closed
     * hat thins to quarters, then stops. A little variation from the seed. */
    static const uint16_t kick[4] = {0x0211u, 0x0049u, 0x0c49u, 0x1041u};
    static const uint16_t lead[4] = {0xf544u, 0xf555u, 0xd6a4u, 0xf0a0u};   /* the main voice of the run */
    static const uint16_t second[4] = {0x0a00u, 0x0000u, 0x2004u, 0x0f00u};  /* a second voice in between */
    static const uint16_t accents[4] = {0x9100u, 0xc110u, 0x9220u, 0xa101u};
    uint32_t rng = seed ? seed : 0x2545f491u;
    memset(out, 0, sizeof *out);
    unsigned kind = fill_random(&rng) % 4u;
    unsigned lv = kind == 1 ? D8_CP : D8_SD;                       /* clap run or snare run */
    unsigned sv = kind == 3 ? D8_RS : (fill_random(&rng) & 1u) ? D8_CP : D8_RS;
    if (sv == lv) sv = D8_RS;
    out->hit[D8_BD] = (uint16_t)(kick[kind] | 0x0001u);
    out->hit[lv] = lead[kind];
    out->hit[sv] |= second[kind];
    out->hit[D8_CH] = (uint16_t)(g->hit[D8_CH] & 0x0111u);           /* quarters of the groove's hat, first half */
    out->hit[D8_CY] = (uint16_t)(g->hit[D8_CY] & 0x0001u);
    out->roll[lv] = (uint16_t)((fill_random(&rng) & 1u) ? 0x4000u : 0x2000u);   /* a 32nd roll near the end */
    out->hit[lv] |= out->roll[lv];
    if (fill_random(&rng) & 1u) out->hit[lv] &= (uint16_t)~(1u << (2u + fill_random(&rng) % 6u));   /* a gap early on */
    if (lv == D8_CP) out->hit[D8_SD] |= 0x8000u;                     /* the last one with the snare */
    out->accent = accents[kind];
    out->used = 1;
}

/* Steps old..len-1 of bits as a copy of the steps a pattern length before. */
static EL_DSP_CODE uint32_t repeat_bits(uint32_t bits, unsigned old, unsigned len)
{
    for (unsigned s = old; s < len && s < 32u; ++s)
        bits = (bits & ~(1u << s)) | (((bits >> (s % old)) & 1u) << s);
    return bits;
}

static EL_DSP_CODE void enter(dr_machine_t *m, unsigned index)
{
    m->current = (uint8_t)index;
    m->pass = 0;
    tr808_sound_apply(&m->kit, &m->sound[index]);
    if (m->pattern[index].kit < DR_KITS) m->kit_index = m->pattern[index].kit;
}

static EL_DSP_CODE void silence(dr_machine_t *m)
{
    tr808_silence(&m->kit);
    m->roll_pending = 0;
    m->suppress = 0;
}

static inline int audible(const dr_machine_t *m, unsigned part)
{
    uint16_t bit = (uint16_t)(1u << part);
    return !(m->mute & bit) && (!m->solo || (m->solo & bit));
}

EL_DSP_CODE void dr_hit(dr_machine_t *m, unsigned part, float velocity, int record, int semitones)
{
    if (part >= DR_PARTS || velocity <= 0.0f) return;
    if (record != 2) tr808_trigger_pitch(&m->kit, (int)part, velocity, semitones);   /* 2: recorded without a sound (N2) */
    if (!record || !m->recording || !m->playing) return;
    /* Quantize to the nearest 16th. A hit late in a step lands on the next
     * one, which must not play it a second time. */
    dr_pattern_t *p = &m->pattern[m->current];
    unsigned step = m->pos;
    if (m->since_step > 0.5f * m->step_frames) {
        step = (step + 1u) % p->length;
        m->suppress |= 1u << part;
    }
    p->hit[part] |= 1u << step;
    if (velocity > 0.9f) p->accent |= 1u << step;
    ++m->revision;
}

EL_DSP_CODE void dr_rewind(dr_machine_t *m)
{
    m->oh_short = 0.0f;
    m->fill_now = 0;
    m->bars = 0;
    m->playing = 0;
    m->pos = 0;
    m->roll_pending = 0;
    m->suppress = 0;
    m->pass = 0;
    if (m->next != m->current) enter(m, m->next);
}

EL_DSP_CODE void dr_step(dr_machine_t *m, uint32_t step, float step_frames)
{
    dr_pattern_t *p = &m->pattern[m->current];
    unsigned pos;
    (void)step;
    if (!m->playing) {
        pos = 0;
        m->playing = 1;
        m->pass = 0;
    } else {
        pos = m->pos + 1u;
        if (pos >= p->length) {
            pos = 0;
            ++m->pass;
            if (m->next != m->current) {
                enter(m, m->next);
                p = &m->pattern[m->current];
            }
        }
    }
    m->pos = (uint8_t)pos;
    m->step_frames = step_frames;
    m->since_step = 0.0f;
    /* Fill-ins (N12): at each bar, whether this one is the fill. */
    uint32_t sixteenth = step;
    if (sixteenth % DR_FILL_STEPS == 0u) {
        if (sixteenth) ++m->bars;
        unsigned every = m->fill_auto;
        m->fill_now = m->fill_req || (every && (m->bars + 1u) % every == 0u);
        if (m->fill_req == 1) m->fill_req = 0;
        dr_fill_t *f = &m->fill[m->current];
        if (m->fill_now && !f->used) dr_generate_fill(p, m->random ^ m->bars, f);
    }
    uint32_t bit = 1u << pos, suppress = m->suppress;
    m->suppress = 0;
    const dr_fill_t *fill = m->fill_now ? &m->fill[m->current] : 0;
    uint32_t fbit = 1u << (sixteenth % DR_FILL_STEPS);
    float velocity = (fill ? (fill->accent & fbit) != 0 : (p->accent & bit) != 0) ? 1.0f : DR_NORMAL;
    m->roll_pending = 0;
    /* B16: OH and CH on this step with the hats choking each other: the open
     * hat alone, cut short (the closed one would choke it at once). */
    int both = m->kit.kit[2] && (fill ? (fill->hit[D8_OH] & fill->hit[D8_CH] & fbit) : (p->hit[D8_OH] & p->hit[D8_CH] & bit)) &&
               audible(m, D8_OH) && audible(m, D8_CH);
    if (both) { suppress |= 1u << D8_CH; m->oh_short = DR_OH_SHORT_MS * (EL_SAMPLE_RATE / 1000.0f); }
    if (fill) {
        for (unsigned part = 0; part < DR_PARTS; ++part) {
            if (!(fill->hit[part] & fbit) || !audible(m, part) || m->muted) continue;
            if (!(suppress & (1u << part))) tr808_trigger_pitch(&m->kit, (int)part, velocity, 0);
            if (fill->roll[part] & fbit) {
                m->roll_pending |= 1u << part;
                m->roll_velocity[part] = velocity * (DR_ROLL_SCALE_Q7 / 127.0f);
                m->roll_semis[part] = 0;
            }
        }
        m->roll_countdown = 0.5f * step_frames;
        return;
    }
    for (unsigned part = 0; part < DR_PARTS; ++part) {
        if (!(p->hit[part] & bit) || !audible(m, part) || m->muted) continue;
        if (!el_cond_plays(dr_cond(m, m->current, part, pos), m->pass)) continue;
        unsigned prob = dr_prob(m, m->current, part, pos);
        if (prob && !el_prob_plays(prob, el_rnd(&m->random))) continue;
        float hit = velocity;
        unsigned level = dr_vol(m, m->current, part, pos);
        if (level) hit = dr_vol_velocity(level);
        int semis = dr_pitch(m, m->current, part, pos);
        if (!(suppress & (1u << part))) tr808_trigger_pitch(&m->kit, (int)part, hit, semis);
        if (p->roll[part] & bit) {
            m->roll_pending |= 1u << part;
            m->roll_velocity[part] = hit * (DR_ROLL_SCALE_Q7 / 127.0f);
            m->roll_semis[part] = (int8_t)semis;
        }
    }
    m->roll_countdown = 0.5f * step_frames;
}

EL_DSP_CODE int dr_frames_until(const dr_machine_t *m, int limit)
{
    if (m->oh_short > 0.0f) {
        int f = (int)m->oh_short;
        if ((float)f < m->oh_short) ++f;
        if (f < limit) limit = f < 1 ? 1 : f;
    }
    if (!m->roll_pending) return limit;
    int frames = (int)m->roll_countdown;
    if ((float)frames < m->roll_countdown) ++frames;
    return frames < limit ? frames : limit;
}

EL_DSP_CODE void dr_render(dr_machine_t *m, float *kick, float *left, float *right,
                           float *rev, float *gate, float *dly, int frames)
{
    if (frames > 0 && tr808_active(&m->kit)) {
        /* Static scratch keeps the audio interrupt's stack small. Plain
         * loops: no libc (Flash-resident) calls on the audio path. */
        static float k[EL_BLOCK], l[EL_BLOCK], r[EL_BLOCK], v[EL_BLOCK], g[EL_BLOCK], d[EL_BLOCK];
        for (int i = 0; i < frames; ++i) k[i] = l[i] = r[i] = v[i] = g[i] = d[i] = 0.0f;
        tr808_render(&m->kit, k, l, r, v, g, d, frames);
        float gain = m->level;
        for (int i = 0; i < frames; ++i) {
            kick[i] += gain * k[i];
            left[i] += gain * l[i];
            right[i] += gain * r[i];
            rev[i] += gain * v[i];
            gate[i] += gain * g[i];
            dly[i] += gain * d[i];
        }
    }
    m->since_step += (float)frames;
    if (m->oh_short > 0.0f) {
        m->oh_short -= (float)frames;
        if (m->oh_short <= 0.0f) { m->oh_short = 0.0f; tr808_choke(&m->kit, D8_OH); }
    }
    if (!m->roll_pending) return;
    m->roll_countdown -= (float)frames;
    if (m->roll_countdown > 0.0f) return;
    for (unsigned part = 0; part < DR_PARTS; ++part)
        if (m->roll_pending & (1u << part)) tr808_trigger_pitch(&m->kit, (int)part, m->roll_velocity[part], m->roll_semis[part]);
    m->roll_pending = 0;
}

static EL_DSP_CODE void apply(dr_machine_t *m, const el_cmd_t *c)
{
    dr_pattern_t *p = &m->pattern[m->current];
    unsigned part = c->a, step = c->b;
    switch (c->type) {
    case DR_CMD_HIT:
        dr_hit(m, part, c->b / 127.0f, c->c, c->value);
        return;
    case DR_CMD_TOGGLE:
        if (part < DR_PARTS && step < DR_STEPS) {
            p->hit[part] ^= 1u << step;
            if (!(p->hit[part] & (1u << step))) { p->roll[part] &= ~(1u << step); clear_locks(m, m->current, part, step); }
        }
        break;
    case DR_CMD_ROLL:
        if (part < DR_PARTS && step < DR_STEPS) {
            p->roll[part] ^= 1u << step;
            if (p->roll[part] & (1u << step)) p->hit[part] |= 1u << step;
        }
        break;
    case DR_CMD_ACCENT:
        if (step < DR_STEPS) p->accent ^= 1u << step;
        break;
    case DR_CMD_CLEAR_PART:
        if (part < DR_PARTS) {
            p->hit[part] = p->roll[part] = 0;
            for (unsigned k = 0; k < 3; ++k) { m->cond[m->current][k][part] = 0; m->vol[m->current][k][part] = 0; }
            for (unsigned k = 0; k < 4; ++k) m->prob[m->current][k][part] = 0;
            if (dr_pitch_slot(part) >= 0) for (unsigned k = 0; k < 5; ++k) m->pitch[m->current][k][dr_pitch_slot(part)] = 0;
        }
        else {
            for (unsigned i = 0; i < DR_PARTS; ++i) p->hit[i] = p->roll[i] = 0;
            memset(m->cond[m->current], 0, sizeof m->cond[m->current]);
            memset(m->prob[m->current], 0, sizeof m->prob[m->current]);
            memset(m->pitch[m->current], 0, sizeof m->pitch[m->current]);
            memset(m->vol[m->current], 0, sizeof m->vol[m->current]);
            p->accent = 0;
        }
        break;
    case DR_CMD_LENGTH: {
        unsigned old = p->length ? p->length : 1u, len = (unsigned)el_clampi(step, 1, DR_STEPS);
        /* Longer (feedback 08.11.): the new steps take the pattern so far,
         * when they hold nothing yet (hits, rolls, accents and the step
         * settings, each step from the one a pattern length before). */
        uint32_t fresh = 0;
        for (unsigned s = old; s < len; ++s) fresh |= 1u << s;
        int empty = (p->accent & fresh) == 0;
        for (unsigned i = 0; i < DR_PARTS && empty; ++i) empty = !((p->hit[i] | p->roll[i]) & fresh);
        if (len > old && empty) {
            p->accent = repeat_bits(p->accent, old, len);
            for (unsigned i = 0; i < DR_PARTS; ++i) { p->hit[i] = repeat_bits(p->hit[i], old, len); p->roll[i] = repeat_bits(p->roll[i], old, len); }
            unsigned c = m->current;
            for (unsigned i = 0; i < DR_PARTS; ++i) {
                for (unsigned k = 0; k < 3; ++k) { m->cond[c][k][i] = repeat_bits(m->cond[c][k][i], old, len); m->vol[c][k][i] = repeat_bits(m->vol[c][k][i], old, len); }
                for (unsigned k = 0; k < 4; ++k) m->prob[c][k][i] = repeat_bits(m->prob[c][k][i], old, len);
            }
            for (unsigned i = 0; i < DR_PITCH_PARTS; ++i)
                for (unsigned k = 0; k < 5; ++k) m->pitch[c][k][i] = repeat_bits(m->pitch[c][k][i], old, len);
        }
        p->length = (uint8_t)len;
        /* Shorter: the playhead keeps its place in the bar (it used to jump to
         * the last step and the beat ran out of step with the other areas). */
        if (m->pos >= p->length) m->pos = (uint8_t)(m->pos % p->length);
        break;
    }
    case DR_CMD_SWING:
        p->swing = (uint8_t)el_clampi(step, 50, 75);
        break;
    case DR_CMD_SELECT:
        if (part < DR_PATTERNS) {
            m->next = (uint8_t)part;
            if (step || !m->playing) enter(m, part);
            if (m->pos >= m->pattern[m->current].length) m->pos = 0;
        }
        break;
    case DR_CMD_COPY:
        if (part < DR_PATTERNS && step < DR_PATTERNS) {
            m->pattern[step] = m->pattern[part];
            m->fill[step] = m->fill[part];
            m->sound[step] = m->sound[part];
            memcpy(m->cond[step], m->cond[part], sizeof m->cond[step]);
            memcpy(m->prob[step], m->prob[part], sizeof m->prob[step]);
            memcpy(m->pitch[step], m->pitch[part], sizeof m->pitch[step]);
            memcpy(m->vol[step], m->vol[part], sizeof m->vol[step]);
            if (step == m->current) enter(m, step);
        }
        break;
    case DR_CMD_LOAD:
        if (part < DR_PATTERNS) {
            /* Another kit: the slot starts from that kit's factory sound;
             * the same kit keeps the slot's knobs. */
            if (m->staging.kit != m->pattern[part].kit && m->staging.kit < DR_KITS) {
                dr_kit_recall(m, &m->sound[part], m->staging.kit);
                if (part == m->current) { tr808_sound_apply(&m->kit, &m->sound[part]); m->kit_index = m->staging.kit; }
            }
            m->pattern[part] = m->staging;
            /* a new groove brings its fill (N12); a variation keeps the slot's */
            if (!c->c || !m->fill[part].used) dr_generate_fill(&m->pattern[part], m->random ^ 0x9e3779b9u, &m->fill[part]);
            if (!c->c) {
                memset(m->cond[part], 0, sizeof m->cond[part]);
                memset(m->prob[part], 0, sizeof m->prob[part]);
                memset(m->pitch[part], 0, sizeof m->pitch[part]);
                memset(m->vol[part], 0, sizeof m->vol[part]);
            }
        }
        EL_FENCE();
        ++m->staging_taken;
        break;
    case DR_CMD_MUTE:
        if (part < DR_PARTS) m->mute = (uint16_t)(step ? m->mute | (1u << part) : m->mute & ~(1u << part));
        break;
    case DR_CMD_SOLO:
        if (part < DR_PARTS) m->solo = (uint16_t)(step ? m->solo | (1u << part) : m->solo & ~(1u << part));
        break;
    case DR_CMD_PARAM:
        tr808_set(&m->kit, (int)part, (int)step, c->value);
        tr808_sound_set(&m->sound[m->current], (int)part, (int)step, c->value);
        if (m->kit_index < DR_KITS) {   /* the kit remembers it (N1) */
            m->kit_mem[m->kit_index] = m->sound[m->current];
            m->kit_mem_used |= (uint8_t)(1u << m->kit_index);
        }
        break;
    case DR_CMD_KIT:
        apply_kit(m, part, step == 1);
        p->kit = (uint8_t)m->kit_index;   /* the pattern keeps the chosen kit */
        break;
    case DR_CMD_LEVEL:
        m->level_value = (uint8_t)el_clampi(c->value, 0, 127);
        m->level = m->level_value / 127.0f;
        break;
    case DR_CMD_RECORD:
        m->recording = step ? 1 : 0;
        break;
    case DR_CMD_PANIC:
        silence(m);
        break;
    case DR_CMD_AREA_MUTE:
        m->muted = step ? 1 : 0;
        break;
    case DR_CMD_COND:
        if (part < DR_PARTS && step < DR_STEPS) set_cond(m, m->current, part, step, (unsigned)el_clampi(c->value, 0, EL_CONDS - 1));
        break;
    case DR_CMD_PROB:
        if (part < DR_PARTS && step < DR_STEPS) set_prob(m, m->current, part, step, (unsigned)el_clampi(c->value, 0, EL_PROBS - 1));
        break;
    case DR_CMD_PITCH:
        if (part < DR_PARTS && step < DR_STEPS) set_pitch(m, m->current, part, step, c->value);
        break;
    case DR_CMD_FILL:
        m->fill_req = (uint8_t)(step > 2 ? 0 : step);
        break;
    case DR_CMD_FILL_AUTO:
        m->fill_auto = (uint8_t)step;
        break;
    case DR_CMD_FILL_EDIT: {
        dr_fill_t *f = &m->fill[m->current];
        m->fill_edit = step ? 1 : 0;
        if (step && !f->used) {   /* an unused fill starts as the groove's first bar */
            memset(f, 0, sizeof *f);
            for (unsigned i = 0; i < DR_PARTS; ++i) { f->hit[i] = (uint16_t)p->hit[i]; f->roll[i] = (uint16_t)p->roll[i]; }
            f->accent = (uint16_t)p->accent;
            if (p->length < DR_FILL_STEPS) {
                uint16_t keep = (uint16_t)((1u << p->length) - 1u);
                for (unsigned i = 0; i < DR_PARTS; ++i) { f->hit[i] &= keep; f->roll[i] &= keep; }
                f->accent &= keep;
            }
            f->used = 1;
        }
        break;
    }
    case DR_CMD_FILL_TOGGLE:
        if (part < DR_PARTS && step < DR_FILL_STEPS) {
            dr_fill_t *f = &m->fill[m->current];
            f->hit[part] ^= (uint16_t)(1u << step);
            if (!(f->hit[part] & (1u << step))) f->roll[part] &= (uint16_t)~(1u << step);
            f->used = 1;
        }
        break;
    case DR_CMD_FILL_ACCENT:
        if (step < DR_FILL_STEPS) { m->fill[m->current].accent ^= (uint16_t)(1u << step); m->fill[m->current].used = 1; }
        break;
    case DR_CMD_FILL_ROLL:
        if (part < DR_PARTS && step < DR_FILL_STEPS) {
            dr_fill_t *f = &m->fill[m->current];
            f->roll[part] ^= (uint16_t)(1u << step);
            if (f->roll[part] & (1u << step)) f->hit[part] |= (uint16_t)(1u << step);
            f->used = 1;
        }
        break;
    case DR_CMD_VOL:
        if (part < DR_PARTS && step < DR_STEPS) set_vol(m, m->current, part, step, (unsigned)el_clampi(c->value, 0, 7));
        break;
    default:
        return;
    }
    ++m->revision;
}

EL_DSP_CODE void dr_process_commands(dr_machine_t *m)
{
    el_cmd_t c;
    while (el_queue_pop(&m->queue, &c)) apply(m, &c);
}

EL_DSP_CODE void dr_step_at(dr_machine_t *m, uint32_t step, float step_frames, unsigned target)
{
    unsigned len = m->pattern[m->current].length ? m->pattern[m->current].length : 1u;
    unsigned real = !m->playing ? 0u : m->pos + 1u < len ? m->pos + 1u : 0u;
    /* the step conditions by the playhead's pass (dr_step counts one more
     * when the repeated step is the first) */
    uint8_t next = m->next, pass = (uint8_t)(m->pass + (m->playing && real == 0));
    m->next = m->current;
    m->playing = 1;
    m->pos = (uint8_t)((target % len + len - 1u) % len);
    m->pass = (uint8_t)(target % len == 0 ? pass - 1u : pass);
    dr_step(m, step, step_frames);
    m->next = next;
    m->pos = (uint8_t)real;
    m->pass = pass;
}
