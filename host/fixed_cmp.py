#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Compares a float and a fixed-point render (raw float32) in equal parts:
the difference against the float one per part; exit 1 above the limit.
  fixed_cmp.py <float.raw> <fixed.raw> <parts> <limit dB> [names...]"""
import sys
import numpy as np
a = np.fromfile(sys.argv[1], dtype=np.float32).astype(float)
b = np.fromfile(sys.argv[2], dtype=np.float32).astype(float)
parts, limit, names = int(sys.argv[3]), float(sys.argv[4]), sys.argv[5:]
n = len(a) // parts
worst = -300.0
for p in range(parts):
    x, y = a[p * n:(p + 1) * n], b[p * n:(p + 1) * n]
    d = 20 * np.log10(np.sqrt(((x - y) ** 2).mean()) / max(np.sqrt((x ** 2).mean()), 1e-12) + 1e-15)
    worst = max(worst, d)
    print(f"{names[p] if p < len(names) else p:16} difference {d:6.1f} dB")
sys.exit(1 if worst > limit else 0)
