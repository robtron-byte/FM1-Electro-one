/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "drums.h"
#include <string.h>

const char *const dr_part_names[DR_PARTS] = {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH"};
const char *const dr_kit_names[DR_KITS] = {"TR-808", "ELECTRO", "MIAMI", "KRAFT"};

enum { DR_NORMAL_VELOCITY_Q7 = 84, DR_ROLL_SCALE_Q7 = 96 };
#define DR_NORMAL (DR_NORMAL_VELOCITY_Q7 / 127.0f)

/* Kit presets: part, parameter index (as listed by tr808_param), value,
 * applied on top of the stock circuit. TR-808 is the untouched machine. */
typedef struct { uint8_t part, param, value; } dr_kit_entry_t;
enum { DR_KIT_ENTRIES = 16 };
static const dr_kit_entry_t kits[DR_KITS][DR_KIT_ENTRIES] EL_DSP_DATA = {
    /* TR-808: stock, a touch of gated reverb on the clap. */
    {{D8_CP, 5, 24}, {255, 0, 0}},
    /* ELECTRO (Anthony Rother style): longer kick with a little drive,
     * snappy snare and clap in the non-linear reverb, tight hats. */
    {{D8_BD, 2, 82}, {D8_BD, 3, 42}, {D8_BD, 7, 14}, {D8_SD, 2, 72}, {D8_SD, 3, 88}, {D8_SD, 6, 44},
     {D8_CP, 0, 108}, {D8_CP, 5, 56}, {D8_CH, 2, 52}, {D8_OH, 1, 34}, {D8_CB, 0, 84}, {D8_RS, 0, 92},
     {D8_RS, 5, 30}, {D8_KIT, 1, 110}, {255, 0, 0}},
    /* MIAMI: booming long kick, a bit lower and driven, darker snare. */
    {{D8_BD, 1, 58}, {D8_BD, 2, 116}, {D8_BD, 0, 112}, {D8_BD, 7, 28}, {D8_BD, 3, 30},
     {D8_SD, 2, 40}, {D8_SD, 3, 76}, {D8_CP, 5, 40}, {D8_CH, 2, 44}, {255, 0, 0}},
    /* KRAFT: short clicky kick, high tight snare, claves, darker cymbal. */
    {{D8_BD, 2, 34}, {D8_BD, 3, 86}, {D8_SD, 1, 74}, {D8_SD, 2, 84}, {D8_SD, 3, 52}, {D8_SD, 6, 52},
     {D8_RS, 3, 1}, {D8_CH, 2, 40}, {D8_CY, 1, 34}, {D8_CB, 1, 70}, {D8_KIT, 2, 1}, {255, 0, 0}},
};

static const uint8_t part_notes[DR_PARTS] = {36, 38, 41, 45, 48, 37, 39, 56, 49, 46, 42};

unsigned dr_part_note(unsigned part) { return part < DR_PARTS ? part_notes[part] : 0; }

int dr_note_part(unsigned note)
{
    for (unsigned p = 0; p < DR_PARTS; ++p)
        if (part_notes[p] == note) return (int)p;
    /* Common alternates: 35 kick, 40 snare, 43/47/50 toms, 44 pedal hat, 51 ride. */
    switch (note) {
    case 35: return D8_BD;
    case 40: return D8_SD;
    case 43: return D8_LT;
    case 47: return D8_MT;
    case 50: return D8_HT;
    case 44: return D8_CH;
    case 51: case 57: return D8_CY;
    default: return -1;
    }
}

static EL_DSP_CODE void apply_kit(dr_machine_t *m, unsigned index)
{
    if (index >= DR_KITS) return;
    float level = m->level;
    tr808_init(&m->kit);
    for (unsigned i = 0; i < DR_KIT_ENTRIES && kits[index][i].part != 255; ++i)
        tr808_set(&m->kit, kits[index][i].part, kits[index][i].param, kits[index][i].value);
    m->kit_index = (uint8_t)index;
    m->level = level;
}

void dr_init(dr_machine_t *m)
{
    memset(m, 0, sizeof *m);
    el_queue_init(&m->queue);
    for (unsigned i = 0; i < DR_PATTERNS; ++i) dr_factory_pattern(i, &m->pattern[i]);
    m->level_value = 100;
    m->level = 100 / 127.0f;
    m->random = 0x2545f491u;
    apply_kit(m, m->pattern[0].kit);
}

int dr_post(dr_machine_t *m, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value)
{
    el_cmd_t cmd = {type, a, b, c, value};
    return el_queue_push(&m->queue, cmd);
}

int dr_post_pattern(dr_machine_t *m, unsigned slot, const dr_pattern_t *p)
{
    if (slot >= DR_PATTERNS || !p) return -1;
    if (m->staging_posted != m->staging_taken) return -2;
    m->staging = *p;
    EL_FENCE();
    if (dr_post(m, DR_CMD_LOAD, (uint8_t)slot, 0, 0, 0)) return -1;
    ++m->staging_posted;
    return 0;
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

EL_DSP_CODE void dr_hit(dr_machine_t *m, unsigned part, float velocity, int record)
{
    if (part >= DR_PARTS || velocity <= 0.0f) return;
    tr808_trigger(&m->kit, (int)part, velocity);
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
    m->playing = 0;
    m->pos = 0;
    m->roll_pending = 0;
    m->suppress = 0;
    if (m->next != m->current) m->current = m->next;
}

EL_DSP_CODE void dr_step(dr_machine_t *m, uint32_t step, float step_frames)
{
    dr_pattern_t *p = &m->pattern[m->current];
    unsigned pos;
    (void)step;
    if (!m->playing) {
        pos = 0;
        m->playing = 1;
    } else {
        pos = m->pos + 1u;
        if (pos >= p->length) {
            pos = 0;
            if (m->next != m->current) {
                m->current = m->next;
                p = &m->pattern[m->current];
            }
        }
    }
    m->pos = (uint8_t)pos;
    m->step_frames = step_frames;
    m->since_step = 0.0f;
    uint32_t bit = 1u << pos, suppress = m->suppress;
    m->suppress = 0;
    float velocity = (p->accent & bit) ? 1.0f : DR_NORMAL;
    m->roll_pending = 0;
    for (unsigned part = 0; part < DR_PARTS; ++part) {
        if (!(p->hit[part] & bit) || !audible(m, part) || m->muted) continue;
        if (!(suppress & (1u << part))) tr808_trigger(&m->kit, (int)part, velocity);
        if (p->roll[part] & bit) {
            m->roll_pending |= 1u << part;
            m->roll_velocity[part] = velocity * (DR_ROLL_SCALE_Q7 / 127.0f);
        }
    }
    m->roll_countdown = 0.5f * step_frames;
}

EL_DSP_CODE int dr_frames_until(const dr_machine_t *m, int limit)
{
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
    if (!m->roll_pending) return;
    m->roll_countdown -= (float)frames;
    if (m->roll_countdown > 0.0f) return;
    for (unsigned part = 0; part < DR_PARTS; ++part)
        if (m->roll_pending & (1u << part)) tr808_trigger(&m->kit, (int)part, m->roll_velocity[part]);
    m->roll_pending = 0;
}

static EL_DSP_CODE void apply(dr_machine_t *m, const el_cmd_t *c)
{
    dr_pattern_t *p = &m->pattern[m->current];
    unsigned part = c->a, step = c->b;
    switch (c->type) {
    case DR_CMD_HIT:
        dr_hit(m, part, c->b / 127.0f, c->c);
        return;
    case DR_CMD_TOGGLE:
        if (part < DR_PARTS && step < DR_STEPS) {
            p->hit[part] ^= 1u << step;
            if (!(p->hit[part] & (1u << step))) p->roll[part] &= ~(1u << step);
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
        if (part < DR_PARTS) p->hit[part] = p->roll[part] = 0;
        else {
            for (unsigned i = 0; i < DR_PARTS; ++i) p->hit[i] = p->roll[i] = 0;
            p->accent = 0;
        }
        break;
    case DR_CMD_LENGTH:
        p->length = (uint8_t)el_clampi(step, 1, DR_STEPS);
        if (m->pos >= p->length) m->pos = (uint8_t)(p->length - 1u);
        break;
    case DR_CMD_SWING:
        p->swing = (uint8_t)el_clampi(step, 50, 75);
        break;
    case DR_CMD_SELECT:
        if (part < DR_PATTERNS) {
            m->next = (uint8_t)part;
            if (step || !m->playing) m->current = (uint8_t)part;
            if (m->pos >= m->pattern[m->current].length) m->pos = 0;
        }
        break;
    case DR_CMD_COPY:
        if (part < DR_PATTERNS && step < DR_PATTERNS) m->pattern[step] = m->pattern[part];
        break;
    case DR_CMD_LOAD:
        if (part < DR_PATTERNS) m->pattern[part] = m->staging;
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
        break;
    case DR_CMD_KIT:
        apply_kit(m, part);
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
