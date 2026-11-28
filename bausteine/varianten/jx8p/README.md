# JX-8P solo

Der Baustein [`../../jx8p`](../../jx8p/) (der JX-8P von FM1-Electro, `engine/jx8p.c`, aus fm1-synths, dort aus der
JX-8P-iOS-App des Nutzers) als eigene Firmware für das M-VAVE FM-1, auf dem Gerüst [`../gemeinsam`](../gemeinsam/).
Löst die JX-8P-Firmware aus fm1-synths ab (gleiche Kennung FM-1_97, eigene Speicherkennung). Entscheidungen:
[ADR 0046](../../../docs/adr/0046-jx8p-baustein.md), [ADR 0044](../../../docs/adr/0044-solo-firmware-geruest.md),
eigene Presets [ADR 0051](../../../docs/adr/0051-eigene-presets.md). Feature Y14.

Die Kennung FM-1_97 ersetzt die fm1-synths-Fassung. Deren Speicher (`STORE_MAGIC` „JX8P“, eigene Sounds, Sets) liest diese Firmware nicht: sie beginnt leer und überschreibt den Bereich beim ersten Speichern (`platform/firmware/src/store.h`). Die fm1-synths-Fassung hatte 4 Parts mit Stimmen-Pool, MIX und SETS; diese hat einen Part mit allen 6 Stimmen.

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/JX8P-0.1.0-alpha.fwsc` (Kennung FM-1_97, `STORE_MAGIC` „JX01“, Journal „JXST“), `tools/fm1-emu/check.sh` ok |
| Klang | der Kern von FM1-Electro unverändert, 6 Stimmen (alle, die der Kern hat), Chorus je Sound, Delay und Hall des Gerüsts |
| Last (Emulator) | 6 gehaltene Noten, verschiedene Sounds: Kern 0 ≈ 37–41 % im Mittel, höchstens 47,6 % (HI STRI 01, Akkord-Modus); Delay + Hall 12 % (Kern 1); keine verspäteten Blöcke. Ziel < 70 % erfüllt; am Gerät nach FM1-Electro etwa 10 % mehr |
| Am Gerät | nicht geprüft |

## Bedienung

Raster wie in allen Firmwares aus fm1-synths (ADR 0008 dort): KNOB 1–4 = die vier Felder unten, SELECT = Seite,
PRESETS = Sound, Taste tippen = ihre Seiten, halten = ihre vier wichtigsten Werte. Werte wie der JX-8P: 0–127,
Schalter mit ihren Namen. Seitenaufteilung nach der fm1-synths-Fassung (`jx8p/app/jx_ui.c`).

| Taste | Seiten (KNOB 1–4) | gehalten |
|---|---|---|
| HOME | PLAY (CUTOFF RESO ENV RELEASE, Name, Oszilloskop), PERFORM (CHORUS DELAY REVERB VOLUME) | |
| EDIT | DCO-1 (RANGE WAVE TUNE LFO), DCO-2 (RANGE WAVE X-MOD TUNE), DCO-2 FINE (FINE LFO DRIFT), DCO ENV (DCO-1 DCO-2 ENV-MODUS DYNA), MIXER (DCO-1 DCO-2 ENV ENV-MODUS), MIXER DYN, VCF (CUTOFF RESO ENV HPF), VCF MOD (LFO KEY ENV-MODUS DYNA), VCA (LEVEL MODE DYNA VOLUME) | X-MOD, 2 TUNE, MIX 1, MIX 2 |
| ENV | ENV-1 und ENV-2 mit Bild (A D S R), ENV KEY (1 KEY, 2 KEY, SNAP, RETRIG) | 1 ATT, 1 DEC, 2 ATT, 2 REL |
| LFO | LFO (RATE DELAY WAVE SYNC), LFO 2 (SHAPE NOTE KEYSYNC), VOICE (MODE PORTA GLIDE SHIFT), VOICE 2 (UNI DET CHORD DRIFT) | RATE, DELAY, PITCH, FILTER |
| FX | CHORUS (an/I/II, MODE, RATE), SENDS (Delay- und Hall-Anteil des Sounds, Pegel von Delay und Hall), DELAY, DELAY 2, REVERB | DELAY, REVERB, TIME, CHORUS |
| ARP | ARP (an, Modus, Rate, Oktaven), ARP 2 (Gate, Hold) | |
| SEQ | SEQUENCER (an, Rate, Gate, Transponieren); tippen auf der Seite: an/aus | |
| GLO | TEMPO (Tempo, Clock-Quelle, Clock senden, MIDI-Kanal), PATCH (BEND, BEND LFO, MOD SENS: im Sound gespeichert, der Kern nutzt sie nicht), SYSTEM (Pegel, Last) | MODE, GLIDE, SHIFT, VOLUME |
| SEL | Soundliste nach Gruppen | |
| REC / PLAY / SAVE | wie im Gerüst: Schritte aufnehmen, Arp-HOLD bzw. Start/Stop, speichern | |

KEY MODE: POLY, CHORD (eine Taste spielt den Akkord aus CHORD, die acht Akkorde von FM1-Electro), MONO, UNISON
(alle 6 Stimmen, UNI DET). Gebundene Sequencer-Schritte spielen legato (Portamento AUTO gleitet).

## Sounds

74, in dieser Reihenfolge und nach Gruppen (`app/jx_sounds.h`, erzeugt von `tools/sounds.py`):

| Gruppe | Sounds |
|---|---|
| EIGENE | die 7 eigenen Presets des Nutzers aus der iOS-App (STRINGS 01, HI STRI 01, SYNTH B 01, PG8X IN 01–03, SPRUNG ODE), vorn |
| BASS, LEAD, STAB, KEYS, BELLS, STR+BRASS, PAD, FX+PERC | die 64 Werks-Töne der JX-8P-ROM, Gruppen wie in der App (fm1-synths) |
| INIT AREA | INIT RHY, INIT STR, INIT BASS (mono) aus FM1-Electro |

Dazu INIT (PG8X INIT) und 40 Plätze für eigene Sounds (Formatversion 1: 68 Rohwerte). Neue Presets vom iPhone:
`tools/iphone_presets_pull.sh jx8p`, dann `python3 bausteine/varianten/jx8p/tools/sounds.py` und neu bauen.

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/jx8p/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/jx8p/build/jx8p.fwsc $PWD/bausteine/varianten/jx8p/build/dinghy.elf
```

Stimmen: `JX_VOICES=<n>` vor dem Build (Standard 6 = alle des Kerns). Neue Fassung: `VERSION` erhöhen (Z), bauen,
prüfen, `firmware/JX8P-<Version>.fwsc` ersetzen (die alte entfernen).

## Offen

- Am Gerät: Last (Seite SYSTEM), Pegel, Bedienung.
- Pitch Bend und Modulationsrad per MIDI werden nicht ausgewertet: der Kern hat keine Eingänge dafür (wie in FM1-Electro);
  BEND, BEND LFO und MOD SENS (Seite PATCH) stehen nur im Sound und wirken nicht. Nächste Fassung: Seite PATCH weglassen
  oder Bender im Kern nachrüsten (Schalter, ADR 0006).
- Mehr als 6 Stimmen oder Stimmen auf Kern 1: braucht eine Änderung am Kern (`JX8P_MAX_VOICES`) bzw. am Gerüst.
- Speichern (eigene Sounds, Journal, Sequenzen): gebaut wie im Gerüst, im Emulator nicht über einen Neustart geprüft.
