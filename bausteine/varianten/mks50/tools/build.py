#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MKS-50 solo bauen (ADR 0047): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/MKS50-<VERSION>.fwsc (und build/mks50.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).

Die MKS-50-Firmware aus fm1-synths (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa, mks50/: App, Pool-Kern mks.c;
Plattform und shared/ des Gerüsts, dieselben Dateien) mit den eigenen Presets des Nutzers (bausteine/mks50). Kennung,
Farbe, Datenfamilie und STORE_MAGIC wie dort: gespeicherte Sounds, Sets, Sequenzen und Journal der 0.2.4 bleiben.
MK_RELEASE_HURRY=1 in der Umgebung: FM1-Electros schnelleres Ausklingen zum Anhören (ADR 0006; Standard 0 wie 0.2.4)."""
import os
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
B = R.parents[1]
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

SH = G / "shared"
g.build(dict(name="MKS50", root=R, app=R / "app" / "mk_app.c",
             dsp=[R / "engine" / "mks.c", SH / "fx" / "syn_fx.c"],
             flash=[R / "engine" / "mks_presets.c", R / "engine" / "mks_cats.c", B / "mks50" / "mks50_eigene.c"],
             inc=[R / "engine", R / "app", B / "mks50", SH / "dsp"],
             defs=[f"-DMKS_POOL=4", "-DSYN_FX_DLY_MAX_S=0.6f", f"-DMK_RELEASE_HURRY={int(os.environ.get('MK_RELEASE_HURRY', '0'))}"],
             product="FM-1_950", prefix="FM-1_95", screen="MKS-50", colour=0x5D5F, family="MKS50",
             store_magic=0x30354B4D))   # "MK50", wie fm1-synths
