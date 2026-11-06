#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Compares the float and the fixed-point distortion (host/dist_fixed_check.c,
DIST_FIXED 0 / 1): per type and setting the difference against the float
output; exit 1 if one is above -60 dB.
  dist_fixed_cmp.py <float.raw> <fixed.raw>"""
import sys
import numpy as np
a = np.fromfile(sys.argv[1], dtype=np.float32).astype(float)
b = np.fromfile(sys.argv[2], dtype=np.float32).astype(float)
n = len(a) // 20
worst = -200.0
for m, name in enumerate(['OD', 'DIST', 'RAT', 'TUBE']):
    row = []
    for s in range(5):
        x, y = a[(m * 5 + s) * n:(m * 5 + s + 1) * n], b[(m * 5 + s) * n:(m * 5 + s + 1) * n]
        d = 20 * np.log10(np.sqrt(((x - y) ** 2).mean()) / np.sqrt((x ** 2).mean()) + 1e-12)
        worst = max(worst, d)
        row.append(d)
    print(f"{name:5}", " ".join(f"{v:6.1f}" for v in row), "dB")
sys.exit(1 if worst > -60 else 0)
