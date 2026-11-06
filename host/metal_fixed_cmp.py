#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Compares two WAVs of host/metal_fixed_check (float 808 against the fixed-point metal voices, docs/adr/0022).
  metal_fixed_cmp.py <ref.wav> <new.wav> <from:to s> ...   prints level, difference, 1/3-octave and envelope error;
  exit 1 if a difference is above -40 dB."""
import wave, numpy as np, sys
def load(p):
    w=wave.open(p); return np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).reshape(-1,2).astype(float)/32768
a=load(sys.argv[1]); b=load(sys.argv[2]); sr=44100
segs=[tuple(map(float,s.split(':'))) for s in sys.argv[3:]]
def thirds(x):
    X=np.abs(np.fft.rfft(x*np.hanning(len(x))))**2; f=np.fft.rfftfreq(len(x),1/sr)
    cs=[125*2**(k/3) for k in range(0,23)]
    return np.array([10*np.log10(X[(f>=c*2**-(1/6))&(f<c*2**(1/6))].sum()+1e-15) for c in cs])
worst=-200
for s0,s1 in segs:
    x=a[int(s0*sr):int(s1*sr)].mean(1); y=b[int(s0*sr):int(s1*sr)].mean(1)
    rx=np.sqrt((x**2).mean()); ry=np.sqrt((y**2).mean()); e=np.sqrt(((x-y)**2).mean())
    worst=max(worst,20*np.log10(e/rx+1e-12))
    tx,ty=thirds(x),thirds(y); m=tx>tx.max()-60
    print(f"{s0:5.1f}-{s1:5.1f}s  level ref {20*np.log10(rx+1e-12):6.1f} new {20*np.log10(ry+1e-12):6.1f} dB  diff {20*np.log10(e/rx+1e-12):6.1f} dB rel  1/3-oct rms {np.sqrt(((tx-ty)[m]**2).mean()):.2f} dB max {np.abs(tx-ty)[m].max():.2f}")
    # envelope 5 ms
    n=int(0.005*sr); ex=np.sqrt(np.convolve(x**2,np.ones(n)/n,'same'))[::n]; ey=np.sqrt(np.convolve(y**2,np.ones(n)/n,'same'))[::n]
    mm=ex>ex.max()*1e-3
    print(f"            envelope rms {np.sqrt(((20*np.log10(ex[mm]+1e-9)-20*np.log10(ey[mm]+1e-9))**2).mean()):.2f} dB")
sys.exit(1 if worst > -40 else 0)
