# jp6: Jupiter-6 als Baustein

**Kurz:** Der Jupiter-6, den FM1-Electro als zweiten Synth spielt (`engine/jp6.c` mit `engine/dist.c`, Port der
JP-6-iOS-App des Nutzers), als Baustein nach `baustein.h`. Der Ordner **bindet die Dateien aus `engine/` nur ein**
(keine Kopie); `engine/` bleibt unverändert, der Klang ist bitgleich mit dem Kern. Entscheidung:
[ADR 0048](../../docs/adr/0048-jp6-baustein.md), eigene Presets: [ADR 0051](../../docs/adr/0051-eigene-presets.md).

## Dateien

| Datei | Wofür |
|---|---|
| `jp6_bs.h`, `jp6_bs.c` | Adapter auf `baustein.h` (`jp6_baustein`), dazu Presets, Rohwerte, Tempo, Sends |
| `jp6_eigene.h`, `jp6_eigene.c` | Eigene Presets des Nutzers (erzeugt, eingecheckt) |
| `tools/eigene2c.py` | Erzeugt `jp6_eigene.c` aus allen JSON in „Eigene Presets iPhone/jp6/“ (Dropbox) |
| `test/test.c` | Host-Test über `baustein.h`, Lastmessung |
| `ref/sums.txt`, `ref/sums4.txt` | Prüfsummen je Preset (6 und 4 Stimmen) |
| eingebunden aus `engine/` | `jp6.c`, `jp6.h`, `jp6_presets.c`, `dist.c/.h`, `chords.c/.h`, `el_config.h`; dazu `sdk_include/fm1_block.h` und `third_party/daisy808/dsp_math.h` |

Gebaut mit `-DFM1_HOST_TEST` und den Standardschaltern des Kerns, wie `jp6_test` im Hauptordner.

## Unterschiede Gerät / Host (für die Solo-Firmware)

| Schalter | Host (Baustein, Prüfsummen) | eigene Plattform (`EL_PLATFORM`) | Wirkung |
|---|---|---|---|
| `DIST_FIXED` (`engine/dist.h`) | 0 (float) | 1 (Festkomma) | Distortion in Festkomma; geprüft mit `make dist-fixed-check` im Hauptordner |
| `DIST_TABLE` (`engine/dist.c`) | 0 | 1, bei `DIST_FIXED=1` aber 0 | Kennlinie aus Tabelle |
| `MKS_CHORUS_HALF` (`engine/jp6.c`) | 0 | 0 | Chorus mit halber Rate, nur als A/B-Schalter |

Nur Presets mit Distortion (`JP_DIST_MODE` > 0) klingen damit auf dem Gerät anders als hier; die Solo-Firmware hört
sie im Emulator gegen den Baustein ab.

## Prüfen

```sh
make -C bausteine jp6          # Test (6 und 4 Stimmen)
make -C bausteine/jp6 last     # Rechenzeit je Stimme
make -C bausteine/jp6 eigene   # eigene Presets neu einlesen (braucht die Dropbox)
```

Der Test spielt mit **jedem** Preset (5 eigene, dann 74 Werks-Sounds) einen Akkord C-E-G (Velocity 100, 0,6 s gehalten,
1,6 s gesamt) einmal über `jp6_baustein` und einmal direkt über `engine/jp6.c`, wie `engine/synth.c` ihn aufruft.
Beide müssen Sample für Sample gleich sein, die Prüfsumme muss zu `ref/sums.txt` passen. Dazu: Werte der eigenen
Presets im Bereich, Parameter 0…1 → Rohwert, Akkord-Modus, Sends. WAV der eigenen Presets: `bausteine/build/jp6/wav/`.
Die Prüfsummen gelten für den Host-Compiler (clang, Apple Silicon, `-O2`, ohne `-ffp-contract=off` wie der Hauptordner).

Stand 28.11.2026: 79 Presets (5 eigene), alle bitgleich mit dem Kern, alle Prüfsummen gleich; `make test` im
Hauptordner grün.

## Voller Umfang (für die Solo-Firmware, FEATURES Y16)

- **Stimmen:** 6 (`JP6_MAX_VOICES`; der Jupiter-6 hat 6). Schalter `JP6_BS_VOICES` (Standard 6). Mit 4 Stimmen weichen im
  Test 14 Presets ab (Unisono und Akkord-Modus): 12 VCO 01, JUPI UN 01, JUPI UN 02, JUPI UNI, DETROIT ST, M9 DUB,
  4THS STAB, POWER 5+8, ORCH HIT, HPF STAB, UNI STRING, M9 STRINGS, NIGHTDRIVE, 12 VCO UNI.
- **Je Stimme:** zwei VCOs mit kombinierbaren Wellenformen (Dreieck, Säge, Puls, Rechteck; VCO 2 mit Rauschen,
  PolyBLEP), Sync 1→2 / 2→1, Cross-Mod (auch per ENV 1), PWM (LFO/MAN/ENV 1), nichtresonanter HPF, Multimode-VCF nach
  IR3109 (LPF 24 dB, BPF 12 dB, HPF 24 dB), zwei ADSR (ENV 1 umkehrbar), VCA, Velocity auf VCF und VCA, Drift.
