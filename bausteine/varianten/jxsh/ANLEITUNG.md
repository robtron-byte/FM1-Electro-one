# JX-8P + SH-101

Firmware für das M-VAVE FM-1 (Fassung 0.1.4-alpha, im Display `JX+SH`). Vier Parts A–D; jeder Part spielt entweder den **JX-8P** oder den **SH-101**. Alle JX-Parts teilen sich einen Pool von **4 Stimmen**; der SH-101 ist **monophon** und darf auf höchstens einem Part laufen. Dazu kommen eine gemeinsame Distortion mit Send je Part, Delay und Hall, je Part ein Arpeggiator, ein Step-Sequencer und eine Live-Schleife (LOOP).

## Überblick der Bedienung

- **ALGORITHM** wählt den Part (A–D, oben im Display hervorgehoben). Tasten, Regler und Seiten gelten immer für den gewählten Part; die Seiten folgen seinem Klangerzeuger (JX-8P oder SH-101).
- **KNOB 1–4** stellen die vier Felder unten im Display ein. Links = weniger, rechts = mehr; ein Regler beginnt beim aktuellen Wert; schnelles Drehen macht größere Schritte.
- **SELECT** blättert durch alle Seiten, **PRESETS** wählt den Sound des gewählten Parts.
- **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO, SEL**: tippen = die zuletzt benutzte Seite dieser Taste, noch einmal tippen = ihre nächste Seite. **Halten** (EDIT, ENV, LFO, FX, ARP, SEQ, GLO) legt die vier wichtigsten Werte auf KNOB 1–4 (unten `HOLD: …`).
- **OCT −** / **OCT +**: Oktave −3 bis +3. Beide zusammen: alle Töne aus (`ALL NOTES OFF`).
- **PLAY** tippen: Arp-HOLD an/aus. **PLAY** halten (0,6 s): Start/Stopp des Takts (folgt das Gerät der MIDI-Clock: `FOLLOWS MIDI CLOCK`).
- **REC**: auf der Seite SEQUENCER Schrittaufnahme, sonst Live-Aufnahme (LOOP).
- **SAVE**: Sound speichern (siehe Speichern). **HOME** lang (1 s): die Änderungen des zuletzt verlassenen Sounds zurückholen.
- Display oben: die Parts mit Soundnamen; kleiner Punkt = Sound geändert, Lichtpunkt = MIDI kommt an, Striche = klingende Stimmen. Unten: Seitenname, Punkte für die Seiten dieser Taste, Oktave.

## Seiten

`—` = Feld leer. Die Seiten stehen nach Tasten geordnet; SELECT blättert in dieser Reihenfolge:

- **JX-8P-Part:** PLAY, PARTS, MIX, EDIT-Seiten, ENV-Seiten, LFO-Seiten, CHORUS (2), ARP (2), VOICE (2), SEQUENCER, LOOP, DIST, DIST SEND, DELAY, DELAY 2, REVERB, REVERB 2, TEMPO, MIDI, SYSTEM, SOUNDS. FX führt also zuerst zu CHORUS, GLO zuerst zu VOICE.
- **SH-101-Part:** PLAY, PARTS, MIX, EDIT-, ENV-, LFO-Seiten, ARP, ARP 2, SEQUENCER, LOOP, DIST … SYSTEM, SOUNDS, zuletzt SENDS (letzte FX-Seite).

### Seiten für beide Klangerzeuger

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PARTS | A | B | C | D |
| HOME | MIX | A | B | C | D |
| SEQ | SEQUENCER | SEQ | RATE | GATE | G.VAR |
| SEQ | PATTERN | STYLE | NEW | VARY | LENGTH |
| SEQ | STEP | STEP | NOTE | ACCENT | TIE |
| SEQ | BANK | SLOT | STORE | CLEAR | SHIFT |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| FX | DIST | MODE | DRIVE | TONE | LEVEL |
| FX | DIST SEND | A | B | C | D |
| FX | DELAY | TIME | FEEDBK | TONE | LEVEL |
| FX | DELAY 2 | SYNC | NOTE | PING | MOD |
| FX | REVERB | SIZE | DECAY | DAMP | LEVEL |
| FX | REVERB 2 | TYPE | MOD | — | — |
| GLO | TEMPO | TEMPO | CLOCK | SEND | — |
| GLO | MIDI | A | B | C | D |
| GLO | SYSTEM | LEVEL | — | — | — |
| SEL | SOUNDS | — | — | — | — |

