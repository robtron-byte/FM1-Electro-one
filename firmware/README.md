# FM1-Electro: fertige Firmware zum Ausprobieren

`FM1_ELECTRO.fwsc` ist ein Testbuild von Commit `f3098ce`.

- **Gebaut** mit der JieLi-Toolchain für Linux, nicht mit der offiziellen Windows-Toolchain 2.5.3.
- **Gepackt** mit `tools/package_fm1.py`, Benutzerbereich 384 KB.
- **Geprüft:** Die Prüfungen aus `Build.ps1` bestehen. Im Emulator fm1-emu startet die Firmware: Display, Tasten und PLAY laufen, die Audio-Puffer füllen sich.
- **Noch nie auf einem echten FM-1 gelaufen.**

SHA-256: `23129666698e60d01c3581794c51e0516ccfc5ee5bab327e6fc61bfbac3251e8`

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
