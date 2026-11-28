/* SPDX-License-Identifier: GPL-3.0-only
 * mks50: Adapter von engine/mks.c auf baustein.h (docs/adr/0047). Was er tut, steht in mks50_bs.h. Der Kern bleibt
 * unverändert: dieselben Aufrufe wie FM1-Electros engine/synth.c und synth_track.c (Velocity / 127, Blöcke von
 * höchstens EL_BLOCK Samples, Akkord-Modus über engine/chords.c), daher bitgleich mit engine/mks.c, wie FM1-Electro
 * ihn mit EL_SYNTH_JX=0 spielt (bis 0.5.5-alpha der Standard, ADR 0039). */
#include "mks.h"
#include "mks50_bs.h"
#include "mks50_eigene.h"
#include <string.h>

#if EL_SYNTH_JX
#error "mit -DEL_SYNTH_JX=0 bauen (engine/mks.c)"
#endif

enum { HELD_CHORDS = 16 };

typedef struct {
    mks_part_t part;
    float bpm;
    double rate;
    float dly[EL_BLOCK], rev[EL_BLOCK];   /* Sends, wenn der Aufrufer sie nicht will */
    /* Akkord-Modus (KEY MODE 1): welche Akkordform zu welcher gehaltenen Taste gehört, damit note_off die richtigen
     * Noten loslässt, auch wenn CHORD dazwischen geändert wurde. */
    int16_t chord_root[HELD_CHORDS];
    uint8_t chord_type[HELD_CHORDS];
} mks50_bs_t;

double mks50_bs_rate(void) { return EL_SAMPLE_RATE; }

unsigned mks50_bs_eigene_count(void) { return mks50_eigene_count; }
unsigned mks50_bs_preset_count(void) { return mks50_eigene_count + mks_preset_count; }

static const mks_preset_t *preset(unsigned i)
{
    if (i < mks50_eigene_count) return &mks50_eigene[i];
    i -= mks50_eigene_count;
    return i < mks_preset_count ? &mks_presets[i] : 0;
}

const char *mks50_bs_preset_name(unsigned i)
{
    const mks_preset_t *p = preset(i);
    return p ? p->name : "";
}

const unsigned char *mks50_bs_preset_values(unsigned i)
{
    const mks_preset_t *p = preset(i);
    return p ? p->v : 0;
}

void mks50_bs_load_preset(void *state, unsigned i)
{
    const mks_preset_t *p = preset(i);
    if (p) mks_part_load(&((mks50_bs_t *)state)->part, p->v);
}

void mks50_bs_set_raw(void *state, unsigned param, int value) { mks_part_set(&((mks50_bs_t *)state)->part, param, value); }
int mks50_bs_get_raw(const void *state, unsigned param)
{
    return param < MP_COUNT ? ((const mks50_bs_t *)state)->part.v[param] : 0;
}
void mks50_bs_set_bpm(void *state, double bpm) { ((mks50_bs_t *)state)->bpm = (float)bpm; }

size_t mks50_bs_state_size(void) { return sizeof(mks50_bs_t); }

void mks50_bs_init(void *state, double sample_rate)
{
    mks50_bs_t *s = state;
    memset(s, 0, sizeof *s);
    s->rate = sample_rate;
    s->bpm = 120.0f;
    for (unsigned i = 0; i < HELD_CHORDS; ++i) s->chord_root[i] = -1;
    mks_part_init(&s->part, MKS50_BS_VOICES, 1u);
    if (mks50_bs_preset_count()) mks50_bs_load_preset(s, 0);
}

static const char *param_name(unsigned param) { return param < MP_COUNT ? mks_param_name(param) : ""; }

static void set_param(void *state, unsigned param, double value)
{
    if (param >= MP_COUNT) return;
    int max = mks_param_max(param);
    value = value < 0 ? 0 : value > 1 ? 1 : value;
    mks50_bs_set_raw(state, param, (int)(value * max + 0.5));
}

