# mks50: MKS-50 / Alpha Juno als Baustein

**Kurz:** Der MKS-50, den FM1-Electro bis 0.5.5-alpha spielte und der mit `EL_SYNTH_JX=0` weiter baut (`engine/mks.c`,
Port der MKS-50-iOS-App des Nutzers), als Baustein nach `baustein.h`. Der Ordner **bindet die Dateien aus `engine/`
nur ein** (keine Kopie); `engine/` bleibt unverändert, der Klang ist bitgleich mit dem Kern. Entscheidung:
[ADR 0047](../../docs/adr/0047-mks50-baustein.md), eigene Presets: [ADR 0051](../../docs/adr/0051-eigene-presets.md).
Nicht zu verwechseln mit [`mksultra/`](../mksultra/) (TAL-Pha-Nachbau, eigener Kern).

## Dateien

| Datei | Wofür |
|---|---|
| `mks50_bs.h`, `mks50_bs.c` | Adapter auf `baustein.h` (`mks50_baustein`), dazu Presets, Rohwerte, Tempo, Sends |
| `mks50_eigene.h`, `mks50_eigene.c` | Eigene Presets des Nutzers (erzeugt, eingecheckt) |
| `tools/eigene2c.py` | Erzeugt `mks50_eigene.c` aus allen JSON in „Eigene Presets iPhone/mks50/“ (Dropbox) |
| `test/test.c` | Host-Test über `baustein.h`, Lastmessung |
| `ref/sums.txt`, `ref/sums4.txt` | Prüfsummen je Preset (6 und 4 Stimmen) |
| eingebunden aus `engine/` | `mks.c`, `mks.h`, `mks_presets.c`, `chords.c/.h`, `el_config.h`; dazu `sdk_include/fm1_block.h` und `third_party/daisy808/dsp_math.h` |

Gebaut mit `-DFM1_HOST_TEST -DEL_SYNTH_JX=0` und den Standardschaltern des Kerns (`MKS_CHOP_GRID=1`,
`MKS_RELEASE_HURRY=1`, `MKS_VCF_DECIM=1` auf dem Host, `MKS_CHORUS_HALF=0`), wie `make test` im Hauptordner.
Auf der eigenen Plattform setzt `el_config.h`/`mks.c` `MKS_VCF_DECIM=2`; das prüft die Solo-Firmware im Emulator.

## Prüfen

```sh
make -C bausteine mks50          # Test (6 und 4 Stimmen)
make -C bausteine/mks50 last     # Rechenzeit je Stimme
make -C bausteine/mks50 eigene   # eigene Presets neu einlesen (braucht die Dropbox)
tools/iphone_presets_pull.sh mks50   # vorher vom iPhone holen (ADR 0051)
```

Der Test spielt mit **jedem** Preset (23 eigene, dann 56 Werks-Sounds) einen Akkord C-E-G (Velocity 100, 0,6 s
gehalten, 1,6 s gesamt) einmal über `mks50_baustein` und einmal direkt über `engine/mks.c`, wie `engine/synth.c` ihn
aufruft. Beide müssen Sample für Sample gleich sein, die Prüfsumme muss zu `ref/sums.txt` passen. Dazu: Werte der
eigenen Presets im Bereich, Parameter 0…1 → Rohwert, Akkord-Modus, Sends. WAV der eigenen Presets:
`bausteine/build/mks50/wav/`. Die Prüfsummen gelten für den Host-Compiler (clang, Apple Silicon, `-O2`, ohne
`-ffp-contract=off` wie der Hauptordner).

Stand 28.11.2026: 79 Presets (23 eigene), alle bitgleich mit dem Kern, alle Prüfsummen gleich; `make test` im
Hauptordner grün.

## Voller Umfang (für die Solo-Firmware, FEATURES Y15)

- **Stimmen:** 6 wie der MKS-50 (`MKS_MAX_VOICES`). Schalter `MKS50_BS_VOICES` (Standard 6). Mit 4 Stimmen weichen im
  Test 11 Presets ab, fast alle im Akkord-Modus (drei Tasten × Akkordnoten): JX STRI 01, TRANS E 02–04, INIT TO 04/05,
  SAW SWE 01, HOUSE STAB, ELEC STAB, GATE CHORD, STR CHORD.
- **Je Stimme:** DCO nach MB87123 (Puls, Sägezahn in 6 Formen, Sub in 6 Formen, Rauschen, PolyBLEP), Range 4'…32',
  Pulse-Detune/Sync im Stil des TAL-Pha, HPF in 4 Stufen, 24-dB-Ladder (ZDF), VCA, Hüllkurve mit 4 Segmenten
  (T1/L1/T2/L2/T3/L3/T4), Drift.
