#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Fits the hats (TR808_HAT_MODEL 1, tr808_hh_cal in engine/tr808.c) to the
hats of the recordings (808 Basics Kit A: closed hat hits 17 / 22, open hat
8 / 18): spectral profile 3-17 kHz in 500 Hz bands (CH 0-60 ms, OH 0-250 ms)
the partials 6-10 kHz in 200 Hz bands (where the tone sits) and the
closed hat's envelope (5 ms, 0-100 ms).
Usage: KIT_WAV=kit.wav KIT_SEG=kit_seg.npy python3 host/calibration/fit_hats.py [evaluations | eval]
Needs build/hit_render_hh1 (hit_render built with -DTR808_HAT_MODEL=1)."""
import sys, os, subprocess, wave, numpy as np
from scipy.optimize import minimize
SR = 44100
KIT = os.environ['KIT_WAV']; SEG = os.environ['KIT_SEG']; R = os.environ.get('HIT_RENDER', 'build/hit_render_hh1')
CAL = [6373, 3.169, 0.3871, 10910, 1.244, 9418, 0.9215]
if os.environ.get('HH_MODEL') == '2': CAL = [2.0, 1.0]
FIT = [int(i) for i in os.environ.get("HH_FIT", "0,1" if os.environ.get('HH_MODEL') == '2' else "0,1,2,3,4,5,6").split(",")]
EDGES = np.arange(3000, 17001, 500)

def load_ref(i, d):
    w = wave.open(KIT); x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, w.getnchannels()).mean(1) / 32768
    seg = np.load(SEG); a = seg[i][0]; nxt = seg[i + 1][0]
    s = x[a:nxt]; on = int(np.argmax(np.abs(s) > 0.01 * np.abs(s).max())); return s[on:on + int(d * SR)]

def profile(s):
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), n=1 << 17)) ** 2; f = np.fft.rfftfreq(1 << 17, 1 / SR)
    v = np.array([X[(f >= EDGES[i]) & (f < EDGES[i + 1])].mean() for i in range(len(EDGES) - 1)]); return 10 * np.log10(v / v.max() + 1e-12)

FINE = np.arange(6000, 10001, 200)
def fine(s):
    """6-10 kHz in 200 Hz bands: which partials of the metal bank stand out
    (where the hats' tone sits). Same partial positions as the original:
    its oscillators run at the nominal frequencies (cymbal, hit 23)."""
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), n=1 << 17)) ** 2; f = np.fft.rfftfreq(1 << 17, 1 / SR)
    v = np.array([X[(f >= FINE[i]) & (f < FINE[i + 1])].mean() for i in range(len(FINE) - 1)]); return np.maximum(10 * np.log10(v / v.max() + 1e-12), -30)

def env(x, ms, dur):
    x = x / np.abs(x).max(); n = int(SR * ms / 1000); m = int(dur / ms); x = np.concatenate([x, np.zeros(max(0, n * m - len(x)))])[:n * m]
    return np.maximum(20 * np.log10(np.sqrt(np.mean(x.reshape(m, n) ** 2, axis=1)) + 1e-12), -60)

REF_CH = [(profile(load_ref(i, 0.06)), env(load_ref(i, 0.1), 5, 100), fine(load_ref(i, 0.06))) for i in (17, 22)]
REF_OH = [(profile(load_ref(i, 0.25)), fine(load_ref(i, 0.25))) for i in (8, 18)]

MODEL2 = os.environ.get('HH_MODEL') == '2'   # fit tr808_hh2_cal (envelope length, gain) instead
def render(part, cal, d):
    p = f'build/calibration/hh_fit{part}.raw'
    key = 'h2=' if MODEL2 else 'hh='
    subprocess.run([R, p, str(part), '0.66', str(d), key + ','.join(f'{v:.6g}' for v in cal)], check=True)
    o = np.fromfile(p, dtype=np.float32).astype(float); on = int(np.argmax(np.abs(o) > 0.01 * np.abs(o).max())); return o[on:]

def score(cal, detail=False):
    ch = render(9, cal, 0.3); oh = render(8, cal, 0.6)
    pc, ec, fc = profile(ch[:int(0.06 * SR)]), env(ch[:int(0.1 * SR)], 5, 100), fine(ch[:int(0.06 * SR)])
    po, fo = profile(oh[:int(0.25 * SR)]), fine(oh[:int(0.25 * SR)])
    a = np.mean([np.sqrt(np.mean((r[0] - pc) ** 2)) for r in REF_CH])
    b = np.mean([np.sqrt(np.mean((r[1] - ec) ** 2)) for r in REF_CH])
    c = np.mean([np.sqrt(np.mean((r[0] - po) ** 2)) for r in REF_OH])
    d = np.mean([np.sqrt(np.mean((r[2] - fc) ** 2)) for r in REF_CH] + [np.sqrt(np.mean((r[1] - fo) ** 2)) for r in REF_OH])
    tot = (2 * a + b + 2 * c + 2 * d) / 7
    return (round(tot, 2), (round(a, 2), round(b, 2), round(c, 2), round(d, 2))) if detail else tot

if __name__ == '__main__':
    if sys.argv[1:] == ['eval']:
        print(score(CAL, True)); sys.exit()
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    z0 = np.log([CAL[i] for i in FIT]); best = [1e9, None]
    def f(z):
        c = list(CAL)
        for a, i in enumerate(FIT): c[i] = float(np.exp(z[a]))
        if not MODEL2 and (max(c[0], c[3], c[5]) > 19000 or c[2] > 3 or min(c[1], c[4], c[6]) < 0.3): return 1e6
        s = score(c)
        if s < best[0]: best[0] = s; best[1] = c
        return s
    minimize(f, z0, method='Nelder-Mead', options={'maxfev': n, 'initial_simplex': np.vstack([z0] + [z0 + np.eye(len(z0))[i] * 0.3 for i in range(len(z0))])})
    print('before', score(CAL, True)); print('after', score(best[1], True))
    print('cal', ', '.join(f'{v:.4g}' for v in best[1]))
