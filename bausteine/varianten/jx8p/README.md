# JX-8P solo

Die JX-8P-Firmware für das M-VAVE FM-1, fortgeführt aus fm1-synths (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa,
`jx8p/`, dort zuletzt JX8P-0.2.4-alpha), mit den eigenen Presets des Nutzers. Kennung FM-1_97 und Speicherformat wie
dort: wer von der fm1-synths-Fassung aktualisiert, behält eigene Sounds, Sets, Sequenzen und den letzten Stand.
Entscheidung: [ADR 0046](../../../docs/adr/0046-jx8p-baustein.md) (Wahl A des Koordinators, 28.11.2026), eigene Presets
[ADR 0051](../../../docs/adr/0051-eigene-presets.md). Feature Y14.

## Warum der Kern aus fm1-synths

Der Baustein [`../../jx8p`](../../jx8p/) ist `engine/jx8p.c` von FM1-Electro: ein Part, Stimmen je Part, ein Kern. Die
fm1-synths-Fassung hat einen Stimmen-Pool über vier Parts, den sich beide Kerne teilen (`engine/jx.c`). Beide stammen
aus derselben Portierung der iOS-App; mit einem Part klingen sie **Sample für Sample gleich**: alle 64 Werks-Töne und die
7 eigenen Presets, geprüft mit `make -C bausteine jx8p` (Ziel `pool`, gehört zu `test`). Mit Stimmen auf zwei Kernen
ändert sich nur die Reihenfolge der Summen (Rundung, fm1-synths `make cores`: unter −120 dB).

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/JX8P-0.2.7-alpha.fwsc` (Kennung FM-1_97, `STORE_MAGIC` „JX8P“ wie fm1-synths), `tools/fm1-emu/check.sh` ok |
| Last (Emulator) | 0.2.6 (Pool 4, Delay ≤ 0,6 s), 6 gehaltene Noten auf Part A: Kern 0 höchstens 17 %, Kern 1 höchstens 17 %; keine verspäteten Blöcke. 0.2.5 mit Pool 6: höchstens 23 % je Kern |
| Am Gerät | 0.2.5 nicht geprüft; die fm1-synths-Fassung 0.2.0 am Gerät: 55 % Spitze (dort ADR 0002) |

## 0.2.6: Regel des Nutzers (28.11.2026)

Pool **4 Stimmen** (`-DJX_POOL=4`, fm1-synths: 6) und Delay höchstens **0,6 s** (`-DSYN_FX_DLY_MAX_S=0.6f`). Speicherformat
unverändert, gespeicherte Stände laden weiter.

## Gleichstand mit fm1-synths JX8P-0.2.4 (keine Rückschritte)

| Funktion | fm1-synths 0.2.4 | JX-8P solo 0.2.5 |
|---|---|---|
| Klangerzeugung | `jx.c`, Pool 6 Stimmen, 4 Parts, 2 Kerne | gleich (dieselbe Datei) |
| Parts | A–D, ALGORITHM wählt, MIX (Pegel je Part), MIDI-Kanal je Part | gleich |
| SETS | 8 Performances (Blob 1) | gleich, gespeicherte bleiben gültig |
| Sounds | 64 Werks-Töne, PG8X INIT, INIT TONE, 40 eigene (Blob 0) | gleich, dazu die 7 eigenen iPhone-Presets (s. u.) |
| Arp, Akkord (CHORD-Taste), Step-Sequencer (Blob 3) | ja | gleich |
| MIDI-Clock, Start/Stop, Clock senden | ja | gleich |
| Delay, Hall, Chorus je Part | ja | gleich |
| Journal (letzter Stand beim Ausschalten) | ja | gleich, Werksnummern unverändert |
| SYSTEM: Last, Test, CORES (1 gegen 2 Kerne) | ja | gleich |
| Pitch Bend, Modulationsrad | nein (`jx.h`: „not taken“, die App liest weder 0xE0 noch CC 1) | nein, wie dort |

Geändert ist nur die Soundliste (`app/jx_app.c`, Abschnitt „the factory list“): die eigenen Presets hängen hinter
INIT TONE (die Nummern der bisherigen Sounds in Sets und Journal bleiben), und die Gruppe EIGENE steht beim Blättern
gleich nach INIT. Sie zeigt zuerst die iPhone-Presets, dann die am Gerät gespeicherten eigenen Sounds.

## Eigene Presets

STRINGS 01, HI STRI 01, SYNTH B 01, PG8X IN 01–03, SPRUNG ODE aus der JX-8P-iOS-App (`engine/jx_eigene.c`, erzeugt von
`tools/eigene83.py` aus `bausteine/jx8p/jx8p_eigene.c`). Neue Presets vom iPhone:

```sh
tools/iphone_presets_pull.sh jx8p                      # holt sie und erzeugt bausteine/jx8p/jx8p_eigene.c
python3 bausteine/varianten/jx8p/tools/eigene83.py     # engine/jx_eigene.c
make -C bausteine jx8p                                 # bitgleich mit dem Baustein?
```

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/jx8p/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/jx8p/build/jx8p.fwsc $PWD/bausteine/varianten/jx8p/build/dinghy.elf
```

Plattform und `shared/` kommen aus dem Gerüst [`../gemeinsam`](../gemeinsam/) (dieselben Dateien wie in fm1-synths;
`shared/dsp/` von dort dazugelegt). Neue Fassung: `VERSION` erhöhen (Z), bauen, prüfen, `firmware/JX8P-<Version>.fwsc`
ersetzen (die alte entfernen). Die Doku der fm1-synths-Fassung liegt unter `docs/` (ADR 0001–0003 dort, `KLANG.md`),
Herkunft der Dateien: `LICENSES.md`.

## Offen

- Am Gerät: 0.2.5 aufspielen über 0.2.4, prüfen, dass eigene Sounds und Sets bleiben; Last.
- Live-Aufnahme nach Art des RK-008 (ADR 0053, noch nicht freigegeben): der Sequencer (`jx_app.c`, Blob 3) bleibt dafür
  unverändert und erweiterbar.
- Mehrere Parts im Gerüst `gemeinsam/` für die Kombis: eigener Schritt.

## Live-Aufnahme (0.2.7, ADR 0053)

REC tippen: Aufnahme sofort; zweimal schnell: 1 Takt Einzählen; REC übernimmt, PLAY tippen verwirft den laufenden
Durchgang. Die erste Aufnahme legt die Länge fest (ganze Takte, bis 16), danach Overdub. Fest auf 1/16, Notenlänge wie
gehalten. Seite LOOP (Taste SEQ): LOOP an/aus, BARS (0 = leer), LONGER (REPEAT/EMPTY), SHORTER (START/END). Auf der
SEQUENCER-Seite bleibt REC die Schritt-Aufnahme. Gespeichert in Blob 4 (alte Stände laden, Schleifen dann leer).
