/* SPDX-License-Identifier: GPL-3.0-only
 * jx8p: Host-Test über baustein.h (docs/adr/0046).
 *  1. Jedes Preset (eigene zuerst, dann die Werks-Sounds) spielt über jx8p_baustein einen Akkord und endet; dasselbe
 *     direkt über engine/jx8p.c (wie FM1-Electros synth.c) muss Sample für Sample gleich sein (bitgleich).
 *  2. Prüfsumme (FNV-1a über die float-Bits) je Preset gegen ref/sums.txt: so fällt jede Klangänderung im Kern auf.
 *     test <ref> <wav-ordner> write  schreibt ref/sums.txt neu.
 *  3. Werte der eigenen Presets im Bereich, Parameter-Schnittstelle (0..1 -> Rohwert), Akkord-Modus, Sends.
 *  Schreibt je eigenem Preset eine WAV zum Anhören.
 *  last: misst die Rechenzeit je Stimme und Sample. */
#include "jx8p.h"
#include "jx8p_bs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { SR = 44100, ON_MS = 600, TOTAL_MS = 1600, FRAMES = SR * TOTAL_MS / 1000, MAXP = 128 };
static const int chord[3] = {60, 64, 67};
static float a_l[FRAMES], a_r[FRAMES], b_l[FRAMES], b_r[FRAMES];
static unsigned char st_mem[1 << 17] __attribute__((aligned(16)));
static int fails;

#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static uint32_t fnv(const float *l, const float *r, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; ++i) {
        uint32_t w[2];
        memcpy(&w[0], &l[i], 4); memcpy(&w[1], &r[i], 4);
        for (int k = 0; k < 2; ++k) for (int b = 0; b < 4; ++b) { h ^= (w[k] >> (8 * b)) & 0xffu; h *= 16777619u; }
    }
    return h;
}

/* Über die Baustein-Schnittstelle; Blöcke ungleich lang (render teilt selbst in EL_BLOCK). */
static void play_baustein(unsigned preset, float *l, float *r)
{
    const baustein_t *b = &jx8p_baustein;
    memset(l, 0, FRAMES * sizeof *l); memset(r, 0, FRAMES * sizeof *r);
    b->init(st_mem, SR);
    jx8p_bs_load_preset(st_mem, preset);
    int on = SR * ON_MS / 1000 / EL_BLOCK * EL_BLOCK;
    for (int i = 0; i < 3; ++i) b->note_on(st_mem, chord[i], 100);
    b->render(st_mem, l, r, on);
    for (int i = 0; i < 3; ++i) b->note_off(st_mem, chord[i]);
    b->render(st_mem, l + on, r + on, FRAMES - on);
}

/* Direkt über den Kern, wie engine/synth.c und synth_track.c ihn aufrufen. */
static void play_engine(unsigned preset, float *l, float *r)
{
    static jx8p_part_t p;
    static float d[EL_BLOCK], v[EL_BLOCK];
    memset(l, 0, FRAMES * sizeof *l); memset(r, 0, FRAMES * sizeof *r);
    jx8p_part_init(&p, JX8P_BS_VOICES, 1u);
    jx8p_part_load(&p, jx8p_bs_preset_values(0));
    jx8p_part_load(&p, jx8p_bs_preset_values(preset));
    int on = SR * ON_MS / 1000 / EL_BLOCK * EL_BLOCK;
    const unsigned char *pv = jx8p_bs_preset_values(preset);
    for (int pos = 0; pos < FRAMES; pos += EL_BLOCK) {
        if (pos == 0) for (int i = 0; i < 3; ++i) {
            if (pv[JX_KEY_MODE] == 1) {
                for (unsigned k = 0; k < MKS_CHORD_NOTES && mks_chords[pv[JX_CHORD]][k] != -128; ++k)
                    jx8p_note_on(&p, chord[i] + mks_chords[pv[JX_CHORD]][k], 100 / 127.0f, 0);
            } else jx8p_note_on(&p, chord[i], 100 / 127.0f, 0);
        }
        if (pos == on) for (int i = 0; i < 3; ++i) {
            if (pv[JX_KEY_MODE] == 1) {
                for (unsigned k = 0; k < MKS_CHORD_NOTES && mks_chords[pv[JX_CHORD]][k] != -128; ++k)
                    jx8p_note_off(&p, chord[i] + mks_chords[pv[JX_CHORD]][k]);
            } else jx8p_note_off(&p, chord[i]);
        }
        int n = FRAMES - pos < EL_BLOCK ? FRAMES - pos : EL_BLOCK;
        memset(d, 0, sizeof d); memset(v, 0, sizeof v);
        jx8p_render(&p, l + pos, r + pos, d, v, n, 120.0f);
    }
}

