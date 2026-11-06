# 0019: Rechenlast auf dem echten FM-1: Lastanzeige, CPU-Test, Master schläft bei Stille

- **Status:** vorgeschlagen
- **Stand:** 2026-11-05

## Kontext

Nach der Korrektur aus ADR 0018 startet FM1-Electro auf dem Gerät. Mit dem Sequencer war es aber dreifach überlastet:

- Ein Pattern bei 120 BPM brauchte 6 statt 2 Sekunden.
- Aus dem Sequencer kam kein Ton, und die Bedienung war träge.
- Ein einzelner Synth über die Tastatur klang.

**Ursache im SDK:** `app/board/audio.c` lässt einen Block aus, wenn die Pause zu lang war, und verwirft einen Block, der nach 7/8 seiner Zeit nicht fertig ist. Der Sequencer zählt in berechneten Samples, also läuft er langsamer, und die zu späten Blöcke bleiben stumm.

**Messungen im Emulator:**
- Die Original-Firmware V16 führt beim Spielen **mehr** Befehle aus als FM1-Electro (165 gegen 134 Mio. pro Sekunde) und läuft auf dem Gerät flüssig.
- Bei FM1-Electro sind im Audio-Interrupt etwa 17 % der Befehle FPU-Befehle, bei V16 0,2 %.
- FM1-Electro erzeugt außerdem sehr viele Denormal-Zahlen. V16 erzeugt fast keine.

**Andere Firmwares:**
- X0X misst etwa 1.300 Befehle pro Sample bei 115 % Last eines Kerns, und das mit Float-DSP.
- SLOOP rechnet fast nur Festkomma (Q15): 2 von 59 Quelldateien benutzen `float`.

**Vermutung:** FPU-Befehle (und vielleicht Denormals) kosten auf dem AC79 ein Vielfaches eines Ganzzahlbefehls. Der Emulator zählt einen Takt pro Befehl.

**Lastverteilung beim Spielen (Emulator, Befehle):**

| Teil | Anteil |
|---|---|
| MKS-50-Stimmen (davon die Chop-Pulsbreite allein 12 %) | 34 % |
| Effekte | 22 % |
| Drums | 20 % |
| Master-Bus | 12 % |

Im Stillstand nahm der Master-Bus 63 %, obwohl nichts klang.

**Heap:** Der Heap war beim SDK-Start nur gut 1 KB vor dem Absturz. Die acht Standardinstrumente des SDK brauchen kurz 77 KB. Schon 928 Byte mehr statischer Speicher ließen das achte und danach den Bluetooth-Start scheitern (`le_hw_init`, `cpu_assert_debug`).

## Entscheidung

Schritt 1, ohne hörbare Klangänderung:

- **Lastanzeige im Header** (nur Gerät):
  - Die Renderzeit jedes Blocks wird mit dem Zeitzähler des SDK gemessen (`0x1EEF0F0`, Umlauf bei `0x1EEF0F4`, wie in `app/board/audio.c`).
  - Angezeigt wird der langsamste Block je halbe Sekunde in Prozent eines Blocks.
  - Farben: grün unter 70 %, gelb ab 70 %, rot ab 88 % oder wenn das SDK Blöcke verworfen hat (`fm1_audio_stats.overruns`).
- **CPU-Test beim Start** (`fm1/el_fm1.c`):
  - Er misst die Dauer je einer verketteten Operation: leere Schleife, Integer-Mul-Add, FADD, FMUL, 4 unabhängige FMUL, FDIV, FMUL mit Denormal.
  - Es zählt jeweils der kürzeste von 5 Läufen à 2000 Schritten.
  - Die Ergebnisse stehen in ns im OPTIONS-Menü unten (`NS LOOP … INT … ADD …` / `MUL … M4 … DIV … DEN …`).
  - Diese Zahlen entscheiden die nächsten Schritte, z. B. Festkomma, Denormals auf null setzen oder Divisionen vermeiden.
