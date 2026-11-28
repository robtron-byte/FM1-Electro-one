# JX-8P

Firmware für das M-VAVE FM-1 mit dem Klang des Roland JX-8P (Fassung 0.2.7-alpha), fortgeführt aus der fm1-synths-Firmware. Vier Parts (A–D) spielen gleichzeitig, jeder mit eigenem Sound, Pegel und MIDI-Kanal; sie teilen sich einen Pool von **4 Stimmen**. Dazu Arpeggiator, Akkord-Modus, Step-Sequencer, Live-Loop, Delay, Hall und 8 Sets. Wer von der fm1-synths-Fassung aktualisiert, behält eigene Sounds, Sets, Sequenzen und den letzten Stand.

## Überblick der Bedienung

| Bedienelement | Wirkung |
|---|---|
| KNOB 1–4 | die vier Felder unten im Display |
| SELECT | Seite zurück / weiter (alle Seiten liegen in einer Reihe) |
| PRESETS | Sound zurück / weiter; am Ende einer Gruppe weiter in die nächste |
| ALGORITHM | Part A–D wählen |
| OCT − / OCT + | Oktave der Tasten (−3 … +3) |
| OCT − und OCT + zusammen | alle Töne aus |
| Notentasten | spielen den gewählten Part |

Überall gilt: links = weniger / zurück, rechts = mehr / weiter. Ein Regler startet am aktuellen Wert; schnell gedreht macht er größere Schritte (bei Werten mit vielen Stufen).

Seitentasten (EDIT, ENV, LFO, FX, ARP, SEQ, GLO):

- **tippen** öffnet die Seiten der Taste; noch einmal tippen: die nächste Seite. Von einer anderen Taste kommend erscheint die Seite, die dort zuletzt offen war.
- **halten** legt die vier wichtigsten Werte sofort auf KNOB 1–4, egal welche Seite gerade zu sehen ist (Fußzeile „HOLD: …“). Loslassen: die Regler gehören wieder der Seite.

| Taste halten | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | X-MOD | 2 TUNE (DCO-2) | MIX 1 | MIX 2 |
| ENV | 1 ATTACK | 1 DECAY | 2 ATTACK | 2 RELEASE |
| LFO | RATE | DELAY | PITCH (DCO-1) | FILTER |
| FX | Delay-Send | Hall-Send | Delay TIME | CHORUS |
| ARP | ARP | MODE | RATE | CHORD |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | Pegel A | Pegel B | Pegel C | Pegel D |

Weitere Tasten:

| Taste | kurz | gehalten |
|---|---|---|
| HOME | Seiten PLAY und MIX; im Speichern-Dialog: abbrechen | 1 s: die beim Soundwechsel verworfenen Änderungen zurückholen |
| SEL | Seiten SOUNDS und SETS | |
| SAVE | Sound speichern (auf SETS: Set speichern); im Dialog: OK | 1 s: Speichern unter |
| PLAY | Arp-HOLD des Parts an/aus; während einer Live-Aufnahme: Durchgang verwerfen | 0,6 s: Transport START / STOP |
| REC | Live-Loop; auf der Seite SEQ: Step-Aufnahme | |

Das Display zeigt oben immer alle vier Parts (Sound, klingende Stimmen, roter Punkt bei ungespeicherten Änderungen), unten den Namen der Seite und die Oktave. SAVE blinkt bei ungespeicherten Änderungen, REC blinkt während einer Aufnahme, PLAY blinkt im Takt, solange der Transport läuft.

## Seiten

In der Reihenfolge, in der SELECT sie durchblättert (die Klangseiten wie auf dem JX-8P-Panel):

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY | CUTOFF | RESO | ENV | RELEASE (ENV-2) |
| HOME | MIX | Pegel A | Pegel B | Pegel C | Pegel D |
| EDIT | DCO-1 | RANGE | WAVE | TUNE | LFO |
| EDIT | DCO-2 | RANGE | WAVE | X-MOD | TUNE |
| EDIT | DCO-2 | FINE | LFO | | |
| EDIT | DCO ENV | DCO-1 | DCO-2 | ENV | DYNA |
| EDIT | MIXER | DCO-1 | DCO-2 | ENV | ENV MD |
| EDIT | VCF | CUTOFF | RESO | ENV | HPF |
| EDIT | VCF | LFO | KEY | ENV MD | DYNA |
| EDIT | VCA | LEVEL | MODE | DYNA | VOLUME |
| ENV | ENV-1 | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV-2 | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV KEY | 1 KEY | 2 KEY | SNAP | RETRIG |
| LFO | LFO | RATE | DELAY | WAVE | SYNC |
| LFO | LFO | SHAPE | KEYSYNC | | |
| FX | CHORUS | CHORUS | MODE | DELAY (Send) | REVERB (Send) |
| FX | CHORUS | RATE | | | |
| FX | DELAY | TIME | FEEDBK | TONE | LEVEL |
| FX | DELAY | SYNC | PING | MOD | |
| FX | REVERB | SIZE | DECAY | DAMP | LEVEL |
| FX | REVERB | TYPE | MOD | | |
| ARP | ARP | ARP | MODE | RATE | OCTAVES |
| ARP | ARP | GATE | HOLD | TEMPO | |
| ARP | CHORD | CHORD | MODE | | |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| SEQ | SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | TEMPO | TEMPO | SOURCE | SEND | |
| GLO | MIDI | Kanal A | Kanal B | Kanal C | Kanal D |
| GLO | VOICE | MODE | GLIDE | TIME | SHIFT |
| GLO | VOICE | UNI DET | DRIFT | | |
| GLO | SYSTEM | TEST | SOUND | CORES | |
| SEL | SOUNDS | GROUP | | | |
| SEL | SETS | SET | LOAD | | |

