# 0032: Fill-ins je Platz und Tipp-Kürzel für EDIT, GLO und SEQ

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

Erste Runde der Feature-Liste (Auswahl des Nutzers vom 07.11.): Fill-ins in BEAT (N12), ein Mute-Modus (N13), der Editor per kurzem Druck (N8) und Pattern-Längen in ganzen Takten (N11). Leitlinie aus `CLAUDE.md`: kurze Klicks für Häufiges, Kombinationen für Seltenes, im Display sichtbar, was passiert. Die Tasten EDIT, GLO und SEQ waren bisher nur Umschalter (gehalten) bzw. in BEAT ohne Funktion.

## Entscheidung

- **Tippen statt Halten:** EDIT, GLO (in BEAT) und SEQ (in BEAT) reagieren auf einen kurzen Druck, wenn dabei nichts anderes gedrückt oder gedreht wurde. Gehalten bleiben EDIT und GLO die Umschalter wie bisher.
  - EDIT tippen: Editor des Bereichs auf/zu (N8).
  - GLO tippen in BEAT: Mute-Modus an/aus (N13); die Instrumenten-Tasten schalten stumm/laut, ihre LEDs zeigen voll = spielt, gedimmt = nichts aufgenommen, blinkend = stumm; GLO blinkt, die Fußzeile sagt MUTE.
  - SEQ tippen in BEAT: Fill im nächsten Takt (einmal); gehalten (ab 0,3 s): jeder Takt ein Fill, solange gehalten.
- **Fills (N12):** je Platz A–D ein eigener Takt (16 Sechzehntel, `dr_fill_t` in `engine/drums.h`), statt eines Takts des Grooves gespielt (ohne Step-Bedingungen und Locks). GLO + SEQ: automatisch alle 8 / 16 / 32 Takte im letzten Takt der Phrase, oder AUS; Countdown in der Fußzeile. EDIT + SEQ: den Fill bearbeiten (Step-Tasten 1–16, EDIT = Akzent, GLO = Roll); ein unbenutzter Fill startet als erster Takt des Grooves. Der Generator legt zu jedem neuen Groove einen Fill an (`dr_generate_fill`: erster Takt, am Ende ein Snare- oder Clap-Lauf, vier Arten); eine Variation behält den Fill. Ein unbenutzter Fill wird beim ersten Abruf erzeugt. Fills und die Automatik stehen in Speicherversion 26.
- **Pattern-Länge (N11):** GLO + ALGORITHM ändert in ganzen Takten (BEAT 16 Steps, Synths je nach Step-Länge), groß im Display; einzelne Steps nur im STEP-Modus der Synth-Bereiche.

## Alternativen

- Fills als eigene Patterns (Platz E) – braucht mehr Speicher und eine Platz-Logik; ein Takt je Platz reicht.
- Fill-Taste auf einer Taste der Tastatur – die Tasten sind in BEAT belegt; SEQ hatte dort keine Funktion.

## Folgen

- Ein Tipp auf EDIT/GLO, der versehentlich ohne zweite Taste losgelassen wird, schaltet etwas um; das zeigt das Display jeweils an.
- Fills spielen ohne Step-Bedingungen, Wahrscheinlichkeit und Locks; wenn gewünscht, später nachrüsten.

## Verlauf

- 2026-11-07: angelegt (N8, N11, N12, N13 umgesetzt, 37. Build)
