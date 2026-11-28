# gemeinsam: Gerüst der Solo- und Kombi-Firmwares

Alles, was eine Firmware aus Bausteinen braucht, außer dem Klang: Plattform, Bedienung, Sequencer, Speichern, MIDI,
Takt, Effekte, Lastmessung. Eine Variante (`../sh101/`, später mksultra, jx8p, mks50, jp6, dk808, Kombis) legt nur fest,
was sie spielt. Entscheidung: [ADR 0044](../../../docs/adr/0044-solo-firmware-geruest.md).

## Herkunft

| Ordner | Woher | Lizenz |
|---|---|---|
| `platform/` | fm1-synths `platform/` (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa): Dinghy / Felucca (Leo Kuroshita, Hügelton Instruments) mit SLOOPs und FM1-Electros Änderungen, `platform/ORIGIN.md` | GPL-3.0-only |
| `shared/` | fm1-synths `shared/` (dasselbe Commit): Delay und Hall (`fx/`), Pixelschrift (`ui/`), `syn_config.h`, `third_party/fm1_open` | GPL-3.0-only, `shared/LICENSES.md` |
| `app/gm_app.c`, `app/gm_ui.c` | abgeleitet aus fm1-synths `mks50/app/mk_app.c` und `mk_ui.c` (dasselbe Commit) | GPL-3.0-only |
| `app/gm_theme.h` | fm1-synths `mks50/app/mk_theme.h` | GPL-3.0-only |
| `tools/build.py` | nach fm1-synths `mks50/tools/build.py` | GPL-3.0-only |

`platform/` und `shared/` bleiben unverändert wie in fm1-synths; Änderungen nur, wenn alle Varianten sie brauchen.

## Was das Gerüst kann

- **Kern 1** startet beim Einschalten mit Selbsttest und rechnet Delay und Hall des Blocks davor, während Kern 0 den
  Klang rechnet. Fällt er aus, rechnet Kern 0 alles (Seite SYSTEM zeigt „1K!“).
- **Audio-Interrupt** (Block 64 = 1,45 ms): MIDI, Ereignisse vom Hauptprogramm, Takt, Arp, Sequencer, dann die Parts.
  Was auf dem Bildschirm passiert, hält keine Note auf (Tasten werden auch beim Zeichnen gelesen).
- **MIDI:** USB und TRS, Kanal je Part (OMNI bei einem Part), Note an/aus, Pitch Bend, CC 1, CC 120/123, Clock,
  Start/Stop/Continue, Song Position; Clock weiter an USB auf Wunsch.
- **Takt:** TEMPO oder MIDI-Clock (AUTO / INT / MIDI), PLAY halten = Start/Stop.
- **Arpeggiator** je Part (UP DOWN UP/DN RAND ORDER, 1–4 Oktaven, Notenwert, Gate, HOLD = PLAY tippen).
- **Step-Sequencer** je Part wie das SH-101: REC, dann jede Taste ein Schritt; gedrückt, während die vorige gehalten
  ist = gebunden; oberste Taste = Pause; GLO + Taste = Note davor einen Schritt länger; REC beendet und spielt. SEQ
  tippen auf seiner Seite = an/aus; im Lauf transponieren die Tasten (C4 = 0). Gespeichert in Blob 3.
- **Sounds:** Werkssounds der Variante plus 40 eigene (Blob 0), Gruppen; PRESETS blättert, SEL zeigt die Liste.
  SAVE kurz = auf den eigenen Platz speichern, lang = speichern unter (Name, Gruppe, Platz). HOME lang = Änderungen des
  zuletzt verlassenen Sounds zurückholen.
- **Stand beim Ausschalten:** Journal in den Slots der Plattform, 1,5 s nach einer Änderung in einer Spielpause.
- **Bedienung** nach fm1-synths ADR 0008: KNOB 1–4 = vier Felder unten, SELECT = Seite, PRESETS = Sound,
  ALGORITHM = Part; eine Taste tippen = ihre Seiten, halten = ihre vier wichtigsten Werte auf KNOB 1–4.
  Links weniger, rechts mehr; Regler starten am aktuellen Wert; schnelles Drehen springt weiter. OCT− + OCT+ = alle Töne aus.

## Eine Variante schreiben

`<variante>/app/<x>_app.c` legt vor `#include "gm_app.c"` fest (Vorbild: `../sh101/app/sh_app.c`):

