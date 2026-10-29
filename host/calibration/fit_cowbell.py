#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Fits the cowbell (TR808_CB_MODEL 1, tr808_cb_cal in engine/tr808.c) to
the cowbell of the recordings (808 Basics Kit A: hit 20; hit 9, used for
model 0, is not a cowbell). Rendered with TUNE 69: that unit runs ~6 %
sharp (573 / 859 Hz instead of 540 / 800 Hz). Compares the envelope
(5 ms, 0-400 ms; 20 ms, 0-1 s), the spectrum in three time windows and
the levels of the two tones and their harmonics.
Usage: KIT_WAV=kit.wav KIT_SEG=kit_seg.npy python3 host/calibration/fit_cowbell.py [evaluations | eval]
Needs build/hit_render_cb1 (hit_render built with -DTR808_CB_MODEL=1)."""
import sys, os, subprocess, wave, numpy as np
from scipy.optimize import minimize
SR = 44100
KIT = os.environ['KIT_WAV']; SEG = os.environ['KIT_SEG']; R = os.environ.get('HIT_RENDER', 'build/hit_render_cb1')
REF_HIT = 20
CAL = [1033, 7.1, 0.05648, 901.6, 0.8923, 21.36, 139.4, 0.02784, 1]
TUNE = 1.0555                                  # spread at TUNE 69
FIT = [0, 1, 2, 3, 4, 5, 6, 7]
EDGES = [250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000, 16000]
WINS = ((0, 20), (20, 150), (150, 500))

def load_ref(i):
    w = wave.open(KIT); x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, w.getnchannels()).mean(1) / 32768
    seg = np.load(SEG); a = seg[i][0]; nxt = seg[i + 1][0]
    s = x[a:nxt]; on = int(np.argmax(np.abs(s) > 0.01 * np.abs(s).max())); return s[on:on + SR]

def env(x, ms, dur, floor):
    n = int(SR * ms / 1000); m = int(dur / ms); x = np.concatenate([x, np.zeros(max(0, n * m - len(x)))])[:n * m]
    r = np.sqrt(np.mean(x.reshape(m, n) ** 2, axis=1)); return np.maximum(20 * np.log10(r / np.abs(x).max() + 1e-12), floor)

def bands(x, a, b):
    s = x[int(a * SR / 1000):int(b * SR / 1000)]
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), n=1 << 16)) ** 2; f = np.fft.rfftfreq(1 << 16, 1 / SR)
    v = np.array([X[(f >= EDGES[i]) & (f < EDGES[i + 1])].mean() for i in range(len(EDGES) - 1)])
    v = 10 * np.log10(v + 1e-30); return np.maximum(v - v.max(), -60)

def partials(x, lo, hi):
    """Levels of the two tones and their harmonics (20-200 ms), dB re the strongest."""
    s = x[int(0.02 * SR):int(0.2 * SR)]; S = np.abs(np.fft.rfft(s * np.hanning(len(s)), n=1 << 17)); f = np.fft.rfftfreq(1 << 17, 1 / SR)
    S = 20 * np.log10(S / S.max() + 1e-12)
    fs = [lo, hi, 2 * lo, 3 * lo, 4 * lo, 3 * hi, 6 * lo, 4 * hi, 5 * hi, 6 * hi]
    return np.maximum(np.array([S[(f > q * 0.97) & (f < q * 1.03)].max() for q in fs]), -50)

def features(x, lo, hi):
    x = x / np.abs(x).max()
    return env(x, 5, 400, -60), env(x, 20, 1000, -70), [bands(x, a, b) for a, b in WINS], partials(x, lo, hi)

REF = features(load_ref(REF_HIT), 573, 859)

def render(cal, path='build/calibration/cb_fit.raw', tune=69):
    subprocess.run([R, path, '6', '0.66', '1.0', f'1={tune}', 'cb=' + ','.join(f'{v:.6g}' for v in cal)], check=True)
    o = np.fromfile(path, dtype=np.float32).astype(float); on = int(np.argmax(np.abs(o) > 0.01 * np.abs(o).max())); return o[on:]

def score(cal, detail=False):
    fo = features(render(cal), 540 * TUNE, 800 * TUNE)
    a = np.sqrt(np.mean((REF[0] - fo[0]) ** 2)); b = np.sqrt(np.mean((REF[1] - fo[1]) ** 2))
    c = np.mean([np.sqrt(np.mean((x - y) ** 2)) for x, y in zip(REF[2], fo[2])])
    d = np.sqrt(np.mean((REF[3] - fo[3]) ** 2))
    tot = (a + b + c + d) / 4
    return (round(tot, 2), (round(a, 2), round(b, 2), round(c, 2), round(d, 2))) if detail else tot

if __name__ == '__main__':
    if sys.argv[1:] == ['eval']:
        print(score(CAL, True)); sys.exit()
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    z0 = np.log([CAL[i] for i in FIT]); best = [1e9, None]
    def f(z):
        c = list(CAL)
        for a, i in enumerate(FIT): c[i] = float(np.exp(z[a]))
        if c[4] >= 1 or c[1] > 40 or c[0] > 4000 or c[7] > 3: return 1e6
        s = score(c)
        if s < best[0]: best[0] = s; best[1] = c
        return s
    minimize(f, z0, method='Nelder-Mead', options={'maxfev': n, 'initial_simplex': np.vstack([z0] + [z0 + np.eye(len(z0))[i] * 0.4 for i in range(len(z0))])})
    print('before', score(CAL, True)); print('after', score(best[1], True))
    print('cal', ', '.join(f'{v:.4g}' for v in best[1]))
