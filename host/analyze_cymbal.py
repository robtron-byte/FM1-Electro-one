#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Compare the 808 cymbal model with measurements of a real TR-808.

Reference numbers: band measurements of the Fischer "Roland TR-808 Rhythm
Composer Sound Sample Set" (1994) cymbal files CY<TONE><DECAY>, as published
in the gf180-parasynth project (docs/scorecard/cymbal-369, fischer.json and
shipped-vs-CY5025.json). The same method is used here: 4th-order Butterworth
bands, zero phase; L 2-5 kHz, Ln 2.9-4.1 kHz, H 6-14 kHz; energy share of
the first second re 200 Hz-20 kHz; EDT10 off the floor-subtracted Schroeder
curve; 1/3-octave energy per time window.

Usage: analyze_cymbal.py <dir from cymbal_check> [--json]  (prints the score)
"""
import json
import math
import sys

import numpy as np
from scipy.signal import butter, sosfiltfilt

SR = 44100
POS = ["00", "25", "50", "75", "10"]
# TONE x DECAY -> (H-L dB, L EDT10 ms, Ln EDT10 ms, H EDT10 ms, H late T20 ms or None)
REF = {
    "0000": (7.217, 378.3, 401.3, 139.1, 250.2), "0010": (4.128, 1231.1, 1280.1, 320.9, 1092.9),
    "0025": (5.446, 586.4, 617.8, 182.8, 446.6), "0050": (4.786, 860.8, 890.4, 229.2, 726.8),
    "0075": (4.204, 1114.1, 1142.7, 295.6, 980.1), "1000": (14.547, 319.8, 363.7, 97.4, 295.1),
    "1010": (11.408, 1165.0, 1236.1, 136.2, None), "1025": (13.532, 535.6, 583.6, 110.7, 394.6),
    "1050": (12.52, 804.4, 880.5, 119.5, 609.4), "1075": (11.347, 1064.0, 1123.4, 137.1, None),
    "2500": (8.078, 372.8, 390.7, 143.2, 254.2), "2510": (5.046, 1236.5, 1275.4, 284.9, 1090.7),
    "2525": (6.752, 589.6, 615.4, 166.2, 445.3), "2550": (5.496, 874.9, 908.3, 212.9, 730.6),
    "2575": (5.201, 1112.5, 1147.1, 253.8, 979.2), "5000": (9.522, 368.0, 396.4, 128.1, 265.3),
    "5010": (6.396, 1224.2, 1272.5, 235.7, 1094.0), "5025": (8.164, 574.5, 591.2, 147.8, 432.4),
    "5050": (7.062, 859.4, 892.1, 180.4, 721.0), "5075": (6.49, 1090.7, 1118.5, 214.1, 962.0),
    "7500": (11.049, 357.7, 387.8, 121.3, 273.9), "7510": (8.025, 1225.9, 1280.2, 190.2, 1081.8),
    "7525": (9.865, 568.6, 599.8, 138.7, 411.4), "7550": (8.439, 845.4, 886.5, 164.7, 697.1),
    "7575": (8.09, 1111.7, 1145.2, 184.0, 966.4),
}
# CY5025 (TONE 5, DECAY 2.5): 1/3-octave energy, dB re 200 Hz-20 kHz, per window.
THIRDS_REF = {
    (0.0, 0.05): [-48.4, -44.71, -38.72, -37.45, -31.42, -22.62, -18.78, -15.06, -4.54, -8.34, -7.45, -7.83, -15.83, -23.04],
    (0.05, 0.3): [-37.6, -34.13, -25.46, -23.47, -16.18, -11.97, -14.43, -12.59, -4.12, -8.38, -10.11, -10.66, -15.91, -23.38],
    (0.3, 1.0): [-28.87, -26.35, -18.66, -15.6, -8.03, -4.89, -10.54, -14.39, -7.6, -12.82, -15.56, -15.66, -20.99, -27.0],
}
THIRDS = [1000.0 * 2 ** (k / 3) for k in range(14)]
LEAD = 4096


def bp(y, lo, hi):
    hi = min(hi, 0.45 * SR)
    sos = butter(4, [lo, hi], btype="bandpass", fs=SR, output="sos")
    return sosfiltfilt(sos, np.concatenate([np.zeros(LEAD), y]))[LEAD:]


def edt10(x):
    p = x * x
    floor = float(np.mean(p[-int(0.1 * SR):]))
    q = np.maximum(p - floor, 0.0)
    sch = np.cumsum(q[::-1])[::-1]
    c = 10 * np.log10(np.maximum(sch / sch[0], 1e-30))
    i10 = np.nonzero(c <= -10.0)[0]
    i30 = np.nonzero(c <= -30.0)[0]
    edt = 1e3 * i10[0] / SR if len(i10) else None
    t20 = None
    if len(i10) and len(i30):
        a, b = i10[0], i30[0]
        t = np.arange(a, b + 1) / SR
        slope = np.polyfit(t, c[a:b + 1], 1)[0]
        if slope < 0:
            t20 = -20.0 / slope * 1e3
    return edt, t20


def measure(y):
    tot = bp(y, 200, 20000)
    n = SR
    e_tot = float(np.sum(tot[:n] ** 2))
    res = {}
    for name, (lo, hi) in {"L": (2000, 5000), "Ln": (2900, 4100), "H": (6000, 14000)}.items():
        xb = bp(y, lo, hi)
        res[name + "_db"] = 10 * math.log10(float(np.sum(xb[:n] ** 2)) / e_tot)
        res[name + "_edt"], res[name + "_t20"] = edt10(xb)
    res["HL"] = res["H_db"] - res["L_db"]
    return res


def thirds(y, t0, t1):
    tot = bp(y, 200, 20000)
    a, b = int(t0 * SR), int(t1 * SR)
    e_tot = float(np.sum(tot[a:b] ** 2))
    out = []
    for fc in THIRDS:
        lo, hi = fc / 2 ** (1 / 6), min(fc * 2 ** (1 / 6), 0.45 * SR)
        xb = bp(y, lo, hi)
        out.append(10 * math.log10(max(float(np.sum(xb[a:b] ** 2)), 1e-30) / e_tot))
    return out


def load(d, key):
    return np.fromfile(f"{d}/CY{key}.raw", dtype=np.float32).astype(np.float64)


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "build/cy"
    quiet = "--score" in sys.argv
    err_hl, err_edt, err_t20, rows = [], [], [], []
    for key, ref in REF.items():
        m = measure(load(d, key))
        err_hl.append(m["HL"] - ref[0])
        err_edt.append(math.log2(m["Ln_edt"] / ref[2]) if m["Ln_edt"] else 3.0)
        err_edt.append(math.log2(m["H_edt"] / ref[3]) if m["H_edt"] else 3.0)
        if ref[4] and m["H_t20"]:
            err_t20.append(math.log2(m["H_t20"] / ref[4]))
        rows.append((key, m, ref))
    y = load(d, "5025")
    th_err = []
    for (t0, t1), ref in THIRDS_REF.items():
        ours = thirds(y, t0, t1)
        th_err.append([o - r for o, r in zip(ours, ref)])
    th = np.array(th_err)
    score = (np.sqrt(np.mean(np.square(err_hl))) / 2.0 + np.sqrt(np.mean(np.square(err_edt))) / 0.15
             + (np.sqrt(np.mean(np.square(err_t20))) / 0.15 if err_t20 else 0) + np.sqrt(np.mean(th ** 2)) / 3.0)
    if quiet:
        print(f"{score:.4f}")
        return
    print("setting   H-L ours/808     Ln EDT ours/808     H EDT ours/808    H T20 ours/808")
    for key, m, ref in rows:
        f = lambda v: f"{v:7.0f}" if v else "      -"
        print(f"CY{key}  {m['HL']:6.1f} {ref[0]:6.1f}   {f(m['Ln_edt'])} {ref[2]:7.0f}   {f(m['H_edt'])} {ref[3]:6.0f}   "
              f"{f(m['H_t20'])} {f(ref[4])}")
    print("\nCY5025 1/3 octaves, ours - 808 (dB):")
    print("window      " + " ".join(f"{fc / 1000:5.1f}" for fc in THIRDS))
    for (t0, t1), row in zip(THIRDS_REF, th):
        print(f"{int(t0 * 1000):4d}-{int(t1 * 1000):4d} ms " + " ".join(f"{v:5.1f}" for v in row))
    print(f"\nrms: H-L {np.sqrt(np.mean(np.square(err_hl))):.2f} dB, EDT {np.sqrt(np.mean(np.square(err_edt))):.3f} oct, "
          f"T20 {np.sqrt(np.mean(np.square(err_t20))) if err_t20 else 0:.3f} oct, thirds {np.sqrt(np.mean(th ** 2)):.2f} dB; "
          f"score {score:.3f}")


if __name__ == "__main__":
    main()
