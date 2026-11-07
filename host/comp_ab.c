/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: the default groove through the master, for A/B listening of
 * the compressor: `make comp-ab` writes build/wav/comp_A.wav (as set from
 * the factory) and comp_B.wav (COMP RATIO CLUB, docs/adr/0029).
 * Usage: comp_ab <out.wav> <ratio index> */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "electro.h"
static el_engine_t e;
int main(int argc, char **argv)
{
    if (argc < 3) return 1;
    el_init(&e);
    el_post(&e, EL_CMD_OPTION, EL_OPT_COMP_RATIO, (int16_t)atoi(argv[2]));
    el_post(&e, EL_CMD_START, 0, 0);
    enum { SECONDS = 16 };
    static float out[44100 * SECONDS * 2];
    const unsigned total = 44100u * SECONDS / EL_BLOCK * EL_BLOCK;
    float gr = 0;
    for (unsigned p = 0; p < total; p += EL_BLOCK) {
        el_render_float(&e, out + 2 * p, EL_BLOCK);
        if (e.mix.gain_reduction_db < gr) gr = e.mix.gain_reduction_db;
    }
    double s2 = 0; float pk = 0;
    for (unsigned i = 0; i < 2 * total; ++i) { s2 += (double)out[i] * out[i]; if (fabsf(out[i]) > pk) pk = fabsf(out[i]); }
    printf("%s: rms %.1f dBFS, peak %.1f dBFS, most reduction %.1f dB\n", argv[1], 10 * log10(s2 / (2 * total) + 1e-20), 20 * log10(pk + 1e-20), gr);
    FILE *f = fopen(argv[1], "wb");
    unsigned data = total * 4, sr = 44100, br = sr * 4, v;
    unsigned short s;
    fwrite("RIFF", 1, 4, f); v = 36 + data; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (unsigned i = 0; i < 2 * total; ++i) { float y = out[i] < -1 ? -1 : out[i] > 1 ? 1 : out[i]; short q = (short)lrintf(y * 32767); fwrite(&q, 2, 1, f); }
    fclose(f);
    return 0;
}
