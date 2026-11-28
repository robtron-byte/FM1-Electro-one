# SH-101

This firmware turns the M-VAVE FM-1 into an SH-101: a monophonic synthesizer (one voice) with VCO, sub oscillator, noise, filter, envelope, LFO, glide, arpeggiator, step sequencer, live recording (LOOP), distortion, delay and reverb. Version 0.1.4-alpha.

## Controls at a glance

- **KNOB 1–4** set the four fields at the bottom of the display. Knobs start from the current value; turning fast takes bigger steps.
- **SELECT** steps through all pages, **PRESETS** chooses the sound.
- Tap **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO**: the first page of that button; tap again: the next one. The light shows the active button.
- **Hold** a button (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): `HOLD: …` appears at the bottom and KNOB 1–4 set the four most important values of that button.
- **SEL**: the sound list.
- **OCT −** / **OCT +**: octave −3 to +3. Both together: all notes off.
- Everywhere: left = less, quieter, shorter; right = more, louder, longer.
- Short taps for frequent things, long holds for rare ones: hold **PLAY** = start/stop, hold **SAVE** = save as, hold **HOME** for 1 s = bring back the edits of the sound you last left.
- The top of the display shows the sound; a dot and `EDITED` mean unsaved changes (SAVE then blinks). Green bars show the sounding voice.
- The keys play at a fixed velocity of 100; MIDI notes keep their velocity.

## Pages

Knobs 0.0 to 10.0 like the SH-101 sliders.

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (name, oscilloscope) | CUTOFF | RESO | ENV | DECAY |
| HOME | PERFORM | GLIDE | DRIVE | VOLUME | TRANSP |
| EDIT | VCO | RANGE (16' 8' 4' 2') | MOD | PW | PWM SRC (ENV MAN LFO) |
| EDIT | SOURCE | PULSE | SAW | SUB | NOISE |
| EDIT | SOURCE 2 | SUB OCT | WHITE | FINE | FLOOR |
| EDIT | VCF | CUTOFF | RESO | ENV | KYBD |
| EDIT | VCF 2 | VCF MOD | SLOPE (24DB 12DB) | ENV FULL | RES COMP |
| EDIT | FM + DRIVE | FM SRC (OFF PULSE SAW SUB NOISE) | FM AMT | DRIVE | VOLUME |
| ENV | ENV (picture) | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | VCA | VCA (GATE ENV) | TRIGGER (LFO GATE G+TRG) | DECLICK | |
| ENV | VELOCITY | VEL VCA | VEL VCF | | |
| LFO | LFO | RATE | WAVE (TRI SQR RAND NOISE) | MOD | VCF MOD |
| LFO | LFO 2 | SYNC | INVERT | KEY TRIG | SNAP |
| LFO | CONTROL | BEND VCO | BEND VCF | MOD WHL | |
| LFO | GLIDE | GLIDE | MODE (AUTO OFF ON) | LINEAR | TRANSP |
| FX | EFFECTS | DLY SEND | REV SEND | DELAY | REVERB |
| FX | DIST | DST SEND | MODE (TUBE OD DIST RAT) | DRIVE | TONE |
| FX | DIST 2 | LEVEL | | | |
| FX | DELAY | TIME | FEEDBACK | TONE | PING |
| FX | DELAY 2 | SYNC | NOTE | MOD | |
| FX | REVERB | SIZE | DECAY | DAMP | TYPE (SPACE ROOM PLATE) |
| ARP | ARP | ARP | MODE | RATE | OCT |
| ARP | ARP 2 | GATE | HOLD | | |
| SEQ | SEQUENCER | SEQ | RATE | GATE | SHIFT |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| GLO | TEMPO | TEMPO | CLOCK (AUTO INT MIDI) | SEND | MIDI CH |
| GLO | SYSTEM (version, load) | LEVEL | | | |
| SEL | SOUNDS (list) | | | | |

Held buttons:

