# JP-6

This firmware turns the M-VAVE FM-1 into a JP-6 modelled on the Jupiter-6: two VCOs with cross-mod and sync, a filter (low-pass, band-pass, high-pass) plus an extra high-pass (HPF), two envelopes, LFO, chorus, chord and unison modes. **4 voices**. On top come an arpeggiator, step sequencer, live recording (LOOP), delay and reverb. No distortion. Version 0.1.1-alpha.

## Controls at a glance

- **KNOB 1–4** set the four fields at the bottom of the display. Knobs start from the current value; turning fast takes bigger steps.
- **SELECT** steps through all pages, **PRESETS** chooses the sound.
- Tap **HOME, EDIT, ENV, LFO, FX, ARP, SEQ, GLO**: the first page of that button; tap again: the next one. The light shows the active button.
- **Hold** a button (EDIT, ENV, LFO, FX, ARP, SEQ, GLO): `HOLD: …` appears at the bottom and KNOB 1–4 set the four most important values of that button.
- **SEL**: the sound list.
- **OCT −** / **OCT +**: octave −3 to +3. Both together: all notes off.
- Everywhere: left = less, quieter, shorter; right = more, louder, longer.
- Short taps for frequent things, long holds for rare ones: hold **PLAY** = start/stop, hold **SAVE** = save as, hold **HOME** for 1 s = bring back the edits of the sound you last left.
- The top of the display shows the sound; a dot and `EDITED` mean unsaved changes (SAVE then blinks). Green bars show the sounding voices.
- Knobs run from 0 to 127. The keys play at a fixed velocity of 100; MIDI notes keep their velocity.

## Pages

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|---|
| HOME | PLAY (name, oscilloscope) | CUTOFF | RESO | ENV | RELEASE |
| HOME | PERFORM | CHORUS | DELAY | REVERB | VOLUME |
| EDIT | VCO-1 | TRI | SAW | PULSE | SQUARE |
| EDIT | VCO-1 MOD | RANGE (32' to 2') | X-MOD | XMD ENV | SYNC |
| EDIT | VCO-2 | TRI | SAW | PULSE | NOISE |
| EDIT | VCO-2 TUNE | RANGE (LO, 32' to 2') | TUNE (−12 to +12) | FINE | MIX |
| EDIT | VCO MOD | LFO | ENV | DEST (VCO-1 VCO-2 BOTH) | DRIFT |
| EDIT | PWM | PW/PWM | SOURCE (LFO MAN ENV-1) | | |
| EDIT | VCF | CUTOFF | RESO | ENV | MODE (LPF BPF HPF) |
| EDIT | VCF MOD | HPF (0 = OFF) | ENV SRC (ENV-1 ENV-2) | LFO | KEY |
| EDIT | VCA | LEVEL | LFO | VCF VEL | VCA VEL |
| ENV | ENV-1 (picture) | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV-2 (picture) | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV | ENV KEY | 1 KEY | 1 POL (NORM INV) | 2 KEY | RETRIG (MULTI LEGATO) |
| LFO | LFO | RATE | DELAY | WAVE (TRI SINE SAW UP SAW DN SQUARE S&H RANDOM) | SYNC |
| LFO | LFO 2 | NOTE | KEYSYNC | | |
| LFO | VOICE | MODE (POLY CHORD MONO UNISON) | PORTA (OFF ON AUTO) | GLIDE | SHIFT |
| LFO | VOICE 2 | UNI DET | CHORD | | |
| FX | CHORUS | CHORUS | MODE (FREE I II I+II) | RATE | |
| FX | SENDS | DELAY | REVERB | DLY LVL | REV LVL |
| FX | DELAY | TIME | FEEDBACK | TONE | PING |
| FX | DELAY 2 | SYNC | NOTE | MOD | |
| FX | REVERB | SIZE | DECAY | DAMP | TYPE (SPACE ROOM PLATE) |
| ARP | ARP | ARP | MODE | RATE | OCT |
| ARP | ARP 2 | GATE | HOLD | | |
| SEQ | LOOP | LOOP | BARS | LONGER | SHORTER |
| SEQ | SEQUENCER | SEQ | RATE | GATE | SHIFT |
| GLO | TEMPO | TEMPO | CLOCK (AUTO INT MIDI) | SEND | MIDI CH |
| GLO | SYSTEM (version, load) | LEVEL | | | |
| SEL | SOUNDS (list) | | | | |

