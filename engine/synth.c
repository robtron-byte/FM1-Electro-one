/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: the constants of this file sit in internal SRAM; its audio
 * code is marked EL_DSP_CODE / EL_DSP_HELPER (control code runs from Flash). */
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "synth.h"
#include <string.h>

/* Engine 0 is the MKS-50 or, with EL_SYNTH_JX, the JX-8P (docs/adr/0039): the same slot (SYN_MKS),
 * the same calls. */
#if EL_SYNTH_JX
#define A_PRESETS jx8p_presets
#define A_PRESET_COUNT jx8p_preset_count
#define A_INIT jx8p_part_init
#define A_LOAD jx8p_part_load
#define A_SET jx8p_part_set
#define A_NOTE_ON jx8p_note_on
#define A_NOTE_OFF jx8p_note_off
#define A_ALL_OFF jx8p_all_off
#define A_RENDER jx8p_render
#define A_PARAM_MAX jx8p_param_max
#define A_PARAM_NAME jx8p_param_name
#define A_PARAM_VALUE_NAME jx8p_param_value_name
#define A_KEY_MODE JX_KEY_MODE
#define A_CHORD JX_CHORD
const char *const syn_engine_names[SYN_ENGINES] = {"JX-8P", "JP-6"};
#else
#define A_PRESETS mks_presets
#define A_PRESET_COUNT mks_preset_count
#define A_INIT mks_part_init
#define A_LOAD mks_part_load
#define A_SET mks_part_set
#define A_NOTE_ON mks_note_on
#define A_NOTE_OFF mks_note_off
#define A_ALL_OFF mks_all_off
#define A_RENDER mks_render
#define A_PARAM_MAX mks_param_max
#define A_PARAM_NAME mks_param_name
#define A_PARAM_VALUE_NAME mks_param_value_name
#define A_KEY_MODE MP_KEY_MODE
#define A_CHORD MP_CHORD
const char *const syn_engine_names[SYN_ENGINES] = {"MKS-50", "JP-6"};
#endif

unsigned syn_sound_count(void) { return A_PRESET_COUNT + jp6_preset_count + syn_extra_count; }

/* Own sounds come after both engines': index into syn_extras, or -1. */
static int extra_index(unsigned s)
{
    unsigned base = A_PRESET_COUNT + jp6_preset_count;
    return s >= base && s - base < syn_extra_count ? (int)(s - base) : -1;
}
static int is_extra(unsigned s) { return s >= A_PRESET_COUNT + jp6_preset_count; }

unsigned syn_sound_engine(unsigned s)
{
    if (is_extra(s)) { int e = extra_index(s); return e >= 0 ? syn_extras[e].engine : SYN_MKS; }
    return s < A_PRESET_COUNT ? SYN_MKS : SYN_JP6;
}

unsigned syn_sound_area(unsigned s)
{
    if (is_extra(s)) { int e = extra_index(s); return e >= 0 ? syn_extras[e].area : 0; }
    if (s < A_PRESET_COUNT) return A_PRESETS[s].area;
    s -= A_PRESET_COUNT;
    return s < jp6_preset_count ? jp6_presets[s].area : 0;
}

const char *syn_sound_name(unsigned s)
{
    if (is_extra(s)) { int e = extra_index(s); return e >= 0 ? syn_extras[e].name : "?"; }
    if (s < A_PRESET_COUNT) return A_PRESETS[s].name;
    s -= A_PRESET_COUNT;
    return s < jp6_preset_count ? jp6_presets[s].name : "?";
}

const uint8_t *syn_sound_values(unsigned s)
{
    if (is_extra(s)) { int e = extra_index(s); return syn_extras[e >= 0 ? e : 0].v; }
    if (s < A_PRESET_COUNT) return A_PRESETS[s].v;
    s -= A_PRESET_COUNT;
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
    else A_INIT(&p->u.mks, voices, seed);
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
        else A_INIT(&p->u.mks, voices, seed);
    }
    if (engine == SYN_JP6) jp6_part_load(&p->u.jp6, v);
    else A_LOAD(&p->u.mks, v);
}

EL_DSP_CODE void syn_set(syn_part_t *p, unsigned param, int value)
{
    if (p->engine == SYN_JP6) jp6_part_set(&p->u.jp6, param, value);
    else A_SET(&p->u.mks, param, value);
}

EL_DSP_CODE void syn_note_on(syn_part_t *p, int note, float velocity, int legato)
{
    if (p->engine == SYN_JP6) jp6_note_on(&p->u.jp6, note, velocity, legato);
    else A_NOTE_ON(&p->u.mks, note, velocity, legato);
}

EL_DSP_CODE void syn_note_off(syn_part_t *p, int note)
{
    if (p->engine == SYN_JP6) jp6_note_off(&p->u.jp6, note);
    else A_NOTE_OFF(&p->u.mks, note);
}

EL_DSP_CODE void syn_all_off(syn_part_t *p)
{
    if (p->engine == SYN_JP6) jp6_all_off(&p->u.jp6);
    else A_ALL_OFF(&p->u.mks);
}

EL_DSP_CODE void syn_render(syn_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm,
                            float level)
{
    /* The JP-6 sounds come out a little quieter than the MKS-50 ones. */
    if (p->engine == SYN_JP6) { p->u.jp6.level = level * 1.4f; jp6_render(&p->u.jp6, left, right, dly, rev, frames, bpm); }
    else { p->u.mks.level = level; A_RENDER(&p->u.mks, left, right, dly, rev, frames, bpm); }
}

unsigned syn_chord_memory(const syn_part_t *p)
{
    if (p->engine == SYN_JP6) return p->u.jp6.v[JP_KEY_MODE] == 1 ? p->u.jp6.v[JP_CHORD] + 1u : 0;
    return p->u.mks.v[A_KEY_MODE] == 1 ? p->u.mks.v[A_CHORD] + 1u : 0;
}

int syn_param_max(unsigned engine, unsigned param)
{
    return engine == SYN_JP6 ? jp6_param_max(param) : A_PARAM_MAX(param);
}

const char *syn_param_name(unsigned engine, unsigned param)
{
    return engine == SYN_JP6 ? jp6_param_name(param) : A_PARAM_NAME(param);
}

const char *syn_param_value_name(unsigned engine, unsigned param, int value)
{
    return engine == SYN_JP6 ? jp6_param_value_name(param, value) : A_PARAM_VALUE_NAME(param, value);
}
