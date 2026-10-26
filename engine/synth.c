/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: the constants of this file sit in internal SRAM; its audio
 * code is marked EL_DSP_CODE / EL_DSP_HELPER (control code runs from Flash). */
#ifndef FM1_HOST_TEST
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "synth.h"
#include <string.h>

const char *const syn_engine_names[SYN_ENGINES] = {"MKS-50", "JP-6"};

unsigned syn_sound_count(void) { return mks_preset_count + jp6_preset_count; }
unsigned syn_sound_engine(unsigned s) { return s < mks_preset_count ? SYN_MKS : SYN_JP6; }

unsigned syn_sound_area(unsigned s)
{
    if (s < mks_preset_count) return mks_presets[s].area;
    s -= mks_preset_count;
    return s < jp6_preset_count ? jp6_presets[s].area : 0;
}

const char *syn_sound_name(unsigned s)
{
    if (s < mks_preset_count) return mks_presets[s].name;
    s -= mks_preset_count;
    return s < jp6_preset_count ? jp6_presets[s].name : "?";
}

const uint8_t *syn_sound_values(unsigned s)
{
    if (s < mks_preset_count) return mks_presets[s].v;
    s -= mks_preset_count;
    return jp6_presets[s < jp6_preset_count ? s : 0].v;
}

int syn_sound_find(unsigned engine, const char *name)
{
    for (unsigned s = 0; s < syn_sound_count(); ++s)
        if (syn_sound_engine(s) == engine && !strcmp(syn_sound_name(s), name)) return (int)s;
    return -1;
}

void syn_init(syn_part_t *p, unsigned engine, unsigned voices, uint32_t seed)
{
    memset(p, 0, sizeof *p);
    p->engine = (uint8_t)(engine == SYN_JP6 ? SYN_JP6 : SYN_MKS);
    if (p->engine == SYN_JP6) jp6_part_init(&p->u.jp6, voices, seed);
    else mks_part_init(&p->u.mks, voices, seed);
}

EL_DSP_CODE void syn_load(syn_part_t *p, unsigned engine, const uint8_t *v)
{
    engine = engine == SYN_JP6 ? SYN_JP6 : SYN_MKS;
    if (engine != p->engine) {
        /* Same voice budget and a fresh seed from the old part's state. */
        unsigned voices = p->engine == SYN_JP6 ? p->u.jp6.voices : p->u.mks.voices;
        uint32_t seed = p->engine == SYN_JP6 ? p->u.jp6.age * 2654435761u + 1u : p->u.mks.age * 2654435761u + 1u;
        p->engine = (uint8_t)engine;
        if (engine == SYN_JP6) jp6_part_init(&p->u.jp6, voices, seed);
        else mks_part_init(&p->u.mks, voices, seed);
    }
    if (engine == SYN_JP6) jp6_part_load(&p->u.jp6, v);
    else mks_part_load(&p->u.mks, v);
}

EL_DSP_CODE void syn_set(syn_part_t *p, unsigned param, int value)
{
    if (p->engine == SYN_JP6) jp6_part_set(&p->u.jp6, param, value);
    else mks_part_set(&p->u.mks, param, value);
}

EL_DSP_CODE void syn_note_on(syn_part_t *p, int note, float velocity, int legato)
{
    if (p->engine == SYN_JP6) jp6_note_on(&p->u.jp6, note, velocity, legato);
    else mks_note_on(&p->u.mks, note, velocity, legato);
}

EL_DSP_CODE void syn_note_off(syn_part_t *p, int note)
{
    if (p->engine == SYN_JP6) jp6_note_off(&p->u.jp6, note);
    else mks_note_off(&p->u.mks, note);
}

EL_DSP_CODE void syn_all_off(syn_part_t *p)
{
    if (p->engine == SYN_JP6) jp6_all_off(&p->u.jp6);
    else mks_all_off(&p->u.mks);
}

EL_DSP_CODE void syn_render(syn_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm,
                            float level)
{
    /* The JP-6 sounds come out a little quieter than the MKS-50 ones. */
    if (p->engine == SYN_JP6) { p->u.jp6.level = level * 1.4f; jp6_render(&p->u.jp6, left, right, dly, rev, frames, bpm); }
    else { p->u.mks.level = level; mks_render(&p->u.mks, left, right, dly, rev, frames, bpm); }
}

unsigned syn_chord_memory(const syn_part_t *p)
{
    if (p->engine == SYN_JP6) return p->u.jp6.v[JP_KEY_MODE] == 1 ? p->u.jp6.v[JP_CHORD] + 1u : 0;
    return p->u.mks.v[MP_KEY_MODE] == 1 ? p->u.mks.v[MP_CHORD] + 1u : 0;
}

int syn_param_max(unsigned engine, unsigned param)
{
    return engine == SYN_JP6 ? jp6_param_max(param) : mks_param_max(param);
}

const char *syn_param_name(unsigned engine, unsigned param)
{
    return engine == SYN_JP6 ? jp6_param_name(param) : mks_param_name(param);
}

const char *syn_param_value_name(unsigned engine, unsigned param, int value)
{
    return engine == SYN_JP6 ? jp6_param_value_name(param, value) : mks_param_value_name(param, value);
}
