# MKS-50 solo

Der Baustein [`../../mks50`](../../mks50/) (der MKS-50 / Alpha Juno von FM1-Electro, `engine/mks.c`, Port der
MKS-50-iOS-App des Nutzers) als eigene Firmware für das M-VAVE FM-1, auf dem Gerüst [`../gemeinsam`](../gemeinsam/).
Löst die MKS-50-Firmware aus fm1-synths ab (gleiche Kennung FM-1_95, eigene Speicherkennung). Entscheidungen:
[ADR 0047](../../../docs/adr/0047-mks50-baustein.md), [ADR 0044](../../../docs/adr/0044-solo-firmware-geruest.md),
eigene Presets [ADR 0051](../../../docs/adr/0051-eigene-presets.md). Feature Y15. Nicht zu verwechseln mit MKS Ultra
(`mksultra`, TAL-Pha-Nachbau, FM-1_93).

Die Kennung FM-1_95 ersetzt die fm1-synths-Fassung. Deren Speicher (`STORE_MAGIC` „MK50“, eigene Sounds, Sets) liest
diese Firmware nicht: sie beginnt leer und überschreibt den Bereich beim ersten Speichern
(`gemeinsam/platform/firmware/src/store.h`). Die fm1-synths-Fassung hatte 4 Parts mit Stimmen-Pool, MIX und SETS; diese
hat einen Part mit allen 6 Stimmen.

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | 0.1.0-alpha gebaut, `tools/fm1-emu/check.sh` ok; **noch kein Paket**: zuerst Gleichstand mit der fm1-synths-Fassung (wie bei JX-8P solo) |
| Klang | der Kern von FM1-Electro unverändert, 6 Stimmen (alle, die der Kern hat), Chorus je Sound, Delay und Hall des Gerüsts |
| Last (Emulator) | 6 gehaltene Noten: Kern 0 ≈ 41–43 % (poly, 6 Stimmen), mono ≈ 10–13 %; Delay + Hall 7–19 % (Kern 1); keine verspäteten Blöcke |
| Geräteunterschied | auf dem Gerät rechnet der Kern die Ladder mit `MKS_VCF_DECIM=2` (Standard auf der eigenen Plattform), der Baustein-Test auf dem PC mit 1; nach Gehör vergleichen |
| Am Gerät | nicht geprüft |

## Bedienung

Raster wie in allen Firmwares aus fm1-synths: KNOB 1–4 = die vier Felder unten, SELECT = Seite, PRESETS = Sound,
Taste tippen = ihre Seiten, halten = ihre vier wichtigsten Werte. Seitenaufteilung nach der fm1-synths-Fassung
(`mks50/app/mk_ui.c`).

| Taste | Seiten (KNOB 1–4) | gehalten |
|---|---|---|
| HOME | PLAY (CUTOFF RESO ENV RELEASE, Name, Oszilloskop), PERFORM (CHORUS DELAY REVERB VOLUME) | |
| EDIT | OSC (PULSE SAW SUB RANGE), OSC 2 (SUB LVL NOISE PW PWM RT), OSC MOD (LFO ENV ENV-MODUS AFTER), SYNC (DETUNE SYNC SYN ENV DRIFT), FILTER (CUTOFF RESO ENV KEY), FILTER 2 (LFO ENV-MODUS HPF AFTER), AMP (LEVEL MODE VOLUME AFTER) | PULSE, SAW, SUB, PW |
| ENV | ENV mit Bild (T1 L1 T2 L2), ENV 2 (T3 L3 T4 KEY) | ATTACK (T1), DECAY (T2), SUSTAIN (L3), RELEASE (T4) |
| LFO | LFO (RATE DELAY WAVE SYNC), LFO 2 (NOTE KEYSYNC), VOICE (MODE PORTA GLIDE SHIFT), VOICE 2 (RETRIG UNI DET CHORD) | RATE, DELAY, PITCH, FILTER |
| FX | CHORUS (OFF/FREE/I/II/I+II, RATE), SENDS (Delay- und Hall-Anteil des Sounds, Pegel von Delay und Hall), DELAY, DELAY 2, REVERB | DELAY, REVERB, TIME, CHORUS |
| ARP, SEQ, GLO, SEL, REC, PLAY, SAVE | wie im Gerüst (Arp, Step-Sequencer, Tempo/MIDI/System, Soundliste, Aufnahme, Start/Stop, Speichern) | |

## Sounds

79, nach Gruppen (`app/mk_sounds.h`, erzeugt von `tools/sounds.py`): EIGENE (die 23 eigenen Presets des Nutzers aus der
iOS-App, vorn), dann die 56 Werks-Sounds der MKS-50-App nach ihren Gruppen (BASS, SEQUENCE, ZAPS, STAB, KEYS, STR+BRASS,
PAD, FX+PERC). Dazu INIT (INIT TONE der App) und 40 Plätze für eigene Sounds (Formatversion 1: 54 Rohwerte). Neue
Presets vom iPhone: `tools/iphone_presets_pull.sh mks50`, dann `python3 bausteine/varianten/mks50/tools/sounds.py`
und neu bauen (der Build bricht ab, wenn die Liste nicht passt).

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/mks50/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/mks50/build/mks50.fwsc $PWD/bausteine/varianten/mks50/build/dinghy.elf
```

Stimmen: `MK_VOICES=<n>` vor dem Build (Standard 6). Neue Fassung: `VERSION` erhöhen (Z), bauen, prüfen,
`firmware/MKS50-<Version>.fwsc` ersetzen (die alte entfernen).

## Offen

- Am Gerät: Last (Seite SYSTEM), Pegel, Bedienung; `MKS_VCF_DECIM=2` gegen den Baustein abhören.
- Pitch Bend, Modulationsrad und Aftertouch: der Kern hat keine Eingänge dafür (wie in FM1-Electro); BEND und die
  AFTER-Tiefen stehen nur im Sound.
- Speichern (eigene Sounds, Journal, Sequenzen): gebaut wie im Gerüst, im Emulator nicht über einen Neustart geprüft.
