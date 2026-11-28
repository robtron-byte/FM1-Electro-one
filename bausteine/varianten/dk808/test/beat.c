/* SPDX-License-Identifier: GPL-3.0-only
 * 808 solo: Host-Test der Drum-Machine (app/dk_beat.c, ADR 0050): Steps im Takt (mit Swing), Plätze, Befehle,
 * Klang auf beiden Zuständen, Fill, Generator. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "dk_beat.h"
static int fails;
static void chk(int ok, const char *w) { if (!ok) { printf("beat: FEHLER %s\n", w); fails++; } }
enum { B = 64 };
static float l[B], r[B], d[B], l1[B], r1[B], d1[B];
/* ein Block wie im Gerät: Takt (ticks = 24 je Viertel), dann Kern 0 und Kern 1 */
static double run(double *ticks, float bpm, int running, unsigned blocks, float *peak)
{
    double tick_per_frame = bpm * 24.0 / 60.0 / 44100.0;
    for (unsigned b = 0; b < blocks; b++) {
        if (running) *ticks += tick_per_frame * B;
        db_block(running, (float)*ticks, bpm, B);
        memset(l, 0, sizeof l); memset(r, 0, sizeof r); memset(d, 0, sizeof d);
        memset(l1, 0, sizeof l1); memset(r1, 0, sizeof r1); memset(d1, 0, sizeof d1);
        db_render_k0(l, r, d, B); db_render_k1(l1, r1, d1, B);
        for (int i = 0; i < B; i++) { float v = fabsf(l[i] + l1[i]); if (v > *peak) *peak = v; }
    }
    return *ticks;
}
int main(void)
{
    double t = 0; float pk = 0;
    db_init(); db_restore();
    chk(db.s.pattern[0].hit[P_BD] != 0, "Grundgroove A hat eine Bassdrum");
    chk(db.s.pattern[1].hit[P_BD] == 0, "B leer");
    /* zwei Takte bei 120 BPM: 32 Steps (Länge 16: zwei Durchgänge) */
    unsigned steps = 0; uint8_t last = 0xff;
    for (unsigned b = 0; b < (unsigned)(2 * 2.0 * 44100 / B); b++) {
        run(&t, 120, 1, 1, &pk);
        if (db.pos != last) { steps++; last = db.pos; }
    }
    chk(steps >= 31 && steps <= 33, "32 Steps in zwei Takten");
    chk(db.pass >= 1, "zweiter Durchgang");
    chk(pk > 0.05f, "es klingt");
    printf("beat: %u Steps in 2 Takten, Spitze %.2f, Durchgang %u\n", steps, pk, db.pass);
    /* Platz B wählen (mitten im Pattern): Wechsel am Ende des Patterns */
    run(&t, 120, 1, (unsigned)(0.5 * 44100 / B), &pk);
    db_post(DB_SELECT, 1, 0, 0, 0);
    run(&t, 120, 1, 1, &pk);
    chk(db.next == 1 && db.current == 0, "B wartet aufs Pattern-Ende");
    run(&t, 120, 1, (unsigned)(2.2 * 44100 / B), &pk);
    chk(db.current == 1, "B spielt nach dem Ende");
    /* Step setzen, Akzent, Länge, Mute */
    db_post(DB_TOGGLE, P_SD, 4, 0, 0); db_post(DB_ACCENT, 0, 4, 0, 0); db_post(DB_LENGTH, 0, 32, 0, 0); db_post(DB_MUTE, P_MA, 1, 0, 0);
    run(&t, 120, 1, 1, &pk);
    chk(db.s.pattern[1].hit[P_SD] == (1u << 4 | 1u << 20) && db.s.pattern[1].accent == (1u << 4 | 1u << 20), "Step und Akzent (Länge 32 wiederholt den Takt)");
    chk(db.s.pattern[1].length == 32, "Länge 32");
    chk(db.mute == (1u << P_MA), "Mute Teil 16 (32 Bit)");
    /* Stop, Kit, Regler */
    run(&t, 120, 0, 2, &pk);
    chk(!db.playing, "Stop");
    db_post(DB_KIT, 2, 0, 0, 0); db_post(DB_PARAM, P_BD, 0, 0, 50);
    run(&t, 120, 0, 1, &pk);
    chk(db.s.kit_index == 2 && db.s.sound.v[P_BD][0] == 50, "Kit und Pegel");
    /* Generator: alle Stile, Länge und Zeilen in Ordnung */
    for (unsigned s = 0; s < DB_STYLES; s++) {
        db_pattern_t g; db_generate(&g, s, 64, 1234u + s, 0);
        chk(g.length == 16 || g.length == 32, "Stil-Länge");
        chk(g.hit[P_BD] != 0, db_style_names[s]);
    }
    /* Live-Aufnahme: REC, Note 38 während des Laufs landet im Pattern */
    db_post(DB_SELECT, 2, 1, 0, 0); db_post(DB_RECORD, 0, 1, 0, 0);
    t = 0; run(&t, 120, 1, 40, &pk);
    db_note(38, 100);
    run(&t, 120, 1, 1, &pk);
    chk(db.s.pattern[2].hit[P_SD] != 0, "Live-Aufnahme");
    /* MIDI-Noten aller 17 Teile */
    for (unsigned p = 0; p < DB_PARTS; p++) chk(db_part_of_note((int)db_part_note(p)) == (int)p, db_part_names[p]);
    printf("beat: %s\n", fails ? "FEHLER" : "ok");
    return fails != 0;
}
