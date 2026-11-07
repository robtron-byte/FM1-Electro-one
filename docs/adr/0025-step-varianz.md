# 0025: Step-Varianz: ein Step spielt nur in jedem zweiten oder vierten Durchlauf

- **Status:** vorgeschlagen
- **Stand:** 2026-11-06

## Kontext

Wunsch des Nutzers: Im Sequencer (Drums und Synths) einem Step per Halten eine Varianz geben, „2:1 oder 4:1, dass jeder Schlag nur alle zwei Takte kommt“. So entstehen längere Bögen aus kurzen Patterns, wie die Trig-Bedingungen der Elektron-Geräte.

## Entscheidung

- **Bedingungen** (`engine/el_config.h`, `EL_COND_*`, `el_cond_plays`): `1:1` immer, `1:2` / `2:2` im ersten / zweiten von je zwei Durchläufen, `1:4` … `4:4` in einem von je vier. Gezählt werden die Durchläufe des eigenen Patterns ab seinem Start (STOP/START, Platzwechsel); bei einem Pattern von einem Takt sind das Takte.
- **Drums** (`engine/drums.c`): pro Platz, Instrument und Step, als drei Bitebenen (`dr_machine_t.cond`, 4 × 3 × 12 Wörter). `DR_CMD_COND` setzt sie. Ein Step, der ausgeschaltet wird, und „Spur/Pattern löschen“ vergessen sie; Kopieren nimmt sie mit; eine Variation behält sie, ein neues Pattern vom Generator fängt ohne an (`dr_post_pattern(…, keep_cond)`).
- **Synths** (`engine/synth_track.c`): in den freien Bits 3–5 der Step-Flags (`ST_COND`), `EL_CMD_STEP_COND`. Ein übersprungener Step wirkt wie eine Pause.
- **Wahrscheinlichkeit** (seit 2026-11-07): 100 % bis 10 % in Zehnerschritten (`EL_PROBS`, `el_prob_plays`), zusätzlich zur Bedingung: Ein Step spielt in seinem Durchlauf nur, wenn auch der Würfel (xorshift je Drum-Maschine bzw. Synth-Bereich) es will. Drums: vier weitere Bitebenen (`dr_machine_t.prob`, `DR_CMD_PROB`); Synths: Bits 6–7 der Flags und Bits 6–7 des Akkord-Bytes (`ST_PROB`, `EL_CMD_STEP_PROB`; die Akkordbefehle lassen sie stehen). Speicher-Version 23 hängt die Drum-Wahrscheinlichkeiten an.
- **Repeat** (Punch-in): Die Bedingung gilt nach dem Durchlauf der unsichtbar weiterlaufenden Abspielposition.
- **Bedienung** (`ui/el_ui.c`): Step-Taste halten (BEAT; Synth-Bereich im STEP-Modus), **PRESETS** dreht die Bedingung, **ALGORITHM** die Wahrscheinlichkeit (links seltener, eine Raste 10 %). Nach ¼ s Halten (oder sobald gedreht wird) zeigt das Display groß Bedingung und Prozent und darunter je Durchlauf des Zyklus ein Kästchen: gefüllt, wo der Step spielen kann, so weit wie seine Wahrscheinlichkeit. Ein stiller Step klingt beim Drücken sofort; ein klingender geht erst beim Loslassen aus, und nur, wenn währenddessen kein Regler gedreht wurde. PRESETS statt beliebiger Regler: Im STEP-Modus der Synths stellen KNOB 1–4 schon Note, Oktave, Akkord und Umkehrung des Steps ein.
- **Anzeige:** ein kleiner weißer Strich oben links im Feld (Drums) bzw. unten in der Spalte (Synths); im STEP-Modus steht die Bedingung in der Infozeile.
- **Speicher** (`fm1/el_store.c`, Version 22): die Drum-Bedingungen hinten angehängt; die der Synths stecken in den Flags. Ältere Stände laden ohne Bedingungen.

## Alternativen

- **Bedingung im `dr_pattern_t`:** Es geht auch durch Generator, Staging und Flash-Format; jede Änderung daran verschiebt den ganzen gespeicherten Stand. Wie bei den Sounds pro Platz (ADR 0024) deshalb daneben.
- **Wahrscheinlichkeit statt fester Durchläufe:** nicht gewünscht; feste Durchläufe bleiben reproduzierbar.

## Folgen

- RAM: 576 Byte für die Drum-Bedingungen.
- Host-Test `host/ui_sim.c` („steps: conditions …“): 2:2 bei den Drums, 1:4 bei einem Synth-Step, unter Repeat, die Bedienung mit gehaltenem Step.

## Verlauf

- 2026-11-06: angelegt auf Wunsch des Nutzers
- 2026-11-07: Wahrscheinlichkeit pro Step und große Anzeige beim Halten, auf Wunsch des Nutzers
