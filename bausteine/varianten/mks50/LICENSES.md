# Lizenzen und Herkunft

Gemeinsame Teile (Effekte, DSP-Bausteine, Schrift, Mathe-Hilfen) und ihre Herkunft: `../shared/LICENSES.md`.

Die MKS-50-Firmware steht unter der GNU General Public License, Version 3 (`GPL-3.0-only`, Text in `../LICENSE`).

| Was | Dateien | Herkunft | Lizenz |
|---|---|---|---|
| MKS-50-Engine | `engine/mks.c`, `engine/mks.h`, `engine/mks_presets.c`, `engine/mk_config.h` | FM1-Electro (github.com/robtron-byte/FM1-Electro-one, Commit `e7b3b14`: `engine/mks.c`, `mks.h`, `mks_presets.c`, `el_config.h`), dort ein Port der DSP der MKS-50-iOS-App (github.com/robtron-byte/Mks50-iOS-); hier mit Stimmen-Pool und Aufteilung auf zwei Kerne; Hüllkurve, Ladder, Chorus und Mathe seit 14.11.2026 in `../shared/dsp/` | dort GPL-3.0-only mit FM1 Opens Linking-Erlaubnis, hier GPL-3.0-only |
| INIT TONE | `app/mk_app.c` (`init_v`) | Werte des „INIT TONE“ der MKS-50-iOS-App (`Shared/Model/FactoryPresets.swift`, `Param.defaultValue`, Commit `080db22`) | GPL-3.0-only |
| Farben | `app/mk_theme.h` | aus der MKS-50-iOS-App des Nutzers (`Shared/UI/Controls.swift`, Commit `080db22`) | GPL-3.0-only |
| Referenz für die Prüfung | `host/ref/` | FM1-Electro `e7b3b14`, unverändert: `engine/mks.c`, `mks.h`, `mks_presets.c`, `el_config.h`, `third_party/daisy808/dsp_math.h`, `engine.h`, `sdk_include/fm1_block.h` | wie in den Dateiköpfen (GPL-3.0-only mit Linking-Erlaubnis; `dsp_math.h`, `engine.h` MIT, Text in `../shared/LICENSES.md`) |
| App, Build | `app/mk_app.c`, `tools/build.py` | nach FM1-Electro `fm1/el_app.c` und `tools/build_platform.py`; der Start des zweiten Kerns nach SLOOP `src/cpu1.c` (Andre Rezemini) | GPL-3.0-only |
| Plattform | `../platform/` | Dinghy (github.com/zvenson/dinghy, Commit `f77bf03`) von Leo Kuroshita (Hügelton Instruments, Felucca), mit SLOOP (isod89), X0X (Charles Vestal), OberVave (zvenson) und den Änderungen von FM1-Electro; Einzelheiten in `../platform/LICENSING.md` und `../platform/ORIGIN.md` | GPL-3.0-only |
| JieLi-AC79-SDK | nicht im Repository; `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` kommen beim Bau ins Paket | gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK | Apache-2.0 |

„MKS-50“ und „Alpha Juno“ beschreiben nur die nachgebildete Architektur. Das Projekt ist nicht mit der Roland Corporation verbunden.
