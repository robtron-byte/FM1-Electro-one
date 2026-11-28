# JP-6

Diese Firmware macht aus dem M-VAVE FM-1 einen JP-6 nach dem Jupiter-6: zwei VCOs mit Cross-Mod und Sync, Filter (Tiefpass, Bandpass, Hochpass) und zusätzlichem Hochpass (HPF), zwei Hüllkurven, LFO, Chorus, Akkord- und Unisono-Modus. **4 Stimmen**. Dazu Arpeggiator, Step-Sequencer, Live-Aufnahme (LOOP), Delay und Hall. Keine Distortion. Fassung 0.1.4-alpha.

## Überblick der Bedienung

- **KNOB 1–4** stellen die vier Felder unten im Display ein. Regler starten am aktuellen Wert; schnell drehen springt weiter.
- **SELECT** blättert durch alle Seiten, **PRESETS** wählt den Sound.
- **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO** kurz tippen: die erste Seite der Taste, noch einmal tippen: die nächste. Die Leuchte zeigt die aktive Taste.
- Taste **halten** (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): unten erscheint `HOLD: …`, KNOB 1–4 stellen dann die vier wichtigsten Werte dieser Taste ein.
- **SEL**: die Soundliste.
- **OCT −** / **OCT +**: Oktave −3 bis +3. Beide zusammen: alle Töne aus.
- Regel überall: links = weniger, leiser, kürzer; rechts = mehr, lauter, länger.
- Kurz tippen ist für Häufiges, langes Halten für Seltenes: **PLAY** halten = Start/Stop, **SAVE** lang = Speichern unter, **HOME** 1 s halten = Änderungen am zuletzt verlassenen Sound zurückholen.
- Oben im Display steht der Sound; ein Punkt und `EDITED` zeigen ungespeicherte Änderungen (SAVE blinkt dann). Grüne Striche zeigen die klingenden Stimmen.
- Regler gehen von 0 bis 127. Die Tasten spielen mit fester Anschlagstärke 100; per MIDI mit Anschlag.

## Seiten

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (Name, Oszilloskop) | CUTOFF | RESO | ENV | RELEASE |
| HOME | PERFORM | CHORUS | DELAY | REVERB | VOLUME |
| EDIT | VCO-1 | TRI | SAW | PULSE | SQUARE |
| EDIT | VCO-1 MOD | RANGE (32' bis 2') | X-MOD | XMD ENV | SYNC |
| EDIT | VCO-2 | TRI | SAW | PULSE | NOISE |
| EDIT | VCO-2 TUNE | RANGE (LO, 32' bis 2') | TUNE (−12 bis +12) | FINE | MIX |
| EDIT | VCO MOD | LFO | ENV | DEST (VCO-1 VCO-2 BOTH) | DRIFT |
| EDIT | PWM | PW/PWM | SOURCE (LFO MAN ENV-1) | | |
| EDIT | VCF | CUTOFF | RESO | ENV | MODE (LPF BPF HPF) |
| EDIT | VCF MOD | HPF (0 = OFF) | ENV SRC (ENV-1 ENV-2) | LFO | KEY |
| EDIT | VCA | LEVEL | LFO | VCF VEL | VCA VEL |
| ENV | ENV-1 (Bild) | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV-2 (Bild) | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV KEY | 1 KEY | 1 POL (NORM INV) | 2 KEY | RETRIG (MULTI LEGATO) |
| LFO | LFO | RATE | DELAY | WAVE (TRI SINE SAW UP SAW DN SQUARE S&H RANDOM) | SYNC |
| LFO | LFO 2 | NOTE | KEYSYNC | | |
| LFO | VOICE | MODE (POLY CHORD MONO UNISON) | PORTA (OFF ON AUTO) | GLIDE | SHIFT |
| LFO | VOICE 2 | UNI DET | CHORD | | |
| FX | CHORUS | CHORUS | MODE (FREE I II I+II) | RATE | |
| FX | SENDS | DELAY | REVERB | DLY LVL | REV LVL |
| FX | DELAY | TIME | FEEDBACK | TONE | PING |
| FX | DELAY 2 | SYNC | NOTE | MOD | |
| FX | REVERB | SIZE | DECAY | DAMP | TYPE (SPACE ROOM PLATE) |
| ARP | ARP | ARP | MODE | RATE | OCT |
| ARP | ARP 2 | GATE | HOLD | | |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| SEQ | SEQUENCER | SEQ | RATE | GATE | SHIFT |
| GLO | TEMPO | TEMPO | CLOCK (AUTO INT MIDI) | SEND | MIDI CH |
| GLO | SYSTEM (Version, Last) | LEVEL | | | |
| SEL | SOUNDS (Liste) | | | | |

CHORD (Seite VOICE 2) wählt den Akkord für den Modus CHORD: m7, SUS2, MIN, 5+8, SUS4, 7TH, MAJ, m7b5.

Gehaltene Tasten:

