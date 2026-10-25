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

/* A groove in a style in slot A of every area (the engine is idle). */
static void style_groove(el_engine_t *e, unsigned style, uint32_t seed)
{
    st_pattern_t parts[EL_TRACKS];
    dr_generate(&e->drums.pattern[0], style, 24, seed, NULL);
    el_generate_parts(&e->drums.pattern[0], seed ^ 0x5bd1e995u, parts);
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        e->track[i].pattern[0] = parts[i];
        e->track[i].current = e->track[i].next = 0;
    }
    dr_post(&e->drums, DR_CMD_SELECT, 0, 1, 0, 0);
}

/* Renders slot A; number only names the file (the style it came from). */
static int render_slot(el_engine_t *e, unsigned number, const char *dir, const char *prefix, int drums_only,
                       int strings_only)
{
    char path[512], name[32];
    const unsigned slot = 0;
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
    snprintf(path, sizeof path, "%s/%s%02u_%s.wav", dir, prefix, number + 1, name);
    write_wav(path, audio, frames);
    const st_pattern_t *sp = &e->track[1].pattern[slot];
    printf("%-5s %-10s kit %-8s %5.1f BPM  strings %2u x %-5s %4.1f s  peak %.2f -> %s\n", prefix, e->drums.pattern[slot].name,
           dr_kit_names[e->drums.pattern[slot].kit], bpm, sp->length, st_rate_names[sp->rate], frames / (double)FM1_SAMPLE_RATE,
           peak, path);
    free(audio);
    if (peak < 0.02f || peak > 1.0f) { printf("  level out of range\n"); return 1; }
    return 0;
}

/* Punch-in effects: stutter gates the mix in 16ths; the reverb throw
 * mutes the dry mix and leaves a long hall tail that goes on release. */
