# 0039: JX-8P statt MKS-50 (Schalter `EL_SYNTH_JX`)

- **Status:** angenommen (Wahl des Nutzers 21.11.: „direkt einbauen ohne Testbuild“)
- **Stand:** 2026-11-21

## Kontext

Wunsch des Nutzers vom 21.11. (FEATURES Y9): Die MKS-50-Engine soll durch die JX-8P-Engine aus seiner Firmware
**fm1-synths** ersetzt werden (github.com/robtron-byte/fm1-synths, `jx8p/engine/jx.c`). Die JX-8P-Engine ist eine
Portierung seiner JX-8P-iOS-App. Ihre Bausteine (Hüllkurve, Ladder, Chorus, DCO-Form) stammen aus FM1-Electros
eigenem `engine/mks.c`.

Die JX-8P-Engine hat zwei DCOs (Rauschen, Rechteck, Puls, Sägezahn; SYNC1, SYNC2, X-MOD), einen Mixer mit
Hüllkurve auf DCO-2, HPF in vier Stufen, eine 24-dB-Ladder, VCA (GATE oder ENV-2), zwei ADSR, ein LFO mit
Verzögerung und Tempo-Sync, BBD-Chorus, die Key Modes poly/chord/mono/unison und Portamento.
Auf dem Gerät gemessen in fm1-synths (`jx8p/docs/adr/0002`): etwa 10–11 % eines 64er-Blocks pro Stimme, also
eher günstiger als der MKS-50 (6 Stimmen: 54 % gegenüber 69–71 %).

Entscheidungen des Nutzers:
- Alte MKS-50-Speicherstände sind egal, weil nichts gespeichert wurde.
- Die 20 Zusatz-Sounds auf MKS-Basis (PLAIN SAW … INIT RHY+/STR+/BASS+) fallen weg.
- JP-6 bleibt.

## Entscheidung

- **Schalter `EL_SYNTH_JX`** (`engine/el_config.h`; zuerst Standard 0 nach ADR 0006, seit 0.5.6-alpha Standard 1).
  - Bei 1 ist Engine 0 (`SYN_MKS`, Feld `u.mks`) der JX-8P statt des MKS-50.
  - Bei 0 bleibt alles bitgleich: alle 70 Demo-WAVs sind unverändert.
  - Ohne Schalter werden `mks.c`, `mks_presets.c` und die MKS-Zusatzsounds übersetzt, mit Schalter `jx8p*.c`.
- **Engine** `engine/jx8p.c` / `jx8p.h`:
  - Stimme, Kurven und Werttexte unverändert aus fm1-synths, Commit 2a7b4fa. Nur die Präfixe sind umbenannt: `JP_` → `JX_`, damit sie nicht mit `jp6.h` kollidieren; `syn_` → `jxd_`.
  - Gebaut wie `mks.c`: ein Part je Bereich mit eigenen Stimmen (3/3/1), `jx8p_render` wie `mks_render`. Es gibt keinen gemeinsamen Pool.
  - Die Parameter sind lückenlos nummeriert (68, statt der echten JX-Nummern 11–82). Damit passt ein Sound in `SYN_VALUES` = 69, und das Speicherformat bleibt gleich.
  - Die DSP-Typen stehen in `jx8p_dsp.h`, der Code nur in `jx8p.c` über `jx8p_dsp_inline.h`. Die Anzeige-Namen liegen in `jx8p_names.c`, also im Flash statt im Audio-SRAM. `platform/firmware/app.ld` legt `e_jx8p.o` in den SRAM wie `e_mks.o`.
- **Akkorde und Bereiche** (`MKS_CHORDS`, `mks_chords`, `MKS_AREA_*`): nach `engine/chords.c/.h` verschoben, unverändert. Die JX-App hat dieselben acht Akkorde.
- **Sounds** (`tools/jx8p_presets.py` → `engine/jx8p_presets.c`):
  - Die 64 Werkstöne des JX-8P, nur Parameterwerte.
  - Bereich nach Kategorie: BASS → BASS (4); STR+BRASS und PAD → STRINGS (17); der Rest → RHYTHM (43).
  - PG8X INIT wird zu INIT RHY, INIT STR und INIT BASS (mono), am Ende der Liste.
  - Die JP-6-Zusatzsounds bleiben, die MKS-Zusatzsounds entfallen und mit ihnen INIT+ (N29).
