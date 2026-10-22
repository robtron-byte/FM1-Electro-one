/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "electro.h"
#include <string.h>

void el_init(el_engine_t *e)
{
    memset(e, 0, sizeof *e);
    el_transport_init(&e->transport);
    el_queue_init(&e->queue);
    el_queue_init(&e->midi_queue);
    dr_init(&e->drums);
    fx_init(&e->fx);
    mb_init(&e->mix);
    static const uint8_t voices[EL_TRACKS] = {3, 4, 2};
    for (unsigned i = 0; i < EL_TRACKS; ++i) st_init(&e->track[i], i + 1u, voices[i], 0x51ed270bu * (i + 1u));
    el_generate_all(e, 0x2545f491u);
    e->master = 1.0f;
}

/* Default sounds per area. */
static const char *const default_sounds[EL_TRACKS] = {"S&H BLEEP", "JX STRINGS", "ROBO BASS"};
/* Generator density per area: the rhythmic sounds stay sparse. */
static const uint8_t gen_density[EL_TRACKS] = {28, 64, 56};
/* Keys a generated groove may land in: C D A E G F Bb Eb (minor). */
static const int8_t gen_keys[8] = {0, 2, -3, 4, -5, 5, -2, 3};

static uint32_t mix_seed(uint32_t s)
{
    s ^= s >> 16; s *= 0x7feb352du; s ^= s >> 15; s *= 0x846ca68bu; s ^= s >> 16;
    return s ? s : 0x9e3779b9u;
}

void el_generate_parts(const dr_pattern_t *d, uint32_t seed, st_pattern_t out[EL_TRACKS])
{
    const dr_style_t *s = &dr_styles[d->style < DR_STYLES ? d->style : 0];
    const uint8_t styles[EL_TRACKS] = {s->rhythm_style, s->strings_style, s->bass_style};
    int key = gen_keys[mix_seed(seed) % 8u];
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        st_pattern_t *p = &out[i];
        st_generate(p, i + 1u, styles[i], gen_density[i], mix_seed(seed + 7919u * (i + 1u)), d->length, s->harmony);
        for (unsigned k = 0; k < ST_STEPS; ++k) p->step[k].note = (uint8_t)el_clampi(p->step[k].note + key, 0, 127);
    }
}

void el_generate_all(el_engine_t *e, uint32_t seed)
{
    for (unsigned slot = 0; slot < DR_STYLE_SLOTS; ++slot) {
        uint32_t s = mix_seed(seed + 0x632be5abu * (slot + 1u));
        dr_generate(&e->drums.pattern[slot], slot, 24, s, NULL);
        st_pattern_t parts[EL_TRACKS];
        el_generate_parts(&e->drums.pattern[slot], s ^ 0x5bd1e995u, parts);
        for (unsigned i = 0; i < EL_TRACKS; ++i) e->track[i].pattern[slot] = parts[i];
    }
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        st_track_t *t = &e->track[i];
        int s = syn_sound_find(SYN_MKS, default_sounds[i]);
        if (s >= 0) {
            syn_load(&t->part, SYN_MKS, syn_sound_values((unsigned)s));
            t->preset = (uint8_t)s;
        }
    }
    e->transport.swing = e->drums.pattern[e->drums.current].swing;
}

static void stage_patch(el_engine_t *e, unsigned preset)
{
    const uint8_t *v = syn_sound_values(preset);
    unsigned n = syn_value_count(syn_sound_engine(preset));
    for (unsigned i = 0; i < n; ++i) e->staged_patch[i] = v[i];
}

int el_load_preset(el_engine_t *e, unsigned track, unsigned preset)
{
    if (track >= EL_TRACKS || preset >= syn_sound_count()) return -1;
    if (e->staged_posted != e->staged_taken) return -2;
    stage_patch(e, preset);
    EL_FENCE();
    if (el_post3(e, EL_CMD_TRACK_PRESET, (uint8_t)track, (uint8_t)preset, (uint8_t)syn_sound_engine(preset), 0)) return -1;
    ++e->staged_posted;
    return 0;
}

int el_load_pattern(el_engine_t *e, unsigned track, unsigned slot, const st_pattern_t *p)
{
    if (track >= EL_TRACKS || slot >= ST_PATTERNS || !p) return -1;
    if (e->staged_posted != e->staged_taken) return -2;
    e->staged_pattern = *p;
    EL_FENCE();
    if (el_post3(e, EL_CMD_TRACK_PATTERN, (uint8_t)track, (uint8_t)slot, 0, 0)) return -1;
    ++e->staged_posted;
    return 0;
}

