# TR-808

This firmware turns the M-VAVE FM-1 into a drum computer. It is played like the **BEAT** area of FM1-Electro, but with **15 instruments** of the DrumKern:

- bass drum, laser gun, snare, rim shot, hand clap, cowbell
- two toms (low, high), two congas (low, high)
- closed and open hi-hat, cymbal
- claves, maracas

It adds:

- four pattern slots A–D of up to two bars
- accent, 32nd rolls, swing, fills
- a generator for new grooves
- live recording
- the **punch-in effects** of FM1-Electro
- distortion as a send (TUBE, OD, DIST, RAT), delay and reverb

Both cores work: core 0 plays most instruments and the punch-ins, core 1 the open hat, cymbal, delay, reverb and distortion. Version 0.2.0-alpha.

## Controls at a glance

| Control | Does |
|---|---|
| PLAY | start / stop |
| REC | live recording on/off (EDIT + REC: clear the pattern; hold REC 1 s: clear and record anew) |
| SELECT | pattern slot A–D (switches at the end of the pattern); GLO + SELECT: pick an instrument without playing it |
| ALGORITHM | tempo (1 BPM per detent); GLO + ALGORITHM: length 1 or 2 bars; EDIT + ALGORITHM: swing (50–75) |
| PRESETS | kit (TR-808, ELECTRO, MIAMI, KRAFT); EDIT + PRESETS: the kit's factory sound |
| KNOB 1–4 | LEVEL and the three knobs of the selected instrument |
| FX | next knob page: instrument, SENDS, DIST, DELAY, REVERB |
| HOME | overview: slots, level and sends |
| SEQ | fill in the next bar (held: every bar) |
| ARP held | generator |
| OCT + / OCT − | preview on/off / step page 1–16 and 17–32 |
| OCT − and OCT + together | all silent |
| SAVE | save everything |
| EDIT, GLO | shift buttons (hold + another button); GLO tapped: mute mode |

The basic rules are as in FM1-Electro: turning left is less, quieter, shorter, back; right is more, louder, longer, on. A short press is for common things, holding EDIT or GLO + a button for rare ones. What happens is shown briefly at the bottom of the display.

## The display

The start screen is the pattern: at the top the slot (A–D), the groove's name, tempo, kit and PLAY/STOP. Below it the grid with all 15 instruments and the accent row (AC). The selected instrument is highlighted and the running step marked. The marks in the grid:

- big field: accent on this step
- dark dot: 32nd roll
- white line at the top: condition or probability
- yellow line at the bottom: own velocity
- `M`: muted instrument

At the bottom are the four knobs of the current page.

## Keys

**Black keys 1–10** pick an instrument and play it: BD, SD, RS, CP, CB, CY, OH, CH, LG (laser gun), CL (claves). The **11th black key** picks the accent row. The other five instruments (LT, HT, LC, HC, MA) are picked with **GLO + SELECT**, which reaches all 15.

**White keys 1–16** set or clear the steps of the selected instrument (with two bars, OCT − switches between page 1–16 and 17–32).

- **EDIT + white key:** accent on the step.
- **GLO + white key:** 32nd roll.
- **EDIT + black key:** clear the instrument.
- **GLO + black key:** mute / unmute the instrument.
- **Tap GLO:** mute mode. The black keys then mute and unmute. Their light: full = plays and has hits, faint = nothing recorded, blinking = muted. Tap GLO again to leave it.

**Hold a step:**

| Control | Does |
|---|---|
| PRESETS | passes: 1:1, 1:2, 2:2, 1:4, 2:4, 3:4, 4:4 |
| ALGORITHM | probability 100 … 10 % |
| KNOB 2 | velocity in 7 levels (the step sounds as a check) |

An empty step is set when pressed. A set step goes off when you let go, unless something was turned meanwhile. There is no pitch per step here: the DrumKern has no input for it.

**Preview (OCT +):** When it is off, a black key only picks the instrument. While recording, the key then records without a sound.

## Knob pages (FX)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| instrument (e.g. BD) | LEVEL | TONE / TUNE / DEPTH | DECAY | SWEEP / SNAPPY / BP FREQ |
| SENDS | DLY SEND | REV SEND | DST SEND | LEVEL |
| DIST | MODE (TUBE OD DIST RAT) | DRIVE | TONE | LEVEL |
| DELAY | TIME (up to 600 MS) | FEEDBACK | TONE | LEVEL |
| REVERB | SIZE | DECAY | DAMP | LEVEL |

The instruments' knobs are those of the DrumKern:

| Instrument | Knobs |
|---|---|
| BD | TONE DECAY SWEEP |
| LG | DEPTH DECAY SWEEP |
| SD | TONE DECAY SNAPPY |
| RS | TUNE DECAY |
| CP | TONE REVERB |
| toms | TUNE DECAY SNAPPY |
| congas | TUNE DECAY |
| CH, OH, CY | TONE DECAY BP FREQ |
| CL | TUNE DECAY |
| MA | TONE |
| CB | TUNE DECAY |

A knob starts from the current value; turning fast takes bigger steps.

## Kits

**PRESETS** changes the kit; it applies to all slots:

- **TR-808:** the DrumKern's factory sound.
- **ELECTRO:** longer kick, snappy snare, short hats.
- **MIAMI:** long, deep kick, darker snare.
- **KRAFT:** short, clicky kick, high snare.

Each kit remembers how it was last set. **EDIT + PRESETS** brings back the kit's factory sound.

