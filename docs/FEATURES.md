# FM1-Electro: Feature-Liste

Zum Abgleichen, Bestellen und Streichen. Jedes Feature hat eine feste Nummer; sie wird nie neu vergeben (gestrichene behalten ihre Nummer mit Status ✗). Ansprechen einfach mit der Nummer, z. B. „S7 streichen“, „N3 umsetzen“.

**Status:** ✓ umgesetzt · ◐ umgesetzt, wartet auf Gerätetest/Urteil · ○ offen (vorgeschlagen) · ✗ gestrichen
**Bereiche:** **B** Beat/Drums · **S** Sequencer · **Y** Synths · **F** Effekte/Master · **U** Bedienung/Display · **P** Plattform/Leistung · **N** neu, noch nicht eingeordnet

Stand: 33. Build (07.11.2026). ADR = Entscheidung in `docs/adr/`.

## B – Beat / Drums

| Nr | Feature | Status | ADR |
|---|---|---|---|
| B1 | Kalibriertes 808-Modell (BD, SD, RS, CP, CB, CY, OH, CH), in Festkomma auf dem Gerät | ✓ | 0006, 0022 |
| B2 | Clap-Ausklang nach Original (D) | ✓ | 0008 |
| B3 | Metall: wandernde Oszillatoren, linearer Open Hat | ✓ | 0009 |
| B4 | Bassdrum-Ausklang mit Oktav-Körper | ✓ | 0010 |
| B5 | Cowbell nach Aufnahme | ✓ | 0012 |
| B6 | Hi-Hats X0X/8W8-Bauweise, Closed Hat an echter 808 kalibriert | ✓ | 0014 |
| B7 | Snare mit längerem Körper | ✓ | 0015 |
| B8 | Drum-Tonart (tonale Drums gestimmt, FOLLOW BASS) | ✓ | 0016 |
| B9 | ZAP nach Referenz, PULSE | ✓ | 0017 |
| B10 | 5 Kits inkl. PO-12 | ✓ | – |
| B11 | Tom und Conga aus der Bedienung genommen (Code noch drin, ~3,5 KB) | ◐ | 0028 |
| B12 | Tom/Conga-Code komplett entfernen (spart ~3,5 KB, keine Rechenzeit) | ○ | – |
| B13 | Drum-Locks pro Step: Tonhöhe (KNOB 1, ±12 Halbtöne, nur BD SD RS CB ZP PU), Velocity (KNOB 2, 7 Stufen); zwei Felder auf dem Display | ◐ | 0028 |
| B14 | Akzent-Zeile über die 11. schwarze Taste | ◐ | – |
| B15 | Abgleich mit Referenzen (Mz808, ReplicantTr8s nur für 808-Klang) | ○ | `docs/TR808.md` |

## S – Sequencer

| Nr | Feature | Status | ADR |
|---|---|---|---|
| S1 | 4 Plätze A–D je Bereich, Wechsel am Takt | ✓ | – |
| S2 | Generator (Variation / neu) je Bereich und für alles | ✓ | – |
| S3 | Jeder Platz behält seinen Sound (Kit, Regler, Synth-Sound) | ◐ | 0024 |
| S4 | Step-Varianz 1:2 … 4:4 (PRESETS bei gehaltenem Step) | ◐ | 0025 |
| S5 | Step-Wahrscheinlichkeit 100 … 10 % (ALGORITHM bei gehaltenem Step) | ◐ | 0025 |
| S6 | Große Anzeige beim Halten eines Steps (nach ¼ s) | ◐ | 0025 |
| S7 | Step-Aufnahme SH-101: Pause mit der letzten Taste | ◐ | – |
| S8 | STRINGS-Schritte bis 8 Takte, Generator-Stil LONG, Live-Aufnahme in Achteln | ◐ | 0027 |
| S9 | Repeat-Punch-in nur solange gehalten | ✗ (zurückgenommen) | 0011 |
| S10 | Erster Start ohne Speicher: leere Plätze, Zufalls-Kit und -Sounds | ◐ | – |
| S11 | Zurücksetzen auf leeren Start per Tastenkombination | ○ | – |

## Y – Synths

| Nr | Feature | Status | ADR |
|---|---|---|---|
| Y1 | MKS-50- und JP-6-Engine je Bereich | ✓ | – |
| Y2 | BASS einstimmig, mono | ✓ | 0007 |
| Y3 | Eigene Sounds (32 Plätze) vor den Werks-Sounds, GLO + PRESETS nur eigene | ◐ | 0026 |
| Y4 | 23 zusätzliche schlichte Sounds/Pads | ◐ | 0027 |

## F – Effekte / Master

