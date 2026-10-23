/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "note_loop.h"
#include <string.h>

const uint8_t loop_bar_choices[LOOP_BAR_CHOICES] = {1, 2, 4, 8};
const char *const loop_state_names[LOOP_STATES] = {"EMPTY", "ARMED", "REC", "PLAY", "OVERDUB"};

void loop_init(note_loop_t *l)
{
    memset(l, 0, sizeof *l);
    l->bars = 2;                  /* 4 bars, like the apps' default take */
    l->sixteenth = EL_SAMPLE_RATE * 60.0f / (120.0f * 4.0f);
}

static EL_DSP_CODE unsigned length_steps(const note_loop_t *l) { return loop_bars(l) * 16u; }
static EL_DSP_CODE unsigned local_step(const note_loop_t *l) { return (l->step - l->origin) % length_steps(l); }
static EL_DSP_CODE int bit(const uint8_t *set, int note) { return (set[note >> 3] >> (note & 7)) & 1; }
static EL_DSP_CODE void put(uint8_t *set, int note, int on)
{
    if (on) set[note >> 3] |= (uint8_t)(1u << (note & 7));
    else set[note >> 3] &= (uint8_t)~(1u << (note & 7));
}

static EL_DSP_CODE unsigned position(const note_loop_t *l)
{
    int sub = l->sixteenth > 0 ? (int)(l->since * LOOP_UNITS / l->sixteenth + 0.001f) : 0;
    return local_step(l) * LOOP_UNITS + (unsigned)el_clampi(sub, 0, LOOP_UNITS - 1);
}

/* Events stay sorted by position; one recorded now is behind the player. */
static EL_DSP_CODE void insert(note_loop_t *l, unsigned pos, int note, int velocity)
{
    if (l->count >= LOOP_EVENTS) return;
    unsigned i = l->count;
    while (i > 0 && l->ev[i - 1].pos > pos) --i;
    memmove(&l->ev[i + 1], &l->ev[i], (l->count - i) * sizeof l->ev[0]);
    l->ev[i].pos = (uint16_t)pos;
    l->ev[i].note = (uint8_t)note;
    l->ev[i].velocity = (uint8_t)velocity;
    ++l->count;
    if (i <= l->next) ++l->next;
}

/* Notes still held when a take ends get their off at pos. */
static EL_DSP_CODE void close_held(note_loop_t *l, unsigned pos)
{
    for (int n = 0; n < 128; ++n)
        if (bit(l->recorded, n)) { insert(l, pos, n, 0); put(l->recorded, n, 0); }
}

EL_DSP_CODE void loop_silence(note_loop_t *l, loop_play_fn play, void *ctx)
{
    for (int n = 0; n < 128; ++n)
        if (bit(l->sounding, n)) { play(ctx, n, 0); put(l->sounding, n, 0); }
}

static EL_DSP_CODE void play_until(note_loop_t *l, unsigned pos, loop_play_fn play, void *ctx)
{
    if (l->state != LOOP_PLAYING && l->state != LOOP_OVERDUB) return;
    while (l->next < l->count && l->ev[l->next].pos <= pos) {
        const loop_event_t *e = &l->ev[l->next++];
        if (l->muted) continue;
        if (e->velocity) {
            if (bit(l->sounding, e->note)) play(ctx, e->note, 0);
            play(ctx, e->note, e->velocity);
            put(l->sounding, e->note, 1);
        } else if (bit(l->sounding, e->note)) {
            /* Only notes the loop started: a held key is never cut. */
            play(ctx, e->note, 0);
            put(l->sounding, e->note, 0);
        }
    }
}

/* First event after pos (the player continues from there). */
static EL_DSP_CODE void seek(note_loop_t *l, unsigned pos)
{
    l->next = 0;
    while (l->next < l->count && l->ev[l->next].pos <= pos) ++l->next;
}

static EL_DSP_CODE void finish_take(note_loop_t *l, unsigned off_pos)
{
    close_held(l, off_pos);
    l->state = l->count ? LOOP_PLAYING : LOOP_EMPTY;
}

