#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Compare the 808 snare model (host/snare_check.c renders) with recordings
of a real TR-808 snare at TONE / SNAPPY settings SD<tone><snappy>.wav.

Per setting: level and decay (to -20 dB) of the two body modes (150-200 Hz,
300-385 Hz), noise level 1.5-12 kHz over the first 100 ms and its decay
(-20 / -40 dB); 1/3-octave spectrum 1-12.7 kHz at SD5010 (noise). Levels are
compared after one common gain offset.
Usage: analyze_snare.py <render dir> <recordings dir> [--score]"""
import glob
import math
import os
import sys
import wave

import numpy as np
from scipy.signal import butter, sosfiltfilt

SR = 44100


def load(path):
    if path.endswith(".raw"):
        return np.fromfile(path, dtype=np.float32).astype(float)
    w = wave.open(path)
    return np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16) / 32768.0


def bp(x, lo, hi):
    sos = butter(4, [lo, hi], btype="band", fs=SR, output="sos")
    return sosfiltfilt(sos, np.concatenate([np.zeros(2048), x]))[2048:]


def t_drop(x, db):
    e = np.sqrt(np.convolve(x * x, np.ones(88) / 88, "same"))
    i = int(np.argmax(e))
    j = np.nonzero(e[i:] < e[i] * 10 ** (-db / 20))[0]
    return j[0] / 44.1 if len(j) else 500.0


def features(x):
    on = int(np.argmax(np.abs(x) > 0.02 * np.abs(x).max()))
    x = x[on:on + int(0.25 * SR)]
    x = np.concatenate([x, np.zeros(max(0, int(0.25 * SR) - len(x)))])
    lo, hi, nz = bp(x, 150, 200), bp(x, 300, 385), bp(x, 1500, 12000)
    db = lambda y: 20 * math.log10(np.abs(y).max() + 1e-9)
    return {"lo": db(lo), "lo_t": t_drop(lo, 20), "hi": db(hi), "hi_t": t_drop(hi, 20),
            "nz": 10 * math.log10(np.mean(nz[:4410] ** 2) + 1e-12), "nz_t20": t_drop(nz, 20), "nz_t40": t_drop(nz, 40), "x": x}


def thirds(x):
    tot = bp(x, 60, 16000)
    et = np.sum(tot[:int(0.2 * SR)] ** 2)
    return np.array([10 * math.log10(np.sum(bp(x, f / 2 ** (1 / 6), f * 2 ** (1 / 6))[:int(0.2 * SR)] ** 2) / et)
                     for f in (1000 * 2 ** (k / 3) for k in range(12))])


def main():
    ours_dir, ref_dir = sys.argv[1], sys.argv[2]
    rows = []
    for p in sorted(glob.glob(os.path.join(ref_dir, "SD*.wav"))):
        key = os.path.basename(p)[2:6]
        rows.append((key, features(load(p)), features(load(os.path.join(ours_dir, f"SD{key}.raw")))))
    # One common gain: match the mean body level.
    off = np.mean([r["lo"] - o["lo"] for _, r, o in rows])
    lv, dec, nz = [], [], []
    for key, r, o in rows:
        lv += [o["lo"] + off - r["lo"], o["hi"] + off - r["hi"]]
        dec += [math.log2(o["lo_t"] / r["lo_t"]), math.log2(o["hi_t"] / r["hi_t"])]
        if key[2:] in ("50", "75", "10"):
            nz.append(o["nz"] + off - r["nz"])
            dec += [math.log2(o["nz_t20"] / r["nz_t20"]), math.log2(o["nz_t40"] / r["nz_t40"])]
    th = thirds(rows[[k for k, _, _ in rows].index("5010")][2]["x"]) - thirds(rows[[k for k, _, _ in rows].index("5010")][1]["x"])
    rms = lambda a: float(np.sqrt(np.mean(np.square(a))))
    score = rms(lv) / 1.5 + rms(nz) / 1.5 + rms(dec) / 0.15 + rms(th) / 3.0
    if "--score" in sys.argv:
        print(f"{score:.4f}")
        return
    print("set   low mode ours/808 (t20)        high mode ours/808 (t20)       noise ours/808 (t20, t40)")
    for key, r, o in rows:
        print(f"SD{key} {o['lo'] + off:6.1f} {r['lo']:6.1f} ({o['lo_t']:4.0f}/{r['lo_t']:4.0f})   "
              f"{o['hi'] + off:6.1f} {r['hi']:6.1f} ({o['hi_t']:4.0f}/{r['hi_t']:4.0f})   "
              f"{o['nz'] + off:6.1f} {r['nz']:6.1f} ({o['nz_t20']:4.0f}/{r['nz_t20']:4.0f}, {o['nz_t40']:4.0f}/{r['nz_t40']:4.0f})")
    print("SD5010 1/3 octaves 1-12.7 kHz, ours - 808:", " ".join(f"{v:5.1f}" for v in th))
    print(f"rms: levels {rms(lv):.2f} dB, noise {rms(nz):.2f} dB, decays {rms(dec):.3f} oct, thirds {rms(th):.2f} dB; score {score:.3f}")


if __name__ == "__main__":
    main()