CHORD (page VOICE 2) chooses the chord for the CHORD mode: m7, SUS2, MIN, 5+8, SUS4, 7TH, MAJ, m7b5.

Held buttons:

| Hold | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| EDIT | X-MOD | 2 TUNE | MIX | PW/PWM |
| ENV | 1 ATT | 1 DEC | 2 ATT | 2 REL |
| LFO | RATE | DELAY | PITCH | FILTER |
| FX | DELAY | REVERB | TIME | CHORUS |
| ARP | ARP | MODE | RATE | OCT |
| SEQ | SEQ | RATE | GATE | SHIFT |
| GLO | MODE | GLIDE | SHIFT | VOLUME |

## Sounds and presets

- 79 sounds in groups: **EIGENE** (the 5 own presets, first: 12 VCO 01, JUPI UN 01, JUPI UN 02, JUPI BA 01, INIT TO 01), then the factory sounds of the JP-6 app: **BASS** (10), **LEAD** (9), **SEQUENCE** (10), **ZAPS** (6), **STAB** (7), **KEYS** (5), **STR+BRASS** (14), **PAD** (6), **FX+PERC** (7). Plus **INIT** (the app's INIT TONE).
- After the very first start, 12 VCO 01 plays.
- 40 places for your own sounds.
- Turn **PRESETS**: right = next sound, left = previous; at the end of a group it moves on to the next group. Order: INIT, the groups as above, then OWN (all your own). Your own sounds also appear in the group they were saved under.
- **SEL** shows the list of the current group.
- If you leave an edited sound without saving, hold **HOME** for 1 s to bring the edits back.

## Sequencer and live recording (LOOP)

**Arpeggiator** (ARP button): ARP on/off, MODE (UP DOWN UP/DN RAND ORDER), RATE (note value), OCT (1–4), GATE, HOLD. A short tap on **PLAY** toggles HOLD.

**Step sequencer** (SEQUENCER page, up to 64 steps; under the SEQ button the second page, after LOOP):

1. Go to the SEQUENCER page and tap **REC**.
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
- Stored in the sound: all sound values including chorus and the DELAY and REVERB amounts.
- Saved automatically (no SAVE needed): the state at power-off (sound with edits, page, octave, tempo, clock, MIDI channel, effects, arp), the sequences and the loops. This happens about 1.5 s after a change, but only during a pause in playing. So before switching off, stop (arp, sequencer, loop) and wait a moment.
- After switching on, the sequencer and LOOP are off; switch them on with SEQ or on their page.

## Effects

- **Chorus** (in the sound): CHORUS on/off, MODE (FREE I II I+II), RATE.
- **Delay and reverb**: the DELAY and REVERB amounts are part of the sound (pages PERFORM and SENDS). Global, not in the sound: DLY LVL and REV LVL (effect levels), DELAY: TIME, FEEDBACK, TONE, PING; DELAY 2: SYNC (to tempo), NOTE (note value), MOD; REVERB: SIZE, DECAY, DAMP, TYPE. Delay at most 0.6 s.
- No distortion: its values stay in the sound but have no page and no effect.

## Tips

- To change page when SEQ switches on/off on its page: turn **SELECT**.
- MIDI over USB and TRS: channel under GLO, TEMPO, MIDI CH (`OMNI` = all). Pitch bend and mod wheel have no effect. CLOCK `AUTO` follows an incoming MIDI clock, `INT` never, `MIDI` always; SEND outputs the clock.
- Rescue: hold **OCT −** while switching on to start only the update mode (no sound) and install a firmware again.
