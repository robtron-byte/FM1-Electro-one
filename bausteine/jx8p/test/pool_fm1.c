/* SPDX-License-Identifier: GPL-3.0-only
 * jx8p: die Seite des Pool-Kerns (bausteine/varianten/jx8p/engine/jx.c, aus fm1-synths) für test/pool.c. Eigene
 * Übersetzungseinheit, weil jx.h und engine/jx8p.h verschiedene Konfigurationen einbinden. */
#include "jx.h"
#include <string.h>
extern const jx_preset_t jx_eigene[];
extern const unsigned jx_eigene_count;
static jx_t s;
unsigned pk_count(void) { return jx_preset_count + jx_eigene_count; }
const char *pk_name(unsigned i) { return i < jx_preset_count ? jx_presets[i].name : jx_eigene[i - jx_preset_count].name; }
int pk_block(void) { return JX_BLOCK; }
void pk_init(unsigned i)
{
    jx_init(&s, 6, 1u);
    jx_part_load(&s, 0, i < jx_preset_count ? jx_presets[i].v : jx_eigene[i - jx_preset_count].v);
}
void pk_on(int note) { jx_note_on(&s, 0, note, 100 / 127.0f, 0); }
void pk_off(int note) { jx_note_off(&s, 0, note); }
void pk_render(float *l, float *r) { jx_render(&s, l, r, 120.0f); }
