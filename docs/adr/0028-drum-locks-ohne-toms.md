# 0028: Drum-Locks für Tonhöhe und Lautstärke, kein Tom und keine Conga mehr

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

Rückmeldung vom Nutzer:

- Tom und Conga sollen weg: weniger Instrumente, einfachere Bedienung, weniger Sounds zu spielen.
- Die Drums sollen wie die Step-Varianz (ADR 0025) Parameter-Locks bekommen: die **Tonhöhe** eines einzelnen Steps in Halbtönen mit einem Drehregler, die **Lautstärke (Velocity)** mit einem zweiten Regler, in groben Schritten; später: die ersten beiden rechten Regler (KNOB 1 und 2) dafür, und es soll gut aussehen, damit man nicht so viel drehen muss.

## Entscheidung

**Ohne Toms und Congas** (LT und HT, die beiden Schaltungen waren zugleich die Congas):

- Die Bedienung kennt zehn Instrumente: BD SD RS CP CB CY OH CH ZP PU (`ui_parts` in `ui/el_ui.c`). Sie liegen der Reihe nach auf den ersten zehn schwarzen Tasten, in der Zeilenliste und im Wechsel mit GLO + SELECT. Die zweite Ebene (SEQ) entfällt; SEQ tut in BEAT nichts mehr.
- Die Stimmen bleiben im Code (kein Umbau von `D8_NUM`, Speicherformat und Fixed-Point-Pfaden). `DR_HIDDEN(part)` in `engine/drums.h`: sie spielen nie (`audible`, `dr_hit`), haben keine MIDI-Note (`dr_note_part`), und ihre Treffer werden beim Laden eines Patterns, beim Erzeugen eines Grooves und beim Laden eines Stands entfernt (`dr_strip_hidden`).

**Locks pro Step** (`engine/drums.h`, `engine/drums.c`):

- **Tonhöhe:** −12 … +12 Halbtöne, nur bei den gestimmten Instrumenten BD SD RS CB ZP PU (dieselben, die der Drum-Tonart folgen, ADR 0016). Bei CP, CY, OH, CH gibt es keine Tonhöhe, ihr Rauschen ändert sich nicht sinnvoll. Gespeichert als fünf Bitebenen im Zweierkomplement (`dr_machine_t.pitch[Platz][5][6]`).
- **Lautstärke:** Stufe 1 … 7, 0 = so wie das Pattern ihn hat (normal oder Akzent). Drei Bitebenen (`vol[Platz][3][DR_PARTS]`). Stufen als Anschlagstärke: 15, 28, 40, 52, 66 (normaler Schlag), 83, 100 %; die Stufe ersetzt die Akzent-Stellung des Steps. Rolls folgen ihrer Stufe und Tonhöhe.
- **Klang:** `tr808_trigger_pitch` setzt pro Treffer das Tonhöhenverhältnis (`shift`, `shift_semis`); die Stimmen multiplizieren es mit ihrer Stimmung, genau dort, wo auch die Drum-Tonart eingreift (`tr808.c`, `tr808_bd_fixed.h`, `tr808_rest_fixed.h`, `tr808_metal_fixed.h` für die Kuhglocke, `dr_fixed.c`); das PO-12-Kit verschiebt seinen Stimmregler. `make metal-fixed-check` vergleicht Float und Festkomma auch mit verschobener Tonhöhe (acht neue Fälle, alle unter −49 dB).
- **Bedienung:** Step-Taste halten (wie bei Durchläufen und Wahrscheinlichkeit): **KNOB 1 = Tonhöhe** (ein Halbton pro Raste), **KNOB 2 = Velocity** (eine Stufe pro Drehung, egal wie schnell; sieben Stufen). Jede Änderung spielt den Step kurz an. Die Anzeige zeigt beide Werte zugleich in zwei Feldern, links Tonhöhe (groß „+3“, Balken um die Mitte, −12 … +12), rechts Velocity („40 %“, sieben aufsteigende Balken, SOFT … LOUD, „AUTO“ = wie im Pattern); das zuletzt gedrehte Feld ist doppelt umrahmt. Unten heißen KNOB 1 und 2 solange PITCH und VELOCITY und zeigen die Werte; bei den Anzeigen für Durchläufe und Wahrscheinlichkeit steht eine Zeile „K1 PITCH … K2 VELOCITY …“. Bei einem Instrument ohne Tonhöhe steht „---“ und „NOT TUNED“. Im Raster trägt ein Step mit Lock einen kleinen orangen Strich unten links (der weiße oben links bleibt für Durchläufe und Wahrscheinlichkeit). Ein Step, der ausgeschaltet wird, vergisst alle Locks; Kopieren, Teil löschen und Groove laden behandeln sie wie die Durchläufe.
- **Speicher:** Store-Version 24 hängt `drum_pitch` und `drum_vol` an (1056 Byte; im Arbeitsspeicher dieselben 1056 Byte). Ältere Stände laden mit leeren Locks.

## Alternativen

- **LT und HT wirklich entfernen (`D8_NUM` 12 → 10):** würde Speicher und ein paar Zeilen Rechenzeit sparen, ändert aber alle Stimmen-Tabellen, den Speicherstand und die Festkomma-Pfade, die gerade auf dem Gerät laufen. Später möglich, wenn sich der Rest bewährt hat.
- **Tonhöhe für alle Instrumente:** Hats und Becken schwingen über einen gemeinsamen Satz Rechteckoszillatoren; die Stimmung pro Schlag würde diesen Satz aufteilen. Rauschklänge haben keinen Grundton.
- **Lautstärke in 127 Stufen oder als Prozent:** wäre feiner, aber der Nutzer wollte grobe Schritte; sieben Stufen reichen für Geisterschläge bis Akzent.
- **SELECT und KNOB 1 für die Locks (erster Entwurf):** Der Nutzer wollte die ersten beiden rechten Regler; SELECT bleibt der Platzwechsel.

## Folgen

- Alte Grooves mit Tom-Treffern verlieren sie beim Laden; die Tom-Knöpfe im Klangspeicher bleiben im Stand, sind aber ohne Wirkung.
- KNOB 1 und 2 bedienen beim Halten eines BEAT-Steps nicht den Klang (LEVEL, TUNE …); KNOB 3 und 4 schon.
- Das Raster in BEAT ist zwei Zeilen kürzer (zehn statt zwölf Instrumente).
- MIDI: Tom-Noten (41, 43, 45, 47, 48, 50) werden ignoriert.
- Nicht auf dem Gerät geprüft: der Klang und der Rechenaufwand der Locks (ein Gleitkomma-Faktor pro Treffer; das Gerät läuft am Limit).

## Verlauf

- 2026-11-07: angelegt; `make test` (Test „locks“), `make metal-fixed-check` und `make web` laufen.
- 2026-11-07: Regler auf KNOB 1 (Tonhöhe) und KNOB 2 (Velocity) gelegt, SELECT wieder frei; Anzeige mit zwei Feldern und umbenannten Reglern unten.
