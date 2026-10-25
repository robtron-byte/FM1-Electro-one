/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "electro.h"
#include "dsp_math.h"
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
    e->dry_gain = e->stutter_gain = 1.0f;
    /* Stutter 1/16, tape stop over an 8th, delay 1/8, crusher
     * 8 bit, phaser once a bar. */
    static const uint8_t punch_init[EL_PUNCHES] = {3, 0, 1, 0, 1, 2, 2};
    memcpy(e->punch_div, punch_init, sizeof punch_init);
    static const uint8_t channels[EL_AREAS] = {9, 0, 1, 2};   /* drums 10, RHYTHM 1, STRINGS 2, BASS 3 */
    for (unsigned a = 0; a < EL_AREAS; ++a) e->midi_channel[a] = channels[a];
    e->option[EL_OPT_MIDI_CLOCK] = 1;
    e->option[EL_OPT_MASTER] = 1;
    e->option[EL_OPT_OUT_LEVEL] = 127;
    e->option[EL_OPT_START] = EL_START_LAST;
    e->option[EL_OPT_HINTS] = 1;
    /* The master compressor as it always was: -14 dB, 1.8:1, 25 / 220 ms, +4.5 dB. */
    e->option[EL_OPT_COMP_THRESHOLD] = 14;
    e->option[EL_OPT_COMP_RATIO] = 2;
    e->option[EL_OPT_COMP_ATTACK] = 4;
    e->option[EL_OPT_COMP_RELEASE] = 3;
    e->option[EL_OPT_COMP_MAKEUP] = 9;
    el_apply_comp(e);
}

const float el_comp_ratios[EL_COMP_RATIOS] = {1.0f, 1.4f, 1.8f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f};
const uint16_t el_comp_attacks[EL_COMP_ATTACKS] = {1, 3, 5, 10, 25, 50, 80, 120};
const uint16_t el_comp_releases[EL_COMP_RELEASES] = {50, 100, 150, 220, 300, 500, 800, 1200};

int el_option_max(unsigned option)
{
    switch (option) {
    case EL_OPT_OUT_LEVEL: return 127;
    case EL_OPT_COMP_THRESHOLD: return 30;
    case EL_OPT_COMP_RATIO: return EL_COMP_RATIOS - 1;
    case EL_OPT_COMP_ATTACK: return EL_COMP_ATTACKS - 1;
    case EL_OPT_COMP_RELEASE: return EL_COMP_RELEASES - 1;
    case EL_OPT_COMP_MAKEUP: return 24;
    case EL_OPT_REC_STEP: return (1 << EL_TRACKS) - 1;
    case EL_OPT_DIST_KICK_FULL: return 1;
    default: return 1;
    }
}

