# 0048: jp6 als Baustein (Einbinden statt Kopieren) und JP-6 solo

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
- **Solo-Firmware „JP-6 solo“** in `bausteine/varianten/jp6/` auf dem Gerüst `varianten/gemeinsam/` (ADR 0044),
  Kennung FM-1_92 (neu), `STORE_MAGIC` „JP01“, Journal „JPST“, eigene `VERSION`. Ein Gerüst-Parameter je Rohwert des Kerns, alle Sounds des
  Bausteins mit den eigenen vorn (Gruppe EIGENE), Delay-/Hall-Anteil aus dem Sound direkt in die Effekte des Gerüsts
  (Kern 1). **6 Stimmen** (alle des Kerns) auf Kern 0, im Emulator eingemessen (Ziel < 70 %). Ein Part, Kern 1 rechnet Delay und Hall. **Ohne Distortion** (Wunsch des Nutzers 28.11.2026): Schalter
  `JP6_BS_DIST` im Baustein (Standard 1, Test und Prüfsummen mit Distortion); die Firmware baut mit 0 und bindet
  `bausteine/jp6/dist_aus.c` statt `engine/dist.c` (keine Tabellen, kein Speicher-Engpass, keine Rechenzeit). Pegel wie in
  FM1-Electro (× 1,4, `GM_OUT_GAIN`).

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

**Last:** Mit 6 Stimmen Unisono (12 VCO 01) brauchte Kern 0 im Emulator bis 66 %, Kern 1 nur 7 % (Delay und Hall).
Nach der Regel des Nutzers (28.11.: 4 Stimmen, Delay ≤ 0,6 s) sind es 41 % auf Kern 0. Stimmen eines Parts auf Kern 1 zu legen, geht
mit `engine/jp6.c` nur mit einem Umbau (Steuerwerte je Schritt vorab festlegen, wie im verworfenen Pool-Kern) und ist
deshalb nicht gemacht. Am Gerät die Seite SYSTEM prüfen.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Baustein fertig, 79 Presets (5 eigene) bitgleich mit dem Kern, Solo-Firmware offen.
- 2026-11-28: Solo-Firmware JP-6: erste Fassung auf dem Gerüst. Ein Versuch mit vier Parts und eigenem Pool-Kern (nach
  fm1-synths `jx.c`) wurde auf Hinweis des Nutzers verworfen: für JP-6 gibt es keine ältere Solo-Firmware, gegen die etwas
  fehlen könnte. Distortion auf Wunsch des Nutzers aus (`JP6_BS_DIST=0`). 0.1.0-alpha, Emulator-Prüfung ok, am Gerät offen.
- 2026-11-28: Regel des Nutzers: 4 Stimmen, Delay höchstens 0,6 s. `firmware/JP6-0.1.0-alpha.fwsc`, Emulator ≤ 41 %.
- 2026-11-28 (sh101-Chat): 0.1.2-alpha: Delay und Hall waren stumm (Wettlauf um `fx_in` im Gerüst, ADR 0044, behoben über
  `fx_src`); im Emulator nachgestellt: 0.1.1 nach der Note Pegel 0, 0.1.2 klingt nach.
