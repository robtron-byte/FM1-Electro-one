/* SPDX-License-Identifier: GPL-3.0-only
 * JP-6 solo (FM1-Electro-one, docs/adr/0048): der JP-6-Kern von FM1-Electro (engine/jp6.c, unverändert eingebunden) mit
 * vier Parts, einem gemeinsamen Stimmen-Pool und Stimmen auf beiden Kernen, nach dem Muster von fm1-synths jx8p/engine/jx.c
 * (jx_plan, jx_control, jx_voices, jx_finish, jx_mix). Mit einem Part bitgleich mit jp6_render (Host-Test
 * bausteine/jp6, Ziel pool). */
#ifndef JPP_H
#define JPP_H
#include "jp6.h"

#ifndef SYN_BLOCK
#define SYN_BLOCK 64
#endif
#ifndef JPP_POOL
#define JPP_POOL 6
#endif
enum { JPP_PARTS = 4, JPP_BLOCK = SYN_BLOCK, JPP_SLICES = SYN_BLOCK / 16, JPP_CORES = 2 };

/* Was ein Part in einem Steuerschritt (16 Samples) für seine Stimmen festlegt (jp6.c control). */
typedef struct { float cut, lfo, hb0, hb1, hb2, ha1, ha2, hpf_s; uint8_t hpf_on; } jpp_slice_t;

typedef struct {
    jp6_part_t part[JPP_PARTS];
    uint8_t pool;                     /* Stimmen gesamt, 1..JP6_MAX_VOICES * JPP_PARTS (Standard JPP_POOL) */
    uint8_t cores;                    /* 1 oder 2 (die App setzt es je Block) */
    uint8_t extra1;                   /* Last, die Kern 1 sonst hat (Delay/Hall): weniger Stimmen dort */
    uint8_t vcore[JPP_PARTS][JP6_MAX_VOICES];   /* dieser Block: welcher Kern die Stimme rechnet */
    uint8_t pcore[JPP_PARTS];         /* dieser Block: welcher Kern den Part fertig macht (Distortion, Chorus) */
    uint8_t live[JPP_PARTS];          /* dieser Block: der Part klingt (Stimmen oder Nachklang) */
    uint8_t anyv[JPP_PARTS];          /* dieser Block: Stimmen am Anfang des Blocks (wie any in jp6_render) */
    uint32_t stamp[JPP_PARTS][JP6_MAX_VOICES], clock;   /* Alter über alle Parts (Pool: die älteste geht) */
    jpp_slice_t sl[JPP_PARTS][JPP_SLICES];
    float vout[JPP_CORES][JPP_PARTS][2][JPP_BLOCK];     /* Stimmen je Kern und Part */
    float pout[JPP_PARTS][2][JPP_BLOCK];                /* Parts fertig */
    float psend[JPP_PARTS][2][JPP_BLOCK];
    float send_sum[2][JPP_BLOCK];
    uint8_t voices_on[JPP_CORES], parts_on[JPP_CORES], ctl_on[JPP_CORES];
} jpp_t;

void jpp_init(jpp_t *s, unsigned pool, uint32_t seed);
EL_DSP_CODE void jpp_part_load(jpp_t *s, unsigned part, const uint8_t v[JP_COUNT]);
EL_DSP_CODE void jpp_part_set(jpp_t *s, unsigned part, unsigned param, int value);
EL_DSP_CODE void jpp_note_on(jpp_t *s, unsigned part, int note, float velocity, int legato);
EL_DSP_CODE void jpp_note_off(jpp_t *s, unsigned part, int note);
EL_DSP_CODE void jpp_all_off(jpp_t *s, unsigned part);
EL_DSP_CODE int jpp_active(const jpp_t *s, unsigned part);
EL_DSP_CODE unsigned jpp_part_voices(const jpp_t *s, unsigned part);
/* Ein Block: jpp_plan, jpp_control(Kern), jpp_voices(Kern), jpp_finish(Kern), jpp_mix (Reihenfolge wie jx.h). */
EL_DSP_CODE void jpp_plan(jpp_t *s);
EL_DSP_CODE void jpp_control(jpp_t *s, unsigned core, float bpm);
EL_DSP_CODE void jpp_voices(jpp_t *s, unsigned core);
EL_DSP_CODE void jpp_finish(jpp_t *s, unsigned core);
EL_DSP_CODE void jpp_mix(jpp_t *s, float *left, float *right);
/* Alles auf einem Kern (Host). */
EL_DSP_CODE void jpp_render(jpp_t *s, float *left, float *right, float bpm);
float jpp_lfo_hz(int rate);
#endif