int el_post3(el_engine_t *e, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value)
{
    el_cmd_t cmd = {type, a, b, c, value};
    return el_queue_push(&e->queue, cmd);
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
        } else if (n >= 3 && ((m[0] & 0xf0u) == 0x90u || (m[0] & 0xf0u) == 0x80u) && (m[0] & 15u) < EL_TRACKS) {
            /* Multitimbral: channel 1 RHYTHM, 2 STRINGS, 3 BASS. */
            cmd.type = EL_CMD_NOTE;
            cmd.a = (uint8_t)(m[0] & 15u);
            cmd.b = (uint8_t)(m[1] & 127u);
            cmd.value = (m[0] & 0xf0u) == 0x90u ? (int16_t)(m[2] & 127u) : 0;
        } else return 0;
    }
    return el_queue_push(&e->midi_queue, cmd) == 0;
}

static EL_DSP_CODE void apply_track(el_engine_t *e, const el_cmd_t *c)
{
    st_track_t *t = &e->track[c->a];
    st_pattern_t *p = &t->pattern[t->current];
    st_step_t *s = c->b < ST_STEPS ? &p->step[c->b] : &p->step[0];
    switch (c->type) {
    case EL_CMD_NOTE:
        st_live(t, c->b, el_clampi(c->value, 0, 127) / 127.0f);
        break;
    case EL_CMD_STEP_TOGGLE:
        if (!(s->flags & ST_ON)) { s->note = t->last_note; s->chord = t->chord; }
        s->flags ^= ST_ON;
        break;
    case EL_CMD_STEP_FLAG:
        s->flags = (uint8_t)((s->flags ^ c->c) | ST_ON);
        break;
    case EL_CMD_STEP_NOTE:
        if (!(s->flags & ST_ON)) { s->flags |= ST_ON; s->note = t->last_note; }
        else s->note = (uint8_t)el_clampi(s->note + c->value, 0, 127);
        t->last_note = s->note;
        break;
    case EL_CMD_STEP_CHORD:
        s->chord = (uint8_t)((s->chord & 0x30u) | (unsigned)el_clampi(c->value, 0, MKS_CHORDS));
        break;
    case EL_CMD_STEP_INV:
        s->chord = (uint8_t)((s->chord & 15u) | (unsigned)el_clampi(c->value, 0, 2) << 4);
        break;
    case EL_CMD_TRACK_RATE:
        p->rate = (uint8_t)el_clampi(c->b, 0, ST_RATES - 1);
        t->tick = 0;
        break;
    case EL_CMD_STEP_SET:
        s->note = c->c;
        s->flags = (uint8_t)(c->value & 0xff);
        s->chord = (uint8_t)((c->value >> 8) & 0x3f);
        break;
    case EL_CMD_TRACK_LENGTH:
        p->length = (uint8_t)el_clampi(c->b, 1, ST_STEPS);
        if (t->pos >= p->length) t->pos = (uint8_t)(p->length - 1u);
        break;
    case EL_CMD_TRACK_GATE:
        p->gate = (uint8_t)el_clampi(c->b, 10, 100);
        break;
    case EL_CMD_TRACK_PARAM:
        syn_set(&t->part, c->b, c->value);
        break;
    case EL_CMD_TRACK_PRESET:
        syn_load(&t->part, c->c, e->staged_patch);
        t->preset = c->b;
        if (syn_chord_memory(&t->part)) t->chord = (uint8_t)syn_chord_memory(&t->part);
        EL_FENCE();
        ++e->staged_taken;
        break;
    case EL_CMD_TRACK_PATTERN:
        if (c->b < ST_PATTERNS) t->pattern[c->b] = e->staged_pattern;
        EL_FENCE();
        ++e->staged_taken;
        break;
    case EL_CMD_TRACK_MUTE:
        st_mute(t, c->b);
        break;
    case EL_CMD_TRACK_LEVEL:
        t->level_value = (uint8_t)el_clampi(c->value, 0, 127);
        break;
    case EL_CMD_TRACK_CHORD:
        t->chord = (uint8_t)el_clampi(c->b, 0, MKS_CHORDS);
        break;
    case EL_CMD_TRACK_CLEAR:
        for (unsigned i = 0; i < ST_STEPS; ++i) p->step[i].flags = 0;
        break;
    default:
        return;
    }
    ++t->revision;
}

