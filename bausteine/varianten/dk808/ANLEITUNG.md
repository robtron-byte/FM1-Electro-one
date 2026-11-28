# TR-808

Diese Firmware macht aus dem M-VAVE FM-1 einen Drum-Computer. Er wird bedient wie der Bereich **BEAT** von FM1-Electro, hat aber **15 Instrumente** des DrumKern:

- Bass Drum, Laser Gun, Snare, Rim Shot, Hand Clap, Cowbell
- zwei Toms (tief, hoch), zwei Congas (tief, hoch)
- Closed und Open Hi-Hat, Cymbal
- Claves, Maracas

Dazu kommen:

- vier Pattern-Plätze A–D mit bis zu zwei Takten
- Akzent, 32tel-Rolls, Swing, Fills
- ein Generator für neue Grooves
- Live-Aufnahme
- die **Punch-in-Effekte** von FM1-Electro
- eine Distortion als Send (TUBE, OD, DIST, RAT), Delay und Hall

Beide Rechenkerne arbeiten: Kern 0 spielt die meisten Instrumente und die Punch-ins, Kern 1 Open Hat, Becken, Delay, Hall und Distortion. Fassung 0.2.0-alpha.

## Überblick der Bedienung

| Bedienelement | Aufgabe |
|---|---|
| PLAY | Start / Stop |
| REC | Live-Aufnahme an/aus (EDIT + REC: Pattern löschen; REC 1 s halten: löschen und neu aufnehmen) |
| SELECT | Pattern-Platz A–D (wechselt am Ende des Patterns); GLO + SELECT: Instrument wählen, ohne es zu spielen |
| ALGORITHM | Tempo (1 BPM pro Raste); GLO + ALGORITHM: Länge 1 oder 2 Takte; EDIT + ALGORITHM: Swing (50–75) |
| PRESETS | Kit (TR-808, ELECTRO, MIAMI, KRAFT); EDIT + PRESETS: Werksklang des Kits |
| KNOB 1–4 | LEVEL und die drei Regler des gewählten Instruments |
| FX | nächste Regler-Seite: Instrument, SENDS, DIST, DELAY, REVERB |
| HOME | Übersicht: Plätze, Pegel und Sends |
| SEQ | Fill im nächsten Takt (gehalten: jeden Takt) |
| ARP gehalten | Generator |
| OCT + / OCT − | Vorschau an/aus / Step-Seite 1–16 und 17–32 |
| OCT − und OCT + zusammen | alles still |
| SAVE | alles speichern |
| EDIT, GLO | Umschalttasten (halten + andere Taste); GLO kurz getippt: Mute-Modus |

Grundregeln wie in FM1-Electro: Links drehen ist weniger, leiser, kürzer, zurück; rechts mehr, lauter, länger, weiter. Kurz drücken ist für Häufiges, EDIT oder GLO halten + Taste für Seltenes. Was passiert, steht kurz unten im Display.

## Das Display

Der Startbildschirm ist das Pattern: oben der Platz (A–D), der Name des Grooves, Tempo, Kit und PLAY/STOP. Darunter das Raster mit allen 15 Instrumenten und der Akzent-Zeile (AC). Das gewählte Instrument ist hervorgehoben, der laufende Step markiert. Die Zeichen im Raster:

- großes Feld: Akzent auf diesem Step
- dunkler Punkt: 32tel-Roll
- weißer Strich oben: Bedingung oder Wahrscheinlichkeit
- gelber Strich unten: eigene Velocity
- `M`: stummes Instrument

Unten stehen die vier Regler der aktuellen Seite.

## Tasten

**Schwarze Tasten 1–10** wählen ein Instrument und spielen es: BD, SD, RS, CP, CB, CY, OH, CH, LG (Laser Gun), CL (Claves). Die **11. schwarze Taste** wählt die Akzent-Zeile. Die übrigen fünf Instrumente (LT, HT, LC, HC, MA) wählt **GLO + SELECT**, dort sind alle 15 erreichbar.

**Weiße Tasten 1–16** setzen oder löschen die Steps des gewählten Instruments (bei zwei Takten: OCT − schaltet die Seite 1–16 / 17–32 um).

- **EDIT + weiße Taste:** Akzent auf dem Step.
- **GLO + weiße Taste:** 32tel-Roll.
- **EDIT + schwarze Taste:** das Instrument löschen.
- **GLO + schwarze Taste:** das Instrument stumm / laut.
- **GLO kurz tippen:** Mute-Modus. Dann schalten die schwarzen Tasten stumm/laut. Ihr Licht: voll = spielt und hat Schläge, schwach = nichts aufgenommen, blinkend = stumm. GLO noch einmal tippen beendet ihn.

**Step halten:**

| Regler | Wirkung |
|---|---|
| PRESETS | Durchläufe: 1:1, 1:2, 2:2, 1:4, 2:4, 3:4, 4:4 |
| ALGORITHM | Wahrscheinlichkeit 100 … 10 % |
| KNOB 2 | Velocity in 7 Stufen (der Step klingt zur Probe) |

