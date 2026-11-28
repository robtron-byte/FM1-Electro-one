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

| `MKSULTRA_FM1` | 0 | Rechenwege fürs FM-1, gleich klingend, nicht bitgleich: LFO-Phase in Festkomma statt double |
| `MKSULTRA_DELAY_TENTHS` | 40 | Delay-Puffer in Zehntelsekunden; die Zeiten rechnen weiter mit 4 s, Längeres endet am Puffer |
| `MKSULTRA_PREDELAY_TENTHS` | 10 | Vorverzögerung des Halls, ebenso |
| `MKSULTRA_FX_INT16` | 0 | Delay- und Hall-Leitungen als 16 Bit (Delay ±1, Hall ±8 voll) |

Bitgleich schneller (ohne Schalter, alle Golden- und Ron-Prüfsummen unverändert, 28.11.2026, sh101-Chat für ADR 0045):
`fabsf/fminf/fmaxf/floorf` inline, `f2l` ohne 64-Bit-Wandlung, Hall-Indizes in 32 Bit, Hüllkurve zählt zwischen ihren
Rechenschritten (Steuerrate sr/243) mit einer ganzen Zahl statt double. Gerät (fm1-emu, Preset Stringer): Master-Kette
ohne Note 295 % → 63 % eines Blocks (mit `MKSULTRA_FM1`), je Stimme 75 % → 39 %.

`make test` prüft zusätzlich die Gerätefassung (`test_fm1`: 6 Stimmen, `MKSULTRA_FM1`, 16 Bit, Delay 0,4 s, Vorverzögerung
0,2 s): Ron-Presets vorn und im Pegel wie der Rust-Kern (RMS 1 %, Spitze 5 %), dazu die Abweichung je Fall.

Der Zustand kommt vom Aufrufer (`mksultra_baustein.state_size`). Er enthält Zeiger auf seine eigenen Puffer und darf nach
`init` nicht verschoben werden.

## Messung Sparfassung (28.11.2026, `make -C bausteine mksultra`)

Die Sparfassung wird gegen die volle Fassung gemessen:

- **6 Stimmen** (`SPAR=6`, Standard im Makefile): alle 7 Golden-Fälle sind Sample für Sample gleich.
  390 der 520 Presets stellen ohnehin höchstens 6 Stimmen ein (272 davon genau 6).
- **4 Stimmen**: ebenfalls alle 7 Golden-Fälle gleich, auch der Arpeggiator.
- **2 Stimmen**: weicht nur beim vierstimmigen Akkord ab (`02_polysynth_chord`).
- Die Stressfälle haben 8 Noten im Abstand von 0,1 s, je 0,5 s gehalten. Sie weichen ab, sobald mehr Noten klingen, als
  Stimmen da sind. Dann wird eine Stimme gestohlen: bei 6 Stimmen eine schon losgelassene, bei 4 Stimmen eine noch
  gehaltene Note. Ab da ist die Differenz zur vollen Fassung etwa so laut wie das Signal selbst, denn es klingen
  andere Noten. Der Gesamtpegel ändert sich dagegen kaum.
  - Bei 6 Stimmen ab 0,6 s: Differenz −1,1 dB (PolySynth) bzw. −2,9 dB (Brass), Gesamtpegel +0,40 bzw. −0,12 dB.
  - Bei 4 Stimmen ab 0,4 s: Differenz +0,8 dB bzw. −0,6 dB, Gesamtpegel +0,07 bzw. −0,37 dB.

## Presets des Nutzers (Ron) – Pflicht (ADR 0051)

Die Presets in `presets/Ron/` der Vorlage sind dem Nutzer am wichtigsten. Stand 28.11.2026 sind es 6:
041 Lead 6 short ron, Bass Hit 01, Micro Ticks, R2 Random, Short Hit, Stringer.

- **Tabelle:** `tools/mks2c.py` liest **alle** `.mks`-Dateien der Vorlage neu ein, auch neue im Ordner `Ron/`. Es stellt
  die Ron-Presets an den Anfang der Tabelle (Index 0–5), vor die Werks-Sounds. Das Ergebnis `mksultra_presets.c` ist
  eingecheckt, der Build braucht die Dropbox also nicht. Neu erzeugen mit `make -C bausteine/mksultra presets PRESETS=<…/app/presets>`.
- **Prüfung:** Jedes Ron-Preset bekommt im Test einen eigenen Render: Akkord 48 55 60 64, 1 s halten, 1 s loslassen,
  48 kHz. Verglichen werden Prüfsumme, RMS und Spitze mit `ref/ron.txt`. Die Datei erzeugt der Rust-Kern:
  `cargo run --release --manifest-path tools/sweep-rust/Cargo.toml -- <presets> ron`.
  Stand: alle 6 bitgleich. Der Test schlägt fehl, wenn ein Ron-Preset fehlt, nicht vorn steht oder abweicht. Zwei davon
  (Micro Ticks, 041 Lead) sind zusätzlich Golden-Fälle der Vorlage. Kommt ein neues Ron-Preset dazu, `ref/ron.txt` neu
  erzeugen.
- **Sparfassung an den Ron-Presets.** Gemessen wurden der Akkord (4 Noten) und ein Stressfall (8 Noten,
  Stimmenzahl des Presets):

| Stimmen | Ergebnis |
|---|---|
| 6 | alle 6 Ron-Presets Sample für Sample gleich (beide Fälle) |
| 4 | Lead, Micro Ticks, Short Hit gleich (sie spielen einstimmig). Bass Hit 01 und Stringer weichen im Stressfall ab (Pegel +0,05 / −1,16 dB). R2 Random weicht schon beim Akkord ab, weil Round Robin andere Stimmen wählt (Pegel −0,04 dB); im Stressfall −0,40 dB. |
| 2 | Bass Hit 01, R2 Random, Stringer weichen deutlich ab (Pegel bis −4,5 dB) |

**Pflicht für die Firmware:**

- Die Ron-Presets stehen vorn (Preset 1–6).
- Es gibt mindestens 6 Stimmen. Weniger nur nach Rücksprache mit dem Nutzer.

## Alle 520 Presets gegen den Rust-Kern

`make sweep RUST=<Liste>` rendert jedes Preset kurz (Noten 60 und 64, 0,26 s halten, 0,26 s loslassen). Dann vergleicht
es die Prüfsumme mit einer Liste, die das Rust-Programm `tools/sweep-rust/` erzeugt. Es nutzt die Vorlage als
Abhängigkeit (Dropbox-Pfad in `Cargo.toml`) und denselben Ablauf. Stand 28.11.2026: **alle 520 Presets bitgleich**.

```sh
CARGO_TARGET_DIR=/tmp/sweep cargo run --release --manifest-path bausteine/mksultra/tools/sweep-rust/Cargo.toml -- "<…/app/presets>" > /tmp/rust.txt
make -C bausteine/mksultra sweep RUST=/tmp/rust.txt
```

## Baustein-Tabelle

Der Test spielt auch über `mksultra_baustein` (Werte 0..1, `render` addiert) und vergleicht mit `mksultra_process`:
gleich. `active` meldet „klingt noch“, solange eine Stimme läuft oder der Ausgang in der letzten halben Sekunde hörbar
war. Ist das Delay an, gilt das für die letzten 4,5 s. So schneidet ein Adapter den Nachklang von Delay und Reverb
nicht ab.

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
