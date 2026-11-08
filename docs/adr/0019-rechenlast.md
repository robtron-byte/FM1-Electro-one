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
- 2026-11-06: Verzerrung (Send in HOME und JP-6) billiger: auf der eigenen Plattform die vier Kennlinien als Tabellen (±12, Schritt 1/128, linear interpoliert; gegen die Kurven OD −91 dB, DIST −85, TUBE −101, RAT −65 nur an der harten Kante; `DIST_TABLE`), die Ringpuffer des Halbband-Filters gespiegelt (bitgleich, ohne Maskieren). Emulator: Zuschlag mit voller Verzerrung ~26 → ~17 Punkte
- 2026-11-06: Gerät mit voll aufgedrehter Verzerrung: MAX 122 %, LATE BLOCKS 930 („nur noch Knacken“). Die Verzerrung auf der eigenen Plattform in Festkomma (`DIST_FIXED`): Kennlinien als Ganzzahl-Tabellen mit quadratischer Interpolation, Halbband-Filter mit 64-Bit-Summen, getriebenes Signal Q18 (bis ±8000), Pegel-Gleiten wie im Float-Code bis zum Ziel. `make dist-fixed-check`: alle Typen und Einstellungen −76 bis −95 dB neben der Float-Version
- 2026-11-06: Master (Mix-Bus) in Festkomma gebaut (`MB_FIXED`, `make mb-fixed-check`: −75 bis −97 dB neben Float), aber im strengen Taktmodell des neuen Emulators (fm1-live, `FM1_STRICT=1`; es trifft die MB-Last des Geräts, 14 gegen 15 %) teurer: MB 18 statt 14 %. Bleibt aus (`MB_FIXED 0`). Profil im strengen Modell (Groove läuft, erster Kern): Effekte 13 %, Master 11 %, Drums 25 % der Zeit; die UI-Schleife (~18 %) läuft außerhalb des Audio-Interrupts. Zweiter Kern: STRINGS-Stimmen (`render_voice`) 29 %
- 2026-11-06: Verzerrung billiger, bitgleich: Halbband-Filter mit festen Koeffizienten vollständig ausgerollt, im Hochtasten die Paare halbiert und summiert (ein Produkt je Paar). Emulator: Verzerrung 12 → 8,4 % eines Blocks. Auf dem Gerät kostete sie FX 18 → 34 %; der Rest der Entlastung kommt aus der Aufteilung auf beide Kerne (ADR 0021)
- 2026-11-06: Gerät (14. Build, Sound DARK STR): ST64, C1 93 %, PEAK 98 %, LATE BLOCKS 44. Alle 36 STRINGS-Sounds im strengen Emulator gemessen: ST 32–44 %, Ausreißer DARK DRONE 61, DARK STR 54, MACHINE PD 53, alle mit Saw-Form 5 (Rampe unter zwei Toren, 8- und 32-fach), die über `chop_general` (sieben Kantenfamilien je Sample) lief. Neuer schneller Weg `MKS_CHOP_GRID`: Liegen alle Kanten einer Form auf einem Raster (ganze Vielfache, Tore halb offen: Saw 5, Sub 1–3 und 5), prüft er nur den nächsten Rasterpunkt. `make chop-grid-check`: alle 56 MKS-50-Sounds bitgleich. Emulator: die drei auf ST45 (C1 81–83 %)
- 2026-11-06: Gerät (16. Build, MIAMI, Platz B): DR29 RH10 ST20 BA19 FX25 MB15, PEAK 79 %, C1 63 %, aber MAX 105 %, LATE BLOCKS 31. 17. Build, bitgleich: `dsp_math.h` mit `__builtin_memcpy` (die Engine baut mit `-fno-builtin`; jedes exp2/log2/sqrt rief das `memcpy` im Flash), MKS-50-Hüllkurvenzeiten pro Stimme gemerkt, `chop_enabled` inline; Effekte je eine Schleife mit Zuständen in lokalen Variablen, `soft`/`delay_read` inline; Werks-Kits vorab berechnet. Emulator: STRINGS (DARK STR) 45 → 39 %, C1 81 → 68 %, FX 18 → 15 %. Spitzen im Emulator beim Platz-, Sound- und Kitwechsel 66–80 %, ohne späte Blöcke; die Spitzen des Geräts noch ungeklärt (SAVE löscht den Flash und erzeugt sie absichtlich, ADR 0023)
- 2026-11-07: Master (`mb_process`, MASTER FX an) mit Zuständen in lokalen Variablen, ohne An/Aus-Tests pro Sample; 70 Demo-Renders bytegleich. Emulator (DARK STR): MB 12 → 11 %, PEAK 70 → 63 %
- 2026-11-07: JP-6 `vco_tick`, `vcf`, `vcf_set` immer eingebettet (vorher ein Aufruf pro Sample und Stimme), `key_update` ohne Bibliotheks-`memcmp`/`memcpy`; 144 Renders bytegleich. Emulator: JP6 STRNG ST 40 → 35 %
- 2026-11-07: Hall-Modulation nur alle n Frames (`FX_REVERB_LFO_EVERY`, `make hall-ab`: −52 dB bei 4, −49 dB bei 8 gegen bisher): im strengen Emulator kein messbarer Gewinn (FX gleich, auch mit aufgedrehtem Hall). Bleibt aus (1)
- 2026-11-07: Takt: Das JieLi-SDK nennt für den AC791N 320 MHz als Standard (`cpu/wl82/tools/isd_config_rule.c`, SYS_CLK=320MHz; Übertakten 396 MHz). Unsere Plattform setzt den Takt nicht selbst, den lässt der Bootloader so, wie er ihn einstellt; die alte Angabe „360 MHz“ stammt aus dem SDK-Build. 29. Build zeigt den Takt in OPTIONS (Register wie X0X), um ihn auf dem Gerät abzulesen, bevor er geändert wird
- 2026-11-07: Gerät meldet Taktquelle 2 (`PLL24 S2=0`), die X0X-Auswertung (Quelle 6) passt nicht. 33. Build misst stattdessen eine Kette abhängiger Additionen gegen den 24-MHz-Zähler (`clock_mhz` in `fm1/el_glue.c`, 64 minus 32 je Runde): strenger Emulator (240-MHz-Modell, zwei Additionen je Takt) zeigt 490/µs
- 2026-11-07: Gerät (33. Build): `ADDS 335/US`. Abhängige Additionen laufen auf dem Gerät eine je Takt (das Emulatormodell rechnet 0,6 Takte, also zwei je Takt), daher etwa 335 MHz, mit den Unterbrechungen durch den Tasten-Timer eher etwas mehr: Das Gerät läuft schon mit 320–360 MHz (wie die alte Angabe 360 MHz). Eine Umstellung auf 320 MHz bringt nichts (Feature P6 gestrichen)
- 2026-11-07: Gerät (34. Build): `ADD 336 IND 348/US`. Vier unabhängige Ketten sind kaum schneller als eine abhängige: Der Kern macht eine Addition je Takt (keine zwei wie im Emulatormodell). Damit läuft das Gerät mit etwa 340 MHz; eine Takt-Umstellung bringt nichts (P6 bleibt gestrichen). Mehr Leistung nur über sparsameres Rechnen
- 2026-11-07: Zwei Klang-Schalter zum Hören (Standard aus, `make chorus-ab`): `MKS_CHORUS_HALF` (Chorus der MKS-50/JP-6-Sounds mit halber Rate) und `MKS_RELEASE_HURRY` (ein weit abgeklungenes Release endet 8-mal schneller, bei vollen Akkorden schon ab −30 dB, sonst ab −46 dB). Emulator, gleicher Groove (fester Startwert `EL_FIXED_SEED`, Schalter per `EL_EXTRA_CFLAGS` an `tools/build_platform.py`): Kern 1 41,7 → 39,0 % (Chorus 3,3 → 1,9, Stimmen 22,2 → 21,4)
- 2026-11-07: Nutzer hört in B (STRINGS solo) Knackser, die in A nicht da sind: beide Schalter bleiben aus (P9 gestrichen). Klang geht vor Last
- 2026-11-07: Nutzer: c (`MKS_RELEASE_HURRY`) bleibt drin, Standard jetzt 1; der Chorus mit halber Rate (`MKS_CHORUS_HALF`) bleibt aus
- 2026-11-08: Nutzer: Debug-Anzeigen entfernen, solange nicht gebraucht. Schalter `EL_DEBUG_PANEL` (Standard 0, `fm1/el_glue.c`): keine Last in Kopfzeile und OPTIONS, kein Takttest beim Start. Die Engine misst ihre Stufen weiter (für die Verteilung auf zwei Kerne). Für Messungen: `EL_EXTRA_CFLAGS=-DEL_DEBUG_PANEL=1`