void el_apply_comp(el_engine_t *e)
{
    const volatile uint8_t *o = e->option;
    mb_master_comp(&e->mix, -(float)o[EL_OPT_COMP_THRESHOLD], el_comp_ratios[o[EL_OPT_COMP_RATIO] % EL_COMP_RATIOS],
                   el_comp_attacks[o[EL_OPT_COMP_ATTACK] % EL_COMP_ATTACKS], el_comp_releases[o[EL_OPT_COMP_RELEASE] % EL_COMP_RELEASES],
                   0.5f * o[EL_OPT_COMP_MAKEUP]);
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
    /* Slot A of every area from the drums' style; B-D stay empty. */
    st_pattern_t parts[EL_TRACKS];
    el_generate_parts(&e->drums.pattern[0], mix_seed(seed + 0x632be5abu) ^ 0x5bd1e995u, parts);
    for (unsigned i = 0; i < EL_TRACKS; ++i) e->track[i].pattern[0] = parts[i];
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

int el_load_preset(el_engine_t *e, unsigned track, unsigned preset)
{
    if (preset >= syn_sound_count()) return -1;
    return el_load_sound(e, track, preset, syn_sound_engine(preset), syn_sound_values(preset));
}

int el_load_sound(el_engine_t *e, unsigned track, unsigned id, unsigned engine, const uint8_t *values)
{
    if (track >= EL_TRACKS || id > 255 || engine >= SYN_ENGINES || !values) return -1;
    if (e->staged_posted != e->staged_taken) return -2;
    unsigned n = syn_value_count(engine);
    for (unsigned i = 0; i < n; ++i) e->staged_patch[i] = values[i];
    EL_FENCE();
    if (el_post3(e, EL_CMD_TRACK_PRESET, (uint8_t)track, (uint8_t)id, (uint8_t)engine, 0)) return -1;
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
    if (m[0] >= 0xf8u && m[0] <= 0xfcu && !e->option[EL_OPT_MIDI_CLOCK]) return 0;   /* OPTIONS: clock ignored */
    switch (m[0]) {
    case 0xf8: cmd.type = EL_CMD_MIDI_TICK; break;
    case 0xfa: cmd.type = EL_CMD_START; cmd.c = 1; break;
    case 0xfb: cmd.type = EL_CMD_CONTINUE; cmd.c = 1; break;
    case 0xfc: cmd.type = EL_CMD_STOP; break;
    default: {
        /* Multitimbral: each area on its own channel (EL_CMD_MIDI_CHANNEL). */
        unsigned status = m[0] & 0xf0u, channel = m[0] & 15u;
        if (n < 3 || (status != 0x90u && status != 0x80u)) return 0;
        int pushed = 0, failed = 0;
        if (status == 0x90u && m[2] && e->midi_channel[EL_AREA_BEAT] == channel) {
            int part = dr_note_part(m[1]);
            if (part >= 0) {
                el_cmd_t hit = {EL_CMD_DRUM_HIT, (uint8_t)part, 0, 0, m[2]};
                failed |= el_queue_push(&e->midi_queue, hit) != 0;
                pushed = 1;
            }
        }
        for (unsigned t = 0; t < EL_TRACKS; ++t) {
            if (e->midi_channel[t + 1u] != channel) continue;
            el_cmd_t note = {EL_CMD_NOTE, (uint8_t)t, (uint8_t)(m[1] & 127u), 0,
                             status == 0x90u ? (int16_t)(m[2] & 127u) : 0};
            failed |= el_queue_push(&e->midi_queue, note) != 0;
            pushed = 1;
        }
        return pushed && !failed;
    }
    }
    return el_queue_push(&e->midi_queue, cmd) == 0;
}

static EL_DSP_CODE void apply_track(el_engine_t *e, const el_cmd_t *c)
{
    st_track_t *t = &e->track[c->a];
    st_pattern_t *p = &t->pattern[t->current];
    st_step_t *s = c->b < ST_STEPS ? &p->step[c->b] : &p->step[0];
    switch (c->type) {
    case EL_CMD_NOTE: {
        uint8_t armed = t->recording;
        if (c->c) t->recording = 0;   /* step record listens only */
        st_live(t, c->b, el_clampi(c->value, 0, 127) / 127.0f);
        t->recording = armed;
        break;
    }
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
    case EL_CMD_STEP_WRITE:
        s->note = c->c;
        s->flags = (uint8_t)(c->value & 0xff);
        s->chord = (uint8_t)((c->value >> 8) & 0x3f);
        if (c->type == EL_CMD_STEP_WRITE) p->length = (uint8_t)el_clampi(c->b + 1, 1, ST_STEPS);
        break;
    case EL_CMD_STEP_TIE:
        s->flags = (uint8_t)(c->c ? s->flags | ST_TIE : s->flags & ~ST_TIE);
        break;
    case EL_CMD_TRACK_TRANSPOSE:
        t->transpose = (int8_t)el_clampi(c->value, -36, 36);
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
    case EL_CMD_TRACK_ARP:
        st_arp_set(t, c->b, c->value);
        break;
    default:
        return;
    }
    ++t->revision;
}

static EL_DSP_CODE void apply(el_engine_t *e, const el_cmd_t *c)
{
    el_transport_t *t = &e->transport;
    if (c->type >= EL_CMD_NOTE && c->type <= EL_CMD_TRACK_ARP) {
        if (c->a < EL_TRACKS) apply_track(e, c);
        return;
    }
    switch (c->type) {
    case EL_CMD_FX:
        if (c->a < FX_PARAMS) e->fx.v[c->a] = (uint8_t)el_clampi(c->value, 0, fx_param_max(c->a));
        break;
    case EL_CMD_MIDI_CHANNEL:
        if (c->a < EL_AREAS) e->midi_channel[c->a] = (uint8_t)el_clampi(c->value, 0, EL_MIDI_OFF);
        break;
    case EL_CMD_PUNCH_RATE:
        for (unsigned n = 0; n < EL_PUNCHES; ++n)
            if ((c->a & (1u << n)) && el_punch_value_name(n, c->b)) e->punch_div[n] = c->b;
        break;
    case EL_CMD_PUNCH:
        /* Repeat starts on the step that comes next. */
        if (c->b && (c->a & EL_PUNCH_REPEAT) && !(e->punch & EL_PUNCH_REPEAT))
            e->punch_div[3] = (uint8_t)((e->sync_step + 1u) % EL_REPEAT_STEPS);
        e->punch = (uint8_t)(c->b ? e->punch | c->a : e->punch & ~c->a);
        if (c->a & EL_PUNCH_THROW) {
            e->fx.throw_on = c->b ? 1 : 0;
            if (c->b) e->throw_fed = 0;
        }
        break;
    case EL_CMD_OPTION:
        if (c->a >= EL_OPTIONS) break;
        e->option[c->a] = (uint8_t)el_clampi(c->value, 0, el_option_max(c->a));
        if (c->a >= EL_OPT_COMP_THRESHOLD && c->a <= EL_OPT_COMP_MAKEUP) el_apply_comp(e);
        if (c->a == EL_OPT_MASTER) e->mix.enabled = e->option[c->a] ? 1 : 0;
        if (c->a == EL_OPT_OUT_LEVEL) e->master = e->option[c->a] / 127.0f;
        if (c->a == EL_OPT_MIDI_CLOCK && !e->option[c->a]) t->source = EL_CLOCK_INTERNAL;
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
    case EL_CMD_TRACK_SELECT:
        if (c->a < EL_TRACKS && c->b < ST_PATTERNS) {
            st_track_t *tr = &e->track[c->a];
            tr->next = c->b;
            if (c->c || !t->running) { tr->current = c->b; tr->playing = 0; }
            ++tr->revision;
        }
        break;
    default:
        break;
    }
}

/* Buses, static so the audio interrupt's stack stays small. */
static float kick[EL_BLOCK], drum_l[EL_BLOCK], drum_r[EL_BLOCK], bass_l[EL_BLOCK], bass_r[EL_BLOCK];
static float synth_l[EL_BLOCK], synth_r[EL_BLOCK], rev[EL_BLOCK], gate[EL_BLOCK], dly[EL_BLOCK];
static float left[EL_BLOCK], right[EL_BLOCK];
static float rhythm_l[EL_BLOCK], rhythm_r[EL_BLOCK], dist_send[EL_BLOCK];

static EL_DSP_CODE void render_sources(el_engine_t *e, int pos, int n)
{
    el_transport_t *t = &e->transport;
    dr_render(&e->drums, kick + pos, drum_l + pos, drum_r + pos, rev + pos, gate + pos, dly + pos, n);
    /* RHYTHM on its own pair until its distortion send is taken. */
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        float *l = i == EL_AREA_BASS - 1u ? bass_l : i == EL_AREA_RHYTHM - 1u ? rhythm_l : synth_l;
        float *r = i == EL_AREA_BASS - 1u ? bass_r : i == EL_AREA_RHYTHM - 1u ? rhythm_r : synth_r;
        st_render(&e->track[i], l + pos, r + pos, dly + pos, rev + pos, n, t->bpm);
    }
}

