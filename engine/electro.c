/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "electro.h"
#include <string.h>

void el_init(el_engine_t *e)
{
    memset(e, 0, sizeof *e);
    el_transport_init(&e->transport);
    el_queue_init(&e->queue);
    el_queue_init(&e->midi_queue);
    dr_init(&e->drums);
    e->transport.swing = e->drums.pattern[0].swing;
    e->master = 0.8f;
}

int el_post(el_engine_t *e, uint8_t type, uint8_t a, int16_t value)
{
    el_cmd_t cmd = {type, a, 0, 0, value};
    return el_queue_push(&e->queue, cmd);
}

int el_midi_input(el_engine_t *e, const uint8_t *m, size_t n)
{
    el_cmd_t cmd = {0, 0, 0, 0, 0};
    if (!m || !n) return 0;
    switch (m[0]) {
    case 0xf8: cmd.type = EL_CMD_MIDI_TICK; break;
    case 0xfa: cmd.type = EL_CMD_START; cmd.c = 1; break;
    case 0xfb: cmd.type = EL_CMD_CONTINUE; cmd.c = 1; break;
    case 0xfc: cmd.type = EL_CMD_STOP; break;
    default:
        if (n >= 3 && (m[0] & 0xf0u) == 0x90u && (m[0] & 15u) == 9u && m[2]) {
            int part = dr_note_part(m[1]);
            if (part < 0) return 0;
            cmd.type = EL_CMD_DRUM_HIT;
            cmd.a = (uint8_t)part;
            cmd.value = m[2];
        } else return 0;
    }
    return el_queue_push(&e->midi_queue, cmd) == 0;
}

static EL_DSP_CODE void apply(el_engine_t *e, const el_cmd_t *c)
{
    el_transport_t *t = &e->transport;
    switch (c->type) {
    case EL_CMD_START:
        /* MIDI Start hands the clock to the sender; PLAY on the panel takes
         * it back. c marks commands from the MIDI queue. */
        t->source = c->c ? EL_CLOCK_MIDI : EL_CLOCK_INTERNAL;
        dr_rewind(&e->drums);
        el_transport_start(t);
        break;
    case EL_CMD_STOP:
        el_transport_stop(t);
        dr_rewind(&e->drums);
        break;
    case EL_CMD_CONTINUE:
        t->source = c->c ? EL_CLOCK_MIDI : EL_CLOCK_INTERNAL;
        el_transport_continue(t);
        break;
    case EL_CMD_BPM:
        el_transport_set_bpm(t, c->value * 0.1f);
        break;
    case EL_CMD_CLOCK_SOURCE:
        el_transport_stop(t);
        dr_rewind(&e->drums);
        t->source = c->a ? EL_CLOCK_MIDI : EL_CLOCK_INTERNAL;
        break;
    case EL_CMD_MIDI_TICK:
        el_transport_midi_tick(t);
        break;
    case EL_CMD_MASTER:
        e->master = el_clampi(c->value, 0, 127) / 127.0f;
        break;
    case EL_CMD_DRUM_HIT:
        dr_hit(&e->drums, c->a, el_clampi(c->value, 1, 127) / 127.0f, 1);
        break;
    default:
        break;
    }
}

/* Gentle saturation instead of hard clipping at full scale. */
static inline float saturate(float x)
{
    x = el_clampf(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static EL_DSP_CODE void render_bus(el_engine_t *e, float *left, float *right, unsigned frames)
{
    static float rev[EL_BLOCK], dly[EL_BLOCK];
    el_cmd_t c;
    while (el_queue_pop(&e->queue, &c)) apply(e, &c);
    while (el_queue_pop(&e->midi_queue, &c)) apply(e, &c);
    dr_process_commands(&e->drums);
    el_transport_t *t = &e->transport;
    t->swing = e->drums.pattern[e->drums.current].swing;
    for (unsigned i = 0; i < frames; ++i) left[i] = right[i] = rev[i] = dly[i] = 0.0f;
    unsigned pos = 0, guard = 0;
    while (pos < frames && guard++ < 64) {
        int n = (int)(frames - pos);
        n = el_transport_frames_until(t, n);
        n = dr_frames_until(&e->drums, n);
        dr_render(&e->drums, left + pos, right + pos, rev + pos, dly + pos, n);
        uint32_t step;
        if (el_transport_advance(t, n, &step)) dr_step(&e->drums, step, t->last_length);
        pos += (unsigned)n;
    }
    if (pos < frames) dr_render(&e->drums, left + pos, right + pos, rev + pos, dly + pos, (int)(frames - pos));
    /* Reverb and delay sends are summed into the FX bus once the shared
     * effects from the synth apps are ported; until then they stay unused. */
    ++e->blocks;
}

/* Mix bus, static so the audio interrupt's stack stays small. */
static float left[EL_BLOCK], right[EL_BLOCK];

EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames)
{
    if (frames > EL_BLOCK) frames = EL_BLOCK;
    render_bus(e, left, right, frames);
    float g = e->master;
    for (unsigned i = 0; i < frames; ++i) {
        output[2 * i] = (int32_t)(saturate(g * left[i]) * 8388607.0f);
        output[2 * i + 1] = (int32_t)(saturate(g * right[i]) * 8388607.0f);
    }
}

EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames)
{
    if (frames > EL_BLOCK) frames = EL_BLOCK;
    render_bus(e, left, right, frames);
    float g = e->master;
    for (unsigned i = 0; i < frames; ++i) {
        output[2 * i] = saturate(g * left[i]);
        output[2 * i + 1] = saturate(g * right[i]);
    }
}
