/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: four 808 kicks (velocity 120 / 90) alone through the whole
 * engine, for A/B listening. `make kick-ab` builds it in four variants
 * (see the switches in engine/tr808.c and engine/mixbus.c) and writes
 * build/wav/kick_A.wav .. kick_D.wav. Usage: kick_ab <out.wav> <decay 0..127> */
#include "electro.h"
#include "tr808.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static el_engine_t e;
static void wav(const char *path, const float *x, unsigned n)
{
    FILE *f = fopen(path, "wb");
    unsigned data = n * 4, sr = 44100, br = sr * 4;
    fwrite("RIFF", 1, 4, f); unsigned v = 36 + data; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); unsigned short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (unsigned i = 0; i < 2 * n; ++i) { float y = x[i] < -1 ? -1 : x[i] > 1 ? 1 : x[i]; short q = (short)lrintf(y * 32767); fwrite(&q, 2, 1, f); }
    fclose(f);
}
int main(int argc, char **argv)
{
    int decay = atoi(argv[2]);
    el_init(&e);
    memset(&e.drums.pattern[0], 0, sizeof e.drums.pattern[0]);
    e.drums.pattern[0].length = 16;
    for (unsigned i = 0; i < EL_TRACKS; ++i) e.track[i].muted = 1;
    tr808_set(&e.drums.kit, D8_BD, 2, decay);
    static float out[44100 * 8 * 2];
    const unsigned total = 44100 * 8 / EL_BLOCK * EL_BLOCK;
    for (unsigned p = 0; p < total; p += EL_BLOCK) {
        if (p % (44100 * 2 / EL_BLOCK * EL_BLOCK) == 0) dr_post(&e.drums, DR_CMD_HIT, 0, p % (44100 * 4 / EL_BLOCK * EL_BLOCK) ? 90 : 120, 0, 0);
        el_render_float(&e, out + 2 * p, EL_BLOCK);
    }
    wav(argv[1], out, total);
    return 0;
}
