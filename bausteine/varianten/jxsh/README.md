# JX-8P + SH-101 (Kombi)

Vier Parts, jeder spielt den **JX-8P** (Pool mit 4 Stimmen) oder den **SH-101** (mono, höchstens ein Part), auf dem
Gerüst [`../gemeinsam`](../gemeinsam/) mit `GM_ENGINES = 2`. Entscheidung: [ADR 0052](../../../docs/adr/0052-kombi-jx8p-sh101.md),
Feature P16. Klang: der Pool-Kern `jx.c` der JX-8P-Firmware ([`../jx8p/engine`](../jx8p/engine/), bitgleich mit dem Baustein
`jx8p`) und der Baustein [`../../sh101`](../../sh101/) mit denselben Geräteschaltern wie SH-101 solo.

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/JXSH-0.1.3-alpha.fwsc` (Kennung FM-1_99), `tools/fm1-emu/check.sh` ok (Paket, Messbau und ein Lauf mit Tasten durch PARTS, Erzeugerwechsel, DIST SEND, MIDI) |
| 0.1.3 | Grafik-Kopf (ADR 0055) je Erzeuger: LFO, ENV (JX: ENV-1/2 mit der jüngsten Stimme), VCF, DIST, Delay, Hall |
| Last (Emulator, Block 64) | SH-101 SQUELCH + 4 JX-Stimmen (STRINGS 01, Chorus voll) + Delay + Hall + Distortion RAT (alle Sends 64): **Kern 0 60 %** (Spitze 61 %), **Kern 1 36 %**, keine verspäteten Blöcke; ohne Distortion Kern 1 28 % (sie kostet 7,4 %). Nur JX: Kern 0 23 %, Kern 1 36 %; nur SH: Kern 0 60 %, Kern 1 8 %. Ohne laufenden Sequencer/Arp gemessen (Kern 0, klein). **Am Gerät nicht gemessen** |
| Speicher | RAM 85 204 von 98 304 B (13,1 kB frei), Pool 341 052 von 344 064 B; Hall-Diffusoren im RAMTEXT, Bildpuffer im NOINIT |
| Pegel (0.1.2) | Gain-Staging je Erzeuger, damit gleicher MIX gleich laut ist: gemessen (Emulator, je eine Note C, Median der klingenden Werkssounds) JX 44,9 dB, SH 63,4 dB; Trim SH −6 dB (`JXSH_TRIM_SH`), JX +12,5 dB (`JXSH_TRIM_JX`, Sends mit). Danach JX 55,8 dB, SH 57,4 dB |
| Am Gerät | nicht geprüft |

## Bedienung

Wie die Kombi MKS-50 + JX-8P aus fm1-synths. ALGORITHM wählt den Part (A–D, oben im Bild), die Seiten folgen dem
Klangerzeuger des gewählten Parts.

| Taste | Seiten |
|---|---|
| HOME | PLAY, **PARTS** (Klangerzeuger je Part: KNOB 1–4 = A–D, JX-8P oder SH-101; ein zweiter SH-101 wird abgewiesen), **MIX** (Pegel A–D) |
| EDIT, ENV, LFO | Klang des gewählten Parts: JX-8P wie JX-8P solo (DCO-1 … VCA, ENV-1/2, LFO), SH-101 wie SH-101 solo |
| FX | **DIST** (Modus TUBE/OD/DIST/RAT, Drive, Tone, Level; für alle Parts), **DIST SEND** (A–D), DELAY (≤ 0,6 s), HALL; JX: CHORUS und Sends im Sound; SH: SENDS |
| ARP, SEQ | Arp des Gerüsts; Sequencer wie die iOS-Apps (unten) und LOOP (Live-Aufnahme, ADR 0053) |
| GLO | TEMPO, **MIDI** (Kanal je Part), SYSTEM (Pegel, Last); JX: VOICE |
| SEL | Soundliste: EIGENE (die eigenen JX-8P-Presets des Nutzers) zuerst, dann die JX-Werksgruppen, JX INIT, SH BASS/LEAD/SEQ/FX |

Start: A–C die eigenen Presets (STRINGS 01, HI STRI 01, SYNTH B 01), D SH-101 SAW BASS. Wechselt ein Part den
Klangerzeuger, lädt er dessen INIT; die Soundliste zeigt nur Sounds seines Erzeugers. Ein zweiter SH-101 wird mit
„SH-101 IN USE“ abgewiesen. FX gehalten (SH-Part): KNOB 4 = DIST-Send des gewählten Parts. Pitch-Bend und Mod-Rad wirken
nur auf den SH-101 (der JX-Kern hat keine, wie JX-8P solo).

## Sequencer (wie die iOS-Apps JX-8P/MKS-50, `GM_SEQ_IOS`)

Seiten unter SEQ (SELECT blättert; SEQ tippen startet/stoppt, sobald eine Folge da ist):

| Seite | KNOB 1–4 |
|---|---|
| SEQUENCER | SEQ an/aus, RATE, GATE (bis 100 % eines Schritts, darüber 1–8 ganze Schritte), G.VAR (lange Noten aus festem Zufall je Schritt) |
| PATTERN | STYLE (KRAFT, ROTHER, ROBOT, ACID, MIAMI), NEW (nach rechts: neue 16er-Folge), VARY (nach rechts: 1–2 Schritte ändern), LENGTH 1–64 |
| STEP | STEP (Schritt wählen, Strich im Bild), NOTE (ganz links REST), ACCENT, TIE |
| BANK | SLOT 1–8 (wählen = spielen; läuft die Folge, wechselt sie am Ende der Folge: „SLOT 3 NEXT“), STORE (nach rechts: Folge in den Platz), CLEAR, SHIFT |

Akzent: Anschlag 127 statt 88 und GATE + 15 %; Tie: legato in den nächsten Schritt. Schrittaufnahme mit REC wie bisher.
Die Bank (8 Plätze, ganze Folge mit RATE, GATE, G.VAR) liegt in Blob 5. Modul `gemeinsam/shared/seq/syn_pat.h`, Host-Test
`test_pat.c` (`make -C bausteine test`).

## Kerne

Kern 0: Gerüst, Sequencer und der SH-101-Part. Kern 1 (Haken `GM_FX_EXTRA`): der JX-8P-Pool, Delay, Hall, Distortion.
Die Sends der JX-Parts (aus ihren Sounds) und alle Distortion-Sends kommen einen Block (1,45 ms) später zu den Effekten.

## Bauen und messen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/jxsh/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/jxsh/build/jxsh.fwsc $PWD/bausteine/varianten/jxsh/build/dinghy.elf
```

Messbau: `GM_EXTRA_CFLAGS=-DJXSH_LAST` (1: SH + JX, 2: nur JX, 3: nur SH, 4: SH + JX ohne Distortion; Modus RAT) spielt nach 0,4 s den schwersten Fall von
selbst (nur Emulator, nie ins Paket).

## Offen

- Speicherkennung `0x48535831` („1XSH“), Journal `0x48535854` („TXSH“), Datenfamilie JXSH.