Ein leerer Step wird beim Drücken gesetzt. Ein gesetzter Step geht beim Loslassen aus, wenn dabei nichts gedreht wurde. Eine Tonhöhe je Step gibt es hier nicht: Der DrumKern hat dafür keinen Eingang.

**Vorschau (OCT +):** Aus, wählt eine schwarze Taste nur das Instrument. Bei laufender Aufnahme nimmt sie dann ohne Ton auf.

## Regler-Seiten (FX)

| Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| Instrument (z. B. BD) | LEVEL | TONE / TUNE / DEPTH | DECAY | SWEEP / SNAPPY / BP FREQ |
| SENDS | DLY SEND | REV SEND | DST SEND | LEVEL |
| DIST | MODE (TUBE OD DIST RAT) | DRIVE | TONE | LEVEL |
| DELAY | TIME (bis 600 MS) | FEEDBACK | TONE | LEVEL |
| REVERB | SIZE | DECAY | DAMP | LEVEL |

Die Regler der Instrumente sind die des DrumKern:

| Instrument | Regler |
|---|---|
| BD | TONE DECAY SWEEP |
| LG | DEPTH DECAY SWEEP |
| SD | TONE DECAY SNAPPY |
| RS | TUNE DECAY |
| CP | TONE REVERB |
| Toms | TUNE DECAY SNAPPY |
| Congas | TUNE DECAY |
| CH, OH, CY | TONE DECAY BP FREQ |
| CL | TUNE DECAY |
| MA | TONE |
| CB | TUNE DECAY |

Ein Regler startet am aktuellen Wert. Schnell gedreht macht er größere Schritte.

## Kits

**PRESETS** wechselt das Kit; es gilt für alle Plätze:

- **TR-808:** der Werksklang des DrumKern.
- **ELECTRO:** längere Bassdrum, knackige Snare, kurze Hats.
- **MIAMI:** lange, tiefe Bassdrum, dunklere Snare.
- **KRAFT:** kurze, klickige Bassdrum, hohe Snare.

Jedes Kit merkt sich, wie es zuletzt eingestellt war. **EDIT + PRESETS** holt den Werksklang des Kits zurück.

## Plätze, Länge, Swing

- **SELECT:** Platz A–D. Der neue Platz beginnt am Ende des laufenden Patterns; im Kopf blinkt er gelb, bis er spielt.
- **HOME:** die Plätze groß. Weiße Tasten 1–4 wählen A–D. Einen Platz halten und weitere dazu drücken ergibt eine **Kette** (bis 4, z. B. `A>B>B`). Ein leerer Platz bekommt dabei eine Kopie. Ein einfacher Druck auf einen Platz löst die Kette. Schwarze Taste 1: alles stumm / laut.
- **GLO + ALGORITHM:** Länge 1 oder 2 Takte. Wird ein Pattern länger, wiederholt sich der erste Takt.
- **EDIT + ALGORITHM:** Swing 50 (gerade) bis 75.

## Fills

- **SEQ tippen:** Im nächsten Takt spielt der Fill des Platzes.
- **SEQ halten:** Fill jeden Takt, solange gehalten.
- **GLO + SEQ:** Fill von selbst alle 8 / 16 / 32 Takte / aus.
- **EDIT + SEQ:** Fill bearbeiten. Die Step-Tasten 1–16 zeigen dann seinen Takt; EDIT + SEQ noch einmal beendet das.

Ein unbenutzter Fill entsteht aus dem Groove (ein Lauf auf Snare oder Clap zum Ende des Takts).

## Generator

**ARP halten:**

| Regler / Taste | Wirkung |
|---|---|
| KNOB 1 | Stil (ELECTRO, BRONX 82, NUMBERS, TRANS EUR, TOUR, COMPUTER, ROTHER, MIAMI, RECKLESS, BOOM BASS, DROWNED, ROBOT, PERFECT, EGYPT, FREAKY, LOVE) |
| KNOB 2 | Stärke der Abwandlung |
| KNOB 3 | Kick behalten / neu |
| weiße Taste 1 | Variation des Patterns |
| weiße Taste 2 | neues Pattern im Stil |

Zur Bestätigung dieselbe Taste innerhalb von 3 s noch einmal drücken. Die Stile sind die von FM1-Electro; ihre Tom-Zeilen (tief, hoch) spielen hier mit.

## Punch-in-Effekte

Wie in FM1-Electro: In **HOME** die **schwarzen Tasten 5–11** halten. Solange eine gehalten wird, klingt ihr Effekt; loslassen beendet ihn. Mehrere gehen zugleich. Solange einer gehalten wird, stellen die **weißen Tasten 1–6** seinen Wert ein (die Tasten zeigen den Wert). Im Display stehen die gehaltenen Effekte als Kacheln mit ihrem Wert.