Hinweise:

- WAVE: NOISE, SQUARE, PULSE, SAW. X-MOD: OFF, SYNC1, SYNC2, XMOD. ENV-Modi: 2 DOWN, 2 UP, 1 DOWN, 1 UP. VCA MODE: GATE oder ENV2.
- LFO RATE wird in Hz angezeigt; mit SYNC an stellt RATE einen Notenwert ein. LFO WAVE: RANDOM, SQUARE, SINE.
- CHORD: OFF oder ein Akkord (m7, SUS2, MIN, 5+8, SUS4, 7TH, MAJ, m7b5); eine Taste spielt dann den ganzen Akkord. MODE: POLY, CHORD, MONO, UNISON.
- ARP MODE: UP, DOWN, UP/DN, RAND, ORDER; OCTAVES 1–4.
- MIDI-Kanäle: OMNI oder 1–16, ab Werk A = 1, B = 2, C = 3, D = 4.
- SYSTEM zeigt Version und Last. TEST ist ein Lasttest: er spielt 1–8 Stimmen und lädt dafür den mit SOUND gewählten Werks-Sound auf die Parts A–C. CORES: ein oder zwei Rechenkerne. Zum Spielen TEST auf OFF lassen.

## Sounds und Presets

Die Sounds liegen in Gruppen. Mit KNOB 1 (GROUP) auf der Seite SOUNDS springt man an den Anfang einer Gruppe, mit PRESETS geht man Sound für Sound weiter. Reihenfolge der Gruppen:

1. **INIT**: INIT TONE (DCO-1 Sägezahn, Filter offen), dann PG8X INIT.
2. **EIGENE**: zuerst die 7 Presets aus der JX-8P-iOS-App (STRINGS 01, HI STRI 01, SYNTH B 01, PG8X IN 01–03, SPRUNG ODE), dann die am Gerät gespeicherten eigenen Sounds.
3. Die 64 Werks-Töne in BASS, LEAD, SEQUENCE, ZAPS, STAB, KEYS, BELLS, STR+BRASS, PAD, FX+PERC, RISERS (leere Gruppen werden übersprungen). Eigene Sounds und die iOS-Presets erscheinen zusätzlich in ihrer Gruppe.

Ein geänderter Sound ist mit einem roten Punkt markiert, bis er gespeichert ist. Wechselt man den Sound mit ungespeicherten Änderungen, holt HOME (1 s halten) sie zurück.

## Parts und Sets

ALGORITHM wählt den Part A–D; die Tasten spielen den gewählten Part, MIDI spielt jeden Part auf seinem Kanal. Pegel auf MIX (oder GLO halten), Kanäle auf MIDI. Beim allerersten Start: A POLY SYNTH, B E. BASS, C STRINGS 1, D PIANO 1.

Ein **Set** sichert den ganzen Aufbau: alle vier Parts mit ihren Sounds so, wie sie gerade klingen (auch ungespeichert), Kanal, Pegel und Arp, dazu Delay, Hall, Tempo und Clock. 8 Plätze. Sequenzen und Loops gehören nicht dazu.

1. SEL zweimal tippen: Seite SETS.
2. KNOB 1 wählt den Platz.
3. KNOB 2 nach rechts lädt das Set; nach links holt den Stand vor dem Laden zurück (einmal).
4. Speichern: auf der Seite SETS SAVE drücken. KNOB 1 Cursor, KNOB 2 Buchstabe, KNOB 4 Platz; SAVE = OK, HOME = abbrechen. Ein belegter Platz wird ersetzt.

## Sequencer und Live-Aufnahme

### Live-Loop (Seite LOOP)

Ein Loop je Part, für den gewählten Part:

- **REC einmal tippen**: die Aufnahme beginnt sofort, das Display springt auf LOOP.
- **REC zweimal schnell tippen**: erst ein Takt Einzählen („COUNT IN“), dann Aufnahme.
- **REC noch einmal**: die Aufnahme wird übernommen („LOOP KEPT“).
- **PLAY tippen** während der Aufnahme: der laufende Durchgang wird verworfen („PASS DROPPED“).

