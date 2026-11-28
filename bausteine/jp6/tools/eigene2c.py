#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Erzeugt bausteine/jp6/jp6_eigene.c: die eigenen JP-6-Presets des Nutzers (docs/adr/0051).

Quelle: alle *.json im Ordner "Eigene Presets iPhone/jp6/" (Dropbox, MKS50 App). Die App JP-6 (de.robtron.jp6) ist
nicht mehr auf dem iPhone; der Ordner ist die Sicherung vom 05.11.2026. Kommt die App zurück, holt
tools/iphone_presets_pull.sh jp6 neue Presets. Format je Datei (ToneData.swift der App, Quelle
"MKS50 App/JP6-iOS/"): {"number", "name", "tone": {"name", "tone": [50 Werte, Index = Param.rawValue 0..49],
"patch": {Param.rawValue: Wert}, "chords": [Akkorde der App]}}.

Abbildung auf die 69 Parameter von engine/jp6.h (JP_*) mit denselben Namen wie tools/convert_jp6_presets.py
(Werks-Sounds). Die Param.rawValue-Nummern unten stammen aus JP6-iOS/Shared/Model/Param.swift (28.11.2026) und passen
genau zu den Schlüsseln der gesicherten Presets. Fehlende Werte nehmen den Standard der App (Param.defaultValue).
CHORD: die Akkordform aus "chords" wird unter den acht Akkorden des Kerns gesucht; ohne Treffer bleibt die Nummer und
die Datei nennt es. Was der Kern nicht kennt (Aftertouch, LFO 2, Arp, Sequencer, Delay-/Hall-Einstellungen außer dem
Anteil), fällt weg. Presets, die nur eine unveränderte Kopie von INIT TONE der App sind, werden ausgelassen.

Aufruf: eigene2c.py [Quellordner]   (ohne Angabe: der Dropbox-Ordner unten)
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
DEFAULT_SRC = Path.home() / "ediundsepp Dropbox/Ron Zöllner/Privat/PP/Programmieren/Musik/MKS50 App/Eigene Presets iPhone/jp6"

# engine/jp6.h-Reihenfolge: (Name in der App, Param.rawValue, Param.defaultValue)
ORDER = [
    ("vco1Tri", 0, 0), ("vco1Saw", 1, 1), ("vco1Pulse", 2, 0), ("vco1Square", 3, 0), ("vco1Range", 4, 2),
    ("vco1CrossMod", 5, 0), ("vcoSync", 6, 0), ("vco2Tri", 7, 0), ("vco2Saw", 8, 1), ("vco2Pulse", 9, 0),
    ("vco2Noise", 10, 0), ("vco2Range", 11, 3), ("vco2Tune", 12, 12), ("vco2Fine", 13, 64), ("pwmDepth", 14, 0),
    ("pwmSource", 15, 1), ("vcoLfoDepth", 16, 0), ("vcoEnvDepth", 17, 0), ("vcoModDest", 18, 2), ("sourceMix", 20, 64),
    ("hpfCutoff", 21, 0), ("vcfMode", 22, 0), ("vcfCutoff", 23, 127), ("vcfResonance", 24, 0), ("vcfEnvDepth", 25, 0),
    ("vcfEnvSource", 26, 0), ("vcfLfoDepth", 27, 0), ("vcfKeyFollow", 28, 0), ("vcfVelocity", 30, 0),
    ("vcaLevel", 31, 110), ("vcaLfoDepth", 32, 0), ("vcaVelocity", 34, 0), ("lfoRate", 35, 64), ("lfoDelay", 36, 0),
    ("lfoWave", 37, 0), ("env1Attack", 38, 0), ("env1Decay", 39, 64), ("env1Sustain", 40, 64), ("env1Release", 41, 40),
    ("env1KeyFollow", 42, 0), ("env1Polarity", 43, 0), ("env2Attack", 44, 0), ("env2Decay", 45, 64),
    ("env2Sustain", 46, 127), ("env2Release", 47, 30), ("env2KeyFollow", 48, 0), ("vco1CrossModEnv", 49, 0),
    ("keyMode", 64, 0), ("portaTime", 65, 30), ("portaSwitch", 66, 0), ("volume", 68, 100), ("keyShift", 71, 12),
    ("voiceDrift", 73, 16), ("unisonDetune", 74, 40), ("lfoSync", 86, 0), ("lfoDivision", 87, 8),
    ("lfoKeySync", 88, 0), ("envRetrigger", 89, 0), ("chorus", 127, 0), ("chorusRate", 128, 60),
    ("chorusMode", 129, 0), ("distMode", 118, 0), ("distDrive", 119, 64), ("distTone", 120, 80),
    ("distLevel", 121, 64), ("distMix", 122, 127), ("delayMix", 75, 0), ("reverbMix", 82, 0), ("chordIndex", 69, 0),
]
TONE_COUNT = 50
# max_values in engine/jp6.c
MAX = ([1, 1, 1, 1, 4, 127, 2, 1, 1, 1, 1, 5, 24, 127, 127, 2, 127, 127, 2, 127, 127, 2, 127, 127, 127, 1, 127, 127,
        127, 127, 127, 127, 127, 127, 6, 127, 127, 127, 127, 127, 1, 127, 127, 127, 127, 127, 127,
        3, 127, 2, 127, 24, 127, 127, 1, 13, 1, 1, 1, 127, 3, 4, 127, 127, 127, 127, 127, 127, 7])
