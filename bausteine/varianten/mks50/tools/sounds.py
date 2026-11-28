#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt app/mk_sounds.h: die Soundliste von MKS-50 solo (ADR 0047, 0051).

Reihenfolge wie im Baustein bausteine/mks50 (mks50_bs_preset_*): zuerst die eigenen Presets des Nutzers
(mks50_eigene.c, Gruppe EIGENE), dann die Werks-Sounds aus engine/mks_presets.c mit ihrer Kategorie der MKS-50-App
(Kommentar am Zeilenende, tools/convert_presets.py). Leere Gruppen fallen weg. Ergebnis eingecheckt."""
import re
from pathlib import Path

R = Path(__file__).resolve().parents[1]
B = R.parents[1]
REPO = B.parent
ORDER = ["bass", "lead", "sequence", "zap", "stab", "keys", "bells", "stringsBrass", "pad", "fxPerc", "riser"]
LABEL = {"bass": "BASS", "lead": "LEAD", "sequence": "SEQUENCE", "zap": "ZAPS", "stab": "STAB", "keys": "KEYS",
         "bells": "BELLS", "stringsBrass": "STR+BRASS", "pad": "PAD", "fxPerc": "FX+PERC", "riser": "RISERS"}

eigene = re.findall(r'^    \{"([^"]*)", \d', (B / "mks50" / "mks50_eigene.c").read_text(), re.M)
werk = re.findall(r'^    \{"([^"]*)", \d, \{[^}]*\}\}, /\* (\w+) \*/', (REPO / "engine" / "mks_presets.c").read_text(), re.M)
groups = ["EIGENE"] + [LABEL[c] for c in ORDER]
rows = [(n, 0) for n in eigene] + [(n, 1 + ORDER.index(c)) for n, c in werk]
used = sorted({c for _, c in rows})
groups = [groups[c] for c in used]
rows = [(n, used.index(c)) for n, c in rows]
out = ["/* SPDX-License-Identifier: GPL-3.0-only",
       " * Erzeugt von bausteine/varianten/mks50/tools/sounds.py: Gruppen der Soundliste (Reihenfolge wie",
       " * mks50_bs_preset_*: eigene zuerst, dann die Werks-Sounds). Nicht von Hand ändern. */",
       f"#define MK_NCAT {len(groups)}u",
       f"#define MK_NSOUND {len(rows)}u",
       "static const char *const mk_cat_names[MK_NCAT] = {" + ", ".join(f'"{g}"' for g in groups) + "};",
       "static const uint8_t mk_sound_cat[MK_NSOUND] = {"]
for i in range(0, len(rows), 16):
    out.append("    " + " ".join(f"{c}," for _, c in rows[i:i + 16]))
out.append("};")
out.append("/* " + "; ".join(f"{i}: {n}" for i, (n, _) in enumerate(rows)) + " */")
(R / "app" / "mk_sounds.h").write_text("\n".join(out) + "\n")
print(len(rows), "Sounds,", len(groups), "Gruppen:", ", ".join(groups))
