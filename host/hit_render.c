/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: render one 808 hit for the calibration scripts.
 * Usage: hit_render <out.raw> <part> <velocity 0..1> <seconds> [index=value ...]
 *        [bd=v,v,...] [sd=v,...] [cy=v,...] [mx=v,...] [cp=v,...] [cb=v,...] [hh=v,...] [k<index>=value (kit)]
 * index=value sets the part's parameter (tr808_set order); bd / sd / cy
 * replace the leading calibration values. Output: float32 mono, 44.1 kHz,
 * hit at sample 0. */
#include "tr808.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_cal(float *cal, int n, const char *list)
{
    for (int i = 0; i < n && *list; ++i) {
        char *end;
        cal[i] = strtof(list, &end);
        list = *end == ',' ? end + 1 : end;
    }
}

int main(int argc, char **argv)
{
    if (argc < 5) { fprintf(stderr, "usage: hit_render out.raw part velocity seconds [i=v] [bd=..]\n"); return 2; }
    static tr808_t t;
    tr808_init(&t);
    int part = atoi(argv[2]);
    for (int i = 5; i < argc; ++i) {
        if (!strncmp(argv[i], "bd=", 3)) set_cal(tr808_bd_cal, BD_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "sd=", 3)) set_cal(tr808_sd_cal, SD_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "cy=", 3)) set_cal(tr808_cy_cal, CY_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "mx=", 3)) set_cal(tr808_mx_cal, MX_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "cp=", 3)) set_cal(tr808_cp_cal, CP_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "cb=", 3)) set_cal(tr808_cb_cal, CB_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "hh=", 3)) set_cal(tr808_hh_cal, HH_CAL, argv[i] + 3);
        else if (!strncmp(argv[i], "h2=", 3)) set_cal(tr808_hh2_cal, HH2_CAL, argv[i] + 3);
        else if (argv[i][0] == 'k') { int p, v; if (sscanf(argv[i] + 1, "%d=%d", &p, &v) == 2) tr808_set(&t, D8_KIT, p, v); }
        else { int p, v; if (sscanf(argv[i], "%d=%d", &p, &v) == 2) tr808_set(&t, part, p, v); }
    }
    for (int p = 0; p < tr808_nparams(part); ++p)
        if (!strcmp(tr808_param(part, p)->name, "Pan")) tr808_set(&t, part, p, 64);
    int frames = (int)(atof(argv[4]) * 44100.0);
    float *out = calloc((size_t)frames + 128, sizeof(float));
    tr808_trigger(&t, part, (float)atof(argv[3]));
    for (int n = 0; n < frames; n += 128) {
        float k[128] = {0}, l[128] = {0}, r[128] = {0}, rv[128] = {0}, g[128] = {0}, d[128] = {0};
        tr808_render(&t, k, l, r, rv, g, d, 128);
        for (int i = 0; i < 128; ++i) out[n + i] = k[i] + 0.5f * (l[i] + r[i]);
    }
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fwrite(out, sizeof(float), (size_t)frames, f);
    fclose(f);
    free(out);
    return 0;
}
