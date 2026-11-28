# MKS Ultra solo

Der Baustein [`../../mksultra`](../../mksultra/) (Alpha Juno / MKS-50 „Ultra“) als Firmware für das M-VAVE FM-1, auf dem
Gerüst [`../gemeinsam`](../gemeinsam/). Entscheidung und Messungen: [ADR 0045](../../../docs/adr/0045-mksultra-solo.md).

| | |
|---|---|
| Firmware | `firmware/MKSULTRA-0.1.0-alpha.fwsc` (Fassung A, Kennung FM-1_93), `check.sh` ok |
| A | 1 Stimme, VCF 4× wie der Baustein; Last im Emulator Kern 1 ~45 % (Unisono bis 98 %), Kern 0 19–59 % |
| B | `MKU_B=1 python3 tools/build.py` → `build/MKSULTRA-ab-0.1.0-alpha.fwsc`: 2 Stimmen, VCF 2×; Kern 1 62 % (Unisono 79 %) |
| Presets | Ron 1–6 vorn, dann Bank A, Bank B, MKS, emptyvessel (213); INIT = `default` der Vorlage |
| Am Gerät | nicht geprüft |

Kern 1 rechnet die Stimmen (Block N), Kern 0 die Master-Kette (Block N−1): bitgleich mit dem Baustein, 1,45 ms später.
Effekte, Arpeggiator und Akkorde sind die des Bausteins; der Sequencer (REC/SEQ) kommt aus dem Gerüst.

## Bedienung

Raster wie alle Firmwares aus fm1-synths: KNOB 1–4 = vier Felder, SELECT = Seite, PRESETS = Sound, Taste tippen = Seiten,
halten = vier wichtigste Werte. Regler 0..100 (Schalter mit ihren Stufen). Seiten: HOME (PLAY, PERFORM), EDIT (DCO, DCO 2,
DCO MOD, VCF, VCF 2, VCF 3, SPECIAL, SPECIAL 2, UNISON), ENV (ENV, ENV 2 mit Bild, VCA), LFO (LFO, LFO 2, CONTROL, GLIDE),
FX (CHORUS + EQ, DELAY, DELAY 2, REVERB), ARP (Arp des Bausteins), SEQ, GLO (TEMPO, VOICES, SYSTEM), SEL (Soundliste).
Eigene Sounds: 36 Plätze (SAVE).

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/mksultra/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/mksultra/build/mksultra.fwsc $PWD/bausteine/varianten/mksultra/build/dinghy.elf
```

Last im Emulator: `mku_meas` (Takte des letzten Blocks: Stimmen Kern 1, Master Kern 0, Spitzen) per `--dump` lesen.
