/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: render the 808 snare alone at the TONE / SNAPPY settings of
 * the Fischer TR-808 sample set (0, 2.5, 5, 7.5, 10 each) for
 * the snare analysis. Usage: snare_check <dir> [calibration values]
 * Files: <dir>/SD<tone><snappy>.raw, float32 mono, 3 s, 44.1 kHz, hit at 0. */
#include "tr808.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    for (int i = 2; i < argc && i - 2 < SD_CAL; ++i) tr808_sd_cal[i - 2] = (float)atof(argv[i]);
    static const char *const pos[5] = {"00", "25", "50", "75", "10"};
    static const int pot[5] = {0, 32, 64, 95, 127};
    enum { FRAMES = 44100 / 2, BLOCK = 128 };
    static float out[FRAMES];
    static tr808_t t;
    for (int a = 0; a < 5; ++a)
        for (int b = 0; b < 5; ++b) {
            tr808_init(&t);
            /* SD params: 0 level, 1 tune, 2 tone, 3 snappy, 4 decay */
            tr808_set(&t, D8_SD, 2, pot[a]);
            tr808_set(&t, D8_SD, 3, pot[b]);

            tr808_trigger(&t, D8_SD, 0.66f);
            float kick[BLOCK], l[BLOCK], r[BLOCK], rev[BLOCK], gate[BLOCK], dly[BLOCK];
            for (int n = 0; n < FRAMES; n += BLOCK) {
                memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(kick, 0, sizeof kick);
                memset(rev, 0, sizeof rev); memset(gate, 0, sizeof gate); memset(dly, 0, sizeof dly);
                tr808_render(&t, kick, l, r, rev, gate, dly, BLOCK);
                for (int i = 0; i < BLOCK && n + i < FRAMES; ++i) out[n + i] = 0.5f * (l[i] + r[i]);
            }
            char path[512];
            snprintf(path, sizeof path, "%s/SD%s%s.raw", dir, pos[a], pos[b]);
            FILE *f = fopen(path, "wb");
            if (!f) return 1;
            fwrite(out, sizeof(float), FRAMES, f);
            fclose(f);
        }
    return 0;
}