- **Je Teil:** LFO mit Delay, 7 Formen, Tempo-Sync und Key-Sync, PWM-LFO, BBD-Chorus (FREE, I, II, I+II), Portamento
  (an/aus/auto), Key-Modi Poly / Akkord / Mono / Unisono (6 Stimmen, Detune), Sends zu Delay und Hall (Effekte bringt
  die Firmware mit).
- **Abtastrate:** fest 44,1 kHz (`EL_SAMPLE_RATE`), Blöcke bis 128.
- **Velocity:** /127 wie `synth_track.c`. Aftertouch-Tiefen und Bender stehen in den Sounds, der Kern nutzt sie nicht.

Last auf dem PC (Apple Silicon, `make last`, PLUCK KEYS, gehaltene Noten): **≈ 20 ns je Stimme und Sample**,
Grundlast des Teils ≈ 5 ns, Chorus ≈ 10 ns; 6 Stimmen mit Chorus ≈ 136 ns je Sample. Teuerstes aller 79 Presets mit
6 Stimmen: WOBBLE 01 (eigenes), ≈ 239 ns je Sample (≈ 40 ns je Stimme samt Grundlast). Zum Vergleich jx8p ≈ 18 ns,
mksultra ≈ 100 ns je Stimme.

### Parameter (`set_param` mit 0…1, Rohwert = Wert × Höchstwert, gerundet; `mks50_bs_set_raw` direkt)

| Nr. | Kern | Anzeige | Bereich |
|---|---|---|---|
| 0 | `MP_DCO_ENV_MODE` | DCO EMODE | 0–3 |
| 1 | `MP_VCF_ENV_MODE` | VCF EMODE | 0–3 |
| 2 | `MP_VCA_ENV_MODE` | VCA MODE | 0–3 |
| 3 | `MP_PULSE` | PULSE | 0–3 |
| 4 | `MP_SAW` | SAW | 0–5 |
| 5 | `MP_SUB` | SUB | 0–5 |
| 6 | `MP_RANGE` | RANGE | 0–3 |
| 7 | `MP_SUB_LEVEL` | SUB LVL | 0–3 |
| 8 | `MP_NOISE` | NOISE | 0–3 |
| 9 | `MP_HPF` | HPF | 0–3 |
| 10 | `MP_CHORUS` | CHORUS | 0–4 |
| 11 | `MP_DCO_LFO` | VIBRATO | 0–127 |
| 12 | `MP_DCO_ENV` | PITCH ENV | 0–127 |
| 13 | `MP_DCO_AFTER` | DCO AFT | 0–127 |
| 14 | `MP_PW` | PW | 0–127 |
| 15 | `MP_PWM_RATE` | PWM RATE | 0–127 |
| 16 | `MP_CUTOFF` | CUTOFF | 0–127 |
| 17 | `MP_RESO` | RESO | 0–127 |
| 18 | `MP_VCF_LFO` | VCF LFO | 0–127 |
| 19 | `MP_VCF_ENV` | VCF ENV | 0–127 |
| 20 | `MP_VCF_KEY` | KEY FLW | 0–127 |
| 21 | `MP_VCF_AFTER` | VCF AFT | 0–127 |
| 22 | `MP_VCA_LEVEL` | LEVEL | 0–127 |
| 23 | `MP_VCA_AFTER` | VCA AFT | 0–127 |
| 24 | `MP_LFO_RATE` | LFO RATE | 0–127 |
| 25 | `MP_LFO_DELAY` | LFO DLY | 0–127 |
| 26 | `MP_T1` | ATTACK | 0–127 |
| 27 | `MP_L1` | L1 | 0–127 |
| 28 | `MP_T2` | DECAY | 0–127 |
| 29 | `MP_L2` | L2 | 0–127 |
| 30 | `MP_T3` | T3 | 0–127 |
| 31 | `MP_L3` | SUSTAIN | 0–127 |
| 32 | `MP_T4` | RELEASE | 0–127 |
| 33 | `MP_ENV_KEY` | ENV KEY | 0–127 |
| 34 | `MP_CHORUS_RATE` | CHO RATE | 0–127 |
| 35 | `MP_BENDER` | BEND | 0–12 |
| 36 | `MP_KEY_MODE` | KEY MODE | 0–3 |
| 37 | `MP_PORTA_TIME` | GLIDE | 0–127 |
| 38 | `MP_PORTA` | PORTA | 0–2 |
| 39 | `MP_VOLUME` | VOLUME | 0–127 |
| 40 | `MP_KEY_SHIFT` | SHIFT | 0–24 |
| 41 | `MP_PULSE_DETUNE` | DETUNE | 0–127 |
| 42 | `MP_PULSE_SYNC` | SYNC | 0–1 |
| 43 | `MP_PULSE_ENV` | SYNC ENV | 0–127 |
| 44 | `MP_DRIFT` | DRIFT | 0–127 |
| 45 | `MP_UNISON_DETUNE` | UNI DET | 0–127 |
| 46 | `MP_LFO_WAVE` | LFO WAVE | 0–6 |
| 47 | `MP_LFO_SYNC` | LFO SYNC | 0–1 |
| 48 | `MP_LFO_DIV` | LFO DIV | 0–13 |
| 49 | `MP_LFO_KEYSYNC` | LFO KEY | 0–1 |
| 50 | `MP_ENV_RETRIG` | RETRIG | 0–1 |
| 51 | `MP_DELAY_SEND` | DELAY | 0–127 |
| 52 | `MP_REVERB_SEND` | REVERB | 0–127 |
| 53 | `MP_CHORD` | CHORD | 0–7 |

