# 0047: mks50 als Baustein (Einbinden statt Kopieren) und MKS-50 solo

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der MKS-50 (`engine/mks.c`, Port der MKS-50-iOS-App) war bis 0.5.5-alpha der Synth von FM1-Electro und baut mit
`EL_SYNTH_JX=0` weiter (ADR 0039); die Host-Tests im Hauptordner prüfen ihn. Für eine Solo-Firmware und Kombis
(ADR 0040, 0044) soll er wie jx8p (ADR 0046) ein Baustein mit `baustein.h` werden, ohne `engine/` zu ändern.

## Entscheidung

- `bausteine/mks50/` **bindet** `engine/mks.c`, `mks_presets.c` und `chords.c` **ein** (Makefile mit
  `-DFM1_HOST_TEST -DEL_SYNTH_JX=0`, Standardschalter des Kerns). Keine Kopie, `engine/` unverändert.
- Adapter `mks50_bs.c` nach dem Muster von jx8p: Velocity / 127, Blöcke bis `EL_BLOCK`, Akkord-Modus über
  `engine/chords.c`, Parameter 0…1 → Rohwert (`mks_param_max`), Presets (23 eigene zuerst, ADR 0051), Rohwerte, Tempo,
  `mks50_bs_render_sends`. Abtastrate fest 44,1 kHz.
- Stimmen voll wie der MKS-50 (6), Sparschalter `MKS50_BS_VOICES` (getestet mit 4).
- Host-Test: jedes Preset über den Baustein und direkt über den Kern Sample für Sample gleich, Prüfsummen je Preset
  (`ref/sums.txt`, `ref/sums4.txt`).
- **Solo-Firmware „MKS-50 solo“** (FEATURES Y15, `bausteine/varianten/mks50/`): die MKS-50-Firmware aus fm1-synths
  (Commit 2a7b4fa, `mks50/`: App, Pool-Kern `engine/mks.c`), fortgeführt ab 0.2.5-alpha, Kennung FM-1_95 und
  `STORE_MAGIC` „MK50“ wie dort (Muster der Wahl A für JX-8P, ADR 0046). Keine Rückschritte: 4 Parts, Pool von 6 Stimmen
  auf beiden Kernen, MIX, SETS, Sequencer, Journal; gespeicherte Daten bleiben gültig. Neu: die 23 eigenen Presets
  hinter INIT TONE, beim Blättern in EIGENE gleich nach INIT.
- **Klang:** Der Pool-Kern ist mit einem Part bitgleich mit `engine/mks.c`, wenn FM1-Electros `MKS_RELEASE_HURRY` an ist
  (alle 56 Werks-Sounds, 23 eigene; `make -C bausteine mks50`, Ziel `pool`). Die fm1-synths-Fassung hat ihn aus; damit
  sich für die Nutzer nichts ändert, bleibt er aus und ist als Schalter `MK_RELEASE_HURRY` zum Anhören da (ADR 0006).

## Alternativen

- **Kern nach `bausteine/mks50/` verschieben:** ändert Pfade des Gerätebuilds; später möglich, Adapter bleibt gleich.
- **Kopie:** zwei Stände. Verworfen.
- **mksultra statt mks50:** mksultra ist ein anderer Kern (TAL-Pha-Nachbau, ADR 0043). mks50 ist der Klang, den der
  Nutzer aus FM1-Electro und seiner MKS-50-App kennt, und braucht nur einen Bruchteil der Rechenzeit.

## Folgen

- Jede Klangänderung an `engine/mks.c` zeigt der Baustein-Test; dann `make -C bausteine/mks50 ref`.
- FM1-Electros eigene Plattform rechnet `engine/mks.c` mit `MKS_VCF_DECIM=2`; die Solo-Firmware nutzt den Pool-Kern ohne
  diese Dezimierung (volle Ladder wie der Baustein-Test).
- Last auf dem PC ≈ 20 ns je Stimme und Sample; teuerstes Preset (WOBBLE 01) ≈ 40 ns je Stimme samt Grundlast.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Baustein fertig, 79 Presets (23 eigene) bitgleich mit dem Kern, Solo-Firmware offen.
- 2026-11-28: Solo-Firmware MKS-50: erste Fassung gebaut, Emulator-Prüfung ok, noch kein Paket (Gleichstand mit fm1-synths zuerst).
- 2026-11-28: MKS-50 solo = fm1-synths-Fassung fortgeführt (0.2.5-alpha, FM-1_95, „MK50“), eigene Presets angehängt;
  Bitgleichheit Pool-Kern ↔ Baustein (mit RELEASE_HURRY) als Prüfung in `make -C bausteine mks50`.
- 2026-11-28: MKS-50 solo 0.2.6-alpha nach der Regel des Nutzers: Pool 4 Stimmen, Delay höchstens 0,6 s (Schalter
  `SYN_FX_DLY_MAX_S` in `gemeinsam/shared/fx/syn_fx.c`); Speicherformat unverändert.
