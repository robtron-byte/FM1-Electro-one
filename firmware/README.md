# FM1-Electro: fertige Firmware zum Ausprobieren

## Neu: `FM1_ELECTRO_PLATTFORM.fwsc` (eigene Plattform, 07.11.2026, 22. Build)

Build auf der eigenen Plattform ohne SDK von FM1 Open (ADR 0021): zwei Kerne, Audio-Code im SRAM, STRINGS mit 3 Stimmen. Neu im 20.–22. Build:
- **Varianz und Wahrscheinlichkeit pro Step** (ADR 0025): Step-Taste halten: **PRESETS** stellt die Varianz ein (`1:1`, `1:2`, `2:2`, `1:4` … `4:4`), **ALGORITHM** die Wahrscheinlichkeit (100 % … 10 %, links seltener); beides kombinierbar. Nach einer Viertelsekunde zeigt das Display groß den Wert des zuletzt gedrehten Reglers.

Aus dem 18./19. Build:
- **Master sparsamer** (19. Build, bitgleich): Zustände während der Schleife in lokalen Variablen. Emulator mit DARK STR: PEAK 70 → 63 %.
- **SAVE ohne Aussetzer:** Der Speicherplatz für das nächste Speichern wird gelöscht, während der Groove kurz stillsteht (1,5 s nach dem Einschalten bzw. nach STOP). SAVE während des Spielens schreibt dann nur noch und unterbricht den Ton nicht mehr. Nur ein zweites SAVE ohne STOP dazwischen hat wieder die kurze Lücke.
- **Display flüssiger:** Das Display wird mit 20 statt 12 MHz angesteuert (ein ganzes Bild ~46 statt ~77 ms).

Aus dem 17. Build (alles bitgleicher Klang):
- **Synths sparsamer:** Die Rechenfunktionen (exp2, log2, Wurzel) riefen bei jeder Umwandlung ein langsames Kopierprogramm im Flash auf; die Hüllkurven-Zeiten werden pro Stimme gemerkt. Emulator: DARK STR STRINGS 45 → 39 %, Kern 1 81 → 68 %; JX STRINGS 38 → 32 %. Master, RHYTHM und BASS profitieren auch.
- **Hall, Delay, Gated Reverb sparsamer:** jeder Effekt in eigener Schleife, ohne Funktionsaufrufe pro Sample. Emulator: FX 18 → 15 %.
- Kitwechsel rechnet das Werks-Kit nicht mehr im Audio-Interrupt aus.

Aus dem 16. Build:
- **DARK STR, DARK DRONE, MACHINE PD** rechnen deutlich weniger (gleicher Klang, bitgleich): Ihre Saw-Form 5 lief über den langsamen allgemeinen Weg der Oszillatoren. Emulator: STRINGS 54–61 % → 45 %, Kern 1 89 % → 81 %. Alle anderen STRINGS-Sounds lagen schon bei 32–44 %.

Aus dem 15. Build:
- **Step-Varianz** (ADR 0025): Step-Taste halten (BEAT, oder ein Synth-Bereich im STEP-Modus) und **PRESETS** drehen: `1:2` spielt den Step nur im ersten von je zwei Durchläufen des Patterns, `2:2` im zweiten, `1:4` bis `4:4` in einem von vier, `1:1` immer. Ein kleiner weißer Strich markiert solche Steps. Ein klingender Step geht beim Loslassen nur aus, wenn kein Regler gedreht wurde. Variationen behalten die Varianz, ein neues Pattern beginnt ohne.
- **Tape-Stop** jetzt standardmäßig über eine Viertel.
- **Regler mit vielen Werten** (Filter, Pegel, die meisten Klangregler) werden schneller, je schneller man dreht; langsam bleibt es ein Wert pro Raste. Regler mit bis zu 16 Werten bleiben wie bisher.

Aus dem 14. Build:
- **Verzerrung entlastet** (13. Build auf dem Gerät: FX 34 %, PEAK 104 %, LATE BLOCKS 140): Sie rechnet jetzt auf dem zweiten Kern vor den Synths, parallel zu den Drums; Hall, Delay und Master bleiben auf dem ersten. Jeder Block wählt die Aufteilung, die am schnellsten fertig ist. Die Verzerrung selbst rechnet ein Drittel weniger. Bei Steps (jede 16tel) laufen Effekte und Master im längeren Teil des Blocks. Klang bitgleich. Emulator: mit Verzerrung PEAK 96 → 83 %, ohne 83 → 74 %.

Aus dem 13. Build:
- **Jeder Pattern-Platz behält seinen Sound** (ADR 0024): in BEAT Kit und alle Drum-Regler, in RHYTHM, STRINGS und BASS der Synth-Sound. Von A nach B und zurück ist A wieder so eingestellt wie vorher; die Drum-Einstellungen setzen sich beim Platzwechsel nicht mehr zurück. Was du einstellst, gilt für den Platz, der gerade spielt. Drum-Tonart und FOLLOW gelten weiter für den ganzen Bereich. Der gespeicherte Stand wird übernommen: Jeder Platz startet mit dem bisherigen Sound.
- Last unverändert (Emulator mit strengem Taktmodell: PEAK 84 %, MAX 89 %, LATE BLOCKS 0).

