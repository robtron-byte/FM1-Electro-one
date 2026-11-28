#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MKS-50 solo bauen (ADR 0047, 0044): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/MKS50-<VERSION>.fwsc (und build/mks50.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).
Stimmen: MK_VOICES=<n> in der Umgebung (Standard unten, im Emulator eingemessen)."""
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

VOICES = int(os.environ.get("MK_VOICES", "6"))
# die Soundliste (app/mk_sounds.h) muss zur Zahl der Presets passen, sonst bliebe die Firmware stumm (kern_boot)
n = sum(len(re.findall(r'^    \{"', p.read_text(), re.M))
        for p in (B / "mks50" / "mks50_eigene.c", REPO / "engine" / "mks_presets.c"))
want = int(re.search(r"#define MK_NSOUND (\d+)u", (R / "app" / "mk_sounds.h").read_text()).group(1))
if n != want:
    raise SystemExit(f"mk_sounds.h hat {want} Sounds, die Presets {n}: python3 {R / 'tools' / 'sounds.py'}")
g.build(dict(name="MKS50", root=R, app=R / "app" / "mk_app.c",
             dsp=[REPO / "engine" / "mks.c", B / "mks50" / "mks50_bs.c", REPO / "engine" / "chords.c",
                  G / "shared" / "fx" / "syn_fx.c"],
             flash=[REPO / "engine" / "mks_presets.c", B / "mks50" / "mks50_eigene.c"],
             inc=[R / "app", B / "mks50", REPO / "engine", REPO / "sdk_include"],
             defs=["-DEL_SYNTH_JX=0", f"-DMKS50_BS_VOICES={VOICES}"],
             product="FM-1_950", prefix="FM-1_95", screen="MKS-50", colour=0x3CBC, family="MKS50",
             store_magic=0x31304B4D))   # "MK01" (fm1-synths-Fassung: "MK50"; deren Daten liest diese Firmware nicht)
