/* Host-Test mksultra: rendert die 7 Golden-Fälle der Vorlage (engine/examples/golden.rs) und vergleicht die FNV-1a-64-
 * Prüfsumme mit ref/manifest.json. Schreibt WAVs nach argv[2].
 *   test <ref> <out> [<Golden-WAV-Ordner>]   Prüfsummen; mit WAV-Ordner zusätzlich erste Abweichung je Fall
 *   test --vs <ref> <out> <Vergleichsordner>  Sparfassung: Abweichung gegen die WAVs der vollen Fassung
 *   test --bench                              Rechenzeit je Stimme
 *   test --sweep                              eine Prüfsumme je Preset (Vergleich mit dem Rust-Gegenstück, README)
 * Exit 0 = bestanden. */
#include "mksultra.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *name, *preset;
    int notes[4], nnotes;
    float sr;
    int len, release;
} gcase_t;

static const gcase_t CASES[] = {
    {"01_default_c4", "default.mks", {60}, 1, 48000.0f, 48000, 36000},
    {"02_polysynth_chord", "Factory Bank A/000 PolySynth1.mks", {48, 55, 60, 64}, 4, 48000.0f, 72000, 48000},
    {"03_brass_44k1", "Factory Bank B/002 BrassHorns.mks", {60}, 1, 44100.0f, 66150, 44100},
    {"04_unison_hat", "Factory MKS/Drums/Short Hat.mks", {60}, 1, 48000.0f, 24000, 12000},
    {"05_kick_sub_noise", "Factory MKS/Drums/Kick No Res.mks", {36}, 1, 48000.0f, 24000, 12000},
    {"06_delay_reverb", "Ron/Micro Ticks.mks", {60, 67}, 2, 48000.0f, 96000, 24000},
    {"07_arpeggiator", "Ron/041 Lead 6 short ron.mks", {48, 52, 55}, 3, 48000.0f, 96000, 72000},
};
#define NCASES ((int)(sizeof CASES / sizeof CASES[0]))

static uint64_t fnv1a(const float *d, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    const unsigned char *b = (const unsigned char *)d;
    for (size_t i = 0; i < n * 4; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
    return h;
}

static void write_wav(const char *path, unsigned sr, const float *d, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    unsigned len = (unsigned)(n * 4), v;
    unsigned short s;
    fwrite("RIFF", 1, 4, f); v = 36 + len; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 3; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    fwrite(&sr, 4, 1, f); v = sr * 8; fwrite(&v, 4, 1, f);
    s = 8; fwrite(&s, 2, 1, f); s = 32; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&len, 4, 1, f);
    fwrite(d, 4, n, f);
    fclose(f);
}

/* liest die float-Daten einer WAV der Vorlage (Kopf 44 Bytes); Anzahl Werte oder 0 */
static size_t read_wav(const char *path, float **out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 44) { fclose(f); return 0; }
    size_t n = (size_t)(sz - 44) / 4;
    *out = malloc(n * 4);
    fseek(f, 44, SEEK_SET);
    n = fread(*out, 4, n, f);
    fclose(f);
    return n;
}

static int manifest_fnv(const char *ref, const char *name, uint64_t *h)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/manifest.json", ref);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    char key[128];
    snprintf(key, sizeof key, "\"name\": \"%s\"", name);
    char *p = strstr(buf, key);
    if (!p) return 0;
    p = strstr(p, "\"fnv1a64\": \"");
    if (!p) return 0;
    *h = strtoull(p + 12, NULL, 16);
    return 1;
}

static void *state(void)
{
    static void *s;
    if (!s) s = calloc(1, mksultra_baustein.state_size);
    return s;
}

/* Rendert einen Fall wie golden.rs; gibt die verschachtelten Daten zurück (2*len Werte). */
static float *render_case(const gcase_t *c)
{
    void *s = state();
    int pi = mksultra_preset_find(c->preset);
    if (pi < 0) { fprintf(stderr, "Preset fehlt: %s\n", c->preset); return NULL; }
    mksultra_init(s, c->sr);
    mksultra_load_preset(s, &mksultra_presets[pi]);
    float l[512], r[512];
    for (int k = 0; k < 20; k++) mksultra_process(s, l, r, 512);
    for (int k = 0; k < c->nnotes; k++) mksultra_note_on(s, c->notes[k], 100);
    float *d = malloc(sizeof(float) * 2 * (size_t)c->len);
    int pos = 0, released = 0;
    while (pos < c->len) {
        if (!released && pos >= c->release) {
            for (int k = 0; k < c->nnotes; k++) mksultra_note_off(s, c->notes[k]);
            released = 1;
        }
        int n = c->len - pos < 512 ? c->len - pos : 512;
        if (!released && c->release - pos < n) n = c->release - pos;
        mksultra_process(s, l, r, n);
        for (int k = 0; k < n; k++) { d[2 * (pos + k)] = l[k]; d[2 * (pos + k) + 1] = r[k]; }
        pos += n;
    }
    return d;
}

