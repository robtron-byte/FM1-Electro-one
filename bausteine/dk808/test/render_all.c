/* SPDX-License-Identifier: GPL-3.0-only
 * dk808: rendert jedes der 17 Instrumente (Velocity 64 und 127, je 2 s, frischer Zustand) als float-Rohdaten nach
 * stdout; einmal mit DK808_FLOAT=0, einmal mit 1 übersetzt, vergleicht test/float_cmp.py beides (docs/adr/0050). */
#include <stdio.h>
#include <string.h>
#include "dk808.h"
enum { SR = 44100, N = SR * 2, B = 64 };
static dk808_t d;
static float l[N], r[B];
int main(void)
{
    for (unsigned i = 0; i < DK_COUNT; i++)
        for (int vv = 0; vv < 2; vv++) {
            memset(l, 0, sizeof l);
            dk808_init(&d, SR);
            dk808_trigger(&d, i, vv ? 127 : 64);
            for (int p = 0; p < N; p += B) dk808_render(&d, l + p, r, B);
            fwrite(l, sizeof l[0], N, stdout);
        }
    return 0;
}
