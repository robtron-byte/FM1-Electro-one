# MKS-50 solo

Die MKS-50-Firmware für das M-VAVE FM-1, fortgeführt aus fm1-synths (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa,
`mks50/`, dort zuletzt MKS50-0.2.4-alpha), mit den 23 eigenen Presets des Nutzers. Kennung FM-1_95 und Speicherformat
wie dort: wer von der fm1-synths-Fassung aktualisiert, behält eigene Sounds, Sets, Sequenzen und den letzten Stand.
Entscheidung: [ADR 0047](../../../docs/adr/0047-mks50-baustein.md) (Muster der Wahl A für JX-8P, Koordinator 28.11.2026),
eigene Presets [ADR 0051](../../../docs/adr/0051-eigene-presets.md). Feature Y15. Nicht zu verwechseln mit MKS Ultra
(`mksultra`, Alpha-Juno-Nachbau).

## Kern und Klang

Der Baustein [`../../mks50`](../../mks50/) ist `engine/mks.c` von FM1-Electro (ein Part, ein Kern). Die Firmware rechnet
mit dem Pool-Kern der fm1-synths-Fassung (`engine/mks.c` hier: 6 Stimmen für 4 Parts, beide Kerne), der aus FM1-Electros
Kern hervorging. Mit einem Part sind beide **bitgleich**, wenn das schnellere Ausklingen von FM1-Electro an ist
(`MKS_RELEASE_HURRY`, dort seit 07.11. Standard): alle 56 Werks-Sounds und die 23 eigenen Presets, geprüft mit
`make -C bausteine mks50` (Ziel `pool`). Ohne den Schalter weichen 20 Sounds nur im Ausklingen ab (−55 … −105 dB).

Die fm1-synths-Fassung 0.2.4 hat ihn aus. Damit sich für die Nutzer nichts ändert, bleibt er hier **aus**
(`MK_RELEASE_HURRY=0`); zum Anhören: `MK_RELEASE_HURRY=1 python3 bausteine/varianten/mks50/tools/build.py` (ADR 0006).

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/MKS50-0.2.7-alpha.fwsc` (Kennung FM-1_95, `STORE_MAGIC` „MK50“ wie fm1-synths), `tools/fm1-emu/check.sh` ok |
| Last (Emulator) | 0.2.6 (Pool 4, Delay ≤ 0,6 s), 6 gehaltene Noten auf Part A: Kern 0 höchstens 29 %, Kern 1 höchstens 8 %; keine verspäteten Blöcke. 0.2.5 mit Pool 6: höchstens 35 % |
| Am Gerät | 0.2.5 nicht geprüft |

## 0.2.6: Regel des Nutzers (28.11.2026)

Pool **4 Stimmen** (`-DMKS_POOL=4`, fm1-synths: 6) und Delay höchstens **0,6 s** (`-DSYN_FX_DLY_MAX_S=0.6f`).
Speicherformat unverändert, gespeicherte Stände laden weiter.

## Gleichstand mit fm1-synths MKS50-0.2.4 (keine Rückschritte)

Alles aus fm1-synths unverändert (App `app/`, Kern `engine/`, Doku `docs/`): 4 Parts mit Pool von 6 Stimmen auf beiden
Kernen, MIX und MIDI-Kanal je Part, 8 SETS (Blob 1), 48 eigene Sounds (Blob 0), Arp, Akkord, Step-Sequencer (Blob 3),
MIDI-Clock, Delay, Hall, Chorus je Part, Journal, Seite SYSTEM mit Last. Pitch Bend und Modulationsrad hatte die
fm1-synths-Fassung nicht (die App liest weder 0xE0 noch CC 1); hier ebenso.

Geändert ist nur die Soundliste (`app/mk_app.c`, „the factory list“): die eigenen Presets hängen hinter INIT TONE (die
Nummern der bisherigen Sounds in Sets und Journal bleiben), die Gruppe EIGENE steht beim Blättern gleich nach INIT und
zeigt zuerst die iPhone-Presets, dann die am Gerät gespeicherten. Dazu der Schalter `MK_RELEASE_HURRY` (Standard aus).

## Eigene Presets

23 aus der MKS-50-iOS-App (`bausteine/mks50/mks50_eigene.c`, direkt eingebunden: dasselbe Format). Neue vom iPhone:
`tools/iphone_presets_pull.sh mks50`, `make -C bausteine mks50`, neu bauen.

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/mks50/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/mks50/build/mks50.fwsc $PWD/bausteine/varianten/mks50/build/dinghy.elf
```

Plattform und `shared/` aus dem Gerüst [`../gemeinsam`](../gemeinsam/) (dieselben Dateien wie in fm1-synths). Neue
Fassung: `VERSION` erhöhen (Z), bauen, prüfen, `firmware/MKS50-<Version>.fwsc` ersetzen (die alte entfernen).
Herkunft: `LICENSES.md`, Doku der fm1-synths-Fassung unter `docs/`.

## Offen

- Am Gerät: 0.2.5 über 0.2.4 aufspielen, eigene Sounds und Sets prüfen; Last; `MK_RELEASE_HURRY` nach Gehör entscheiden.
- Live-Aufnahme nach Art des RK-008 (ADR 0053, noch nicht freigegeben): der Sequencer bleibt dafür unverändert.

## Live-Aufnahme (0.2.7, ADR 0053)

REC tippen: Aufnahme sofort; zweimal schnell: 1 Takt Einzählen; REC übernimmt, PLAY tippen verwirft den laufenden
Durchgang. Die erste Aufnahme legt die Länge fest (ganze Takte, bis 16), danach Overdub. Fest auf 1/16, Notenlänge wie
gehalten. Seite LOOP (Taste SEQ): LOOP an/aus, BARS (0 = leer), LONGER (REPEAT/EMPTY), SHORTER (START/END). Auf der
SEQUENCER-Seite bleibt REC die Schritt-Aufnahme. Gespeichert in Blob 4 (alte Stände laden, Schleifen dann leer).
