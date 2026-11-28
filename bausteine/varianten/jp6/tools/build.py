#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""JP-6 solo bauen (ADR 0048): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/JP6-<VERSION>.fwsc (und build/jp6.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).

App: die JX-8P-App aus fm1-synths (2a7b4fa) mit den Seiten des JP-6 (app/), Kern: engine/jpp.c (bindet engine/jp6.c
von FM1-Electro ein: 4 Parts, Pool, zwei Kerne), Distortion engine/dist.c in float (DIST_FIXED=0, siehe unten).
Plattform und shared/ des Gerüsts. Neue Kennung FM-1_92, eigene STORE_MAGIC „JP06“."""
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
B = R.parents[1]
REPO = B.parent
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

SH = G / "shared"
g.build(dict(name="JP6", root=R, app=R / "app" / "jp_app.c",
             dsp=[R / "engine" / "jpp.c", REPO / "engine" / "dist.c", SH / "fx" / "syn_fx.c"],
             flash=[REPO / "engine" / "jp6_presets.c", B / "jp6" / "jp6_eigene.c"],
             inc=[R / "engine", R / "app", B / "jp6", REPO / "engine", REPO / "sdk_include", SH / "dsp"],
             # Distortion in float ohne Tabellen, wie der Baustein-Test (bitgleich): FM1-Electros Festkomma-Tabellen (49 KB
             # im Pool) und ihr Zustand (7 KB RAM) passen nicht neben Delay und Hall
             defs=["-DDIST_FIXED=0", "-DDIST_TABLE=0"],
             product="FM-1_920", prefix="FM-1_92", screen="JP-6", colour=0xE545, family="JP6",
             store_magic=0x3630504A))   # "JP06"
