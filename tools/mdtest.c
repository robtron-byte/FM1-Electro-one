// mdtest.c - Testrender: Monotron-Delay-artiger Delay-Loop (Entwurf fuer FM-1)
// Urheber: Ron Zoellner (Dropbox: Privat/PP/Programmieren/M-Vave FM1/Notizen/mdtest.c, Stand 08.11.2026).
// Angepasst nach der Recherche in docs/adr/0033-mono-delay.md (N15):
//   - Das Korg Monotron Delay nutzt einen PT2399 (kein BBD), getaktet mit etwa 2..20 MHz.
//     Delay in ms ~= 683.21 / Takt in MHz + 0.08 (Electric Druid, aus dem PT2399-Datenblatt):
//     20 MHz -> ~34 ms (die kuerzeste Zeit), 2 MHz -> ~342 ms.
//   - Die Delayzeit ist darum hier in Millisekunden angegeben, Standard 34 ms (vorher 96 Samples = 2 ms).
//     Die Laenge in Samples folgt aus der Abtastrate, so klingt es bei 48 kHz und 44,1 kHz gleich.
// Aufbau: in -> [+ Feedback] -> HP 100 Hz -> Softclip -> Korg35-LPF -> Delay (34 ms) -> Feedback
// Kein Rauschen. Cutoff 100 Hz..10 kHz; unterhalb des Reglerbereichs wird der Effekt per Pegel "gekillt".
//
// Aufruf:
//   mdtest fresp K                      Frequenzgang des Korg35-LPF allein (fc = 1 kHz)
//   mdtest render out.wav sec p0 p1 wetonly [MS] [K]
//       p0..p1 = Reglerstellung (0..1) linear ueber die Dauer, wetonly=1 gibt nur das Delay-Signal aus,
//       MS = Delayzeit in Millisekunden (Standard 34, erlaubt 1..340: der Bereich des PT2399 im Monotron)
//
// Bauen: cc -O2 -o mdtest tools/mdtest.c -lm

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FS 48000.0f        /* Testrate; das FM-1 rechnet mit 44,1 kHz - die Zeiten unten sind in ms und passen fuer beide */
#define PI_F 3.14159265f
#define MD_MS_DEFAULT 34.0f /* kuerzeste Delayzeit des Monotron Delay (PT2399 bei ~20 MHz) */
#define MD_MS_MAX 340.0f    /* laengste im normalen Bereich (PT2399 bei ~2 MHz) */

/* ---------- Korg-35-Tiefpass (ZDF/TPT, nach Pirkle/Zavalishin, hier hergeleitet) ---------- */
typedef struct { float s1, s2, s3, G, a0, K; } k35_t;

static void k35_set(k35_t *f, float fc, float fs, float K)
{
    float g = tanf(PI_F * fc / fs);
    f->G  = g / (1.f + g);
    f->K  = K;
    f->a0 = 1.f / (1.f - K * f->G + K * f->G * f->G);
}

static float k35_lp(k35_t *f, float x)
{
    float G = f->G, ig = 1.f - G;
    float v1 = (x - f->s1) * G, y1 = v1 + f->s1;  f->s1 += 2.f * v1;
    float y  = f->a0 * (G * y1 + ig * f->s2 - G * f->K * ig * f->s3);
    float v3 = (y - f->s3) * G, h = (y - f->s3) * ig;  f->s3 += 2.f * v3;
    float v2 = (y1 + f->K * h - f->s2) * G;  f->s2 += 2.f * v2;
    return y;
}

/* ---------- 1-Pol-Hochpass ---------- */
typedef struct { float s, G; } hp1_t;

static void hp1_set(hp1_t *h, float fc, float fs)
{
    float g = tanf(PI_F * fc / fs);
    h->G = g / (1.f + g);
}

static float hp1_run(hp1_t *h, float x)
{
    float v = (x - h->s) * h->G, lp = v + h->s;
    h->s = lp + v;
    return x - lp;
}

static float softclip(float x)
{
    if (x >  3.f) x =  3.f;
    if (x < -3.f) x = -3.f;
    return x * (27.f + x * x) / (27.f + 9.f * x * x);   /* Pade-tanh, +-1 bei |x|=3 */
}

/* ---------- Delay-Loop ---------- */
#define MAXN 16321   /* Puffer fuer die laengste Zeit: 340 ms bei 48 kHz (+1) */
typedef struct {
    float buf[MAXN]; int N, wp;
    k35_t f; hp1_t hp;
    float K, fbmax, wet;
    float kill, fb;
} md_t;

/* Delayzeit in Millisekunden -> Laenge in Samples bei FS */
static int md_samples(float ms)
{
    int n = (int)(ms * 0.001f * FS + 0.5f);
    return n < 8 ? 8 : n > MAXN ? MAXN : n;
}

static void md_init(md_t *m, float ms, float K)
{
    memset(m, 0, sizeof *m);
    m->N = md_samples(ms); m->K = K; m->fbmax = 1.0f; m->wet = 0.7f;
    hp1_set(&m->hp, 100.f, FS);
}

