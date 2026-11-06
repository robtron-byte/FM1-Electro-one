/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host check: the mix bus (engine/mixbus.c) on synthetic buses loud enough
 * for the compressors, the saturation and the limiter, MASTER FX on and
 * off; raw float32 stereo out. Built with MB_FIXED 0 and 1, compared by
 * make mb-fixed-check. */
#include "mixbus.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static el_mixbus_t mb;
int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    for (int pass = 0; pass < 3; ++pass) {
        mb_init(&mb);
        mb.enabled = pass != 1;
        float level = pass == 2 ? 2.5f : 1.0f;   /* pass 2: hot, the limiter works hard */
        static float k[128], dl[128], dr[128], bl[128], br[128], sl[128], sr[128], ol[128], orr[128];
        uint32_t rng = 777;
        for (int b = 0; b < 3 * 44100 / 128; ++b) {
            for (int n = 0; n < 128; ++n) {
                int i = b * 128 + n;
                float t = (float)(i % 11025) / 44100.0f, beat = expf(-t * 12.0f);
                rng = rng * 1664525u + 1013904223u;
                float noise = (float)(int32_t)rng / 2147483648.0f;
                k[n] = level * 0.9f * beat * sinf(2.0f * 3.14159265f * 50.0f * t);
                dl[n] = level * 0.4f * expf(-(float)(i % 5512) / 900.0f) * noise; dr[n] = dl[n] * 0.8f;
                bl[n] = br[n] = level * 0.35f * sinf(2.0f * 3.14159265f * 55.0f * (float)i / 44100.0f);
                sl[n] = level * 0.25f * sinf(2.0f * 3.14159265f * 440.0f * (float)i / 44100.0f);
                sr[n] = level * 0.25f * sinf(2.0f * 3.14159265f * 660.0f * (float)i / 44100.0f);
            }
            mb_process(&mb, k, dl, dr, bl, br, sl, sr, ol, orr, 128);
            for (int n = 0; n < 128; ++n) { fwrite(&ol[n], 4, 1, f); fwrite(&orr[n], 4, 1, f); }
        }
    }
    fclose(f);
    return 0;
}