| Nr | Feature | Status | ADR |
|---|---|---|---|
| F1 | Punch-in-Effekte (Stutter, Hall-Wurf, Tape-Stop, Repeat, Delay, Crusher, Phaser) | ✓ | 0011 |
| F2 | Tape-Stop Standard 1/4 | ◐ | – |
| F3 | Gated Reverb (echter Hall mit Gate) | ✓ | 0013 |
| F4 | Verzerrung als Send (OD, DIST, RAT, TUBE), phasengleich | ◐ | 0030 |
| F5 | Send-Regler gesperrt bei Verzerrung OFF | ◐ | 0031 |
| F6 | Master-Kompressor CLUB | ◐ | 0029 |
| F7 | COMP MIX (Parallel-Kompression) | ◐ | 0031 |
| F8 | EP-133-Kompressor nachbauen (braucht Zugriff auf teenage.engineering) | ○ | 0029 |
| F9 | Hall-Modulation seltener rechnen (gebaut, bringt nichts, aus) | ✗ | 0019 |
| F10 | Master in Festkomma (gebaut, langsamer, aus) | ✗ | 0019 |

## U – Bedienung / Display

| Nr | Feature | Status | ADR |
|---|---|---|---|
| U1 | Tasten-LEDs: inaktive Bereiche gedimmt, OCT blinkt am Ende | ✓ | 0020 |
| U2 | Regler mit vielen Werten beschleunigen beim schnellen Drehen | ◐ | – |
| U3 | Gerade gedrehter Wert hervorgehoben | ◐ | – |
| U4 | Display 30 MHz, Bild in einem Zug | ◐ | – |
| U5 | Echtes Vsync (braucht TE-Leitung des Displays, unklar ob verdrahtet) | ○ | – |
| U6 | Lastanzeige in OPTIONS (DR…PU, PEAK, MAX, C1, LATE) | ✓ | 0019 |
| U7 | Hervorhebung auch im Synth-Editor und STEP-Modus | ○ | – |
| U8 | OPTIONS: alle 16 Zeilen sichtbar (9 statt 10 Pixel, HELP LINE war abgeschnitten) | ◐ | 0031 |

## P – Plattform / Leistung

| Nr | Feature | Status | ADR |
|---|---|---|---|
| P1 | Eigene Plattform ohne SDK, zwei Kerne | ✓ | 0021 |
| P2 | Bus dynamisch auf beide Kerne (Verzerrung auf Kern 1) | ✓ | 0021 |
| P3 | Speichern ohne Aussetzer (vorab löschen, Autosave bei STOP) | ◐ | 0023 |
| P4 | Bitgleiche Optimierungen (STRINGS, Hall, Master, JP-6, Saw 5 …) | ✓ | 0019 |
| P5 | Takt messen (`ADDS …/US` in OPTIONS) | ◐ | 0019 |
| P6 | Auf 320 MHz (Herstellervorgabe) umstellen, falls niedriger | ○ | 0019 |
| P7 | Überlastschutz (sparsamer rechnen bei Überlast) | ✗ (auf Wunsch) | – |
| P8 | Web-Simulator | ✓ | – |

## N – Neu (noch einzuordnen)

| Nr | Feature | Status |
|---|---|---|
| N1 | Drum-Kits merken sich deine Einstellungen: Kit wechseln und zurück bringt das Kit so, wie du es eingestellt hattest (nicht das Werks-Kit) | ○ |
| N2 | Drum-Vorschau (Instrument klingt beim Drücken seiner Taste) mit OCT+ in BEAT an/aus; OCT+ leuchtet, solange sie an ist; bei REC wird ohne Ton weiter aufgenommen. Die Step-Seiten (1–16 / 17–32) schaltet in BEAT dann OCT− um (hin und her) | ○ |
| N3 | Gedrehter Wert hebt sich ab: etwas größer und in Highlight-Farbe, nach kurzer Pause zurück zum Normalzustand, beides mit kurzer Animation (Ausbau von U3; gilt auch für Synth-Editor und STEP-Modus, vgl. U7) | ○ |
| N4 | PLAY leuchtet grün, solange der Groove läuft (nur wenn die LED Farbe kann; sonst hell/blinkend wie bisher) | ○ |
| N5 | REC gedimmt, solange nicht aufgenommen wird; voll (blinkend) beim Aufnehmen | ○ |
| N6 | STRINGS-**Live**-Aufnahme ohne feste Länge: Das Pattern wächst, bis REC gestoppt wird, gerundet auf ganze Takte, höchstens 4 Takte (32 Achtel). Jede Note wird als **eine** Note mit ihrer ganzen Haltedauer gespeichert (Anfang + Länge, keine aneinander gebundenen Schritte); dafür bekommt ein Step ein Längenfeld (neues Speicherformat) | ○ |
| N7 | Später vielleicht: STRINGS-Aufnahmen länger als 4 Takte (mehr Schritte pro Pattern oder gröbere Auflösung, neues Speicherformat) | ○ (Merkposten) |
