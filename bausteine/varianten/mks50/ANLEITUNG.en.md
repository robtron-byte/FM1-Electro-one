# MKS-50

Firmware for the M-VAVE FM-1 with the sound of the Roland MKS-50 (version 0.2.8-alpha), continued from the fm1-synths firmware. Four parts (A–D) play at once, each with its own sound, level and MIDI channel; they share a pool of **4 voices**. Also on board: arpeggiator, chord mode, step sequencer, live loop, delay, reverb and 8 sets. Updating from the fm1-synths version keeps your own sounds, sets, sequences and the last state.

## Controls at a glance

| Control | Action |
|---|---|
| KNOB 1–4 | the four cells at the bottom of the display |
| SELECT | page back / forward (all pages sit in one row) |
| PRESETS | sound back / forward; past the end of a group into the next one |
| ALGORITHM | select part A–D |
| OCT − / OCT + | octave of the keys (−3 … +3) |
| OCT − and OCT + together | all notes off |
| Note keys | play the selected part |

The rule everywhere: left = less / back, right = more / forward. A knob starts at the current value; turned fast it takes bigger steps (on values with many steps).

Page buttons (EDIT, ENV, LFO, FX, ARP, SEQ, GLO):

- **tap** opens the button's pages; tap again: its next page. Coming from another button, the page last open there comes back.
- **hold** puts the four most important values on KNOB 1–4 at once, whatever page is shown (footer "HOLD: …"). Let go: the knobs belong to the page again.

| Hold button | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | PULSE | SAW | SUB | PW |
| ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| LFO | RATE | DELAY | PITCH | FILTER |
| FX | delay send | reverb send | delay TIME | CHORUS |
| ARP | ARP | MODE | RATE | CHORD |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | level A | level B | level C | level D |

Other buttons:

| Button | short | held |
|---|---|---|
| HOME | pages PLAY and MIX; in the save dialog: cancel | 1 s: bring back the edits dropped by the last sound change |
| SEL | pages SOUNDS and SETS | |
| SAVE | save the sound (on SETS: save a set); in the dialog: OK | 1 s: save as |
| PLAY | arp HOLD of the part on/off; during a live recording: drop the pass | 0.6 s: transport START / STOP |
| REC | live loop; on the page SEQ: step recording | |

The top of the display always shows all four parts (sound, sounding voices, a red dot for unsaved changes); the bottom shows the page name and the octave. SAVE blinks while there are unsaved changes, REC blinks while recording, PLAY flashes on the beat while the transport runs.

## Pages

In the order SELECT steps through them:

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY | CUTOFF | RESO | ENV | RELEASE |
| HOME | MIX | level A | level B | level C | level D |
| EDIT | OSC | PULSE | SAW | SUB | RANGE |
| EDIT | OSC | SUB LVL | NOISE | PW | PWM RT |
| EDIT | OSC | LFO | ENV | ENV MD | |
| EDIT | FILTER | CUTOFF | RESO | ENV | KEY |
| EDIT | FILTER | LFO | ENV MD | HPF | |
| EDIT | AMP | LEVEL | MODE | VOLUME | |
| ENV | ENV | T1 | L1 | T2 | L2 |
| ENV | ENV | T3 | L3 | T4 | KEY |
| LFO | LFO | RATE | DELAY | WAVE | SYNC |
| LFO | LFO | KEYSYNC | | | |
| FX | CHORUS | CHORUS | RATE | DELAY (send) | REVERB (send) |
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
| GLO | MIDI | channel A | channel B | channel C | channel D |
| GLO | VOICE | MODE | GLIDE | TIME | SHIFT |
| GLO | VOICE | RETRIG | UNI DET | DRIFT | |
| GLO | SYNC | DETUNE | SYNC | SYN ENV | |
| GLO | SYSTEM | (display: version, load per core, voices) | | | |
| SEL | SOUNDS | GROUP | | | |
| SEL | SETS | SET | LOAD | | |

Notes:

- PW and PWM RT only work when PULSE or SAW is set to 3; otherwise they are grey and the display says "PW WORKS ON PULSE 3 + SAW 3".
- LFO RATE is shown in Hz; with SYNC on, RATE sets a note value.
- CHORD: OFF or a chord (m7, SUS2, MIN, 5+8, SUS4, 7TH, MAJ, m7b5); one key then plays the whole chord. MODE: POLY, CHORD, MONO, UNISON.
- ARP MODE: UP, DOWN, UP/DN, RAND, ORDER; OCTAVES 1–4.
- MIDI channels: OMNI or 1–16, factory setting A = 1, B = 2, C = 3, D = 4.

## Sounds and presets

Sounds are sorted into groups. KNOB 1 (GROUP) on the page SOUNDS jumps to the start of a group; PRESETS steps sound by sound. Order of the groups:

1. **INIT**: INIT TONE (saw, filter open), the starting point for your own sounds.
2. **EIGENE** (own): first the 23 presets from the MKS-50 iOS app, then the sounds saved on the device.
3. The 56 factory sounds in BASS, SEQUENCE, ZAPS, STRINGS, STAB, KEYS, PAD, FX+PERC. Own sounds and the iOS presets also appear in their group.