/* Note values of the punch-ins (white keys while one is held): length in
 * 16ths, and every how many steps their pattern starts again. */
const char *const el_punch_div_names[EL_PUNCH_DIVS] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const float div_16ths[EL_PUNCH_DIVS] = {4.0f, 2.0f, 4.0f / 3.0f, 1.0f, 2.0f / 3.0f, 0.5f};
static const uint8_t div_reset[EL_PUNCH_DIVS] = {4, 2, 4, 1, 2, 1};
static const char *const crush_names[EL_PUNCH_DIVS] = {"12 BIT", "10 BIT", "8 BIT", "6 BIT", "5 BIT", "4 BIT"};
static const char *const phaser_names[EL_PUNCH_DIVS] = {"1/4", "1/2", "1 BAR", "2 BARS", "4 BARS", "8 BARS"};
static const char *const tape_names[EL_PUNCH_DIVS] = {"1/16", "1/8", "3/16", "1/4", "3/8", "1/2"};
static const float tape_16ths[EL_PUNCH_DIVS] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
static const char *const step_names[EL_REPEAT_STEPS] = {"STEP 1", "STEP 2", "STEP 3", "STEP 4", "STEP 5", "STEP 6",
    "STEP 7", "STEP 8", "STEP 9", "STEP 10", "STEP 11", "STEP 12", "STEP 13", "STEP 14", "STEP 15", "STEP 16"};