| Hold | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | PULSE | SAW | SUB | PW |
| ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| LFO | RATE | MOD | VCF MOD | WAVE |
| FX | DLY SEND | REV SEND | DST SEND | TIME |
| ARP | ARP | MODE | RATE | OCT |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | GLIDE | DRIVE | TRANSP | VOLUME |

Held keys and tied steps play legato; with GLIDE mode AUTO they glide like on the SH-101.

## Sounds and presets

- Own factory settings in four groups: **BASS** (SAW BASS, SUB BASS, ACID, PULSE BASS, SQUELCH), **LEAD** (LEAD, PWM LEAD, WHISTLE), **SEQUENCE** (PLUCK, BLIP, AUTO TRIG), **FX** (NOISE HAT, SIREN, RANDOM). Plus **INIT** (saw and sub full, filter open).
- 40 places for your own sounds.
- Turn **PRESETS**: right = next sound, left = previous; at the end of a group it moves on to the next group. Order: INIT, BASS, LEAD, SEQUENCE, FX, then OWN (all your own). Your own sounds also appear in the group they were saved under.
- **SEL** shows the list of the current group.
- If you leave an edited sound without saving, hold **HOME** for 1 s to bring the edits back.

## Sequencer and live recording (LOOP)

**Arpeggiator** (ARP button): ARP on/off, MODE (UP DOWN UP/DN RAND ORDER), RATE (note value), OCT (1–4), GATE, HOLD. A short tap on **PLAY** toggles HOLD.

**Step sequencer** (SEQUENCER page, up to 64 steps):

1. Go to the SEQUENCER page with **SEQ** and tap **REC**.
2. Each key is one step. Pressing a key while the previous one is still held = tied (legato). The top key = rest. **GLO** + key = the previous note one step longer. MIDI notes are recorded the same way.
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

- **SAVE** short: one of your own sounds is overwritten in its place (`SAVED`). For a factory sound, "save as" opens.
- **SAVE** long (1 s): save as. KNOB 1 = CURSOR, KNOB 2 = LETTER (space, A–Z, 0–9, `-`, `+`, `.`), KNOB 3 = GROUP, KNOB 4 = PLACE (01–40). The display warns when the place is taken. **SAVE** = OK, **HOME** = cancel. Name up to 10 characters, not empty.
- Stored in the sound: all sound values including the distortion (DIST, DIST 2) and DRIVE.
- Saved automatically (no SAVE needed): the state at power-off (sound with edits, page, octave, tempo, clock, MIDI channel, effects, sends, arp), the sequences and the loops. This happens about 1.5 s after a change, but only during a pause in playing. So before switching off, stop (arp, sequencer, loop) and wait a moment.
- After switching on, the sequencer and LOOP are off; switch them on with SEQ or on their page.

## Effects

- **Distortion** (FX, pages DIST and DIST 2): a send effect with DST SEND, MODE (TUBE OD DIST RAT), DRIVE, TONE, LEVEL. Part of the sound.
- **DRIVE** (PERFORM, FM + DRIVE): the overdrive of the voice itself, part of the sound.
- **Delay and reverb**: DLY SEND and REV SEND (amount), DELAY and REVERB (effect levels) on the EFFECTS page; DELAY: TIME, FEEDBACK, TONE, PING; DELAY 2: SYNC (to tempo), NOTE (note value), MOD; REVERB: SIZE, DECAY, DAMP, TYPE. Delay at most 0.6 s. These values are global, not part of the sound.
- On the very first start DELAY and REVERB are at 0: turn them up on the EFFECTS page to hear them.

## Tips

- To change page when SEQ switches on/off on its page: turn **SELECT**.
- MIDI over USB and TRS: channel under GLO, TEMPO, MIDI CH (`OMNI` = all). Pitch bend and mod wheel (CC 1) act through BEND VCO, BEND VCF, MOD WHL. CLOCK `AUTO` follows an incoming MIDI clock, `INT` never, `MIDI` always; SEND outputs the clock.
- Rescue: hold **OCT −** while switching on to start only the update mode (no sound) and install a firmware again.