A changed sound carries a red dot until it is saved. If you change the sound with unsaved edits, HOME (hold 1 s) brings them back.

## Parts and sets

ALGORITHM selects part A–D; the keys play the selected part, MIDI plays each part on its channel. Levels on MIX (or hold GLO), channels on MIDI.

A **set** stores the whole setup: all four parts with their sounds as they sound right now (unsaved edits included), channel, level and arp, plus delay, reverb, tempo and clock. 8 places. Sequences and loops are not part of it.

1. Tap SEL twice: page SETS.
2. KNOB 1 chooses the place.
3. KNOB 2 to the right loads the set; to the left brings back the state before loading (once).
4. To save: press SAVE on the page SETS. KNOB 1 cursor, KNOB 2 letter, KNOB 4 place; SAVE = OK, HOME = cancel. A used place is replaced.

## Sequencer and live recording

### Live loop (page LOOP)

One loop per part, for the selected part:

- **Tap REC once**: recording starts at once, the display jumps to LOOP.
- **Tap REC twice quickly**: one bar of count-in ("COUNT IN"), then recording.
- **REC again**: the recording is kept ("LOOP KEPT").
- **Tap PLAY** while recording: the running pass is dropped ("PASS DROPPED").

The first recording into an empty loop sets its length: whole bars, at most 16 (at 16 bars it ends by itself). Then the loop plays; every further recording adds to it (overdub). Each finished pass stays, PLAY only drops the running one. Notes are quantised to 1/16 and play as long as they were held.

Page LOOP:

| KNOB | Value |
|---|---|
| 1 LOOP | the part's loop ON / OFF |
| 2 BARS | length in bars; 0 = EMPTY (loop erased) |
| 3 LONGER | how BARS lengthens: REPEAT (repeat the content) or EMPTY (empty bars) |
| 4 SHORTER | how BARS shortens: START (keep the beginning) or END (keep the end) |

The loop runs at the tempo (page TEMPO or MIDI clock). Holding PLAY stops the transport and with it the loops; START begins again at bar 1. REC with the transport stopped starts it again. After power-on all loops are OFF; the saved loops are still there, KNOB 1 switches them back on.

### Step sequencer (page SEQ)

One sequencer per part with up to 64 steps. The SEQ button shows LOOP first; tap again: page SEQ.

1. On the page SEQ press **REC**: recording starts for the selected part.
2. Every key is the next step. A key pressed while the one before is still held: tied (legato). The **top key**: a rest. **GLO + key**: the note before one step longer. MIDI notes on the part's channel count as well.
3. **REC** again: done, the sequence plays.

REC on the page SEQ starts a new sequence as soon as a key is played. REC and REC again without playing a key keeps the old sequence ("NOTHING RECORDED: SEQ KEPT"). To clear: **hold EDIT + REC** on the page SEQ ("SEQ CLEARED"). Changing the part (ALGORITHM) ends the recording.

KNOB 1 SEQ on/off, 2 RATE (note value), 3 GATE, 4 SHIFT (transpose). While the sequence plays, the keys transpose it (C4 = no shift). It runs at the tempo or with the MIDI clock; holding PLAY starts (from step 1) and stops.

### Tempo and clock (GLO, page TEMPO)

TEMPO 40–240 BPM. SOURCE: AUTO (follow an incoming MIDI clock, else own tempo), INT, MIDI. SEND: OFF or USB (send clock and START/STOP over USB). When a clock ends, the device goes on at its last tempo ("TEMPO KEPT").

## Saving

- **SAVE short**: saves the selected part's sound to its place. A factory sound (an iOS preset too) stays unchanged; it is copied to the next free own place ("SAVED: USER 03"). A changed INIT TONE opens the dialog.
- **Hold SAVE 1 s**: save as. KNOB 1 CURSOR, KNOB 2 LETTER, KNOB 3 GROUP, KNOB 4 PLACE; SAVE = OK, HOME = cancel.
- 48 places for own sounds, names up to 10 characters. "MEMORY FULL": no place free.
- While saving, the sound drops out briefly (about 50 ms).
- **The last state** (the parts' sounds with unsaved edits, levels, channels, arp, effects, tempo, part, octave, page), the sequences and the loops are stored automatically, but only in a pause in playing: no note, no running arp or loop, about 1 s of silence.

## Effects

- **CHORUS** belongs to the sound: OFF, FREE, I, II, I+II; RATE for FREE.
- **Delay and reverb** exist once for the whole device. Each sound feeds them through its sends (page CHORUS: DELAY, REVERB).
- DELAY: TIME (up to 600 MS; with SYNC a note value, then also at most 0.6 s), FEEDBK, TONE, LEVEL, PING (ping-pong), MOD.
- REVERB: SIZE, DECAY, DAMP, LEVEL, TYPE (SPACE, ROOM, PLATE), MOD.

## Tips

- Before switching off, stop the transport (hold PLAY) and wait two seconds so the last state is stored.
- Something hangs: press OCT − and OCT + together.
- Pitch bend and the modulation wheel are not used.
- Rescue: holding OCT − while switching on starts only the update mode (no sound); a firmware can then be installed again.
