#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SH-101 solo bauen (ADR 0042, 0044): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/SH101-<VERSION>.fwsc (und build/sh101.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools)."""
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
B = R.parents[1]
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

DEVICE = ["-DSH101_FLOAT=1", "-DSH101_FASTMATH=1", "-DSH101_WITH_POLY=0", "-DSH101_WITH_ARPSEQ=0",
          '-DSH101_TABLES=__attribute__((section(".pool")))']
g.build(dict(name="SH101", root=R, app=R / "app" / "sh_app.c",
             dsp=[B / "sh101" / "sh101.c", G / "shared" / "fx" / "syn_fx.c"],
             double=[B / "sh101" / "sh101.c"],    # (Tabellen beim Start in double; Audiopfad float, SH101_FLOAT)
             inc=[R / "app"], defs=DEVICE,
             product="FM-1_940", prefix="FM-1_94", screen="SH-101", colour=0xF9E5, family="SH101",
             store_magic=0x31304853))
