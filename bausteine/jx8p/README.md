# jx8p: JX-8P als Baustein

**Kurz:** Der JX-8P, den FM1-Electro seit 0.5.6-alpha spielt (`engine/jx8p.c`, aus fm1-synths, dort aus der
JX-8P-iOS-App des Nutzers), als Baustein nach `baustein.h`. Der Ordner **bindet die Dateien aus `engine/` nur ein**
(keine Kopie); `engine/` bleibt unverändert, der Klang ist bitgleich mit dem Gerätebuild. Entscheidung:
[ADR 0046](../../docs/adr/0046-jx8p-baustein.md), eigene Presets: [ADR 0051](../../docs/adr/0051-eigene-presets.md).

## Dateien

| Datei | Wofür |
|---|---|
| `jx8p_bs.h`, `jx8p_bs.c` | Adapter auf `baustein.h` (`jx8p_baustein`), dazu Presets, Rohwerte, Tempo, Sends |
| `jx8p_eigene.h`, `jx8p_eigene.c` | Eigene Presets des Nutzers (erzeugt, eingecheckt) |
| `tools/eigene2c.py` | Erzeugt `jx8p_eigene.c` aus allen JSON in „Eigene Presets iPhone/jx8p/“ (Dropbox) |
| `test/test.c` | Host-Test über `baustein.h`, Lastmessung |
| `ref/sums.txt`, `ref/sums4.txt` | Prüfsummen je Preset (6 und 4 Stimmen) |
| eingebunden aus `engine/` | `jx8p.c`, `jx8p.h`, `jx8p_dsp.h`, `jx8p_dsp_inline.h`, `jx8p_names.c`, `jx8p_presets.c`, `chords.c/.h`, `el_config.h`; dazu `sdk_include/fm1_block.h` und `third_party/daisy808/dsp_math.h` |

## Prüfen

```sh
make -C bausteine jx8p          # Test (6 und 4 Stimmen)
make -C bausteine/jx8p last     # Rechenzeit je Stimme
make -C bausteine/jx8p eigene   # eigene Presets neu einlesen (braucht die Dropbox)
tools/iphone_presets_pull.sh jx8p   # vorher vom iPhone holen (ADR 0051)
```

Der Test spielt mit **jedem** Preset (eigene zuerst, dann die 64 Werks-Töne und 3 INIT) einen Akkord C-E-G
(Velocity 100, 0,6 s gehalten, 1,6 s gesamt) einmal über `jx8p_baustein` und einmal direkt über `engine/jx8p.c`,
so wie `engine/synth.c` ihn aufruft. Beide müssen Sample für Sample gleich sein, und die Prüfsumme muss zu
`ref/sums.txt` passen (fällt eine Klangänderung im Kern auf). Dazu: Werte der eigenen Presets im Bereich,
Parameter 0…1 → Rohwert, Akkord-Modus (auch beim Umschalten der Akkordform während gehalten), Sends. WAV der eigenen
Presets: `bausteine/build/jx8p/wav/`.

Stand 28.11.2026: 74 Presets (7 eigene), alle 74 bitgleich mit dem Kern, alle Prüfsummen gleich; `make test` im
Hauptordner grün. Die Prüfsummen gelten für den Host-Compiler (clang, Apple Silicon, `-O2`); ein anderer Compiler
darf andere float-Rundung haben, dann `make ref` nach einem grünen Vergleich mit dem Kern.

## Voller Umfang (für die Solo-Firmware, FEATURES Y14)

- **Stimmen:** 6 wie der JX-8P (`JX8P_MAX_VOICES`). Schalter `JX8P_BS_VOICES` (Standard 6). Mit 4 Stimmen weicht im
  Test nur **HI STRI 01** ab (eigenes Preset, Akkord-Modus: drei Tasten × vier Akkordnoten).
- **Je Stimme:** zwei DCOs (Rauschen/Rechteck/Puls/Sägezahn, Range 16'/8'/4'/2', SYNC1/SYNC2/XMOD), Mixer mit ENV auf
  DCO 2, HPF in 4 Stufen, 24-dB-Ladder (ZDF), VCA (GATE/ENV 2), zwei ADSR, Drift.
- **Je Teil:** LFO mit Delay, 8 Formen, Tempo-Sync und Key-Sync, BBD-Chorus (I, II, I+II, FREE), Portamento
  (an/aus/auto), Key-Modi Poly / Akkord / Mono / Unisono (6 Stimmen, Detune), Sends zu Delay und Hall (Effekte selbst
  bringt die Firmware mit, nicht der Baustein).
