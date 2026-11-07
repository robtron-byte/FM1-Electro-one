#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Low-end check of the bass sounds against the 808 kick (docs/adr/0027).
Usage: preset_check.py <dir with *.f32 from build/preset_probe> [area-name-filter ...]
Prints, per sound, how its energy splits over the sub region (below 40 Hz),
the kick's body (40-90 Hz; the 808 kick sits at 45-60 Hz), above it and the
top. The bass sounds play an octave down (16'): MIDI 36 gives about 33 Hz.
Played with `build/preset_probe <dir> 36 2.5` (docs/adr/0027)."""
import glob, os, sys
import numpy as np

SR = 44100


def loudest(x):
    """Highest rms over 0.25 s windows of the whole note (pads swell slowly)."""
    w = SR // 4
    return float(max(np.sqrt(np.mean(x[i:i + w] ** 2)) for i in range(0, len(x) - w, w // 2)))


def bands(x):
    """First half second (the body of a note; plucks are over soon after)."""
    whole = x
    x = x[: SR // 2]
    x = x - x.mean()
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    total = spec.sum() + 1e-12
    share = lambda lo, hi: spec[(f >= lo) & (f < hi)].sum() / total
    band = (f >= 20) & (f < 400)
    peak = float(f[band][np.argmax(spec[band])])   # the strongest partial, usually the fundamental
    return share(0, 40), share(40, 90), share(90, 300), share(300, 1e5), float(np.sqrt(np.mean(x ** 2))), peak, loudest(whole)


def main():
    rows = []
    for p in sorted(glob.glob(os.path.join(sys.argv[1], "*.f32"))):
        name = os.path.basename(p)[:-4]
        if len(sys.argv) > 2 and not any(k in name for k in sys.argv[2:]):
            continue
        rows.append((*bands(np.fromfile(p, dtype=np.float32)), name))
    print(f"{'':30s} {'<40 Hz':>7s} {'40-90':>7s} {'90-300':>7s} {'>300':>6s}  {'peak':>6s}  rms 0.5 s / loudest")
    for sub, kick, body, top, rms, peak, loud, name in sorted(rows, key=lambda r: r[5]):
        flag = "  <- on the kick" if kick > 0.4 else ""
        print(f"{name:30s} {sub*100:6.1f}% {kick*100:6.1f}% {body*100:6.1f}% {top*100:5.1f}% {peak:5.0f}Hz  {rms:.3f} / {loud:.3f}{flag}")


main()