/* Kein Golden-Fall: 8 Noten im Abstand von 0,1 s, je 0,5 s gehalten, NumberOfVoices auf 12. Misst, was die
 * Sparfassung (weniger Stimmen) hörbar ändert, wenn mehr Noten gleichzeitig klingen, als sie Stimmen hat. */
#define STRESS_LEN 96000
static float *render_stress(const char *preset)
{
    void *s = state();
    mksultra_init(s, 48000.0f);
    mksultra_load_preset(s, &mksultra_presets[mksultra_preset_find(preset)]);
    mksultra_set_param(s, mksultra_param_find("NumberOfVoices"), 11.0f);
    static const int keys[8] = {48, 55, 60, 64, 67, 71, 72, 76};
    float l[512], r[512];
    for (int k = 0; k < 20; k++) mksultra_process(s, l, r, 512);
    float *d = malloc(sizeof(float) * 2 * STRESS_LEN);
    for (int pos = 0; pos < STRESS_LEN; pos += 480) {
        for (int k = 0; k < 8; k++) {
            if (pos == k * 4800) mksultra_note_on(s, keys[k], 100);
            if (pos == k * 4800 + 24000) mksultra_note_off(s, keys[k]);
        }
        mksultra_process(s, l, r, 480);
        for (int k = 0; k < 480; k++) { d[2 * (pos + k)] = l[k]; d[2 * (pos + k) + 1] = r[k]; }
    }
    return d;
}
static const char *STRESS[2][2] = {{"stress_polysynth", "Factory Bank A/000 PolySynth1.mks"},
                                   {"stress_brass", "Factory Bank B/002 BrassHorns.mks"}};

/* Abweichung a gegen Referenz b: größte Differenz, erste abweichende Stelle, Rest in dB */
static void compare(const char *name, const float *a, const float *b, size_t n)
{
    double e2 = 0.0, r2 = 0.0, a2 = 0.0, mx = 0.0;
    long first = -1;
    for (size_t i = 0; i < n; i++) {
        double d = (double)a[i] - (double)b[i];
        if (d != 0.0 && first < 0) first = (long)i;
        if (fabs(d) > mx) mx = fabs(d);
        e2 += d * d;
        r2 += (double)b[i] * (double)b[i];
        a2 += (double)a[i] * (double)a[i];
    }
    if (first < 0) { printf("  %-20s Sample für Sample gleich\n", name); return; }
    double db = e2 > 0.0 && r2 > 0.0 ? 10.0 * log10(e2 / r2) : -999.0;
    double lv = a2 > 0.0 && r2 > 0.0 ? 10.0 * log10(a2 / r2) : 0.0;
    printf("  %-20s erste Abweichung bei Sample %ld (Kanal %s), größte %.3g, Differenz %.1f dB, Pegel %+.2f dB\n", name,
           first / 2, first % 2 ? "R" : "L", mx, db, lv);
}

static int smoke(void);

/* ---------- Presets des Nutzers (presets/Ron/, ADR 0051): Pflicht, stehen vorn in der Tabelle ----------
 * Jedes Ron-Preset bekommt einen eigenen Render: 20×512 Vorlauf, Akkord 48 55 60 64 (v100), 1 s halten, 1 s loslassen,
 * 48 kHz. Sollwerte (FNV-1a, RMS, Spitze) kommen vom Rust-Kern: ref/ron.txt (tools/sweep-rust, Modus "ron"). */
#define RON_LEN 96000
#define RON_REL 48000
static int is_ron(int i) { return strncmp(mksultra_presets[i].path, "Ron/", 4) == 0; }

