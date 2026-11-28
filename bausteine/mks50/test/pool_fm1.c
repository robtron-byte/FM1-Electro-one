/* SPDX-License-Identifier: GPL-3.0-only
 * mks50: die Seite des Pool-Kerns (bausteine/varianten/mks50/engine/mks.c, aus fm1-synths) für test/pool.c, mit
 * release_hurry = 1 (FM1-Electros MKS_RELEASE_HURRY, in der Firmware der Schalter MK_RELEASE_HURRY). */
#include "mks.h"
extern const mks_preset_t mks50_eigene[];
extern const unsigned mks50_eigene_count;
static mks_t s;
unsigned pk_count(void) { return mks_preset_count + mks50_eigene_count; }
const char *pk_name(unsigned i) { return i < mks_preset_count ? mks_presets[i].name : mks50_eigene[i - mks_preset_count].name; }
int pk_block(void) { return MKS_BLOCK; }
void pk_init(unsigned i)
{
    mks_init(&s, 6, 1u);
    s.release_hurry = 1;
    mks_part_load(&s, 0, i < mks_preset_count ? mks_presets[i].v : mks50_eigene[i - mks_preset_count].v);
}
void pk_on(int note) { mks_note_on(&s, 0, note, 100 / 127.0f, 0); }
void pk_off(int note) { mks_note_off(&s, 0, note); }
void pk_render(float *l, float *r) { mks_render(&s, l, r, 120.0f); }
