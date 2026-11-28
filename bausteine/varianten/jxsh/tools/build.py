#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Kombi JX-8P + SH-101 bauen (ADR 0052): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis: build/JXSH-<VERSION>.fwsc (und build/jxsh.fwsc für den Emulator). Auf dem Mac über Docker (gemeinsam/tools).
Klang: Pool-Kern jx.c der JX-8P-Firmware (varianten/jx8p/engine, bitgleich mit dem Baustein jx8p) und der Baustein sh101."""
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
V = R.parent
B = V.parent
G = V / "gemeinsam"
JX = V / "jx8p" / "engine"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

SH101 = ["-DSH101_FLOAT=1", "-DSH101_FASTMATH=1", "-DSH101_WITH_POLY=0", "-DSH101_WITH_ARPSEQ=0",
         '-DSH101_TABLES=__attribute__((section(".pool")))']
g.build(dict(name="JXSH", root=R, app=R / "app" / "jxsh_app.c",
             dsp=[B / "sh101" / "sh101.c", JX / "jx.c", G / "shared" / "fx" / "syn_fx.c", G / "fx" / "gm_dist.c"],
             flash=[JX / "jx_presets.c", JX / "jx_eigene.c", G / "fx" / "gm_dist_tab.c"],
             double=[B / "sh101" / "sh101.c"],
             inc=[R / "app", JX, G / "shared" / "dsp", G / "fx", B.parent / "sdk_include"],
             defs=SH101 + ["-DJX_POOL=4", "-DSYN_FX_DELAY_MAX_S=0.6f", "-DSYN_FX_DLY_MAX_S=0.6f", "-DGM_DIST_ROM=1",
                                   # Speicher: Hall-Diffusoren ins RAMTEXT, Bildpuffer ins NOINIT (Pool und RAM sonst voll)
                                   "-DSYN_FX_DIFF_ATTR=__attribute__((section(\".ram_bss.rvdiff\")))", "-DCV_SECTION=\".noinit.cv\""],
             product="FM-1_990", prefix="FM-1_99", screen="JX+SH", colour=0xEDA5, family="JXSH",
             store_magic=0x48535831))   # "1XSH"