## Slots, length, swing

- **SELECT:** slot A–D. The new slot starts at the end of the running pattern; until then it blinks yellow in the header.
- **HOME:** the slots shown big. White keys 1–4 pick A–D. Hold a slot and press others to make a **chain** (up to 4, e.g. `A>B>B`); an empty slot gets a copy. A single press on a slot ends the chain. Black key 1: everything muted / unmuted.
- **GLO + ALGORITHM:** length 1 or 2 bars. When a pattern gets longer, its first bar repeats.
- **EDIT + ALGORITHM:** swing 50 (straight) to 75.

## Fills

- **Tap SEQ:** the slot's fill plays in the next bar.
- **Hold SEQ:** fill every bar while held.
- **GLO + SEQ:** fill by itself every 8 / 16 / 32 bars / off.
- **EDIT + SEQ:** edit the fill. Step keys 1–16 then show its bar; EDIT + SEQ again ends it.

An unused fill is made from the groove (a run on snare or clap towards the end of the bar).

## Generator

**Hold ARP:**

| Knob / key | Does |
|---|---|
| KNOB 1 | style (ELECTRO, BRONX 82, NUMBERS, TRANS EUR, TOUR, COMPUTER, ROTHER, MIAMI, RECKLESS, BOOM BASS, DROWNED, ROBOT, PERFECT, EGYPT, FREAKY, LOVE) |
| KNOB 2 | amount of variation |
| KNOB 3 | keep kick / new |
| white key 1 | variation of the pattern |
| white key 2 | new pattern in the style |

Press the same key again within 3 s to confirm. The styles are FM1-Electro's; their tom rows (low, high) play here.

## Punch-in effects

As in FM1-Electro: in **HOME**, hold **black keys 5–11**. While a key is held its effect sounds; letting go ends it. Several work at once. While one is held, **white keys 1–6** set its value (the keys show the value). The display shows the held effects as tiles with their value.

| Key | Effect | white keys 1–6 |
|---|---|---|
| 5 | STUTTER: the mix chopped in the note value | 1/4 1/8 1/8T 1/16 1/16T 1/32 |
| 6 | REVERB THROW: the mix fades into the reverb over two beats | – |
| 7 | TAPE STOP: the tape comes to a halt | 1/16 1/8 3/16 1/4 3/8 1/2 |
| 8 | REPEAT: one step of the bar repeats every sixteenth | white key 1–16 = the step (it starts with that) |
| 9 | DELAY: dub echo, rings out when let go | 1/4 1/8 1/8T 1/16 1/16T 1/32 |
| 10 | BITCRUSH: fewer bits and rate | 12 10 8 6 5 4 bit |
| 11 | PHASER: one sweep per length | 1/4 1/2 1 2 4 8 bars |

REVERB THROW opens the reverb even when it is closed from the factory; 4 s after letting go, your own setting applies again.

## Live recording

- **REC:** recording on. While the pattern runs (PLAY), the instruments you play land on the next sixteenth, both black keys and MIDI notes. A strong hit (velocity above 90 %) sets an accent. **REC** again: off.
- **Hold REC 1 s:** clear the pattern and record anew.
- **EDIT + REC:** clear the pattern.

## MIDI

Notes over USB and TRS on all channels:

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

Other notes follow the DrumKern drum map. Incoming MIDI clock sets tempo and start/stop.

## Saving

- **SAVE:** save everything: slots A–D with all step values, the fills, the kit with its settings, tempo, effects and generator.
- It also saves by itself 2 s after a change, but only while stopped (writing makes the sound drop out briefly). So stop and wait a moment before switching off.
- States of version 0.1.0 are not taken over; 0.2.0 starts with the basic groove (ROTHER on slot A).

## Effects

- **Distortion as a send:** DST SEND (page SENDS) sends the 808 into the distortion. MODE TUBE, OD, DIST or RAT, plus DRIVE, TONE and LEVEL (page DIST). In HOME with EDIT held: KNOB 1 the send, PRESETS the mode.
- **Delay and reverb:** DLY SEND and REV SEND (page SENDS), plus the pages DELAY and REVERB. Both are closed from the factory. Delay at most 0.6 s.

## Where 808 solo differs from BEAT

- **15 instruments instead of 10:** black keys 1–10 are the same as in BEAT, only LG (laser gun) and CL (claves) take the places of ZAP and PULSE. The other five (toms, congas, maracas) are picked with GLO + SELECT, because there are no more keys. Mid tom and mid conga are left out (the user's wish): that saves processing time and choices.
- **FX instead of four area buttons:** 808 solo has one area. FX steps through instrument, SENDS, DIST, DELAY and REVERB, which in FM1-Electro sit under HOME + GLO / EDIT. SEL, ENV and LFO have no job.
- **HOME:** instead of the four areas, HOME shows slots A–D. KNOB 1–4 are the 808's level and sends; the punch-ins are on black keys 5–11 as there.
- **No pitch per step** (KNOB 1 on a held step): the DrumKern has no input for it.
- **No OPTIONS menu:** MIDI listens on all channels, the clock follows incoming MIDI clock.
- **Kits:** the four kits are variations of the DrumKern sound, no PO-12.

## Tips

- This firmware shares the identity FM-1_91 with the 808 + 101 combo. Each recognises its saved data by its own storage identity.
- Rescue: holding **OCT −** at power-on starts only the update mode (no sound), to install a firmware again.
