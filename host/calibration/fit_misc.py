#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
import sys, os, subprocess, json, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from features import *
from scipy.optimize import minimize
S=os.environ.get('CAL_WORK', 'build/calibration')
KIT=os.environ['KIT_WAV']; SEG=os.environ['KIT_SEG']; R='build/hit_render'
MX=[1100.0,2.8,8.0,40.0,0.5,43.0,0.38,1.0,50.0,0.0]
D=S+'/fit/mx'; os.makedirs(D,exist_ok=True)
FC=[250*2**(k/3) for k in range(0,27)]
def thirds(x):
    x=x[:int(0.4*SR)]; L=len(x); X=np.abs(np.fft.rfft(x,n=1<<16))**2; f=np.fft.rfftfreq(1<<16,1/SR)
    v=np.array([X[(f>=c/2**(1/6))&(f<c*2**(1/6))].sum()+1e-20 for c in FC if c*2**(1/6)<20000]); v=10*np.log10(v); return np.maximum(v-v.max(),-45)
# instrument: part, refs, mx indices, extra per-ref knob (param index) or None, seconds
INST={'cb':(6,[9],[0,1,2,3,4],None,0.5),'cp':(5,[10,12],[5,6],None,0.6),'rs':(4,[21],[7],None,0.1),
      'ch':(9,[17,22],[8],None,0.3),'oh':(8,[8,18],[9],1,1.0)}
def run(name, mx, knobs=None, detail=False):
    part,refs,idx,kp,sec=INST[name]; tot=0; info=[]
    for j,ri in enumerate(refs):
        r=load_kit(KIT,SEG,ri); p=f"{D}/{name}{j}.raw"
        args=[R,p,str(part),'0.66',str(sec),'mx='+",".join(f"{v:.6g}" for v in mx)]
        if kp is not None: args.append(f"{kp}={int(round(knobs[j]))}")
        subprocess.run(args,check=True); o=load_raw(p)
        n=min(len(r),len(o),int(sec*SR))
        e1=env_db(r,2,sec*1000*0.8); e2=env_db(o,2,sec*1000*0.8)
        ee=np.sqrt(np.mean((e1-e2)**2)); te=np.sqrt(np.mean((thirds(r)-thirds(o))**2))
        info.append((round(ee,2),round(te,2))); tot+=ee/3+te/3
    return (tot,info) if detail else tot
def fit(name, n=150):
    part,refs,idx,kp,sec=INST[name]; mx=list(MX)
    z0=[mx[i] for i in idx]+([60.0]*len(refs) if kp is not None else [])
    best=[1e9,None]
    def f(z):
        m=list(mx)
        for a,i in enumerate(idx): m[i]=z[a]
        if any(v<=0 for k,v in enumerate(z) if not (idx[k]==9 if k<len(idx) else False)): return 1e6
        s=run(name,m,z[len(idx):] if kp is not None else None)
        if s<best[0]: best[0]=s; best[1]=list(z)
        return s
    minimize(f,np.array(z0),method='Nelder-Mead',options={'maxfev':n,'initial_simplex':np.vstack([z0]+[np.array(z0)+np.eye(len(z0))[i]*(abs(z0[i])*0.3+0.3) for i in range(len(z0))])})
    m=list(mx)
    for a,i in enumerate(idx): m[i]=best[1][a]
    print(name,'before',run(name,mx,[60.0]*len(refs) if kp is not None else None,True),'after',run(name,m,best[1][len(idx):] if kp is not None else None,True),'values',[round(v,3) for v in best[1]],flush=True)
    return {i:best[1][a] for a,i in enumerate(idx)}
res={}
for name in sys.argv[1:]: res.update(fit(name))
json.dump(res,open(D+'/best_'+'_'.join(sys.argv[1:])+'.json','w'))
