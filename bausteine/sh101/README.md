# sh101: SH-101 (MonoBass)

Ein monophoner Synth nach Roland SH-101 (wahlweise 6 Stimmen poly), als Baustein. Port des Swift-Nachbaus „MonoBass“ des
Nutzers nach C99. Grundlage für die Firmwares „101 solo“ (`../varianten/sh101/`), „808 + 101“ und „808 + 101 + JX“.

## Stand (28.11.2026)

**Fertig. Sample für Sample gleich wie die Vorlage:** alle sieben Testvektoren der Vorlage stimmen bitgenau mit ihren
Referenz-WAVs überein (Abweichung 0 über je 96 000 Samples, macOS/clang). Ohne die WAVs prüft der Test die 64 Samples und
die 20 RMS-Fenster aus `ref/vectors.json`.

| Datei | Inhalt |
|---|---|
| `sh101.h`, `sh101.c` | Kern: BLEP-Oszillator (Puls, Säge, Sub, Rauschen), 4× überabgetastete nichtlineare Ladder (24/12 dB), Hüllkurven, Glide, LFO, Drift, 8× überabgetasteter Clipper mit ADAA, Arpeggiator/Sequencer (96 Schritte), Poly 6 Stimmen |
| `test/test.c` | Host-Test (Vektoren, WAVs, `baustein_t`) |
| `ref/vectors.json` | Testvektoren der Vorlage (unverändert), `ref/vectors.h` daraus erzeugt (`ref/make_vectors_h.py`) |
| `ref/SHA256SUMS.txt` | Prüfsummen der Referenz-WAVs (die WAVs selbst bleiben in der Dropbox, je 384 kB) |

**Genauigkeit:** Die Vorlage rechnet Audio in 32-bit `float`, nur Hüllkurven, Glide, LFO-Phase und Clipper in `double`.
„Volle Genauigkeit“ heißt hier: dieselbe Rechnung, nicht mehr. Gebaut mit `-ffp-contract=off`, damit der Compiler keine
Multiplikationen und Additionen zusammenzieht. Fallstrick beim Port: Swift `Float.pi` ist zur Null hin gerundet
(`0x1.921fb4p+1`); mit dem üblich gerundeten π weicht die Resonanz um bis zu 2e-4 ab.

## Vorlage

Dropbox `Programmieren/Musik/REA/SH101/app/` (Stand 27.11.2026): `docs/SOUND-ENGINE.md` (`id:bJM3TYf_3HoAAAAAABvbPg`),
`Sources/MonoBassCore/` (`Oscillator`, `Voice`, `Engine`, `Halfband`, `Params`, `Preset`), Testvektoren
`docs/test-vectors/` (erzeugt mit `swift run -c release RenderCheck --vectors docs/test-vectors`).

## Schnittstelle

- `sh101_baustein` (`../baustein.h`): Parameter 0..1 (ARP_MODE und ARP_SEQ_SYNC_MODE werden auf 0..6 bzw. 0..3 gestreckt),
  Velocity 0..127 → 0..1, `render` addiert mono auf links und rechts.
- Direkt: `sh101_set(e, SH101_…, roh)` mit Werten wie in den Presetdateien der Vorlage, `sh101_param_key()` liefert den
  Attributnamen (`filtercutoff` …), `sh101_note_on/off`, `sh101_pitch_bend`, `sh101_mod_wheel`, `sh101_set_step`,
  `sh101_prepare` (Parameter übernehmen, Blockanfang) und `sh101_sample` (ein Sample, samplegenau).
- Parameter gelten ab dem nächsten `render`/`sh101_prepare`, wie in der Vorlage am Blockanfang.

Zufall (S&H, LFO-Trigger, Random-Arp) nimmt einen eigenen xorshift statt des Systemzufalls der Vorlage; die Testvektoren
berühren ihn nicht.

## Schalter

| Schalter | Standard | Wirkung |
|---|---|---|
| `SH101_WITH_POLY` | 1 | Poly-Modus mit 6 Stimmen (Parameter `polymode`); 0 = nur eine Stimme |
| `SH101_WITH_ARPSEQ` | 1 | Arpeggiator und Sequencer der Vorlage; 0 = weg |

Zustand: 12 256 Bytes voll, 3 096 Bytes schlank (beide aus).

## Prüfen

```sh
make -C bausteine sh101
make -C bausteine/sh101 test WAV="$HOME/ediundsepp Dropbox/Ron Zöllner/Privat/PP/Programmieren/Musik/REA/SH101/app/docs/test-vectors"
```

Die WAVs (float32 mono) landen in `bausteine/build/sh101/` (voll) und `…/schlank/`.

## Nicht anfassen

`engine/`, `ui/`, `platform/`, `firmware/` und das Haupt-`Makefile`. Ins Gerät kommt der Baustein über die Solo-Firmware
`../varianten/sh101/` (ADR 0042, 0044).