static float *render_ron(int pi)
{
    static const int notes[4] = {48, 55, 60, 64};
    void *s = state();
    mksultra_init(s, 48000.0f);
    mksultra_load_preset(s, &mksultra_presets[pi]);
    float l[512], r[512];
    for (int k = 0; k < 20; k++) mksultra_process(s, l, r, 512);
    for (int k = 0; k < 4; k++) mksultra_note_on(s, notes[k], 100);
    float *d = malloc(sizeof(float) * 2 * RON_LEN);
    int pos = 0;
    while (pos < RON_LEN) {
        if (pos == RON_REL) for (int k = 0; k < 4; k++) mksultra_note_off(s, notes[k]);
        int n = RON_LEN - pos < 512 ? RON_LEN - pos : 512;
        if (pos < RON_REL && RON_REL - pos < n) n = RON_REL - pos;
        mksultra_process(s, l, r, n);
        for (int k = 0; k < n; k++) { d[2 * (pos + k)] = l[k]; d[2 * (pos + k) + 1] = r[k]; }
        pos += n;
    }
    return d;
}

/* Wie der Stressfall, aber mit der Stimmenzahl des Presets: 8 Noten im Abstand von 0,1 s, je 0,5 s gehalten. */
static float *render_ron_stress(int pi)
{
    static const int keys[8] = {48, 55, 60, 64, 67, 71, 72, 76};
    void *s = state();
    mksultra_init(s, 48000.0f);
    mksultra_load_preset(s, &mksultra_presets[pi]);
    float l[480], r[480];
    for (int k = 0; k < 20; k++) mksultra_process(s, l, r, 480);
    float *d = malloc(sizeof(float) * 2 * RON_LEN);
    for (int pos = 0; pos < RON_LEN; pos += 480) {
        for (int k = 0; k < 8; k++) {
            if (pos == k * 4800) mksultra_note_on(s, keys[k], 100);
            if (pos == k * 4800 + 24000) mksultra_note_off(s, keys[k]);
        }
        mksultra_process(s, l, r, 480);
        for (int k = 0; k < 480; k++) { d[2 * (pos + k)] = l[k]; d[2 * (pos + k) + 1] = r[k]; }
    }
    return d;
}

static void ron_wavname(char *buf, size_t n, const char *dir, int pi, const char *suffix)
{
    char name[256];
    snprintf(name, sizeof name, "%s", mksultra_presets[pi].path + 4);
    for (char *c = name; *c; c++) if (*c == ' ' || *c == '/') *c = '_';
    char *dot = strstr(name, ".mks");
    if (dot) *dot = 0;
    snprintf(buf, n, "%s/ron_%s%s.wav", dir, name, suffix);
}

static int ron_ref(const char *ref, const char *path, uint64_t *h, double *rms, double *pk)
{
    char fn[1024], line[1024];
    snprintf(fn, sizeof fn, "%s/ron.txt", ref);
    FILE *f = fopen(fn, "r");
    if (!f) return 0;
    int found = 0;
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *p = line;
        unsigned long long hh = strtoull(p, &p, 16);
        double r = strtod(p, &p), k = strtod(p, &p);
        while (*p == ' ') p++;
        if (strcmp(p, path) == 0) { *h = hh; *rms = r; *pk = k; found = 1; break; }
    }
    fclose(f);
    return found;
}

static int ron_check(const char *ref, const char *out)
{
    int fails = 0, n = 0, front = 1;
    for (int i = 0; i < mksultra_preset_count; i++) {
        if (!is_ron(i)) continue;
        if (i != n) front = 0;
        n++;
        float *d = render_ron(i);
        size_t m = 2 * RON_LEN;
        uint64_t h = fnv1a(d, m), want = 0;
        double sq = 0.0, pk = 0.0, wr = 0.0, wp = 0.0;
        for (size_t k = 0; k < m; k++) { sq += (double)d[k] * (double)d[k]; if (fabs((double)d[k]) > pk) pk = fabs((double)d[k]); }
        double rms = sqrt(sq / (double)m);
        int have = ron_ref(ref, mksultra_presets[i].path, &want, &wr, &wp);
        int ok = have && h == want;
        printf("  Ron %-24s fnv1a64 %016llx RMS %.6f Spitze %.6f %s\n", mksultra_presets[i].name, (unsigned long long)h,
               rms, pk, ok ? "ok" : have ? "FALSCH" : "(kein Sollwert in ref/ron.txt)");
        if (!ok) fails++;
        char path[1024];
        ron_wavname(path, sizeof path, out, i, "");
        write_wav(path, 48000, d, m);
        free(d);
        d = render_ron_stress(i);
        ron_wavname(path, sizeof path, out, i, "_stress");
        write_wav(path, 48000, d, m);
        free(d);
    }
    if (n == 0) { printf("  Ron: keine Presets aus presets/Ron/ in der Tabelle\n"); fails++; }
    if (!front) { printf("  Ron: Presets stehen nicht vorn in der Tabelle\n"); fails++; }
    printf("  Ron: %d Presets, %s\n", n, fails ? "FEHLER" : "alle wie der Rust-Kern, vorn in der Tabelle");
    return fails;
}

