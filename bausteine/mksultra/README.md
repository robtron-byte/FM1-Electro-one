# mksultra: Alpha Juno / MKS-50 „Ultra“ – Stand und Auftrag

**Stand 28.11.2026: Baustein fertig.** Der C-Kern trifft alle 7 Golden-Renders der Vorlage bitgenau (FNV-1a-64 über
jedes Sample). Prüfen mit `make -C bausteine mksultra`; zum Anhören liegen die WAVs danach in `bausteine/build/mksultra/`.
Die Solo-Firmware („Alpha Juno solo“, ADR 0045, FEATURES Y13) ist ein eigener Auftrag. Sie wartet auf das gemeinsame
Gerüst `../varianten/gemeinsam/` aus dem sh101-Chat. Entscheidung: [docs/adr/0043](../../docs/adr/0043-mksultra-baustein.md).

Wer daran arbeitet: [../ARBEITSTAFEL.md](../ARBEITSTAFEL.md).

## Was drin ist

Der Klang des Rust-Nachbaus „MKS Ultra“ (TAL-Pha-Nachbau), vollständig:

- 12 Stimmen. Je Stimme ein DCO bei 2× Fs (Säge-, Puls- und Sub-Familie, Rauschen, PWM, Hard Sync, BLEP) und bis zu
  6 Unisono-DCOs. Dazu das VCF bei 4× Fs (zwei nichtlineare SVFs in Reihe, Halbband-Filter hiir), DC-Blocker und VCA.
- Hüllkurve mit 4 Stufen, globales LFO (6 Wellenformen, Delay, Tempo-Sync), PWM-LFO.
- Master: HPF bzw. Bass-Boost, DC-Offset-Nachbildung, Sub-Sonic-Filter, Chorus, Bell-EQ, Delay, Reverb, Lautstärke.
- Arpeggiator (Up, Down, Up&Down, Random, Up&Down II), Chord-Speicher, Portamento, Stimmungstabellen.
- Alle 520 Presets der Vorlage als C-Tabelle (`mksultra_presets.c`, ca. 690 kB Quelltext).

## Dateien

| Datei | Inhalt |
|---|---|
| `mksultra.h` | Schnittstelle: `mksultra_baustein` (baustein.h) und die volle Schnittstelle wie `Synth` der Vorlage (Pitch-Bend, Mod-Wheel, Aftertouch, Sustain, Tempo, Parameter in natürlichen Einheiten, Presets) |
| `mksultra_dsp.h` | fastmath, rng, filters, blep, hiir, vcf, env (Rust-Dateien gleichen Namens) |
| `mksultra_fx.h` | dco, lfo, chorus, delay, reverb, arp |
| `mksultra.c` | params, voice, engine (Stimmenverwaltung, Master-Kette) |
| `mksultra_presets.c` | erzeugt von `tools/mks2c.py` (`make presets PRESETS=<…/app/presets>`) |
| `test/test.c` | Golden-Test, Vergleich Sparfassung gegen volle Fassung, Rechenzeit |
| `ref/manifest.json` | Kopie von `docs/golden/manifest.json` der Vorlage (Prüfsummen). Die WAVs (3,3 MB) liegen nicht im Repository. |

## Wie genau

Der Port folgt `docs/SOUND_ENGINE_PORTING.md` der Vorlage, Route B:

- Gleiche Typen je Variable (`float` = f32, `double` = f64) und gleiche Klammerung.
- Keine FMA (`-ffp-contract=off`). Ganzzahl-Überläufe laufen über `uint32_t`, Float→Int sättigt wie Rusts `as`.
- Die Zufallswerte werden in derselben Reihenfolge gezogen.
- Presets: Zahlen werden als Text übernommen und erst vom C-Compiler auf `float` gerundet, wie Rusts `parse::<f32>()`.

Geprüft auf macOS arm64 mit Apple-libm (wie die Golden-Renders). Auf einem anderen libm können die letzten Bits der
Koeffizienten abweichen; dann gilt die Toleranzprüfung aus §8 der Anleitung.