- **Abtastrate:** fest 44,1 kHz (`EL_SAMPLE_RATE`), Steuerschritt 16 Samples (`JX8P_CONTROL`), Blöcke bis 128.
- **Velocity:** /127 wie `synth_track.c`. Bender, Aftertouch und MOD SENS stehen in den Sounds, der Kern nutzt sie nicht.

Last auf dem PC (Apple Silicon, `make last`, E. BASS, gehaltene Noten): **≈ 18 ns je Stimme und Sample**, Grundlast
des Teils ≈ 5 ns, Chorus ≈ 4 ns; 6 Stimmen mit Chorus ≈ 115 ns je Sample (0,5 % eines Kerns in Echtzeit).
Zum Vergleich mksultra ≈ 100 ns je Stimme. Auf dem FM-1 ist der Kern bereits im Gerätebuild (FM1-Electro spielt ihn
mit bis zu 6 Stimmen je Bereich), die Solo-Firmware misst im Emulator nach.

### Parameter (`set_param` mit 0…1, Rohwert = Wert × Höchstwert, gerundet; `jx8p_bs_set_raw` direkt)

| Nr. | Kern | Anzeige | Bereich |
|---|---|---|---|
| 0 | `JX_DCO1_RANGE` | 1 RANGE | 0–3 |
| 1 | `JX_DCO1_WAVE` | 1 WAVE | 0–3 |
| 2 | `JX_DCO1_TUNE` | 1 TUNE | 0–24 |
| 3 | `JX_DCO1_LFO` | 1 LFO | 0–127 |
| 4 | `JX_DCO1_ENV` | 1 ENV | 0–127 |
| 5 | `JX_DCO2_RANGE` | 2 RANGE | 0–3 |
| 6 | `JX_DCO2_WAVE` | 2 WAVE | 0–3 |
| 7 | `JX_DCO2_XMOD` | X-MOD | 0–3 |
| 8 | `JX_DCO2_TUNE` | 2 TUNE | 0–24 |
| 9 | `JX_DCO2_FINE` | 2 FINE | 0–127 |
| 10 | `JX_DCO2_LFO` | 2 LFO | 0–127 |
| 11 | `JX_DCO2_ENV` | 2 ENV | 0–127 |
| 12 | `JX_DCO_DYNA` | DCO DYN | 0–3 |
| 13 | `JX_DCO_ENV_MODE` | DCO EMD | 0–3 |
| 14 | `JX_MIX_DCO1` | DCO1 | 0–127 |
| 15 | `JX_MIX_DCO2` | DCO2 | 0–127 |
| 16 | `JX_MIX_ENV` | MIX ENV | 0–127 |
| 17 | `JX_MIX_DYNA` | MIX DYN | 0–3 |
| 18 | `JX_MIX_ENV_MODE` | MIX EMD | 0–3 |
| 19 | `JX_HPF` | HPF | 0–3 |
| 20 | `JX_CUTOFF` | CUTOFF | 0–127 |
| 21 | `JX_RESO` | RESO | 0–127 |
| 22 | `JX_VCF_LFO` | VCF LFO | 0–127 |
| 23 | `JX_VCF_ENV` | VCF ENV | 0–127 |
| 24 | `JX_VCF_KEY` | KEY FLW | 0–127 |
| 25 | `JX_VCF_DYNA` | VCF DYN | 0–3 |
| 26 | `JX_VCF_ENV_MODE` | VCF EMD | 0–3 |
| 27 | `JX_VCA_LEVEL` | LEVEL | 0–127 |
| 28 | `JX_VCA_DYNA` | VCA DYN | 0–3 |
| 29 | `JX_CHORUS` | CHORUS | 0–2 |
| 30 | `JX_LFO_WAVE` | LFO WAVE | 0–2 |
| 31 | `JX_LFO_DELAY` | LFO DLY | 0–127 |
| 32 | `JX_LFO_RATE` | LFO RATE | 0–127 |
| 33 | `JX_E1_ATTACK` | 1 ATTACK | 0–127 |
| 34 | `JX_E1_DECAY` | 1 DECAY | 0–127 |
| 35 | `JX_E1_SUSTAIN` | 1 SUST | 0–127 |
| 36 | `JX_E1_RELEASE` | 1 REL | 0–127 |
| 37 | `JX_E1_KEY` | 1 KEY | 0–3 |
| 38 | `JX_E2_ATTACK` | 2 ATTACK | 0–127 |
| 39 | `JX_E2_DECAY` | 2 DECAY | 0–127 |
| 40 | `JX_E2_SUSTAIN` | 2 SUST | 0–127 |
| 41 | `JX_E2_RELEASE` | 2 REL | 0–127 |
| 42 | `JX_E2_KEY` | 2 KEY | 0–3 |
| 43 | `JX_VCA_ENV_MODE` | VCA MODE | 0–1 |
| 44 | `JX_BENDER` | BEND | 0–12 |
| 45 | `JX_BEND_LFO` | BEND LFO | 0–127 |
| 46 | `JX_AFTER_VIB` | AT VIB | 0–127 |
| 47 | `JX_AFTER_BRIL` | AT BRIL | 0–127 |
| 48 | `JX_AFTER_VOL` | AT VOL | 0–127 |
| 49 | `JX_CHORUS_RATE` | CHO RATE | 0–127 |
| 50 | `JX_CHORUS_MODE` | CHO MODE | 0–2 |
| 51 | `JX_KEY_MODE` | KEY MODE | 0–3 |
| 52 | `JX_PORTA_TIME` | GLIDE | 0–127 |
| 53 | `JX_PORTA` | PORTA | 0–2 |
| 54 | `JX_MOD_SENS` | MOD SENS | 0–127 |
| 55 | `JX_VOLUME` | VOLUME | 0–127 |
| 56 | `JX_KEY_SHIFT` | SHIFT | 0–24 |
| 57 | `JX_DRIFT` | DRIFT | 0–127 |
| 58 | `JX_UNISON_DETUNE` | UNI DET | 0–127 |
| 59 | `JX_LFO_SHAPE` | LFO SHP | 0–7 |
| 60 | `JX_LFO_SYNC` | LFO SYNC | 0–1 |
| 61 | `JX_LFO_DIV` | LFO DIV | 0–13 |
| 62 | `JX_LFO_KEYSYNC` | LFO KEY | 0–1 |
| 63 | `JX_ENV_RETRIG` | RETRIG | 0–1 |
| 64 | `JX_ENV_SNAP` | ENV SNAP | 0–1 |
| 65 | `JX_DELAY_SEND` | DELAY | 0–127 |
| 66 | `JX_REVERB_SEND` | REVERB | 0–127 |
| 67 | `JX_CHORD` | CHORD | 0–7 |