| Taste | Effekt | weiße Tasten 1–6 |
|---|---|---|
| 5 | STUTTER: die Summe im Notenwert zerhackt | 1/4 1/8 1/8T 1/16 1/16T 1/32 |
| 6 | REVERB THROW: die Summe blendet über zwei Schläge in den Hall aus | – |
| 7 | TAPE STOP: das Band bleibt stehen | 1/16 1/8 3/16 1/4 3/8 1/2 |
| 8 | REPEAT: ein Step des Takts wiederholt sich jedes Sechzehntel | weiße Taste 1–16 = der Step (erst damit beginnt es) |
| 9 | DELAY: Dub-Echo, losgelassen klingt es aus | 1/4 1/8 1/8T 1/16 1/16T 1/32 |
| 10 | BITCRUSH: weniger Bits und Rate | 12 10 8 6 5 4 Bit |
| 11 | PHASER: Sweep je Länge | 1/4 1/2 1 2 4 8 Takte |

REVERB THROW öffnet den Hall, auch wenn er ab Werk zu ist; 4 s nach dem Loslassen gilt wieder die eigene Einstellung.

## Live-Aufnahme

- **REC:** Aufnahme an. Läuft das Pattern (PLAY), landen gespielte Instrumente auf dem nächsten Sechzehntel, und zwar die schwarzen Tasten und MIDI-Noten. Ein kräftiger Schlag (Velocity über 90 %) setzt einen Akzent. **REC** noch einmal: aus.
- **REC 1 s halten:** Pattern löschen und neu aufnehmen.
- **EDIT + REC:** das Pattern löschen.

## MIDI

Noten über USB und TRS auf allen Kanälen:

| Note | Instrument |
|---|---|
| 36 | BD |
| 37 | RS |
| 38 | SD |
| 39 | CP |
| 41 | LT |
| 42 | CH |
| 43 | HT |
| 46 | OH |
| 49 | CY |
| 56 | CB |
| 58 | LG |
| 62 | HC |
| 64 | LC |
| 70 | MA |
| 75 | CL |

Weitere Noten folgen der Drum-Map des DrumKern. Eine ankommende MIDI-Clock bestimmt Tempo und Start/Stop.

## Speichern

- **SAVE:** alles speichern, nämlich die Plätze A–D mit allen Step-Werten, die Fills, das Kit mit seinen Einstellungen, Tempo, Effekte und Generator.
- Von selbst wird 2 s nach einer Änderung gespeichert, aber nur, wenn gestoppt ist (das Schreiben setzt den Klang kurz aus). Vor dem Ausschalten also anhalten und kurz warten.
- Stände der Fassung 0.1.0 werden nicht übernommen; 0.2.0 beginnt mit dem Grundgroove (ROTHER auf Platz A).

## Effekte

- **Distortion als Send:** DST SEND (Seite SENDS) schickt die 808 in die Distortion. MODE TUBE, OD, DIST oder RAT, dazu DRIVE, TONE und LEVEL (Seite DIST). In HOME mit EDIT gehalten: KNOB 1 der Send, PRESETS der Modus.
- **Delay und Hall:** DLY SEND und REV SEND (Seite SENDS), dazu die Seiten DELAY und REVERB. Ab Werk ist beides zu. Delay höchstens 0,6 s.

## Wo 808 solo von BEAT abweicht

- **17 → 15 Instrumente statt 10:** Die schwarzen Tasten 1–10 sind dieselben wie in BEAT, nur LG (Laser Gun) und CL (Claves) liegen auf den Plätzen von ZAP und PULSE. Die übrigen fünf (Toms, Congas, Maracas) wählt GLO + SELECT, weil es nicht mehr Tasten gibt. Mid Tom und Mid Conga sind gestrichen (Wunsch des Nutzers): Das spart Rechenzeit und Auswahl.
- **FX statt vier Bereichstasten:** 808 solo hat nur einen Bereich. FX blättert durch Instrument, SENDS, DIST, DELAY und REVERB, die in FM1-Electro unter HOME + GLO / EDIT liegen. SEL, ENV und LFO haben keine Aufgabe.
- **HOME:** Statt der vier Bereiche zeigt HOME die Plätze A–D. KNOB 1–4 sind Pegel und Sends der 808; die Punch-ins liegen wie dort auf den schwarzen Tasten 5–11.
- **Keine Tonhöhe je Step** (KNOB 1 beim gehaltenen Step): Der DrumKern hat dafür keinen Eingang.
- **Kein OPTIONS-Menü:** MIDI hört auf allen Kanälen, die Clock folgt einer ankommenden MIDI-Clock.
- **Kits:** Die vier Kits sind Abwandlungen des DrumKern-Klangs, kein PO-12.

## Tipps

- Diese Firmware teilt sich die Kennung FM-1_91 mit der Kombi 808 + 101. Jede erkennt ihre gespeicherten Daten an einer eigenen Speicherkennung.
- Rettung: **OCT −** beim Einschalten halten startet nur den Update-Modus (kein Klang), um eine Firmware neu aufzuspielen.
