# Herkunft: Felucca-Drumstimmen

Übernommen für die Festkomma-Drums der eigenen Plattform (`engine/dr_fixed.c`, `docs/adr/0022`).

| Datei | Herkunft | Lizenz |
|---|---|---|
| `drum_voice.c` | [hugelton/Felucca](https://github.com/hugelton/Felucca) `firmware/src/drum_voice.c`, Commit `a4f007c` (Stand 2026-11-06), Leo Kuroshita, Hügelton Instruments | GPL-3.0-only |
| `fe_dsp.c` | [hugelton/Felucca-Engines](https://github.com/hugelton/Felucca-Engines) `src/dsp.c`, Commit `d026007` | MIT (`LICENSE-MIT`) |
| `gen_engine_tables.py` | Felucca-Engines `tools/gen_engine_tables.py`, Commit `d026007` | MIT (`LICENSE-MIT`) |
| `fe_tables.h` | erzeugt mit `python3 gen_engine_tables.py fe_tables.h` (44,1 kHz) | MIT |

Unverändert übernommen. Nur der Build der eigenen Plattform verwendet sie (`TR808_FIXED`); der Host- und der SDK-Build
bleiben bei der Gleitkomma-808.
