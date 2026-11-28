/* Host-Test dk808: jeder Schlag (Velocity 64 und 127, Standard-Kit, 2 s bei 48 kHz) gegen die Referenz-Renders des
 * Swift-Kerns (ref/manifest.json: erste 16 Samples, Spitze, RMS, Summe; liegen die WAVs in <wavdir>, jedes Sample).
 * Aufruf: test <refdir> <outdir> [wavdir]. Schreibt hit-XX-v127.wav (16 bit) zum Anhören nach <outdir>.
 * Instrumente, die mit DK808_INSTRUMENTS weggelassen sind, müssen stumm bleiben. */
#include "dk808.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FRAMES = 96000, RATE = 48000 };
static float buf[FRAMES];

typedef struct { char abbr[4]; int vel; double first[16], peak, rms, sum; } ref_t;
static ref_t refs[40];
static int nref;

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return 0; }
    s[n] = 0;
    fclose(f);
    return s;
}

/* liest den Zahlenwert nach "key" ab p */
static double num_after(const char *p, const char *key, const char **end)
{
    const char *k = strstr(p, key);
    if (!k) { *end = 0; return 0; }
    k = strchr(k + strlen(key), ':') + 1;
    char *e;
    double v = strtod(k, &e);
    *end = e;
    return v;
}

static int load_manifest(const char *dir)
{
    char path[512];
    snprintf(path, sizeof path, "%s/manifest.json", dir);
    char *s = slurp(path);
    if (!s) return 0;
    const char *p = s;
    while ((p = strstr(p, "\"abbr\" : \"")) && nref < 40) {
        ref_t *r = &refs[nref];
        p += 10;
        memcpy(r->abbr, p, 2); r->abbr[2] = 0;
        const char *a = strchr(strstr(p, "\"first_16_left\""), '[') + 1;
        for (int i = 0; i < 16; ++i) { char *e; r->first[i] = strtod(a, &e); a = strchr(e, ',') ? e + 1 : e; }
        const char *e;
        r->peak = num_after(p, "\"peak_left\"", &e);
        r->rms = num_after(p, "\"rms_left\"", &e);
        r->sum = num_after(p, "\"sum_left\"", &e);
        r->vel = (int)num_after(p, "\"velocity\"", &e);
        p = e;
        ++nref;
    }
    free(s);
    return nref;
}

static int instrument_of(const char *ab)
{
    for (unsigned i = 0; i < DK_COUNT; ++i) if (!strcmp(dk808_instrument_abbr(i), ab)) return (int)i;
    return -1;
}

static void render_hit(unsigned inst, int vel)
{
    static dk808_t d;
    dk808_init(&d, RATE);
    dk808_trigger(&d, inst, vel);
    /* Der Kanalzug der Referenz multipliziert bei Mitte mit cos(pi/4) * 1.41421356237 (Mixer.swift, panGains) */
    const double pan = cos((0.0 / 100.0 + 1.0) * 0.25 * M_PI) * 1.41421356237;
    for (int i = 0; i < FRAMES; ++i) buf[i] = (float)(dk808_tick(&d) * pan);
}

static void write_wav(const char *dir, const char *ab, int vel)
{
    char path[512];
    snprintf(path, sizeof path, "%s/hit-%s-v%d.wav", dir, ab, vel);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t data = FRAMES * 4, riff = data + 36, rate = RATE, bps = RATE * 4;
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0,
                     0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
    memcpy(h + 4, &riff, 4); memcpy(h + 24, &rate, 4); memcpy(h + 28, &bps, 4); memcpy(h + 40, &data, 4);
    fwrite(h, 1, 44, f);
    for (int i = 0; i < FRAMES; ++i) {
        float x = buf[i] > 1 ? 1 : buf[i] < -1 ? -1 : buf[i];
        int16_t s = (int16_t)lrintf(x * 32767.0f), st[2] = {s, s};
        fwrite(st, sizeof st, 1, f);
    }
    fclose(f);
}

/* Vergleicht mit der Referenz-WAV (32-bit float stereo), falls vorhanden; liefert die größte Abweichung oder -1. */
static double compare_wav(const char *dir, const char *ab, int vel)
{
    if (!dir) return -1;
    char path[512];
    snprintf(path, sizeof path, "%s/hit-%s-v%d.wav", dir, ab, vel);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 44, SEEK_SET);
    double worst = 0;
    float st[2];
    for (int i = 0; i < FRAMES && fread(st, sizeof st, 1, f) == 1; ++i) {
        double e = fabs((double)st[0] - buf[i]);
        if (e > worst) worst = e;
    }
    fclose(f);
    return worst;
}

int main(int argc, char **argv)
{
    const char *refdir = argc > 1 ? argv[1] : "ref";
    const char *outdir = argc > 2 ? argv[2] : ".";
    const char *wavdir = argc > 3 ? argv[3] : 0;
    if (!load_manifest(refdir)) { printf("dk808: %s/manifest.json fehlt\n", refdir); return 1; }
    int ok = 1, checked = 0, silent = 0, wavs = 0;
    double worst = 0;
    for (int r = 0; r < nref; ++r) {
        const ref_t *x = &refs[r];
        int inst = instrument_of(x->abbr);
        if (inst < 0) { printf("  %s: unbekannt\n", x->abbr); ok = 0; continue; }
        render_hit((unsigned)inst, x->vel);
        double peak = 0, sq = 0, sum = 0, first = 0;
        for (int i = 0; i < FRAMES; ++i) {
            double s = buf[i];
            if (fabs(s) > peak) peak = fabs(s);
            sq += s * s;
            sum += s;
        }
        for (int i = 0; i < 16; ++i) if (fabs(buf[i] - x->first[i]) > first) first = fabs(buf[i] - x->first[i]);
        double rms = sqrt(sq / FRAMES);
        if (!DK808_HAS(inst)) {   /* weggelassen: muss stumm sein */
            if (peak != 0) { printf("  %s: weggelassen, klingt aber\n", x->abbr); ok = 0; }
            ++silent;
            continue;
        }
        double wav = compare_wav(wavdir, x->abbr, x->vel);
        if (wav >= 0) ++wavs;
        if (wav > worst) worst = wav;
        if (first > worst) worst = first;
        int good = first < 1e-5 && fabs(peak - x->peak) < 1e-5 && fabs(rms - x->rms) < 1e-6 &&
                   fabs(sum - x->sum) < 1e-3 && wav < 1e-5;
        if (!good)
            printf("  %s v%d: erste16 %.2g, Spitze %.7f/%.7f, RMS %.7f/%.7f, Summe %.6f/%.6f, WAV %.2g\n", x->abbr,
                   x->vel, first, peak, x->peak, rms, x->rms, sum, x->sum, wav);
        ok = ok && good;
        ++checked;
        if (x->vel == 127) write_wav(outdir, x->abbr, 127);
    }
    printf("dk808: %d Schläge wie die Referenz (größte Abweichung %.1g), %d weggelassen und stumm, %d Sample für Sample (WAV) %s\n",
           checked, worst, silent, wavs, ok ? "ok" : "FEHLER");
    return ok ? 0 : 1;
}
