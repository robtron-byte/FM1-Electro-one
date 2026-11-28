#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""JX-8P solo bauen (ADR 0046, 0044): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/JX8P-<VERSION>.fwsc (und build/jx8p.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).
Stimmen: JX_VOICES=<n> in der Umgebung (Standard unten, im Emulator eingemessen)."""
import os
import re
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
B = R.parents[1]
REPO = B.parent
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

VOICES = int(os.environ.get("JX_VOICES", "6"))
# die Soundliste (app/jx_sounds.h) muss zur Zahl der Presets passen, sonst bliebe die Firmware stumm (kern_boot)
n = sum(len(re.findall(r'^    \{"', (p).read_text(), re.M))
        for p in (B / "jx8p" / "jx8p_eigene.c", REPO / "engine" / "jx8p_presets.c"))
want = int(re.search(r"#define JX_NSOUND (\d+)u", (R / "app" / "jx_sounds.h").read_text()).group(1))
if n != want:
    raise SystemExit(f"jx_sounds.h hat {want} Sounds, die Presets {n}: python3 {R / 'tools' / 'sounds.py'}")
g.build(dict(name="JX8P", root=R, app=R / "app" / "jx_app.c",
             dsp=[REPO / "engine" / "jx8p.c", B / "jx8p" / "jx8p_bs.c", REPO / "engine" / "chords.c",
                  G / "shared" / "fx" / "syn_fx.c"],
             flash=[REPO / "engine" / "jx8p_names.c", REPO / "engine" / "jx8p_presets.c", B / "jx8p" / "jx8p_eigene.c"],
             inc=[R / "app", B / "jx8p", REPO / "engine", REPO / "sdk_include"],
             defs=["-DEL_SYNTH_JX=1", f"-DJX8P_BS_VOICES={VOICES}"],
             product="FM-1_970", prefix="FM-1_97", screen="JX-8P", colour=0xD225, family="JX8P",
             store_magic=0x3130584A))   # "JX01" (fm1-synths-Fassung: "JX8P"; deren Daten liest diese Firmware nicht, siehe README)
