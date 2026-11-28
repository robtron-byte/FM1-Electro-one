#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt engine/jx_eigene.c: die eigenen Presets des Nutzers (ADR 0051) im Format dieser Firmware (83 Werte JP_*,
Index = JX-8P-Parameternummer, wie engine/jx_presets.c), aus bausteine/jx8p/jx8p_eigene.c (68 Werte JX_*).

Die Umsetzung ist die Umkehrung von tools/jx8p_presets.py im Hauptordner (KEEP); die übrigen Plätze bleiben 0.
Gruppe je Preset (für „speichern unter“): BASS bei Bereich BASS, STR+BRASS bei STRINGS, sonst LEAD.
Neue iPhone-Presets: tools/iphone_presets_pull.sh jx8p, dann dieses Skript, dann bauen."""
import re
from pathlib import Path

R = Path(__file__).resolve().parents[1]
B = R.parents[1]
KEEP = list(range(11, 23)) + list(range(26, 57)) + [58] + list(range(59, 83))
assert len(KEEP) == 68
CAT = {1: 2, 2: 8, 3: 1}          # Bereich -> jx_cat_names: LEAD, STR+BRASS, BASS

rows = re.findall(r'/\* (.*?) \*/\n    \{"([^"]*)", (\d), \{([^}]*)\}\}', (B / "jx8p" / "jx8p_eigene.c").read_text())
out = ["/* SPDX-License-Identifier: GPL-3.0-only",
       " * Erzeugt von tools/eigene83.py aus bausteine/jx8p/jx8p_eigene.c: die eigenen Presets des Nutzers aus der",
       " * JX-8P-iOS-App (docs/adr/0051 im Hauptordner), 83 Werte wie engine/jx_presets.c. Nicht von Hand ändern. */",
       '#include "jx.h"', "", "const jx_preset_t jx_eigene[] = {"]
for comment, name, area, vals in rows:
    v68 = [int(x) for x in vals.split(",")]
    v = [0] * 83
    for k, j in enumerate(KEEP):
        v[j] = v68[k]
    out.append(f"    /* {comment} */")
    out.append(f'    {{"{name}", {CAT[int(area)]}, {{{", ".join(map(str, v))}}}}},')
out += ["};", "const unsigned jx_eigene_count = sizeof jx_eigene / sizeof jx_eigene[0];", ""]
(R / "engine" / "jx_eigene.c").write_text("\n".join(out))
print(len(rows), "eigene Presets:", ", ".join(r[1] for r in rows))
