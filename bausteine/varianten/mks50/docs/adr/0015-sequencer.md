# 0015: Step-Sequencer wie der SH-101 (SEQ, REC)

- **Status:** vorgeschlagen
- **Stand:** 2026-11-14 (gebaut, im Emulator geprüft; am Gerät noch nicht)

## Kontext

Der Nutzer wünscht einen Sequencer auf der noch freien Taste SEQ, „den SH-101 wie im Electro One“ (FM1-Electro,
github.com/robtron-byte/FM1-Electro-one, `ui/el_ui.c` `step_record`, eigenes Projekt des Nutzers, GPL-3.0-only): Schritt
für Schritt aufnehmen, Bindungen durch Halten, Pausen mit der letzten Taste, Transponieren mit den Tasten.

## Entscheidung

- **Ein Sequencer je Part**, bis 64 Schritte; ein Schritt ist eine Note, eine Pause oder eine an die nächste gebundene
  Note (Legato). Gilt gleich für MKS-50, JX-8P und die Kombi.
- **REC** (bisher frei) startet die Aufnahme für den gewählten Part (REC blinkt, die Seite SEQ zeigt die Schritte):
  jede Taste = der nächste Schritt; eine Taste, während die vorige noch gehalten ist = gebunden; die **oberste Taste** =
  Pause; **GLO + Taste** = die Note davor einen Schritt länger. **REC** noch einmal beendet die Aufnahme, die Sequenz
  spielt. Ein Partwechsel beendet sie ebenso.
- **SEQ** tippen: Seite SEQ (KNOB 1 SEQ an/aus, 2 RATE als Notenwert, 3 GATE, 4 SHIFT); gehalten: dieselben vier live.
- **Spielen:** im Raster von Tempo bzw. MIDI-Clock wie der Arp; PLAY halten (Start) setzt sie auf Schritt 1, STOP hält
  sie an. Solange sie spielt, **transponieren die Tasten** (und MIDI-Noten des Parts): C4 = 0, wie beim SH-101. Eine
  laufende Sequenz hat Vorrang vor dem Arp des Parts.
- **Speichern:** Schritte und Einstellungen in Blob 3, in der nächsten Spielpause nach einer Änderung (wie das Journal,
  ADR 0011). Die Sets (ADR 0013) enthalten die Sequenzen nicht: alte Sets blieben sonst nicht lesbar, und in der Kombi
  ist kein Platz.

## Prüfung (Emulator)

Aufnahme von 5 Schritten mit Bindung und Pause, Abspielen im Takt, Transponieren (+2) in allen drei Firmwares.

## Folgen

- Kombi: der schnelle Speicher ist danach voll (152 Byte frei); die Tabellen der JX-Engine liegen dort im RAM-Code-
  Bereich (`EL_DSP_DATA_SECTION`).

## Verlauf

- 2026-11-14: angelegt auf Wunsch des Nutzers (MKS-50, JX-8P, Kombi 0.2.0-alpha)
- 2026-11-14: REC nimmt auch MIDI-Noten auf dem Kanal des Parts als Schritte auf (Legato bindet, GLO + Note verlängert, Pause mit der obersten Taste am Gerät); steht SEQ beim Drücken von REC auf ON, läuft die neue Sequenz wie beim FM1-Electro weiter und wächst mit jedem Schritt, die Tasten klingen dabei statt zu transponieren (Wunsch des Nutzers); 0.2.3-alpha
