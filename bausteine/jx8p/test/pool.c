/* SPDX-License-Identifier: GPL-3.0-only
 * jx8p: Prüfung für JX-8P solo (docs/adr/0046). Die Firmware rechnet mit dem Pool-Kern aus fm1-synths
 * (bausteine/varianten/jx8p/engine/jx.c: Stimmen-Pool über vier Parts, zwei Kerne), nicht mit engine/jx8p.c. Mit
 * einem Part muss er Sample für Sample gleich klingen: jeder Werks-Ton und jedes eigene Preset spielt einen Akkord
 * C-E-G (Velocity 100, 0,6 s, 1,6 s gesamt, Blöcke von 64) in beiden Kernen. */
#include "jx8p.h"
#include "jx8p_eigene.h"
#include <stdio.h>
#include <string.h>
unsigned pk_count(void); const char *pk_name(unsigned i); int pk_block(void);
void pk_init(unsigned i); void pk_on(int note); void pk_off(int note); void pk_render(float *l, float *r);
enum { B = 64, N = 44100 * 16 / 10 / B * B, ON = 44100 * 6 / 10 / B * B };
static float al[N], ar[N], bl[N], br[N];
static const jx8p_preset_t *find(const char *name)
{
    for (unsigned j = 0; j < jx8p_eigene_count; j++) if (!strcmp(jx8p_eigene[j].name, name)) return &jx8p_eigene[j];
    for (unsigned j = 0; j < jx8p_preset_count; j++) if (!strcmp(jx8p_presets[j].name, name)) return &jx8p_presets[j];
    return 0;
}
int main(void)
{
    static const int ch[3] = {60, 64, 67};
    static jx8p_part_t p;
    static float d[B], v[B];
    unsigned same = 0, n = 0, missing = 0;
    if (pk_block() != B) { printf("FAIL: Block %d\n", pk_block()); return 1; }
    for (unsigned i = 0; i < pk_count(); i++) {
        const jx8p_preset_t *q = find(pk_name(i));
        if (!q) { if (strcmp(pk_name(i), "PG8X INIT")) { printf("FAIL: %s fehlt im Baustein\n", pk_name(i)); ++missing; } continue; }
        memset(al, 0, sizeof al); memset(ar, 0, sizeof ar); memset(bl, 0, sizeof bl); memset(br, 0, sizeof br);
        pk_init(i);
        jx8p_part_init(&p, 6, 1u);
        jx8p_part_load(&p, q->v);
        for (int pos = 0; pos < N; pos += B) {
            if (pos == 0) for (int c = 0; c < 3; c++) { pk_on(ch[c]); jx8p_note_on(&p, ch[c], 100 / 127.0f, 0); }
            if (pos == ON) for (int c = 0; c < 3; c++) { pk_off(ch[c]); jx8p_note_off(&p, ch[c]); }
            pk_render(al + pos, ar + pos);
            memset(d, 0, sizeof d); memset(v, 0, sizeof v);
            jx8p_render(&p, bl + pos, br + pos, d, v, B, 120.0f);
        }
        ++n;
        int eq = !memcmp(al, bl, sizeof al) && !memcmp(ar, br, sizeof ar);
        same += eq;
        if (!eq) printf("FAIL: %s: Pool-Kern und Baustein verschieden\n", pk_name(i));
    }
    printf("jx8p pool: %u von %u Sounds bitgleich (Pool-Kern der Firmware gegen engine/jx8p.c)\n", same, n);
    return same == n && !missing && n > 64 ? 0 : 1;
}