- **Startsounds:** SYNC PLUK, HI STRINGS, SYNTH BASS. HI STRINGS ist im Pegel nah an JX STRINGS; STRINGS 1 wäre 11 dB leiser.
- **Bedienung:**
  - Seiten wie beim JP-6: FILTER (CUTOFF RESO VCF-ENV VCF-LFO), ENVELOPE (ENV-2 A D S R), OSC (DCO-1 WAVE, DCO-2 WAVE, X-MOD, DCO-2 MIX), MOD (LFO RATE, DCO-1 LFO, GLIDE, ENV-1 DECAY), FX (CHORUS DELAY REVERB VOLUME).
  - Der Editor zeigt das Panel des JX-8P bzw. PG-800 in zehn Abschnitten: DC1 DC2 MIX VCF VCA EN1 EN2 LFO KEY FX. Bender-, Aftertouch- und Mod-Tiefen fehlen, weil das FM-1 sie nicht hat.
- **Speicher:**
  - JX-Builds schreiben Version 29 und Bank-Version 3. Eine 28 oder älter wird geladen, die MKS-Sounds darin werden zum Startsound des Bereichs.
  - Eigene MKS-Sounds einer Bank 1 oder 2 werden als leer behandelt.
  - Ein MKS-Build liest eine 29 bzw. Bank 3 nicht, damit keine fremden Werte als MKS-Klang laufen.
- **Prüfen:**
  - `make test` (Schalter aus) und `make test-jx` (mit `host/jx8p_test.c`: jeder Sound klingt und endet, poly/mono/unison, Wertebereiche, Akkordspeicher).
  - `make jx-ab` rendert dieselben Grooves mit beiden Engines nach `build/jx_ab/mks` und `build/jx_ab/jx`.
  - Gerätebuild mit `EL_EXTRA_CFLAGS="-DEL_SYNTH_JX=1"`.

## Alternativen

- Den JX-8P als dritte Engine neben dem MKS-50 behalten: Dafür fehlt Audio-SRAM (≈3 KB frei nach 0.5.4-alpha).
- Den gemeinsamen Stimmen-Pool von fm1-synths mit 4 Parts übernehmen: Das hätte den Synth-Bereich und die
  Kernverteilung von FM1-Electro umgebaut. So bleibt alles wie beim MKS-50.
- Die echten JX-Parameternummern behalten (83 Werte): `SYN_VALUES` und damit das ganze Speicherformat und die Bank hätten sich geändert.
- Alte MKS-Sounds näherungsweise umrechnen: Laut Nutzer nicht nötig.

## Folgen

- Zur Wahl gehört ein Gerätebuild mit Schalter. Der Bau-Chat prüft dabei Last und SRAM im Emulator (`tools/fm1-emu/check.sh`).
  Der JX-8P hat keine VCF-Dezimierung wie der MKS-50 (`MKS_VCF_DECIM`); wird es zu eng, ist das der erste Hebel.
- Die Lautstärke der Werkstöne streut stark (−13 bis −44 dB RMS), wie beim Original. Die Gesamtmischung der Demos
  ist im Median 0,1 dB lauter als mit dem MKS-50.
- Der MKS-50 bleibt dauerhaft als Schalterstellung `EL_SYNTH_JX=0` erhalten (Wunsch 22.11.): Der Nutzer will
  gelegentlich zurückwechseln und ihn in anderen Firmwares nutzen. `mks.c`, `mks_presets.c`, die MKS-Zusatzsounds
  und ihre Tests bleiben deshalb, und `make test` prüft weiter beide Engines. Zum Wechseln den Gerätebuild mit
  `EL_EXTRA_CFLAGS="-DEL_SYNTH_JX=0"` bauen. Ein MKS-Build liest einen Speicherstand der Version 29 nicht und
  startet leer.

## Verlauf

- 2026-11-21: angelegt (Y9), Schalter Standard aus, Plan vom Nutzer freigegeben
- 2026-11-21: Der Nutzer wählt den JX-8P ohne eigenen Testbuild. Standard `EL_SYNTH_JX` = 1 ab 0.5.6-alpha. Der MKS-50 bleibt vorerst als Schalterstellung 0 baubar; `make test` prüft beide: die bisherigen Host-Tests mit `-DEL_SYNTH_JX=0` (Referenz-WAVs unverändert) und `test-jx` (Makefile). Das Entfernen der MKS-Dateien steht noch aus. Emulator (`FM1_STRICT=1`, derselbe neu erzeugte Groove, Lastanzeige): MKS-50 PEAK 71 %, Kern 1 63 %, STRINGS 39; JX-8P PEAK 66 %, Kern 1 61 %, STRINGS 35. Audio-SRAM: knapp 6 KB frei (`_pool_end` 0x01C72888). Ein mit 0.5.5-alpha gespeicherter Groove lädt; die MKS-Sounds darin werden zum Startsound des Bereichs
- 2026-11-22: Die MKS-50-Dateien werden nicht entfernt. Der MKS-50 bleibt als Schalterstellung 0 zum Zurückwechseln und für andere Firmwares (Wunsch des Nutzers)