static void wav(const char *dir, unsigned i, const char *name, const float *l, const float *r)
{
    char path[512];
    snprintf(path, sizeof path, "%s/eigen%02u_%s.wav", dir, i + 1, name);
    for (char *c = path + strlen(dir) + 1; *c; ++c) if (*c == ' ' || *c == '.' || *c == '/' || *c == '\'') *c = '_';
    memcpy(path + strlen(path) - 4, ".wav", 4);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t data = FRAMES * 4u, riff = data + 36u, sr = SR, br = SR * 4u;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    uint32_t fmt = 16; uint16_t pcm = 1, ch = 2, al = 4, bits = 16;
    fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (int i2 = 0; i2 < FRAMES; ++i2) {
        int16_t s[2] = {(int16_t)(fmaxf(-1, fminf(1, l[i2])) * 32767), (int16_t)(fmaxf(-1, fminf(1, r[i2])) * 32767)};
        fwrite(s, sizeof s, 1, f);
    }
    fclose(f);
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* ns je Sample für `notes` gehaltene Noten (Poly) bzw. Unisono; Preset `preset`, 10 s Audio, beste von 3 Läufen. */
static double cost(unsigned preset, int notes, int key_mode, int chorus)
{
    const baustein_t *b = &jx8p_baustein;
    enum { SEC = 10, N = 512 };
    static float l[N], r[N];
    double best = 1e9;
    for (int run = 0; run < 3; ++run) {
        b->init(st_mem, SR);
        jx8p_bs_load_preset(st_mem, preset);
        jx8p_bs_set_raw(st_mem, JX_KEY_MODE, key_mode);
        jx8p_bs_set_raw(st_mem, JX_CHORUS, chorus);
        jx8p_bs_set_raw(st_mem, JX_CHORUS_MODE, 0);
        jx8p_bs_set_raw(st_mem, JX_E2_SUSTAIN, 127);
        for (int i = 0; i < notes; ++i) b->note_on(st_mem, 48 + 5 * i, 100);
        double t0 = now();
        for (int s = 0; s < SEC * SR / N; ++s) b->render(st_mem, l, r, N);
        double t = (now() - t0) / (SEC * SR) * 1e9;
        if (t < best) best = t;
    }
    return best;
}

static int last(void)
{
    unsigned p = jx8p_bs_eigene_count();   /* erster Werks-Sound: E. BASS (zwei DCOs, Ladder, HPF) */
    double t1 = cost(p, 1, 0, 0), t6 = cost(p, 6, 0, 0), t6c = cost(p, 6, 0, 1), tu = cost(p, 1, 3, 0);
    printf("Last auf diesem Rechner (ns je Sample, 44,1 kHz, %s):\n", jx8p_bs_preset_name(p));
    printf("  1 Stimme %.1f, 6 Stimmen %.1f -> je Stimme %.1f ns, Grundlast Teil %.1f ns\n",
           t1, t6, (t6 - t1) / 5, t1 - (t6 - t1) / 5);
    printf("  6 Stimmen mit Chorus %.1f (Chorus %.1f ns), Unisono 6 Stimmen %.1f\n", t6c, t6c - t6, tu);
    printf("  ein Kern bei 44,1 kHz: 6 Stimmen = %.2f %% Echtzeit\n", t6c * SR / 1e7);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "last")) return last();
    const char *ref = argc > 1 ? argv[1] : "ref", *dir = argc > 2 ? argv[2] : ".";
    int write = argc > 3 && !strcmp(argv[3], "write");
    const baustein_t *b = &jx8p_baustein;
    CHECK(b->state_size <= sizeof st_mem, "state_size %zu", b->state_size);
    CHECK(b->param_count == JX_COUNT, "param_count");
    CHECK(fabs(jx8p_bs_rate() - SR) < 1e-9, "Abtastrate");
    unsigned count = jx8p_bs_preset_count(), eigene = jx8p_bs_eigene_count();
    CHECK(count <= MAXP && eigene > 0, "Presets %u / eigene %u", count, eigene);
    for (unsigned i = 0; i < JX_COUNT; ++i) CHECK(b->param_name(i)[0], "Name von Parameter %u", i);

    /* eigene Presets: Werte im Bereich, stehen vorn */
    for (unsigned i = 0; i < eigene; ++i)
        for (unsigned k = 0; k < JX_COUNT; ++k)
            CHECK(jx8p_bs_preset_values(i)[k] <= jx8p_param_max(k), "%s: %s = %d", jx8p_bs_preset_name(i),
                  jx8p_param_name(k), jx8p_bs_preset_values(i)[k]);
    CHECK(!strcmp(jx8p_bs_preset_name(eigene), "E. BASS"), "erster Werks-Sound hinter den eigenen");

    /* Parameter 0..1 -> Rohwert */
    b->init(st_mem, SR);
    b->set_param(st_mem, JX_CUTOFF, 0.5);
    CHECK(jx8p_bs_get_raw(st_mem, JX_CUTOFF) == 64, "CUTOFF 0.5 -> %d", jx8p_bs_get_raw(st_mem, JX_CUTOFF));
    b->set_param(st_mem, JX_KEY_MODE, 1.0);
    CHECK(jx8p_bs_get_raw(st_mem, JX_KEY_MODE) == 3, "KEY MODE 1.0 -> %d", jx8p_bs_get_raw(st_mem, JX_KEY_MODE));

    /* jedes Preset: bitgleich mit dem Kern, Prüfsumme */
    FILE *sf;
    char path[512];
    snprintf(path, sizeof path, "%s/sums.txt", ref);
    uint32_t want[MAXP] = {0};
    int have = 0;
    if (!write && (sf = fopen(path, "r"))) {
        char line[256];
        while (fgets(line, sizeof line, sf)) {
            unsigned i; uint32_t h;
            if (line[0] != '#' && sscanf(line, "%u %x", &i, &h) == 2 && i < MAXP) { want[i] = h; ++have; }
        }
        fclose(sf);
    }
    FILE *out = write ? fopen(path, "w") : 0;
    if (out) fprintf(out, "# jx8p: FNV-1a der float-Samples je Preset (Akkord C-E-G, Velocity 100, %d ms, %d ms gesamt)\n",
                     ON_MS, TOTAL_MS);
    unsigned same = 0, ok_sum = 0, ended = 0;
    for (unsigned i = 0; i < count; ++i) {
        play_baustein(i, a_l, a_r);
        play_engine(i, b_l, b_r);
        int eq = !memcmp(a_l, b_l, sizeof a_l) && !memcmp(a_r, b_r, sizeof a_r);
        same += eq;
        CHECK(eq, "%s: Baustein und Kern verschieden", jx8p_bs_preset_name(i));
        float peak = 0; int nans = 0;
        for (int k = 0; k < FRAMES; ++k) {
            if (isnan(a_l[k]) || isnan(a_r[k])) ++nans;
            peak = fmaxf(peak, fmaxf(fabsf(a_l[k]), fabsf(a_r[k])));
        }
        CHECK(!nans && peak > 0.001f && peak < 4.0f, "%s: Spitze %g, NaN %d", jx8p_bs_preset_name(i), peak, nans);
        ended += !b->active(st_mem);
        uint32_t h = fnv(a_l, a_r, FRAMES);
        if (out) fprintf(out, "%u %08x %s\n", i, h, jx8p_bs_preset_name(i));
        else if (have) {
            ok_sum += want[i] == h;
            CHECK(want[i] == h, "%s: Prüfsumme %08x statt %08x", jx8p_bs_preset_name(i), h, want[i]);
        }
        if (i < eigene) {
            wav(dir, i, jx8p_bs_preset_name(i), a_l, a_r);
            printf("eigen %u %-10s Spitze %.3f\n", i + 1, jx8p_bs_preset_name(i), peak);
        }
    }
    if (out) { fclose(out); printf("ref/sums.txt neu geschrieben (%u Presets)\n", count); }
    else CHECK(have == (int)count, "ref/sums.txt hat %d von %u Presets", have, count);
    printf("jx8p: %u Presets (%u eigene), %u bitgleich mit engine/jx8p.c, %u Prüfsummen gleich, %u nach %d ms still\n",
           count, eigene, same, ok_sum, ended, TOTAL_MS);

    /* Akkord-Modus: KEY MODE 1 spielt die Akkordform; note_off lässt alle los */
    b->init(st_mem, SR);
    jx8p_bs_load_preset(st_mem, eigene);
    jx8p_bs_set_raw(st_mem, JX_KEY_MODE, 1);
    jx8p_bs_set_raw(st_mem, JX_CHORD, 0);   /* m7: vier Noten */
    b->note_on(st_mem, 60, 100);
    jx8p_bs_set_raw(st_mem, JX_CHORD, 3);   /* umschalten, während gehalten */
    b->render(st_mem, a_l, a_r, 256);
    b->note_off(st_mem, 60);
    b->render(st_mem, a_l, a_r, 256);
    {
        /* nach note_off hat keine Stimme mehr Gate */
        const jx8p_part_t *p = (const jx8p_part_t *)(const void *)st_mem;
        int gates = 0, act = 0;
        for (unsigned i = 0; i < p->voices; ++i) { gates += p->voice[i].gate; act += p->voice[i].active; }
        CHECK(gates == 0, "Akkord: %d Gates nach note_off", gates);
        CHECK(act == 4, "Akkord m7: %d Stimmen klingen aus statt 4", act);
    }

    /* Sends: ein Preset mit Delay-Anteil schreibt in den Delay-Bus */
    b->init(st_mem, SR);
    jx8p_bs_set_raw(st_mem, JX_DELAY_SEND, 100);
    b->note_on(st_mem, 60, 100);
    memset(a_l, 0, 4096 * 4); memset(a_r, 0, 4096 * 4); memset(b_l, 0, 4096 * 4); memset(b_r, 0, 4096 * 4);
    jx8p_bs_render_sends(st_mem, a_l, a_r, b_l, b_r, 4096);
    float sd = 0, sr2 = 0;
    for (int i = 0; i < 4096; ++i) { sd += fabsf(b_l[i]); sr2 += fabsf(b_r[i]); }
    CHECK(sd > 0 && sr2 == 0, "Sends: Delay %g, Hall %g", sd, sr2);

    printf(fails ? "jx8p: %d FEHLER\n" : "jx8p: alles gut\n", fails);
    return fails ? 1 : 0;
}
