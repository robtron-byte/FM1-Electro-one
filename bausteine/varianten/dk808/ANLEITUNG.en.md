# TR-808

This firmware turns the M-VAVE FM-1 into an 808 drum machine with all 17 instruments of the DrumKern: bass drum, laser gun, snare, rim shot, hand clap, three congas, three toms, closed and open hi-hat, cymbal, claves, maracas and cowbell. It adds a step sequencer with 16 steps per instrument and an accent row, live recording (LOOP), distortion as a send (TUBE, OD, DIST, RAT), delay and reverb. Both cores work: core 0 runs most instruments, core 1 the hi-hats, cymbal and the effects. Version 0.1.0-alpha.

## Controls at a glance

- **KNOB 1–4** set the four fields at the bottom of the display. Knobs start from the current value; turning fast takes bigger steps.
- **SELECT** steps through all pages, **PRESETS** chooses the kit.
- Tap **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO**: the first page of that button; tap again: the next one. The light shows the active button.
- **Hold** a button (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): `HOLD: …` appears at the bottom and KNOB 1–4 set four important values of that button.
- **SEL**: the kit list.
- Hold **PLAY**: start and stop. The sequencer and the loop follow.
- Everywhere: left = less, quieter, shorter; right = more, louder, longer.
- Long **SAVE** = save as, hold **HOME** for 1 s = bring back the edits of the kit you last left.
- Knobs run from 0 to 100 (distortion DRIVE, TONE, LEVEL 0 to 127).

## Keys

Outside the STEPS page the keys play the instruments (velocity 100):

- **White keys 1–16:** BD, LG, SD, RS, CP, LC, MC, HC, LT, MT, HT, CH, OH, CY, CL, MA.
- **Black keys 1–11:** BD, SD, RS, CP, CB, CY, OH, CH, LT, MT, HT. The cowbell is only here.
- **MIDI** (USB and TRS) follows the DrumKern drum map: 36 BD, 37 RS, 38 SD, 39 CP, 41 LT, 42 CH, 43 HT, 46 OH, 47 MT, 49 CY, 56 CB, 58 LG, 62 HC, 63 MC, 64 LC, 70 MA, 75 CL, with velocity.

## Pages

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (name, scope) | BD level | SD level | CH level | OH level |
| HOME | MIX | CP level | RS level | CY level | CB level |
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
| SEQ | STEPS (grid) | INSTR | LENGTH | | |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| GLO | TEMPO | TEMPO | CLOCK (AUTO INT MIDI) | SEND | MIDI CH |
| GLO | SYSTEM (version, load) | LEVEL | | | |
| SEL | SOUNDS (list) | | | | |

Every instrument has its own page; EDIT, ENV, LFO and ARP step through their instruments.

Held buttons:

| Hold | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | BD level | SD level | CH level | OH level |
| ENV | LT level | MT level | HT level | MC level |
| LFO | BD DECAY | SD DECAY | OH DECAY | CY DECAY |
| FX | DLY SEND | REV SEND | DST SEND | DRIVE |
| ARP | CL level | MA level | CB level | CP level |
| SEQ | INSTR | LENGTH | | |
| GLO | CP level | RS level | CY level | CB level |

CHOKE: HAT on (default) = the closed hat cuts the open hat. CONGA and TOM on = one conga or tom cuts the others.

## Sounds and presets

- A sound is a **kit**: level and knobs of all 17 instruments, choke, distortion send and the distortion itself.
- Factory kits: **808 KIT** (the DrumKern defaults) and **808 TUBE** (the same kit, distortion send 40).
- 40 places for your own kits. Turning **PRESETS** changes the kit; the pattern stays.

## Sequencer (STEPS page)

**SEQ** opens the STEPS page. The display shows all 17 instruments and the accent row (bottom) as a grid of 16 steps. The selected row is bright, the running step is marked.

- **Black key:** select and hear an instrument (BD, SD, RS, CP, CB, CY, OH, CH, LT, MT, HT).
- **KNOB 1 (INSTR):** select any of the 17 instruments or `ACC` (accent).
- **White key 1–16:** set or clear a step of the selected instrument.
- **EDIT + white key:** accent on that step (all instruments of the step at full velocity 127, otherwise 100).
- **EDIT + black key:** clear that instrument's row.
- **KNOB 2 (LENGTH):** pattern length, 1 to 16 steps.
- The key lights show the steps of the selected row (with EDIT held: the accents); the running step blinks.
- Hold **PLAY** to start and stop. The sequencer runs in sixteenths at the tempo (GLO, TEMPO) or follows MIDI clock.
- On first start a basic groove is there: BD on 1, 9, 11, SD on 5, 13, CH on the eighths.

**Live recording LOOP** (REC on any page):

- Tap **REC** once: recording starts at once; the display jumps to the LOOP page. If the clock is stopped, it starts again.
- Tap **REC** twice quickly: one bar count-in, then recording. **REC** while recording: keep. Tap **PLAY**: drop the running pass.
- The first recording sets the length (whole bars, at most 16). After that REC overdubs. Quantised to 1/16.
- Recorded are the keys you play and MIDI notes. The loop plays together with the step pattern.
- LOOP page: **LOOP** on/off, **BARS** = length (`EMPTY` = clear), **LONGER** (REPEAT/EMPTY), **SHORTER** (START/END). Tap **SEQ** on the LOOP page to switch the loop on/off.

## Saving

- Short **SAVE**: your own kit is overwritten in its place. A factory kit opens "save as".
- Long **SAVE** (1 s): save as. KNOB 1 = CURSOR, KNOB 2 = LETTER, KNOB 3 = GROUP, KNOB 4 = PLACE (01–40). **SAVE** = OK, **HOME** = cancel.
- Saved automatically (without SAVE): the state at power-off (kit with edits, page, tempo, clock, MIDI channel, effects), the step pattern and the loop. This happens about 1.5 s after a change, but only while nothing plays. So stop and wait a moment before switching off.

## Effects

- **Distortion as a send:** DST SEND (in the kit) sends the whole 808 into the distortion. Mode TUBE, OD, DIST or RAT, plus DRIVE, TONE and LEVEL (DIST page, in the kit). It runs on core 1.
- **Delay and reverb:** DLY SEND and REV SEND on the SENDS page. From the factory both are closed, as are DLY LVL and REV LVL. DELAY, DELAY 2, REVERB pages as in the other firmwares. Delay at most 0.6 s.

## Tips

- MIDI channel under GLO, TEMPO, MIDI CH (`OMNI` = all). CLOCK `AUTO` follows incoming MIDI clock, `INT` never, `MIDI` always; SEND outputs the clock.
- This firmware shares the identity FM-1_91 with the later 808 + 101 combo. Each recognises its saved data by its own storage identity; after switching it starts with its factory settings.
- Rescue: holding **OCT −** at power-on starts only the update mode (no sound), to install a firmware again.