static int punch_test(const char *dir)
{
    static el_engine_t e;
    static float out[FM1_SAMPLE_RATE * 12 * 2];
    const unsigned total = FM1_SAMPLE_RATE * 12 / EL_BLOCK * EL_BLOCK;
    el_init(&e);
    el_post(&e, EL_CMD_BPM, 0, 1200);   /* a 16th = 5512.5 frames */
    el_post(&e, EL_CMD_START, 0, 0);
    /* 0-2 s plain, 2-4 s stutter, 4-6 s plain, 6-9 s throw, 9-12 s plain. */
    for (unsigned pos = 0; pos < total; pos += EL_BLOCK) {
        double t = (double)pos / FM1_SAMPLE_RATE;
        if (pos == 2u * FM1_SAMPLE_RATE / EL_BLOCK * EL_BLOCK) el_post3(&e, EL_CMD_PUNCH, EL_PUNCH_STUTTER, 1, 0, 0);
        if (pos == 4u * FM1_SAMPLE_RATE / EL_BLOCK * EL_BLOCK) el_post3(&e, EL_CMD_PUNCH, EL_PUNCH_STUTTER, 0, 0, 0);
        if (pos == 6u * FM1_SAMPLE_RATE / EL_BLOCK * EL_BLOCK) el_post3(&e, EL_CMD_PUNCH, EL_PUNCH_THROW, 1, 0, 0);
        if (pos == 9u * FM1_SAMPLE_RATE / EL_BLOCK * EL_BLOCK) el_post3(&e, EL_CMD_PUNCH, EL_PUNCH_THROW, 0, 0, 0);
        (void)t;
        el_render_float(&e, out + pos * 2u, EL_BLOCK);
    }
    char path[512];
    snprintf(path, sizeof path, "%s/punch_stutter_throw.wav", dir);
    write_wav(path, out, total);
    /* Stutter: second halves of the 16ths (from 2.2 s on) near silent. */
    double open = 0, shut = 0, sixteenth = FM1_SAMPLE_RATE * 15.0 / 120.0;
    for (unsigned i = (unsigned)(2.2 * FM1_SAMPLE_RATE); i < 4u * FM1_SAMPLE_RATE; ++i) {
        double ph = fmod((double)i, sixteenth) / sixteenth, x = out[2 * i] * out[2 * i];
        if (ph > 0.05 && ph < 0.45) open += x; else if (ph > 0.55 && ph < 0.95) shut += x;
    }
    double gate_db = 10 * log10((shut + 1e-12) / (open + 1e-12));
    /* Throw: level 1-3 s after the press (only the hall) against the groove
     * before, and 0.3-1 s after release back to normal. */
    double before = 0, tail = 0, after = 0, normal = 0;
    for (unsigned i = 4u * FM1_SAMPLE_RATE; i < 6u * FM1_SAMPLE_RATE; ++i) before += out[2 * i] * out[2 * i];
    for (unsigned i = 7u * FM1_SAMPLE_RATE; i < 9u * FM1_SAMPLE_RATE; ++i) tail += out[2 * i] * out[2 * i];
    for (unsigned i = (unsigned)(9.3 * FM1_SAMPLE_RATE); i < (unsigned)(11.3 * FM1_SAMPLE_RATE); ++i) after += out[2 * i] * out[2 * i];
    for (unsigned i = 0; i < 2u * FM1_SAMPLE_RATE; ++i) normal += out[2 * i] * out[2 * i];
    double tail_db = 10 * log10((tail + 1e-12) / (before + 1e-12)), back_db = 10 * log10((after + 1e-12) / (normal + 1e-12));
    int ok = gate_db < -25 && tail_db > -30 && tail_db < -3 && fabs(back_db) < 3;
    printf("punch: stutter closed halves %.1f dB, throw tail %.1f dB, after release %+.1f dB: %s\n", gate_db, tail_db, back_db,
           ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static el_engine_t e;
    char path[512];
    int failed = timing_test() | midi_clock_test() | punch_test(dir);

    /* A/B: the same groove without and with the mix / master processing. */
    for (int processed = 0; processed < 2; ++processed) {
        el_init(&e);
        el_post(&e, EL_CMD_MIX, (uint8_t)processed, 0);
        el_post(&e, EL_CMD_BPM, 0, 1280);
        style_groove(&e, 6, 0x2545f497u);
        size_t frames; float peak;
        float *audio = render(&e, 128.0f, 64, &frames, &peak);
        snprintf(path, sizeof path, "%s/ab_rother_%s.wav", dir, processed ? "mixed" : "raw");
        write_wav(path, audio, frames);
        printf("A/B %s peak %.2f -> %s\n", processed ? "mixed" : "raw", peak, path);
        free(audio);
    }

    /* Every style: full groove and beat only. */
    for (unsigned i = 0; i < DR_STYLES; ++i) {
        el_init(&e);
        style_groove(&e, i, 0x2545f491u + i);
        failed |= render_slot(&e, i, dir, "full_", 0, 0);
        el_init(&e);
        style_groove(&e, i, 0x2545f491u + i);
        failed |= render_slot(&e, i, dir, "beat_", 1, 0);
    }
    /* Other power-ups: new seeds give new grooves in the same styles. */
    static const unsigned styles[] = {6, 5, 7, 10, 13};
    for (unsigned seed = 1; seed <= 2; ++seed)
        for (unsigned k = 0; k < sizeof styles / sizeof styles[0]; ++k) {
            char prefix[16];
            el_init(&e);
            style_groove(&e, styles[k], 0x1000193u * seed + 77u + styles[k]);
            snprintf(prefix, sizeof prefix, "var%u_", seed);
            failed |= render_slot(&e, styles[k], dir, prefix, 0, 0);
        }
    /* Strings alone: Rother, Kraftwerk and electro chord loops. */
    static const unsigned chord_styles[] = {6, 0, 10, 2, 5, 15, 7};
    for (unsigned k = 0; k < sizeof chord_styles / sizeof chord_styles[0]; ++k) {
        el_init(&e);
        style_groove(&e, chord_styles[k], 0xabcdefu + k);
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
        style_groove(&e, mixed[k].slot, 0x2545f491u + mixed[k].slot);
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

    /* Distortion send: the same groove dry and with sends (RAT on beat and
     * bass, OD on rhythm). */
    static const struct { uint8_t mode, drive, beat, rhythm, strings, bass; const char *name; } dist[] = {
        {DIST_RAT, 60, 55, 0, 0, 80, "rat"}, {DIST_TUBE, 90, 70, 60, 40, 60, "tube"}, {DIST_OD, 40, 0, 80, 0, 0, "od_rhythm"}};
    for (unsigned k = 0; k < sizeof dist / sizeof dist[0]; ++k) {
        char prefix[32];
        el_init(&e);
        el_post(&e, EL_CMD_FX, FX_DIST_MODE, dist[k].mode);
        el_post(&e, EL_CMD_FX, FX_DIST_DRIVE, dist[k].drive);
        el_post(&e, EL_CMD_FX, FX_DIST_SEND_BEAT, dist[k].beat);
        el_post(&e, EL_CMD_FX, FX_DIST_SEND_RHYTHM, dist[k].rhythm);
        el_post(&e, EL_CMD_FX, FX_DIST_SEND_STRINGS, dist[k].strings);
        el_post(&e, EL_CMD_FX, FX_DIST_SEND_BASS, dist[k].bass);
        snprintf(prefix, sizeof prefix, "dist_%s_", dist[k].name);
        style_groove(&e, 6, 0x2545f497u);
        failed |= render_slot(&e, 6, dir, prefix, 0, 0);
    }

    /* Arpeggiator on RHYTHM instead of its sequence: a latched minor chord
     * in the key of the groove, two styles. */
    static const struct { uint8_t mode, rate, octaves; unsigned slot; } arps[] = {{2, 2, 1, 6}, {3, 5, 2, 0}, {0, 1, 1, 13}};
    for (unsigned k = 0; k < sizeof arps / sizeof arps[0]; ++k) {
        char prefix[16];
        el_init(&e);
        unsigned slot = arps[k].slot;
        style_groove(&e, slot, 0x2545f491u + slot);
        float buf[EL_BLOCK * 2];
        el_render_float(&e, buf, EL_BLOCK);
        el_post3(&e, EL_CMD_TRACK_CLEAR, 0, 0, 0, 0);
        el_post3(&e, EL_CMD_TRACK_ARP, 0, ARP_MODE, 0, arps[k].mode);
        el_post3(&e, EL_CMD_TRACK_ARP, 0, ARP_RATE, 0, arps[k].rate);
        el_post3(&e, EL_CMD_TRACK_ARP, 0, ARP_OCTAVES, 0, arps[k].octaves);
        el_post3(&e, EL_CMD_TRACK_ARP, 0, ARP_HOLD, 0, 1);
        el_post3(&e, EL_CMD_TRACK_ARP, 0, ARP_ON, 0, 1);
        int root = 48;
        const st_pattern_t *bp = &e.track[2].pattern[0];
        for (unsigned i = 0; i < bp->length; ++i)
            if (bp->step[i].flags & ST_ON) { root = 48 + bp->step[i].note % 12; break; }
        static const int chord[3] = {0, 3, 7};
        for (unsigned i = 0; i < 3; ++i) el_post3(&e, EL_CMD_NOTE, 0, (uint8_t)(root + chord[i]), 0, 100);
        for (unsigned i = 0; i < 3; ++i) el_post3(&e, EL_CMD_NOTE, 0, (uint8_t)(root + chord[i]), 0, 0);
        el_render_float(&e, buf, EL_BLOCK);
        snprintf(prefix, sizeof prefix, "arp_%s_", arp_mode_names[arps[k].mode]);
        for (char *c = prefix; *c; ++c) if (*c == '/') *c = '-';
        failed |= render_slot(&e, slot, dir, prefix, 0, 0);
    }

    /* The PO-12 kit on a few grooves (beat only and full). */
    static const unsigned po_slots[] = {6, 13, 7};
    for (unsigned k = 0; k < sizeof po_slots / sizeof po_slots[0]; ++k)
        for (int full = 0; full < 2; ++full) {
            unsigned slot = po_slots[k];
            el_init(&e);
            style_groove(&e, slot, 0x2545f491u + slot);
            float buf[EL_BLOCK * 2];
            el_render_float(&e, buf, EL_BLOCK);
            dr_post(&e.drums, DR_CMD_KIT, 4, 0, 0, 0);    /* PO-12 */
            failed |= render_slot(&e, slot, dir, full ? "po12_full_" : "po12_beat_", !full, 0);
        }

    /* Cost: a busy groove with all four areas for 60 seconds. */
    el_init(&e);
    el_post(&e, EL_CMD_BPM, 0, 1400);
    style_groove(&e, 7, 0x2545f498u);
    el_post(&e, EL_CMD_START, 0, 0);
    float buf[EL_BLOCK * 2];
    unsigned blocks = 60u * FM1_SAMPLE_RATE / EL_BLOCK;
    clock_t start = clock();
    for (unsigned b = 0; b < blocks; ++b) el_render_float(&e, buf, EL_BLOCK);
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("cost: 60 s of audio in %.3f s on this host (%.2f%% of real time)\n", seconds, seconds / 60.0 * 100.0);
    /* The same with all four distortion sends open. */
    el_init(&e);
    el_post(&e, EL_CMD_BPM, 0, 1400);
    for (unsigned p = FX_DIST_SEND_BEAT; p <= FX_DIST_SEND_BASS; ++p) el_post(&e, EL_CMD_FX, (uint8_t)p, 80);
    style_groove(&e, 7, 0x2545f498u);
    el_post(&e, EL_CMD_START, 0, 0);
    start = clock();
    for (unsigned b = 0; b < blocks; ++b) el_render_float(&e, buf, EL_BLOCK);
    seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("cost with distortion send: %.2f%% of real time\n", seconds / 60.0 * 100.0);
    return failed;
}
