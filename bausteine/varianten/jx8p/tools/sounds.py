#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt app/jx_sounds.h: die Soundliste von JX-8P solo (ADR 0046, 0051).

Reihenfolge wie im Baustein bausteine/jx8p (jx8p_bs_preset_*): zuerst die eigenen Presets des Nutzers
(jx8p_eigene.c, Gruppe EIGENE), dann die Werks-Sounds aus engine/jx8p_presets.c. Die Gruppen der Werks-Sounds kommen
aus fm1-synths (jx8p/engine/jx_presets.c, Commit 2a7b4fa, Kategorien der JX-8P-App), über den Namen zugeordnet; die
drei INIT je Bereich bilden die Gruppe INIT AREA. Ergebnis eingecheckt, der Build braucht fm1-synths nicht.

Aufruf: sounds.py [Pfad zu fm1-synths]   (Standard: ~/fm1-work/fm1-synths)"""
import re
import sys
from pathlib import Path

R = Path(__file__).resolve().parents[1]
B = R.parents[1]
REPO = B.parent
FS = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home() / "fm1-work" / "fm1-synths"

eigene = re.findall(r'\{"([^"]*)", \d', (B / "jx8p" / "jx8p_eigene.c").read_text())
werk = re.findall(r'\{"([^"]*)", \d', (REPO / "engine" / "jx8p_presets.c").read_text())
src = (FS / "jx8p" / "engine" / "jx_presets.c").read_text()
cat_names = re.search(r'jx_cat_names\[\] = \{([^}]*)\}', src).group(1)
cat_names = re.findall(r'"([^"]*)"', cat_names)
fs_cat = {n: int(c) for n, c in re.findall(r'\{"([^"]*)", (\d+), \{', src)}
groups = ["EIGENE"] + cat_names[1:] + ["INIT AREA"]          # (cat 0 der App = INIT: das Gerüst hat sein eigenes INIT)
rows = [(n, 0) for n in eigene]
for n in werk:
    if n.startswith("INIT "):
        rows.append((n, len(groups) - 1))
    else:
        if n not in fs_cat:
            raise SystemExit(f"{n}: keine Gruppe in fm1-synths")
        rows.append((n, fs_cat[n]))
used = sorted({c for _, c in rows})                          # leere Gruppen weglassen
groups = [groups[c] for c in used]
rows = [(n, used.index(c)) for n, c in rows]
out = ["/* SPDX-License-Identifier: GPL-3.0-only",
       " * Erzeugt von bausteine/varianten/jx8p/tools/sounds.py: Gruppen der Soundliste (Reihenfolge wie",
       " * jx8p_bs_preset_*: eigene zuerst, dann die Werks-Sounds). Nicht von Hand ändern. */",
       f"#define JX_NCAT {len(groups)}u",
       f"#define JX_NSOUND {len(rows)}u",
       "static const char *const jx_cat_names[JX_NCAT] = {" + ", ".join(f'"{g}"' for g in groups) + "};",
       "static const uint8_t jx_sound_cat[JX_NSOUND] = {"]
for i in range(0, len(rows), 16):
    out.append("    " + " ".join(f"{c}," for _, c in rows[i:i + 16]))
out.append("};")
out.append("/* " + "; ".join(f"{i}: {n}" for i, (n, _) in enumerate(rows)) + " */")
(R / "app" / "jx_sounds.h").write_text("\n".join(out) + "\n")
print(len(rows), "Sounds,", len(groups), "Gruppen:", ", ".join(groups))
