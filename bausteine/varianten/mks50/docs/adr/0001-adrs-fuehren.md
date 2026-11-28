# 0001: Entscheidungen als ADRs führen

- **Status:** angenommen
- **Stand:** 2026-11-10

## Kontext

Der Auftrag (`AUFTRAG-MKS50.md`) verlangt Dokumentation und Entscheidungen auf Deutsch. FM1-Electro führt seine Entscheidungen als ADRs und hat damit über viele Sitzungen den Faden gehalten.

## Entscheidung

- Jede grundsätzliche Entscheidung (Klang, Architektur, Speicher, Bedienung, Build) bekommt einen Record in `docs/adr/` nach `TEMPLATE.md` und eine Zeile in `docs/adr/README.md`.
- Ändert sich eine Entscheidung, werden Status, Stand und „Verlauf“ im selben Commit nachgeführt.
- Was der Nutzer noch nicht gehört oder gemessen hat, bleibt *vorgeschlagen*.
- Klangänderungen kommen zuerst als Schalter mit Standard „aus“ und einem A/B-Rendering zum Hören.

## Alternativen

- Nur Commit-Nachrichten: Der Grund einer Entscheidung ist später schwer zu finden.

## Folgen

- Eine neue Sitzung liest `CLAUDE.md`, dann `docs/adr/README.md`.

## Verlauf

- 2026-11-10: angelegt
