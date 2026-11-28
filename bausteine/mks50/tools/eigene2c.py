#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt bausteine/mks50/mks50_eigene.c: die eigenen MKS-50-Presets des Nutzers (docs/adr/0051).

Quelle: alle *.json im Ordner "Eigene Presets iPhone/mks50/" (Dropbox, MKS50 App), dorthin geholt mit
tools/iphone_presets_pull.sh aus der iOS-App MKS-50 (de.robtron.mks50, Library/Application Support/UserPresets).
Format je Datei (ToneData.swift der App, Quelle "MKS50 App/Mks50-iOS-/"): {"number", "name", "tone": {"name",
"tone": [36 Werte, MKS-50 #0..#35], "patch": {Param.rawValue: Wert}, "chords": [Akkorde der App]}}.

Abbildung auf die 54 Parameter von engine/mks.h (MP_*) wie tools/convert_presets.py für die Werks-Sounds: die 36
Tonwerte unverändert, außer CHORUS: der Kern hat 0 OFF, 1 FREE, 2 I, 3 II, 4 I+II, die App einen Schalter (#10) und
CHORUS MODE (AJ = FREE, I, II, I+II). Dann die Patch-Werte; fehlende nehmen den Standard der App (Param.defaultValue).
CHORD: die Akkordform aus "chords" der Datei wird unter den acht Akkorden des Kerns (engine/chords.c) gesucht; ohne
Treffer bleibt die Nummer und die Datei nennt es. Was der Kern nicht kennt (Arp, Sequencer, Distortion, Delay-/Hall-
Einstellungen außer dem Anteil, MOD SENS, HPF FREQ, ENV SNAP), fällt weg. Presets, die nur eine unveränderte Kopie von
INIT TONE der App sind, werden ausgelassen. Reihenfolge: nach der Nummer im Dateinamen.

Aufruf: eigene2c.py [Quellordner]   (ohne Angabe: der Dropbox-Ordner unten)
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
DEFAULT_SRC = Path.home() / "ediundsepp Dropbox/Ron Zöllner/Privat/PP/Programmieren/Musik/MKS50 App/Eigene Presets iPhone/mks50"

# MP_KEY_MODE .. MP_CHORD in der Reihenfolge von engine/mks.h, je Param.rawValue der App
PATCH = [64, 65, 66, 68, 71, 72, 73, 74, 75, 76, 88, 89, 90, 91, 93, 77, 84, 69]
PATCH_DEFAULT = {65: 30, 68: 100, 71: 12, 72: 64, 75: 8, 76: 40, 90: 8}
CHORUS, CHORUS_MODE, CHORD_PARAM = 10, 148, 69
# max_values in engine/mks.c
MAX = ([3, 3, 3, 3, 5, 5, 3, 3, 3, 3, 4] + [127] * 24 + [12]
       + [3, 127, 2, 127, 24, 127, 1, 127, 127, 127, 6, 1, 13, 1, 1, 127, 127, 7])
assert len(MAX) == 54 and len(PATCH) == 18
# engine/chords.c, mks_chords
KERN_CHORDS = [[0, 3, 7, 10], [0, 2, 7], [0, 3, 7], [0, 7, 12], [0, 5, 7], [0, 4, 7, 10], [0, 4, 7], [0, 3, 6, 10]]
# ToneData.initTone der App (Param.defaultValue), schon abgebildet
INIT_TONE = ([0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 110, 0, 64, 0,
              0, 127, 0, 127, 0, 127, 20, 0, 60, 2] + [PATCH_DEFAULT.get(p, 0) for p in PATCH])
KEY_MODE = 36
RHYTHM, STRINGS, BASS = 1, 2, 3


def values(d, notes):
    tone = d["tone"]
    t, patch = tone["tone"], {int(k): v for k, v in tone.get("patch", {}).items()}
    assert len(t) == 36, "Ton braucht 36 Werte"
    t = list(t)
    t[CHORUS] = 1 + patch.get(CHORUS_MODE, 0) if t[CHORUS] else 0
    p = [patch.get(k, PATCH_DEFAULT.get(k, 0)) for k in PATCH]
    chords = tone.get("chords")
    i = patch.get(CHORD_PARAM, 0)
    if chords and 0 <= i < len(chords):
        if chords[i] in KERN_CHORDS:
            p[-1] = KERN_CHORDS.index(chords[i])
        elif p[KEY_MODE - 36] == 1:
            notes.append(f"Akkord {chords[i]} fehlt im Kern, Nummer {i} bleibt")
    return [min(max(int(x), 0), m) for x, m in zip(t + p, MAX)]


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
    rows, skipped = [], []
    for f in files:
        d = json.loads(f.read_text())
        notes = []
        v = values(d, notes)
        name = c_name(d["tone"].get("name") or d.get("name") or f.stem)
        if v == INIT_TONE:
            skipped.append(f"{f.name} ({name})")
            continue
        rows.append((f.name, d.get("name", name), name, area(name, v), v, notes))
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only",
             " * Erzeugt von bausteine/mks50/tools/eigene2c.py aus den eigenen Presets des Nutzers (iOS-App MKS-50,",
             " * Ordner \"Eigene Presets iPhone/mks50/\", docs/adr/0051). Nicht von Hand ändern: neu erzeugen.",
             f" * {len(rows)} Presets" + (f"; ausgelassen (reine INIT-Kopie): {', '.join(skipped)}" if skipped else "") + ". */",
             '#include "mks50_eigene.h"', "", "const mks_preset_t mks50_eigene[] = {"]
    for fname, long_name, name, a, v, notes in rows:
        lines.append(f'    /* {fname}: {long_name}' + (f" ({'; '.join(notes)})" if notes else "") + " */")
        lines.append(f'    {{"{name}", {a}, {{{", ".join(map(str, v))}}}}},')
    lines += ["};", "const unsigned mks50_eigene_count = sizeof mks50_eigene / sizeof mks50_eigene[0];", ""]
    (HERE / "mks50_eigene.c").write_text("\n".join(lines))
    print(f"{len(rows)} eigene Presets:", ", ".join(r[2] for r in rows))
    for r in rows:
        for n in r[5]:
            print(f"  {r[0]}: {n}")
    if skipped:
        print("ausgelassen (INIT-Kopie):", ", ".join(skipped))


if __name__ == "__main__":
    main()
