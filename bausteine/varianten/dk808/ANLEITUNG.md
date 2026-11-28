# TR-808

Diese Firmware macht aus dem M-VAVE FM-1 eine 808-Drum-Machine mit allen 17 Instrumenten des DrumKern: Bass Drum, Laser Gun, Snare, Rim Shot, Hand Clap, drei Congas, drei Toms, Closed und Open Hi-Hat, Cymbal, Claves, Maracas und Cowbell. Dazu kommen ein Step-Sequencer mit 16 Steps je Instrument und einer Akzent-Zeile, die Live-Aufnahme (LOOP), eine Distortion als Send (TUBE, OD, DIST, RAT), Delay und Hall. Beide Kerne rechnen: Kern 0 die meisten Instrumente, Kern 1 Hi-Hats, Becken und die Effekte. Fassung 0.1.0-alpha.

## Überblick der Bedienung

- **KNOB 1–4** stellen die vier Felder unten im Display ein. Regler starten am aktuellen Wert; schnell drehen springt weiter.
- **SELECT** blättert durch alle Seiten, **PRESETS** wählt das Kit.
- **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO** kurz tippen: die erste Seite der Taste, noch einmal tippen: die nächste. Die Leuchte zeigt die aktive Taste.
- Taste **halten** (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): unten erscheint `HOLD: …`, KNOB 1–4 stellen dann vier wichtige Werte dieser Taste ein.
- **SEL**: die Kit-Liste.
- **PLAY** halten: Start und Stop. Der Sequencer und die Schleife laufen mit.
- Regel überall: links = weniger, leiser, kürzer; rechts = mehr, lauter, länger.
- **SAVE** lang = Speichern unter, **HOME** 1 s halten = Änderungen am zuletzt verlassenen Kit zurückholen.
- Regler gehen von 0 bis 100 (Distortion: DRIVE, TONE, LEVEL 0 bis 127).

## Tasten

Außerhalb der Seite STEPS spielen die Tasten die Instrumente (Anschlagstärke 100):

- **Weiße Tasten 1–16:** BD, LG, SD, RS, CP, LC, MC, HC, LT, MT, HT, CH, OH, CY, CL, MA.
- **Schwarze Tasten 1–11:** BD, SD, RS, CP, CB, CY, OH, CH, LT, MT, HT. Die Cowbell liegt nur hier.
- **MIDI** (USB und TRS) spielt nach der Drum-Belegung des DrumKern: 36 BD, 37 RS, 38 SD, 39 CP, 41 LT, 42 CH, 43 HT, 46 OH, 47 MT, 49 CY, 56 CB, 58 LG, 62 HC, 63 MC, 64 LC, 70 MA, 75 CL, mit Anschlag.

## Seiten

| Taste | Seite | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (Name, Oszilloskop) | BD Pegel | SD Pegel | CH Pegel | OH Pegel |
| HOME | MIX | CP Pegel | RS Pegel | CY Pegel | CB Pegel |
| EDIT | BASS DRUM | LEVEL | TONE | DECAY | SWEEP |
| EDIT | LASER GUN | LEVEL | DEPTH | DECAY | SWEEP |
| EDIT | SNARE DRUM | LEVEL | TONE | DECAY | SNAPPY |
| EDIT | RIM SHOT | LEVEL | TUNE | DECAY | |
| EDIT | HAND CLAP | LEVEL | TONE | REVERB | |
| ENV | LOW/MID/HI CONGA | LEVEL | TUNE | DECAY | |
| ENV | LOW/MID/HI TOM | LEVEL | TUNE | DECAY | SNAPPY |
| LFO | CLOSED HAT, OPEN HAT, CYMBAL | LEVEL | TONE | DECAY | BP FREQ |
| ARP | CLAVES | LEVEL | TUNE | DECAY | |
| ARP | MARACAS | LEVEL | TONE | | |
| ARP | COW BELL | LEVEL | TUNE | DECAY | |
| FX | SENDS | DLY SEND | REV SEND | DST SEND | DLY LVL |
| FX | DIST | MODE (TUBE OD DIST RAT) | DRIVE | TONE | LEVEL |
| FX | DELAY | TIME | FEEDBACK | TONE | PING |
| FX | DELAY 2 | SYNC | NOTE | MOD | |
| FX | REVERB | SIZE | DECAY | DAMP | REV LVL |
| FX | CHOKE | HAT | CONGA | TOM | |
| SEQ | STEPS (Raster) | INSTR | LENGTH | | |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| GLO | TEMPO | TEMPO | CLOCK (AUTO INT MIDI) | SEND | MIDI CH |
| GLO | SYSTEM (Version, Last) | LEVEL | | | |
| SEL | SOUNDS (Liste) | | | | |

Jedes Instrument hat eine eigene Seite; die Tasten EDIT, ENV, LFO und ARP blättern durch ihre Instrumente.

Gehaltene Tasten:

| Taste halten | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | BD Pegel | SD Pegel | CH Pegel | OH Pegel |
| ENV | LT Pegel | MT Pegel | HT Pegel | MC Pegel |
| LFO | BD DECAY | SD DECAY | OH DECAY | CY DECAY |
| FX | DLY SEND | REV SEND | DST SEND | DRIVE |
| ARP | CL Pegel | MA Pegel | CB Pegel | CP Pegel |
| SEQ | INSTR | LENGTH | | |
| GLO | CP Pegel | RS Pegel | CY Pegel | CB Pegel |

