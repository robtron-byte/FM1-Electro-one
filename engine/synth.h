/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * One synth sound of an area, played by either engine: the MKS-50 port or
 * the JP-6 port. Both share the RAM of one part (only one runs per area),
 * so every area can pick its engine and all three play at once.
 *
 * Sounds are one list: the MKS-50 factory sounds first, then the JP-6
 * ones, then FM1-Electro's own; a sound index picks engine and patch
 * together. */
#ifndef EL_SYNTH_H
#define EL_SYNTH_H
#include "el_config.h"
#if EL_SYNTH_JX
#include "jx8p.h"
#else
#include "mks.h"
#endif
#include "jp6.h"

enum { SYN_MKS, SYN_JP6, SYN_ENGINES };
/* Engine 0 (SYN_MKS): the MKS-50, or with EL_SYNTH_JX the JX-8P (docs/adr/0039). Its part is u.mks either way. */
#if EL_SYNTH_JX
typedef jx8p_part_t syn_a_part_t;
enum { SYN_A_COUNT = JX_COUNT, SYN_A_LFO_RATE = JX_LFO_RATE, SYN_A_LFO_SYNC = JX_LFO_SYNC, SYN_A_LFO_DIV = JX_LFO_DIV,
       SYN_A_KEY_MODE = JX_KEY_MODE, SYN_A_CHORD = JX_CHORD };
#else
typedef mks_part_t syn_a_part_t;
enum { SYN_A_COUNT = MP_COUNT, SYN_A_LFO_RATE = MP_LFO_RATE, SYN_A_LFO_SYNC = MP_LFO_SYNC, SYN_A_LFO_DIV = MP_LFO_DIV,
       SYN_A_KEY_MODE = MP_KEY_MODE, SYN_A_CHORD = MP_CHORD };
#endif
enum { SYN_VALUES = (int)SYN_A_COUNT > (int)JP_COUNT ? (int)SYN_A_COUNT : (int)JP_COUNT };

typedef struct {
    uint8_t engine;
    union { syn_a_part_t mks; jp6_part_t jp6; } u;
} syn_part_t;

extern const char *const syn_engine_names[SYN_ENGINES];

/* FM1-Electro's own factory sounds (engine/extra_presets.c, docs/adr/0027):
 * after the sounds of both engines, so saved sound numbers stay valid. */
typedef struct {
    char name[11];
    uint8_t area, engine;
    uint8_t v[SYN_VALUES];
} syn_extra_t;
extern const syn_extra_t syn_extras[];
extern const unsigned syn_extra_count;

/* Sound list (control side, Flash tables). */
unsigned syn_sound_count(void);
unsigned syn_sound_engine(unsigned sound);
unsigned syn_sound_area(unsigned sound);
const char *syn_sound_name(unsigned sound);
const uint8_t *syn_sound_values(unsigned sound);
/* Index of a sound by name and engine; -1 if missing. */
int syn_sound_find(unsigned engine, const char *name);

void syn_init(syn_part_t *p, unsigned engine, unsigned voices, uint32_t seed);
/* Audio side: switch engine if needed (stops its voices) and load values. */
EL_DSP_CODE void syn_load(syn_part_t *p, unsigned engine, const uint8_t *v);
EL_DSP_CODE void syn_set(syn_part_t *p, unsigned param, int value);
EL_DSP_CODE void syn_note_on(syn_part_t *p, int note, float velocity, int legato);
EL_DSP_CODE void syn_note_off(syn_part_t *p, int note);
EL_DSP_CODE void syn_all_off(syn_part_t *p);
EL_DSP_CODE void syn_render(syn_part_t *p, float *left, float *right, float *dly, float *rev, int frames, float bpm,
                            float level);

/* Values of the sound as it is now (SYN_A_COUNT or JP_COUNT of them). */
static inline const uint8_t *syn_values(const syn_part_t *p) { return p->engine == SYN_JP6 ? p->u.jp6.v : p->u.mks.v; }
static inline unsigned syn_value_count(unsigned engine) { return engine == SYN_JP6 ? JP_COUNT : SYN_A_COUNT; }
/* Live chord preset of a sound in chord key mode: chord type (1..), or 0. */
unsigned syn_chord_memory(const syn_part_t *p);

/* Display helpers. */
int syn_param_max(unsigned engine, unsigned param);
const char *syn_param_name(unsigned engine, unsigned param);
const char *syn_param_value_name(unsigned engine, unsigned param, int value);
#endif