/* Regler p: 0..1. Unteres Fuenftel = Kill-Zone (Cutoff bleibt bei 100 Hz, Pegel faellt auf 0). */
static void md_knob(md_t *m, float p)
{
    const float p0 = 0.2f;
    float fc = (p < p0) ? 100.f : 100.f * powf(100.f, (p - p0) / (1.f - p0));  /* 100 Hz .. 10 kHz */
    float k  = (p >= p0) ? 1.f : p / p0;
    k = k * k * (3.f - 2.f * k);                    /* smoothstep */
    k35_set(&m->f, fc, FS, m->K);
    m->kill = k;
    m->fb = fminf(1.f, 4.f * k) * m->fbmax;         /* Schleife wird nahe Null leergeraeumt */
}

static float md_wet(md_t *m, float in)
{
    float d = m->buf[m->wp];                        /* Ausgang des Delays, N Samples alt */
    float w = in * m->kill + m->fb * d;
    w = hp1_run(&m->hp, w);                         /* keine Tiefen in die Schleife */
    w = softclip(w);                                /* ersetzt die Analogbegrenzung */
    float y = k35_lp(&m->f, w);
    m->buf[m->wp] = y;
    if (++m->wp >= m->N) m->wp = 0;
    return m->wet * m->kill * d;
}

/* ---------- Testsignal: einfache 2-Op-FM-E-Piano-Noten ---------- */
static float fm_note(float t, float f)
{
    float env = expf(-t * 4.f);
    float mod = sinf(2.f * PI_F * f * t);
    return 0.5f * env * sinf(2.f * PI_F * f * t + 1.5f * env * mod);
}

static void write_wav(const char *name, const int16_t *x, int n)
{
    FILE *fp = fopen(name, "wb");
    if (!fp) { perror("wav"); exit(1); }
    uint32_t datalen = (uint32_t)n * 2, riff = 36 + datalen, sr = (uint32_t)FS, br = sr * 2;
    uint16_t fmt = 1, ch = 1, ba = 2, bits = 16; uint32_t fmtlen = 16;
    fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
    fwrite(&fmtlen, 4, 1, fp); fwrite(&fmt, 2, 1, fp); fwrite(&ch, 2, 1, fp);
    fwrite(&sr, 4, 1, fp); fwrite(&br, 4, 1, fp); fwrite(&ba, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&datalen, 4, 1, fp); fwrite(x, 2, n, fp);
    fclose(fp);
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "fresp")) {
        float K = (float)atof(argv[2]);
        float freqs[] = {50, 100, 250, 500, 800, 1000, 1200, 2000, 4000, 8000};
        printf("Korg35-LPF, fc=1000 Hz, K=%.2f\n", K);
        for (int i = 0; i < 10; i++) {
            k35_t f = {0}; k35_set(&f, 1000.f, FS, K);
            float peak = 0.f;
            int n = 48000;
            for (int s = 0; s < n; s++) {
                float y = k35_lp(&f, sinf(2.f * PI_F * freqs[i] * s / FS));
                if (s > n / 2 && fabsf(y) > peak) peak = fabsf(y);
            }
            printf("  %6.0f Hz: %+6.2f dB\n", freqs[i], 20.f * log10f(peak + 1e-12f));
        }
        return 0;
    }
    if (argc >= 7 && !strcmp(argv[1], "render")) {
        const char *out = argv[2];
        float sec = (float)atof(argv[3]), p0 = (float)atof(argv[4]), p1 = (float)atof(argv[5]);
        int wetonly = atoi(argv[6]);
        float ms = argc > 7 ? (float)atof(argv[7]) : MD_MS_DEFAULT;
        float K = argc > 8 ? (float)atof(argv[8]) : 1.0f;
        if (ms < 1.f || ms > MD_MS_MAX) { fprintf(stderr, "Delayzeit ausserhalb 1..%.0f ms\n", MD_MS_MAX); return 1; }
        int n = (int)(sec * FS);
        int16_t *o = malloc(n * sizeof(int16_t));
        md_t *m = malloc(sizeof *m); md_init(m, ms, K);
        const float notes[] = {110.f, 164.81f, 220.f, 261.63f};
        float peak = 0.f, maxabs_internal = 0.f;
        for (int i = 0; i < n; i++) {
            float t = (float)i / FS;
            int idx = (int)(t / 0.5f);
            float tn = t - idx * 0.5f;
            float in = fm_note(tn, notes[idx & 3]);
            float p = p0 + (p1 - p0) * (float)i / (float)n;
            md_knob(m, p);
            float wet = md_wet(m, in);
            float y = wetonly ? wet : (in + wet);
            if (!isfinite(y)) { fprintf(stderr, "NaN/Inf bei Sample %d\n", i); return 2; }
            if (fabsf(y) > peak) peak = fabsf(y);
            if (fabsf(m->buf[m->wp]) > maxabs_internal) maxabs_internal = fabsf(m->buf[m->wp]);
            float v = y * 0.7f;
            if (v > 1.f) { v = 1.f; }
            if (v < -1.f) { v = -1.f; }
            o[i] = (int16_t)lrintf(v * 32767.f);
        }
        write_wav(out, o, n);
        printf("%s: %.1f s, %.1f ms (N=%d), K=%.2f, Peak(vor 0.7-Gain)=%.3f, max Loop-Pegel=%.3f\n",
               out, sec, 1000.f * m->N / FS, m->N, K, peak, maxabs_internal);
        return 0;
    }
    fprintf(stderr, "Aufruf: mdtest fresp K | mdtest render out.wav sec p0 p1 wetonly [MS=34] [K]\n");
    return 1;
}
