/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host check: the distortion (engine/dist.c) on a test signal for every
 * type and a few settings, raw float32 out. Built once with DIST_FIXED 0 and
 * once with 1; compare with host/dist_fixed_cmp.py (make dist-fixed-check). */
#include "dist.h"
#include "dsp_math.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    dist_init_tables();
    static const int set[][4] = {{30, 64, 64, 127}, {100, 30, 80, 127}, {127, 127, 64, 90}, {0, 64, 64, 127}, {64, 90, 40, 60}};
    for (int mode = 1; mode <= 4; ++mode)
        for (unsigned s = 0; s < sizeof set / sizeof set[0]; ++s) {
            el_dist_t d;
            memset(&d, 0, sizeof d);
            d.gain = 1; d.mono = 1;
            uint32_t rng = 12345;
            static float l[128], r[128];
            for (int b = 0; b < 44100 / 128; ++b) {
                for (int n = 0; n < 128; ++n) {
                    int i = b * 128 + n;
                    float env = expf(-(float)(i % 11025) / 1500.0f);
                    rng = rng * 1664525u + 1013904223u;
                    l[n] = 0.6f * env * sinf(2.0f * 3.14159265f * 55.0f * (float)i / 44100.0f)
                         + 0.3f * env * ((float)(int32_t)rng / 2147483648.0f) + 0.1f * sinf(2.0f * 3.14159265f * 440.0f * (float)i / 44100.0f);
                }
                dist_process(&d, l, r, 128, mode, set[s][0], set[s][1], set[s][2], set[s][3], 0);
                fwrite(l, 4, 128, f);
            }
        }
    fclose(f);
    return 0;
}
