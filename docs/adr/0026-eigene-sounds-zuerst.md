# 0026: Eigene Sounds stehen vor den Werks-Sounds

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

PRESETS blätterte erst durch alle Werks-Sounds eines Bereichs (bis zu rund 74 MKS-50- und JP-6-Sounds) und danach durch die eigenen (U01–U32). Wer eigene Sounds nutzt, musste jedes Mal bis ans Ende drehen. Rückmeldung vom Nutzer: Die User-Liste soll so angeordnet sein, dass man schneller bei ihnen ist.

## Entscheidung

- **Reihenfolge:** erst die belegten eigenen Plätze (aufsteigend), dann die Werks-Sounds des Bereichs. Ein neuer Bereich startet mit dem ersten Werks-Sound; die eigenen Sounds liegen also einen Klick links davon (`next_sound` in `ui/el_ui.c`). Die Liste läuft nicht im Kreis.
- **Schnellspur:** GLO + PRESETS blättert nur durch die eigenen Sounds, im Kreis. Von einem Werks-Sound aus landet der erste Klick rechts auf dem ersten, links auf dem letzten eigenen Sound (`next_user_sound`).
- EDIT + PRESETS (andere Engine) bleibt unverändert.

## Alternativen

- **Liste im Kreis:** schnelles Drehen würde über die Enden hinweglaufen und an unerwarteten Stellen landen.
- **Eigene Sounds nur über eine eigene Taste:** alle Tasten der Synth-Bereiche sind belegt; GLO + PRESETS war frei.

## Folgen

- Wer bisher durch Rechtsdrehen am Ende zu den eigenen kam, dreht jetzt nach links oder nimmt GLO + PRESETS.
- Ohne eigene Sounds ändert sich nichts.

## Verlauf

- 2026-11-07: angelegt; `host/ui_sim.c` prüft Reihenfolge und Schnellspur.
