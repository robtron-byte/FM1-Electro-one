# 0020: Funktionstasten leuchten, aktive Zustände blinken

- **Status:** vorgeschlagen
- **Stand:** 2026-11-05

## Kontext

Beim ersten Gerätetest waren die Funktionstasten fast immer dunkel. Bisher leuchtete eine Taste nur, wenn ihr Zustand aktiv war (PLAY läuft, REC, ARP, SEQ, EDIT, Bereich). Im Dunkeln waren die Tasten damit kaum zu finden, und der Nutzer fand die Bedienung zu schwer.

Die LEDs des FM-1 kennen nur an und aus (`fm1_led_set`), eine Helligkeitsstufe gibt es nicht.

## Entscheidung

`update_leds()` in `ui/el_ui.c`:

- **Alle 14 Funktionstasten leuchten dauerhaft.**
- **Ein aktiver Zustand blinkt**, das heißt die LED geht kurz aus:
  - PLAY, solange der Groove läuft, im Takt: an in der ersten Hälfte jeder Viertel.
  - REC während einer Aufnahme schnell (125 ms).
  - ARP (Arpeggiator an oder Generator-Taste gehalten), SEQ (Step-Modus bzw. zweite Beat-Ebene) und EDIT (Editor offen) langsam (400 ms).
  - OCT− / OCT+ langsam für die angezeigte Step-Seite bei Patterns über 16 Steps.
  - SAVE schnell, während ein Sound benannt wird.
- **Bereichstasten** (FX/SEL/ENV/LFO für die vier Bereiche) und HOME leuchten einfach. Welcher Bereich gewählt ist, zeigt der Header des Displays.

## Alternativen

- **Nur aktive Tasten leuchten:** wie bisher und wie FM1 Open. Das ist zu dunkel.
- **Aktive Tasten aus, inaktive an:** Das ist ohne Erklärung missverständlich.
- **Einstellung im OPTIONS-Menü:** Die Liste ist schon länger als das Display. Das kommt erst mit einem blätternden Menü.

## Folgen

- Der Host-Test (`host/ui_sim.c`) prüft, dass EDIT im Editor blinkt und außerhalb wie die anderen Funktionstasten leuchtet.
- Wenn dem Nutzer das Blinken zu unruhig ist: Rhythmen anpassen oder einzelne Zustände wieder als „aus“ zeigen.

## Verlauf

- 2026-11-05: angelegt auf Wunsch des Nutzers nach dem ersten Gerätetest