Aus dem 12. Build:
- **PO-12-Kit** rechnet viel weniger (die Werte, die sich während eines Schlags nicht ändern, einmal pro Schlag statt pro Sample; Klang gleich).
- **STRINGS:** Saw-Formen 2–4 über einen schnellen Weg (gleiche Welle).
- **Becken, Open Hat und Closed Hat** sind wieder unsere kalibrierte 808, jetzt in Festkomma nachgebaut (gleicher Klang wie das Gleitkomma-Modell, ADR 0022). Die Closed Hat ist an vier Aufnahmen einer echten 808 nachkalibriert (Variante B, ADR 0014).
- **Die ganze 808** ist wieder unser kalibriertes Modell, in Festkomma nachgebaut: BD, SD, Toms/Congas, Rim/Claves, Clap/Maracas, Cowbell, Becken und Hats (ADR 0022).
- **OPTIONS** (HOME + SAVE) zeigt bei laufendem Groove unten: `PEAK` (langsamster Audio-Block der letzten halben Sekunde, % seiner Zeit), `MAX` (seit dem Start), `C1` (langsamster Block auf Kern 1), `1CORE` falls Kern 1 ausgefallen ist, und `LATE BLOCKS` (zu spät gerechnete Blöcke seit dem Start = Knackser).
- Bereichstasten gedimmt, OCT blinkt am Ende der Oktave (ADR 0020).
- **Kein Knacksen mehr beim Verlassen von OPTIONS:** gespeichert wird nur, was sich geändert hat, und während der Groove läuft erst bei STOP (das Löschen des Flash hält den Ton ~50 ms je Sektor an, ADR 0023). SAVE speichert weiter sofort.
- **Hall-Wurf** (schwarze Taste 6) rechnet sparsamer, gleicher Klang.
- **Verzerrung** (HOME + EDIT, KNOB 1–4, auch in den JP-6-Sounds) rechnet jetzt in Festkomma, Abweichung zur bisherigen unter −76 dB.
- **Sequencer-Balken** läuft gerade: die Position wird für ein ganzes Bild festgehalten.
- **Nur Drums, dichtes Kit:** Effekte und Master wandern dann auf den zweiten Kern (der sonst die Synths rechnet). ZAP und PULSE rechnen in Festkomma.

**Last:** Gegenüber dem 7. Build (Gerät: DR49, PEAK 100 %, MAX 103 %, LATE BLOCKS 11) rechnen die Drums sparsamer, bei gleichem Klang: Mischung blockweise, Koeffizienten nur bei Reglerbewegung, Oszillatoren springen über die Strecken ohne Kante, DRIVE über eine Tabelle. Emulator: Kern 0 mit Groove 91 % → 82 %. Bitte wieder die OPTIONS-Zeilen fotografieren (HOME + SAVE, Groove läuft).

Installieren mit M-Flasher (FLASH FIRMWARE → Datei → REVIEW & FLASH; findet er das Gerät nicht: OCT− und OCT+ 5 s halten). Rettung: OCT− beim Einschalten. Geprüft: `make test`, Hardware-Prüfung im Emulator (`tools/fm1-emu/check.sh`). Auf dem Gerät noch nicht gelaufen.

SHA-256: `bd81721b0b33b109ced29d3c5b489b3f2f46a1eb7a356bd8aec356eca6e2dfc2`

## Älterer SDK-Build `FM1_ELECTRO.fwsc`

`FM1_ELECTRO.fwsc` ist ein Testbuild vom Stand 05.11.2026, im selben Commit wie diese Zeile.