const char *el_punch_value_name(unsigned n, unsigned v)
{
    if (n >= EL_PUNCHES || (1u << n) == EL_PUNCH_THROW) return NULL;
    if ((1u << n) == EL_PUNCH_REPEAT) return v < EL_REPEAT_STEPS ? step_names[v] : NULL;
    if (v >= EL_PUNCH_DIVS) return NULL;
    if ((1u << n) == EL_PUNCH_CRUSH) return crush_names[v];
    if ((1u << n) == EL_PUNCH_PHASER) return phaser_names[v];
    if ((1u << n) == EL_PUNCH_TAPE) return tape_names[v];
    return el_punch_div_names[v];
}

/* A step fired in this block at frame stutter_sync: the 16th grid. */
static EL_DSP_CODE int on_grid(const el_engine_t *e, unsigned i, unsigned div)
{
    return (int)i == e->stutter_sync && e->sync_step % div_reset[div] == 0;
}

/* Reverb throw (held): the dry mix (and its sends) fades out slowly, over
 * two beats, and while it fades it feeds the big hall (without the kick,
 * which would thump in a big room; darkened to about 2.5 kHz). Then only
 * the hall stays. Let go: the dry mix is back in 20 ms, the hall rings out
 * (fx.c). */
static EL_DSP_CODE void punch_throw(el_engine_t *e, unsigned frames)
{
    int on = (e->punch & EL_PUNCH_THROW) != 0;
    if (!on && e->dry_gain >= 1.0f) return;
    float beat = 60.0f / dsp_max(e->transport.bpm, 30.0f) * EL_SAMPLE_RATE;
    float down = 1.0f / (2.0f * beat), up = 1.0f / (0.02f * EL_SAMPLE_RATE);
    for (unsigned i = 0; i < frames; ++i) {
        e->dry_gain = dsp_clamp(e->dry_gain + (on ? -down : up), 0.0f, 1.0f);
        float g = e->dry_gain * e->dry_gain;   /* slow at first, then away */
        float mix = 0.5f * (drum_l[i] + drum_r[i] + 0.5f * (bass_l[i] + bass_r[i]) + synth_l[i] + synth_r[i] +
                            rhythm_l[i] + rhythm_r[i]);
        e->throw_lp += (mix - e->throw_lp) * 0.3f;   /* one pole, about 2.5 kHz */
        kick[i] *= g; drum_l[i] *= g; drum_r[i] *= g; bass_l[i] *= g; bass_r[i] *= g;
        synth_l[i] *= g; synth_r[i] *= g; rhythm_l[i] *= g; rhythm_r[i] *= g;
        dly[i] *= g; gate[i] *= g;
        rev[i] = rev[i] * g + (on ? e->throw_lp * e->dry_gain * 1.8f : 0.0f);
    }
}

/* Stutter (held): the finished mix gated at the chosen note value (1/16
 * unless a white key chose another), open for the first half of each, with
 * 1.5 ms edges, on the grid of the steps. */
static EL_DSP_CODE void punch_stutter(el_engine_t *e, unsigned frames)
{
    unsigned which = e->punch & EL_PUNCH_STUTTER, div = e->punch_div[0];
    if (!which && e->stutter_gain >= 1.0f) return;
    float sixteenth = 15.0f / dsp_max(e->transport.bpm, 30.0f) * EL_SAMPLE_RATE;
    float rate = 1.0f / (sixteenth * div_16ths[div]), edge = 1.0f / (0.0015f * EL_SAMPLE_RATE);
    for (unsigned i = 0; i < frames; ++i) {
        if (on_grid(e, i, div)) e->stutter_phase = 0.0f;
        e->stutter_phase += rate;
        if (e->stutter_phase >= 1.0f) e->stutter_phase -= 1.0f;
        float target = !which || e->stutter_phase < 0.5f ? 1.0f : 0.0f;
        e->stutter_gain += dsp_clamp(target - e->stutter_gain, -edge, edge);
        left[i] *= e->stutter_gain;
        right[i] *= e->stutter_gain;
    }
}