Schalter (Bereich 0–3, 0–2, 0–1) sind Stufen; Werte und Namen: `jx8p_param_value_name` in `engine/jx8p_names.c`.

## Eigene Presets (ADR 0051)

`tools/eigene2c.py` liest **alle** `*.json` aus „Eigene Presets iPhone/jx8p/“ (Format der App: `ToneData` mit
`tone` = 59 Werte nach JX-8P-Nummer #11…#58 und `patch` = Werte nach `Param.rawValue`), bildet sie auf die 68
Parameter ab, lässt reine INIT-Kopien aus und schreibt `jx8p_eigene.c`. Sie stehen vor den Werks-Sounds (Preset 1–7).
Was der Kern nicht kennt (Arp, Sequencer, Distortion, Delay-/Hall-Einstellungen außer dem Anteil, eigene
Akkordformen der App), fällt weg; der Kern nutzt FM1-Electros acht Akkorde, dieselben wie die App ab Werk.

| Nr. | Name | Datei | Bereich |
|---|---|---|---|
| 1 | STRINGS 01 | preset1.json | STRINGS |
| 2 | HI STRI 01 | preset2.json | STRINGS |
| 3 | SYNTH B 01 | preset3.json | BASS |
| 4 | PG8X IN 01 | preset4.json | RHYTHM |
| 5 | PG8X IN 02 | preset5.json | RHYTHM |
| 6 | PG8X IN 03 | preset6.json | RHYTHM |
| 7 | SPRUNG ODE („Sprung oder Herzschlag“) | preset7.json | RHYTHM |

Keins ist eine reine INIT-Kopie. Preset 7 hat dieselben Werte wie Preset 5 (nur anders benannt).

## Was man nicht anfasst

`engine/` (gehört zum Gerätebuild, Bau-Chat). Änderungen am Klang nur dort, als Schalter (ADR 0006), danach
`make test` im Hauptordner und hier `make ref`.

## Nächster Schritt

Solo-Firmware „JX-8P solo“ (ADR 0046, FEATURES Y14) in `bausteine/varianten/jx8p/`, sobald das Gerüst
`bausteine/varianten/gemeinsam/` abgenommen ist: eigene Presets vorn, Stimmenzahl im Emulator einmessen, Delay/Hall
aus dem Gerüst an die Sends.