/* Gerätefassung (MKSULTRA_FM1, 16-Bit-Effektleitungen, kürzere Puffer, 6 Stimmen): nicht bitgleich. Die Ron-Presets
 * müssen vorn stehen und im Pegel wie der Rust-Kern klingen: RMS auf 1 %, Spitze auf 5 % (ref/ron.txt). */
static int ron_check_fm1(const char *ref, const char *out)
{
    int fails = 0, n = 0, front = 1;
    printf("mksultra Gerätefassung (%d Stimmen, FM1 %d, 16 Bit %d): Ron-Presets gegen den Rust-Kern (Pegel)\n",
           MKSULTRA_VOICES, MKSULTRA_FM1, MKSULTRA_FX_INT16);
    for (int i = 0; i < mksultra_preset_count; i++) {
        if (!is_ron(i)) continue;
        if (i != n) front = 0;
        n++;
        float *d = render_ron(i);
        size_t m = 2 * RON_LEN;
        uint64_t want = 0;
        double sq = 0.0, pk = 0.0, wr = 0.0, wp = 0.0;
        for (size_t k = 0; k < m; k++) { sq += (double)d[k] * (double)d[k]; if (fabs((double)d[k]) > pk) pk = fabs((double)d[k]); }
        double rms = sqrt(sq / (double)m);
        int have = ron_ref(ref, mksultra_presets[i].path, &want, &wr, &wp);
        int ok = have && fabs(rms - wr) <= 0.01 * wr + 1e-5 && fabs(pk - wp) <= 0.05 * wp + 1e-4;
        printf("  Ron %-24s RMS %.6f (Soll %.6f, %+.2f %%) Spitze %.6f (Soll %.6f) %s\n", mksultra_presets[i].name, rms, wr,
               wr > 0 ? 100.0 * (rms - wr) / wr : 0.0, pk, wp, ok ? "ok" : "FEHLER");
        if (!ok) fails++;
        char path[1024];
        ron_wavname(path, sizeof path, out, i, "");
        write_wav(path, 48000, d, m);
        free(d);
    }
    if (n == 0 || !front) { printf("  Ron: fehlen oder stehen nicht vorn\n"); fails++; }
    printf("mksultra Gerätefassung: %s\n", fails ? "FEHLER" : "bestanden");
    return fails ? 1 : 0;
}

static void ron_versus(const char *out, const char *fulldir)
{
    for (int i = 0; i < mksultra_preset_count; i++) {
        if (!is_ron(i)) continue;
        for (int st = 0; st < 2; st++) {
            float *d = st ? render_ron_stress(i) : render_ron(i), *g = NULL;
            char path[1024], label[64];
            ron_wavname(path, sizeof path, out, i, st ? "_stress" : "");
            write_wav(path, 48000, d, 2 * RON_LEN);
            ron_wavname(path, sizeof path, fulldir, i, st ? "_stress" : "");
            snprintf(label, sizeof label, "Ron %.14s%s", mksultra_presets[i].name, st ? " 8N" : " 4N");
            if (read_wav(path, &g) == 2 * RON_LEN) compare(label, d, g, 2 * RON_LEN);
            else printf("  %-20s keine Vergleichsdatei\n", label);
            free(g);
            free(d);
        }
    }
}

