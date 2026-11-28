# Hinweise für Claude-Sitzungen

- **ADRs pflegen:** Jede grundsätzliche Entscheidung (Klang, Architektur, Speicher, Bedienung, Build) steht in `docs/adr/`. Neue Entscheidung → neuer Record nach `docs/adr/TEMPLATE.md` und Zeile in `docs/adr/README.md`. Geänderte Entscheidung → Status, Stand und „Verlauf“ des Records im **selben Commit** nachführen. Offene A/B-Vergleiche sind *vorgeschlagen*, bis der Nutzer gewählt hat. Siehe `docs/adr/0001-adrs-fuehren.md`.
- **Klangänderungen** zuerst als Präprozessor-Schalter (Standard aus) mit `make …-ab` zum Hören (`docs/adr/0006`).
- **Code aus anderen Projekten** (fm1-synths, REA-Ordner, FM-1-Firmwares …) darf übernommen werden; Lizenzfragen klärt der Nutzer (Wunsch 28.11.2026). Klangänderungen trotzdem erst als Schalter (`docs/adr/0006`).
- Dokumentation und Records auf Deutsch.
- Nach Änderungen am Gerätecode: `make test`; wenn Standards umgestellt werden, den Testbuild in `firmware/` neu packen (`tools/package_fm1.py`, siehe `docs/adr/0003`).
- **Versionen (`docs/adr/0035`):** Datei `VERSION` (X.Y.Z-alpha, eine Ziffer je Stelle). Jeder neue Plattform-Testbuild erhöht Z; Datei `firmware/FM1_ELECTRO-<Version>.fwsc`, die alte entfernen. Tag und Release legt `.github/workflows/release.yml` beim Push an (vom Nutzer gewünscht, 14.11.2026).
- **Vor jedem Testbuild in `firmware/`** zusätzlich `tools/fm1-emu/check.sh` (Hardware-Prüfung im Emulator, `docs/adr/0018`). Keine Float-Division, deren Nenner 0 werden kann: der Compiler rechnet bewachte Divisionen vor ihrem Test aus.
- **Bedienfreundlichkeit (Wunsch des Nutzers):** Bei jeder Funktion auf eine logische, einheitliche Bedienung achten: links = weniger/leiser/kürzer/zurück, rechts = mehr/lauter/länger/weiter; ein Regler startet am aktuellen Zustand (kein Sprung ans Ende); kurze Klicks für Häufiges, Kombinationen nur für Seltenes; was gerade passiert, ist im Display sichtbar. Feature-Liste `docs/FEATURES.md` mitpflegen.
