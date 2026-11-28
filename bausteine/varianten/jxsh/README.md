# JX-8P + SH-101 (Kombi)

Vier Parts, jeder spielt den **JX-8P** (Pool mit 4 Stimmen) oder den **SH-101** (mono, höchstens ein Part), auf dem
Gerüst [`../gemeinsam`](../gemeinsam/) mit `GM_ENGINES = 2`. Entscheidung: [ADR 0052](../../../docs/adr/0052-kombi-jx8p-sh101.md),
Feature P16. Klang: der Pool-Kern `jx.c` der JX-8P-Firmware ([`../jx8p/engine`](../jx8p/engine/), bitgleich mit dem Baustein
`jx8p`) und der Baustein [`../../sh101`](../../sh101/) mit denselben Geräteschaltern wie SH-101 solo.

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/JXSH-0.1.0-alpha.fwsc` (Kennung FM-1_99), `tools/fm1-emu/check.sh` ok (Paket und Messbau) |
| Last (Emulator, Block 64) | SH-101 SQUELCH + 4 JX-Stimmen (STRINGS 01, Chorus voll) + Delay + Hall + Distortion: **Kern 0 60 %** (Spitze 61 %), **Kern 1 36 %**, keine verspäteten Blöcke. Nur JX: Kern 0 23 %, Kern 1 36 %; nur SH: Kern 0 60 %, Kern 1 8 %. **Am Gerät nicht gemessen** |
| Speicher | RAM 85 204 von 98 304 B (13,1 kB frei), Pool 341 052 von 344 064 B; Hall-Diffusoren im RAMTEXT, Bildpuffer im NOINIT |
| Pegel | JX wie JX-8P solo, SH wie SH-101 solo (`SH_GAIN`) |
| Am Gerät | nicht geprüft |

## Bedienung

Wie die Kombi MKS-50 + JX-8P aus fm1-synths. ALGORITHM wählt den Part (A–D, oben im Bild), die Seiten folgen dem
Klangerzeuger des gewählten Parts.

| Taste | Seiten |
|---|---|
| HOME | PLAY, **PARTS** (Klangerzeuger je Part: KNOB 1–4 = A–D, JX-8P oder SH-101; ein zweiter SH-101 wird abgewiesen), **MIX** (Pegel A–D) |
| EDIT, ENV, LFO | Klang des gewählten Parts: JX-8P wie JX-8P solo (DCO-1 … VCA, ENV-1/2, LFO), SH-101 wie SH-101 solo |
| FX | **DIST** (Modus TUBE/OD/DIST/RAT, Drive, Tone, Level; für alle Parts), **DIST SEND** (A–D), DELAY (≤ 0,6 s), HALL; JX: CHORUS und Sends im Sound; SH: SENDS |
| ARP, SEQ | Arp des Gerüsts; SEQUENCER des Gerüsts und LOOP (Live-Aufnahme, ADR 0053) |
| GLO | TEMPO, **MIDI** (Kanal je Part), SYSTEM (Pegel, Last); JX: VOICE |
| SEL | Soundliste: EIGENE (die eigenen JX-8P-Presets des Nutzers) zuerst, dann die JX-Werksgruppen, JX INIT, SH BASS/LEAD/SEQ/FX |

Start: A–C die eigenen Presets (STRINGS 01, HI STRI 01, SYNTH B 01), D SH-101 SAW BASS. Wechselt ein Part den
Klangerzeuger, lädt er dessen INIT; die Soundliste zeigt nur Sounds seines Erzeugers.

## Kerne

Kern 0: Gerüst, Sequencer und der SH-101-Part. Kern 1 (Haken `GM_FX_EXTRA`): der JX-8P-Pool, Delay, Hall, Distortion.
Die Sends der JX-Parts (aus ihren Sounds) und alle Distortion-Sends kommen einen Block (1,45 ms) später zu den Effekten.

## Bauen und messen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/jxsh/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/jxsh/build/jxsh.fwsc $PWD/bausteine/varianten/jxsh/build/dinghy.elf
```

Messbau: `GM_EXTRA_CFLAGS=-DJXSH_LAST` (1: SH + JX, 2: nur JX, 3: nur SH) spielt nach 0,4 s den schwersten Fall von
selbst (nur Emulator, nie ins Paket).

## Offen

- Sequencer der iOS-Apps (SequencerViews.swift, PatternBank.swift): bisher der Sequencer des Gerüsts; die Erweiterung
  (Pattern-Bank, Akzent/Tie je Step) folgt abwärtskompatibel im Gerüst.
- Speicherkennung `0x48535831` („1XSH“), Journal `0x48535854` („TXSH“), Datenfamilie JXSH.