- **Master-Bus schläft bei Stille** (`engine/mixbus.c`):
  - Schläft, wenn Ein- und Ausgang eine halbe Sekunde unter −120 dBFS bleiben.
  - Die Zustände werden dann einmal auf null gesetzt, und der Bus gibt bis zum nächsten Signal exakte Stille aus.
  - Die Effekte schliefen schon vorher (`fx.c`).
- **Kleine Einsparungen ohne Klangänderung:**
  - `tape`/`tube` multiplizieren mit dem Kehrwert des Drives statt zu dividieren.
  - Der Limiter-Zeiger springt per Vergleich statt per Modulo zurück.
  - Die Demos weichen von vorher höchstens um Rundungsreste ab, im Mittel etwa −95 dB.
- **Mehr Heap:**
  - Der Puffer von `fm1/el_store.c` (2,6 KB) kommt nur noch während Laden und Sichern vom Heap.
  - Der CPU-Test läuft aus dem Flash.
  - Der Heap hat damit 83.820 B, 1.952 B mehr als der Build, der auf dem Gerät lief.

## Alternativen

- **Gleich auf Festkomma umbauen:** Das ist ein großer Umbau mit Klangrisiko. Erst sollen die Messwerte vom Gerät zeigen, welche Operation wirklich teuer ist.
- **Zweiter Kern** wie X0X/SLOOP: Das ist ein Folgeschritt. Er muss gegen das SDK von FM1 Open geprüft werden, das Bluetooth und Tasks auf beiden Kernen betreibt.
- **Weniger Stimmen:** Der Nutzer hat zugestimmt. Es kommt als A/B nach den Messwerten.

## Folgen

- Mit der Lastanzeige zeigt jeder Testbuild am Gerät selbst, wie viel Reserve er hat. Der Emulator ist für die Rechenlast nicht maßgeblich.
- **Speicher:**
  - Der Heap bleibt knapp. Jeder neue statische Puffer verkleinert ihn.
  - `tools/fm1-emu/check.sh` meldet jetzt einen SDK-Assert (`cpu_assert_debug`). So fällt es auf, wenn der Start an zu wenig Heap scheitert.
- **Nächster Schritt:** die Messwerte vom Gerät, dann gezielt die teuren Operationen ersetzen, die MKS-50-Chop vereinfachen und die Stimmenzahl als A/B anbieten.

## Messung auf dem Gerät (2026-11-05)

CPU-Test, ns pro Schritt, inklusive des Schleifensprungs:

| Messung | ns |
|---|---|
| LOOP | 21,9 |
| INT | 34,4 |
| ADD | 25,0 |
| MUL | 28,1 |
| M4 | 44,2 |
| DIV | 34,4 |
| DEN | 25,0 |

- **Kommazahl-Befehle sind schnell:** FADD etwa 3 ns, FMUL etwa 6 ns, FDIV etwa 12 ns.
- **Denormals kosten nichts extra.** Die Vermutung „FPU teuer“ ist damit widerlegt.
- **Teuer ist der genommene Sprung** (die leere Schleife): etwa 22 ns, rund 8 Takte. X0X misst dasselbe.

Die Original-Firmware V16 hat im Emulator aber noch mehr Sprünge als FM1-Electro und läuft trotzdem. Bei 175 % Last (Anzeige beim Spielen, 170–180 %) kostet ein Befehl von FM1-Electro auf dem Gerät etwa 4,8 Takte, einer von V16 höchstens 2,4.

**Folgeschritt:**
- **Zeit je Render-Abschnitt auf dem Gerät** (`el_engine_t.prof`, `prof_add` in `engine/electro.c`):
  - Abschnitte: Drums, RHYTHM, STRINGS, BASS, Effekte, Master, Punch-ins.
  - Im OPTIONS-Menü steht der Anteil eines Blocks in Prozent.
- **Weitere Messungen im CPU-Test:**
  - Schleife und FMUL aus dem SRAM;
  - Laden der Reihe nach (8 KB);
  - Laden mit abhängigen Sprüngen durch 64 KB (Latenz);
  - Funktionsaufruf.
