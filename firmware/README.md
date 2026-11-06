# FM1-Electro: fertige Firmware zum Ausprobieren

## Neu: `FM1_ELECTRO_PLATTFORM.fwsc` (eigene Plattform, 06.11.2026)

Build auf der eigenen Plattform ohne SDK von FM1 Open (ADR 0021): zwei Kerne, Audio-Code im SRAM, Drums in Festkomma (ADR 0022), STRINGS mit 3 Stimmen und dem MKS-50-Filter in halber Rate. Neu in diesem Build:
- Die Bereichstasten FX/SEL/ENV/LFO und HOME: Die gewählte leuchtet voll, die anderen etwa halb so hell (ADR 0020).
- OCT− / OCT+ blinken, wenn die Tastatur-Oktave am Ende ist.

Installieren mit M-Flasher (FLASH FIRMWARE → Datei → REVIEW & FLASH; findet er das Gerät nicht: OCT− und OCT+ 5 s halten). Rettung: OCT− beim Einschalten. Geprüft: `make test`, Hardware-Prüfung im Emulator (`tools/fm1-emu/check.sh`). Auf dem Gerät noch nicht gelaufen. Bitte ein Foto der OPTIONS-Zeile (HOME + SAVE) mit laufendem Groove schicken und sagen, ob die gedimmten Tasten passen.

SHA-256: `379ce8b78bd49010ae22defc7718c708a8e89e4ace4d4443350cf258ada9b655`

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