static EL_DSP_CODE void put(int16_t *m, uint32_t i, float x) { m[i] = (int16_t)(dsp_clamp(x, -1.0f, 1.0f) * 32767.0f); }

/* Tape stop (held): the mix (mono, at half rate in its buffer region) plays
 * at a speed falling from 1 to 0 over the chosen length (white keys: 1/8 ..
 * 4 bars, an 8th unless chosen), pitch and tempo dropping together. The
 * region holds a quarter of a second of lag, enough for a real stop up to
 * about half a second; on longer stops, once the lag is that far behind, the
 * playback slips forward half the region in a 25 ms cross-fade, the pitch
 * still falling. It takes whatever comes before it, the other punch-ins
 * included. Let go: live again in 10 ms. */
static EL_DSP_CODE float tape_read(const int16_t *m, float rp, uint32_t cap)
{
    uint32_t k = (uint32_t)rp, k1 = k + 1u < cap ? k + 1u : 0u;
    return (m[k] + (m[k1] - m[k]) * (rp - (float)k)) * (1.0f / 32767.0f);
}

static EL_DSP_CODE void punch_tape(el_engine_t *e, unsigned frames)
{
    el_fx_t *fx = &e->fx;
    int held = (e->punch & EL_PUNCH_TAPE) != 0;
    const uint32_t cap = FX_TAPE_REGION;
    if (!held && !(fx->users & FX_USE_TAPE)) return;
    if (held && !(fx->users & FX_USE_TAPE)) {
        fx_borrow(fx, FX_USE_TAPE);
        e->tape_mix = 0;
        e->tape_wp = 0;
        e->tape_half = 0;
        e->tape_rp = (float)cap - 1.5f;   /* just behind what is written */
        e->tape_xf = -1.0f;
        e->tape_t = 0;
        e->tape_rate = 1;
    }
    int16_t *m = fx_region(fx, FX_USE_TAPE);
    float sixteenth = 15.0f / dsp_max(e->transport.bpm, 30.0f) * EL_SAMPLE_RATE, xf = 1.0f / (0.01f * EL_SAMPLE_RATE);
    float stop_len = dsp_clamp(sixteenth * tape_16ths[e->punch_div[2]], 0.04f * EL_SAMPLE_RATE, 20.0f * EL_SAMPLE_RATE);
    const float slip_at = (float)cap - 800.0f, slip_xf = 1.0f / (0.025f * EL_SAMPLE_RATE);
    for (unsigned i = 0; i < frames; ++i) {
        float l = left[i], r = right[i];
        e->tape_acc += 0.5f * (l + r);
        e->tape_half ^= 1u;
        if (!e->tape_half) {
            put(m, e->tape_wp, 0.5f * e->tape_acc);
            e->tape_acc = 0.0f;
            e->tape_wp = e->tape_wp + 1u < cap ? e->tape_wp + 1u : 0u;
        }
        if (held) {
            e->tape_t += 1.0f;
            e->tape_rate = dsp_max(0.0f, 1.0f - e->tape_t / stop_len);
            e->tape_mix = dsp_min(1.0f, e->tape_mix + xf);
        } else e->tape_mix = dsp_max(0.0f, e->tape_mix - xf);
        float lag = (float)e->tape_wp + 0.5f * e->tape_half - e->tape_rp;
        if (lag < 0.0f) lag += (float)cap;
        if (e->tape_xf < 0.0f && lag > slip_at) {
            /* A second head half the region further on takes over. */
            e->tape_rp2 = e->tape_rp + 0.5f * (float)cap;
            if (e->tape_rp2 >= (float)cap) e->tape_rp2 -= (float)cap;
            e->tape_xf = 0.0f;
        }
        float y = tape_read(m, e->tape_rp, cap), step = 0.5f * e->tape_rate;
        e->tape_rp += step;
        if (e->tape_rp >= (float)cap) e->tape_rp -= (float)cap;
        if (e->tape_xf >= 0.0f) {
            float g = e->tape_xf;
            y = y * dsp_sqrt(1.0f - g) + tape_read(m, e->tape_rp2, cap) * dsp_sqrt(g);
            e->tape_rp2 += step;
            if (e->tape_rp2 >= (float)cap) e->tape_rp2 -= (float)cap;
            e->tape_xf += slip_xf;
            if (e->tape_xf >= 1.0f) { e->tape_rp = e->tape_rp2; e->tape_xf = -1.0f; }
        }
        y *= dsp_min(1.0f, e->tape_rate * 6.0f);   /* the last moments fade: no click at standstill */
        left[i] = l + (y - l) * e->tape_mix;
        right[i] = r + (y - r) * e->tape_mix;
    }
    if (!held && e->tape_mix <= 0.0f) fx_release(fx, FX_USE_TAPE);
}

