# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Integrated loudness (ITU-R BS.1770 / EBU R128, LUFS) and true-ish peak of
WAV files, without scipy. Usage: loudness.py file.wav [...]"""
import math
import sys
import wave

import numpy as np


def biquad(x, b, a):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i, v in enumerate(x):
        out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, out
        y[i] = out
    return y


def k_weighting(rate):
    # Pre-filter (high shelf) and RLB high-pass, per pyloudnorm's derivation.
    G, Q, fc = 3.999843853973347, 0.7071752369554196, 1681.974450955533
    A = 10 ** (G / 40)
    w0 = 2 * math.pi * fc / rate
    alpha = math.sin(w0) / (2 * Q)
    b = [A * ((A + 1) + (A - 1) * math.cos(w0) + 2 * math.sqrt(A) * alpha),
         -2 * A * ((A - 1) + (A + 1) * math.cos(w0)),
         A * ((A + 1) + (A - 1) * math.cos(w0) - 2 * math.sqrt(A) * alpha)]
    a = [(A + 1) - (A - 1) * math.cos(w0) + 2 * math.sqrt(A) * alpha,
         2 * ((A - 1) - (A + 1) * math.cos(w0)),
         (A + 1) - (A - 1) * math.cos(w0) - 2 * math.sqrt(A) * alpha]
    shelf = ([v / a[0] for v in b], [v / a[0] for v in a])
    fc, Q = 38.13547087602444, 0.5003270373238773
    w0 = 2 * math.pi * fc / rate
    alpha = math.sin(w0) / (2 * Q)
    b = [(1 + math.cos(w0)) / 2, -(1 + math.cos(w0)), (1 + math.cos(w0)) / 2]
    a = [1 + alpha, -2 * math.cos(w0), 1 - alpha]
    hp = ([v / a[0] for v in b], [v / a[0] for v in a])
    return shelf, hp


def lufs(path):
    with wave.open(path) as w:
        rate = w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2) / 32768.0
    shelf, hp = k_weighting(rate)
    ch = [biquad(biquad(x[:, c].copy(), *shelf), *hp) for c in range(2)]
    block, step = int(0.4 * rate), int(0.1 * rate)
    powers = []
    for start in range(0, len(x) - block, step):
        powers.append(sum(np.mean(c[start:start + block] ** 2) for c in ch))
    powers = np.array(powers)
    loud = -0.691 + 10 * np.log10(powers + 1e-20)
    gated = powers[loud > -70]
    rel = -0.691 + 10 * np.log10(gated.mean()) - 10
    final = gated[(-0.691 + 10 * np.log10(gated + 1e-20)) > rel]
    return -0.691 + 10 * np.log10(final.mean()), 20 * np.log10(np.abs(x).max() + 1e-12)


def main():
    for path in sys.argv[1:]:
        value, peak = lufs(path)
        print(f"{value:6.1f} LUFS  peak {peak:5.1f} dBFS  {path}")


if __name__ == "__main__":
    main()