| Taste halten | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | X-MOD | 2 TUNE | MIX | PW/PWM |
| ENV | 1 ATT | 1 DEC | 2 ATT | 2 REL |
| LFO | RATE | DELAY | PITCH | FILTER |
| FX | DELAY | REVERB | TIME | CHORUS |
| ARP | ARP | MODE | RATE | OCT |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | MODE | GLIDE | SHIFT | VOLUME |

## Sounds und Presets

- 79 Sounds in Gruppen: **EIGENE** (die 5 eigenen Presets, vorn: 12 VCO 01, JUPI UN 01, JUPI UN 02, JUPI BA 01, INIT TO 01), dann die Werks-Sounds der JP-6-App: **BASS** (10), **LEAD** (9), **SEQUENCE** (10), **ZAPS** (6), **STAB** (7), **KEYS** (5), **STR+BRASS** (14), **PAD** (6), **FX+PERC** (7). Dazu **INIT** (INIT TONE der App).
- Nach dem ersten Einschalten klingt 12 VCO 01.
- 40 Plätze für eigene Sounds.
- **PRESETS** drehen: rechts der nächste Sound, links der vorige; am Ende einer Gruppe geht es in die nächste. Reihenfolge: INIT, die Gruppen wie oben, dann OWN (alle eigenen). Eigene Sounds stehen auch in der Gruppe, unter der sie gespeichert wurden.
- **SEL** zeigt die Liste der aktuellen Gruppe.
- Wer einen geänderten Sound verlässt, ohne zu speichern, holt die Änderungen mit **HOME** (1 s halten) zurück.

## Sequencer und Live-Aufnahme (LOOP)

**Arpeggiator** (Taste ARP): ARP an/aus, MODE (UP DOWN UP/DN RAND ORDER), RATE (Notenwert), OCT (1–4), GATE, HOLD. **PLAY** kurz tippen schaltet HOLD um.

**Step-Sequencer** (Seite SEQUENCER, bis 64 Schritte; bei der Taste SEQ die zweite Seite nach LOOP):

1. Auf die Seite SEQUENCER gehen und **REC** tippen.
2. Jede Taste ist ein Schritt. Taste drücken, während die vorige noch gehalten ist = gebunden (legato). Die oberste Taste = Pause. **GLO** + Taste = die Note davor einen Schritt länger. MIDI-Noten werden ebenso aufgenommen.
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

- **SAVE** kurz: Ein eigener Sound wird auf seinem Platz überschrieben (`SAVED`). Bei einem Werkssound öffnet sich „Speichern unter“.
- **SAVE** lang (1 s): Speichern unter. KNOB 1 = CURSOR, KNOB 2 = LETTER (Leerzeichen, A–Z, 0–9, `-`, `+`, `.`), KNOB 3 = GROUP, KNOB 4 = PLACE (01–40). Das Display warnt, wenn der Platz belegt ist. **SAVE** = OK, **HOME** = Abbruch. Name höchstens 10 Zeichen, nicht leer.
- Im Sound gespeichert: alle Klangwerte einschließlich Chorus und der Anteile DELAY und REVERB.
- Automatisch gespeichert (ohne SAVE): der Stand beim Ausschalten (Sound mit Änderungen, Seite, Oktave, Tempo, Clock, MIDI-Kanal, Effekte, Arp), die Sequenzen und die Schleifen. Das passiert etwa 1,5 s nach einer Änderung, aber nur in einer Spielpause. Vor dem Ausschalten also anhalten (Arp, Sequencer, Schleife) und kurz warten.
- Nach dem Einschalten sind Sequencer und LOOP aus; einschalten mit SEQ bzw. auf ihrer Seite.

## Effekte

- **Chorus** (im Sound): CHORUS an/aus, MODE (FREE I II I+II), RATE.
- **Delay und Hall**: Die Anteile DELAY und REVERB gehören zum Sound (Seiten PERFORM und SENDS). Global, nicht im Sound: DLY LVL und REV LVL (Pegel der Effekte), DELAY: TIME, FEEDBACK, TONE, PING; DELAY 2: SYNC (zum Tempo), NOTE (Notenwert), MOD; REVERB: SIZE, DECAY, DAMP, TYPE. Delay höchstens 0,6 s.
- Keine Distortion: ihre Werte stehen weiter im Sound, haben aber keine Seite und keine Wirkung.

## Tipps

- Seite wechseln, wenn SEQ auf seiner Seite an/aus schaltet: **SELECT** drehen.
- MIDI über USB und TRS: Kanal unter GLO, TEMPO, MIDI CH (`OMNI` = alle). Pitch Bend und Modulationsrad haben keine Wirkung. CLOCK `AUTO` folgt einer eingehenden MIDI-Clock, `INT` nie, `MIDI` immer; SEND gibt die Clock aus.
- Rettung: **OCT −** beim Einschalten halten startet nur den Update-Modus (kein Klang), um eine Firmware neu aufzuspielen.
