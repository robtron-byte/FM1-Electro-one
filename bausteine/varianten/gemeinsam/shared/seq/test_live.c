/* SPDX-License-Identifier: GPL-3.0-only
 * Host-Test der Live-Aufnahme (syn_live.h, docs/adr/0053): erste Aufnahme legt die Länge fest (ganze Takte), 1/16,
 * Notenlänge, Wiedergabe, Overdub, STOP verwirft den Durchgang, Länge ändern (wiederholen, leer, Anfang, Ende). */
#include <stdio.h>
static int on_n, off_n, last_on = -1;
#define LV_PARTS 4
#define LV_NOTE_ON(p, n, v) (on_n++, last_on = (n))
#define LV_NOTE_OFF(p, n) (off_n++)
#include "syn_live.h"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static void run(unsigned ticks) { while (ticks--) lv_block(1); }
int main(void)
{
    lv_reset();
    /* erste Aufnahme: 2 Takte und ein bisschen, Noten bei Tick 1 (-> 1/16 0) und 50 (-> 8), 13 Ticks gehalten */
    lv_ui_rec(0, 0); lv_block(0);
    CHECK(lv_state == LV_FIRST, "Aufnahme läuft nicht");
    run(1); lv_hear(0, 60, 100, 1); run(13); lv_hear(0, 60, 0, 0);
    run(50 - 14); lv_hear(0, 64, 90, 1); run(20); lv_hear(0, 64, 0, 0);
    run(2 * 96 + 30 - 70);              /* REC bei 2 Takten + 30 Ticks: 2 Takte */
    lv_ui_rec(0, 1000); lv_block(0);
    CHECK(lv_state == LV_OFF && lv_store.p[0].bars == 2, "Länge %u statt 2", lv_store.p[0].bars);
    CHECK(lv_store.p[0].n == 2 && lv_store.ev[0][0].at == 0 && lv_store.ev[0][0].len == 2 && lv_store.ev[0][1].at == 8,
          "Ereignisse %u, at %u len %u, at %u", lv_store.p[0].n, lv_store.ev[0][0].at, lv_store.ev[0][0].len, lv_store.ev[0][1].at);
    /* Wiedergabe: ein Durchgang (2 Takte) spielt beide Noten und schaltet sie wieder aus */
    on_n = off_n = 0; run(192);
    CHECK(on_n == 2 && off_n == 2, "Wiedergabe %d an, %d aus", on_n, off_n);
    /* Overdub: eine Note dazu, dann STOP: verworfen */
    lv_ui_rec(0, 5000); lv_block(0);
    CHECK(lv_state == LV_OVER, "Overdub läuft nicht");
    run(30); lv_hear(0, 67, 100, 1); run(6); lv_hear(0, 67, 0, 0);
    lv_ui_stop(0); lv_block(0);
    CHECK(lv_store.p[0].n == 2 && lv_state == LV_OFF, "STOP: %u Ereignisse", lv_store.p[0].n);
    /* Overdub über das Schleifenende: der fertige Durchgang bleibt, auch wenn danach STOP kommt */
    lv_ui_rec(0, 9000); lv_block(0);
    run(10); lv_hear(0, 67, 100, 1); run(6); lv_hear(0, 67, 0, 0);
    run(192); lv_ui_stop(0); lv_block(0);
    CHECK(lv_store.p[0].n == 3, "Durchgang nicht behalten: %u", lv_store.p[0].n);
    /* Doppeltippen: Einzählen (ein Takt), dann Aufnahme */
    lv_ui_rec(1, 20000); lv_block(0); lv_ui_rec(1, 20200); lv_block(0);
    CHECK(lv_state == LV_COUNT, "kein Einzählen");
    run(96); CHECK(lv_state == LV_FIRST && lv_part == 1, "nach dem Einzählen keine Aufnahme");
    lv_ui_stop(1); lv_block(0);
    /* Länge: 2 -> 4 wiederholen, dann 4 -> 1 Ende behalten, dann 0 = leer */
    lv_ui_set(0, LV_SET_GROW, LV_GROW_REPEAT); lv_ui_set(0, LV_SET_BARS, 4); lv_block(0);
    CHECK(lv_store.p[0].bars == 4 && lv_store.p[0].n == 6, "verlängert: %u Takte, %u Ereignisse", lv_store.p[0].bars, lv_store.p[0].n);
    lv_ui_set(0, LV_SET_CUT, LV_CUT_END); lv_ui_set(0, LV_SET_BARS, 2); lv_block(0);   /* das Ende: die Wiederholung */
    CHECK(lv_store.p[0].bars == 2 && lv_store.p[0].n == 3, "gekürzt (Ende): %u Ereignisse", lv_store.p[0].n);
    {
        unsigned want = 0;
        for (unsigned i = 0; i < lv_store.p[0].n; i++) want += lv_store.ev[0][i].at < 16u;
        lv_ui_set(0, LV_SET_CUT, LV_CUT_START); lv_ui_set(0, LV_SET_BARS, 1); lv_block(0);
        CHECK(lv_store.p[0].bars == 1 && lv_store.p[0].n == want && want >= 2, "gekürzt (Anfang): %u statt %u", lv_store.p[0].n, want);
    }
    lv_ui_set(0, LV_SET_GROW, LV_GROW_EMPTY); lv_ui_set(0, LV_SET_BARS, 3); lv_block(0);
    CHECK(lv_store.p[0].bars == 3 && lv_store.p[0].n >= 2 && lv_store.p[0].n <= 3, "leer verlängert: %u", lv_store.p[0].n);
    lv_ui_set(0, LV_SET_BARS, 0); lv_block(0);
    CHECK(lv_store.p[0].bars == 0 && lv_store.p[0].n == 0, "nicht gelöscht");
    /* höchstens LV_BARS: die erste Aufnahme endet von selbst */
    lv_ui_rec(2, 30000); lv_block(0); run(LV_BARS * 96);
    CHECK(lv_state == LV_OFF && lv_store.p[2].bars == LV_BARS, "Höchstlänge: %u", lv_store.p[2].bars);
    printf(fails ? "live: %d FEHLER\n" : "live: alles gut (%u Ereignisse je Part, %u Takte)\n", fails ? fails : LV_EV, LV_BARS);
    return fails != 0;
}
