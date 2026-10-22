/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Sound check: every 808 instrument on its own (normal, accent, and across
 * its tune / decay range) plus a kit roll, as WAV files for listening and
 * for host/analyze_808.py. Usage: soundcheck <output directory> */
#include "electro.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static el_engine_t e;

static void fresh(void)
{
    el_init(&e);
    for (unsigned t = 0; t < EL_TRACKS; ++t) el_post3(&e, EL_CMD_TRACK_MUTE, (uint8_t)t, 1, 0, 0);
    dr_post(&e.drums, DR_CMD_KIT, 0, 0, 0, 0); /* TR-808 kit */
    el_post(&e, EL_CMD_MIX, 0, 0);            /* dry: the instruments alone */
}

/* Hits: velocity per hit (0 = rest), one hit every `spacing` seconds. */
static void hits(const char *dir, const char *name, unsigned part, const float *velocity, unsigned count,
                 float spacing, const int16_t *param_values, int param)
{
    size_t frames = (size_t)((count * spacing + 0.5f) * FM1_SAMPLE_RATE);
    frames = (frames + EL_BLOCK - 1) / EL_BLOCK * EL_BLOCK;
    float *out = calloc(frames * 2u, sizeof(float));
    size_t next = 0;
    unsigned n = 0;
    for (size_t pos = 0; pos < frames; pos += EL_BLOCK) {
        if (n < count && pos >= next) {
            if (param >= 0 && param_values) dr_post(&e.drums, DR_CMD_PARAM, (uint8_t)part, (uint8_t)param, 0, param_values[n]);
            if (velocity[n] > 0) dr_post(&e.drums, DR_CMD_HIT, (uint8_t)part, (uint8_t)(velocity[n] * 127), 0, 0);
            ++n;
            next = (size_t)(n * spacing * FM1_SAMPLE_RATE);
        }
        el_render_float(&e, out + pos * 2u, EL_BLOCK);
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    write_wav(path, out, frames);
    free(out);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static const char *const names[DR_PARTS] = {"bd", "sd", "lt", "ht", "rs", "cp", "cb", "cy", "oh", "ch", "zp", "pu"};
    const float normal_accent[4] = {0.66f, 1.0f, 0.66f, 1.0f};
    char name[64];
    for (unsigned part = 0; part < DR_PARTS; ++part) {
        /* normal, accent, normal, accent */
        fresh();
        snprintf(name, sizeof name, "%02u_%s_hits", part + 1u, names[part]);
        hits(dir, name, part, normal_accent, 4, part == D8_CY || part == D8_OH ? 1.6f : 0.9f, NULL, -1);
        /* parameter sweeps: tune (or tone) and decay, 5 values each */
        int tune = -1, decay = -1;
        for (int p = 0; p < tr808_nparams((int)part); ++p) {
            const fm1_param_t *d = tr808_param((int)part, p);
            if (!strcmp(d->name, "Tune")) tune = p;
            if (!strcmp(d->name, "Decay")) decay = p;
        }
        const int16_t sweep[5] = {0, 32, 64, 96, 127};
        const float vel[5] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
        if (tune >= 0) {
            fresh();
            snprintf(name, sizeof name, "%02u_%s_tune_sweep", part + 1u, names[part]);
            hits(dir, name, part, vel, 5, 0.9f, sweep, tune);
        }
        if (decay >= 0) {
            fresh();
            snprintf(name, sizeof name, "%02u_%s_decay_sweep", part + 1u, names[part]);
            hits(dir, name, part, vel, 5, part == D8_BD || part == D8_CY || part == D8_OH ? 1.8f : 0.9f, sweep, decay);
        }
    }
    /* Kit roll: every instrument once, normal velocity. */
    fresh();
    size_t frames = (size_t)(DR_PARTS * 0.5f * FM1_SAMPLE_RATE) + FM1_SAMPLE_RATE;
    frames = (frames + EL_BLOCK - 1) / EL_BLOCK * EL_BLOCK;
    float *out = calloc(frames * 2u, sizeof(float));
    for (size_t pos = 0; pos < frames; pos += EL_BLOCK) {
        unsigned k = (unsigned)(pos / (FM1_SAMPLE_RATE / 2));
        if (pos % (FM1_SAMPLE_RATE / 2) < EL_BLOCK && k < DR_PARTS) dr_post(&e.drums, DR_CMD_HIT, (uint8_t)k, 84, 0, 0);
        el_render_float(&e, out + pos * 2u, EL_BLOCK);
    }
    char path[512];
    snprintf(path, sizeof path, "%s/00_kit_roll.wav", dir);
    write_wav(path, out, frames);
    free(out);
    printf("sound check written to %s\n", dir);
    return 0;
}
