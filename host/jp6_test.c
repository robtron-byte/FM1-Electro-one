/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host test: play every JP-6 preset (a chord and a single note), check the
 * output, and compare the cost per voice with the MKS-50 engine. */
#include "jp6.h"
#include "mks.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Both engines share one union per track: the JP-6 part may not be larger. */
_Static_assert(sizeof(jp6_part_t) <= sizeof(mks_part_t), "jp6_part_t must fit in mks_part_t");

enum { SECONDS_FRAMES = 44100 * 3 / 2, HOLD_FRAMES = 44100 };

typedef struct {
    double seconds;       /* render time */
    double voice_frames;  /* sum of active voices x frames */
    float peak;
    int bad;
} result_t;

static float wav[SECONDS_FRAMES * 2 * 2];

static void write_wav(const char *path, const float *data, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t bytes = frames * 4, rate = 44100, br = rate * 4, riff = 36 + bytes, fs = 16;
    uint16_t pcm = 1, ch = 2, al = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fs, 4, 1, f);
    fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    for (uint32_t i = 0; i < frames * 2; ++i) {
        float x = data[i] < -1 ? -1 : data[i] > 1 ? 1 : data[i];
        int16_t s = (int16_t)lrintf(x * 32767);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

/* Plays notes for HOLD_FRAMES, then lets them ring out; out (optional)
 * receives SECONDS_FRAMES stereo frames. */
#define PLAY(prefix, part, notes, count, out, res)                                                       \
    do {                                                                                                 \
        float l[EL_BLOCK], r[EL_BLOCK], d[EL_BLOCK], v[EL_BLOCK], *o_ = (out);                           \
        for (int k = 0; k < (count); ++k) prefix##_note_on((part), (notes)[k], 0.8f, 0);                 \
        for (int frame = 0; frame < SECONDS_FRAMES; frame += EL_BLOCK) {                                 \
            if (frame <= HOLD_FRAMES && frame + EL_BLOCK > HOLD_FRAMES)                                  \
                for (int k = 0; k < (count); ++k) prefix##_note_off((part), (notes)[k]);                 \
            memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(d, 0, sizeof d); memset(v, 0, sizeof v); \
            unsigned voices = 0;                                                                         \
            for (unsigned i = 0; i < (part)->voices; ++i) voices += (part)->voice[i].active;               \
            clock_t start = clock();                                                                     \
            prefix##_render((part), l, r, d, v, EL_BLOCK, 120);                                          \
            (res)->seconds += (double)(clock() - start) / CLOCKS_PER_SEC;                                \
            (res)->voice_frames += (double)voices * EL_BLOCK;                                            \
            for (int i = 0; i < EL_BLOCK && frame + i < SECONDS_FRAMES; ++i) {                           \
                if (!isfinite(l[i]) || !isfinite(r[i]) || !isfinite(d[i]) || !isfinite(v[i])) (res)->bad = 1; \
                if (fabsf(l[i]) > (res)->peak) (res)->peak = fabsf(l[i]);                                \
                if (fabsf(r[i]) > (res)->peak) (res)->peak = fabsf(r[i]);                                \
                if (o_) { o_[2 * (frame + i)] = l[i]; o_[2 * (frame + i) + 1] = r[i]; }                  \
            }                                                                                            \
        }                                                                                                \
    } while (0)

static void slug(char *out, const char *name)
{
    unsigned j = 0;
    for (const char *c = name; *c && j < 31; ++c)
        out[j++] = (*c >= 'A' && *c <= 'Z') ? (char)(*c | 32) : (*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ? *c : '_';
    out[j] = 0;
}

int main(int argc, char **argv)
{
    static jp6_part_t jp;
    static mks_part_t mks;
    const char *dir = argc > 1 ? argv[1] : 0;
    const int chord[3] = {48, 51, 55}, single[1] = {48}, six[6] = {48, 51, 55, 58, 62, 65};
    int failed = 0;
    result_t total = {0, 0, 0, 0};
    printf("sizeof(jp6_part_t) %u, sizeof(mks_part_t) %u\n", (unsigned)sizeof(jp6_part_t), (unsigned)sizeof(mks_part_t));
    unsigned areas[4] = {0, 0, 0, 0};
    float area_peak[4] = {0, 0, 0, 0}, mks_peak[4] = {0, 0, 0, 0};
    unsigned mks_areas[4] = {0, 0, 0, 0};
    for (unsigned n = 0; n < jp6_preset_count; ++n) {
        const jp6_preset_t *pr = &jp6_presets[n];
        ++areas[pr->area & 3u];
        int base = pr->area == JP6_AREA_BASS ? -12 : 0;
        int notes[3] = {chord[0] + base, chord[1] + base, chord[2] + base}, one[1] = {single[0] + base};
        result_t a = {0, 0, 0, 0}, b = {0, 0, 0, 0};
        jp6_part_init(&jp, 6, 1234 + n);
        jp6_part_load(&jp, pr->v);
        PLAY(jp6, &jp, notes, pr->v[JP_KEY_MODE] < 2 ? 3 : 1, wav, &a);
        PLAY(jp6, &jp, one, 1, wav + SECONDS_FRAMES * 2, &b);
        float peak = a.peak > b.peak ? a.peak : b.peak;
        area_peak[pr->area & 3u] += peak;
        int bad = a.bad || b.bad || peak < 0.01f || peak > 2.0f;
        total.seconds += a.seconds + b.seconds;
        total.voice_frames += a.voice_frames + b.voice_frames;
        printf("%-10s area %u  peak chord %.2f single %.2f  %s\n", pr->name, pr->area, a.peak, b.peak,
               a.bad || b.bad ? "NaN/Inf" : bad ? "LEVEL" : "ok");
        failed |= bad;
        if (dir) {
            char path[512], s[32];
            slug(s, pr->name);
            snprintf(path, sizeof path, "%s/jp6_%02u_%s.wav", dir, n, s);
            write_wav(path, wav, SECONDS_FRAMES * 2);
        }
    }
    printf("presets: %u (rhythm %u, strings %u, bass %u)\n", jp6_preset_count, areas[1], areas[2], areas[3]);

    /* Same chord and note through every MKS-50 preset. */
    result_t m = {0, 0, 0, 0};
    for (unsigned n = 0; n < mks_preset_count; ++n) {
        const mks_preset_t *pr = &mks_presets[n];
        int base = pr->area == MKS_AREA_BASS ? -12 : 0;
        int notes[3] = {chord[0] + base, chord[1] + base, chord[2] + base}, one[1] = {single[0] + base};
        mks_part_init(&mks, 6, 1234 + n);
        mks_part_load(&mks, pr->v);
        float before = m.peak;
        m.peak = 0;
        PLAY(mks, &mks, notes, pr->v[MP_KEY_MODE] < 2 ? 3 : 1, (float *)0, &m);
        PLAY(mks, &mks, one, 1, (float *)0, &m);
        mks_peak[pr->area & 3u] += m.peak;
        ++mks_areas[pr->area & 3u];
        m.peak = m.peak > before ? m.peak : before;
    }
    for (unsigned a = 1; a < 4; ++a)
        printf("area %u mean peak: jp6 %.2f, mks %.2f\n", a, area_peak[a] / (areas[a] ? areas[a] : 1),
               mks_peak[a] / (mks_areas[a] ? mks_areas[a] : 1));
    double jp_ns = total.seconds / total.voice_frames * 1e9, mks_ns = m.seconds / m.voice_frames * 1e9;
    printf("cost, all presets: jp6 %.1f ns, mks %.1f ns per voice-sample (jp6/mks %.2f)\n", jp_ns, mks_ns, jp_ns / mks_ns);

    /* Six sustained voices, plain pad sound of each engine (part effects included). */
    const char *jp_names[] = {"JP STRINGS", "SYNC BRASS", "SEQ EBM"};
    for (unsigned k = 0; k < sizeof jp_names / sizeof jp_names[0]; ++k) {
        for (unsigned n = 0; n < jp6_preset_count; ++n) {
            if (strcmp(jp6_presets[n].name, jp_names[k])) continue;
            result_t a = {0, 0, 0, 0};
            uint8_t v[JP_COUNT];
            memcpy(v, jp6_presets[n].v, sizeof v);
            v[JP_KEY_MODE] = 0;
            jp6_part_init(&jp, 6, 7);
            jp6_part_load(&jp, v);
            PLAY(jp6, &jp, six, 6, (float *)0, &a);
            printf("6 voices %-10s %.1f ns per voice-sample, %.2f%% real time per voice\n", jp_names[k],
                   a.seconds / a.voice_frames * 1e9, a.seconds / a.voice_frames * 44100 * 100);
        }
    }
    for (unsigned n = 0; n < mks_preset_count; ++n) {
        if (strcmp(mks_presets[n].name, "JX STRINGS")) continue;
        result_t a = {0, 0, 0, 0};
        uint8_t v[MP_COUNT];
        memcpy(v, mks_presets[n].v, sizeof v);
        v[MP_KEY_MODE] = 0;
        mks_part_init(&mks, 6, 7);
        mks_part_load(&mks, v);
        PLAY(mks, &mks, six, 6, (float *)0, &a);
        printf("6 voices MKS %-10s %.1f ns per voice-sample, %.2f%% real time per voice\n", "JX STRINGS",
               a.seconds / a.voice_frames * 1e9, a.seconds / a.voice_frames * 44100 * 100);
    }
    printf(failed ? "jp6 test FAILED\n" : "jp6 test ok\n");
    return failed;
}
