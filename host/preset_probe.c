/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Plays factory sounds on one note and writes raw mono floats (44.1 kHz) for
 * analysis (tools/preset_check.py).
 * Usage: preset_probe <out dir> <note> <seconds> [--area=N | name ...]
 * (all sounds without names; --area=3: the bass sounds) */
#include "synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s dir note seconds [name...]\n", argv[0]); return 1; }
    int note = atoi(argv[2]);
    float secs = (float)atof(argv[3]);
    static syn_part_t part;
    unsigned frames_total = (unsigned)(secs * 44100.0f);
    static float mono[44100 * 6];
    if (frames_total > sizeof mono / sizeof mono[0]) frames_total = sizeof mono / sizeof mono[0];
    for (unsigned s = 0; s < syn_sound_count(); ++s) {
        int wanted = argc == 4;
        for (int a = 4; a < argc; ++a) {
            if (!strncmp(argv[a], "--area=", 7)) wanted |= syn_sound_area(s) == (unsigned)atoi(argv[a] + 7);
            else wanted |= !strcmp(argv[a], syn_sound_name(s));
        }
        if (!wanted) continue;
        unsigned engine = syn_sound_engine(s);
        syn_init(&part, engine, 4, 777u + s);
        syn_load(&part, engine, syn_sound_values(s));
        syn_note_on(&part, note, 0.9f, 0);
        unsigned off_at = frames_total * 3u / 4u;
        for (unsigned f = 0; f < frames_total; f += EL_BLOCK) {
            float l[EL_BLOCK] = {0}, r[EL_BLOCK] = {0}, d[EL_BLOCK] = {0}, v[EL_BLOCK] = {0};
            if (f <= off_at && off_at < f + EL_BLOCK) syn_note_off(&part, note);
            syn_render(&part, l, r, d, v, EL_BLOCK, 120.0f, 1.0f);
            for (unsigned i = 0; i < EL_BLOCK && f + i < frames_total; ++i) mono[f + i] = 0.5f * (l[i] + r[i]);
        }
        char path[600], slug[32];
        unsigned j = 0;
        for (const char *c = syn_sound_name(s); *c && j < 31; ++c) slug[j++] = (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ? *c : '_';
        slug[j] = 0;
        snprintf(path, sizeof path, "%s/%u_%s_%s.f32", argv[1], s, engine == SYN_JP6 ? "jp6" : "mks", slug);
        FILE *fp = fopen(path, "wb");
        if (!fp) return 1;
        fwrite(mono, sizeof mono[0], frames_total, fp);
        fclose(fp);
    }
    return 0;
}
