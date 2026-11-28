# JP-6 solo

Der Baustein [`../../jp6`](../../jp6/) (der Jupiter-6 von FM1-Electro, `engine/jp6.c` mit `engine/dist.c`, Port der
JP-6-iOS-App des Nutzers) als eigene Firmware für das M-VAVE FM-1, auf dem Gerüst [`../gemeinsam`](../gemeinsam/).
Entscheidungen: [ADR 0048](../../../docs/adr/0048-jp6-baustein.md), [ADR 0044](../../../docs/adr/0044-solo-firmware-geruest.md),
eigene Presets [ADR 0051](../../../docs/adr/0051-eigene-presets.md). Feature Y16. Kennung FM-1_92 (neu, keine Vorgängerin),
`STORE_MAGIC` „JP01“, Journal „JPST“. **Ohne Distortion** (Wunsch des Nutzers 28.11.2026): statt `engine/dist.c` wird
`bausteine/jp6/dist_aus.c` gebunden (Schalter `JP6_BS_DIST=0`); die Distortion-Werte bleiben im Sound, haben aber keine
Seite und keine Wirkung. Ein Part, alle 6 Stimmen auf Kern 0, Kern 1 rechnet Delay und Hall (Gerüst, ADR 0044).

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/JP6-0.1.0-alpha.fwsc` (Kennung FM-1_92), `tools/fm1-emu/check.sh` ok |
| Klang | der Kern von FM1-Electro unverändert außer der Distortion (aus), 6 Stimmen (alle, die der Kern hat), Chorus je Sound, Delay und Hall des Gerüsts; Pegel wie in FM1-Electro (JP-6 × 1,4, `GM_OUT_GAIN`) |
| Last (Emulator) | LAST_ZEILE |
| Geräteunterschied | keine Distortion (FM1-Electro: Festkomma-Distortion); sonst rechnet der Kern wie im Baustein-Test |
| Am Gerät | nicht geprüft |

## Bedienung

Raster wie in allen Firmwares aus fm1-synths: KNOB 1–4 = die vier Felder unten, SELECT = Seite, PRESETS = Sound,
Taste tippen = ihre Seiten, halten = ihre vier wichtigsten Werte. Seiten nach dem Panel des Jupiter-6.

| Taste | Seiten (KNOB 1–4) | gehalten |
|---|---|---|
| HOME | PLAY (CUTOFF RESO ENV RELEASE, Name, Oszilloskop), PERFORM (CHORUS DELAY REVERB VOLUME) | |
| EDIT | VCO-1 (TRI SAW PULSE SQUARE), VCO-1 MOD (RANGE X-MOD XMD ENV SYNC), VCO-2 (TRI SAW PULSE NOISE), VCO-2 TUNE (RANGE TUNE FINE MIX), VCO MOD (LFO ENV DEST DRIFT), PWM (PW/PWM SOURCE), VCF (CUTOFF RESO ENV MODE), VCF MOD (HPF ENV SRC LFO KEY), VCA (LEVEL LFO VCF VEL VCA VEL) | X-MOD, 2 TUNE, MIX, PW/PWM |
| ENV | ENV-1 und ENV-2 mit Bild (A D S R), ENV KEY (1 KEY, 1 POL, 2 KEY, RETRIG) | 1 ATT, 1 DEC, 2 ATT, 2 REL |
| LFO | LFO (RATE DELAY WAVE SYNC), LFO 2 (NOTE KEYSYNC), VOICE (MODE PORTA GLIDE SHIFT), VOICE 2 (UNI DET CHORD) | RATE, DELAY, PITCH, FILTER |
| FX | CHORUS (an, MODE, RATE), SENDS, DELAY, DELAY 2, REVERB | DELAY, REVERB, TIME, CHORUS |
| ARP, SEQ, GLO, SEL, REC, PLAY, SAVE | wie im Gerüst | |

## Sounds

79, nach Gruppen (`app/jp_sounds.h`, erzeugt von `tools/sounds.py`): EIGENE (die 5 eigenen Presets aus der Sicherung
der iOS-App vom 05.11., vorn), dann die 74 Werks-Sounds der JP-6-App nach ihren Gruppen (BASS, LEAD, SEQUENCE, ZAPS,
STAB, KEYS, STR+BRASS, PAD, FX+PERC). Dazu INIT (INIT TONE der App) und 40 Plätze für eigene Sounds (Formatversion 1:
69 Rohwerte).

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/jp6/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/jp6/build/jp6.fwsc $PWD/bausteine/varianten/jp6/build/dinghy.elf
```

Stimmen: `JP_VOICES=<n>` vor dem Build (Standard 6). Neue Fassung: `VERSION` erhöhen (Z), bauen, prüfen,
`firmware/JP6-<Version>.fwsc` ersetzen (die alte entfernen).

## Offen

- Am Gerät: Last (Seite SYSTEM), Pegel, Bedienung.
- Pitch Bend und Modulationsrad: der Kern hat keine Eingänge dafür (wie in FM1-Electro).
- Speichern (eigene Sounds, Journal, Sequenzen): gebaut wie im Gerüst, im Emulator nicht über einen Neustart geprüft.