## Bauschalter

| Schalter | Standard | Wirkung |
|---|---|---|
| `MKSULTRA_VOICES` | 12 | Stimmenzahl. **Sparfassung fürs FM-1: weniger Stimmen, das Filter bleibt unverändert** (Wunsch des Nutzers). Die Zufallswerte der weggelassenen Stimmen werden trotzdem gezogen. So klingen die übrigen Stimmen bitgleich. |
| `MKSULTRA_MAX_SR` | 48000 | Größte Abtastrate. Sie bestimmt die Puffer im Zustand (Delay 2 × 4 s, Reverb, Chorus): bei 48 kHz rund 2 MB, fast alles Delay. |

Der Zustand kommt vom Aufrufer (`mksultra_baustein.state_size`). Er enthält Zeiger auf seine eigenen Puffer und darf nach
`init` nicht verschoben werden.

## Messung Sparfassung (28.11.2026, `make -C bausteine mksultra`)

Die Sparfassung wird gegen die volle Fassung gemessen:

- **6 Stimmen** (`SPAR=6`, Standard im Makefile): alle 7 Golden-Fälle sind Sample für Sample gleich.
  390 der 520 Presets stellen ohnehin höchstens 6 Stimmen ein (272 davon genau 6).
- **4 Stimmen**: ebenfalls alle 7 Golden-Fälle gleich, auch der Arpeggiator.
- **2 Stimmen**: weicht nur beim vierstimmigen Akkord ab (`02_polysynth_chord`).
- Die Stressfälle haben 8 Noten im Abstand von 0,1 s, je 0,5 s gehalten. Sie weichen erst ab, sobald mehr Noten klingen,
  als Stimmen da sind; dann wird eine ausklingende Stimme früher abgeschnitten.
  - Bei 6 Stimmen ab 0,6 s; der Gesamtpegel ändert sich um +0,40 dB (PolySynth) bzw. −0,12 dB (Brass).
  - Bei 4 Stimmen ab 0,4 s; der Gesamtpegel ändert sich um +0,07 dB bzw. −0,37 dB.

## Rechenzeit (PC des Nutzers, Apple Silicon, `-O2`, `make -C bausteine/mksultra bench`)

| Last | Zeit je Sample | Anteil Echtzeit bei 48 kHz |
|---|---|---|
| Grundlast ohne Note (Preset `default.mks`, Effekte aus) | – | 0,08 % |
| je Stimme (Preset `default.mks`) | ≈ 100 ns | ≈ 0,5 % |
| je Stimme mit Unisono (Preset `Short Hat`) | ≈ 330 ns | ≈ 1,6 % |
| 12 Stimmen (Preset `default.mks`) | – | 5,6 % |

Fürs FM-1 heißt das: Die Stimme mit DCO 2×, VCF 4× und zwei nichtlinearen SVFs kostet ein Vielfaches einer einfachen
Stimme. Wie viele Stimmen das FM-1 schafft, misst erst der Firmware-Auftrag im Emulator und auf dem Gerät. Unisono
kostet etwa das Dreifache einer normalen Stimme.

## Nächste Schritte (je ein eigener Auftrag)

1. Solo-Firmware „Alpha Juno solo“ auf dem gemeinsamen Gerüst (ADR 0045, Y13). Dort die Stimmenzahl im Emulator
   einmessen und die Puffergröße festlegen (`MKSULTRA_MAX_SR`; Delay eventuell kürzer).
2. Reicht das Sparen an den Stimmen nicht, über andere Wege reden (zum Beispiel Effekte abschaltbar oder Unisono
   begrenzen). Am Filter wird erst gespart, wenn der Nutzer das will.

## Nicht anfassen

`engine/` (auch nicht `engine/mks.c`, der bisherige MKS-50 bleibt), `ui/`, `platform/`, `firmware/` und das
Haupt-`Makefile`.
