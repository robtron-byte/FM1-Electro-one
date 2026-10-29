# 0006: 808: eigenes kalibriertes Modell, fremde Ideen nur als eigene Umsetzung

- **Status:** angenommen
- **Stand:** 2026-10-29

## Kontext

Die 808 in `engine/tr808.c` ist ein eigenes Modell nach den Service Notes und Werner, an Aufnahmen echter TR-808 kalibriert (Snare, Becken, Metall, Bassdrum, siehe `docs/TR808.md`). Andere FM-1-Firmwares haben eigene 808-Engines, z. B. X0X-RMX (Port von 8W8, GPL-3.0-only). FM1-Electro steht unter GPL-3.0-only **mit** Linking-Exception für das SDK; fremder GPL-Code ohne diese Ausnahme würde das verkomplizieren.

## Entscheidung

- Unser Modell bleibt die Grundlage.
- Ideen aus anderen Engines werden **selbst umgesetzt**, nicht kopiert, und im Code mit Herkunft der Idee vermerkt.
- Jede Klangänderung kommt zuerst als **A/B-Schalter** (Präprozessor, Standard aus) mit einem `make …-ab`-Hörvergleich. Erst nach der Wahl wird der Standard umgestellt und der Record angenommen.
- Fremder Code wird in dieser Umgebung nicht ausgeführt, nur gelesen.

## Folgen

- Kalibrierte Teile werden nicht ungefragt verändert.
- Offene Vergleiche: 0008 (Clap), 0009 (Metall), früher 0010 (Bassdrum).

## Verlauf

- 2026-10-29: angelegt nach dem Vergleich mit X0X-RMX