| Was | Bedeutung |
|---|---|
| `APP_NAME`, `GM_TITLE`, `GM_ACCENT` | Name, Kopfzeile, Farbe der Kopfzeile |
| `GM_PARTS`, `GM_NAMEMAX`, `GM_SOUND_VER`, `GM_JR_MAGIC` | Parts, Namenslänge, Format der eigenen Sounds, Kennung des Journals |
| `enum { …, GM_NP }`, `gm_params[GM_NP]`, `gm_init_v[GM_NP]` | Parameter (Name, Höchstwert, Farbklasse, Anzeige) und Grundklang |
| `gm_factory[]`, `GM_NFAC`, `GM_NCAT`, `gm_cat_name[]` | Werkssounds und Gruppen |
| `gm_pages[]`, `gm_layers[NGROUP]` | Seiten (Gruppe = Taste, Bild, Titel, vier Felder) und Ebenen gehaltener Tasten |
| `kern_boot/load/set/on/off/all_off/bend/mod/render/voices/stage` | der Klang: Laden eines ganzen Sounds, ein Wert, Noten, Block rendern (addiert), klingende Stimmen, eigenes Bild |
| `GM_SEND_FIRST`, `GM_FX_FIRST_ON` (wahlweise) | erster Start: Sends der Parts (Standard 40) und Delay/Hall an (Standard aus); JX-8P: 0 und 1, weil die Sends im Sound stehen |
| `GM_ENGINES` (wahlweise, Standard 1) mit `GM_ENG_OF(v)`, `GM_PARAMS_E/INIT_E/PAGES_E/NPAGE_E/LAYERS_E(e)`, `GM_CAT_ENG(c)`, `GM_ENG_OK(p,e)`, `GM_ENG_NAME(e)`, `GM_FIRST_ENG(p)` | mehrere Klangerzeuger (ADR 0052): Erzeuger steht in einem Wert des Sounds; Seiten, Tabellen und Soundliste folgen dem Erzeuger des gewählten Parts; Felder `K_ENGINE` (PARTS), `K_CHANP` (MIDI-Kanal je Part) |
| `GM_SET_EXTRA` (wahlweise) mit `gm_extra_max/text/default`, `kern_extra` | weitere Werte der Variante im Set (Feld `K_EXTRA`), z. B. globale Distortion der Kombi |
| `GM_FX_EXTRA(wl, wr)`, `GM_FX_EXTRA_ON()` (wahlweise) | weitere Arbeit auf Kern 1 im Effekt-Takt (SH-101: Distortion; Kombi: JX-Pool und Distortion). Delay und Hall lesen den Abzug `fx_src` von `fx_in` (vor dem Start von Kern 1 kopiert) |
| `GM_OUT_GAIN` (wahlweise) | Ausgangspegel (Standard 0,6 × Vollaussteuerung) |

`kern_*` außer `kern_boot` und `kern_stage` laufen im Audio-Interrupt (`.dsp_text`). `<variante>/tools/build.py` ruft
`build(dict(...))` aus `tools/build.py` mit App, Klangdateien (SRAM), Schaltern, Kennung, Speicherkennung.

## Kennungen

| Kennung | Firmware | `STORE_MAGIC` | Journal |
|---|---|---|---|
| FM-1_94 | SH-101 solo | `0x31304853` „SH01“ | `0x54534853` „SHST“ |
| FM-1_95 | MKS-50 (fm1-synths) | | |
| FM-1_96 | FM1-Electro | | |
| FM-1_97 | JX-8P solo (löst fm1-synths ab) | `0x3130584A` „JX01“ | `0x5453584A` „JXST“ |
| FM-1_98 | MKS-50 + JX-8P (fm1-synths) | | |
| FM-1_99 | JX-8P + SH-101 (`varianten/jxsh`, ADR 0052; vorher nur Dinghy-Vorlage) | `0x48535831` „1XSH“ | `0x48535854` „TXSH“ |
| FM-1_90 … 93 | frei (nur noch vier: Kombis ggf. bündeln) | | |

## Bauen und prüfen

```sh
# einmal: Werkzeuge (siehe ../../ARBEITSTAFEL.md, Koordination)
sh gemeinsam/platform/tools/get_toolchain.sh
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
# je Variante
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 sh101/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/sh101/build/sh101.fwsc $PWD/bausteine/varianten/sh101/build/dinghy.elf
```

Auf dem Mac läuft der Build in Docker (Docker Desktop muss laufen); `tools/mac_docker.py` hängt das Repository unter
demselben Pfad ein. Pfade ohne Leerzeichen.

**Last im Emulator lesen:** Die Seite SYSTEM zeigt sie am Gerät. Im Emulator die Mittelwerte aus dem RAM holen
(`avg`, `peak`, `max_all`, `late_blocks` in Takten, ein Block = 34 829): `fm1-emu --fwsc … --ms 5000 --keys
'1500-4900:n12' --dump <Adresse von max_all> 80` (Adressen mit `nm build/dinghy.elf`; nur ein `--dump` wirkt).
Tasten: `n0..n26` Noten, `enc0` SELECT, `enc2..5` KNOB 1–4, `enc6` PRESETS, Tastennamen wie `REC`, `PLAY`.
Am Gerät liegt die Last nach FM1-Electro etwa 10 % höher.
