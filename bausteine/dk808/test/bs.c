/* SPDX-License-Identifier: GPL-3.0-only
 * dk808: Test des Gerüst-Adapters (dk808_bs.h, docs/adr/0050): INIT-Kit = Standard des DrumKern (Sample für Sample
 * gleich dk808_render), Pegel 0 = stumm, Pegel 50 = halb, DIST SEND füllt den Send, Namen, all_off. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "dk808_bs.h"
enum { SR = 44100, N = 8192, B = 64 };
static dk808_t ref;
static dk808_bs_t bs;
static float rl[N], rr[N], l[N], r[N], ds[N];
static int fails;
static void chk(int ok, const char *what) { if (!ok) { printf("dk808_bs: FEHLER %s\n", what); fails++; } }
static void run(dk808_bs_t *d, float *dst) { for (int p = 0; p < N; p += B) dk808_bs_render(d, l + p, r + p, dst ? dst + p : 0, B); }
int main(void)
{
    char nm[16];
    for (unsigned i = 0; i < DK_COUNT; i++) {
        static const int notes[DK_COUNT] = {36, 58, 38, 37, 39, 64, 63, 62, 41, 47, 43, 42, 46, 49, 75, 70, 56};
        memset(rl, 0, sizeof rl); memset(rr, 0, sizeof rr); memset(l, 0, sizeof l); memset(r, 0, sizeof r);
        dk808_init(&ref, SR); dk808_trigger(&ref, i, 100);
        for (int p = 0; p < N; p += B) dk808_render(&ref, rl + p, rr + p, B);
        dk808_bs_init(&bs, SR); dk808_bs_load_values(&bs, dk808_bs_init_kit);
        chk(dk808_bs_note_on(&bs, notes[i], 100) == (int)i, "Drum-Map");
        run(&bs, 0);
        chk(!memcmp(l, rl, sizeof l) && !memcmp(r, rr, sizeof r), dk808_instrument_abbr(i));
        /* Pegel 50: halb */
        float pk = 0, e = 0;
        for (int k = 0; k < N; k++) pk = fmaxf(pk, fabsf(rl[k]));
        dk808_bs_init(&bs, SR); dk808_bs_load_values(&bs, dk808_bs_init_kit);
        dk808_bs_set_raw(&bs, DK_BS_LEVEL + i, 50); dk808_bs_set_raw(&bs, DK_BS_DIST, 100);
        memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(ds, 0, sizeof ds);
        dk808_bs_note_on(&bs, notes[i], 100); run(&bs, ds);
        for (int k = 0; k < N; k++) e = fmaxf(e, fabsf(l[k] - 0.5f * rl[k]) + fabsf(ds[k] - l[k]));
        chk(e <= 1e-6f * pk, "Pegel 50 / DIST SEND");
    }
    dk808_bs_init(&bs, SR); dk808_bs_load_values(&bs, dk808_bs_init_kit);
    dk808_bs_set_raw(&bs, DK_BS_LEVEL + DK_BD, 0); memset(l, 0, sizeof l);
    dk808_bs_note_on(&bs, 36, 127); run(&bs, 0);
    float m = 0; for (int k = 0; k < N; k++) m = fmaxf(m, fabsf(l[k]));
    chk(m == 0, "Pegel 0");
    dk808_bs_init(&bs, SR); dk808_bs_note_on(&bs, 38, 127); dk808_bs_note_on(&bs, 46, 127); run(&bs, 0);
    chk(dk808_bs_voices(&bs) == 2, "Stimmen");
    dk808_bs_all_off(&bs); chk(dk808_bs_voices(&bs) == 0, "all_off");
    chk(!strcmp(dk808_bs_param_name(DK_BS_KNOB + DK_SD * 3 + 2, nm), "SD SNAPPY"), "Name");
    chk(!strcmp(dk808_bs_param_name(DK_BS_LEVEL + DK_CB, nm), "CB LEVEL"), "Name Pegel");
    chk(dk808_bs_param_name(DK_BS_KNOB + DK_MA * 3 + 1, nm)[0] == 0, "leerer Regler");
    chk(DK_BS_NP == 72, "DK_BS_NP");
    {   /* Aufteilung auf zwei Kerne (dk808_set_run): Metall-Instrumente in B, der Rest in A; A + B = alle */
        static dk808_bs_t a, b2;
        static float al[N], ar[N];
        float pk = 0, e = 0;
        dk808_bs_init(&bs, SR); dk808_bs_init(&a, SR); dk808_bs_init(&b2, SR);
        dk808_set_run(&a.k, DK808_MASK_ALL & ~DK808_METAL_USERS); dk808_set_run(&b2.k, DK808_METAL_USERS);
        memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(al, 0, sizeof al); memset(ar, 0, sizeof ar);
        for (int p = 0; p < N; p += B) {
            if (p % 1024 == 0)
                for (unsigned i = 0; i < DK_COUNT; i++)
                    if ((p / 1024 + i) % 3 == 0) {
                        dk808_trigger(&bs.k, i, 90 + (int)i); dk808_trigger(&a.k, i, 90 + (int)i); dk808_trigger(&b2.k, i, 90 + (int)i);
                    }
            dk808_bs_render(&bs, l + p, r + p, 0, B);
            dk808_bs_render(&a, al + p, ar + p, 0, B);
            dk808_bs_render(&b2, al + p, ar + p, 0, B);
        }
        for (int k = 0; k < N; k++) { pk = fmaxf(pk, fabsf(l[k])); e = fmaxf(e, fabsf(l[k] - al[k])); }
        chk(pk > 0.1f && e <= 1e-5f * pk, "Aufteilung A + B = alle");
        printf("dk808_bs: Aufteilung auf zwei Zustände: größte Abweichung %.1e der Spitze\n", (double)(e / pk));
    }
    printf("dk808_bs: Adapter %s (%u Werte je Kit)\n", fails ? "FEHLER" : "ok", DK_BS_NP);
    return fails != 0;
}
