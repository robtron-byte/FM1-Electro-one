# Hinweise für Claude-Sitzungen

- **ADRs pflegen:** Jede grundsätzliche Entscheidung (Klang, Architektur, Speicher, Bedienung, Build) steht in `docs/adr/`. Neue Entscheidung → neuer Record nach `docs/adr/TEMPLATE.md` und Zeile in `docs/adr/README.md`. Geänderte Entscheidung → Status, Stand und „Verlauf“ des Records im **selben Commit** nachführen. Offene A/B-Vergleiche sind *vorgeschlagen*, bis der Nutzer gewählt hat. Siehe `docs/adr/0001-adrs-fuehren.md`.
- **Klangänderungen** zuerst als Präprozessor-Schalter (Standard aus) mit `make …-ab` zum Hören (`docs/adr/0006`).
- **Fremden Code** anderer Firmwares nur lesen und Ideen selbst umsetzen (Lizenz: GPL-3.0-only mit Linking-Exception).
- Dokumentation und Records auf Deutsch.
- Nach Änderungen am Gerätecode: `make test`; wenn Standards umgestellt werden, den Testbuild in `firmware/` neu packen (`tools/package_fm1.py`, siehe `docs/adr/0003`).