PARTS: Klangerzeuger je Part. MIX: Pegel je Part (0–127). MIDI: Kanal je Part (OMNI, 1–16). SYSTEM: LEVEL ist der Pegel des gewählten Parts; dazu Fassung und Auslastung beider Kerne.

### JX-8P-Part

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY | CUTOFF | RESO | ENV | RELEASE (ENV-2) |
| EDIT | DCO-1 | RANGE | WAVE | TUNE | LFO |
| EDIT | DCO-2 | RANGE | WAVE | X-MOD | TUNE |
| EDIT | DCO-2 | FINE | LFO | — | — |
| EDIT | DCO ENV | DCO-1 | DCO-2 | ENV | DYNA |
| EDIT | MIXER | DCO-1 | DCO-2 | ENV | ENV MD |
| EDIT | VCF | CUTOFF | RESO | ENV | HPF |
| EDIT | VCF | LFO | KEY | ENV MD | DYNA |
| EDIT | VCA | LEVEL | MODE | DYNA | VOLUME |
| ENV | ENV-1 | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV-2 | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV KEY | 1 KEY | 2 KEY | SNAP | RETRIG |
| LFO | LFO | RATE | DELAY | WAVE | SYNC |
| LFO | LFO | SHAPE | KEYSYNC | — | — |
| FX | CHORUS | CHORUS | MODE | DELAY | REVERB |
| FX | CHORUS | RATE | — | — | — |
| ARP | ARP | ARP | MODE | RATE | OCTAVES |
| ARP | ARP | GATE | HOLD | TEMPO | — |
| GLO | VOICE | MODE | GLIDE | TIME | SHIFT |
| GLO | VOICE | UNI DET | DRIFT | — | — |

JX-Werte erscheinen als Zahlen. VOICE MODE: 2 = Mono, 3 = Unison, 0 und 1 spielen polyphon.

### SH-101-Part

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY | CUTOFF | RESO | ENV | DECAY |
| EDIT | VCO | RANGE | MOD | PW | PWM SRC |
| EDIT | SOURCE | PULSE | SAW | SUB | NOISE |
| EDIT | SOURCE 2 | SUB OCT | WHITE | FINE | FLOOR |
| EDIT | VCF | CUTOFF | RESO | ENV | KYBD |
| EDIT | VCF 2 | VCF MOD | SLOPE | ENV FULL | RES COMP |
| EDIT | FM + DRIVE | FM SRC | FM AMT | DRIVE | VOLUME |
| ENV | ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | VCA | VCA | TRIGGER | DECLICK | — |
| ENV | VELOCITY | VEL VCA | VEL VCF | — | — |
| LFO | LFO | RATE | WAVE | MOD | VCF MOD |
| LFO | LFO 2 | SYNC | INVERT | KEY TRIG | SNAP |
| LFO | CONTROL | BEND VCO | BEND VCF | MOD WHL | — |
| LFO | GLIDE | GLIDE | MODE | LINEAR | TRANSP |
| ARP | ARP | ARP | MODE | RATE | OCT |
| ARP | ARP 2 | GATE | HOLD | — | — |
| FX | SENDS | DLY SEND | REV SEND | — | — |

Schieberegler-Werte zeigen 0.0–10.0.

### Gehaltene Tasten