static void chord_notes(mks50_bs_t *s, int root, unsigned type, int on, float velocity, int legato)
{
    for (unsigned k = 0; k < MKS_CHORD_NOTES && mks_chords[type][k] != -128; ++k) {
        int n = root + mks_chords[type][k];
        if (n < 0 || n > 127) continue;
        if (on) mks_note_on(&s->part, n, velocity, legato);
        else mks_note_off(&s->part, n);
    }
}

void mks50_bs_note_on(void *state, int note, int velocity, int legato)
{
    mks50_bs_t *s = state;
    float vel = (float)(velocity < 0 ? 0 : velocity > 127 ? 127 : velocity) / 127.0f;
    if (s->part.v[MP_KEY_MODE] != 1) { mks_note_on(&s->part, note, vel, legato); return; }
    unsigned type = s->part.v[MP_CHORD] < MKS_CHORDS ? s->part.v[MP_CHORD] : 0, slot = 0;
    while (slot < HELD_CHORDS && s->chord_root[slot] != note) ++slot;     /* dieselbe Taste noch gehalten? */
    if (slot < HELD_CHORDS) chord_notes(s, note, s->chord_type[slot], 0, 0, 0);
    else {
        slot = 0;
        while (slot < HELD_CHORDS && s->chord_root[slot] >= 0) ++slot;   /* freie Zeile */
        if (slot == HELD_CHORDS) slot = 0;   /* voll: Zeile 0 überschreiben */
    }
    s->chord_root[slot] = (int16_t)note;
    s->chord_type[slot] = (uint8_t)type;
    chord_notes(s, note, type, 1, vel, legato);
}

static void note_on(void *state, int note, int velocity) { mks50_bs_note_on(state, note, velocity, 0); }

void mks50_bs_note_off(void *state, int note)
{
    mks50_bs_t *s = state;
    for (unsigned i = 0; i < HELD_CHORDS; ++i)
        if (s->chord_root[i] == note) {
            chord_notes(s, note, s->chord_type[i], 0, 0, 0);
            s->chord_root[i] = -1;
            return;
        }
    mks_note_off(&s->part, note);
}

void mks50_bs_render_sends(void *state, float *left, float *right, float *delay, float *reverb, int n)
{
    mks50_bs_t *s = state;
    for (int pos = 0; pos < n; pos += EL_BLOCK) {
        int k = n - pos < EL_BLOCK ? n - pos : EL_BLOCK;
        float *d = delay ? delay + pos : s->dly, *r = reverb ? reverb + pos : s->rev;
        if (!delay) memset(s->dly, 0, sizeof s->dly);   /* verworfen: nicht über die Blöcke aufsummieren */
        if (!reverb) memset(s->rev, 0, sizeof s->rev);
        mks_render(&s->part, left + pos, right + pos, d, r, k, s->bpm);
    }
}

static void render(void *state, float *left, float *right, int n)
{
    mks50_bs_render_sends(state, left, right, 0, 0, n);
}

static int active(const void *state) { return mks_active(&((const mks50_bs_t *)state)->part); }

void mks50_bs_all_off(void *state)
{
    mks50_bs_t *s = state;
    mks_all_off(&s->part);
    for (unsigned i = 0; i < HELD_CHORDS; ++i) s->chord_root[i] = -1;
}

void mks50_bs_load_values(void *state, const unsigned char *v) { mks_part_load(&((mks50_bs_t *)state)->part, v); }

int mks50_bs_voices(const void *state)
{
    const mks50_bs_t *s = state;
    int n = 0;
    for (unsigned i = 0; i < s->part.voices; ++i) n += s->part.voice[i].active;
    return n;
}

const baustein_t mks50_baustein = {
    "mks50", "MKS-50 / Alpha Juno (FM1-Electro)", sizeof(mks50_bs_t), MP_COUNT, param_name,
    mks50_bs_init, note_on, mks50_bs_note_off, set_param, render, active,
};
