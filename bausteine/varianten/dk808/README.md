# 808 solo (Messfassung)

Der Baustein [`../../dk808`](../../dk808/) (DrumKern, 17 Instrumente) in float (`DK808_FLOAT=1`) über den
Gerüst-Adapter `dk808_bs` auf dem Gerüst [`../gemeinsam`](../gemeinsam/). Entscheidung:
[ADR 0050](../../../docs/adr/0050-dk808-float-adapter.md), Feature B20. **Noch keine Firmware des Nutzers:** Diese
Fassung ist zum Messen im Emulator da. Sie kommt nicht nach `firmware/`. Kennung vorerst FM-1_91 (geteilt mit
808 + 101), `STORE_MAGIC` „DK01“, Journal „DKST“.

- **Ein Drum-Part.** Sein Sound ist ein Kit: Pegel und drei Regler je Instrument, Choke, DIST SEND, dazu Modus, Drive,
  Tone und Level der Distortion. Werkskits: „808 KIT“ (Standard des DrumKern) und „808 TUBE“ (Send 40).
- **Seiten:** eine je Instrument, mit KNOB 1–4 = Pegel und drei Regler.
  - EDIT: BD, LG, SD, RS, CP
  - ENV: Congas, Toms
  - LFO: CH, OH, CY
  - ARP: CL, MA, CB
  - HOME: Pegel der wichtigsten Instrumente
  - FX: SENDS, DIST, DELAY, REVERB, CHOKE
- **Tasten:** die Drum-Map des DrumKern (MIDI 35..75). Andere Noten spielen nach dem Ton in der Oktave
  (C BD, C# RS, D SD, D# CP, E LT, F MT, F# CH, G HT, G# OH, A CY, A# CB, H CL).
- **Distortion als Send** wie beim SH-101 (`gemeinsam/fx/gm_dist.c`): Kern 0 schreibt den Send, Kern 1 rechnet ihn
  neben Delay und Hall.

## Bauen und messen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/dk808/tools/build.py
# Messmuster (je 1 s: nichts, jedes Instrument einzeln, alle 17, volles Pattern; Distortion-Send an):
DK_LAST=1 JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/dk808/tools/build.py
```

Mit `DK_LAST=1` stehen die Takte je Block für jede Phase in `dk_last_peak[]`, `dk_last_sum[]` und `dk_last_n[]` (20
Phasen, 34829 Takte = ein Block). Im Emulator 22 s laufen lassen und die Felder auslesen (`fm1-emu --dump`). Die
Ergebnisse stehen in ADR 0050.

## Offen

- Die Firmware 808 solo: Bedienung (Pads, Step-Sequencer für Drums), Kits, eigene Kits, Paket.
- Kombis 808 + 101 und 808 + 101 + JX: für 808 und Synth zusammen reicht Kern 0 nicht (ADR 0050, Folgen).