- **Je Teil:** LFO mit Delay, 7 Formen, Tempo-Sync und Key-Sync, Portamento (an/aus/auto), Key-Modi Poly / Akkord /
  Mono / Unisono (6 Stimmen, Detune), Distortion (OD, DIST, RAT, TUBE), BBD-Chorus (FREE, I, II, I+II), Sends zu
  Delay und Hall (Effekte bringt die Firmware mit).
- **Abtastrate:** fest 44,1 kHz (`EL_SAMPLE_RATE`), Blöcke bis 128.
- **Velocity:** /127 wie `synth_track.c`, wirkt über `JP_VCF_VELO` und `JP_VCA_VELO`.

Last auf dem PC (Apple Silicon, `make last`, SYNC SOLO, gehaltene Noten): **≈ 13 ns je Stimme und Sample**, Grundlast
des Teils ≈ 6 ns, Chorus ≈ 6 ns; 6 Stimmen mit Chorus ≈ 92 ns je Sample. Teuerstes aller 79 Presets mit 6 Stimmen:
12 VCO 01 (eigenes), ≈ 137 ns je Sample (≈ 23 ns je Stimme samt Grundlast). Zum Vergleich jx8p ≈ 18 ns, mks50 ≈ 20 ns,
mksultra ≈ 100 ns je Stimme.

### Parameter (`set_param` mit 0…1, Rohwert = Wert × Höchstwert, gerundet; `jp6_bs_set_raw` direkt)

| Nr. | Kern | Anzeige | Bereich |
|---|---|---|---|
| 0 | `JP_VCO1_TRI` | VCO1 TRI | 0–1 |
| 1 | `JP_VCO1_SAW` | VCO1 SAW | 0–1 |
| 2 | `JP_VCO1_PULSE` | VCO1 PLS | 0–1 |
| 3 | `JP_VCO1_SQUARE` | VCO1 SQR | 0–1 |
| 4 | `JP_VCO1_RANGE` | VCO1 RNG | 0–4 |
| 5 | `JP_XMOD` | X-MOD | 0–127 |
| 6 | `JP_SYNC` | SYNC | 0–2 |
| 7 | `JP_VCO2_TRI` | VCO2 TRI | 0–1 |
| 8 | `JP_VCO2_SAW` | VCO2 SAW | 0–1 |
| 9 | `JP_VCO2_PULSE` | VCO2 PLS | 0–1 |
| 10 | `JP_VCO2_NOISE` | NOISE | 0–1 |
| 11 | `JP_VCO2_RANGE` | VCO2 RNG | 0–5 |
| 12 | `JP_VCO2_TUNE` | TUNE | 0–24 |
| 13 | `JP_VCO2_FINE` | FINE | 0–127 |
| 14 | `JP_PWM_DEPTH` | PW/PWM | 0–127 |
| 15 | `JP_PWM_SOURCE` | PWM SRC | 0–2 |
| 16 | `JP_VCO_LFO` | VCO LFO | 0–127 |
| 17 | `JP_VCO_ENV` | PITCH ENV | 0–127 |
| 18 | `JP_VCO_MOD_DEST` | MOD DEST | 0–2 |
| 19 | `JP_MIX` | MIX | 0–127 |
| 20 | `JP_HPF` | HPF | 0–127 |
| 21 | `JP_VCF_MODE` | VCF MODE | 0–2 |
| 22 | `JP_CUTOFF` | CUTOFF | 0–127 |
| 23 | `JP_RESO` | RESO | 0–127 |
| 24 | `JP_VCF_ENV` | VCF ENV | 0–127 |
| 25 | `JP_VCF_ENV_SRC` | ENV SRC | 0–1 |
| 26 | `JP_VCF_LFO` | VCF LFO | 0–127 |
| 27 | `JP_VCF_KEY` | KEY FLW | 0–127 |
| 28 | `JP_VCF_VELO` | VCF VELO | 0–127 |
| 29 | `JP_VCA_LEVEL` | LEVEL | 0–127 |
| 30 | `JP_VCA_LFO` | VCA LFO | 0–127 |
| 31 | `JP_VCA_VELO` | VCA VELO | 0–127 |
| 32 | `JP_LFO_RATE` | LFO RATE | 0–127 |
| 33 | `JP_LFO_DELAY` | LFO DLY | 0–127 |
| 34 | `JP_LFO_WAVE` | LFO WAVE | 0–6 |
| 35 | `JP_ENV1_A` | ENV1 A | 0–127 |
| 36 | `JP_ENV1_D` | ENV1 D | 0–127 |
| 37 | `JP_ENV1_S` | ENV1 S | 0–127 |
| 38 | `JP_ENV1_R` | ENV1 R | 0–127 |
| 39 | `JP_ENV1_KEY` | ENV1 KEY | 0–127 |
| 40 | `JP_ENV1_POL` | ENV1 POL | 0–1 |
| 41 | `JP_ENV2_A` | ENV2 A | 0–127 |
| 42 | `JP_ENV2_D` | ENV2 D | 0–127 |
| 43 | `JP_ENV2_S` | ENV2 S | 0–127 |
| 44 | `JP_ENV2_R` | ENV2 R | 0–127 |
| 45 | `JP_ENV2_KEY` | ENV2 KEY | 0–127 |
| 46 | `JP_XMOD_ENV` | X-MOD ENV | 0–127 |
| 47 | `JP_KEY_MODE` | KEY MODE | 0–3 |
| 48 | `JP_PORTA_TIME` | GLIDE | 0–127 |
| 49 | `JP_PORTA` | PORTA | 0–2 |
| 50 | `JP_VOLUME` | VOLUME | 0–127 |
| 51 | `JP_KEY_SHIFT` | SHIFT | 0–24 |
| 52 | `JP_DRIFT` | DRIFT | 0–127 |
| 53 | `JP_UNISON_DETUNE` | UNI DET | 0–127 |
| 54 | `JP_LFO_SYNC` | LFO SYNC | 0–1 |
| 55 | `JP_LFO_DIV` | LFO DIV | 0–13 |
| 56 | `JP_LFO_KEYSYNC` | LFO KEY | 0–1 |
| 57 | `JP_ENV_RETRIG` | RETRIG | 0–1 |
| 58 | `JP_CHORUS` | CHORUS | 0–1 |
| 59 | `JP_CHORUS_RATE` | CHO RATE | 0–127 |
| 60 | `JP_CHORUS_MODE` | CHO MODE | 0–3 |
| 61 | `JP_DIST_MODE` | DIST | 0–4 |
| 62 | `JP_DIST_DRIVE` | DRIVE | 0–127 |
| 63 | `JP_DIST_TONE` | DIST TONE | 0–127 |
| 64 | `JP_DIST_LEVEL` | DIST LVL | 0–127 |
| 65 | `JP_DIST_MIX` | DIST MIX | 0–127 |
| 66 | `JP_DELAY_SEND` | DELAY | 0–127 |
| 67 | `JP_REVERB_SEND` | REVERB | 0–127 |
| 68 | `JP_CHORD` | CHORD | 0–7 |