/* Delay (held): the mix goes into a dub echo (mono, at half rate in its
 * region) at the chosen note value (1/8 unless a white key chose another;
 * at most half a second), feedback 72 %, darkened and thinned on every
 * round. A new note value glides there like a tape echo. Let go: nothing
 * more goes in, the echoes ring out; the region is given back once they are
 * gone. */
static EL_DSP_CODE void punch_delay(el_engine_t *e, unsigned frames)
{
    el_fx_t *fx = &e->fx;
    int held = (e->punch & EL_PUNCH_DELAY) != 0;
    const uint32_t cap = FX_DELAY_REGION;
    float sixteenth = 15.0f / dsp_max(e->transport.bpm, 30.0f) * EL_SAMPLE_RATE;
    float target = dsp_clamp(0.5f * sixteenth * div_16ths[e->punch_div[4]], 64.0f, (float)(cap - 4u));
    if (!held && !(fx->users & FX_USE_DELAY)) return;
    if (held && !(fx->users & FX_USE_DELAY)) {
        fx_borrow(fx, FX_USE_DELAY);
        e->dl_wp = 0;
        e->dl_half = 0;
        e->dl_in = e->dl_acc = e->dl_lp = e->dl_hp = e->dl_hpx = e->dl_env = 0.0f;
        e->dl_time = target;
    }
    int16_t *m = fx_region(fx, FX_USE_DELAY);
    const float ramp = 1.0f / (0.005f * EL_SAMPLE_RATE), lp = 0.45f, hp = 0.979f;   /* about 4.5 kHz and 150 Hz */
    for (unsigned i = 0; i < frames; ++i) {
        e->dl_time += (target - e->dl_time) * 0.0004f;
        float pos = (float)e->dl_wp + (e->dl_half ? 0.5f : 0.0f) - e->dl_time;
        if (pos < 0.0f) pos += (float)cap;
        uint32_t k = (uint32_t)pos, k1 = k + 1u < cap ? k + 1u : 0u;
        float f = pos - (float)k, y = (m[k] + (m[k1] - m[k]) * f) * (1.0f / 32767.0f);
        e->dl_in = held ? dsp_min(1.0f, e->dl_in + ramp) : dsp_max(0.0f, e->dl_in - ramp);
        float x = e->dl_in * 0.5f * (left[i] + right[i]) + 0.72f * y;
        e->dl_lp += (x - e->dl_lp) * lp;
        float h = hp * (e->dl_hp + e->dl_lp - e->dl_hpx);
        e->dl_hpx = e->dl_lp;
        e->dl_hp = h;
        e->dl_acc += h;
        e->dl_half ^= 1u;
        if (!e->dl_half) {
            put(m, e->dl_wp, dsp_tanh(0.5f * e->dl_acc));
            e->dl_acc = 0.0f;
            e->dl_wp = e->dl_wp + 1u < cap ? e->dl_wp + 1u : 0u;
        }
        left[i] += 0.8f * y;
        right[i] += 0.8f * y;
        e->dl_env = dsp_max(dsp_abs(y), e->dl_env * 0.9999f);
    }
    if (!held && e->dl_in <= 0.0f && e->dl_env < 0.0002f) fx_release(fx, FX_USE_DELAY);
}

