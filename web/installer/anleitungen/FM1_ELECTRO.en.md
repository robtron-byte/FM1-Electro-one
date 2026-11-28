# FM1-Electro

FM1-Electro turns the M-VAVE FM-1 into an electro groovebox with four areas that run together at one tempo: **BEAT** (808 drums with ten instruments and five kits: TR-808, ELECTRO, MIAMI, KRAFT, PO-12), **RHYTHM** (3 voices), **STRINGS** (3 voices, chords) and **BASS** (1 voice, mono). The synth areas play sounds of the JX-8P or JP-6 engine. Each area has four pattern slots A–D. Version: 0.5.6-alpha.

## Controls at a glance

| Control | Job |
|---|---|
| MASTER | Volume |
| SELECT | Pattern slot A–D (in HOME: all areas at once) |
| ALGORITHM | Tempo (1 BPM per detent) |
| PRESETS | Sound; in BEAT the kit |
| KNOB 1–4 | Sound controls of the current page |
| FX / SEL / ENV / LFO | Area BEAT / RHYTHM / STRINGS / BASS; press again: next page |
| HOME | Overview and mixer (again: back) |
| PLAY | Start / stop |
| REC | Record into the current area |
| SAVE | Save everything |
| ARP | Tap: arpeggiator on/off; held: generator |
| SEQ | Synth areas: STEP mode on/off; BEAT and HOME: fill |
| OCT − / OCT + | Synths: octave; BEAT: OCT + preview on/off, OCT − step page 1–16 / 17–32 |
| EDIT, GLO | Shift keys (hold + another key); a short tap has its own job |

Basic rules:

- Turn left = less, quieter, shorter, back; right = more, louder, longer, forward.
- Structure is on the left (SELECT, ALGORITHM, PRESETS), sound on the right (KNOB 1–4).
- Short presses for frequent things, EDIT or GLO held + key for rarer ones. What happens is shown briefly at the bottom of the display.
- Tap EDIT (in RHYTHM, STRINGS, BASS): synth editor open/close. Tap GLO in BEAT: mute mode on/off.
- GLO + area button: mute/unmute the area. EDIT + area button: editor of that area.
- Black keys: the instruments in BEAT, notes elsewhere. White keys: steps or notes, the slots in HOME.

## Pages

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| HOME | Level BEAT | Level RHYTHM | Level STRINGS | Level BASS |
| HOME + GLO held | `DLY TIME` | `FEEDBACK` | `REV SIZE` | `DECAY` |
| HOME + EDIT held | Distortion send BEAT | Send RHYTHM | Send STRINGS | Send BASS |
| BEAT | Controls of the selected instrument, four per page (e.g. Level, Tune, Decay, Tone; then Pan, Rev, Dly, Drive) | | | |
| `FILTER` | `CUTOFF` | `RESO` | `VCF ENV` | `VCF LFO` |
| `ENVELOPE` | `2 ATTACK` | `2 DECAY` | `2 SUST` | `2 REL` |
| `OSC` | `1 WAVE` | `2 WAVE` | `X-MOD` | `DCO2` |
| `MOD` | `LFO RATE` | `1 LFO` | `GLIDE` | `1 DECAY` |
| `FX` | `CHORUS` | `DELAY` | `REVERB` | `VOLUME` |
| `ARP` | `MODE` | `RATE` | `OCTAVES` | `GATE` |

The pages `FILTER` to `ARP` apply to RHYTHM, STRINGS and BASS; the area button steps through them. A JP-6 sound shows its own controls on the same pages (names in the display).

**Synth editor:** tap EDIT (or EDIT + area button). SELECT picks the section (`DC1 DC2 MIX VCF VCA EN1 EN2 LFO KEY FX`), ALGORITHM the page, KNOB 1–4 the controls, PRESETS the sound; the keys keep playing. Tap EDIT or press the area button: back.

**OPTIONS:** hold HOME + SAVE. SELECT or ALGORITHM picks the line, PRESETS or KNOB 1–4 the value, HOME or SAVE closes. Lines: MIDI channel per area (factory BEAT 10, RHYTHM 1, STRINGS 2, BASS 3), `MIDI CLOCK` (FOLLOW / IGNORE), `MASTER FX`, compressor (`COMP …`), `DIST BASS DRUM`, `OUTPUT LEVEL`, `POWER-ON` (LAST / SLOTS A), `HELP LINE`. The version is shown bottom right.

## Sounds and presets

- PRESETS steps through the area's sounds: your own ones on the far left (`U01 …`), then the factory sounds (JX-8P, JP-6, `INIT RHY` / `INIT STR` / `INIT BASS`).
- GLO + PRESETS: your own sounds only. EDIT + PRESETS: jump to the other engine (JX-8P / JP-6).
- RHYTHM, STRINGS and BASS keep a sound per slot A–D. BEAT has one kit for all slots; EDIT + PRESETS in BEAT restores the kit's factory sound.

Saving your own sound (32 slots):

1. Shape the sound with KNOB 1–4 or in the editor.
2. GLO + SAVE (in the editor SAVE is enough).
3. Pick the slot with PRESETS (or SELECT), name it with KNOB 1 (position) and KNOB 2 (letter).
4. SAVE stores it (`SOUND SAVED AS U01`). Any other button cancels.

