/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * One synth sound of an area, played by either engine: the MKS-50 port or
 * the JP-6 port. Both share the RAM of one part (only one runs per area),
 * so every area can pick its engine and all three play at once.
 *
 * Sounds are one list: the MKS-50 factory sounds first, then the JP-6
 * ones; a sound index picks engine and patch together. */
#ifndef EL_SYNTH_H
#define EL_SYNTH_H
#include "mks.h"
#include "jp6.h"

enum { SYN_MKS, SYN_JP6, SYN_ENGINES };
enum { SYN_VALUES = (int)MP_COUNT > (int)JP_COUNT ? (int)MP_COUNT : (int)JP_COUNT };

typedef struct {
    uint8_t engine;
    union { mks_part_t mks; jp6_part_t jp6; } u;
} syn_part_t;

extern const char *const syn_engine_names[SYN_ENGINES];

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

/* Values of the sound as it is now (MP_COUNT or JP_COUNT of them). */
static inline const uint8_t *syn_values(const syn_part_t *p) { return p->engine == SYN_JP6 ? p->u.jp6.v : p->u.mks.v; }
static inline unsigned syn_value_count(unsigned engine) { return engine == SYN_JP6 ? JP_COUNT : MP_COUNT; }
/* Live chord preset of a sound in chord key mode: chord type (1..), or 0. */
unsigned syn_chord_memory(const syn_part_t *p);

/* Display helpers. */
int syn_param_max(unsigned engine, unsigned param);
const char *syn_param_name(unsigned engine, unsigned param);
const char *syn_param_value_name(unsigned engine, unsigned param, int value);
#endif
