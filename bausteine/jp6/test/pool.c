/* SPDX-License-Identifier: GPL-3.0-only
 * jp6: Prüfung für JP-6 solo (docs/adr/0048). Die Firmware rechnet mit dem Pool-Kern bausteine/varianten/jp6/engine/jpp.c
 * (vier Parts, Stimmen-Pool, zwei Kerne), der engine/jp6.c einbindet. Mit einem Part muss jpp_render Sample für Sample
 * wie jp6_render klingen: jeder Werks-Sound und jedes eigene Preset spielt einen Akkord C-E-G (Velocity 100, 0,6 s,
 * 1,6 s gesamt, Blöcke von 64). Dazu: vier Parts zugleich bleiben im Pool (höchstens JPP_POOL Stimmen). */
#include "jpp.h"
#include "jp6_eigene.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
enum { B = JPP_BLOCK, N = 44100 * 16 / 10 / B * B, ON = 44100 * 6 / 10 / B * B };
static float al[N], ar[N], bl[N], br[N];
static jpp_t s;
static jp6_part_t p;
int main(void)
{
    static const int ch[3] = {60, 64, 67};
    static float d[B], v[B];
    unsigned same = 0, n = 0, fails = 0;
    for (unsigned i = 0; i < jp6_eigene_count + jp6_preset_count; i++) {
        const jp6_preset_t *q = i < jp6_eigene_count ? &jp6_eigene[i] : &jp6_presets[i - jp6_eigene_count];
        memset(al, 0, sizeof al); memset(ar, 0, sizeof ar); memset(bl, 0, sizeof bl); memset(br, 0, sizeof br);
        jpp_init(&s, JPP_POOL, 1u);
        jpp_part_load(&s, 0, q->v);
        jp6_part_init(&p, JP6_MAX_VOICES, 1u);
        jp6_part_load(&p, q->v);
        for (int pos = 0; pos < N; pos += B) {
            if (pos == 0) for (int c = 0; c < 3; c++) { jpp_note_on(&s, 0, ch[c], 100 / 127.0f, 0); jp6_note_on(&p, ch[c], 100 / 127.0f, 0); }
            if (pos == ON) for (int c = 0; c < 3; c++) { jpp_note_off(&s, 0, ch[c]); jp6_note_off(&p, ch[c]); }
            jpp_render(&s, al + pos, ar + pos, 120.0f);
            memset(d, 0, sizeof d); memset(v, 0, sizeof v);
            jp6_render(&p, bl + pos, br + pos, d, v, B, 120.0f);
        }
        ++n;
        int eq = !memcmp(al, bl, sizeof al) && !memcmp(ar, br, sizeof ar);
        same += eq;
        int f = -1; for (int k = 0; k < N; k++) if (al[k] != bl[k] || ar[k] != br[k]) { f = k; break; }
        if (!eq) printf("FAIL: %s: Pool-Kern und jp6_render verschieden ab Sample %d\n", q->name, f);
    }
    /* zwei Kerne (Stimmen auf beide verteilt): nur die Reihenfolge der Summen ändert sich */
    {
        double e = 0, sig = 0;
        for (int two = 0; two < 2; two++) {
            float *L = two ? bl : al, *R = two ? br : ar;
            memset(L, 0, sizeof al); memset(R, 0, sizeof ar);
            jpp_init(&s, JPP_POOL, 1u);
            jpp_part_load(&s, 0, jp6_presets[0].v);
            jpp_part_set(&s, 0, JP_KEY_MODE, 0);
            for (int pos = 0; pos < N; pos += B) {
                if (pos == 0) for (int c = 0; c < 6; c++) jpp_note_on(&s, 0, 48 + 4 * c, 0.8f, 0);
                if (pos == ON) for (int c = 0; c < 6; c++) jpp_note_off(&s, 0, 48 + 4 * c);
                s.cores = (uint8_t)(two ? 2 : 1);
                jpp_plan(&s); jpp_control(&s, 0, 120.0f); jpp_voices(&s, 0); jpp_voices(&s, 1);
                jpp_finish(&s, 0); jpp_finish(&s, 1); jpp_mix(&s, L + pos, R + pos);
            }
        }
        for (int k = 0; k < N; k++) { e += (al[k] - bl[k]) * (al[k] - bl[k]); sig += al[k] * al[k]; }
        double db = e > 0 ? 10 * log10(e / sig) : -999;
        printf("jp6 pool: zwei Kerne gegen einen: Abweichung %.0f dB\n", db);
        if (db > -100) { printf("FAIL: zwei Kerne weichen ab\n"); ++fails; }
    }
    /* Pool: vier Parts, je vier Noten: nie mehr als JPP_POOL Stimmen */
    jpp_init(&s, JPP_POOL, 1u);
    unsigned most = 0;
    for (unsigned pt = 0; pt < JPP_PARTS; pt++) {
        jpp_part_load(&s, pt, jp6_presets[pt].v);
        jpp_part_set(&s, pt, JP_KEY_MODE, 0);
        for (int c = 0; c < 4; c++) jpp_note_on(&s, pt, 48 + 12 * (int)pt + c * 3, 0.8f, 0);
        jpp_render(&s, al, ar, 120.0f);
        unsigned t = 0;
        for (unsigned q = 0; q < JPP_PARTS; q++) t += jpp_part_voices(&s, q);
        if (t > most) most = t;
    }
    if (most > JPP_POOL) { printf("FAIL: Pool %u Stimmen statt höchstens %u\n", most, (unsigned)JPP_POOL); ++fails; }
    printf("jp6 pool: %u von %u Sounds bitgleich (Pool-Kern der Firmware gegen jp6_render), Pool höchstens %u Stimmen\n",
           same, n, most);
    return same == n && !fails ? 0 : 1;
}
