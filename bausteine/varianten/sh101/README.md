# SH-101 solo

Der Baustein [`../../sh101`](../../sh101/) (SH-101 nach MonoBass) als eigene Firmware für das M-VAVE FM-1, auf dem
Gerüst [`../gemeinsam`](../gemeinsam/). Entscheidungen: [ADR 0042](../../../docs/adr/0042-sh101-im-geraet.md),
[ADR 0044](../../../docs/adr/0044-solo-firmware-geruest.md). Feature Y12.

## Stand (28.11.2026)

| | |
|---|---|
| Firmware | `firmware/SH101-0.1.6-alpha.fwsc` (Kennung FM-1_94), `tools/fm1-emu/check.sh` ok |
| 0.1.5 | Grafik-Kopf (ADR 0055): LFO, ENV mit Punkt der Note, VCF-Kurve, DIST-Kennlinie, Delay, Hall; gedrehter Regler mit Wert |
| 0.1.4 | Fehler behoben (Gerätetest 0.1.3: Delay und Hall stumm, leises Knacksen): Kern 1 las die Sends, während Kern 0 sie für den nächsten Block schon löschte; jetzt liest er einen Abzug (`fx_src`, ADR 0044). Emulator: Note mit Delay/Hall klingt nach (0,1.3: nach 0,1 s Pegel 0, Spitze 59; 0.1.4: Ausklang über 3 s) |
| Klang | eine Stimme in voller Gerätequalität: 4× Ladder, 8× Clipper, BLEP; float und Polynome (`SH101_FLOAT`, `SH101_FASTMATH`), RMS ≤ 6e-5 neben der Vorlage |
| 0.1.2 | Distortion-Send wie FM1-Electro (TUBE, OD, DIST, RAT; Seite FX/DIST), Kern 1; Drive bleibt; mono; Delay ≤ 0,6 s. Kern 0 57,5 %, Kern 1 13,9 % mit allen Effekten |
| Last (Emulator) | eine Note 44 %, mit Drive und hoher Resonanz 58,5 % Spitze (Kern 0); Delay + Hall 6,6 % (Kern 1); keine verspäteten Blöcke. Kostenmodell des Emulators: 98 % Laufzeit mit Note (MKS-50 zum Abgleich 80 %, dort Gerät = Zeitgeber), Einordnung in ADR 0042. **Am Gerät nicht gemessen** |
| Am Gerät | nicht geprüft |

## Bedienung

Raster wie in allen Firmwares aus fm1-synths: KNOB 1–4 = die vier Felder unten, SELECT = Seite, PRESETS = Sound,
Taste tippen = ihre Seiten, halten = ihre vier wichtigsten Werte. Regler 0.0 … 10.0 wie die Schieber des SH-101.

| Taste | Seiten (KNOB 1–4) | gehalten |
|---|---|---|
| HOME | PLAY (CUTOFF RESO ENV DECAY, Name, Oszilloskop), PERFORM (GLIDE DRIVE VOLUME TRANSP) | |
| EDIT | VCO (RANGE MOD PW PWM SRC), SOURCE (PULSE SAW SUB NOISE), SOURCE 2 (SUB OCT WHITE FINE FLOOR), VCF (CUTOFF RESO ENV KYBD), VCF 2 (VCF MOD SLOPE ENV FULL RES COMP), FM + DRIVE | PULSE SAW SUB PW |
| ENV | ENV mit Bild (A D S R), VCA (VCA TRIGGER DECLICK), VELOCITY | A D S R |
| LFO | LFO (RATE WAVE MOD VCF MOD), LFO 2 (SYNC INVERT KEY TRIG SNAP), CONTROL (Bender, Mod-Rad), GLIDE | RATE MOD VCF MOD WAVE |
| FX | EFFECTS (Sends, Pegel), DIST (Send, Modus, Drive, Tone), DIST 2 (Level), DELAY, DELAY 2, REVERB | DLY/REV/DST SEND, Zeit |
| ARP | ARP (an, Modus, Rate, Oktaven), ARP 2 (Gate, Hold) | |
| SEQ | SEQUENCER (an, Rate, Gate, Transponieren), LOOP (Live-Aufnahme des Gerüsts, ADR 0053) | |
| GLO | TEMPO (Tempo, Clock-Quelle, Clock senden, MIDI-Kanal), SYSTEM (Pegel, Last) | GLIDE DRIVE TRANSP VOLUME |
| SEL | Soundliste | |
| REC | Schritte aufnehmen wie am SH-101 (oberste Taste = Pause, gehalten = gebunden) | |
| PLAY | tippen: Arp-HOLD; halten: Start/Stop | |
| SAVE | kurz: speichern; lang: speichern unter | |

Gebundene Schritte und gehaltene Tasten spielen legato; mit GLIDE-Modus AUTO gleitet es dann wie beim SH-101.

## Sounds

INIT (Vorgaben des Bausteins) und 14 eigene Einstellungen: BASS (SAW BASS, SUB BASS, ACID, PULSE BASS, SQUELCH), LEAD
(LEAD, PWM LEAD, WHISTLE), SEQUENCE (PLUCK, BLIP, AUTO TRIG), FX (NOISE HAT, SIREN, RANDOM). Nach Gehör anzupassen;
die Vorlage hat keine Werkssounds. Eigene Sounds: 40 Plätze, Formatversion 1 (`GM_SOUND_VER`); spätere Fassungen mit
mehr Parametern laden diese und ergänzen den Rest aus INIT.

## Bauen

```sh
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 bausteine/varianten/sh101/tools/build.py
tools/fm1-emu/check.sh ~/fm1-emu $PWD/bausteine/varianten/sh101/build/sh101.fwsc $PWD/bausteine/varianten/sh101/build/dinghy.elf
```

Aufspielen wie die anderen Pakete: `python3 bausteine/varianten/gemeinsam/platform/tools/fm1_install.py <Paket>` oder
über den Firmware Hub. Neue Fassung: `VERSION` erhöhen (Z), bauen, prüfen, `firmware/SH101-<Version>.fwsc` ersetzen (die alte entfernen).

## Offen

- Am Gerät: Last mit Drive (Seite SYSTEM), Bedienung, Pegel (`SH_GAIN`).
- Speichern (eigene Sounds, Journal, Sequenzen): gebaut wie in fm1-synths, im Emulator nicht über einen Neustart geprüft.
- Speicher knapp (RAM 4 kB frei, Pool 2 kB): neue Funktionen brauchen erst Platz.
- Bilder für VCO und VCF (bisher nur ENV und PLAY).
- Velocity: die FM-1-Tasten spielen mit 100; per MIDI mit Anschlag.
