# 808 solo

Der Baustein [`../../dk808`](../../dk808/) (DrumKern, alle 17 Instrumente) in float (`DK808_FLOAT=1`) über den
Gerüst-Adapter `dk808_bs` auf dem Gerüst [`../gemeinsam`](../gemeinsam/). Entscheidung:
[ADR 0050](../../../docs/adr/0050-dk808-float-adapter.md), Feature B20. Paket `firmware/DK808-<Version>.fwsc`,
Kennung FM-1_91 (geteilt mit 808 + 101), `STORE_MAGIC` „DK01“, Journal „DKST“. Bedienung: [ANLEITUNG.md](ANLEITUNG.md).

## Stand (28.11.2026)

| | |
|---|---|
| Klang | dk808 in float (schlechtestes Instrument −62 dB gegen double), alle 17 Instrumente, Pegel je Instrument, `DK_GAIN` 1,6 |
| Kerne | zwei Zustände des Kerns (`dk808_set_run`): Kern 0 alles außer CH/OH/CY, Kern 1 CH/OH/CY neben Delay, Hall und Distortion (`DK_K1_MASK`; `DK808_MASK_ALL` = ganze 808 auf Kern 1, für die Kombis) |
| Last (Emulator, `DK_LAST=1`) | volles Pattern: Kern 0 Spitze 26 %, Kern 1 50 % (mit Delay, Hall, TUBE); alle 17 zugleich 43 % / 51 % |
| Sequencer | 16 Steps je Instrument + Akzent, Länge 1–16, im Set des Gerüsts (Journal); Seite STEPS; Live-Aufnahme LOOP (512 Ereignisse) |
| Effekte | Distortion als Send (TUBE/OD/DIST/RAT), Delay ≤ 0,6 s, Hall |
| Am Gerät | nicht geprüft |

## Aufbau

- `app/dk_app.c`: Kit (72 Werte des Adapters + 4 Distortion), Seiten, zwei Zustände `engA`/`engB` im Pool.
  - Tasten und MIDI schlagen in beiden Zuständen an (vor dem Start von Kern 1; jeder spielt nur seine Maske).
  - Die Steps des Sequencers rechnet Kern 0 und legt sie in `pend[]`; beide Kerne schlagen sie im nächsten Block an.
  - Kern 1 rechnet in `GM_FX_EXTRA` die Distortion des Blocks davor und dann `engB`. Dessen Delay-/Hall-Sends
    gehen über `GM_FX_PRE` in den nächsten Block.
- Gerüst-Haken (abwärtskompatibel): `GM_FX_PRE`, `GM_KEY`, `GM_LIGHTS`.

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/dk808/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/dk808/build/dk808.fwsc $PWD/bausteine/varianten/dk808/build/dinghy.elf
```

`DK_LAST=1` in der Umgebung baut die Messfassung (je 1 s: nichts, jedes Instrument einzeln, alle 17, volles Pattern;
Distortion-Send an). Die Takte je Block stehen in `dk_last_peak[]`/`dk_last_sum[]` (Kern 0) und
`dk_last_peak1[]`/`dk_last_sum1[]` (Kern 1: Delay, Hall, Distortion, engB), 34829 Takte = ein Block.

## Offen

- Am Gerät: Last, Pegel (`DK_GAIN`), Bedienung.
- Mehrere Pattern-Plätze (A–D), Swing, Fills wie BEAT: noch nicht.
