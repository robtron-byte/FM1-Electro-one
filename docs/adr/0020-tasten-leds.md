# 0020: Funktionstasten leuchten, aktive Zustände blinken, inaktive Bereiche gedimmt

- **Status:** vorgeschlagen
- **Stand:** 2026-11-06

## Kontext

Beim ersten Gerätetest waren die Funktionstasten fast immer dunkel. Bisher leuchtete eine Taste nur, wenn ihr Zustand aktiv war (PLAY läuft, REC, ARP, SEQ, EDIT, Bereich). Im Dunkeln waren die Tasten damit kaum zu finden, und der Nutzer fand die Bedienung zu schwer.

Unter dem SDK von FM1 Open kennen die LEDs nur an und aus (`fm1_led_set`). Die eigene Plattform (ADR 0021) kann dimmen: Ihre HAL hat eine Hintergrund-Ebene (`fm1_led_bg`), die eine LED in jedem Scan-Durchlauf nur kurz einschaltet.

## Entscheidung

`update_leds()` in `ui/el_ui.c`:

- **Alle 14 Funktionstasten leuchten dauerhaft.**
- **Ein aktiver Zustand blinkt**, das heißt die LED geht kurz aus:
  - PLAY, solange der Groove läuft, im Takt: an in der ersten Hälfte jeder Viertel.
  - REC während einer Aufnahme schnell (125 ms).
  - ARP (Arpeggiator an oder Generator-Taste gehalten), SEQ (Step-Modus bzw. zweite Beat-Ebene) und EDIT (Editor offen) langsam (400 ms).
  - OCT− / OCT+ langsam für die angezeigte Step-Seite bei Patterns über 16 Steps.
  - OCT− bzw. OCT+ langsam, wenn die Tastatur-Oktave eines Synth-Bereichs am unteren (−3) bzw. oberen Ende (+2) steht, wie in der Original-Firmware von M-VAVE.
  - SAVE schnell, während ein Sound benannt wird.
- **Bereichstasten** (FX/SEL/ENV/LFO für die vier Bereiche) und HOME: Die gewählte leuchtet voll, die anderen gedimmt auf etwa die halbe Helligkeit (in HOME leuchtet HOME voll). Gedimmt heißt: ein Puls von 8 µs je Scan-Durchlauf statt rund 95 µs (`EL_LED_DIM_NS` in `fm1/el_app.c`). Das Auge sieht logarithmisch, darum wirkt etwa 1/12 der Zeit wie halb hell. Im SDK-Build gibt es kein Dimmen, dort leuchten diese Tasten voll.

## Alternativen

- **Nur aktive Tasten leuchten:** wie bisher und wie FM1 Open. Das ist zu dunkel.
- **Aktive Tasten aus, inaktive an:** Das ist ohne Erklärung missverständlich.
- **Inaktive Bereichstasten aus statt gedimmt:** Dann wären sie im Dunkeln wieder schwer zu finden.
- **Einstellung im OPTIONS-Menü:** Die Liste ist schon länger als das Display. Das kommt erst mit einem blätternden Menü.

## Folgen

- Der Host-Test (`host/ui_sim.c`) prüft, dass EDIT im Editor blinkt und außerhalb wie die anderen Funktionstasten leuchtet, dass nur die gewählte Bereichstaste voll leuchtet und dass OCT− am Ende der Oktave blinkt.
- Das Dimmen kostet Rechenzeit: Ist der Puls länger als das Weiterschalten der Spalte, wartet der Scan-Interrupt. Im Emulator steigt die Interrupt-Zeit auf Kern 0 dadurch von 25,9 auf 27,3 % (mit 1 µs Puls wäre es fast nichts). Wenn das stört oder die Helligkeit nicht passt: `EL_LED_DIM_NS` anpassen.
- Wenn dem Nutzer das Blinken zu unruhig ist: Rhythmen anpassen oder einzelne Zustände wieder als „aus“ zeigen.

## Verlauf

- 2026-11-05: angelegt auf Wunsch des Nutzers nach dem ersten Gerätetest
- 2026-11-06: auf Wunsch des Nutzers: inaktive Bereichstasten gedimmt (etwa halb), OCT blinkt am Ende der Oktave wie in der Original-Firmware
