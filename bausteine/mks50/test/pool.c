/* SPDX-License-Identifier: GPL-3.0-only
 * mks50: Prüfung für MKS-50 solo (docs/adr/0047). Die Firmware rechnet mit dem Pool-Kern aus fm1-synths
 * (bausteine/varianten/mks50/engine/mks.c: Stimmen-Pool über vier Parts, zwei Kerne), nicht mit engine/mks.c. Mit
 * einem Part muss er Sample für Sample gleich klingen: jeder Werks-Ton und jedes eigene Preset spielt einen Akkord
 * C-E-G (Velocity 100, 0,6 s, 1,6 s gesamt, Blöcke von 64) in beiden Kernen. */
#include "mks.h"
#include "mks50_eigene.h"
#include <stdio.h>
#include <string.h>
unsigned pk_count(void); const char *pk_name(unsigned i); int pk_block(void);
void pk_init(unsigned i); void pk_on(int note); void pk_off(int note); void pk_render(float *l, float *r);
enum { B = 64, N = 44100 * 16 / 10 / B * B, ON = 44100 * 6 / 10 / B * B };
static float al[N], ar[N], bl[N], br[N];
static const mks_preset_t *find(const char *name)
{
    for (unsigned j = 0; j < mks50_eigene_count; j++) if (!strcmp(mks50_eigene[j].name, name)) return &mks50_eigene[j];
    for (unsigned j = 0; j < mks_preset_count; j++) if (!strcmp(mks_presets[j].name, name)) return &mks_presets[j];
    return 0;
}
int main(void)
{
    static const int ch[3] = {60, 64, 67};
    static mks_part_t p;
    static float d[B], v[B];
    unsigned same = 0, n = 0, missing = 0;
    if (pk_block() != B) { printf("FAIL: Block %d\n", pk_block()); return 1; }
    for (unsigned i = 0; i < pk_count(); i++) {
        const mks_preset_t *q = find(pk_name(i));
        if (!q) { { printf("FAIL: %s fehlt im Baustein\n", pk_name(i)); ++missing; } continue; }
        memset(al, 0, sizeof al); memset(ar, 0, sizeof ar); memset(bl, 0, sizeof bl); memset(br, 0, sizeof br);
        pk_init(i);
        mks_part_init(&p, 6, 1u);
        mks_part_load(&p, q->v);
        for (int pos = 0; pos < N; pos += B) {
            if (pos == 0) for (int c = 0; c < 3; c++) { pk_on(ch[c]); mks_note_on(&p, ch[c], 100 / 127.0f, 0); }
            if (pos == ON) for (int c = 0; c < 3; c++) { pk_off(ch[c]); mks_note_off(&p, ch[c]); }
            pk_render(al + pos, ar + pos);
            memset(d, 0, sizeof d); memset(v, 0, sizeof v);
            mks_render(&p, bl + pos, br + pos, d, v, B, 120.0f);
        }
        ++n;
        int eq = !memcmp(al, bl, sizeof al) && !memcmp(ar, br, sizeof ar);
        same += eq;
        if (!eq) printf("FAIL: %s: Pool-Kern und Baustein verschieden\n", pk_name(i));
    }
    printf("mks50 pool: %u von %u Sounds bitgleich (Pool-Kern der Firmware gegen engine/mks.c, mit MKS_RELEASE_HURRY wie dort)\n", same, n);
    return same == n && !missing && n >= 56 ? 0 : 1;
}