| Taste halten | JX-8P-Part | SH-101-Part |
|---|---|---|
| EDIT | X-MOD, 2 TUNE, MIX 1, MIX 2 | PULSE, SAW, SUB, PW |
| ENV | 1 ATTACK, 1 DECAY, 2 ATTACK, 2 RELEASE | ATTACK, DECAY, SUSTAIN, RELEASE |
| LFO | RATE, DELAY, PITCH, FILTER | RATE, MOD, VCF MOD, WAVE |
| FX | DELAY, REVERB (Sends im Sound), TIME, CHORUS | DLY SEND, REV SEND, TIME, DIST (DIST-Send dieses Parts) |
| ARP | ARP, MODE, RATE, OCT | ARP, MODE, RATE, OCT |
| SEQ | SEQ, NEW, SLOT, G.VAR | SEQ, NEW, SLOT, G.VAR |
| GLO | Pegel A, B, C, D | Pegel A, B, C, D |

## Sounds und Presets

- **PRESETS** drehen: nächster bzw. vorheriger Sound; am Ende einer Gruppe geht es in die nächste. Es erscheinen nur Sounds des Klangerzeugers, den der gewählte Part spielt.
- Gruppen in dieser Reihenfolge: INIT, **EIGENE** (die eigenen JX-8P-Presets, z. B. STRINGS 01, HI STRI 01, SYNTH B 01), die JX-Werksgruppen (BASS, LEAD, PAD …), JX INIT, SH BASS, SH LEAD, SH SEQ, SH FX, zuletzt OWN (alle selbst gespeicherten Sounds).
- **SEL** zeigt die Soundliste (Seite SOUNDS) mit dem Gruppennamen oben rechts; PRESETS blättert darin.
- Ohne gespeicherten Stand startet das Gerät mit A STRINGS 01, B HI STRI 01, C SYNTH B 01 (JX-8P) und D SAW BASS (SH-101).
- Geänderter Sound: Punkt im Kopf, `EDITED` auf der Seite, SAVE blinkt. Wer den Sound wechselt, kann die Änderungen mit **HOME lang** zurückholen (`EDITS BROUGHT BACK`).
- Pitch Bend und Modulationsrad (MIDI) wirken nur auf den SH-101.

## Parts

- Seite **PARTS** (HOME): KNOB 1–4 stellt für Part A–D JX-8P oder SH-101 ein. Beim Wechsel lädt der Part den INIT-Sound des neuen Erzeugers. Ein zweiter SH-101 wird mit `SH-101 IN USE` abgewiesen.
- Die JX-Parts teilen sich 4 Stimmen.
- Seite **MIX**: Pegel A–D (Start 100). Der SH-101 ist deutlich lauter als der JX-8P; mit MIX ausgleichen.
- Seite **MIDI** (GLO): Empfangskanal je Part, Start A = 1, B = 2, C = 3, D = 4; OMNI = alle Kanäle. MIDI über USB und TRS: Noten, Pitch Bend, CC 1, CC 120/123 (alle Töne aus), Clock, Start/Stop/Continue, Song Position.
- Die Tasten des FM-1 spielen den gewählten Part.

## Sequencer und Live-Aufnahme (LOOP)

**Takt** (GLO, Seite TEMPO): TEMPO 40–240 (Start 120), CLOCK AUTO / INT / MIDI, SEND = MIDI-Clock senden an/aus. PLAY halten startet und stoppt.

**Arpeggiator** (ARP, je Part): ARP an/aus, MODE (UP, DOWN, UP/DN, RAND, ORDER), RATE (1/32 bis 1/1), OCT 1–4, GATE 5–100 %, HOLD (auch PLAY tippen).

**Step-Sequencer** (SEQ, je Part, bis 64 Schritte):

1. Seite SEQUENCER aufrufen, **REC** tippen (`REC  KEYS = STEPS  REC = DONE`).
2. Jede Taste ist ein Schritt. Eine Taste, gedrückt während die vorige noch gehalten ist, wird gebunden. Die oberste Taste setzt eine Pause. **GLO** + Taste verlängert die vorige Note um einen Schritt. Noten über MIDI werden ebenfalls aufgenommen.
3. **REC** beendet die Aufnahme, der Sequencer spielt (`… STEPS: PLAYING`).
4. **REC** und gleich wieder **REC** ohne Schritt lässt die alte Folge stehen (`NOTHING RECORDED: SEQ KEPT`). Löschen: **EDIT** halten und **REC** tippen auf der Seite SEQUENCER (`SEQ CLEARED`).

