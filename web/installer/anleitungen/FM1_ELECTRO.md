# FM1-Electro

FM1-Electro macht das M-VAVE FM-1 zu einer Electro-Groovebox mit vier Bereichen, die zusammen im selben Tempo laufen: **BEAT** (808-Drums mit zehn Instrumenten und fünf Kits: TR-808, ELECTRO, MIAMI, KRAFT, PO-12), **RHYTHM** (3 Stimmen), **STRINGS** (3 Stimmen, Akkorde) und **BASS** (1 Stimme, mono). Die Synth-Bereiche spielen Sounds der Engines JX-8P oder JP-6. Jeder Bereich hat vier Pattern-Plätze A–D. Stand: 0.5.6-alpha.

## Überblick der Bedienung

| Bedienelement | Aufgabe |
|---|---|
| MASTER | Lautstärke |
| SELECT | Pattern-Platz A–D (in HOME: alle Bereiche zugleich) |
| ALGORITHM | Tempo (1 BPM pro Raste) |
| PRESETS | Sound, in BEAT das Kit |
| KNOB 1–4 | Klangregler der aktuellen Seite |
| FX / SEL / ENV / LFO | Bereich BEAT / RHYTHM / STRINGS / BASS; noch einmal drücken: nächste Seite |
| HOME | Übersicht und Mixer (noch einmal: zurück) |
| PLAY | Start / Stop |
| REC | Aufnahme in den aktuellen Bereich |
| SAVE | alles speichern |
| ARP | kurz: Arpeggiator an/aus; gehalten: Generator |
| SEQ | Synth-Bereiche: STEP-Modus an/aus; BEAT und HOME: Fill |
| OCT − / OCT + | Synths: Oktave; BEAT: OCT + Vorschau an/aus, OCT − Step-Seite 1–16 / 17–32 |
| EDIT, GLO | Umschalttasten (halten + andere Taste); kurz getippt mit eigener Aufgabe |

Grundregeln:

- Links drehen = weniger, leiser, kürzer, zurück; rechts = mehr, lauter, länger, weiter.
- Links sitzt die Struktur (SELECT, ALGORITHM, PRESETS), rechts der Klang (KNOB 1–4).
- Kurz drücken für Häufiges, EDIT oder GLO halten + Taste für Seltenes. Was passiert, steht kurz unten im Display.
- EDIT kurz tippen (in RHYTHM, STRINGS, BASS): Synth-Editor auf/zu. GLO kurz tippen in BEAT: Mute-Modus an/aus.
- GLO + Bereichstaste: Bereich stumm/laut. EDIT + Bereichstaste: Editor dieses Bereichs.
- Schwarze Tasten: in BEAT die Instrumente, sonst Noten. Weiße Tasten: Steps oder Noten, in HOME die Plätze.

## Seiten

| Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| HOME | Pegel BEAT | Pegel RHYTHM | Pegel STRINGS | Pegel BASS |
| HOME + GLO halten | `DLY TIME` | `FEEDBACK` | `REV SIZE` | `DECAY` |
| HOME + EDIT halten | Distortion-Send BEAT | Send RHYTHM | Send STRINGS | Send BASS |
| BEAT | Regler des gewählten Instruments, je vier pro Seite (z. B. Level, Tune, Decay, Tone; dann Pan, Rev, Dly, Drive) | | | |
| `FILTER` | `CUTOFF` | `RESO` | `VCF ENV` | `VCF LFO` |
| `ENVELOPE` | `2 ATTACK` | `2 DECAY` | `2 SUST` | `2 REL` |
| `OSC` | `1 WAVE` | `2 WAVE` | `X-MOD` | `DCO2` |
| `MOD` | `LFO RATE` | `1 LFO` | `GLIDE` | `1 DECAY` |
| `FX` | `CHORUS` | `DELAY` | `REVERB` | `VOLUME` |
| `ARP` | `MODE` | `RATE` | `OCTAVES` | `GATE` |

Die Seiten `FILTER` bis `ARP` gelten für RHYTHM, STRINGS und BASS; die Bereichstaste blättert weiter. Ein JP-6-Sound zeigt auf denselben Seiten seine eigenen Regler (Namen im Display).