/* Bitcrusher (held): fewer bits and a lower rate (white keys: 12 bit and
 * every sample .. 4 bit and every 8th), 5 ms in and out. */
static EL_DSP_CODE void punch_crush(el_engine_t *e, unsigned frames)
{
    static const float levels[EL_PUNCH_DIVS] = {2048.0f, 512.0f, 128.0f, 32.0f, 16.0f, 8.0f};
    static const uint8_t hold[EL_PUNCH_DIVS] = {1, 2, 3, 4, 6, 8};
    int held = (e->punch & EL_PUNCH_CRUSH) != 0;
    if (!held && e->crush_mix <= 0.0f) return;
    unsigned v = e->punch_div[5];
    float q = levels[v], ramp = 1.0f / (0.005f * EL_SAMPLE_RATE);
    for (unsigned i = 0; i < frames; ++i) {
        if (++e->crush_n >= hold[v]) {
            e->crush_n = 0;
            for (unsigned c = 0; c < 2; ++c) {
                float x = dsp_clamp(c ? right[i] : left[i], -1.0f, 1.0f) * q;
                e->crush_hold[c] = (float)(int32_t)(x + (x >= 0.0f ? 0.5f : -0.5f)) / q;
            }
        }
        e->crush_mix = held ? dsp_min(1.0f, e->crush_mix + ramp) : dsp_max(0.0f, e->crush_mix - ramp);
        left[i] += (e->crush_hold[0] - left[i]) * e->crush_mix;
        right[i] += (e->crush_hold[1] - right[i]) * e->crush_mix;
    }
}

/* Phaser (held): six all-passes per side swept between 200 Hz and 3 kHz
 * once per chosen length (white keys: 1/4 .. 8 bars, a bar unless chosen),
 * the sides a quarter turn apart, some feedback. It starts at the bottom of
 * its sweep; 10 ms in and out. */