Die erste Aufnahme eines leeren Loops legt seine Länge fest: ganze Takte, höchstens 16 (bei 16 Takten endet sie von selbst). Danach läuft der Loop; jede weitere Aufnahme spielt dazu (Overdub). Jeder fertige Durchgang bleibt, PLAY verwirft nur den laufenden. Die Noten werden auf 1/16 gerastet und so lang gespielt, wie sie gehalten wurden.

Seite LOOP:

| KNOB | Wert |
|---|---|
| 1 LOOP | Loop des Parts ON / OFF |
| 2 BARS | Länge in Takten; 0 = EMPTY (Loop gelöscht) |
| 3 LONGER | wie BARS verlängert: REPEAT (Inhalt wiederholen) oder EMPTY (leere Takte) |
| 4 SHORTER | wie BARS kürzt: START (Anfang behalten) oder END (Ende behalten) |

Der Loop läuft im Tempo (Seite TEMPO oder MIDI-Clock). PLAY halten stoppt den Transport und damit die Loops; START beginnt wieder bei Takt 1. REC bei gestopptem Transport startet ihn neu. Nach dem Einschalten stehen alle Loops auf OFF; die gespeicherten Loops sind noch da, KNOB 1 schaltet sie wieder an.

### Step-Sequencer (Seite SEQ)

Ein Sequencer je Part mit bis zu 64 Schritten. Die Taste SEQ zeigt zuerst LOOP; noch einmal tippen: Seite SEQ.

1. Auf der Seite SEQ **REC** drücken: die Aufnahme für den gewählten Part beginnt.
2. Jede Taste ist der nächste Schritt. Eine Taste, während die vorige noch gehalten ist: gebunden (Legato). Die **oberste Taste**: Pause. **GLO + Taste**: die Note davor einen Schritt länger. MIDI-Noten auf dem Kanal des Parts zählen ebenso.
3. **REC** noch einmal: fertig, die Sequenz spielt.

Achtung: REC auf der Seite SEQ beginnt immer eine neue Sequenz; REC ohne gespielte Taste hinterlässt eine leere. Ein Partwechsel (ALGORITHM) beendet die Aufnahme.

KNOB 1 SEQ an/aus, 2 RATE (Notenwert), 3 GATE, 4 SHIFT (Transponieren). Solange die Sequenz spielt, transponieren die Tasten sie (C4 = keine Verschiebung). Sie läuft im Tempo bzw. mit der MIDI-Clock; PLAY halten startet (ab Schritt 1) und stoppt.

### Tempo und Clock (GLO, Seite TEMPO)

TEMPO 40–240 BPM. SOURCE: AUTO (einer ankommenden MIDI-Clock folgen, sonst eigenes Tempo), INT, MIDI. SEND: OFF oder USB (Clock und START/STOP über USB senden). Endet eine Clock, läuft das Gerät mit ihrem letzten Tempo weiter („TEMPO KEPT“).

## Speichern

- **SAVE kurz**: speichert den Sound des gewählten Parts auf seinen Platz. Ein Werks-Sound (auch ein iOS-Preset) bleibt unverändert; er wird als Kopie auf den nächsten freien eigenen Platz gelegt („SAVED: USER 03“). Ein geänderter INIT TONE öffnet den Dialog.
- **SAVE 1 s halten**: Speichern unter. KNOB 1 CURSOR, KNOB 2 LETTER, KNOB 3 GROUP, KNOB 4 PLACE; SAVE = OK, HOME = abbrechen.
- 40 Plätze für eigene Sounds, Namen bis 10 Zeichen. „MEMORY FULL“: kein Platz frei.
- Beim Speichern setzt der Klang kurz (etwa 50 ms) aus.
- **Der letzte Stand** (Sounds der Parts mit ungespeicherten Änderungen, Pegel, Kanäle, Arp, Effekte, Tempo, Part, Oktave, Seite), die Sequenzen und die Loops werden von selbst gesichert, aber nur in einer Spielpause: kein Ton, kein laufender Arp oder Loop, etwa 1 s Stille.

## Effekte

- **CHORUS** gehört zum Sound: CHORUS OFF, I, II; MODE JX, I+II, FREE; RATE auf der zweiten CHORUS-Seite.
- **Delay und Hall** gibt es einmal für das ganze Gerät. Jeder Sound schickt über seine Sends (Seite CHORUS: DELAY, REVERB) dorthin.
- DELAY: TIME (höchstens 0,6 s; mit SYNC ein Notenwert), FEEDBK, TONE, LEVEL, PING (Ping-Pong), MOD.
- REVERB: SIZE, DECAY, DAMP, LEVEL, TYPE (SPACE, ROOM, PLATE), MOD.

## Tipps

- Vor dem Ausschalten den Transport stoppen (PLAY halten) und zwei Sekunden warten, damit der letzte Stand gesichert ist.
- Klingt etwas fest: OCT − und OCT + zusammen drücken.
- Pitch Bend und Modulationsrad werden nicht ausgewertet.
- Notfall: OCT − beim Einschalten halten startet nur den Update-Modus (kein Klang); dann lässt sich eine Firmware neu aufspielen.