EL_DSP_CODE void loop_command(note_loop_t *l, unsigned cmd, int value, loop_play_fn play, void *ctx)
{
    unsigned pos = l->clocked ? position(l) : 0;
    switch (cmd) {
    case LOOP_CMD_RECORD:
        switch (l->state) {
        case LOOP_EMPTY: l->state = LOOP_ARMED; break;
        case LOOP_ARMED: l->state = LOOP_EMPTY; break;
        case LOOP_RECORDING: finish_take(l, pos); seek(l, pos); break;   /* the rest of the bars stays empty */
        case LOOP_PLAYING: l->state = LOOP_OVERDUB; break;
        default: close_held(l, pos); l->state = LOOP_PLAYING; break;
        }
        break;
    case LOOP_CMD_CLEAR:
        loop_silence(l, play, ctx);
        l->count = l->next = 0;
        memset(l->recorded, 0, sizeof l->recorded);
        l->state = LOOP_EMPTY;
        break;
    case LOOP_CMD_BARS:
        /* The length is fixed once something is recorded. */
        if (l->state == LOOP_EMPTY || l->state == LOOP_ARMED) l->bars = (uint8_t)el_clampi(value, 0, LOOP_BAR_CHOICES - 1);
        break;
    case LOOP_CMD_PLAY:
        l->muted = value ? 0 : 1;
        if (l->muted) loop_silence(l, play, ctx);
        break;
    default:
        break;
    }
}

EL_DSP_CODE void loop_note(note_loop_t *l, int note, int velocity)
{
    if (!l->clocked || note < 0 || note > 127) return;
    if (l->state == LOOP_ARMED && velocity > 0) {
        /* The take starts on the bar of its first note, which keeps its
         * place in the bar. */
        l->origin = l->step - l->step % 16u;
        l->count = l->next = 0;
        memset(l->recorded, 0, sizeof l->recorded);
        l->state = LOOP_RECORDING;
    }
    if (l->state != LOOP_RECORDING && l->state != LOOP_OVERDUB) return;
    unsigned pos = position(l);
    if (velocity > 0) {
        if (l->count >= LOOP_EVENTS) return;
        if (bit(l->recorded, note)) insert(l, pos, note, 0);
        insert(l, pos, note, el_clampi(velocity, 1, 127));
        put(l->recorded, note, 1);
    } else if (bit(l->recorded, note)) {
        insert(l, pos, note, 0);
        put(l->recorded, note, 0);
    }
}

EL_DSP_CODE void loop_sync(note_loop_t *l, uint32_t step, float sixteenth_frames, loop_play_fn play, void *ctx)
{
    l->step = step;
    l->since = 0;
    l->sixteenth = sixteenth_frames;
    l->clocked = 1;
    if (l->state == LOOP_RECORDING && step - l->origin >= length_steps(l)) {
        /* All bars recorded: play on from this very step. */
        finish_take(l, length_steps(l) * LOOP_UNITS - 1u);
        l->next = 0;
    }
    if (local_step(l) == 0) l->next = 0;
    play_until(l, position(l), play, ctx);
}

EL_DSP_CODE int loop_frames_until(const note_loop_t *l, int limit)
{
    if (!l->clocked || (l->state != LOOP_PLAYING && l->state != LOOP_OVERDUB) || l->next >= l->count) return limit;
    unsigned base = local_step(l) * LOOP_UNITS, target = l->ev[l->next].pos;
    if (target >= base + LOOP_UNITS) return limit;   /* in a later 16th: loop_sync */
    float f = (float)(target - (target > base ? base : target)) * l->sixteenth / LOOP_UNITS - l->since;
    if (f >= (float)limit) return limit;
    int n = (int)f;
    if ((float)n < f) ++n;
    return n < 1 ? 1 : n;
}

EL_DSP_CODE void loop_advance(note_loop_t *l, int frames, loop_play_fn play, void *ctx)
{
    if (!l->clocked) return;
    l->since += (float)frames;
    play_until(l, position(l), play, ctx);
}

EL_DSP_CODE void loop_rewind(note_loop_t *l, loop_play_fn play, void *ctx)
{
    loop_silence(l, play, ctx);
    unsigned pos = l->clocked ? position(l) : 0;
    if (l->state == LOOP_RECORDING) finish_take(l, pos);
    else if (l->state == LOOP_OVERDUB) { close_held(l, pos); l->state = LOOP_PLAYING; }
    l->origin = 0;
    l->step = 0;
    l->since = 0;
    l->next = 0;
    l->clocked = 0;
}

float loop_progress(const note_loop_t *l)
{
    if (!l->clocked || l->state < LOOP_RECORDING) return 0;
    return (float)position(l) / (float)(length_steps(l) * LOOP_UNITS);
}
