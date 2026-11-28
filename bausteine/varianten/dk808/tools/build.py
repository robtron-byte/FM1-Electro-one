#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""808 solo (ADR 0050), Bedienung wie BEAT in FM1-Electro: JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/DK808-<VERSION>.fwsc (und build/dk808.fwsc für den Emulator). DK_LAST=1 in der Umgebung: Messmuster.
Kennung vorerst FM-1_91 (geteilt mit 808 + 101, Arbeitstafel)."""
import os
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
B = R.parents[1]
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

LAST = os.environ.get("DK_LAST", "0")
g.build(dict(name="DK808", root=R, app=R / "app" / "dk_app.c",
             dsp=[B / "dk808" / "dk808.c", B / "dk808" / "dk808_bs.c", R / "app" / "dk_beat.c", G / "shared" / "fx" / "syn_fx.c",
                  G / "fx" / "gm_dist.c"],
             flash=[G / "fx" / "gm_dist_tab.c"],
             inc=[R / "app", B / "dk808", B, G / "fx", B.parent / "sdk_include"],
             defs=["-DDK808_FLOAT=1", "-DDK808_INSTRUMENTS=(DK808_MASK_ALL&~((1u<<DK_MT)|(1u<<DK_MC)))", f"-DDK_LAST={LAST}", "-DSYN_FX_DLY_MAX_S=0.6f", "-DGM_DIST_ROM=1"],
             product="FM-1_910", prefix="FM-1_91", screen="TR-808", colour=0xFBC0, family="DK808",
             store_magic=0x31304B44))   # "DK01"
