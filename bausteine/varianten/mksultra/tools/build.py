#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MKS Ultra solo bauen (ADR 0045): JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
Ergebnis build/MKSULTRA-<VERSION>.fwsc. MKU_B=1: Schalter B (VCF 2×, mehr Stimmen) als Paket nur in build/ (A/B)."""
import os
import sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]
B = R.parents[1]
G = R.parent / "gemeinsam"
sys.path.insert(0, str(G / "tools"))
import build as g   # noqa: E402

M = B / "mksultra"
# Stimmen: MKU_VOICES (Standard 1 wie 0.1.0; die Regel „4 Stimmen“ überlastet, ADR 0045), Delay 0,6 s (ein Kanal im Pool, einer im RAM). MKU_VCF2X=1: VCF 2×;
# MKU_AB=1: Paket mit dem Zusatz -ab (nur build/, A/B nach ADR 0006).
VCF2X = os.environ.get("MKU_VCF2X", "0")
AB = os.environ.get("MKU_AB") == "1"
DEVICE = ["-DMKSULTRA_VOICES=" + os.environ.get("MKU_VOICES", "1"), "-DMKSULTRA_MAX_SR=44100", "-DMKSULTRA_FM1=1", "-DMKSULTRA_FX_INT16=1",
          "-DMKSULTRA_DELAY_TENTHS=6", "-DMKSULTRA_DELAY_EXTERN=1", "-DMKSULTRA_PREDELAY_TENTHS=2",
          "-DMKSULTRA_ARP_MAX=256", "-DMKSULTRA_REVERB_CAP=69000", "-DMKSULTRA_VCF2X=" + VCF2X,
          "-DMKU_STATE_BYTES=200000u", "-DMKU_DELAY_BYTES=52924u"]   # (kern_boot prüft beide)
g.build(dict(name="MKSULTRA" + ("-ab" if AB else ""), root=R, app=R / "app" / "mku_app.c",
             dsp=[M / "mksultra.c"], flash=[M / "mksultra_presets.c"],
             double=[M / "mksultra.c", M / "mksultra_presets.c"],   # (Rust-Typen: f64 wo die Vorlage f64 rechnet)
             inc=[R / "app"], defs=DEVICE,
             product="FM-1_930", prefix="FM-1_93", screen="MKS ULTRA", colour=0x33DF, family="MKSU",
             store_magic=0x55534B4D))
