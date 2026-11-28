# MKS Ultra

Diese Firmware macht aus dem M-VAVE FM-1 einen MKS Ultra, einen Alpha-Juno-Nachbau (Alpha Juno / MKS-50) mit DCO, Sub-Oszillator, Rauschen, Filter mit Hochpass, Hüllkurve mit vier Zeiten und drei Pegeln, LFO, Unisono, Chorus, EQ, Delay, Hall und Arpeggiator des Klangerzeugers. Diese Fassung spielt **eine Stimme** (monofon). Dazu kommen Step-Sequencer und Live-Aufnahme (LOOP). Fassung 0.1.3-alpha.

## Überblick der Bedienung

- **KNOB 1–4** stellen die vier Felder unten im Display ein. Regler starten am aktuellen Wert; schnell drehen springt weiter.
- **SELECT** blättert durch alle Seiten, **PRESETS** wählt den Sound.
- **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO** kurz tippen: die erste Seite der Taste, noch einmal tippen: die nächste. Die Leuchte zeigt die aktive Taste.
- Taste **halten** (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): unten erscheint `HOLD: …`, KNOB 1–4 stellen dann die vier wichtigsten Werte dieser Taste ein.
- **SEL**: die Soundliste.
- **OCT −** / **OCT +**: Oktave −3 bis +3. Beide zusammen: alle Töne aus.
- Regel überall: links = weniger, leiser, kürzer; rechts = mehr, lauter, länger.
- Kurz tippen ist für Häufiges, langes Halten für Seltenes: **PLAY** halten = Start/Stop, **SAVE** lang = Speichern unter, **HOME** 1 s halten = Änderungen am zuletzt verlassenen Sound zurückholen.
- Oben im Display steht der Sound; ein Punkt und `EDITED` zeigen ungespeicherte Änderungen (SAVE blinkt dann).
- Regler zeigen 0 bis 100; Schalter zeigen ihre Stufe als Zahl.
- Die Tasten spielen mit fester Anschlagstärke 100; per MIDI mit Anschlag.

## Seiten

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (Name, Oszilloskop) | CUTOFF | RESO | ENV | RELEASE |
| HOME | PERFORM | VOLUME | GLIDE | OCTAVE | CHORUS |
| EDIT | DCO | PULSE | SAW | SUB | RANGE |
| EDIT | DCO 2 | SUB LVL | NOISE | PWM | PWM RATE |
| EDIT | DCO MOD | LFO | ENV | ENV MODE | AFTERT |
| EDIT | VCF | CUTOFF | RESO | ENV | KYBD |
| EDIT | VCF 2 | LFO | ENV MODE | HPF | AFTERT |
| EDIT | VCF 3 | BOOST | BOOST HZ | NO DC | LEVEL |
| EDIT | SPECIAL | SAW VOL | PLS VOL | SYNC | TUNE |
| EDIT | SPECIAL 2 | FINE | ENV | LFO | |
| EDIT | UNISON | UNISON | DETUNE | SPREAD | R-ROBIN |
| ENV | ENV (Bild) | T1 | L1 | T2 | L2 |
| ENV | ENV 2 (Bild) | T3 | L3 | T4 | KEY FLW |
| ENV | VCA | VCA MODE | LEVEL | AFTERT | |
| LFO | LFO | RATE | DELAY | WAVE | SYNC |
| LFO | LFO 2 | TRIGGER | INVERT | | |
| LFO | CONTROL | BEND | MW VCF | MW PITCH | MW WAH |
| LFO | GLIDE | MODE | TIME | TIMEMODE | CHORDORG |
| FX | CHORUS + EQ | CHORUS | RATE | EQ FREQ | EQ GAIN |
| FX | DELAY | MIX | TIME | FEEDBK | SYNC |
| FX | DELAY 2 | SPREAD | TONE | EQ Q | |
| FX | REVERB | MIX | SIZE | PREDLY | TONE |
| ARP | ARP | MODE | RATE | RANGE | SYNC |
| SEQ | SEQUENCER | SEQ | RATE | GATE | SHIFT |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| GLO | TEMPO | TEMPO | CLOCK (AUTO INT MIDI) | SEND | MIDI CH |
| GLO | VOICES | VOICES | FINE | BEND | LEVEL |
| GLO | SYSTEM (Version, Last) | LEVEL | | | |
| SEL | SOUNDS (Liste) | | | | |