SEQ tippen auf einer SEQ-Seite (sobald es eine Folge gibt) schaltet ihn an/aus; SELECT blättert dann zwischen SEQUENCER, PATTERN, STEP, BANK und LOOP. Läuft er, transponieren die Tasten die Folge (C4 = unverändert, Anzeige SHIFT). Nach dem Einschalten ist jeder Sequencer aus.

Wie in den iOS-Apps JX-8P/MKS-50:

- **SEQUENCER**: RATE = Notenwert; GATE = Notenlänge, bis 100 % eines Schritts, darüber 1–8 ganze Schritte (die Note klingt über Pausen); **G.VAR** macht aus festen Schritten lange Noten (2–4 Schritte, zuerst auf den Zählzeiten); mehr G.VAR = mehr lange Noten, der Rhythmus bleibt.
- **PATTERN**: STYLE KRAFT, ROTHER, ROBOT, ACID, MIAMI; **NEW** nach rechts drehen = neue 16er-Folge im Stil; **VARY** nach rechts = 1–2 Schritte ändern; LENGTH 1–64.
- **STEP**: STEP wählt den Schritt (Strich im Bild), NOTE ändert ihn (ganz links = Pause), ACCENT (lauter, etwas länger, im Bild orange), TIE (legato in den nächsten Schritt).
- **BANK**: 8 Plätze. **STORE** nach rechts = die Folge des Parts mit RATE, GATE, G.VAR in den Platz unter SLOT; **SLOT** drehen = diesen Platz spielen: läuft die Folge, wechselt sie erst am Ende (`SLOT 3 NEXT`), sonst sofort; `*` = spielt, `-` = leer. **CLEAR** nach rechts leert den Platz. Die Bank bleibt über das Ausschalten.

**Live-Aufnahme LOOP** (je Part, REC auf jeder Seite außer SEQUENCER):

1. **REC einmal**: Die Aufnahme beginnt sofort, ohne Einzählen; der Takt startet, falls er stand; das Display zeigt die Seite LOOP.
2. **REC zweimal schnell** (innerhalb 0,4 s): ein Takt Einzählen, dann Aufnahme. Das Einzählen ist stumm, nur das Display zeigt `COUNT IN`.
3. **REC** noch einmal: die Aufnahme wird übernommen (`LOOP KEPT`). Die erste Aufnahme legt die Länge fest, gerundet auf ganze Takte (1–16); nach 16 Takten endet sie von selbst. Danach spielt die Schleife.
4. Gibt es schon eine Schleife, nimmt **REC** darüber auf (Overdub): jeder fertige Durchgang bleibt, **REC** beendet.
5. **PLAY tippen** während Aufnahme oder Einzählen verwirft den laufenden Durchgang (`PASS DROPPED`): bei der ersten Aufnahme bleibt die Schleife leer, beim Overdub verschwindet nur der laufende Durchgang.

Quantisiert fest auf 1/16, jede Note so lang wie gehalten. Seite LOOP: LOOP an/aus (auch SEQ tippen auf dieser Seite), BARS = Länge 0–16 Takte (0 = leer, löscht die Schleife), LONGER = beim Verlängern wiederholen (REPEAT) oder leer anhängen (EMPTY), SHORTER = beim Kürzen den Anfang (START) oder das Ende (END) behalten. Das Display zeigt Takt/Länge. Nach dem Einschalten sind die Schleifen aus und werden mit LOOP wieder angeschaltet.

## Bilder oben (Grafik-Kopf)

Viele Seiten zeigen oben ein Bild, das live auf die Regler antwortet. Der gerade gedrehte Regler steht 1,5 s mit seinem Wert oben links (orange).

