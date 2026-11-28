/* SPDX-License-Identifier: GPL-3.0-only
 * jx8p: Adapter von engine/jx8p.c auf baustein.h (docs/adr/0046). Was er tut, steht in jx8p_bs.h. Der Kern bleibt
 * unverändert: dieselben Aufrufe wie FM1-Electros engine/synth.c und synth_track.c (Velocity / 127, Blöcke von
 * höchstens EL_BLOCK Samples, Akkord-Modus über engine/chords.c), daher bitgleich mit dem Gerätebuild auf dem
 * gleichen Rechner. */
#include "jx8p.h"
#include "jx8p_bs.h"
#include "jx8p_eigene.h"
#include <string.h>

#if !EL_SYNTH_JX
#error "mit -DEL_SYNTH_JX=1 bauen"
#endif

enum { HELD_CHORDS = 16 };

typedef struct {
    jx8p_part_t part;
    float bpm;
    double rate;
    float dly[EL_BLOCK], rev[EL_BLOCK];   /* Sends, wenn der Aufrufer sie nicht will */
    /* Akkord-Modus (KEY MODE 1): welche Akkordform zu welcher gehaltenen Taste gehört, damit note_off die richtigen
     * Noten loslässt, auch wenn CHORD dazwischen geändert wurde. */
    int16_t chord_root[HELD_CHORDS];
    uint8_t chord_type[HELD_CHORDS];
} jx8p_bs_t;

double jx8p_bs_rate(void) { return EL_SAMPLE_RATE; }

unsigned jx8p_bs_eigene_count(void) { return jx8p_eigene_count; }
unsigned jx8p_bs_preset_count(void) { return jx8p_eigene_count + jx8p_preset_count; }

static const jx8p_preset_t *preset(unsigned i)
{
    if (i < jx8p_eigene_count) return &jx8p_eigene[i];
    i -= jx8p_eigene_count;
    return i < jx8p_preset_count ? &jx8p_presets[i] : 0;
}

const char *jx8p_bs_preset_name(unsigned i)
{
    const jx8p_preset_t *p = preset(i);
    return p ? p->name : "";
}

const unsigned char *jx8p_bs_preset_values(unsigned i)
{
    const jx8p_preset_t *p = preset(i);
    return p ? p->v : 0;
}

void jx8p_bs_load_preset(void *state, unsigned i)
{
    const jx8p_preset_t *p = preset(i);
    if (p) jx8p_part_load(&((jx8p_bs_t *)state)->part, p->v);
}

void jx8p_bs_set_raw(void *state, unsigned param, int value) { jx8p_part_set(&((jx8p_bs_t *)state)->part, param, value); }
int jx8p_bs_get_raw(const void *state, unsigned param)
{
    return param < JX_COUNT ? ((const jx8p_bs_t *)state)->part.v[param] : 0;
}
void jx8p_bs_set_bpm(void *state, double bpm) { ((jx8p_bs_t *)state)->bpm = (float)bpm; }

static void init(void *state, double sample_rate)
{
    jx8p_bs_t *s = state;
    memset(s, 0, sizeof *s);
    s->rate = sample_rate;
    s->bpm = 120.0f;
    for (unsigned i = 0; i < HELD_CHORDS; ++i) s->chord_root[i] = -1;
    jx8p_part_init(&s->part, JX8P_BS_VOICES, 1u);
    if (jx8p_bs_preset_count()) jx8p_bs_load_preset(s, 0);
}

static const char *param_name(unsigned param) { return param < JX_COUNT ? jx8p_param_name(param) : ""; }

static void set_param(void *state, unsigned param, double value)
{
    if (param >= JX_COUNT) return;
    int max = jx8p_param_max(param);
    value = value < 0 ? 0 : value > 1 ? 1 : value;
    jx8p_bs_set_raw(state, param, (int)(value * max + 0.5));
}

static void chord_notes(jx8p_bs_t *s, int root, unsigned type, int on, float velocity)
{
    for (unsigned k = 0; k < MKS_CHORD_NOTES && mks_chords[type][k] != -128; ++k) {
        int n = root + mks_chords[type][k];
        if (n < 0 || n > 127) continue;
        if (on) jx8p_note_on(&s->part, n, velocity, 0);
        else jx8p_note_off(&s->part, n);
    }
}

static void note_on(void *state, int note, int velocity)
{
    jx8p_bs_t *s = state;
    float vel = (float)(velocity < 0 ? 0 : velocity > 127 ? 127 : velocity) / 127.0f;
    if (s->part.v[JX_KEY_MODE] != 1) { jx8p_note_on(&s->part, note, vel, 0); return; }
    unsigned type = s->part.v[JX_CHORD] < MKS_CHORDS ? s->part.v[JX_CHORD] : 0, slot = 0;
    while (slot < HELD_CHORDS && s->chord_root[slot] != note) ++slot;     /* dieselbe Taste noch gehalten? */
    if (slot < HELD_CHORDS) chord_notes(s, note, s->chord_type[slot], 0, 0);
    else {
        slot = 0;
        while (slot < HELD_CHORDS && s->chord_root[slot] >= 0) ++slot;   /* freie Zeile */
        if (slot == HELD_CHORDS) slot = 0;   /* voll: Zeile 0 überschreiben */
    }
    s->chord_root[slot] = (int16_t)note;
    s->chord_type[slot] = (uint8_t)type;
    chord_notes(s, note, type, 1, vel);
}

static void note_off(void *state, int note)
{
    jx8p_bs_t *s = state;
    for (unsigned i = 0; i < HELD_CHORDS; ++i)
        if (s->chord_root[i] == note) {
            chord_notes(s, note, s->chord_type[i], 0, 0);
            s->chord_root[i] = -1;
            return;
        }
    jx8p_note_off(&s->part, note);
}

void jx8p_bs_render_sends(void *state, float *left, float *right, float *delay, float *reverb, int n)
{
    jx8p_bs_t *s = state;
    for (int pos = 0; pos < n; pos += EL_BLOCK) {
        int k = n - pos < EL_BLOCK ? n - pos : EL_BLOCK;
        float *d = delay ? delay + pos : s->dly, *r = reverb ? reverb + pos : s->rev;
        if (!delay) memset(s->dly, 0, sizeof s->dly);   /* verworfen: nicht über die Blöcke aufsummieren */
        if (!reverb) memset(s->rev, 0, sizeof s->rev);
        jx8p_render(&s->part, left + pos, right + pos, d, r, k, s->bpm);
    }
}

static void render(void *state, float *left, float *right, int n)
{
    jx8p_bs_render_sends(state, left, right, 0, 0, n);
}

static int active(const void *state) { return jx8p_active(&((const jx8p_bs_t *)state)->part); }

const baustein_t jx8p_baustein = {
    "jx8p", "JX-8P (FM1-Electro)", sizeof(jx8p_bs_t), JX_COUNT, param_name,
    init, note_on, note_off, set_param, render, active,
};
