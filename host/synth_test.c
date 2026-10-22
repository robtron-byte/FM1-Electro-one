/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host test: play a few MKS-50 presets and measure level and cost. */
#include "mks.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const mks_preset_t *find(const char *name)
{
    for (unsigned i = 0; i < mks_preset_count; ++i) if (!strcmp(mks_presets[i].name, name)) return &mks_presets[i];
    return 0;
}

int main(int argc, char **argv)
{
    static mks_part_t part;
    const char *names[] = {"ACID LINE", "JX STRINGS", "ZAP", "ROBO BASS", "STR CHORD", "SYNC ZAP"};
    const char *dir = argc > 1 ? argv[1] : ".";
    int failed = 0;
    for (unsigned n = 0; n < sizeof names / sizeof names[0]; ++n) {
        const mks_preset_t *pr = find(names[n]);
        if (!pr) { printf("missing %s\n", names[n]); return 1; }
        mks_part_init(&part, 6, 1234 + n);
        mks_part_load(&part, pr->v);
        static float out[44100 * 4 * 2];
        float l[EL_BLOCK], r[EL_BLOCK], d[EL_BLOCK], v[EL_BLOCK], peak = 0;
        const int notes[4][3] = {{48, 51, 55}, {50, 53, 57}, {43, 46, 50}, {46, 50, 53}};
        clock_t start = clock();
        for (int block = 0; block < 44100 * 4 / EL_BLOCK; ++block) {
            int frame = block * EL_BLOCK, beat = frame / 22050;
            if (frame % 22050 < EL_BLOCK) {
                int chord = beat % 4, poly = part.v[MP_KEY_MODE] < 2 ? 3 : 1;
                for (int k = 0; k < 3; ++k) mks_note_off(&part, notes[(beat + 3) % 4][k] + (n == 2 || n == 5 ? 24 : 0));
                for (int k = 0; k < poly; ++k) mks_note_on(&part, notes[chord][k] + (n == 2 || n == 5 ? 24 : 0), 0.8f, 0);
            }
            if (frame % 22050 >= 11025 && frame % 22050 < 11025 + EL_BLOCK && part.v[MP_KEY_MODE] >= 2)
                for (int k = 0; k < 3; ++k) mks_note_off(&part, notes[beat % 4][k] + (n == 2 || n == 5 ? 24 : 0));
            memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(d, 0, sizeof d); memset(v, 0, sizeof v);
            mks_render(&part, l, r, d, v, EL_BLOCK, 120);
            for (int i = 0; i < EL_BLOCK; ++i) {
                out[2 * (frame + i)] = l[i]; out[2 * (frame + i) + 1] = r[i];
                if (fabsf(l[i]) > peak) peak = fabsf(l[i]);
                if (!isfinite(l[i])) { peak = 99; }
            }
        }
        double secs = (double)(clock() - start) / CLOCKS_PER_SEC;
        char path[512], slug[32];
        unsigned j = 0;
        for (const char *c = names[n]; *c && j < 31; ++c) slug[j++] = *c == ' ' ? '_' : (char)(*c | 32);
        slug[j] = 0;
        snprintf(path, sizeof path, "%s/synth_%s.wav", dir, slug);
        FILE *f = fopen(path, "wb");
        uint32_t frames = 44100 * 4, bytes = frames * 4, rate = 44100, br = rate * 4, riff = 36 + bytes, fs = 16;
        uint16_t pcm = 1, ch = 2, al = 4, bits = 16;
        fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fs, 4, 1, f);
        fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f);
        fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
        for (uint32_t i = 0; i < frames * 2; ++i) {
            float x = out[i] < -1 ? -1 : out[i] > 1 ? 1 : out[i];
            int16_t s = (int16_t)lrintf(x * 32767);
            fwrite(&s, 2, 1, f);
        }
        fclose(f);
        printf("%-10s peak %.2f  cost %.2f%% real time  -> %s\n", names[n], peak, secs / 4 * 100, path);
        if (peak < 0.02f || peak > 2.0f) failed = 1;
    }
    return failed;
}
