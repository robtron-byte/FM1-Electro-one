# 0046: jx8p als Baustein (Einbinden statt Kopieren) und JX-8P solo (fm1-synths fortgeführt)

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
- **Solo-Firmware „JX-8P solo“** (FEATURES Y14, `bausteine/varianten/jx8p/`): die JX-8P-Firmware aus fm1-synths
  (Commit 2a7b4fa, `jx8p/`: App, Pool-Kern `engine/jx.c`), fortgeführt ab 0.2.5-alpha, mit Kennung FM-1_97 und
  `STORE_MAGIC` „JX8P“ wie dort. Plattform und `shared/` aus dem Gerüst (dieselben Dateien). Keine Rückschritte gegenüber
  fm1-synths 0.2.4: 4 Parts, Stimmen-Pool (6) auf beiden Kernen, MIX, MIDI-Kanal je Part, 8 SETS, Arp, Akkord,
  Sequencer, Journal; gespeicherte Sounds und Sets bleiben beim Update gültig. Neu: die eigenen iPhone-Presets
  (`engine/jx_eigene.c`) hinter INIT TONE angehängt (Werksnummern bleiben), beim Blättern in der Gruppe EIGENE gleich
  nach INIT.
- **Warum im Gerät `jx.c` statt des Bausteins läuft:** Pool und Kernverteilung stecken in `jx.c` (`jx_plan`,
  `jx_voices(core)`), `engine/jx8p.c` hat Stimmen je Part und einen Kern. Beide stammen aus derselben Portierung; mit
  einem Part rechnen sie **bitgleich**: alle 64 Werks-Töne und die 7 eigenen Presets, Akkord C-E-G, 1,6 s (Host-Test
  `make -C bausteine jx8p`, Ziel `pool`, Teil von `test`). Zwei Kerne ändern nur die Reihenfolge der Summen (Rundung).
  So bleibt der Baustein die Referenz, und die Firmware fällt nicht hinter fm1-synths zurück.

## Alternativen

- **Kern nach `bausteine/jx8p/` verschieben** und `engine/` darauf zeigen lassen: sauberer, ändert aber Gerätebuild
  und Pfade des Bau-Chats. Später möglich, wenn der Bau-Chat zustimmt; der Adapter bleibt dann gleich.
- **Kopie in den Baustein:** zwei Stände, die auseinanderlaufen. Verworfen.
- **Die Firmware auf dem Gerüst mit einem Part** (0.1.0-alpha, 28.11.): vom Koordinator nicht abgenommen, weil sie
  hinter fm1-synths zurückfiel (Parts, SETS) und deren Speicher nicht las.
- **Mehrere Parts und Pool im Gerüst** (Wahl B): deutlich mehr Arbeit, Speicherumwandlung nötig; bleibt ein eigener
  Schritt für die Kombi-Firmwares (ADR 0049).
- **Freie Abtastrate** (Koeffizienten zur Laufzeit): Eingriff in `engine/`, nicht bitgleich. Erst wenn ein Gerät mit
  anderer Rate kommt.

## Folgen

- Jede Klangänderung an `engine/jx8p*` zeigt der Baustein-Test (Prüfsummen); dann `make -C bausteine/jx8p ref`.
- Der Baustein hängt an `engine/el_config.h`, `sdk_include/fm1_block.h`, `third_party/daisy808/dsp_math.h`; er ist damit
  nicht so frei wie sh101/mksultra, aber ohne Risiko für den Gerätebuild.
- Last auf dem PC ≈ 18 ns je Stimme und Sample (mksultra ≈ 100 ns): der JX-8P ist der leichteste Synth-Baustein.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Baustein fertig, 74 Presets bitgleich mit dem Kern, Solo-Firmware offen.
- 2026-11-28: Solo-Firmware JX-8P 0.1.0-alpha gebaut, Emulator-Prüfung ok; vom Koordinator nicht abgenommen (hinter fm1-synths
  JX8P-0.2.0: Parts, SETS, Speicher), Paket wieder aus `firmware/` entfernt. Nächste Fassung mit Gleichstand.
  Gerüst dafür abwärtskompatibel ergänzt: `.ctrl_text` in `app.ld`, `GM_SEND_FIRST`/`GM_FX_FIRST_ON`.
- 2026-11-28: Wahl A des Koordinators: JX-8P solo = fm1-synths-Fassung fortgeführt (0.2.5-alpha, FM-1_97, „JX8P“), eigene
  Presets angehängt; Bitgleichheit Pool-Kern ↔ Baustein als Prüfung in `make -C bausteine jx8p`.
