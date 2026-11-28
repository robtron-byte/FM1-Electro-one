#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt app/jp_cats.h: die Gruppe jedes Werks-Sounds von engine/jp6_presets.c (Kommentar am Zeilenende, Kategorie
der JP-6-App, tools/convert_jp6_presets.py im Hauptordner). Gruppe 0 ist INIT (INIT TONE)."""
import re
from pathlib import Path
R = Path(__file__).resolve().parents[1]
REPO = R.parents[2]
NAMES = ["INIT", "BASS", "LEAD", "SEQUENCE", "ZAPS", "STAB", "KEYS", "STR+BRASS", "PAD", "FX+PERC"]
KEY = {"bass": 1, "lead": 2, "sequence": 3, "zap": 4, "stab": 5, "keys": 6, "stringsBrass": 7, "pad": 8, "fxPerc": 9}
rows = re.findall(r'^    \{"([^"]*)", \d, \{[^}]*\}\}, /\* (\w+) \*/', (REPO / "engine" / "jp6_presets.c").read_text(), re.M)
out = ["/* SPDX-License-Identifier: GPL-3.0-only",
       " * Erzeugt von tools/cats.py aus engine/jp6_presets.c (Hauptordner): Gruppe je Werks-Sound. Nicht von Hand ändern. */",
       f"enum {{ NFC = {len(NAMES)} }};   /* Gruppen der Werks-Sounds, 0 = INIT */",
       "static const char *const jp_cat_names[NFC] = {" + ", ".join(f'"{n}"' for n in NAMES) + "};",
       f"static const uint8_t jp_preset_cat[{len(rows)}] = {{"]
for n, c in rows:
    out.append(f"    {KEY[c]},   /* {n} */")
out.append("};")
(R / "app" / "jp_cats.h").write_text("\n".join(out) + "\n")
print(len(rows), "Werks-Sounds")
