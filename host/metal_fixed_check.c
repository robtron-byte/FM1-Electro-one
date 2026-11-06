/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host check: single hits of the cymbal and the hats, one WAV per case
 * (cases below), from the engine's start state. Built once with the float
 * 808 and once with TR808_FIXED (engine/tr808_metal_fixed.h); compare with
 * host/metal_fixed_cmp.py. Usage: metal_fixed_check <dir> */
#include "electro.h"
#include "tr808.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static el_engine_t e;
static void wav(const char *path, const float *x, unsigned n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    unsigned data = n * 4, sr = 44100, br = sr * 4;
    fwrite("RIFF", 1, 4, f); unsigned v = 36 + data; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); unsigned short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (unsigned i = 0; i < 2 * n; ++i) { float y = x[i] < -1 ? -1 : x[i] > 1 ? 1 : x[i]; short q = (short)lrintf(y * 32767); fwrite(&q, 2, 1, f); }
    fclose(f);
}
typedef struct { const char *name; int part, velocity, tune, decay, tone, second, slot, value; } hit_t;   /* slot / value: one more knob (tr808.c P_*) */
static const hit_t cases[] = {
    {"cy", D8_CY, 100, 64, 64, 64, -1},     {"cy_acc", D8_CY, 127, 64, 64, 64, -1},
    {"cy_dark", D8_CY, 100, 64, 20, 10, -1}, {"cy_bright", D8_CY, 100, 90, 120, 120, -1},
    {"oh", D8_OH, 100, 64, 64, 64, -1},     {"oh_acc", D8_OH, 127, 64, 64, 64, -1},
    {"oh_long", D8_OH, 100, 64, 120, 64, -1}, {"oh_tune", D8_OH, 100, 90, 64, 64, -1},
    {"ch", D8_CH, 100, 64, 64, 64, -1},     {"ch_acc", D8_CH, 127, 64, 64, 64, -1},
    {"ch_tune", D8_CH, 100, 30, 100, 64, -1}, {"oh_choke", D8_OH, 100, 64, 64, 64, D8_CH},
    {"bd", D8_BD, 100, 64, 64, 64, -1},     {"bd_acc", D8_BD, 127, 64, 64, 64, -1},
    {"bd_short", D8_BD, 100, 64, 0, 64, -1}, {"bd_long", D8_BD, 100, 64, 127, 64, -1},
    {"bd_tune", D8_BD, 100, 100, 64, 64, -1}, {"bd_tone", D8_BD, 100, 64, 64, 127, -1},
    {"bd_punch", D8_BD, 127, 64, 64, 64, -1, 5, 127}, {"bd_drive", D8_BD, 100, 64, 64, 64, -1, 4, 90},
    {"bd_retrig", D8_BD, 100, 64, 100, 64, D8_BD},
    {"sd", D8_SD, 100, 64, 64, 64, -1, 4, 100}, {"sd_acc", D8_SD, 127, 64, 64, 64, -1, 4, 127},
    {"sd_tone", D8_SD, 100, 90, 30, 120, -1, 4, 60}, {"sd_early", D8_SD, 100, 64, 64, 64, -1, 5, 1},
    {"lt", D8_LT, 100, 64, 64, 64, -1}, {"ht_acc", D8_HT, 127, 64, 64, 64, -1, 4, 100},
    {"lt_conga", D8_LT, 100, 64, 64, 64, -1, 5, 1}, {"ht_tune", D8_HT, 100, 100, 100, 100, D8_HT},
    {"rs", D8_RS, 100, 64, 64, 64, -1}, {"rs_clave", D8_RS, 127, 80, 64, 64, -1, 5, 1},
    {"cp", D8_CP, 100, 64, 64, 64, -1}, {"cp_acc", D8_CP, 127, 64, 100, 90, -1},
    {"cp_maracas", D8_CP, 100, 64, 64, 64, -1, 5, 1},
    {"cb", D8_CB, 100, 64, 64, 64, -1}, {"cb_tune", D8_CB, 127, 90, 100, 64, -1},
    {"ch_drive", D8_CH, 100, 64, 64, 64, -1, 4, 80}, {"cp_drive", D8_CP, 100, 64, 64, 64, -1, 4, 100},
};
int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    static float out[44100 * 3 * 2];
    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
        const hit_t *h = &cases[c];
        el_init(&e);
        memset(&e.drums.pattern[0], 0, sizeof e.drums.pattern[0]);
        for (unsigned i = 0; i < EL_TRACKS; ++i) e.track[i].muted = 1;
        e.drums.kit.pot[h->part][1] = (uint8_t)h->tune;    /* tr808.c P_TUNE, P_DECAY, P_TONE */
        e.drums.kit.pot[h->part][2] = (uint8_t)h->decay;
        e.drums.kit.pot[h->part][3] = (uint8_t)h->tone;
        if (h->slot) e.drums.kit.pot[h->part][h->slot] = (uint8_t)h->value;
        unsigned total = 44100 * 2 / EL_BLOCK * EL_BLOCK;
        memset(out, 0, sizeof out);
        for (unsigned p = 0; p < total; p += EL_BLOCK) {
            if (p == 0) dr_post(&e.drums, DR_CMD_HIT, (uint8_t)h->part, (uint8_t)h->velocity, 0, 0);
            if (h->second >= 0 && p == 44100 / 4 / EL_BLOCK * EL_BLOCK) dr_post(&e.drums, DR_CMD_HIT, (uint8_t)h->second, 100, 0, 0);
            el_render_float(&e, out + 2 * p, EL_BLOCK);
        }
        char path[256];
        snprintf(path, sizeof path, "%s/%s.wav", argv[1], h->name);
        wav(path, out, total);
    }
    return 0;
}