Werte der Schalter: `mks_param_value_name` in `engine/mks.c`.

## Eigene Presets (ADR 0051)

`tools/eigene2c.py` liest **alle** `*.json` aus „Eigene Presets iPhone/mks50/“ (Format der App, Quelle
„MKS50 App/Mks50-iOS-/“: `ToneData` mit `tone` = 36 Werte #0…#35 und `patch` = Werte nach `Param.rawValue`), bildet sie
wie `tools/convert_presets.py` die Werks-Sounds auf die 54 Parameter ab, lässt reine Kopien von INIT TONE aus und
schreibt `mks50_eigene.c`. Abweichend von der App: CHORUS wird aus Schalter und CHORUS MODE der App zusammengesetzt
(AJ → FREE, I, II, I+II); CHORD sucht die Akkordform der Datei unter den acht Akkorden des Kerns. Was der Kern nicht
kennt (Arp, Sequencer, Distortion, Delay-/Hall-Einstellungen außer dem Anteil, HPF FREQ, ENV SNAP), fällt weg.

| Nr. | Name | Datei | Bereich |
|---|---|---|---|
| 1 | WOBBLE 01 | preset1.json | BASS |
| 2 | INIT TO 01 | preset2.json | RHYTHM |
| 3 | ZAP 01 | preset3.json | RHYTHM |
| 4 | TRANS E 01 | preset4.json | RHYTHM |
| 5 | PEW PEW 01 | preset5.json | RHYTHM |
| 6 | JX STRI 01 | preset6.json | STRINGS |
| 7 | SWEEP S 01 | preset7.json | RHYTHM |
| 8 | NUMBERS 01 | preset8.json | RHYTHM |
| 9 | WOBBLE 02 | preset9.json | RHYTHM |
| 10 | TRANS E 02 | preset10.json | RHYTHM |
| 11 | TRANS E 03 | preset11.json | RHYTHM |
| 12 | TRANS E 04 | preset12.json | RHYTHM |
| 13 | ROBO CH 01 | preset13.json | RHYTHM |
| 14 | ROBO CH 02 | preset14.json | RHYTHM |
| 15 | S&H BLE 01 | preset15.json | RHYTHM |
| 16 | INIT TO 02 | preset16.json | RHYTHM |
| 17 | INIT TO 03 | preset17.json | RHYTHM |
| 18 | INIT TO 04 | preset18.json | RHYTHM |
| 19 | INIT TO 05 | preset19.json | RHYTHM |
| 20 | INIT TO 06 | preset20.json | BASS |
| 21 | INIT TO 07 | preset21.json | BASS |
| 22 | SAW SWE 01 | preset22.json | RHYTHM |
| 23 | BOOM 80 01 | preset23.json | BASS |

Keins ist eine reine INIT-Kopie: die sieben „INIT TO …“ weichen in 8 bis 35 Werten von INIT TONE ab. preset18
(INIT TO 04) wählt in der App den Akkord MAJ7, den der Kern nicht hat; dort spielt der Akkord Nr. 6 des Kerns (MAJ).

## Was man nicht anfasst

`engine/` (gehört zum Gerätebuild, Bau-Chat). Änderungen am Klang nur dort, als Schalter (ADR 0006), danach
`make test` im Hauptordner und hier `make ref`.

## Nächster Schritt

Solo-Firmware „MKS-50 solo“ (ADR 0047, FEATURES Y15) in `bausteine/varianten/mks50/`, sobald das Gerüst
`bausteine/varianten/gemeinsam/` abgenommen ist: eigene Presets vorn (Preset 1–23), Stimmenzahl und `MKS_VCF_DECIM`
im Emulator einmessen, Delay/Hall des Gerüsts an `mks50_bs_render_sends`.
