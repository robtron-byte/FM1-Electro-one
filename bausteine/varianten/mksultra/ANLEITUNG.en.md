# MKS Ultra

This firmware turns the M-VAVE FM-1 into the MKS Ultra, an Alpha Juno recreation (Alpha Juno / MKS-50) with DCO, sub oscillator, noise, filter with high-pass, an envelope with four times and three levels, LFO, unison, chorus, EQ, delay, reverb and the sound engine's own arpeggiator. This version plays **one voice** (monophonic). On top come a step sequencer and live recording (LOOP). Version 0.1.0-alpha.

## Controls at a glance

- **KNOB 1–4** set the four fields at the bottom of the display. Knobs start from the current value; turning fast takes bigger steps.
- **SELECT** steps through all pages, **PRESETS** chooses the sound.
- Tap **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO**: the first page of that button; tap again: the next one. The light shows the active button.
- **Hold** a button (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): `HOLD: …` appears at the bottom and KNOB 1–4 set the four most important values of that button.
- **SEL**: the sound list.
- **OCT −** / **OCT +**: octave −3 to +3. Both together: all notes off.
- Everywhere: left = less, quieter, shorter; right = more, louder, longer.
- Short taps for frequent things, long holds for rare ones: hold **PLAY** = start/stop, hold **SAVE** = save as, hold **HOME** for 1 s = bring back the edits of the sound you last left.
- The top of the display shows the sound; a dot and `EDITED` mean unsaved changes (SAVE then blinks).
- Knobs show 0 to 100; switches show their step as a number.
- The keys play at a fixed velocity of 100; MIDI notes keep their velocity.

## Pages

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (name, oscilloscope) | CUTOFF | RESO | ENV | RELEASE |
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
| ENV | ENV (picture) | T1 | L1 | T2 | L2 |
| ENV | ENV 2 (picture) | T3 | L3 | T4 | KEY FLW |
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
| GLO | SYSTEM (version, load) | LEVEL | | | |
| SEL | SOUNDS (list) | | | | |

The firmware is built with one voice: VOICES above 1 has no effect in this version.

Held buttons:

| Hold | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | PULSE | SAW | SUB | PWM |
| ENV | T1 | T3 | L3 | T4 |
| LFO | RATE | DELAY | PITCH | FILTER |
| FX | CHORUS | DELAY | TIME | REVERB |
| ARP | MODE | RATE | RANGE | SYNC |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | VOLUME | GLIDE | OCTAVE | VOICES |

## Sounds and presets

- 213 presets in five groups: **RON** (the 6 own presets, first: 041 Lead 6 short ron, Bass Hit 01, Micro Ticks, R2 Random, Short Hit, Stringer), **BANK A** (64), **BANK B** (70), **MKS** (53), **VESSEL** (20). Plus **INIT** (the preset "default").
- After the very first start, the first RON preset plays.
- 36 places for your own sounds.
- Turn **PRESETS**: right = next sound, left = previous; at the end of a group it moves on to the next group. Order: INIT, RON, BANK A, BANK B, MKS, VESSEL, then OWN (all your own). Your own sounds also appear in the group they were saved under.
- **SEL** shows the list of the current group.
- If you leave an edited sound without saving, hold **HOME** for 1 s to bring the edits back.

## Sequencer and live recording (LOOP)

**Arpeggiator** (ARP button): the sound engine's arpeggiator, part of the sound. MODE 0 = off, higher values switch it on; RATE, RANGE, SYNC.

**Step sequencer** (SEQUENCER page, up to 64 steps):

1. Go to the SEQUENCER page with **SEQ** and tap **REC**.
2. Each key is one step. Pressing a key while the previous one is still held = tied. The top key = rest. **GLO** + key = the previous note one step longer. MIDI notes are recorded the same way.
3. **REC** ends the recording; the sequence plays at once.

- Tapping **SEQ** on the SEQUENCER page switches the sequencer on/off (once a sequence exists). The SEQ light shows it is running.
- While it runs, the keys transpose the sequence (C4 = unchanged); SHIFT shows the transposition (−24 to +24).
- RATE = note value of a step, GATE = note length.

**Live recording LOOP** (REC on any page except SEQUENCER):

- Tap **REC** once: recording starts at once and the display jumps to the LOOP page. If the clock was stopped, it starts again.
- Tap **REC** twice quickly (within 0.4 s): one bar count-in (display `COUNT IN`), then recording.
- **REC** while recording: keep it (`LOOP KEPT`).
- Tap **PLAY** while recording: the current pass is dropped (`PASS DROPPED`); during the count-in it cancels.
- The first recording into an empty loop sets its length: rounded to whole bars, at most 16 bars (then it ends by itself). The loop starts when you press REC, or after the count-in.
- If a loop exists, REC adds to it (overdub). Every complete pass stays; PLAY drops only the pass that is running. New notes are heard from the next pass on.
- Quantised to 1/16, each note as long as it was held. Recorded are the keys you play and MIDI notes; while the sequencer runs, the keys only transpose and are not recorded.
- LOOP page: **LOOP** on/off, **BARS** = length (`EMPTY` = 0 = erase the loop, otherwise 1–16), **LONGER** = how lengthening fills (`REPEAT` repeats the content, `EMPTY` leaves it empty), **SHORTER** = what stays when shortening (`START` or `END`).
- Tapping **SEQ** on the LOOP page switches the loop on/off once it has bars.
- The loop follows start/stop (hold **PLAY**); start plays it from the beginning.
- REC blinks while recording.

## Saving

- **SAVE** short: one of your own sounds is overwritten in its place (`SAVED`). For a preset, "save as" opens.
- **SAVE** long (1 s): save as. KNOB 1 = CURSOR, KNOB 2 = LETTER (space, A–Z, 0–9, `-`, `+`, `.`), KNOB 3 = GROUP, KNOB 4 = PLACE (01–36). The display warns when the place is taken. **SAVE** = OK, **HOME** = cancel. Name up to 10 characters, not empty.
- Stored in the sound: all sound values including chorus, EQ, delay, reverb and arpeggiator.
- Saved automatically (no SAVE needed): the state at power-off (sound with edits, page, octave, tempo, clock, MIDI channel), the sequences and the loops. This happens about 1.5 s after a change, but only during a pause in playing. So before switching off, stop (sequencer, loop) and wait a moment.
- After switching on, the sequencer and LOOP are off; switch them on with SEQ or on their page.

## Effects

All effects are part of the sound and come from the sound engine:

- **Chorus**: CHORUS on/off, RATE (page CHORUS + EQ; CHORUS also on PERFORM).
- **EQ** (one bell filter): EQ FREQ, EQ GAIN (−24 to +24), EQ Q (page DELAY 2).
- **Delay**: MIX, TIME, FEEDBK, SYNC (to tempo), SPREAD, TONE.
- **Reverb**: MIX, SIZE, PREDLY, TONE.

## Tips

- To change page when SEQ switches on/off on its page: turn **SELECT**.
- MIDI over USB and TRS: channel under GLO, TEMPO, MIDI CH (`OMNI` = all). Pitch bend and mod wheel (CC 1) go to the sound (BEND, MW VCF, MW PITCH, MW WAH). CLOCK `AUTO` follows an incoming MIDI clock, `INT` never, `MIDI` always; SEND outputs the clock.
- Rescue: hold **OCT −** while switching on to start only the update mode (no sound) and install a firmware again.
