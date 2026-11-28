# 0048: jp6 als Baustein (Einbinden statt Kopieren) und später solo

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der Jupiter-6 (`engine/jp6.c` mit der Distortion `engine/dist.c`, Port der JP-6-iOS-App) ist der zweite Synth von
FM1-Electro. Für eine Solo-Firmware und Kombis (ADR 0040, 0044) soll er wie jx8p und mks50 (ADR 0046, 0047) ein
Baustein mit `baustein.h` werden, ohne `engine/` zu ändern.

## Entscheidung

- `bausteine/jp6/` **bindet** `engine/jp6.c`, `jp6_presets.c`, `dist.c` und `chords.c` **ein** (Makefile mit
  `-DFM1_HOST_TEST`, Standardschalter des Kerns wie `jp6_test`). Keine Kopie, `engine/` unverändert.
- Adapter `jp6_bs.c` nach dem Muster von jx8p/mks50: Velocity / 127, Blöcke bis `EL_BLOCK`, Akkord-Modus über
  `engine/chords.c`, Parameter 0…1 → Rohwert (`jp6_param_max`), Presets (5 eigene zuerst, ADR 0051), Rohwerte, Tempo,
  `jp6_bs_render_sends`. Abtastrate fest 44,1 kHz.
- Stimmen voll (6), Sparschalter `JP6_BS_VOICES` (getestet mit 4).
- Host-Test: jedes Preset über den Baustein und direkt über den Kern Sample für Sample gleich, Prüfsummen je Preset
  (`ref/sums.txt`, `ref/sums4.txt`).
- Geräteunterschiede stehen im README des Bausteins: auf der eigenen Plattform rechnet die Distortion in Festkomma
  (`DIST_FIXED=1`), der Baustein auf dem Host in float.
- Solo-Firmware „JP-6 solo“ (FEATURES Y16) später in `bausteine/varianten/jp6/` auf dem Gerüst `varianten/gemeinsam/`.

## Alternativen

- **Kern nach `bausteine/jp6/` verschieben:** ändert Pfade des Gerätebuilds; später möglich, Adapter bleibt gleich.
- **Kopie:** zwei Stände. Verworfen.
- **Distortion aus dem Baustein lassen** (das Gerüst bringt Effekte mit): die Distortion gehört bei der JP-6-App zum
  Sound (je Preset eingestellt, vor dem Chorus); ohne sie klängen die Presets anders.

## Folgen

- Jede Klangänderung an `engine/jp6.c` oder `engine/dist.c` zeigt der Baustein-Test; dann `make -C bausteine/jp6 ref`.
- Last auf dem PC ≈ 13 ns je Stimme und Sample (der leichteste der drei herausgelösten Synths); teuerstes Preset
  (12 VCO 01, Unisono) ≈ 23 ns je Stimme samt Grundlast.
- Neue eigene Presets nur, wenn die JP-6-App wieder auf das iPhone kommt; bis dahin ist die Sicherung vom 05.11. die Quelle.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Baustein fertig, 79 Presets (5 eigene) bitgleich mit dem Kern, Solo-Firmware offen.
