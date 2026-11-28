#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""JX-8P solo bauen (ADR 0046): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/JX8P-<VERSION>.fwsc (und build/jx8p.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).

Die Firmware ist die JX-8P-Firmware aus fm1-synths (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa, jx8p/: App,
Pool-Kern jx.c; Plattform und shared/ sind die des Gerüsts, dieselben Dateien) mit den eigenen Presets des Nutzers.
Kennung, Farbe, Datenfamilie und STORE_MAGIC wie dort (fm1-synths jx8p/tools/build.py): gespeicherte Sounds, Sets,
Sequenzen und das Journal der Fassung 0.2.4 bleiben gültig. Übersetzt wie dort (App -Os mit platform.c, Kern -O2)."""
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

SH = G / "shared"
g.build(dict(name="JX8P", root=R, app=R / "app" / "jx_app.c",
             dsp=[R / "engine" / "jx.c", SH / "fx" / "syn_fx.c"],
             flash=[R / "engine" / "jx_presets.c", R / "engine" / "jx_eigene.c"],
             inc=[R / "engine", R / "app", SH / "dsp"],
             defs=["-DJX_POOL=4", "-DSYN_FX_DLY_MAX_S=0.6f"],   # Pool 4, Delay höchstens 0,6 s (Regel des Nutzers 28.11.2026)
             product="FM-1_970", prefix="FM-1_97", screen="JX-8P", colour=0xE9C5, family="JX8P",
             store_magic=0x5038584A))   # "JX8P", wie fm1-synths
