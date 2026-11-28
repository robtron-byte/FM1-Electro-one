#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt bausteine/jx8p/jx8p_eigene.c: die eigenen JX-8P-Presets des Nutzers (docs/adr/0051).

Quelle: alle *.json im Ordner "Eigene Presets iPhone/jx8p/" (Dropbox, MKS50 App), dorthin geholt mit
tools/iphone_presets_pull.sh aus der iOS-App JX-8P (de.robtron.jx8p, Library/Application Support/UserPresets).
Format je Datei (ToneData.swift der App): {"number", "name", "tone": {"name", "tone": [59 Werte, Index =
JX-8P-Parameternummer #11..#58], "patch": {Param.rawValue: Wert}, "chords"}}.

Die Werte werden auf die 68 Parameter von engine/jx8p.h (JX_*) abgebildet, wie fm1-synths die Werks-Sounds abbildet
(tools/jx8p_presets.py): Ton #11..#22, #26..#56, #58, dann die Patch-Werte. Fehlende Patch-Werte nehmen den
Standard der App (Param.defaultValue). Was der Kern nicht kennt (Arp, Sequencer, Distortion, Delay-/Hall-Einstellungen
außer dem Anteil, eigene Akkordformen), fällt weg. Presets, die nur eine unveränderte Kopie eines INIT sind (die drei PG8X-INIT aus engine/jx8p_presets.c
und INIT TONE der App), werden
ausgelassen. Reihenfolge: nach der Nummer im Dateinamen (preset1, preset2, ...).

Aufruf: eigene2c.py [Quellordner]   (ohne Angabe: der Dropbox-Ordner unten)
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
ROOT = HERE.parent.parent
DEFAULT_SRC = Path.home() / "ediundsepp Dropbox/Ron Zöllner/Privat/PP/Programmieren/Musik/MKS50 App/Eigene Presets iPhone/jx8p"

TONE = list(range(11, 23)) + list(range(26, 57)) + [58]          # JX_DCO1_RANGE .. JX_VCA_ENV_MODE
# JX_BENDER .. JX_CHORD in der Reihenfolge von engine/jx8p.h, je Param.rawValue der App
PATCH = [0, 1, 3, 4, 5, 2, 129, 64, 65, 66, 67, 68, 71, 72, 73, 85, 86, 87, 88, 90, 124, 74, 81, 69]
assert len(TONE) == 44 and len(TONE) + len(PATCH) == 68
# Param.defaultValue der App für diese Patch-Werte
PATCH_DEFAULT = {0: 2, 1: 64, 2: 60, 65: 30, 67: 64, 68: 100, 71: 12, 72: 8, 73: 40, 87: 8}
# jx8p_param_max (engine/jx8p.c), in der Reihenfolge JX_*
MAX = ([3, 3, 24, 127, 127, 3, 3, 3, 24, 127, 127, 127, 3, 3, 127, 127, 127, 3, 3, 3, 127, 127, 127, 127, 127, 3, 3,
        127, 3, 2, 2, 127, 127, 127, 127, 127, 127, 3, 127, 127, 127, 127, 3, 1]
       + [12, 127, 127, 127, 127, 127, 2, 3, 127, 2, 127, 127, 24, 127, 127, 7, 1, 13, 1, 1, 1, 127, 127, 7])
assert len(MAX) == 68
# ToneData.initTone der App ("INIT TONE": jeder Wert Param.defaultValue), Ton in der Reihenfolge von TONE
INIT_TONE = [1, 3, 12, 0, 0, 1, 3, 0, 12, 64, 0, 0, 0, 3, 127, 0, 0, 0, 3, 0, 127, 0, 0, 0, 0, 0, 3, 100, 0, 0, 2, 0,
             64, 0, 40, 127, 20, 0, 0, 40, 127, 20, 0, 1] + [PATCH_DEFAULT.get(p, 0) for p in PATCH]
assert len(INIT_TONE) == 68
KEY_MODE = 44 + 7
RHYTHM, STRINGS, BASS = 1, 2, 3


def values(d):
    tone = d["tone"]
    t, patch = tone["tone"], {int(k): v for k, v in tone.get("patch", {}).items()}
    assert len(t) == 59, "Ton braucht 59 Werte"
    v = [t[i] for i in TONE] + [patch.get(p, PATCH_DEFAULT.get(p, 0)) for p in PATCH]
    return [min(max(int(x), 0), m) for x, m in zip(v, MAX)]


def factory_inits():
    text = (ROOT / "engine" / "jx8p_presets.c").read_text()
    return [[int(x) for x in vals.split(",")] for name, vals in
            re.findall(r'\{"(INIT [A-Z]+)", \d+, \{([^}]*)\}\}', text)]


def area(name, v):
    n = f" {name.upper()} "
    if v[KEY_MODE] == 2 or "BASS" in n or " B " in n:
        return BASS
    if "STR" in n or "PAD" in n:
        return STRINGS
    return RHYTHM


def c_name(s):
    s = "".join(c if 0x20 <= ord(c) < 0x7f else " " for c in s.upper())[:10].rstrip()
    return s.replace("\\", "/").replace('"', "'")


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SRC
    files = sorted(src.glob("*.json"), key=lambda f: [int(x) if x.isdigit() else x for x in re.split(r"(\d+)", f.name)])
    if not files:
        sys.exit(f"keine JSON in {src}")
    inits = factory_inits()
    assert len(inits) == 3
    inits.append(INIT_TONE)
    rows, skipped = [], []
    for f in files:
        d = json.loads(f.read_text())
        v = values(d)
        name = c_name(d["tone"].get("name") or d.get("name") or f.stem)
        if v in inits:
            skipped.append(f"{f.name} ({name})")
            continue
        rows.append((f.name, d.get("name", name), name, area(name, v), v))
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only",
             " * Erzeugt von bausteine/jx8p/tools/eigene2c.py aus den eigenen Presets des Nutzers (iOS-App JX-8P,",
             " * Ordner \"Eigene Presets iPhone/jx8p/\", docs/adr/0051). Nicht von Hand ändern: neu erzeugen.",
             f" * {len(rows)} Presets" + (f"; ausgelassen (reine INIT-Kopie): {', '.join(skipped)}" if skipped else "") + ". */",
             '#include "jx8p_eigene.h"', "", "const jx8p_preset_t jx8p_eigene[] = {"]
    for fname, long_name, name, a, v in rows:
        lines.append(f'    /* {fname}: {long_name} */')
        lines.append(f'    {{"{name}", {a}, {{{", ".join(map(str, v))}}}}},')
    lines += ["};", "const unsigned jx8p_eigene_count = sizeof jx8p_eigene / sizeof jx8p_eigene[0];", ""]
    (HERE / "jx8p_eigene.c").write_text("\n".join(lines))
    print(f"{len(rows)} eigene Presets", ", ".join(r[2] for r in rows))
    if skipped:
        print("ausgelassen (INIT-Kopie):", ", ".join(skipped))


if __name__ == "__main__":
    main()
