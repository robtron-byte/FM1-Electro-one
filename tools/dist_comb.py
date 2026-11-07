#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Comb filtering of the distortion send (docs/adr/0030): the held chord with
the send up against the same chord without it. Prints the deepest notch and
the highest peak of the level change per frequency, 150 Hz - 10 kHz, in dB.
Usage: dist_comb.py <without.f32> <with.f32> [drive-label]"""
import sys
import numpy as np

SR = 44100


def spectrum(path):
    x = np.fromfile(path, dtype=np.float32)[SR // 2:]          # after the attack
    n = 8192
    segs = [x[i:i + n] * np.hanning(n) for i in range(0, len(x) - n, n // 2)]
    return np.fft.rfftfreq(n, 1 / SR), np.mean([np.abs(np.fft.rfft(s)) ** 2 for s in segs], axis=0)


f, a = spectrum(sys.argv[1])
_, b = spectrum(sys.argv[2])
band = (f >= 150) & (f <= 10000)
k = np.ones(9) / 9
db = 10 * np.log10(np.convolve(b, k, "same")[band] / (np.convolve(a, k, "same")[band] + 1e-20) + 1e-20)
print(f"{sys.argv[3] if len(sys.argv) > 3 else '':28s} deepest notch {db.min():6.1f} dB at {f[band][db.argmin()]:6.0f} Hz   highest peak {db.max():5.1f} dB   mean {db.mean():5.1f} dB")
