#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""JP-6 solo bauen (ADR 0048, 0044): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/JP6-<VERSION>.fwsc (und build/jp6.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).
Stimmen: JP_VOICES=<n> in der Umgebung (Standard unten, im Emulator eingemessen)."""
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

VOICES = int(os.environ.get("JP_VOICES", "6"))
# die Soundliste (app/jp_sounds.h) muss zur Zahl der Presets passen, sonst bliebe die Firmware stumm (kern_boot)
n = sum(len(re.findall(r'^    \{"', p.read_text(), re.M))
        for p in (B / "jp6" / "jp6_eigene.c", REPO / "engine" / "jp6_presets.c"))
want = int(re.search(r"#define JP_NSOUND (\d+)u", (R / "app" / "jp_sounds.h").read_text()).group(1))
if n != want:
    raise SystemExit(f"jp_sounds.h hat {want} Sounds, die Presets {n}: python3 {R / 'tools' / 'sounds.py'}")
g.build(dict(name="JP6", root=R, app=R / "app" / "jp_app.c",
             dsp=[REPO / "engine" / "jp6.c", B / "jp6" / "dist_aus.c", B / "jp6" / "jp6_bs.c",
                  REPO / "engine" / "chords.c", G / "shared" / "fx" / "syn_fx.c"],
             flash=[REPO / "engine" / "jp6_presets.c", B / "jp6" / "jp6_eigene.c"],
             inc=[R / "app", B / "jp6", REPO / "engine", REPO / "sdk_include"],
             defs=["-DEL_SYNTH_JX=1", f"-DJP6_BS_VOICES={VOICES}",
                   # ohne Distortion (Wunsch des Nutzers 28.11.2026): bausteine/jp6/dist_aus.c statt engine/dist.c
                   "-DJP6_BS_DIST=0"],
             product="FM-1_920", prefix="FM-1_92", screen="JP-6", colour=0xE545, family="JP6",
             store_magic=0x3130504A))   # "JP01"
