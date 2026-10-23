#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
import numpy as np, wave
from scipy.signal import butter, sosfiltfilt
SR=44100
def load_kit(path, segfile, i):
    w=wave.open(path); x=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).reshape(-1,w.getnchannels()).mean(1)/32768.0
    a,b=np.load(segfile)[i]; s=x[a:b]
    on=int(np.argmax(np.abs(s)>0.01*np.abs(s).max())); return s[max(0,on-5):]
def load_raw(p):
    s=np.fromfile(p,dtype=np.float32).astype(float); on=int(np.argmax(np.abs(s)>0.01*np.abs(s).max())); return s[max(0,on-5):]
def lp(x,f): return sosfiltfilt(butter(4,f,fs=SR,output='sos'),np.concatenate([np.zeros(4096),x]))[4096:]
def hp(x,f): return sosfiltfilt(butter(4,f,btype='high',fs=SR,output='sos'),np.concatenate([np.zeros(4096),x]))[4096:]
def env_db(x, step_ms=5, dur_ms=1500):
    x=x/np.abs(x).max(); n=int(SR*step_ms/1000); m=int(dur_ms/step_ms)
    x=np.concatenate([x,np.zeros(max(0,n*m-len(x)))])[:n*m]
    r=np.sqrt(np.mean(x.reshape(m,n)**2,axis=1)); return np.maximum(20*np.log10(r+1e-9),-60)
def f0_track(x, wins=((10,40),(40,100),(100,250),(250,500))):
    y=lp(x,150); out=[]
    for a,b in wins:
        s=y[int(a*44.1):int(b*44.1)]
        if len(s)<100 or np.abs(s).max()<1e-4: out.append(np.nan); continue
        z=np.nonzero((s[:-1]<0)&(s[1:]>=0))[0]
        if len(z)<2: out.append(np.nan); continue
        fr=z+(-s[z]/(s[z+1]-s[z])); out.append(SR*(len(fr)-1)/(fr[-1]-fr[0]))
    return np.array(out)
def harmonics(x, f0, a_ms=20, b_ms=300, n=4):
    s=x[int(a_ms*44.1):int(b_ms*44.1)]; L=len(s)
    S=np.abs(np.fft.rfft(s*np.hanning(L),n=1<<17)); f=np.fft.rfftfreq(1<<17,1/SR)
    def amp(fc): m=(f>fc*0.85)&(f<fc*1.15); return S[m].max() if m.any() else 1e-9
    a1=amp(f0); return np.array([20*np.log10(amp(f0*h)/a1+1e-9) for h in range(2,n+1)])
def attack_err(a, b, ms=25):
    n=int(ms*44.1); a=a[:n]/np.abs(a[:n]).max(); best=9
    for sh in range(-40,41,2):
        bb=np.roll(np.concatenate([b,np.zeros(n)])[:n+80],sh)[:n]; bb=bb/(np.abs(bb).max()+1e-9)
        for sg in (1,-1): best=min(best,np.mean((a-sg*bb)**2))
    return best