static EL_DSP_CODE void apply(el_engine_t *e, const el_cmd_t *c)
{
    el_transport_t *t = &e->transport;
    if (c->type >= EL_CMD_NOTE && c->type <= EL_CMD_TRACK_CLEAR) {
        if (c->a < EL_TRACKS) apply_track(e, c);
        return;
    }
    switch (c->type) {
    case EL_CMD_FX:
        if (c->a < FX_PARAMS) e->fx.v[c->a] = (uint8_t)el_clampi(c->value, 0, fx_param_max(c->a));
        break;
    case EL_CMD_RECORD_AREA:
        e->drums.recording = c->a == EL_AREA_BEAT && c->b;
        for (unsigned i = 0; i < EL_TRACKS; ++i) e->track[i].recording = c->a == i + 1u && c->b;
        break;
    case EL_CMD_START:
        /* MIDI Start hands the clock to the sender; PLAY on the panel takes
         * it back. c marks commands from the MIDI queue. */
        t->source = c->c ? EL_CLOCK_MIDI : EL_CLOCK_INTERNAL;
        dr_rewind(&e->drums);
        for (unsigned i = 0; i < EL_TRACKS; ++i) st_rewind(&e->track[i]);
        el_transport_start(t);
        break;
    case EL_CMD_STOP:
        el_transport_stop(t);
        dr_rewind(&e->drums);
        for (unsigned i = 0; i < EL_TRACKS; ++i) st_rewind(&e->track[i]);
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
        for (unsigned i = 0; i < EL_TRACKS; ++i) st_rewind(&e->track[i]);
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
    case EL_CMD_MIX:
        e->mix.enabled = c->a ? 1 : 0;
        break;
    case EL_CMD_SLOT_COPY:
        if (c->a < DR_PATTERNS && c->b < DR_PATTERNS && c->a != c->b) {
            char name[DR_NAME + 1];
            memcpy(name, e->drums.pattern[c->b].name, sizeof name);
            e->drums.pattern[c->b] = e->drums.pattern[c->a];
            if (c->c) memcpy(e->drums.pattern[c->b].name, name, sizeof name); /* user slot keeps its name */
            for (unsigned i = 0; i < EL_TRACKS; ++i) e->track[i].pattern[c->b] = e->track[i].pattern[c->a];
            ++e->drums.revision;
        }
        EL_FENCE();
        ++e->slot_copies;
        break;
    default:
        break;
    }
}

/* Buses, static so the audio interrupt's stack stays small. */
static float kick[EL_BLOCK], drum_l[EL_BLOCK], drum_r[EL_BLOCK], bass_l[EL_BLOCK], bass_r[EL_BLOCK];
static float synth_l[EL_BLOCK], synth_r[EL_BLOCK], rev[EL_BLOCK], gate[EL_BLOCK], dly[EL_BLOCK];
static float left[EL_BLOCK], right[EL_BLOCK];

static EL_DSP_CODE void render_sources(el_engine_t *e, int pos, int n)
{
    el_transport_t *t = &e->transport;
    dr_render(&e->drums, kick + pos, drum_l + pos, drum_r + pos, rev + pos, gate + pos, dly + pos, n);
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        int bass = i == EL_AREA_BASS - 1u;
        st_render(&e->track[i], (bass ? bass_l : synth_l) + pos, (bass ? bass_r : synth_r) + pos,
                  dly + pos, rev + pos, n, t->bpm);
    }
}

static EL_DSP_CODE void render_bus(el_engine_t *e, unsigned frames)
{
    el_cmd_t c;
    while (el_queue_pop(&e->queue, &c)) apply(e, &c);
    while (el_queue_pop(&e->midi_queue, &c)) apply(e, &c);
    dr_process_commands(&e->drums);
    el_transport_t *t = &e->transport;
    t->swing = e->drums.pattern[e->drums.current].swing;
    for (unsigned i = 0; i < frames; ++i)
        kick[i] = drum_l[i] = drum_r[i] = bass_l[i] = bass_r[i] = synth_l[i] = synth_r[i] = rev[i] = gate[i] = dly[i] = 0.0f;
    unsigned pos = 0, guard = 0;
    while (pos < frames && guard++ < 64) {
        int n = (int)(frames - pos);
        n = el_transport_frames_until(t, n);
        n = dr_frames_until(&e->drums, n);
        for (unsigned i = 0; i < EL_TRACKS; ++i) n = st_frames_until(&e->track[i], n);
        render_sources(e, (int)pos, n);
        uint32_t step;
        if (el_transport_advance(t, n, &step)) {
            dr_step(&e->drums, step, t->last_length);
            for (unsigned i = 0; i < EL_TRACKS; ++i) st_step(&e->track[i], e->drums.current, t->last_length);
        }
        pos += (unsigned)n;
    }
    if (pos < frames) render_sources(e, (int)pos, (int)(frames - pos));
    /* Effect returns sit with the synths (high-passed, gently ducked). */
    fx_render(&e->fx, dly, rev, gate, synth_l, synth_r, (int)frames, t->bpm);
    mb_process(&e->mix, kick, drum_l, drum_r, bass_l, bass_r, synth_l, synth_r, left, right, (int)frames);
    ++e->blocks;
}

EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames)
{
    if (frames > EL_BLOCK) frames = EL_BLOCK;
    render_bus(e, frames);
    float g = e->master;
    for (unsigned i = 0; i < frames; ++i) {
        output[2 * i] = (int32_t)(el_clampf(g * left[i], -1.0f, 1.0f) * 8388607.0f);
        output[2 * i + 1] = (int32_t)(el_clampf(g * right[i], -1.0f, 1.0f) * 8388607.0f);
    }
}

EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames)
{
    if (frames > EL_BLOCK) frames = EL_BLOCK;
    render_bus(e, frames);
    float g = e->master;
    for (unsigned i = 0; i < frames; ++i) {
        output[2 * i] = g * left[i];
        output[2 * i + 1] = g * right[i];
    }
}
