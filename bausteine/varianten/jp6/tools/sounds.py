#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt app/jp_sounds.h: die Soundliste von JP-6 solo (ADR 0048, 0051).

Reihenfolge wie im Baustein bausteine/jp6 (jp6_bs_preset_*): zuerst die eigenen Presets des Nutzers
(jp6_eigene.c, Gruppe EIGENE), dann die Werks-Sounds aus engine/jp6_presets.c mit ihrer Kategorie der JP-6-App
(Kommentar am Zeilenende, tools/convert_jp6_presets.py). Leere Gruppen fallen weg. Ergebnis eingecheckt."""
import re
from pathlib import Path

R = Path(__file__).resolve().parents[1]
B = R.parents[1]
REPO = B.parent
ORDER = ["bass", "lead", "sequence", "zap", "stab", "keys", "bells", "stringsBrass", "pad", "fxPerc", "riser"]
LABEL = {"bass": "BASS", "lead": "LEAD", "sequence": "SEQUENCE", "zap": "ZAPS", "stab": "STAB", "keys": "KEYS",
         "bells": "BELLS", "stringsBrass": "STR+BRASS", "pad": "PAD", "fxPerc": "FX+PERC", "riser": "RISERS"}

eigene = re.findall(r'^    \{"([^"]*)", \d', (B / "jp6" / "jp6_eigene.c").read_text(), re.M)
werk = re.findall(r'^    \{"([^"]*)", \d, \{[^}]*\}\}, /\* (\w+) \*/', (REPO / "engine" / "jp6_presets.c").read_text(), re.M)
groups = ["EIGENE"] + [LABEL[c] for c in ORDER]
rows = [(n, 0) for n in eigene] + [(n, 1 + ORDER.index(c)) for n, c in werk]
used = sorted({c for _, c in rows})
groups = [groups[c] for c in used]
rows = [(n, used.index(c)) for n, c in rows]
out = ["/* SPDX-License-Identifier: GPL-3.0-only",
       " * Erzeugt von bausteine/varianten/jp6/tools/sounds.py: Gruppen der Soundliste (Reihenfolge wie",
       " * jp6_bs_preset_*: eigene zuerst, dann die Werks-Sounds). Nicht von Hand ändern. */",
       f"#define JP_NCAT {len(groups)}u",
       f"#define JP_NSOUND {len(rows)}u",
       "static const char *const jp_cat_names[JP_NCAT] = {" + ", ".join(f'"{g}"' for g in groups) + "};",
       "static const uint8_t jp_sound_cat[JP_NSOUND] = {"]
for i in range(0, len(rows), 16):
    out.append("    " + " ".join(f"{c}," for _, c in rows[i:i + 16]))
out.append("};")
out.append("/* " + "; ".join(f"{i}: {n}" for i, (n, _) in enumerate(rows)) + " */")
(R / "app" / "jp_sounds.h").write_text("\n".join(out) + "\n")
print(len(rows), "Sounds,", len(groups), "Gruppen:", ", ".join(groups))