**Synth-Editor:** EDIT tippen (oder EDIT + Bereichstaste). SELECT wählt die Sektion (`DC1 DC2 MIX VCF VCA EN1 EN2 LFO KEY FX`), ALGORITHM die Seite, KNOB 1–4 die Regler, PRESETS den Sound; die Tasten spielen weiter. EDIT tippen oder die Bereichstaste: zurück.

**OPTIONS:** HOME halten + SAVE. SELECT oder ALGORITHM wählt die Zeile, PRESETS oder KNOB 1–4 den Wert, HOME oder SAVE schließt. Zeilen: MIDI-Kanal je Bereich (ab Werk BEAT 10, RHYTHM 1, STRINGS 2, BASS 3), `MIDI CLOCK` (FOLLOW / IGNORE), `MASTER FX`, Kompressor (`COMP …`), `DIST BASS DRUM`, `OUTPUT LEVEL`, `POWER-ON` (LAST / SLOTS A), `HELP LINE`. Unten rechts steht die Version.

## Sounds und Presets

- PRESETS blättert durch die Sounds des Bereichs: ganz links deine eigenen (`U01 …`), dann die Werks-Sounds (JX-8P, JP-6, `INIT RHY` / `INIT STR` / `INIT BASS`).
- GLO + PRESETS: nur durch die eigenen Sounds. EDIT + PRESETS: zur anderen Engine (JX-8P / JP-6).
- RHYTHM, STRINGS und BASS merken sich den Sound je Platz A–D. BEAT hat ein Kit für alle Plätze; EDIT + PRESETS holt in BEAT den Werksklang des Kits zurück.

Eigenen Sound speichern (32 Plätze):

1. Sound mit KNOB 1–4 oder im Editor einstellen.
2. GLO + SAVE (im Editor reicht SAVE).
3. Platz mit PRESETS (oder SELECT) wählen, Name mit KNOB 1 (Stelle) und KNOB 2 (Buchstabe).
4. SAVE speichert (`SOUND SAVED AS U01`). Jede andere Taste bricht ab.

## Sequencer und Live-Aufnahme

**BEAT:** schwarze Taste 1–10 wählt das Instrument (BD SD RS CP CB CY OH CH ZAP PULSE), weiße Tasten setzen oder löschen Steps. Die 11. schwarze Taste wählt die Akzent-Zeile.

- EDIT + weiße Taste: Akzent; GLO + weiße Taste: 32tel-Roll.
- EDIT + schwarze Taste: Instrument löschen; GLO + schwarze Taste: stumm.
- EDIT + ALGORITHM: Swing. GLO + SELECT: Instrument wählen, ohne es zu spielen.

**Step halten** (BEAT oder STEP-Modus): PRESETS = Durchläufe (`1:1`, `1:2` … `4:4`), ALGORITHM = Wahrscheinlichkeit (100 … 10 %); bei Drums zusätzlich KNOB 1 = Tonhöhe (±12 Halbtöne), KNOB 2 = Velocity.

**RHYTHM / STRINGS / BASS, Tasten-Modus:** die Tasten spielen den Sound, OCT −/+ die Oktave. In RHYTHM und STRINGS wählt GLO + weiße Taste 1–9 einen Akkord (1 = `SINGLE NOTES`). BASS spielt nur Einzeltöne.

**STEP-Modus (SEQ):** weiße Tasten schalten Steps, EDIT + weiß = Akzent, GLO + weiß = Tie, OCT −/+ = Steps 1–16 / 17–32, GLO + SELECT wählt den Step. KNOB 1 Note, KNOB 2 Oktave, KNOB 3 Akkord (STRINGS) bzw. Akzent, KNOB 4 Umkehrung (STRINGS) bzw. Tie.

**Live-Aufnahme** (Standard): REC drücken, der Groove muss laufen (PLAY). Gespielte Noten landen auf dem nächsten Step, dazu wird weiter aufgenommen (Overdub). REC noch einmal: fertig.

- Leeres STRINGS- oder BASS-Pattern: die Aufnahme beginnt mit der ersten Taste (ihr Takt wird Takt 1). STRINGS rastet auf Achtel, BASS auf Sechzehntel; jede Note klingt so lange wie gehalten. Am Ende wird auf ganze Takte gekürzt (1, 2, 4, 8). Läuft sie länger, wird das Raster gröber: STRINGS bis 8 Takte, BASS bis 4.
- In STRINGS werden zusammen gegriffene Tasten als Akkord gespeichert (solange `SINGLE NOTES` gewählt ist).
- REC eine Sekunde halten: Pattern löschen und neu aufnehmen. EDIT + REC: nur löschen.

