# Lizenzen und Herkunft: `jx8p/`

Die JX-8P-Firmware steht unter der GNU General Public License, Version 3 (`GPL-3.0-only`, Text in `../LICENSE`).
Gemeinsame Teile und ihre Herkunft: `../shared/LICENSES.md`.

| Was | Dateien | Herkunft | Lizenz |
|---|---|---|---|
| JX-8P-Stimme | `engine/jx.c`, `engine/jx.h` | Port der JX-8P-iOS-App des Nutzers (github.com/robtron-byte/JX8P, Branch `cursor/ios-jx8p-grundlage-76ee`, Commit `9687574`: `ios/Shared/DSP/DCO.swift`, `HardwareCurves.swift`, `SynthEngine.swift`) | GPL-3.0-only |
| Pool, Parts, Kerne | `engine/jx.c` | aus `../mks50/engine/mks.c` (14.11.2026), dort aus FM1-Electro (github.com/robtron-byte/FM1-Electro-one, Commit `e7b3b14`) | GPL-3.0-only |
| Werks-Sounds | `engine/jx_presets.c` (`tools/make_presets.py`) | aus der iOS-App (`ios/Shared/Model/FactoryPresets.swift`): die 64 Werks-Tones des JX-8P (ROM V2.1, nur Parameterwerte, Repo-ADR 0002) und PG8X INIT | Parameterwerte; Werks-Sounds von Roland |
| App | `app/jx_app.c`, `app/jx_ui.c` | aus `../mks50/app/mk_app.c`, `mk_ui.c` (dieses Repository, 14.11.2026), dort mit Code aus SLOOP, X0X und FM1-Electro (siehe `../mks50/LICENSES.md`) | GPL-3.0-only |
| INIT TONE | `app/jx_app.c` (`init_v`) | Grundwerte der JX-8P-iOS-App (`ios/Shared/Model/Param.swift` `defaultValue`, `ToneData.initTone`, Commit `9687574`) | MIT (Repo JX8P), nur Parameterwerte |
| Farben | `app/jx_theme.h` | Farbwerte der JX-8P-iOS-App des Nutzers (github.com/robtron-byte/JX8P, Branch `cursor/ios-jx8p-grundlage-76ee`, Commit `9687574`, `ios/Shared/UI/Controls.swift`) | MIT (Repo JX8P), nur Farbwerte |
| Build | `tools/build.py` | aus `../mks50/tools/build.py`, dort nach FM1-Electros `tools/build_platform.py` | GPL-3.0-only |
| Prüfungen | `host/` | `scenario.h`, `cmp.py`, `pool_test.c` nach `../mks50/host/` | GPL-3.0-only |

„JX-8P“ beschreibt nur die nachgebildete Architektur. Das Projekt ist nicht mit der Roland Corporation verbunden.
