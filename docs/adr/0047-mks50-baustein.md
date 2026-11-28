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
- **Solo-Firmware „MKS-50 solo“** in `bausteine/varianten/mks50/` auf dem Gerüst `varianten/gemeinsam/` (ADR 0044),
  Kennung FM-1_95 (löst die fm1-synths-Fassung ab), `STORE_MAGIC` „MK01“ (den Speicher der fm1-synths-Fassung, „MK50“, liest sie nicht: Neustart leer, überschrieben beim ersten Speichern), Journal „MKST“, eigene `VERSION`. Ein Gerüst-Parameter je Rohwert des Kerns, alle Sounds des
  Bausteins mit den eigenen vorn (Gruppe EIGENE), Delay-/Hall-Anteil aus dem Sound direkt in die Effekte des Gerüsts
  (Kern 1). **6 Stimmen** (alle des Kerns) auf Kern 0, im Emulator eingemessen (Ziel < 70 %). Auf dem Gerät rechnet die Ladder mit `MKS_VCF_DECIM=2` (Standard der eigenen Plattform).

## Alternativen

- **Kern nach `bausteine/mks50/` verschieben:** ändert Pfade des Gerätebuilds; später möglich, Adapter bleibt gleich.
- **Kopie:** zwei Stände. Verworfen.
- **mksultra statt mks50:** mksultra ist ein anderer Kern (TAL-Pha-Nachbau, ADR 0043). mks50 ist der Klang, den der
  Nutzer aus FM1-Electro und seiner MKS-50-App kennt, und braucht nur einen Bruchteil der Rechenzeit.

## Folgen

- Jede Klangänderung an `engine/mks.c` zeigt der Baustein-Test; dann `make -C bausteine/mks50 ref`.
- Auf der eigenen Plattform rechnet der Kern die Ladder mit `MKS_VCF_DECIM=2`; die Solo-Firmware muss das im Emulator
  gegen den Baustein (Host, 1) abhören.
- Last auf dem PC ≈ 20 ns je Stimme und Sample; teuerstes Preset (WOBBLE 01) ≈ 40 ns je Stimme samt Grundlast.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Baustein fertig, 79 Presets (23 eigene) bitgleich mit dem Kern, Solo-Firmware offen.
- 2026-11-28: Solo-Firmware MKS-50: erste Fassung gebaut, Emulator-Prüfung ok, noch kein Paket (Gleichstand mit fm1-synths zuerst).