- **Tabelle im Flash behoben:** Die Kalibriertabelle der Hats lag im Flash, weil LTO sie als nie beschrieben erkannte. Die Kalibriertabellen sind auf dem Gerät jetzt `const` (`TR808_CAL`) und liegen damit im SRAM.

## Kostenmodell und erste Umbauten (2026-11-05)

- **Kostenmodell im Emulator** (`tools/fm1-emu/hardware-checks.patch`, `MODEL`-Zeile): etwa 6,5 ns pro Befehl, je genommenen Sprung, je weiteres Wort bei Push/Pop und je FPU-Befehl noch einmal. Das ist die Messung von Felucca auf dem Gerät (fm1-emulator `docs/performance.md`); die Zahlen des CPU-Tests passen dazu.
- **Vorhersage:** 125 % für den Testbuild (das Gerät zeigt Spitzen von 166 %), 92 % für V16.
- **Der Takt ist nicht die Ursache:** Das Gerät läuft wie bei Felucca, SLOOP und X0X mit M-VAVEs 360 MHz.
- **Mess-Groove mit allen Bereichen und Master an:** 853.000 Slots pro Block bei einem Budget von 446.000.
- **Umbauten ohne neue Klangidee** (die Schalter bleiben zum Vergleich):
  - `MB_LITE_EQ` (Standard 1): Die EQs des Master-Busses sind einpolige Shelves bzw. Hochpässe statt 14 Biquads. Der BASS (mono) wird nur einmal gefiltert, die Präsenz bei 2,5 kHz entfällt.
  - `FX_REVERB_HALF` (Standard 1): Der Hall rechnet mit halber Rate. Längen, Modulation und Dämpfung sind umgerechnet, der Ausgang wird interpoliert.
  - MKS-50: Die beiden häufigen Chop-Formen sind inline, die anderen bleiben in `chop_general`.
  - Ergebnis: 853.000 → 777.000 Slots (−9 %). Weniger Stimmen (RHYTHM 2, STRINGS 2) brachten nur weitere −9 % und bleiben vorerst aus.
- **Zweiter Kern:** Das SDK von FM1 Open hält seine Task-Funktionen (`os_task_create`, `thread_fork`) intern. Eigene Arbeit auf dem zweiten Kern ist so nicht möglich.

## Verlauf

- 2026-11-05: angelegt nach dem ersten Gerätetest mit laufendem Groove (6 s statt 2 s pro Pattern)
- 2026-11-05: Messwerte vom Gerät: FPU schnell, Last 170–180 %; Zeitmessung je Render-Abschnitt und weitere CPU-Tests
- 2026-11-05: Kostenmodell, `MB_LITE_EQ`, `FX_REVERB_HALF`, Chop inline: 853.000 → 777.000 Slots im Mess-Groove; zweiter Kern im SDK nicht nutzbar
- 2026-11-05: Zweite Messung: Drums 59 %, Effekte 21 %, RHYTHM 14 %, BASS 4 % (STRINGS stumm, Master aus). Die Schleife aus dem SRAM braucht 10,7 ns, mit verketteter FMUL 28,1 ns. Eine abhängige FMUL kostet also ~17 ns (~6 Takte): Die erste Deutung („FPU schnell“) war falsch, die Flash-Schleife hatte die Latenz verdeckt. Speicherzugriffe sind nicht die Ursache: V16 macht im Emulator fünfmal so viele. Nächster Messbuild: Ganzzahl- und 64-Bit-Multiplikation aus dem SRAM, als Entscheidung für Festkomma (zuerst die Drums).
- 2026-11-06: Hall-Wurf (Dattorro-Tank) billiger ohne Klangänderung: Abgriffe mit vorberechneter Lage statt Gleitkomma pro Schritt, Hilfsfunktionen eingebettet, Modulo-Divisionen in `hole_get` und `delay_read` durch Subtraktion ersetzt (Emulator: Zuschlag beim Wurf ~18 → ~8 Punkte)
