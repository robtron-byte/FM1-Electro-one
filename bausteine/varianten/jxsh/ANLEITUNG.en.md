# JX-8P + SH-101

Firmware for the M-VAVE FM-1 (version 0.1.1-alpha, shown as `JX+SH`). Four parts A–D; each part plays either the **JX-8P** or the **SH-101**. All JX parts share a pool of **4 voices**; the SH-101 is **monophonic** and may run on at most one part. On top: one shared distortion with a send per part, delay and reverb, and per part an arpeggiator, a step sequencer and a live loop (LOOP).

## Operation overview

- **ALGORITHM** selects the part (A–D, highlighted at the top of the display). Keys, knobs and pages always act on the selected part; the pages follow its sound engine (JX-8P or SH-101).
- **KNOB 1–4** set the four fields at the bottom of the display. Left = less, right = more; a knob starts at the current value; turning fast takes bigger steps.
- **SELECT** steps through all pages, **PRESETS** chooses the sound of the selected part.
- **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO, SEL**: tap = the page of this button used last, tap again = its next page. **Hold** (EDIT, ENV, LFO, FX, ARP, SEQ, GLO) puts the four most important values on KNOB 1–4 (`HOLD: …` at the bottom).
- **OCT −** / **OCT +**: octave −3 to +3. Both together: all notes off (`ALL NOTES OFF`).
- **PLAY** tap: arp HOLD on/off. **PLAY** hold (0.6 s): start/stop the clock (when following MIDI clock: `FOLLOWS MIDI CLOCK`).
- **REC**: step recording on the SEQUENCER page, live recording (LOOP) everywhere else.
- **SAVE**: save the sound (see Saving). **HOME** long (1 s): bring back the edits of the sound you last left.
- Display top: the parts with sound names; small dot = sound edited, light dot = MIDI arriving, bars = sounding voices. Bottom: page name, dots for the pages of this button, octave.

## Pages

`—` = empty field. The pages are grouped by button here; SELECT steps through them in this order:

- **JX-8P part:** PLAY, PARTS, MIX, EDIT pages, ENV pages, LFO pages, CHORUS (2), ARP (2), VOICE (2), SEQUENCER, LOOP, DIST, DIST SEND, DELAY, DELAY 2, REVERB, REVERB 2, TEMPO, MIDI, SYSTEM, SOUNDS. So FX first leads to CHORUS, GLO first to VOICE.
- **SH-101 part:** PLAY, PARTS, MIX, EDIT, ENV, LFO pages, ARP, ARP 2, SEQUENCER, LOOP, DIST … SYSTEM, SOUNDS, finally SENDS (last FX page).

### Pages for both engines

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PARTS | A | B | C | D |
| HOME | MIX | A | B | C | D |
| SEQ | SEQUENCER | SEQ | RATE | GATE | SHIFT |
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

PARTS: sound engine per part. MIX: level per part (0–127). MIDI: channel per part (OMNI, 1–16). SYSTEM: LEVEL is the level of the selected part; also version and load of both cores.

### JX-8P part

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
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

JX values are shown as numbers. VOICE MODE: 2 = mono, 3 = unison, 0 and 1 play polyphonically.

### SH-101 part

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
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

Slider values show 0.0–10.0.

### Held buttons

| Hold | JX-8P part | SH-101 part |
|---|---|---|
| EDIT | X-MOD, 2 TUNE, MIX 1, MIX 2 | PULSE, SAW, SUB, PW |
| ENV | 1 ATTACK, 1 DECAY, 2 ATTACK, 2 RELEASE | ATTACK, DECAY, SUSTAIN, RELEASE |
| LFO | RATE, DELAY, PITCH, FILTER | RATE, MOD, VCF MOD, WAVE |
| FX | DELAY, REVERB (sends in the sound), TIME, CHORUS | DLY SEND, REV SEND, TIME, DIST (DIST send of this part) |
| ARP | ARP, MODE, RATE, OCT | ARP, MODE, RATE, OCT |
| SEQ | SEQ, RATE, GATE, SHIFT | SEQ, RATE, GATE, SHIFT |
| GLO | levels A, B, C, D | — |

## Sounds and presets

