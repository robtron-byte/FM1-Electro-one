/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: the default groove with a big hall, for A/B listening of
 * FX_REVERB_LFO_EVERY (engine/fx.c, docs/adr/0019). `make hall-ab` builds it
 * with 1 (every frame, as before), 4 and 8 and writes build/wav/hall_A..C.wav,
 * and compares B and C with A.
 * Usage: hall_ab <out.wav> <out.raw> */
#include <stdio.h>
#include <math.h>
#include "electro.h"
static el_engine_t e;
int main(int argc, char **argv)
{
    if (argc < 3) return 1;
    el_init(&e);
    el_post(&e, EL_CMD_FX, FX_REVERB_RETURN, 110);
    el_post(&e, EL_CMD_FX, FX_REVERB_SIZE, 120);
    el_post(&e, EL_CMD_FX, FX_REVERB_DECAY, 100);
    for (unsigned i = 0; i < EL_TRACKS; ++i) el_post3(&e, EL_CMD_TRACK_PARAM, (uint8_t)i, MP_REVERB_SEND, 0, 100);
    el_post(&e, EL_CMD_START, 0, 0);
    enum { SECONDS = 14 };
    static float out[44100 * SECONDS * 2];
    const unsigned total = 44100u * SECONDS / EL_BLOCK * EL_BLOCK;
    for (unsigned p = 0; p < total; p += EL_BLOCK) {
        if (p == 44100u * 10u / EL_BLOCK * EL_BLOCK) el_post(&e, EL_CMD_STOP, 0, 0);   /* the tail alone at the end */
        el_render_float(&e, out + 2 * p, EL_BLOCK);
    }
    FILE *r = fopen(argv[2], "wb");
    fwrite(out, sizeof(float), 2 * total, r);
    fclose(r);
    FILE *f = fopen(argv[1], "wb");
    unsigned data = total * 4, sr = 44100, br = sr * 4, v;
    unsigned short s;
    fwrite("RIFF", 1, 4, f); v = 36 + data; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (unsigned i = 0; i < 2 * total; ++i) {
        float y = out[i] < -1 ? -1 : out[i] > 1 ? 1 : out[i];
        short q = (short)lrintf(y * 32767);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
    return 0;
}
