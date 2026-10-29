# FM1-Electro: fertige Firmware zum Ausprobieren

`FM1_ELECTRO.fwsc` ist ein Testbuild vom Stand 29.10.2026, im selben Commit wie diese Zeile. Er enthält den neuen Sound: Clap, Cowbell und Hats nach den Vergleichen (ADR 0008, 0012, 0014), Metall-Oszillatoren mit Drift, Gated Reverb neu (0013, Clap trocken), längere Snare (0015), Zap nach deiner Referenz (0017) und die Drum-Tonart (0016).

- **Gebaut** mit der JieLi-Toolchain für Linux, nicht mit der offiziellen Windows-Toolchain 2.5.3.
- **Gepackt** mit `tools/package_fm1.py`, Benutzerbereich 384 KB.
- **Geprüft:** Die Prüfungen aus `Build.ps1` bestehen. Im Emulator fm1-emu startet die Firmware: Display, Tasten und PLAY laufen, die Audio-Puffer füllen sich.
- **Noch nie auf einem echten FM-1 gelaufen.**

SHA-256: `fe5b6ba920e5f01357e51eeec2b9d4c0cf9f6b7aeab5a09cd3927b650ebe9bd5`

Geprüft vor dem Packen: `make test` (alle Host-Tests), die Prüfungen aus `Build.ps1` (Pflicht-Symbole, Audio-Pfad im SRAM, 80 KB Heap), Start im Emulator fm1-emu (Display, PLAY, Audio im DMA-Puffer, Benutzerbereich 384 KB erkannt). Rechenlast im Emulator beim Spielen ~80 % im Mittel.

## Flashen

1. **Vorher sichern:** Projekte und Samples von FM1 Open bzw. der Original-Firmware gehen beim Wechsel verloren. Auch Bluetooth-Kopplungen können zurückgesetzt werden.
2. Die Datei mit der FM1-Workbench oder dem offiziellen Updater auf das FM-1 übertragen.
3. **Zurück:** Die Original-Firmware V15 lässt sich jederzeit über die Workbench wieder aufspielen.

## Beim ersten Test darauf achten

- Startet das Gerät, und erscheint die FM1-Electro-Oberfläche?
- Kommt Ton, wenn du PLAY drückst?
- Regelt der MASTER-Regler die Lautstärke?
- Bleiben die Grooves nach SAVE und einem Neustart erhalten?

Selbst bauen: siehe „Firmware bauen“ im Haupt-README.