**Neu in diesem Build (Messbuild, ADR 0019):** Der Build vom 04.11. startet, ist aber beim Spielen etwa dreifach überlastet (Sequencer zu langsam und stumm). Dieser Build behebt das noch nicht vollständig. Er zeigt, wie viel Rechenzeit das Gerät wirklich hat:
- **Oben im Header** steht die Last in Prozent, rechts neben dem Tempo: grün unter 70 %, gelb ab 70 %, rot ab 88 % oder wenn Audio-Blöcke ausfallen.
- **Im OPTIONS-Menü** (HOME + SAVE) stehen unten zwei Zeilen mit dem CPU-Test vom Start, in Nanosekunden pro Operation: `NS LOOP … INT … ADD …` und `MUL … M4 … DIV … DEN …`. Bitte ein Foto davon schicken.
- Im Stillstand pausiert die Master-Bearbeitung jetzt; die Bedienung sollte ohne laufenden Groove flüssig sein.
- **Seit dem zweiten Messbuild:** Während ein Groove läuft, zeigt die erste der beiden Zeilen, wie viel Prozent eines Audio-Blocks jeder Teil braucht: `DR` Drums, `RH` RHYTHM, `ST` STRINGS, `BA` BASS, `FX` Effekte, `MB` Master, `PU` Punch-ins. Die zweite Zeile zeigt weitere Messwerte (`R` Schleife aus dem RAM, `RM` Multiplikation aus dem RAM, `LD`/`RND` Speicher, `C` Funktionsaufruf).
- **Seit dem dritten Messbuild:** eine dritte Zeile unten (statt „HOME OR SAVE: DONE“) misst Ganzzahl-Addition `IA`, Ganzzahl-Multiplikation `IM`, 64-Bit-Multiplikation `M64`, Kommazahl-Addition `FA` und vier unabhängige Ganzzahl-Multiplikationen `IM4`, alle aus dem RAM: entscheidet, ob Festkomma schneller ist.
- Alle Funktionstasten leuchten jetzt; aktive Zustände blinken (PLAY im Takt, REC schnell, ARP/SEQ/EDIT langsam, ADR 0020).

**Behoben gegenüber dem Build vom 29.10.:** Der erste Build startete auf dem echten FM-1 in einer Schleife neu (Display blinkte etwa im Sekundentakt, oben ein Streifen der Oberfläche). Ursache: eine Division durch null im Master-Limiter bei Stille, die die CPU mit dem eingeschalteten Divisions-Trap des SDK als Ausnahme behandelt und das Gerät neu startet. Der Trap ist jetzt aus, und die Division kann nicht mehr null werden (ADR 0018).

Er enthält den neuen Sound: Clap, Cowbell und Hats nach den Vergleichen (ADR 0008, 0012, 0014), Metall-Oszillatoren mit Drift, Gated Reverb neu (0013, Clap trocken), längere Snare (0015), Zap nach deiner Referenz (0017) und die Drum-Tonart (0016).

- **Gebaut** mit der JieLi-Toolchain für Linux, nicht mit der offiziellen Windows-Toolchain 2.5.3.
- **Gepackt** mit `tools/package_fm1.py`, Benutzerbereich 384 KB.
- **Geprüft:** Die Prüfungen aus `Build.ps1` bestehen. Im Emulator fm1-emu startet die Firmware: Display, Tasten und PLAY laufen, die Audio-Puffer füllen sich.
- **Hardware-Prüfung im Emulator** (`tools/fm1-emu/check.sh`, ADR 0018): kein Divisions-Trap, keine Division durch null, kein NaN/Inf, keine unausgerichteten Zugriffe, Stacks in ihren Fenstern, Watchdog bedient, auch mit zufälligem RAM und Flash-Inhalt beim Start. Der alte Build fällt bei dieser Prüfung durch.
- **Auf einem echten FM-1:** Der Build vom 04.11. startet und spielt Synths über die Tastatur. Mit Groove ist er etwa dreifach überlastet (ADR 0019). Dieser Messbuild ist noch nicht auf dem Gerät gelaufen.

SHA-256: `aab77dd83d2e42e6bff2756491e382ded9d3b619474113f67981ae4540efcf34`

Geprüft vor dem Packen: `make test` (alle Host-Tests), die Prüfungen aus `Build.ps1` (Pflicht-Symbole, Audio-Pfad im SRAM, 80 KB Heap), Start im Emulator fm1-emu (Display, PLAY, Audio im DMA-Puffer, Benutzerbereich 384 KB erkannt). Rechenlast im Emulator: etwa 15 % im Leerlauf, 55 % bei der Prüffolge mit laufendem Groove.

## Flashen

1. **Vorher sichern:** Projekte und Samples von FM1 Open bzw. der Original-Firmware gehen beim Wechsel verloren. Auch Bluetooth-Kopplungen können zurückgesetzt werden.
2. Die Datei mit einem Flasher übertragen, der eigene `.fwsc`-Dateien annimmt: fm-static (https://seajaysec.github.io/fm-static/, im Browser), M-Flasher oder `tools/fm1_install.py`. Die FM1-Workbench nimmt nur ihre eingebauten Pakete.
3. **Zurück:** Die Original-Firmware lässt sich wieder aufspielen (Workbench oder die offizielle Datei mit einem der Flasher). Startet das Gerät gar nicht mehr, hilft der Download-Modus des Bootloaders (M-Flasher `download-mode`) oder der FM-1 Transporter mit einem RP2040.

## Beim ersten Test darauf achten

- Startet das Gerät, und erscheint die FM1-Electro-Oberfläche?
- Kommt Ton, wenn du PLAY drückst?
- Regelt der MASTER-Regler die Lautstärke?
- Bleiben die Grooves nach SAVE und einem Neustart erhalten?

Selbst bauen: siehe „Firmware bauen“ im Haupt-README.
