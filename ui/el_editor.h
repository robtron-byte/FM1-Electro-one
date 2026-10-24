/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Synth editor: every parameter of the MKS-50 and JP-6 engines, grouped in
 * the sections of the original panels (DCO, VCF, VCA, ENV ... / VCO-1,
 * VCO-2, ... ENV-2, LFO), four per page for KNOB 1-4. Draws the section:
 * envelopes as curves, the filter as its response, the LFO as its wave,
 * the rest as a list of bars. Control task only. */
#ifndef EL_EDITOR_H
#define EL_EDITOR_H
#include "synth.h"
#include "gfx.h"

enum { ED_LIST, ED_ENV, ED_VCF, ED_LFO };
enum { ED_NONE = 0xff, ED_MAX_PARAMS = 16 };

typedef struct {
    const char *name, *tab;
    uint8_t kind, count;
    uint8_t param[ED_MAX_PARAMS];   /* ED_NONE: empty knob */
} ed_section_t;

unsigned ed_sections(unsigned engine);
const ed_section_t *ed_section(unsigned engine, unsigned section);
static inline unsigned ed_pages(const ed_section_t *s) { return (s->count + 3u) / 4u; }
/* Parameter on KNOB slot 0-3 of a page, or ED_NONE. */
unsigned ed_param(unsigned engine, unsigned section, unsigned page, unsigned slot);
/* Section tabs, title and picture between y and y + h. */
void ed_draw(gfx_band_t *b, const syn_part_t *part, unsigned section, unsigned page, uint16_t color, int y, int h);
#endif
