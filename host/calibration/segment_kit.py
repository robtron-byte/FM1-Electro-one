#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
import wave,sys,numpy as np
w=wave.open(sys.argv[1]); sr=w.getframerate()
x=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).reshape(-1,2)/32768.0
print('dur',len(x)/sr,'L/R corr',np.corrcoef(x[:,0],x[:,1])[0,1], 'peakL',np.abs(x[:,0]).max(),'peakR',np.abs(x[:,1]).max())
m=x.mean(1)
env=np.sqrt(np.convolve(m*m,np.ones(220)/220,'same'))
thr=10**(-50/20)
on=[];i=0;N=len(m)
while i<N:
    if env[i]>thr:
        j=i
        # end: env below thr for 50 ms
        k=i
        while k<N:
            if env[k]<thr:
                q=k
                while q<N and env[q]<thr and q-k<int(0.05*sr): q+=1
                if q-k>=int(0.05*sr) or q>=N: break
                k=q
            k+=1
        on.append((i,k)); i=k+1
    else: i+=1
print(len(on),'segments')
np.save(sys.argv[2],np.array(on))
for n,(a,b) in enumerate(on):
    s=m[a:b]; S=np.abs(np.fft.rfft(s[:min(len(s),8192)]*np.hanning(min(len(s),8192)),n=1<<15)); f=np.fft.rfftfreq(1<<15,1/sr)
    cen=(S*f).sum()/S.sum(); pk=f[np.argmax(S*(f>25))]
    print(f"{n:3d} t={a/sr:7.3f}s len={1000*(b-a)/sr:6.0f}ms peak={20*np.log10(np.abs(s).max()):6.1f}dB f0={pk:7.1f}Hz cent={cen:7.0f}Hz")
