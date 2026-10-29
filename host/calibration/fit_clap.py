#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Fits the clap (TR808_CP_MODEL 3, tr808_cp_cal in engine/tr808.c) to the
recordings: fine envelope of the teeth and main clap (2 ms, 0-120 ms), the
long tail (20 ms, 0-1.2 s, down to -70 dB) and the spectrum in five time
windows (0-30, 30-80, 80-200, 200-600, 600-1200 ms).
Usage: KIT_WAV=kit.wav KIT_SEG=kit_seg.npy python3 host/calibration/fit_clap.py [evaluations | eval]
Needs build/hit_render_cp3 (hit_render built with -DTR808_CP_MODEL=3)."""
import sys, os, subprocess, wave, numpy as np
from scipy.optimize import minimize
SR = 44100
KIT = os.environ['KIT_WAV']; SEG = os.environ['KIT_SEG']; R = os.environ.get('HIT_RENDER', 'build/hit_render_cp3')
REFS = (10, 12)
CAL = [1146, 1.499, 0.7455, 5.498, 11.36, 33.58, 0.2133, 31.8, 0.0231, 657.6, 1186, 1.5, 1.557]
FIT = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12]   # gain (11) only scales the level
EDGES = [300, 600, 900, 1200, 1600, 2200, 3000, 4500, 7000, 11000, 16000]
WINS = ((0, 30), (30, 80), (80, 200), (200, 600), (600, 1200))

def load_ref(i):
    w = wave.open(KIT); x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, w.getnchannels()).mean(1) / 32768
    seg = np.load(SEG); a = seg[i][0]; nxt = seg[i + 1][0] if i + 1 < len(seg) else len(x)
    s = x[a:nxt]; on = int(np.argmax(np.abs(s) > 0.01 * np.abs(s).max())); return s[on:on + int(1.3 * SR)]

def env(x, ms, dur, floor):
    n = int(SR * ms / 1000); m = int(dur / ms); x = np.concatenate([x, np.zeros(max(0, n * m - len(x)))])[:n * m]
    r = np.sqrt(np.mean(x.reshape(m, n) ** 2, axis=1)); return np.maximum(20 * np.log10(r / np.abs(x).max() + 1e-12), floor)

def bands(x, a, b):
    s = x[int(a * SR / 1000):int(b * SR / 1000)]
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), n=1 << 16)) ** 2; f = np.fft.rfftfreq(1 << 16, 1 / SR)
    v = np.array([X[(f >= EDGES[i]) & (f < EDGES[i + 1])].mean() for i in range(len(EDGES) - 1)])
    v = 10 * np.log10(v + 1e-30); return np.maximum(v - v.max(), -50)

def features(x, length_ms=1200):
    x = x / np.abs(x).max()
    wins = [(a, min(b, length_ms)) for a, b in WINS if a + 50 < length_ms]
    return env(x, 2, 120, -60), env(x, 20, length_ms, -70), [bands(x, a, b) for a, b in wins]

def ref_features(i):
    x = load_ref(i); return features(x, min(1200, 20 * int(len(x) / SR * 50)))

REF = [ref_features(i) for i in REFS]

def score(cal, detail=False):
    p = 'build/calibration/cp_fit.raw'
    subprocess.run([R, p, '5', '0.66', '1.3', 'cp=' + ','.join(f'{v:.6g}' for v in cal)], check=True)
    o = np.fromfile(p, dtype=np.float32).astype(float)
    on = int(np.argmax(np.abs(o) > 0.01 * np.abs(o).max())); o = o[on:]
    tot = 0; info = []
    for fr in REF:
        fo = features(o, len(fr[1]) * 20)
        a = np.sqrt(np.mean((fr[0] - fo[0]) ** 2)); b = np.sqrt(np.mean((fr[1] - fo[1]) ** 2))
        c = np.mean([np.sqrt(np.mean((x - y) ** 2)) for x, y in zip(fr[2], fo[2])])
        tot += (a + b + c) / 3; info.append((round(a, 2), round(b, 2), round(c, 2)))
    return (tot / len(REF), info) if detail else tot / len(REF)

if __name__ == '__main__':
    if sys.argv[1:] == ['eval']:
        print(score(CAL, True)); sys.exit()
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    z0 = np.log([CAL[i] for i in FIT]); best = [1e9, None]
    def f(z):
        c = list(CAL)
        for a, i in enumerate(FIT): c[i] = float(np.exp(z[a]))
        if c[5] < 2 * c[4] + 2 or c[5] > 60 or c[1] > 20: return 1e6
        s = score(c)
        if s < best[0]: best[0] = s; best[1] = c
        return s
    minimize(f, z0, method='Nelder-Mead', options={'maxfev': n, 'initial_simplex': np.vstack([z0] + [z0 + np.eye(len(z0))[i] * 0.35 for i in range(len(z0))])})
    print('before', score(CAL, True)); print('after', score(best[1], True))
    print('cal', ', '.join(f'{v:.4g}' for v in best[1]))
