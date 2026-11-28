/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-Test für syn_pat.h (Sequencer-Zusätze nach den iOS-Apps, ADR 0052). */
#include "syn_pat.h"
#include <stdio.h>
#include <stdlib.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FEHLER: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void)
{
    /* GATE-Skala wie StepSequencer.gateSteps */
    CHECK(sp_gate_steps(0) == 0.05f, "gate 0");
    CHECK(sp_gate_steps(80) == 1.0f, "gate 80 = ein Schritt");
    CHECK(sp_gate_steps(81) == 1.0f && sp_gate_steps(127) == 8.0f, "gate 81/127");
    CHECK(sp_gate_steps(87) == 2.0f, "gate 87 -> 2 (7*7/47 = 1.04 -> 1)");
    for (unsigned v = 1; v <= 127; v++) CHECK(sp_gate_steps(v) >= sp_gate_steps(v - 1), "gate monoton %u", v);

    /* Rohwert wie SeqStep.raw */
    CHECK(sp_raw(SP_BASE, 0) == 13u, "raw Grundton");
    CHECK(sp_raw(SP_BASE + 18, SP_ACC | SP_TIE) == 127u, "raw max");
    CHECK(sp_raw(0, SP_REST) == 0u, "raw Pause");

    /* Zufallsfolgen: Umfang, erster Schritt, Bindung nur vor einer Note, gleicher Startwert = gleiche Folge */
    unsigned notes = 0, accs = 0, ties = 0, holds = 0;
    for (unsigned style = 0; style < SP_STYLES; style++)
        for (uint32_t seed = 1; seed <= 400; seed++) {
            sp_gen_t g, h;
            uint32_t s1 = seed * 2654435761u, s2 = s1;
            sp_generate(&g, style, &s1);
            sp_generate(&h, style, &s2);
            for (unsigned i = 0; i < SP_STEPS; i++) {
                CHECK(g.rel[i] == h.rel[i] && g.flag[i] == h.flag[i], "Stil %u: nicht reproduzierbar", style);
                CHECK(g.rel[i] >= -12 && g.rel[i] <= 18, "Stil %u: Note %d außerhalb", style, g.rel[i]);
                if (!(g.flag[i] & SP_REST)) { notes++; if (g.flag[i] & SP_ACC) accs++; if (g.flag[i] & SP_TIE) ties++; }
                if (g.flag[i] & SP_TIE) CHECK(i + 1 < SP_STEPS && !(g.flag[i + 1] & SP_REST) && !(g.flag[i] & SP_REST),
                                              "Stil %u: Tie auf Schritt %u ohne Note danach", style, i);
            }
            CHECK(!(g.flag[0] & SP_REST) && g.rel[0] == 0 && (style == 0 || style == 2 || (g.flag[0] & SP_ACC) || style == 3),
                  "Stil %u: erster Schritt ist nicht der Grundton", style);
            /* VARY ändert höchstens 2 Schritte, nie den ersten */
            sp_gen_t v = g;
            sp_vary(&v, style, &s1);
            unsigned d = 0;
            for (unsigned i = 0; i < SP_STEPS; i++) d += v.rel[i] != g.rel[i] || v.flag[i] != g.flag[i];
            CHECK(d <= 2 && v.rel[0] == g.rel[0] && v.flag[0] == g.flag[0], "VARY: %u Schritte geändert", d);
            /* G.VAR: fest je Folge, nie über das Ende, 0 = keine langen Noten */
            uint8_t n[SP_STEPS], f[SP_STEPS];
            for (unsigned i = 0; i < SP_STEPS; i++) { n[i] = (uint8_t)(SP_BASE + g.rel[i]); f[i] = g.flag[i]; }
            for (unsigned i = 0; i < SP_STEPS; i++) {
                unsigned a = sp_hold_steps(i, SP_STEPS, 127, n, f), b = sp_hold_steps(i, SP_STEPS, 127, n, f);
                CHECK(a == b && a >= 1 && a <= 4 && i + a <= SP_STEPS, "G.VAR Schritt %u: %u", i, a);
                CHECK(sp_hold_steps(i, SP_STEPS, 0, n, f) == 1u, "G.VAR 0");
                if (sp_hold_steps(i, SP_STEPS, 40, n, f) > 1u) CHECK(a > 1u, "G.VAR: mehr G.VAR nimmt lange Noten weg");
                holds += a > 1u;
            }
        }
    printf("syn_pat: %u Noten, %u Akzente, %u Bindungen, %u lange Noten (G.VAR voll) in %u Folgen\n", notes, accs, ties, holds,
           SP_STYLES * 400u);
    CHECK(accs > 0 && ties > 0 && holds > 0, "Akzente/Bindungen/lange Noten fehlen");
    CHECK(sizeof(sp_bank_t) <= 4096u, "Bank passt nicht in einen Blob (%u)", (unsigned)sizeof(sp_bank_t));
    if (fails) { printf("syn_pat: %d Fehler\n", fails); return 1; }
    printf("syn_pat: alles gut (Bank %u Byte)\n", (unsigned)sizeof(sp_bank_t));
    return 0;
}