- Turn **PRESETS**: next or previous sound; at the end of a group it moves into the next one. Only sounds of the engine the selected part plays are offered.
- Groups in this order: INIT, **EIGENE** (the user's own JX-8P presets, e.g. STRINGS 01, HI STRI 01, SYNTH B 01), the JX factory groups (BASS, LEAD, PAD …), JX INIT, SH BASS, SH LEAD, SH SEQ, SH FX, finally OWN (all sounds you saved).
- **SEL** shows the sound list (page SOUNDS) with the group name at the top right; PRESETS scrolls through it.
- Without a saved state the unit starts with A STRINGS 01, B HI STRI 01, C SYNTH B 01 (JX-8P) and D SAW BASS (SH-101).
- Edited sound: dot in the header, `EDITED` on the page, SAVE blinks. If you switch sounds, **HOME long** brings the edits back (`EDITS BROUGHT BACK`).
- Pitch bend and mod wheel (MIDI) only affect the SH-101.

## Parts

- Page **PARTS** (HOME): KNOB 1–4 sets JX-8P or SH-101 for part A–D. On a change the part loads the INIT sound of the new engine. A second SH-101 is refused with `SH-101 IN USE`.
- The JX parts share 4 voices.
- Page **MIX**: levels A–D (start 100). The SH-101 is much louder than the JX-8P; balance with MIX.
- Page **MIDI** (GLO): receive channel per part, start A = 1, B = 2, C = 3, D = 4; OMNI = all channels. MIDI over USB and TRS: notes, pitch bend, CC 1, CC 120/123 (all notes off), clock, start/stop/continue, song position.
- The FM-1 keys play the selected part.

## Sequencer and live recording (LOOP)

**Clock** (GLO, page TEMPO): TEMPO 40–240 (start 120), CLOCK AUTO / INT / MIDI, SEND = send MIDI clock on/off. Hold PLAY to start and stop.

**Arpeggiator** (ARP, per part): ARP on/off, MODE (UP, DOWN, UP/DN, RAND, ORDER), RATE (1/32 to 1/1), OCT 1–4, GATE 5–100 %, HOLD (also tap PLAY).

**Step sequencer** (SEQ, per part, up to 64 steps):

1. Go to the SEQUENCER page, tap **REC** (`REC  KEYS = STEPS  REC = DONE`).
2. Each key is one step. A key pressed while the previous one is still held is tied. The top key enters a rest. **GLO** + key makes the previous note one step longer. MIDI notes are recorded too.
3. **REC** ends recording and the sequencer plays (`… STEPS: PLAYING`).

Tapping SEQ on the SEQUENCER page turns it on/off. While it runs, the keys transpose the sequence (C4 = unchanged, shown as SHIFT). RATE = note value, GATE = note length. After power-on every sequencer is off.

**Live recording LOOP** (per part, REC on any page except SEQUENCER):

1. **REC once**: recording starts at once, without count-in; the clock starts if it was stopped; the display shows the LOOP page.
2. **REC twice quickly** (within 0.4 s): one bar count-in, then recording. The count-in is silent; only the display shows `COUNT IN`.
3. **REC** again: the take is kept (`LOOP KEPT`). The first take sets the length, rounded to whole bars (1–16); after 16 bars it ends by itself. Then the loop plays.
4. If a loop exists, **REC** records on top (overdub): every finished pass stays, **REC** ends.
5. **Tap PLAY** during recording or count-in to discard the running pass (`PASS DROPPED`): on the first take the loop stays empty, in overdub only the running pass is removed.

Quantised fixed to 1/16, each note as long as held. Page LOOP: LOOP on/off (also tap SEQ on this page), BARS = length 0–16 bars (0 = empty, deletes the loop), LONGER = when lengthening, repeat the content (REPEAT) or add empty bars (EMPTY), SHORTER = when shortening, keep the start (START) or the end (END). The display shows bar/length. After power-on the loops are off; switch them back on with LOOP.

## Saving

- **SAVE short**: if the sound is one of your own, it is overwritten in its place (`SAVED`). Otherwise the dialog opens.
- **SAVE long** (1 s): `save as` dialog. KNOB 1 = CURSOR, KNOB 2 = LETTER (space, A–Z, 0–9, - + .), KNOB 3 = GROUP (only groups of the part's engine), KNOB 4 = PLACE (01–40). `THIS PLACE IS TAKEN: SAVE REPLACES IT` warns before overwriting. **SAVE** = OK, **HOME** = cancel. Names up to 10 characters; without a name `A NAME, PLEASE`.
- 40 places for your own sounds, shared by JX-8P and SH-101.
- Automatic: parts, sounds including unsaved edits, levels, channels, effects, arp, tempo and the page are stored 1.5 s after the last change during a pause in playing and restored at power-on. Step sequences and loops are stored as well when nothing is playing.

## Effects

- **DIST** (FX): one distortion for all parts. MODE TUBE / OD / DIST / RAT, DRIVE, TONE, LEVEL (start TUBE, 60, 70, 64).
- **DIST SEND** (FX): amount per part A–D, 0–127 (start 0 = off). The JX parts go into the distortion together: the highest DIST SEND of all JX parts applies. DRIVE on the SH page FM + DRIVE is the SH-101 sound's own overdrive.
- **DELAY / DELAY 2** (up to 0.6 s): TIME, FEEDBK, TONE, LEVEL; SYNC, NOTE, PING, MOD. **REVERB / REVERB 2**: SIZE, DECAY, DAMP, LEVEL; TYPE, MOD. Delay and reverb serve all parts and are on as soon as their LEVEL is above 0. On the very first start both LEVELs are 0.
- **Sends**: JX-8P parts have their sends in the sound (page CHORUS: DELAY, REVERB, plus CHORUS, MODE, RATE). SH-101 part: page SENDS, DLY SEND and REV SEND (start 40).

## Tips

- No delay or reverb: turn up LEVEL on DELAY or REVERB.
- Page SYSTEM shows the version and the load; `1K!` means the second core is not running and core 0 computes everything.
- Rescue: hold **OCT −** while switching on to start only the USB update mode (no sound). This way another firmware can always be installed.
