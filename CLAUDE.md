# Hinweise für Claude-Sitzungen

- **ADRs pflegen:** Jede grundsätzliche Entscheidung (Klang, Architektur, Speicher, Bedienung, Build) steht in `docs/adr/`. Neue Entscheidung → neuer Record nach `docs/adr/TEMPLATE.md` und Zeile in `docs/adr/README.md`. Geänderte Entscheidung → Status, Stand und „Verlauf“ des Records im **selben Commit** nachführen. Offene A/B-Vergleiche sind *vorgeschlagen*, bis der Nutzer gewählt hat. Siehe `docs/adr/0001-adrs-fuehren.md`.
- **Klangänderungen** zuerst als Präprozessor-Schalter (Standard aus) mit `make …-ab` zum Hören (`docs/adr/0006`).
- **Fremder Code** anderer Firmwares: Solange FM1-Electro auf dem SDK von FM1 Open aufbaut (GPL-3.0-only mit Linking-Exception), nur lesen und Ideen selbst umsetzen. Für die eigene Grundlage ohne dieses SDK (`docs/adr/0021`) darf GPL-3.0-Code (SLOOP, X0X, Dinghy …) übernommen werden: mit Urheber, Herkunft (Repository, Commit) und Lizenz im Dateikopf und in `LICENSES`/README.
- Dokumentation und Records auf Deutsch.
- Nach Änderungen am Gerätecode: `make test`; wenn Standards umgestellt werden, den Testbuild in `firmware/` neu packen (`tools/package_fm1.py`, siehe `docs/adr/0003`).
- **Vor jedem Testbuild in `firmware/`** zusätzlich `tools/fm1-emu/check.sh` (Hardware-Prüfung im Emulator, `docs/adr/0018`). Keine Float-Division, deren Nenner 0 werden kann: der Compiler rechnet bewachte Divisionen vor ihrem Test aus.
