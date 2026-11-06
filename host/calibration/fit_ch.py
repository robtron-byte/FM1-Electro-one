#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Fits the closed hat of hats model 2 (tr808_ch2_cal in engine/tr808.c, A/B
switch TR808_CH_FIT, docs/adr/0014) to single closed hats of a real TR-808
(the user's recordings Closed_HiHat0001..0004, 2026-11-06; WAV, 44.1 kHz,
mono): 1/3-octave profile 1-20 kHz (0-80 ms), the partials 6-10 kHz in
200 Hz bands and the envelope (2 ms RMS, 0-80 ms, peak-normalised).
The gain (last value) keeps the closed hat as loud as before (RMS 0-50 ms).
Usage: python3 host/calibration/fit_ch.py <ref.wav> ... [--eval v,v,v,v,v,v]
Needs build/hit_render (make build/hit_render)."""
import sys, os, subprocess, tempfile, wave, numpy as np
from scipy.optimize import minimize
SR = 44100
R = os.environ.get('HIT_RENDER', 'build/hit_render')
START = [5200.0, 1.6, 2700.0, 0.75, 10500.0, 42.5, 1.0]
LO = [1500, 0.4, 800, 0.4, 4000, 8, 0.01]
HI = [16000, 6.0, 14000, 4.0, 19000, 120, 100]
DUR = 0.08

def load(p):
    w = wave.open(p)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, w.getnchannels()).mean(1) / 32768
    on = int(np.argmax(np.abs(x) > 0.05 * np.abs(x).max()))
    x = x[max(0, on - 10):]
    return np.concatenate([x, np.zeros(max(0, int(DUR * SR) - len(x)))])[:int(DUR * SR)]

CS = [1000 * 2 ** (k / 3) for k in range(0, 14)]   # 1 .. 8 kHz .. 20 kHz
def thirds(x):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)), 1 << 14)) ** 2; f = np.fft.rfftfreq(1 << 14, 1 / SR)
    v = np.array([X[(f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))].sum() for c in CS])
    return np.maximum(10 * np.log10(v / v.max() + 1e-12), -60)
FINE = np.arange(6000, 10001, 200)
def fine(x):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)), 1 << 16)) ** 2; f = np.fft.rfftfreq(1 << 16, 1 / SR)
    v = np.array([X[(f >= FINE[i]) & (f < FINE[i + 1])].mean() for i in range(len(FINE) - 1)])
    return np.maximum(10 * np.log10(v / v.max() + 1e-12), -30)
def env(x):
    n = int(0.002 * SR); m = len(x) // n
    e = np.sqrt(np.mean(x[:n * m].reshape(m, n) ** 2, axis=1))
    return np.maximum(20 * np.log10(e / e.max() + 1e-12), -50)

def render(cal):
    with tempfile.NamedTemporaryFile(suffix='.raw') as f:
        subprocess.run([R, f.name, '9', '0.66', str(DUR + 0.02), 'c2=' + ','.join('%.6g' % v for v in cal)], check=True)
        x = np.fromfile(f.name, dtype=np.float32).astype(float)
    on = int(np.argmax(np.abs(x) > 0.05 * np.abs(x).max()))
    x = x[max(0, on - 10):]
    return np.concatenate([x, np.zeros(max(0, int(DUR * SR) - len(x)))])[:int(DUR * SR)]

def features(x): return thirds(x), fine(x), env(x)

def error(feat, refs):
    t, fi, e = feat
    rt = np.mean([r[0] for r in refs], 0); rf = np.mean([r[1] for r in refs], 0); re_ = np.mean([r[2] for r in refs], 0)
    et = np.sqrt(np.mean((t - rt) ** 2)); ef = np.sqrt(np.mean((fi - rf) ** 2)); ee = np.sqrt(np.mean((e - re_) ** 2))
    return et, ef, ee

def main():
    argv = sys.argv[1:]
    if '--eval' in argv: i = argv.index('--eval'); argv = argv[:i] + argv[i + 2:]
    args = [a for a in argv if not a.startswith('--')]
    refs = [features(load(p)) for p in args]
    spread = np.mean([error(r, refs) for r in refs], 0)
    print('real hits against their mean: spectrum %.2f, partials %.2f, envelope %.2f dB' % tuple(spread))
    if '--eval' in sys.argv:
        cal = [float(v) for v in sys.argv[sys.argv.index('--eval') + 1].split(',')]
        print('eval %s: spectrum %.2f, partials %.2f, envelope %.2f dB' % ((cal,) + error(features(render(cal)), refs)))
        return
    print('before: spectrum %.2f, partials %.2f, envelope %.2f dB' % error(features(render(START)), refs))
    # log-scaled parameters 0..5 (Hz, Q, ms), the gain fixed during the fit
    def unpack(z): return [float(np.clip(np.exp(z[i]), LO[i], HI[i])) for i in range(6)] + [1.0]
    def cost(z):
        et, ef, ee = error(features(render(unpack(z))), refs)
        return et + 0.5 * ef + ee
    z0 = np.log(START[:6]); best = None
    for seed in range(3):   # a few starts: the high-passes trade against each other
        zs = z0 + (0 if seed == 0 else np.random.default_rng(seed).normal(0, 0.3, 6))
        r = minimize(cost, zs, method='Nelder-Mead', options={'maxfev': 400, 'xatol': 1e-3, 'fatol': 1e-3})
        if best is None or r.fun < best.fun: best = r
    cal = unpack(best.x)
    # gain: as loud as before over 50 ms
    a = render(START)[:int(0.05 * SR)]; b = render(cal)[:int(0.05 * SR)]
    cal[6] = float(np.sqrt(np.mean(a ** 2) / np.mean(b ** 2)))
    print('after: spectrum %.2f, partials %.2f, envelope %.2f dB' % error(features(render(cal)), refs))
    print('tr808_ch2_cal = {' + ', '.join('%.4gf' % v for v in cal) + '}')

if __name__ == '__main__':
    main()