assert len(ORDER) == 69 == len(MAX)
KERN_CHORDS = [[0, 3, 7, 10], [0, 2, 7], [0, 3, 7], [0, 7, 12], [0, 5, 7], [0, 4, 7, 10], [0, 4, 7], [0, 3, 6, 10]]
INIT_TONE = [min(d, m) for (_, _, d), m in zip(ORDER, MAX)]
KEY_MODE = 47
RHYTHM, STRINGS, BASS = 1, 2, 3


def values(d, notes):
    tone = d["tone"]
    t, patch = tone["tone"], {int(k): v for k, v in tone.get("patch", {}).items()}
    assert len(t) == TONE_COUNT, f"Ton braucht {TONE_COUNT} Werte"
    v = [t[raw] if raw < TONE_COUNT else patch.get(raw, default) for _, raw, default in ORDER]
    chords, i = tone.get("chords"), v[-1]
    if chords and 0 <= i < len(chords):
        if chords[i] in KERN_CHORDS:
            v[-1] = KERN_CHORDS.index(chords[i])
        elif v[KEY_MODE] == 1:
            notes.append(f"Akkord {chords[i]} fehlt im Kern, Nummer {i} bleibt")
    return [min(max(int(x), 0), m) for x, m in zip(v, MAX)]


def area(name, v):
    n = f" {name.upper()} "
    if v[KEY_MODE] == 2 or "BASS" in n or " BA " in n or " B " in n:
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
             " * Erzeugt von bausteine/jp6/tools/eigene2c.py aus den eigenen Presets des Nutzers (iOS-App JP-6,",
             " * Ordner \"Eigene Presets iPhone/jp6/\", docs/adr/0051). Nicht von Hand ändern: neu erzeugen.",
             f" * {len(rows)} Presets" + (f"; ausgelassen (reine INIT-Kopie): {', '.join(skipped)}" if skipped else "") + ". */",
             '#include "jp6_eigene.h"', "", "const jp6_preset_t jp6_eigene[] = {"]
    for fname, long_name, name, a, v, notes in rows:
        lines.append(f'    /* {fname}: {long_name}' + (f" ({'; '.join(notes)})" if notes else "") + " */")
        lines.append(f'    {{"{name}", {a}, {{{", ".join(map(str, v))}}}}},')
    lines += ["};", "const unsigned jp6_eigene_count = sizeof jp6_eigene / sizeof jp6_eigene[0];", ""]
    (HERE / "jp6_eigene.c").write_text("\n".join(lines))
    print(f"{len(rows)} eigene Presets:", ", ".join(r[2] for r in rows))
    for r in rows:
        for n in r[5]:
            print(f"  {r[0]}: {n}")
    if skipped:
        print("ausgelassen (INIT-Kopie):", ", ".join(skipped))


if __name__ == "__main__":
    main()
