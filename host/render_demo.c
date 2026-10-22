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

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static const uint16_t tempo[DR_PATTERNS] = {1120, 1180, 1250, 1160, 1240, 1280, 1280, 1300,
                                                1380, 1240, 1360, 1200, 1240, 1250, 1200, 1200,
                                                1270, 1280, 1250, 1200, 1200, 1200, 1200, 1200};
    static el_engine_t e;
    char path[512], name[32];
    int failed = timing_test() | midi_clock_test();

    /* A/B: the same groove without and with the mix / master processing. */
    for (int processed = 0; processed < 2; ++processed) {
        el_init(&e);
        el_post(&e, EL_CMD_MIX, (uint8_t)processed, 0);
        el_post(&e, EL_CMD_BPM, 0, 1280);
        dr_post(&e.drums, DR_CMD_SELECT, 6, 1, 0, 0);
        dr_post(&e.drums, DR_CMD_KIT, e.drums.pattern[6].kit, 0, 0, 0);
        size_t frames; float peak;
        float *audio = render(&e, 128.0f, 64, &frames, &peak);
        snprintf(path, sizeof path, "%s/ab_rother_%s.wav", dir, processed ? "mixed" : "raw");
        write_wav(path, audio, frames);
        printf("A/B %s peak %.2f -> %s\n", processed ? "mixed" : "raw", peak, path);
        free(audio);
    }

    /* Full grooves: beat plus rhythm, strings and bass. */
    for (unsigned i = 0; i < DR_PATTERNS; i += 1) {
        el_init(&e);
        el_post(&e, EL_CMD_BPM, 0, (int16_t)tempo[i]);
        dr_post(&e.drums, DR_CMD_SELECT, (uint8_t)i, 1, 0, 0);
        dr_post(&e.drums, DR_CMD_KIT, e.drums.pattern[i].kit, 0, 0, 0);
        unsigned length = e.drums.pattern[i].length;
        size_t frames; float peak;
        float *audio = render(&e, tempo[i] / 10.0f, length * (length > 16 ? 2u : 4u), &frames, &peak);
        slug(name, e.drums.pattern[i].name);
        snprintf(path, sizeof path, "%s/full_%02u_%s.wav", dir, i + 1, name);
        write_wav(path, audio, frames);
        printf("full %-10s peak %.2f -> %s\n", e.drums.pattern[i].name, peak, path);
        if (peak < 0.05f || peak > 1.0f) { printf("  level out of range\n"); failed = 1; }
        free(audio);
    }

    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        el_init(&e);
        for (unsigned t = 0; t < EL_TRACKS; ++t) el_post3(&e, EL_CMD_TRACK_MUTE, (uint8_t)t, 1, 0, 0);
        if (!e.drums.pattern[i].hit[D8_BD] && !e.drums.pattern[i].hit[D8_CH]) continue;
        el_post(&e, EL_CMD_BPM, 0, (int16_t)tempo[i]);
        dr_post(&e.drums, DR_CMD_SELECT, (uint8_t)i, 1, 0, 0);
        dr_post(&e.drums, DR_CMD_KIT, e.drums.pattern[i].kit, 0, 0, 0);
        unsigned length = e.drums.pattern[i].length;
        size_t frames; float peak;
        float *audio = render(&e, tempo[i] / 10.0f, length * (length > 16 ? 2u : 4u), &frames, &peak);
        slug(name, e.drums.pattern[i].name);
        snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, i + 1, name);
        write_wav(path, audio, frames);
        printf("%-10s kit %-8s %5.1f BPM  %4.1f s  peak %.2f  -> %s\n", e.drums.pattern[i].name,
               dr_kit_names[e.drums.pattern[i].kit], tempo[i] / 10.0, frames / (double)FM1_SAMPLE_RATE, peak, path);
        if (peak < 0.05f || peak > 1.0f) { printf("  level out of range\n"); failed = 1; }
        free(audio);
    }

    for (unsigned s = 0; s < DR_STYLES; ++s) {
        el_init(&e);
        for (unsigned t = 0; t < EL_TRACKS; ++t) el_post3(&e, EL_CMD_TRACK_MUTE, (uint8_t)t, 1, 0, 0);
        dr_pattern_t p;
        dr_factory_pattern(14, &p);
        p.length = 32;
        dr_generate(&p, s, 72, 0x51ed270bu + s * 7919u, NULL);
        dr_post_pattern(&e.drums, 0, &p);
        dr_post(&e.drums, DR_CMD_KIT, p.kit, 0, 0, 0);
        float bpm = s == DR_STYLE_MIAMI ? 130.0f : 120.0f;
        el_post(&e, EL_CMD_BPM, 0, (int16_t)(bpm * 10));
        size_t frames; float peak;
        float *audio = render(&e, bpm, 64, &frames, &peak);
        slug(name, dr_style_names[s]);
        snprintf(path, sizeof path, "%s/gen_%s.wav", dir, name);
        write_wav(path, audio, frames);
        printf("generator %-8s peak %.2f  -> %s\n", dr_style_names[s], peak, path);
        free(audio);
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
