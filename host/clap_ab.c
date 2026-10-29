/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: the 808 clap through the whole engine, for A/B listening.
 * `make clap-ab` builds it in three variants (TR808_CP_MODEL 0 / 1 / 2 in
 * engine/tr808.c, docs/adr/0008) and writes build/wav/clap_A..C.wav.
 * 120 BPM, 8 bars: single claps (normal, accented) with room to hear the
 * tail, then a groove with kick and closed hats.
 * Usage: clap_ab <out.wav> */
#include "electro.h"
#include "tr808.h"
#include <stdio.h>
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
static void hit(int part, int velocity) { dr_post(&e.drums, DR_CMD_HIT, (uint8_t)part, (uint8_t)velocity, 0, 0); }
int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    el_init(&e);
    memset(&e.drums.pattern[0], 0, sizeof e.drums.pattern[0]);
    e.drums.pattern[0].length = 16;
    for (unsigned i = 0; i < EL_TRACKS; ++i) e.track[i].muted = 1;
    enum { STEPS = 8 * 16 };
    static float out[44100 * 20 * 2];
    const double step_frames = 44100.0 * 60.0 / 120.0 / 4.0;
    const unsigned total = (unsigned)(step_frames * (STEPS + 8)) / EL_BLOCK * EL_BLOCK;
    int next = 0;
    for (unsigned p = 0; p < total; p += EL_BLOCK) {
        while (next < STEPS && next * step_frames < p + EL_BLOCK) {
            int s = next % 16, bar = next / 16;
            if (bar < 4) {
                if (s == 0) hit(D8_CP, bar % 2 ? 127 : 90);
                if (s == 8) hit(D8_CP, bar % 2 ? 90 : 127);
            } else {
                if (s % 4 == 0) hit(D8_BD, 110);
                if (s == 4 || s == 12) hit(D8_CP, 100);
                if (s % 2 == 0) hit(D8_CH, s % 4 ? 80 : 100);
            }
            ++next;
        }
        el_render_float(&e, out + 2 * p, EL_BLOCK);
    }
    wav(argv[1], out, total);
    return 0;
}