Werte der Schalter: `jp6_param_value_name` in `engine/jp6.c`.

## Eigene Presets (ADR 0051)

Die App JP-6 ist nicht mehr auf dem iPhone; „Eigene Presets iPhone/jp6/“ ist die Sicherung vom 05.11.2026 (5 Dateien).
`tools/eigene2c.py` liest **alle** `*.json` dort (Format der App, Quelle „MKS50 App/JP6-iOS/“: `ToneData` mit
`tone` = 50 Werte nach `Param.rawValue` 0…49 und `patch` = Werte nach `Param.rawValue`), bildet sie mit denselben Namen
wie `tools/convert_jp6_presets.py` auf die 69 Parameter ab, lässt reine Kopien von INIT TONE aus und schreibt
`jp6_eigene.c`. Die Nummern der App sind im Werkzeug festgehalten und passen genau zu den Schlüsseln der Sicherung.
CHORD sucht die Akkordform der Datei unter den acht Akkorden des Kerns. Was der Kern nicht kennt (Aftertouch, LFO 2,
Arp, Sequencer, Delay-/Hall-Einstellungen außer dem Anteil), fällt weg.

| Nr. | Name | Datei | Bereich |
|---|---|---|---|
| 1 | 12 VCO 01 | preset1.json | RHYTHM |
| 2 | JUPI UN 01 | preset2.json | RHYTHM |
| 3 | JUPI UN 02 | preset3.json | RHYTHM |
| 4 | JUPI BA 01 | preset4.json | BASS |
| 5 | INIT TO 01 | preset5.json | RHYTHM |

Keins ist eine reine INIT-Kopie: INIT TO 01 ist INIT TONE mit Chorus, Delay- und Hall-Anteil. JUPI UN 01 und
JUPI UN 02 haben dieselben Werte wie der Werks-Sound JUPI UNI.

## Was man nicht anfasst

`engine/` (gehört zum Gerätebuild, Bau-Chat). Änderungen am Klang nur dort, als Schalter (ADR 0006), danach
`make test` im Hauptordner und hier `make ref`.

## Nächster Schritt

Solo-Firmware „JP-6 solo“ (ADR 0048, FEATURES Y16) in `bausteine/varianten/jp6/`, sobald das Gerüst
`bausteine/varianten/gemeinsam/` abgenommen ist: eigene Presets vorn (Preset 1–5), Stimmenzahl im Emulator einmessen,
Distortion-Presets mit `DIST_FIXED=1` gegen den Baustein abhören, Delay/Hall des Gerüsts an `jp6_bs_render_sends`.
