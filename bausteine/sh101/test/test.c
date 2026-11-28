/* sh101 Host-Test: rendert die sieben Testvektoren der Vorlage (MonoBass RenderCheck --vectors) und vergleicht
 *   - samples_2400_to_2463 (64 Samples, Grenze 1e-4 absolut, SOUND-ENGINE.md §16),
 *   - rms_per_100ms (20 Fenster, Grenze 1e-3 absolut + 1 % relativ, Angabe in vectors.json hat 6 Stellen),
 *   - optional jedes Sample gegen die Referenz-WAVs (argv[3], 32-bit float mono), Grenze 1e-4.
 * Schreibt <name>.wav (float32 mono) nach argv[2]. Zusätzlich prüft er die Schnittstelle baustein_t.
 * Exit 0 = bestanden. */
#include "sh101.h"
#include "../ref/vectors.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Gerätefassung (SH101_FLOAT / SH101_FASTMATH): nicht bitgleich; geprüft werden die RMS-Fenster und die 64 Samples mit
 * weiterer Grenze, die Abweichung zur WAV wird nur gemeldet. */
#if SH101_FLOAT || SH101_FASTMATH
#define LIM_SAMPLES 1e-3
#define WAV_STRICT 0
#else
#define LIM_SAMPLES 1e-4
#define WAV_STRICT 1
#endif

static sh101_t eng;
static float out[48000 * 2];

typedef struct { int at; int note; int on; } ev_t;

static int find_param(const char *key)
{
    for (unsigned p = 0; p < SH101_PARAM_COUNT; ++p) if (!strcmp(sh101_param_key(p), key)) return (int)p;
    return -1;
}

static void write_wav(const char *dir, const char *name, const float *x, int n, int rate)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    unsigned char h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 3, 0, 1, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 32, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
    unsigned data = (unsigned)n * 4, riff = data + 36, r = (unsigned)rate, bps = (unsigned)rate * 4;
    memcpy(h + 4, &riff, 4); memcpy(h + 24, &r, 4); memcpy(h + 28, &bps, 4); memcpy(h + 40, &data, 4);
    fwrite(h, 1, 44, f);
    fwrite(x, 4, (size_t)n, f);
    fclose(f);
}

/* größte Abweichung gegen die Referenz-WAV oder -1, wenn keine da ist */
static double compare_wav(const char *dir, const char *name, const float *x, int n)
{
    if (!dir) return -1;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 44, SEEK_SET);
    double worst = 0;
    for (int i = 0; i < n; ++i) {
        float r;
        if (fread(&r, 4, 1, f) != 1) { worst = 1e9; break; }
        double dlt = fabs((double)r - (double)x[i]);
        if (dlt > worst) worst = dlt;
    }
    fclose(f);
    return worst;
}

static int run(const vec_t *v, const char *outdir, const char *wavdir)
{
    sh101_init(&eng, v->rate);
    for (int i = 0; i < v->np; ++i) {
        int p = find_param(v->params[i].key);
        if (p < 0) { printf("  unbekannter Parameter %s\n", v->params[i].key); return 1; }
        sh101_set(&eng, (unsigned)p, v->params[i].value);
    }
    ev_t ev[8];
    int ne = 0;
    for (int i = 0; i < v->nn; ++i) {
        ev[ne++] = (ev_t){(int)(v->notes[i].time * v->rate), v->notes[i].note, 1};
        ev[ne++] = (ev_t){(int)((v->notes[i].time + v->notes[i].length) * v->rate), v->notes[i].note, 0};
    }
    for (int i = 1; i < ne; ++i) { /* stabil nach Zeit sortieren wie renderOffline */
        ev_t t = ev[i];
        int j = i - 1;
        while (j >= 0 && ev[j].at > t.at) { ev[j + 1] = ev[j]; --j; }
        ev[j + 1] = t;
    }
    int total = (int)(v->seconds * v->rate), ei = 0;
    sh101_prepare(&eng);
    for (int i = 0; i < total; ++i) {
        while (ei < ne && ev[ei].at <= i) {
            if (ev[ei].on) sh101_note_on(&eng, ev[ei].note, v->velocity); else sh101_note_off(&eng, ev[ei].note);
            ++ei;
        }
        out[i] = sh101_sample(&eng);
    }
    double ds = 0, dr = 0;
    for (int i = 0; i < 64; ++i) {
        double d = fabs((double)out[2400 + i] - (double)v->samples[i]);
        if (d > ds) ds = d;
    }
    int bad = ds > LIM_SAMPLES;
    for (int w = 0; w < 20; ++w) {
        int seg = v->rate / 10;
        float acc = 0;
        for (int i = 0; i < seg; ++i) acc += out[w * seg + i] * out[w * seg + i];
        float rms = sqrtf(acc / (float)seg);
        double d = fabs((double)rms - (double)v->rms[w]);
        if (d > dr) dr = d;
        if (d > 1e-3 + 0.01 * v->rms[w]) bad = 1;
    }
    double dw = compare_wav(wavdir, v->name, out, total);
    if (WAV_STRICT && dw > 1e-4) bad = 1;
    write_wav(outdir, v->name, out, total, v->rate);
    printf("  %-20s Samples %.2e  RMS %.2e  WAV %s%.2e  %s\n", v->name, ds, dr, dw < 0 ? "(fehlt) " : "",
           dw < 0 ? 0 : dw, bad ? "FEHLER" : "ok");
    return bad;
}

static int check_interface(void)
{
    /* baustein_t: Zustand vom Aufrufer, render addiert, active meldet Ausklingen */
    static float l[4800], r[4800];
    void *st = calloc(1, sh101_baustein.state_size + 16);
    if (!st) return 1;
    sh101_baustein.init(st, 48000);
    sh101_baustein.set_param(st, SH101_ARP_MODE, 1.0);   /* 0..1 auf 0..6 gestreckt */
    int bad = sh101_get((sh101_t *)st, SH101_ARP_MODE) != 6;
    for (int i = 0; i < 4800; ++i) l[i] = r[i] = 1;
    sh101_baustein.note_on(st, 48, 100);
    sh101_baustein.render(st, l, r, 4800);
    double e = 0;
    for (int i = 0; i < 4800; ++i) e += fabs(l[i] - 1) + fabs(r[i] - l[i]);
    if (e < 1e-3) bad = 1;
    sh101_baustein.note_off(st, 48);
    for (int k = 0; k < 40 && sh101_baustein.active(st); ++k) sh101_baustein.render(st, l, r, 4800);
    if (sh101_baustein.active(st)) bad = 1;
    free(st);
    printf("  baustein_t           %s\n", bad ? "FEHLER" : "ok");
    return bad;
}

int main(int argc, char **argv)
{
    const char *outdir = argc > 2 ? argv[2] : ".";
    const char *wavdir = argc > 3 && argv[3][0] ? argv[3] : NULL;
    int bad = 0;
    printf("sh101 (Poly %d, Arp/Seq %d, float %d, Polynome %d), Zustand %zu Bytes\n", SH101_WITH_POLY, SH101_WITH_ARPSEQ,
           SH101_FLOAT, SH101_FASTMATH, sizeof(sh101_t));
    for (size_t i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
#if !SH101_WITH_POLY
        if (vectors[i].np && !strcmp(vectors[i].name, "07-poly-chord")) { printf("  %-20s (ohne Poly übersprungen)\n", vectors[i].name); continue; }
#endif
        bad |= run(&vectors[i], outdir, wavdir);
    }
    bad |= check_interface();
    printf("sh101: %s\n", bad ? "FEHLER" : "bestanden");
    return bad;
}