CHOKE: HAT an (Standard) = Closed Hat dämpft die Open Hat. CONGA und TOM an = eine Conga bzw. ein Tom dämpft die anderen.

## Sounds und Presets

- Ein Sound ist ein **Kit**: Pegel und Regler aller 17 Instrumente, Choke, Distortion-Send und die Distortion selbst.
- Werkskits: **808 KIT** (die Grundeinstellung des DrumKern) und **808 TUBE** (dasselbe Kit, Distortion-Send 40).
- 40 Plätze für eigene Kits. **PRESETS** drehen wechselt das Kit, das Pattern bleibt.

## Sequencer (Seite STEPS)

Die Taste **SEQ** öffnet die Seite STEPS. Das Display zeigt alle 17 Instrumente und die Akzent-Zeile (unten) als Raster mit 16 Steps. Die gewählte Zeile ist hell, der laufende Step ist markiert.

- **Schwarze Taste:** Instrument wählen und hören (BD, SD, RS, CP, CB, CY, OH, CH, LT, MT, HT).
- **KNOB 1 (INSTR):** jedes der 17 Instrumente oder `ACC` (Akzent) wählen.
- **Weiße Taste 1–16:** Step des gewählten Instruments setzen oder löschen.
- **EDIT + weiße Taste:** Akzent auf diesem Step (alle Instrumente des Steps mit voller Anschlagstärke 127, sonst 100).
- **EDIT + schwarze Taste:** die Zeile dieses Instruments löschen.
- **KNOB 2 (LENGTH):** Länge des Patterns, 1 bis 16 Steps.
- Die Tasten-Leuchten zeigen die Steps der gewählten Zeile (mit EDIT gehalten: die Akzente), der laufende Step blinkt.
- **PLAY** halten startet und stoppt. Der Sequencer läuft in Sechzehnteln im Tempo (GLO, TEMPO) oder nach der MIDI-Clock.
- Beim ersten Start steht ein Grundmuster da: BD auf 1, 9, 11, SD auf 5, 13, CH auf den Achteln.

**Live-Aufnahme LOOP** (REC auf jeder Seite):

- **REC** einmal tippen: Aufnahme beginnt sofort; das Display springt auf die Seite LOOP. Steht der Takt, läuft er wieder an.
- **REC** zweimal schnell: ein Takt Einzählen, dann Aufnahme. **REC** während der Aufnahme: übernehmen. **PLAY** tippen: den laufenden Durchgang verwerfen.
- Die erste Aufnahme legt die Länge fest (ganze Takte, höchstens 16). Danach nimmt REC dazu auf (Overdub). Quantisiert auf 1/16.
- Aufgenommen werden die gespielten Tasten und MIDI-Noten. Die Schleife spielt zusammen mit dem Step-Pattern.
- Seite LOOP: **LOOP** an/aus, **BARS** = Länge (`EMPTY` = löschen), **LONGER** (REPEAT/EMPTY), **SHORTER** (START/END). **SEQ** tippen auf der Seite LOOP schaltet die Schleife an/aus.

## Speichern

- **SAVE** kurz: Ein eigenes Kit wird auf seinem Platz überschrieben. Bei einem Werkskit öffnet sich „Speichern unter“.
- **SAVE** lang (1 s): Speichern unter. KNOB 1 = CURSOR, KNOB 2 = LETTER, KNOB 3 = GROUP, KNOB 4 = PLACE (01–40). **SAVE** = OK, **HOME** = Abbruch.
- Automatisch gespeichert (ohne SAVE): der Stand beim Ausschalten (Kit mit Änderungen, Seite, Tempo, Clock, MIDI-Kanal, Effekte), das Step-Pattern und die Schleife. Das passiert etwa 1,5 s nach einer Änderung, aber nur in einer Spielpause. Vor dem Ausschalten also anhalten und kurz warten.

## Effekte

- **Distortion als Send:** DST SEND (im Kit) schickt die ganze 808 anteilig in die Distortion. Modus TUBE, OD, DIST oder RAT, dazu DRIVE, TONE und LEVEL (Seite DIST, im Kit). Sie rechnet auf Kern 1.
- **Delay und Hall:** DLY SEND und REV SEND auf der Seite SENDS. Ab Werk sind beide zu, ebenso DLY LVL und REV LVL. Seiten DELAY, DELAY 2, REVERB wie in den anderen Firmwares. Delay höchstens 0,6 s.

## Tipps

- MIDI-Kanal unter GLO, TEMPO, MIDI CH (`OMNI` = alle). CLOCK `AUTO` folgt einer eingehenden MIDI-Clock, `INT` nie, `MIDI` immer; SEND gibt die Clock aus.
- Diese Firmware teilt sich die Kennung FM-1_91 mit der späteren Kombi 808 + 101. Jede erkennt ihre gespeicherten Daten an einer eigenen Speicherkennung; nach einem Wechsel beginnt sie mit ihren Werkseinstellungen.
- Rettung: **OCT −** beim Einschalten halten startet nur den Update-Modus (kein Klang), um eine Firmware neu aufzuspielen.
