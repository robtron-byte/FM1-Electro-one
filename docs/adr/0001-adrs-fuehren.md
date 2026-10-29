# 0001: Entscheidungen als ADRs festhalten

- **Status:** angenommen
- **Stand:** 2026-10-29

## Kontext

FM1-Electro wächst über viele Sitzungen. Gründe für Entscheidungen (warum BASS mono ist, warum ein eigener Packer, welche Clap-Variante) gehen sonst in Chatverläufen und Commit-Texten verloren.

## Entscheidung

- Jede grundsätzliche Entscheidung bekommt eine Datei `docs/adr/NNNN-titel.md` nach `TEMPLATE.md`, auf Deutsch.
- Ändert sich eine Entscheidung, wird der Record im **selben Commit** aktualisiert: Status, Stand und eine Zeile unter „Verlauf“. Ersetzte Entscheidungen bleiben stehen und verweisen auf den Nachfolger.
- Offene Hörvergleiche (A/B-Schalter) sind *vorgeschlagen*, bis die Wahl gefallen ist.
- `CLAUDE.md` im Repo-Wurzelverzeichnis verpflichtet jede Claude-Sitzung darauf.

## Folgen

- Kleiner Mehraufwand pro Entscheidung, dafür ist nachvollziehbar, was warum so ist.
- Die Tabelle in `README.md` ist der Einstieg und muss mitgepflegt werden.

## Verlauf

- 2026-10-29: angelegt
