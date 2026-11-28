# 0046: jx8p als Baustein (Einbinden statt Kopieren) und JX-8P solo

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der JX-8P steckt in `engine/jx8p.c` (ADR 0039) und im Gerätebuild von FM1-Electro. Für die Solo- und Kombi-Firmwares
(ADR 0040, 0044, 0049) soll er wie sh101 und mksultra ein Baustein mit `baustein.h` werden. Der Klang darf sich
dabei nicht ändern, `make test` im Hauptordner muss grün bleiben, und `engine/` gehört dem Bau-Chat.

## Entscheidung

- `bausteine/jx8p/` **bindet** `engine/jx8p.c`, `jx8p_names.c`, `jx8p_presets.c` und `chords.c` **ein** (Makefile,
  dieselben Schalter wie `make test-jx`: `-DFM1_HOST_TEST -DEL_SYNTH_JX=1`). Keine Kopie, `engine/` unverändert.
- Adapter `jx8p_bs.c` auf `baustein.h` (`jx8p_baustein`): Velocity / 127, Blöcke bis `EL_BLOCK`, Akkord-Modus über
  `engine/chords.c` wie `synth_track.c`, Parameter 0…1 → Rohwert (`jx8p_param_max`). Zusätzlich Presets (eigene
  zuerst, ADR 0051), Rohwerte, Tempo für LFO-Sync und `jx8p_bs_render_sends` (Delay-/Hall-Bus für die Firmware).
- Abtastrate bleibt fest 44,1 kHz (`EL_SAMPLE_RATE`); `init` nimmt jede Rate an, `jx8p_bs_rate()` sagt, womit er rechnet.
- Stimmen voll wie der JX-8P (6), Sparschalter `JX8P_BS_VOICES` (getestet mit 4).
- Host-Test: jedes Preset über den Baustein und direkt über den Kern, Sample für Sample gleich, dazu Prüfsummen je
  Preset (`ref/sums.txt`, `ref/sums4.txt`).
- **Solo-Firmware „JX-8P solo“** (FEATURES Y14) in `bausteine/varianten/jx8p/` auf dem Gerüst `varianten/gemeinsam/`
  (ADR 0044), Kennung FM-1_97 (löst die fm1-synths-Fassung ab), `STORE_MAGIC` „JX01“ (die Daten der fm1-synths-Fassung,
  „JX8P“, bleiben unberührt), Journal „JXST“, eigene `VERSION`. Ein Gerüst-Parameter je Rohwert des Kerns (68),
  Seiten nach der fm1-synths-Fassung. Alle 74 Sounds des Bausteins, eigene vorn (Gruppe EIGENE). Delay-/Hall-Anteil
  bleibt im Sound und geht direkt in die Effekte des Gerüsts (Kern 1). **6 Stimmen** (alle des Kerns) auf Kern 0:
  im Emulator höchstens ≈ 48 % (Ziel < 70 %).

## Alternativen

- **Kern nach `bausteine/jx8p/` verschieben** und `engine/` darauf zeigen lassen: sauberer, ändert aber Gerätebuild
  und Pfade des Bau-Chats. Später möglich, wenn der Bau-Chat zustimmt; der Adapter bleibt dann gleich.
- **Kopie in den Baustein:** zwei Stände, die auseinanderlaufen. Verworfen.
- **Die fm1-synths-Firmware weiterführen** (Stimmen-Pool über vier Parts auf beiden Kernen): eigener App-Rahmen neben dem
  Gerüst; das Gerüst bringt Sequencer, Speichern und MIDI für alle Solo-Firmwares einheitlich.
- **Freie Abtastrate** (Koeffizienten zur Laufzeit): Eingriff in `engine/`, nicht bitgleich. Erst wenn ein Gerät mit
  anderer Rate kommt.

## Folgen

- Jede Klangänderung an `engine/jx8p*` zeigt der Baustein-Test (Prüfsummen); dann `make -C bausteine/jx8p ref`.
- Der Baustein hängt an `engine/el_config.h`, `sdk_include/fm1_block.h`, `third_party/daisy808/dsp_math.h`; er ist damit
  nicht so frei wie sh101/mksultra, aber ohne Risiko für den Gerätebuild.
- Last auf dem PC ≈ 18 ns je Stimme und Sample (mksultra ≈ 100 ns): der JX-8P ist der leichteste Synth-Baustein.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Baustein fertig, 74 Presets bitgleich mit dem Kern, Solo-Firmware offen.
- 2026-11-28: Solo-Firmware JX-8P 0.1.0-alpha (`firmware/JX8P-0.1.0-alpha.fwsc`), Emulator-Prüfung ok, am Gerät offen.
  Gerüst dafür abwärtskompatibel ergänzt: `.ctrl_text` in `app.ld`, `GM_SEND_FIRST`/`GM_FX_FIRST_ON`.