**Step-Aufnahme wie SH-101:** GLO + REC schaltet den Bereich zwischen `LIVE` und `SH-101 STEP` um. Dann REC, jede Taste schreibt den nächsten Step (bis 32). Taste gedrückt, während die vorige noch gehalten wird = Legato; die letzte (höchste) Taste = Pause; GLO + Taste = vorige Note einen Step länger. REC: fertig.

**TRANSPOSE:** GLO + SEQ, dann verschiebt eine Taste den Bereich (C4 = 0), EDIT + Taste RHYTHM, STRINGS und BASS zusammen (Drums nie). Der Wert steht oben rechts (z. B. `T+3`) und wird gespeichert. GLO + SEQ noch einmal: Tasten spielen wieder.

**Pattern-Länge:** GLO + ALGORITHM, in ganzen Takten (BEAT 1–2 Takte; STRINGS und BASS 1, 2, 4, 8). Einzelne Steps nur im STEP-Modus. EDIT + ALGORITHM: in STRINGS die Step-Länge, in RHYTHM und BASS die Notenlänge.

**Plätze und Ketten:** SELECT wechselt den Platz A–D (BEAT am Ende seines Patterns, die Synths zum nächsten Takt). In HOME: weiße Tasten 1–4 BEAT A–D, 5–8 RHYTHM, 9–12 STRINGS, 13–16 BASS. Einen Platz halten und weitere derselben Bank drücken ergibt eine Kette (bis 4, z. B. `A>B>B`); ein leerer Platz bekommt dabei eine Kopie. Einfacher Druck auf einen Platz löst die Kette.

**Fills (BEAT und HOME):** SEQ = Fill im nächsten Takt, SEQ halten = wiederholen. GLO + SEQ: automatisch alle 8 / 16 / 32 Takte / aus. EDIT + SEQ in BEAT: Fill bearbeiten (Step-Tasten 1–16), EDIT + SEQ: fertig.

**Generator:** ARP halten, KNOB 1 Stil, KNOB 2 Stärke (BEAT: KNOB 3 Kick behalten / neu). Weiße Taste 1 = Variation, Taste 2 = neues Pattern; zur Bestätigung dieselbe Taste innerhalb von 3 s noch einmal. In HOME wirkt das auf alle Bereiche. HOME, ARP + 16. weiße Taste zweimal: leerer Start (erst SAVE macht ihn dauerhaft).

## Speichern

- SAVE speichert alles: alle Plätze, Sounds, Kit, Ketten, Einstellungen (`SAVED`).
- Vom Generator erzeugte Grooves werden kurz danach von selbst gesichert, sobald der Groove steht.
- Geänderte OPTIONS werden beim Schließen gespeichert, während der Groove läuft erst bei STOP.
- Fragt der Web-Installer ein Update an, speichert FM1-Electro vorher alles (`SAVING BEFORE UPDATE`).

## Effekte

- **Hall und Delay:** in HOME regelt PRESETS den Hall-Anteil, GLO + PRESETS den Delay-Anteil; GLO + KNOB 1–4 formt sie. Jeder Synth-Sound hat eigene Sends (Seite `FX`), jedes Drum-Instrument Rev und Dly.
- **Distortion:** HOME, EDIT halten: KNOB 1–4 Send je Bereich, PRESETS Typ (OFF OD DIST RAT TUBE), ALGORITHM Drive, SELECT Tone. Bei OFF sind die Sends gesperrt.
- **Live-Effekte (Punch-ins)** in HOME, solange gehalten: schwarze Taste 5–11 = STUTTER, REVERB THROW, TAPE STOP, REPEAT, DELAY, BITCRUSH, PHASER (kombinierbar). Dabei stellen die weißen Tasten 1–6 Notenwert oder Stärke ein; bei REPEAT wählt eine weiße Taste 1–16 den Step.
- **Master:** Kompressor und Master-FX in OPTIONS; COMP RATIO ganz rechts = `CLUB`.

## Tipps

- **Rettung:** OCT − beim Einschalten halten startet nur den Update-Modus (kein Ton); der Web-Installer funktioniert dann weiter.
- Updates über den Web-Installer behalten Grooves und eigene Sounds; der M-Flasher löscht sie sehr wahrscheinlich.
