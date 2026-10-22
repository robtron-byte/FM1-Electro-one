/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host tool: render the factory grooves and generator styles to WAV files,
 * check step timing and measure render cost.
 * Usage: render_demo <output directory> */
#include "electro.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int write_wav(const char *path, const float *data, size_t frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t bytes = (uint32_t)(frames * 4u), rate = FM1_SAMPLE_RATE, byte_rate = rate * 4u;
    uint32_t riff = 36u + bytes, fmt_size = 16u;
    uint16_t pcm = 1, channels = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt_size, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&channels, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&byte_rate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    for (size_t i = 0; i < frames * 2u; ++i) {
        float x = data[i] < -1 ? -1 : data[i] > 1 ? 1 : data[i];
        int16_t s = (int16_t)lrintf(x * 32767.0f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
    return 0;
}

/* Render whole loops of the current pattern plus a short tail. */
static float *render(el_engine_t *e, float bpm, unsigned steps, size_t *frames_out, float *peak)
{
    size_t frames = (size_t)((double)steps * FM1_SAMPLE_RATE * 15.0 / bpm) + FM1_SAMPLE_RATE / 2;
    frames = (frames + EL_BLOCK - 1) / EL_BLOCK * EL_BLOCK;
    float *out = calloc(frames * 2u, sizeof(float));
    el_post(e, EL_CMD_START, 0, 0);
    *peak = 0;
    for (size_t pos = 0; pos < frames; pos += EL_BLOCK) {
        if (pos >= frames - FM1_SAMPLE_RATE / 2 && e->transport.running) el_post(e, EL_CMD_STOP, 0, 0);
        el_render_float(e, out + pos * 2u, EL_BLOCK);
    }
    for (size_t i = 0; i < frames * 2u; ++i) if (fabsf(out[i]) > *peak) *peak = fabsf(out[i]);
    *frames_out = frames;
    return out;
}

static void slug(char *dst, const char *name)
{
    for (; *name; ++name) *dst++ = (*name == ' ') ? '_' : (char)((*name >= 'A' && *name <= 'Z') ? *name + 32 : *name);
    *dst = 0;
}

/* The step for each 16th must start on the frame the clock predicts. */
static int timing_test(void)
{
    static el_engine_t e;
    el_init(&e);
    el_post(&e, EL_CMD_BPM, 0, 1234);
    dr_post(&e.drums, DR_CMD_SWING, 0, 62, 0, 0);
    el_post(&e, EL_CMD_START, 0, 0);
    double expected = 0, pair = 2.0 * FM1_SAMPLE_RATE * 15.0 / 123.4;
    uint32_t last_step = 0;
    long frame = 0;
    int errors = 0;
    for (int block = 0; block < 4000; ++block) {
        float buf[EL_BLOCK * 2];
        for (int i = 0; i < EL_BLOCK; ++i) {
            /* Render one frame at a time to observe the exact firing frame. */
            uint32_t before = e.transport.step;
            el_render_float(&e, buf, 1);
            if (e.transport.step != before) {
                long want = (long)ceil(expected - 1e-6);
                if (labs(frame - want) > 1) {
                    if (errors < 5) printf("  step %u at frame %ld, expected %ld\n", before, frame, want);
                    ++errors;
                }
                expected += pair * ((before & 1u) ? 0.38 : 0.62);
                last_step = before;
            }
            ++frame;
        }
    }
    printf("timing: %u steps checked at 123.4 BPM, swing 62%%: %s\n", last_step + 1, errors ? "FAIL" : "ok");
    return errors ? 1 : 0;
}

/* MIDI clock: Start plus F8 at 120 BPM must give 8 steps per second and
 * a measured tempo of 120. */
static int midi_clock_test(void)
{
    static el_engine_t e;
    const uint8_t start = 0xfa, tick = 0xf8;
    el_init(&e);
    el_midi_input(&e, &start, 1);
    double next_tick = 0, tick_frames = FM1_SAMPLE_RATE * 60.0 / (120.0 * 24.0);
    float buf[EL_BLOCK * 2];
    long frame = 0;
    for (int block = 0; block < (int)(2 * FM1_SAMPLE_RATE / EL_BLOCK); ++block) {
        while (next_tick < frame + EL_BLOCK) { el_midi_input(&e, &tick, 1); next_tick += tick_frames; }
        el_render_float(&e, buf, EL_BLOCK);
        frame += EL_BLOCK;
    }
    int ok = e.transport.source == EL_CLOCK_MIDI && e.transport.step >= 15 && e.transport.step <= 17 &&
             e.transport.bpm > 118.0f && e.transport.bpm < 122.0f;
    printf("midi clock: %u steps in 2 s, %.1f BPM measured: %s\n", e.transport.step, e.transport.bpm, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static unsigned groove_steps(const el_engine_t *e, unsigned slot)
{
    /* At least 8 bars, and whole loops of the strings sequence. */
    unsigned bars = 8;
    const st_pattern_t *p = &e->track[1].pattern[slot];
    static const unsigned rate_steps[ST_RATES] = {1, 2, 4, 8, 16, 32};
    unsigned strings = p->length * rate_steps[p->rate < ST_RATES ? p->rate : 0];
    if (strings > bars * 16u) bars = (strings + 15u) / 16u;
    (void)e->drums.pattern[slot].length;
    return bars * 16u;
}

static int render_slot(el_engine_t *e, unsigned slot, const char *dir, const char *prefix, int drums_only,
                       int strings_only)
{
    char path[512], name[32];
    float bpm = dr_styles[e->drums.pattern[slot].style].bpm_x10 / 10.0f;
    el_post(e, EL_CMD_BPM, 0, (int16_t)(bpm * 10.0f + 0.5f));
    dr_post(&e->drums, DR_CMD_SELECT, (uint8_t)slot, 1, 0, 0);
    for (unsigned t = 0; t < EL_TRACKS; ++t)
        el_post3(e, EL_CMD_TRACK_MUTE, (uint8_t)t, (uint8_t)(drums_only || (strings_only && t != 1)), 0, 0);
    if (strings_only) dr_post(&e->drums, DR_CMD_AREA_MUTE, 0, 1, 0, 0);
    size_t frames; float peak;
    float *audio = render(e, bpm, drums_only ? e->drums.pattern[slot].length * (e->drums.pattern[slot].length > 16 ? 2u : 4u)
                                              : groove_steps(e, slot), &frames, &peak);
    slug(name, e->drums.pattern[slot].name);
    snprintf(path, sizeof path, "%s/%s%02u_%s.wav", dir, prefix, slot + 1, name);
    write_wav(path, audio, frames);
    const st_pattern_t *sp = &e->track[1].pattern[slot];
    printf("%-5s %-10s kit %-8s %5.1f BPM  strings %2u x %-5s %4.1f s  peak %.2f -> %s\n", prefix, e->drums.pattern[slot].name,
           dr_kit_names[e->drums.pattern[slot].kit], bpm, sp->length, st_rate_names[sp->rate], frames / (double)FM1_SAMPLE_RATE,
           peak, path);
    free(audio);
    if (peak < 0.02f || peak > 1.0f) { printf("  level out of range\n"); return 1; }
    return 0;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static el_engine_t e;
    char path[512];
    int failed = timing_test() | midi_clock_test();

    /* A/B: the same groove without and with the mix / master processing. */
    for (int processed = 0; processed < 2; ++processed) {
        el_init(&e);
        el_post(&e, EL_CMD_MIX, (uint8_t)processed, 0);
        el_post(&e, EL_CMD_BPM, 0, 1280);
        dr_post(&e.drums, DR_CMD_SELECT, 6, 1, 0, 0);
        size_t frames; float peak;
        float *audio = render(&e, 128.0f, 64, &frames, &peak);
        snprintf(path, sizeof path, "%s/ab_rother_%s.wav", dir, processed ? "mixed" : "raw");
        write_wav(path, audio, frames);
        printf("A/B %s peak %.2f -> %s\n", processed ? "mixed" : "raw", peak, path);
        free(audio);
    }

    /* Every style slot as generated at power-up: full groove and beat only. */
    for (unsigned i = 0; i < DR_STYLE_SLOTS; ++i) {
        el_init(&e);
        failed |= render_slot(&e, i, dir, "full_", 0, 0);
        el_init(&e);
        failed |= render_slot(&e, i, dir, "beat_", 1, 0);
    }
    /* Other power-ups: new seeds give new grooves in the same styles. */
    static const unsigned styles[] = {6, 5, 7, 10, 13};
    for (unsigned seed = 1; seed <= 2; ++seed)
        for (unsigned k = 0; k < sizeof styles / sizeof styles[0]; ++k) {
            char prefix[16];
            el_init(&e);
            el_generate_all(&e, 0x1000193u * seed + 77u);
            snprintf(prefix, sizeof prefix, "var%u_", seed);
            failed |= render_slot(&e, styles[k], dir, prefix, 0, 0);
        }
    /* Strings alone: Rother, Kraftwerk and electro chord loops. */
    static const unsigned chord_styles[] = {6, 0, 10, 2, 5, 15, 7};
    for (unsigned k = 0; k < sizeof chord_styles / sizeof chord_styles[0]; ++k) {
        el_init(&e);
        el_generate_all(&e, 0xabcdefu + k);
        failed |= render_slot(&e, chord_styles[k], dir, "chords_", 0, 1);
    }

    /* Multitimbral with both engines: JP-6 sounds on some areas. */
    static const struct { unsigned slot; const char *sound[EL_TRACKS]; } mixed[] = {
        {6, {"SEQ PHRYG", "JP STRINGS", "DETROIT B"}},
        {5, {"COMPUTER", "JP6 STRNG", "ROBO BASS"}},
        {10, {"SEQ EBM", "JP SLOWSTR", "SYNC GROWL"}},
    };
    for (unsigned k = 0; k < sizeof mixed / sizeof mixed[0]; ++k) {
        el_init(&e);
        for (unsigned t = 0; t < EL_TRACKS; ++t) {
            int s = syn_sound_find(SYN_JP6, mixed[k].sound[t]);
            if (s < 0) s = syn_sound_find(SYN_MKS, mixed[k].sound[t]);
            if (s < 0) { printf("missing sound %s\n", mixed[k].sound[t]); failed = 1; continue; }
            float buf[EL_BLOCK * 2];
            el_load_preset(&e, t, (unsigned)s);
            el_render_float(&e, buf, EL_BLOCK);
        }
        failed |= render_slot(&e, mixed[k].slot, dir, "jp6mix_", 0, 0);
    }

    /* Cost: a busy groove with all four areas for 60 seconds. */
    el_init(&e);
    el_post(&e, EL_CMD_BPM, 0, 1400);
    dr_post(&e.drums, DR_CMD_SELECT, 7, 1, 0, 0);
    el_post(&e, EL_CMD_START, 0, 0);
    float buf[EL_BLOCK * 2];
    unsigned blocks = 60u * FM1_SAMPLE_RATE / EL_BLOCK;
    clock_t start = clock();
    for (unsigned b = 0; b < blocks; ++b) el_render_float(&e, buf, EL_BLOCK);
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("cost: 60 s of audio in %.3f s on this host (%.2f%% of real time)\n", seconds, seconds / 60.0 * 100.0);
    return failed;
}