static int golden(const char *ref, const char *out, const char *wavdir)
{
    int fails = 0;
    printf("mksultra: %d Golden-Fälle (Stimmen %d)\n", NCASES, MKSULTRA_VOICES);
    for (int i = 0; i < NCASES; i++) {
        const gcase_t *c = &CASES[i];
        float *d = render_case(c);
        if (!d) { fails++; continue; }
        size_t n = 2 * (size_t)c->len;
        uint64_t h = fnv1a(d, n), want = 0;
        int have = manifest_fnv(ref, c->name, &want);
        int ok = have && h == want;
        printf("  %-20s fnv1a64 %016llx %s\n", c->name, (unsigned long long)h,
               ok ? "ok" : have ? "FALSCH" : "(kein Sollwert)");
        if (!ok) fails++;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.wav", out, c->name);
        write_wav(path, (unsigned)c->sr, d, n);
        if (wavdir && !ok) {
            float *g = NULL;
            snprintf(path, sizeof path, "%s/%s.wav", wavdir, c->name);
            size_t gn = read_wav(path, &g);
            if (gn == n) compare(c->name, d, g, n);
            free(g);
        }
        free(d);
    }
    for (int i = 0; i < 2; i++) {
        float *d = render_stress(STRESS[i][1]);
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.wav", out, STRESS[i][0]);
        write_wav(path, 48000, d, 2 * STRESS_LEN);
        free(d);
    }
    fails += smoke();
    fails += ron_check(ref, out);
    printf("mksultra: %s (%d/%d Prüfsummen gleich)\n", fails ? "FEHLER" : "bestanden", NCASES - fails, NCASES);
    return fails ? 1 : 0;
}