## Sequencer and live recording

**BEAT:** black keys 1–10 pick the instrument (BD SD RS CP CB CY OH CH ZAP PULSE), white keys set or clear steps. The 11th black key picks the accent row.

- EDIT + white key: accent; GLO + white key: 32nd roll.
- EDIT + black key: clear the instrument; GLO + black key: mute it.
- EDIT + ALGORITHM: swing. GLO + SELECT: pick an instrument without playing it.

**Hold a step** (BEAT or STEP mode): PRESETS = passes (`1:1`, `1:2` … `4:4`), ALGORITHM = probability (100 … 10 %); for drums also KNOB 1 = pitch (±12 semitones), KNOB 2 = velocity.

**RHYTHM / STRINGS / BASS, key mode:** the keys play the sound, OCT −/+ the octave. In RHYTHM and STRINGS, GLO + white key 1–9 picks a chord (1 = `SINGLE NOTES`). BASS plays single notes only.

**STEP mode (SEQ):** white keys switch steps, EDIT + white = accent, GLO + white = tie, OCT −/+ = steps 1–16 / 17–32, GLO + SELECT picks the step. KNOB 1 note, KNOB 2 octave, KNOB 3 chord (STRINGS) or accent, KNOB 4 inversion (STRINGS) or tie.

**Live recording** (default): press REC; the groove must be running (PLAY). Played notes land on the nearest step and recording goes on (overdub). REC again: done.

- Empty STRINGS or BASS pattern: the take starts with the first key (its bar becomes bar 1). STRINGS snaps to 8ths, BASS to 16ths; each note lasts as long as it is held. At the end the pattern is cut to whole bars (1, 2, 4, 8). If the take runs longer, the grid gets coarser: STRINGS up to 8 bars, BASS up to 4.
- In STRINGS, keys played together are stored as a chord (while `SINGLE NOTES` is selected).
- Hold REC for one second: clear the pattern and record anew. EDIT + REC: clear only.

**SH-101 step recording:** GLO + REC switches the area between `LIVE` and `SH-101 STEP`. Then REC; each key writes the next step (up to 32). A key pressed while the previous one is still held = legato; the last (highest) key = rest; GLO + key = previous note one step longer. REC: done.

**TRANSPOSE:** GLO + SEQ, then a key shifts the area (C4 = 0), EDIT + key shifts RHYTHM, STRINGS and BASS together (never the drums). The value is shown top right (e.g. `T+3`) and is saved. GLO + SEQ again: the keys play again.

**Pattern length:** GLO + ALGORITHM, in whole bars (BEAT 1–2 bars; STRINGS and BASS 1, 2, 4, 8). Single steps only in STEP mode. EDIT + ALGORITHM: step length in STRINGS, note length in RHYTHM and BASS.

**Slots and chains:** SELECT changes slot A–D (BEAT at the end of its pattern, the synths at the next bar). In HOME: white keys 1–4 BEAT A–D, 5–8 RHYTHM, 9–12 STRINGS, 13–16 BASS. Hold a slot and press others of the same bank to build a chain (up to 4, e.g. `A>B>B`); an empty slot gets a copy. A single press on a slot ends the chain.

**Fills (BEAT and HOME):** SEQ = fill in the next bar, SEQ held = repeat. GLO + SEQ: automatic every 8 / 16 / 32 bars / off. EDIT + SEQ in BEAT: edit the fill (step keys 1–16), EDIT + SEQ: done.

**Generator:** hold ARP, KNOB 1 style, KNOB 2 amount (BEAT: KNOB 3 keep kick / new). White key 1 = variation, key 2 = new pattern; confirm by pressing the same key again within 3 s. In HOME it acts on all areas. HOME, ARP + 16th white key twice: empty start (only SAVE makes it permanent).

## Saving

- SAVE stores everything: all slots, sounds, kit, chains, settings (`SAVED`).
- Grooves made by the generator are kept automatically shortly afterwards, once the groove has stopped.
- Changed OPTIONS are saved when you close the menu; while the groove plays, at STOP.
- When the web installer asks for an update, FM1-Electro saves everything first (`SAVING BEFORE UPDATE`).

## Effects

- **Reverb and delay:** in HOME, PRESETS sets the reverb return, GLO + PRESETS the delay return; GLO + KNOB 1–4 shapes them. Each synth sound has its own sends (page `FX`), each drum instrument Rev and Dly.
- **Distortion:** HOME, hold EDIT: KNOB 1–4 send per area, PRESETS type (OFF OD DIST RAT TUBE), ALGORITHM drive, SELECT tone. With OFF the sends are locked.
- **Live effects (punch-ins)** in HOME, while held: black keys 5–11 = STUTTER, REVERB THROW, TAPE STOP, REPEAT, DELAY, BITCRUSH, PHASER (can be combined). Meanwhile white keys 1–6 set note value or strength; for REPEAT a white key 1–16 picks the step.
- **Master:** compressor and master FX in OPTIONS; COMP RATIO fully right = `CLUB`.

## Tips

- **Rescue:** holding OCT − while switching on starts only the update mode (no sound); the web installer still works then.
- Updates through the web installer keep grooves and own sounds; the M-Flasher very likely erases them.