Die Firmware ist mit einer Stimme gebaut: VOICES über 1 hat in dieser Fassung keine Wirkung.

Gehaltene Tasten:

| Taste halten | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | PULSE | SAW | SUB | PWM |
| ENV | T1 | T3 | L3 | T4 |
| LFO | RATE | DELAY | PITCH | FILTER |
| FX | CHORUS | DELAY | TIME | REVERB |
| ARP | MODE | RATE | RANGE | SYNC |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | VOLUME | GLIDE | OCTAVE | VOICES |

## Sounds und Presets

- 213 Presets in fünf Gruppen: **RON** (die 6 eigenen Presets, vorn: 041 Lead 6 short ron, Bass Hit 01, Micro Ticks, R2 Random, Short Hit, Stringer), **BANK A** (64), **BANK B** (70), **MKS** (53), **VESSEL** (20). Dazu **INIT** (das Preset „default“).
- Nach dem ersten Einschalten klingt das erste RON-Preset.
- 36 Plätze für eigene Sounds.
- **PRESETS** drehen: rechts der nächste Sound, links der vorige; am Ende einer Gruppe geht es in die nächste. Reihenfolge: INIT, RON, BANK A, BANK B, MKS, VESSEL, dann OWN (alle eigenen). Eigene Sounds stehen auch in der Gruppe, unter der sie gespeichert wurden.
- **SEL** zeigt die Liste der aktuellen Gruppe.
- Wer einen geänderten Sound verlässt, ohne zu speichern, holt die Änderungen mit **HOME** (1 s halten) zurück.

## Sequencer und Live-Aufnahme (LOOP)

**Arpeggiator** (Taste ARP): der Arpeggiator des Klangerzeugers, Teil des Sounds. MODE 0 = aus, höhere Werte schalten ihn ein; RATE, RANGE, SYNC.

**Step-Sequencer** (Seite SEQUENCER, bis 64 Schritte):

1. Mit **SEQ** auf die Seite SEQUENCER gehen und **REC** tippen.
2. Jede Taste ist ein Schritt. Taste drücken, während die vorige noch gehalten ist = gebunden. Die oberste Taste = Pause. **GLO** + Taste = die Note davor einen Schritt länger. MIDI-Noten werden ebenso aufgenommen.
3. **REC** beendet die Aufnahme; die Folge spielt sofort.
4. **REC** und gleich wieder **REC** ohne Schritt lässt die alte Folge stehen (`NOTHING RECORDED: SEQ KEPT`). Löschen: **EDIT** halten und **REC** tippen auf der Seite SEQUENCER (`SEQ CLEARED`).

- **SEQ** tippen auf der Seite SEQUENCER schaltet den Sequencer an/aus (sobald eine Folge da ist). Die Leuchte SEQ zeigt, dass er läuft.
- Während er läuft, transponieren die Tasten die Folge (C4 = unverändert); SHIFT zeigt die Verschiebung (−24 bis +24).
- RATE = Notenwert eines Schritts, GATE = Notenlänge.

**Live-Aufnahme LOOP** (REC auf jeder Seite außer SEQUENCER):

- **REC** einmal tippen: Aufnahme beginnt sofort; das Display springt auf die Seite LOOP. Steht der Takt (gestoppt), läuft er wieder an.
- **REC** zweimal schnell (innerhalb 0,4 s): ein Takt Einzählen (Anzeige `COUNT IN`), dann Aufnahme.
- **REC** während der Aufnahme: übernehmen (`LOOP KEPT`).
- **PLAY** tippen während der Aufnahme: der laufende Durchgang wird verworfen (`PASS DROPPED`); beim Einzählen bricht es ab.
- Die erste Aufnahme einer leeren Schleife legt die Länge fest: gerundet auf ganze Takte, höchstens 16 Takte (dann endet sie von selbst). Die Schleife beginnt mit dem Drücken von REC bzw. nach dem Einzählen.
- Ist schon eine Schleife da, nimmt REC dazu auf (Overdub). Jeder volle Durchgang bleibt; PLAY verwirft nur den gerade laufenden. Neue Noten klingen ab dem nächsten Durchgang.
- Quantisiert fest auf 1/16, jede Note so lang wie gehalten. Aufgenommen werden die gespielten Tasten und MIDI-Noten; läuft der Sequencer, transponieren die Tasten nur und werden nicht aufgenommen.
- Seite LOOP: **LOOP** an/aus, **BARS** = Länge (`EMPTY` = 0 = Schleife löschen, sonst 1–16), **LONGER** = wie Verlängern füllt (`REPEAT` wiederholt den Inhalt, `EMPTY` lässt leer), **SHORTER** = was beim Kürzen bleibt (`START` oder `END`).
- **SEQ** tippen auf der Seite LOOP schaltet die Schleife an/aus, sobald sie Takte hat.
- Die Schleife folgt Start/Stop (**PLAY** halten); Start beginnt sie von vorn.
- Während der Aufnahme blinkt REC.