static EL_DSP_CODE void punch_phaser(el_engine_t *e, unsigned frames)
{
    static const float lengths[EL_PUNCH_DIVS] = {4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f};
    int held = (e->punch & EL_PUNCH_PHASER) != 0;
    if (!held && e->ph_mix <= 0.0f) return;
    if (held && e->ph_mix <= 0.0f) {
        e->ph_phase = 0.0f;
        memset(e->ph_z, 0, sizeof e->ph_z);
        e->ph_fb[0] = e->ph_fb[1] = 0.0f;
    }
    float sixteenth = 15.0f / dsp_max(e->transport.bpm, 30.0f) * EL_SAMPLE_RATE;
    float rate = 1.0f / (sixteenth * lengths[e->punch_div[6]]), ramp = 1.0f / (0.01f * EL_SAMPLE_RATE);
    for (unsigned i = 0; i < frames; ++i) {
        if ((i & 15u) == 0) {
            for (unsigned c = 0; c < 2; ++c) {
                float ph = e->ph_phase + (c ? 0.25f : 0.0f);
                float tri = 0.5f - 0.5f * dsp_cos(2.0f * DSP_PI * ph);   /* 0 at the bottom */
                float fc = 200.0f * dsp_exp2(tri * 3.9f);                /* 200 Hz .. 3 kHz */
                float t = dsp_tan(DSP_PI * fc / EL_SAMPLE_RATE);
                e->ph_a[c] = (t - 1.0f) / (t + 1.0f);
            }
        }
        e->ph_phase += rate;
        if (e->ph_phase >= 1.0f) e->ph_phase -= 1.0f;
        e->ph_mix = held ? dsp_min(1.0f, e->ph_mix + ramp) : dsp_max(0.0f, e->ph_mix - ramp);
        for (unsigned c = 0; c < 2; ++c) {
            float x = c ? right[i] : left[i], a = e->ph_a[c], y = x + 0.5f * e->ph_fb[c];
            for (unsigned s = 0; s < 6; ++s) {
                float o = a * y + e->ph_z[c][s];
                e->ph_z[c][s] = y - a * o;
                y = o;
            }
            e->ph_fb[c] = y;
            float out = 0.5f * (x + y);
            if (c) right[i] = x + (out - x) * e->ph_mix;
            else left[i] = x + (out - x) * e->ph_mix;
        }
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
        kick[i] = drum_l[i] = drum_r[i] = bass_l[i] = bass_r[i] = synth_l[i] = synth_r[i] = rev[i] = gate[i] = dly[i] =
            rhythm_l[i] = rhythm_r[i] = 0.0f;
    unsigned pos = 0, guard = 0;
    e->stutter_sync = -1;
    while (pos < frames && guard++ < 64) {
        int n = (int)(frames - pos);
        n = el_transport_frames_until(t, n);
        n = dr_frames_until(&e->drums, n);
        for (unsigned i = 0; i < EL_TRACKS; ++i) n = st_frames_until(&e->track[i], n);
        render_sources(e, (int)pos, n);
        uint32_t step;
        if (el_transport_advance(t, n, &step)) {
            e->stutter_sync = (int)(pos + (unsigned)n);   /* the punch-ins' grid starts on the steps */
            e->sync_step = step;
            if (e->punch & EL_PUNCH_REPEAT) {
                /* Repeat (held): every 16th the chosen step of the bar (white
                 * keys 1-16, else the one after the press) plays again in
                 * every area, the playheads going on underneath. */
                unsigned target = e->punch_div[3];
                dr_step_at(&e->drums, step, t->last_length, target);
                for (unsigned i = 0; i < EL_TRACKS; ++i) st_step_at(&e->track[i], step, t->last_length, target);
            } else {
                dr_step(&e->drums, step, t->last_length);
                for (unsigned i = 0; i < EL_TRACKS; ++i) st_step(&e->track[i], step, t->last_length);
            }
        }
        pos += (unsigned)n;
    }
    if (pos < frames) render_sources(e, (int)pos, (int)(frames - pos));
    punch_throw(e, frames);
    /* Distortion send per area (mono), its return on the drum bus. */
    const uint8_t *fv = e->fx.v;
    float sb = fv[FX_DIST_SEND_BEAT] / 127.0f, sr = fv[FX_DIST_SEND_RHYTHM] / 127.0f;
    float ss = fv[FX_DIST_SEND_STRINGS] / 127.0f, sa = fv[FX_DIST_SEND_BASS] / 127.0f;
    sb *= sb; sr *= sr; ss *= ss; sa *= sa;
    /* OPTIONS DIST BASS DRUM: FULL LOWS. Each type cuts below f_t (one
     * pole) before its drive, which leaves little of an 808 kick (a sine
     * around 50 Hz). With FULL LOWS the kick's send gets 1 + (f_t / 25 Hz
     * - 1) x its 25 Hz low-pass, so it reaches the drive as if cut only
     * below 25 Hz. By default (PEDAL) it goes in like the rest. */
    float ft = dist_tight_hz(fv[FX_DIST_MODE]), kick_boost = e->option[EL_OPT_DIST_KICK_FULL] ? ft / 25.0f - 1.0f : 0.0f;
    const float kick_lp = 1.0f - 0.99644f;   /* 25 Hz one pole */
    for (unsigned i = 0; i < frames; ++i) {
        e->kick_low += (kick[i] - e->kick_low) * kick_lp;
        float kick_send = kick[i] + kick_boost * e->kick_low;
        dist_send[i] = sb * (kick_send + 0.5f * (drum_l[i] + drum_r[i])) + sr * 0.5f * (rhythm_l[i] + rhythm_r[i]) +
                       ss * 0.5f * (synth_l[i] + synth_r[i]) + sa * 0.5f * (bass_l[i] + bass_r[i]);
        synth_l[i] += rhythm_l[i];
        synth_r[i] += rhythm_r[i];
    }
    fx_distortion(&e->fx, dist_send, drum_l, drum_r, (int)frames);
    /* Effect returns sit with the synths (high-passed, gently ducked). */
    fx_render(&e->fx, dly, rev, gate, synth_l, synth_r, (int)frames, t->bpm);
    mb_process(&e->mix, kick, drum_l, drum_r, bass_l, bass_r, synth_l, synth_r, left, right, (int)frames);
    punch_delay(e, frames);
    punch_crush(e, frames);
    punch_phaser(e, frames);
    punch_stutter(e, frames);
    punch_tape(e, frames);
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
