# 0036: Externe MIDI-Clock über ein Schwungrad (α-β-Filter) statt Step auf jedem sechsten Tick

- **Status:** vorgeschlagen
- **Stand:** 2026-11-20

## Kontext

Fehlermeldung des Nutzers (20.11.2026): Mit externer MIDI-Clock läuft der Groove unruhig, und das Tempo schwankt dauernd um 2–3 BPM.

Ursache: Die Clock-Bytes (F8) kommen über die Hauptschleife (`fm1/el_app.c`, `midi_in`) in die Warteschlange der Engine. Die Engine liest diese Schlange nur am Anfang eines Audioblocks (256 Frames ≈ 5,8 ms). Bisher feuerte jeder sechste Tick sofort einen Step, also immer auf einem Blockanfang. Das Tempo kam aus dem Abstand der letzten beiden Steps (`since_step`). Schon bei einer perfekten Clock mit 120 BPM dauert ein Step (5512,5 Frames) mal 21 und mal 22 Blöcke. Das Tempo sprang dadurch zwischen 117,5 und 123,0 BPM, und die Steps wackelten um ±5,8 ms. Dazu kommt die Verzögerung der Hauptschleife. Das schwankende Tempo verstellte auch Delay-Zeiten, Gates und Rolls.

## Entscheidung

`engine/transport.c` folgt der MIDI-Clock mit einem Schwungrad:

- **α-β-Filter über die Tick-Zeiten:** Periode und Phase werden aus vielen Ticks geschätzt. Bis zum ersten Tick gilt die bekannte Periode. Danach rechnet das Filter mit wachsendem Gedächtnis: jeweils die Ausgleichsgerade durch alle Ticks seit dem Start. Später gelten feste kleine Verstärkungen (α = 0,05, β = 0,0004).
- **Steps auf dem vorhergesagten Frame:** Der Step feuert framegenau zur vorhergesagten Zeit seines Ticks, nicht mehr am Blockanfang. Das gilt auch, wenn der Tick etwas früher eintrifft. Die Steps bleiben an die Zahl der Ticks gebunden. Kein Step läuft mehr als 2 Ticks vor den empfangenen voraus; bleibt die Clock stehen, steht auch der Groove. Liegt das Schwungrad mehr als 2 Ticks hinter den empfangenen Ticks, holt es sofort auf.
- **Messphase:** In den ersten 12 Ticks nach Start oder Continue feuern die Steps wie bisher auf dem Tick selbst.
- **Neu messen:** Bei Pausen und Tempowechseln misst das Schwungrad von vorn. Auslöser ist ein einzelner Fehler über 1,5 Ticks (mindestens 40 ms) oder ein mittlerer Fehler über ¼ Tick (mindestens 8 ms).
- **Tempo mit Hysterese:** Das angezeigte und für Delays/Rolls benutzte Tempo (`transport.bpm`) ändert sich erst, wenn die Schätzung mehr als 0,2 BPM (bzw. 0,2 %) davon abweicht. `last_length` ist die geglättete Step-Länge.
- **Test:** `host/render_demo.c` `midi_jitter_test` (Teil von `make test`). Die Clock ist exakt, aber jeder Tick kommt mit 0–15 bzw. 20 ms Verspätung am nächsten Blockanfang an; geprüft werden 93,5, 120 und 174 BPM sowie die Sprünge 120 → 135 und 140 → 128. Bedingungen: Das Tempo bleibt nach dem Einschwingen auf ±0,3 BPM und ändert sich höchstens 3-mal; jeder Step liegt innerhalb von 1,5 bzw. 2 ms des idealen Abstands; es feuern genau so viele Steps, wie Ticks kamen. Vorher: Tempo 107,7–136,0 BPM, Steps bis 15 ms daneben. Nachher: ein fester Wert, Steps innerhalb von 1,3 ms.

Kein Klangschalter (ADR 0006): Das ist eine Fehlerbehebung des Timings. Mit interner Clock bleibt alles bitgleich (alle 76 Demo-WAVs).

## Alternativen

- **Zeitstempel im Timer-Interrupt:** Die Ticks bekommen schon beim Empfang (Timer5, 2 kHz) eine Zeit und werden mit fester Latenz framegenau gespielt. Das wäre genauer, braucht aber Eingriffe in `platform/firmware/src/platform.c` (soll unverändert bleiben) und ein zweites Feld in der MIDI-Schlange. Möglich als spätere Ergänzung, falls nötig.
- **Tempo über ein festes Fenster (z. B. 1 Takt):** Das ist bei 20 ms Jitter noch ±1 % ungenau und reagiert trotzdem träge.
- **Nur die Anzeige glätten:** Das beruhigt die Zahl, aber nicht die Steps.

## Folgen

- Wird das Tempo am Sender geändert, schwingt das Schwungrad in etwa 1–2 Takten nach. Bei einem Sprung misst es neu; dann feuern die Steps für einen halben Schlag direkt auf den Ticks.
- Swing gilt bei externer Clock weiter nicht (wie bisher).
- Die Gesamtverzögerung bleibt die mittlere Verzögerung der Hauptschleife plus ein halber Block, ist jetzt aber gleichmäßig.

## Verlauf

- 2026-11-20: angelegt (Fehlermeldung des Nutzers, 53. Build / 0.5.3-alpha)