## Bilder oben (Grafik-Kopf)

Viele Seiten zeigen oben ein Bild, das live auf die Regler antwortet. Der gerade gedrehte Regler steht 1,5 s mit seinem Wert oben links (orange).

- **LFO**: die Wellenform; Tiefe = Höhe, mehr Schwingungen = höhere Rate; ein grüner Punkt läuft mit dem LFO.
- **Hüllkurve** (ENV): die Form aus Anstieg, Abfall, Halten, Ausklingen; der gedrehte Abschnitt ist orange; ein grüner Punkt zeigt, wo die gespielte Note gerade steht.
- **VCF**: die Filterkurve aus CUTOFF und RESO (Buckel = Resonanz), der grüne Pfeil unten = Anteil der Hüllkurve.
- **DELAY**: die Echos als abnehmende Striche (Abstand = Zeit, Abnahme = Feedback; PING abwechselnd oben/unten). **REVERB**: das Abklingen.
- **DIST** (wo vorhanden): die Kennlinie des Modus mit DRIVE, daneben der Modus groß und TONE als Balken.

Seiten ohne Bild zeigen ihren Namen; beim Drehen erscheint dort für 1,5 s groß der Regler mit Wert.

## Speichern

- **SAVE** kurz: Ein eigener Sound wird auf seinem Platz überschrieben (`SAVED`). Bei einem Preset öffnet sich „Speichern unter“.
- **SAVE** lang (1 s): Speichern unter. KNOB 1 = CURSOR, KNOB 2 = LETTER (Leerzeichen, A–Z, 0–9, `-`, `+`, `.`), KNOB 3 = GROUP, KNOB 4 = PLACE (01–36). Das Display warnt, wenn der Platz belegt ist. **SAVE** = OK, **HOME** = Abbruch. Name höchstens 10 Zeichen, nicht leer.
- Im Sound gespeichert: alle Klangwerte einschließlich Chorus, EQ, Delay, Hall und Arpeggiator.
- Automatisch gespeichert (ohne SAVE): der Stand beim Ausschalten (Sound mit Änderungen, Seite, Oktave, Tempo, Clock, MIDI-Kanal), die Sequenzen und die Schleifen. Das passiert etwa 1,5 s nach einer Änderung, aber nur in einer Spielpause. Vor dem Ausschalten also anhalten (Sequencer, Schleife) und kurz warten.
- Nach dem Einschalten sind Sequencer und LOOP aus; einschalten mit SEQ bzw. auf ihrer Seite.

## Effekte

Alle Effekte gehören zum Sound und sind die des Klangerzeugers:

- **Chorus**: CHORUS an/aus, RATE (Seite CHORUS + EQ; CHORUS auch auf PERFORM).
- **EQ** (ein Glockenfilter): EQ FREQ, EQ GAIN (−24 bis +24), EQ Q (Seite DELAY 2).
- **Delay**: MIX, TIME, FEEDBK, SYNC (zum Tempo), SPREAD, TONE.
- **Hall**: MIX, SIZE, PREDLY, TONE.

## Tipps

- Seite wechseln, wenn SEQ auf seiner Seite an/aus schaltet: **SELECT** drehen.
- MIDI über USB und TRS: Kanal unter GLO, TEMPO, MIDI CH (`OMNI` = alle). Pitch Bend und Modulationsrad (CC 1) gehen an den Klang (BEND, MW VCF, MW PITCH, MW WAH). CLOCK `AUTO` folgt einer eingehenden MIDI-Clock, `INT` nie, `MIDI` immer; SEND gibt die Clock aus.
- Rettung: **OCT −** beim Einschalten halten startet nur den Update-Modus (kein Klang), um eine Firmware neu aufzuspielen.
