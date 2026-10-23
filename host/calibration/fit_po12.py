#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
import sys, os, subprocess, json, wave, numpy as np
from scipy.optimize import minimize
SR=44100
S=os.environ.get('CAL_WORK', 'build/calibration')
REC=os.environ['PO_WAV']; SEG=os.environ['PO_SEG']; R='build/po_render'
D=S+'/fit/po'; os.makedirs(D,exist_ok=True)
NAMES=['BD','SD','CH','OH','SynSD','Sticks','CY','Noise','Clap','Click','LoTom','HiTom','Cowbell','Blip','Tone','BassTone']
P=['wave','fs','fe','pms','olev','oatt','ohold','odec','nlev','nmode','nhz','nq','natt','ndec','bursts','bms','fmr','fmi','drive','gate','crush','pw','outlp','gain','ofilt']
DISCRETE={'wave','nmode','bursts'}
w=wave.open(REC); X=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16)/32768.0; SEGS=np.load(SEG)
def ref(i):
    a,b=SEGS[i]; s=X[a:b]; on=int(np.argmax(np.abs(s)>0.02*np.abs(s).max())); return s[on:]
FC=[63*2**(k/3) for k in range(0,28)]
def thirds(x):
    if len(x)<64: x=np.pad(x,(0,64-len(x)))
    Xf=np.abs(np.fft.rfft(x*np.hanning(len(x)),n=1<<15))**2; f=np.fft.rfftfreq(1<<15,1/SR)
    v=np.array([Xf[(f>=c/2**(1/6))&(f<c*2**(1/6))].sum()+1e-20 for c in FC if c*2**(1/6)<21000]); v=10*np.log10(v); return np.maximum(v-v.max(),-50)
def env(x,n):
    x=np.pad(x,(0,max(0,n-len(x))))[:n]; m=n//44; r=np.sqrt(np.mean(x[:m*44].reshape(m,44)**2,1)); return np.maximum(20*np.log10(r+1e-9),-60)
def peaks(x, wins):
    out=[]
    for a,b in wins:
        s=x[int(a*44.1):int(b*44.1)]
        if len(s)<64: out.append(0); continue
        F=np.abs(np.fft.rfft(s*np.hanning(len(s)),n=1<<15)); f=np.fft.rfftfreq(1<<15,1/SR); m=f>30
        out.append(np.log2(f[m][np.argmax(F[m])]))
    return np.array(out)
def feats(x, L, tonal):
    x=x/(np.abs(x).max()+1e-12); e=env(x,L+int(0.02*SR))
    early=thirds(x[:min(len(x),int(0.03*SR))]); late=thirds(x[int(0.03*SR):L]) if L>int(0.05*SR) else early
    pk=peaks(x,[(0,15),(15,50),(50,150)]) if tonal else None
    return e,early,late,pk
TONAL={0,4,10,11,12,14}
def score(i, params, detail=False):
    r=ref(i); L=len(r); p=f"{D}/o{i}.raw"
    subprocess.run([R,p,str(i),str(L/SR+0.05),'0.5','0.5',",".join(f"{v:.6g}" for v in params)],check=True)
    o=np.fromfile(p,dtype=np.float32).astype(float)
    if np.abs(o).max()<1e-6: return 1e3
    on=int(np.argmax(np.abs(o)>0.02*np.abs(o).max())); o=o[on:]
    fr=feats(r,L,i in TONAL); fo=feats(o,L,i in TONAL)
    ee=np.sqrt(np.mean((fr[0]-fo[0])**2)); e1=np.sqrt(np.mean((fr[1]-fo[1])**2)); e2=np.sqrt(np.mean((fr[2]-fo[2])**2))
    pe=np.sqrt(np.mean(((fr[3]-fo[3])*12)**2)) if fr[3] is not None else 0
    s=ee/3+e1/4+e2/4+pe/2
    return (s,round(ee,1),round(e1,1),round(e2,1),round(pe,1)) if detail else s
def fit(i, p0, n):
    ZERO_OFF={'crush','gate','nlev','olev','ohold','bms','fmr'}
    free=[k for k,name in enumerate(P) if name not in DISCRETE and not (name in ZERO_OFF and p0[k]==0) and not (name=='fmr' and p0[P.index('fmi')]==0)]
    base=np.array(p0,float)
    def unpack(z):
        q=base.copy()
        for j,k in enumerate(free):
            v=z[j]
            q[k]= v if P[k] in ('drive','fmi','ofilt') else np.exp(v)
        return q
    z0=np.array([base[k] if P[k] in ('drive','fmi','ofilt') else np.log(max(base[k],1e-3)) for k in free])
    best=[score(i,base),base]
    def f(z):
        q=unpack(z)
        if q[P.index('pw')]>0.95 or q[P.index('drive')]<0 or q[P.index('fmi')]<0 or not 0<=q[P.index('ofilt')]<=1: return 1e3
        s=score(i,q)
        if s<best[0]: best[0]=s; best[1]=q
        return s
    sim=np.vstack([z0]+[z0+np.eye(len(z0))[j]*(0.4 if P[free[j]] not in ('drive','fmi','ofilt') else 0.3) for j in range(len(z0))])
    minimize(f,z0,method='Nelder-Mead',options={'maxfev':n,'initial_simplex':sim})
    return best
if __name__=='__main__':
    init={k:v for k,v in json.load(open(sys.argv[1])).items()}
    n=int(sys.argv[2]); which=[int(a) for a in sys.argv[3:]] or range(16)
    res={}
    for i in which:
        p0=init[str(i)]
        b0=score(i,p0,True); best=fit(i,p0,n); b1=score(i,best[1],True)
        res[i]=list(map(float,best[1])); print(i,NAMES[i],'before',b0,'after',b1,flush=True)
        json.dump(res,open(D+'/fit_%s.json'%'_'.join(map(str,which)),'w'))
