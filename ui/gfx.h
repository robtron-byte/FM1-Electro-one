/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Band renderer for the 240x240 LCD. A page is drawn once per 8-row band
 * into a 3.75 KB buffer; every primitive clips to the current band, so no
 * full frame buffer is needed. */
#ifndef EL_GFX_H
#define EL_GFX_H
#include <stdint.h>

enum { GFX_W = 240, GFX_H = 240, GFX_BAND = 8, GFX_BANDS = GFX_H / GFX_BAND };

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | ((b) >> 3)))

typedef struct {
    uint16_t px[GFX_W * GFX_BAND];
    int top; /* first screen row held in px */
} gfx_band_t;

void gfx_begin(gfx_band_t *b, unsigned band, uint16_t background);
void gfx_fill(gfx_band_t *b, int x, int y, int w, int h, uint16_t color);
void gfx_frame(gfx_band_t *b, int x, int y, int w, int h, uint16_t color);
/* 5x7 glyphs on a 6x8 cell, scaled by an integer factor. Lowercase prints
 * as uppercase. Returns the x after the text. */
int gfx_text(gfx_band_t *b, int x, int y, const char *s, uint16_t color, int scale);
int gfx_text_width(const char *s, int scale);
/* Does the rectangle touch the current band? Lets callers skip work. */
static inline int gfx_touches(const gfx_band_t *b, int y, int h) { return y < b->top + GFX_BAND && y + h > b->top; }
#endif