/* Sparfassung: Abweichung gegen die WAVs der vollen Fassung; kein Fehler, nur Bericht */
static int versus(const char *out, const char *fulldir)
{
    printf("mksultra Sparfassung (%d Stimmen) gegen volle Fassung:\n", MKSULTRA_VOICES);
    for (int i = 0; i < NCASES; i++) {
        const gcase_t *c = &CASES[i];
        float *d = render_case(c);
        if (!d) continue;
        size_t n = 2 * (size_t)c->len;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.wav", out, c->name);
        write_wav(path, (unsigned)c->sr, d, n);
        float *g = NULL;
        snprintf(path, sizeof path, "%s/%s.wav", fulldir, c->name);
        size_t gn = read_wav(path, &g);
        if (gn == n) compare(c->name, d, g, n);
        else printf("  %-20s keine Vergleichsdatei\n", c->name);
        free(g);
        free(d);
    }
    ron_versus(out, fulldir);
    for (int i = 0; i < 2; i++) {
        float *d = render_stress(STRESS[i][1]), *g = NULL;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.wav", out, STRESS[i][0]);
        write_wav(path, 48000, d, 2 * STRESS_LEN);
        snprintf(path, sizeof path, "%s/%s.wav", fulldir, STRESS[i][0]);
        if (read_wav(path, &g) == 2 * STRESS_LEN) compare(STRESS[i][0], d, g, 2 * STRESS_LEN);
        free(g);
        free(d);
    }
    return 0;
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* Zeit für `secs` Sekunden mit `notes` gehaltenen Noten; Preset nach Pfad */
static double bench_one(const char *preset, int notes, double secs)
{
    void *s = state();
    mksultra_init(s, 48000.0f);
    mksultra_load_preset(s, &mksultra_presets[mksultra_preset_find(preset)]);
    int pv = mksultra_param_find("NumberOfVoices");
    mksultra_set_param(s, pv, 11.0f);
    static const int keys[12] = {48, 52, 55, 59, 60, 62, 64, 67, 69, 71, 72, 76};
    for (int k = 0; k < notes && k < MKSULTRA_VOICES; k++) mksultra_note_on(s, keys[k], 100);
    float l[256], r[256];
    int blocks = (int)(secs * 48000.0 / 256.0);
    double t0 = now();
    for (int b = 0; b < blocks; b++) mksultra_process(s, l, r, 256);
    return now() - t0;
}

static int bench(void)
{
    const double secs = 10.0;
    const char *plain = "default.mks", *uni = "Factory MKS/Drums/Short Hat.mks";
    int nmax = MKSULTRA_VOICES;
    double t0 = bench_one(plain, 0, secs);
    double t1 = bench_one(plain, 1, secs);
    double tn = bench_one(plain, nmax, secs);
    double u0 = bench_one(uni, 0, secs);
    double u1 = bench_one(uni, 1, secs);
    printf("mksultra Rechenzeit auf diesem PC (48 kHz, %.0f s Audio, %d Stimmen gebaut):\n", secs, nmax);
    printf("  Grundlast (Effekte, keine Note):  %.3f s = %.2f %% Echtzeit\n", t0, 100.0 * t0 / secs);
    printf("  je Stimme (default.mks):          %.3f s = %.2f %% Echtzeit, %.0f ns je Sample\n", t1 - t0,
           100.0 * (t1 - t0) / secs, 1e9 * (t1 - t0) / (secs * 48000.0));
    printf("  %2d Stimmen (default.mks):         %.3f s = %.2f %% Echtzeit\n", nmax, tn, 100.0 * tn / secs);
    printf("  je Unisono-Stimme (Short Hat):    %.3f s = %.2f %% Echtzeit, %.0f ns je Sample\n", u1 - u0,
           100.0 * (u1 - u0) / secs, 1e9 * (u1 - u0) / (secs * 48000.0));
    return 0;
}

/* Alle Presets: 4 Blöcke Vorlauf, Noten 60 (v100) und 64 (v90), 24 Blöcke halten, 24 Blöcke loslassen; Blöcke à 512. */
static int sweep(void)
{
    void *s = state();
    float l[512], r[512], d[1024];
    for (int i = 0; i < mksultra_preset_count; i++) {
        mksultra_init(s, 48000.0f);
        mksultra_load_preset(s, &mksultra_presets[i]);
        for (int k = 0; k < 4; k++) mksultra_process(s, l, r, 512);
        mksultra_note_on(s, 60, 100);
        mksultra_note_on(s, 64, 90);
        uint64_t h = 0xcbf29ce484222325ull;
        for (int b = 0; b < 48; b++) {
            if (b == 24) { mksultra_note_off(s, 60); mksultra_note_off(s, 64); }
            mksultra_process(s, l, r, 512);
            for (int k = 0; k < 512; k++) { d[2 * k] = l[k]; d[2 * k + 1] = r[k]; }
            const unsigned char *by = (const unsigned char *)d;
            for (int k = 0; k < 4096; k++) { h ^= by[k]; h *= 0x100000001b3ull; }
        }
        printf("%016llx %s\n", (unsigned long long)h, mksultra_presets[i].path);
    }
    return 0;
}

/* Weg über die Baustein-Tabelle (baustein.h): Werte 0..1, render addiert. Muss mksultra_process gleichen. */
static int smoke(void)
{
    const baustein_t *b = &mksultra_baustein;
    void *s1 = calloc(1, b->state_size), *s2 = calloc(1, b->state_size);
    int pc = mksultra_param_find("VcfCutoff"), pr = mksultra_param_find("VcfResonance");
    b->init(s1, 48000.0);
    b->set_param(s1, (unsigned)pc, 0.4);
    b->set_param(s1, (unsigned)pr, 0.7);
    mksultra_init(s2, 48000.0f);
    mksultra_set_param(s2, pc, 0.4f);
    mksultra_set_param(s2, pr, 0.7f);
    b->note_on(s1, 57, 110);
    mksultra_note_on(s2, 57, 110);
    static float al[24000], ar[24000], bl[24000], br[24000];
    int bad = 0, act_end = -1;
    for (int pos = 0; pos < 24000; pos += 100) {
        if (pos == 12000) { b->note_off(s1, 57); mksultra_note_off(s2, 57); }
        b->render(s1, al + pos, ar + pos, 100);
        mksultra_process(s2, bl + pos, br + pos, 100);
        if (b->active(s1)) act_end = pos;
    }
    for (int i = 0; i < 24000; i++) if (al[i] != bl[i] || ar[i] != br[i]) bad++;
    printf("  Baustein-Tabelle: %d Parameter, Zustand %zu Bytes, render %s, active bis Sample %d\n", (int)b->param_count,
           b->state_size, bad ? "WEICHT AB" : "gleich", act_end);
    free(s1);
    free(s2);
    return bad ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--sweep") == 0) return sweep();
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) return bench();
    if (argc >= 5 && strcmp(argv[1], "--vs") == 0) return versus(argv[3], argv[4]);
    if (argc >= 4 && strcmp(argv[1], "--fm1") == 0) return ron_check_fm1(argv[2], argv[3]);
    if (argc < 3) { fprintf(stderr, "Aufruf: test <ref> <out> [<wav>]\n"); return 2; }
    return golden(argv[1], argv[2], argc > 3 && argv[3][0] ? argv[3] : NULL);
}