- **LFO**: die Wellenform; Tiefe = Höhe, mehr Schwingungen = höhere Rate; ein grüner Punkt läuft mit dem LFO.
- **Hüllkurve** (ENV): die Form aus Anstieg, Abfall, Halten, Ausklingen; der gedrehte Abschnitt ist orange; ein grüner Punkt zeigt, wo die gespielte Note gerade steht.
- **VCF**: die Filterkurve aus CUTOFF und RESO (Buckel = Resonanz), der grüne Pfeil unten = Anteil der Hüllkurve.
- **DELAY**: die Echos als abnehmende Striche (Abstand = Zeit, Abnahme = Feedback; PING abwechselnd oben/unten). **REVERB**: das Abklingen.
- **DIST** (wo vorhanden): die Kennlinie des Modus mit DRIVE, daneben der Modus groß und TONE als Balken.

Seiten ohne Bild zeigen ihren Namen; beim Drehen erscheint dort für 1,5 s groß der Regler mit Wert.

## Speichern

- **SAVE kurz**: Ist der Sound ein eigener, wird er an seinem Platz überschrieben (`SAVED`). Sonst öffnet sich der Dialog.
- **SAVE lang** (1 s): Dialog `Speichern unter`. KNOB 1 = CURSOR, KNOB 2 = LETTER (Leerzeichen, A–Z, 0–9, - + .), KNOB 3 = GROUP (nur Gruppen des Erzeugers des Parts), KNOB 4 = PLACE (01–40). `THIS PLACE IS TAKEN: SAVE REPLACES IT` warnt vor dem Überschreiben. **SAVE** = OK, **HOME** = Abbruch. Namen bis 10 Zeichen; ohne Namen `A NAME, PLEASE`.
- 40 Plätze für eigene Sounds, gemeinsam für JX-8P und SH-101.
- Automatisch: Parts, Sounds samt ungespeicherten Änderungen, Pegel, Kanäle, Effekte, Arp, Tempo und die Seite werden 1,5 s nach der letzten Änderung in einer Spielpause gesichert und beim Einschalten wiederhergestellt. Step-Folgen und Schleifen werden ebenfalls gesichert, wenn nichts spielt.

## Effekte

- **Pegel:** JX-8P und SH-101 sind bei gleichem MIX gleich laut.
- **DIST** (FX): eine Distortion für alle Parts. MODE TUBE / OD / DIST / RAT, DRIVE, TONE, LEVEL (Start TUBE, 60, 70, 64).
- **DIST SEND** (FX): Anteil je Part A–D, 0–127 (Start 0 = aus). Die JX-Parts gehen gemeinsam in die Distortion: es gilt der höchste DIST SEND aller JX-Parts. DRIVE auf der SH-Seite FM + DRIVE ist die eigene Übersteuerung des SH-101-Sounds.
- **DELAY / DELAY 2** (höchstens 0,6 s): TIME, FEEDBK, TONE, LEVEL; SYNC, NOTE, PING, MOD. **REVERB / REVERB 2**: SIZE, DECAY, DAMP, LEVEL; TYPE, MOD. Delay und Hall gelten für alle Parts und sind an, sobald ihr LEVEL über 0 steht. Beim allerersten Start stehen beide LEVEL auf 0.
- **Sends**: JX-8P-Parts haben ihre Sends im Sound (Seite CHORUS: DELAY, REVERB, dazu CHORUS, MODE, RATE). SH-101-Part: Seite SENDS, DLY SEND und REV SEND (Start 40).

## Tipps

- Kein Delay, kein Hall: auf DELAY bzw. REVERB den LEVEL aufdrehen.
- Seite SYSTEM zeigt die Fassung und die Auslastung; `1K!` heißt, der zweite Kern läuft nicht und Kern 0 rechnet alles.
- Rettung: **OCT −** beim Einschalten halten startet nur den Update-Modus über USB (kein Klang). So lässt sich jederzeit eine andere Firmware aufspielen.
