# FM1-Electro: fertige Firmware zum Ausprobieren

`FM1_ELECTRO.fwsc` ist ein Testbuild vom Stand 04.11.2026, im selben Commit wie diese Zeile.

**Behoben gegenüber dem Build vom 29.10.:** Der erste Build startete auf dem echten FM-1 in einer Schleife neu (Display blinkte etwa im Sekundentakt, oben ein Streifen der Oberfläche). Ursache: eine Division durch null im Master-Limiter bei Stille, die die CPU mit dem eingeschalteten Divisions-Trap des SDK als Ausnahme behandelt und das Gerät neu startet. Der Trap ist jetzt aus, und die Division kann nicht mehr null werden (ADR 0018).

Er enthält den neuen Sound: Clap, Cowbell und Hats nach den Vergleichen (ADR 0008, 0012, 0014), Metall-Oszillatoren mit Drift, Gated Reverb neu (0013, Clap trocken), längere Snare (0015), Zap nach deiner Referenz (0017) und die Drum-Tonart (0016).

- **Gebaut** mit der JieLi-Toolchain für Linux, nicht mit der offiziellen Windows-Toolchain 2.5.3.
- **Gepackt** mit `tools/package_fm1.py`, Benutzerbereich 384 KB.
- **Geprüft:** Die Prüfungen aus `Build.ps1` bestehen. Im Emulator fm1-emu startet die Firmware: Display, Tasten und PLAY laufen, die Audio-Puffer füllen sich.
- **Hardware-Prüfung im Emulator** (`tools/fm1-emu/check.sh`, ADR 0018): kein Divisions-Trap, keine Division durch null, kein NaN/Inf, keine unausgerichteten Zugriffe, Stacks in ihren Fenstern, Watchdog bedient, auch mit zufälligem RAM und Flash-Inhalt beim Start. Der alte Build fällt bei dieser Prüfung durch.
- **Auf einem echten FM-1** lief bisher nur der alte Build (Neustart-Schleife). Dieser hier ist der erste mit der Korrektur.

SHA-256: `0ae67f7199ab31747dcc603379ee54ab7d597f077f141da4e52160b50c9eb302`

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
