/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: render one PO-12 style sound for the calibration.
 * Usage: po_render <out.raw> <sound> <seconds> [a b] [p0,p1,...]  (float32 mono) */
#include "po12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    unsigned s = (unsigned)atoi(argv[2]);
    float a = argc > 5 ? (float)atof(argv[4]) : 0.5f, b = argc > 5 ? (float)atof(argv[5]) : 0.5f;
    if (argc > 6) {
        const char *l = argv[6];
        for (int i = 0; i < POP_PARAMS && *l; ++i) { char *e; po_sounds[s][i] = strtof(l, &e); l = *e == ',' ? e + 1 : e; }
    }
    int frames = (int)(atof(argv[3]) * 44100.0);
    float *out = calloc((size_t)frames, sizeof(float));
    static po_voice_t v;
    po_trigger(&v, s, 0.66f, a, b);
    for (int i = 0; i < frames; ++i) out[i] = po_tick(&v);
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fwrite(out, sizeof(float), (size_t)frames, f);
    fclose(f);
    free(out);
    return 0;
}
