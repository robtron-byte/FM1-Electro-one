# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Measure the sound-check renders: for each hit, the strongest frequency
(after the first 5 ms), the level and the time to fall 20 / 40 / 60 dB.
Usage: analyze_808.py <soundcheck directory> [pattern]"""
import glob
import os
import sys
import wave

import numpy as np

SR = 44100


def load(path):
    with wave.open(path) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2) / 32768.0
    return x.mean(axis=1)


def onsets(x, spacing_guess):
    env = np.abs(x)
    found, i, hold = [], 0, int(0.05 * SR)
    while i < len(env):
        if env[i] > 0.01:
            found.append(i)
            i += max(hold, int(spacing_guess * SR * 0.8))
        else:
            i += 1
    return found


def measure(seg):
    peak = np.max(np.abs(seg)) + 1e-12
    env = np.abs(seg)
    # running max envelope in 2 ms windows
    w = int(0.002 * SR)
    frames = len(env) // w
    e = env[: frames * w].reshape(frames, w).max(axis=1)
    times = {}
    for db in (20, 40, 60):
        below = np.where(e < peak * 10 ** (-db / 20))[0]
        times[db] = below[0] * 2 if len(below) else None
    body = seg[int(0.005 * SR): int(0.005 * SR) + 8192]
    if len(body) < 1024:
        body = seg
    spec = np.abs(np.fft.rfft(body * np.hanning(len(body)), n=1 << 16))
    freqs = np.fft.rfftfreq(1 << 16, 1 / SR)
    lo = np.searchsorted(freqs, 25)
    f0 = freqs[lo + np.argmax(spec[lo:])]
    return 20 * np.log10(peak), f0, times


def main():
    folder = sys.argv[1]
    pattern = sys.argv[2] if len(sys.argv) > 2 else "*"
    for path in sorted(glob.glob(os.path.join(folder, pattern + ".wav"))):
        name = os.path.basename(path)[:-4]
        if name.startswith("00_"):
            continue
        x = load(path)
        spacing = 1.6 if ("cy" in name or "oh" in name) else 0.9
        if "decay_sweep" in name and ("bd" in name or "cy" in name or "oh" in name):
            spacing = 1.8
        rows = []
        for start in onsets(x, spacing):
            seg = x[start: start + int(spacing * SR * 0.95)]
            level, f0, t = measure(seg)
            rows.append(f"{level:6.1f} dB {f0:7.1f} Hz  -20dB {t[20]} ms  -40dB {t[40]} ms  -60dB {t[60]} ms")
        print(name)
        for r in rows:
            print("   ", r)


if __name__ == "__main__":
    main()
