#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
import sys, os, subprocess, json, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from features import *
from scipy.optimize import minimize
S=os.environ.get('CAL_WORK', 'build/calibration')
KIT=os.environ['KIT_WAV']; SEG=os.environ['KIT_SEG']; R='build/hit_render'
IDX=[0,1,3,4,7]
refs=[load_kit(KIT,SEG,i) for i in IDX]
rf=[]
for r in refs:
    f=f0_track(r); rf.append((env_db(r),f,harmonics(r,np.nanmean(f[1:3]))))
CAL0=[44.0,0.17,0.04,232,6.7,4.4,1.03,0.57,0.06,-0.04,0.0,2.95,5.9,40.0,3.0]
# x: cal(13) + tone + velocity + decay knob per ref (5)
x0=np.array(CAL0+[60,0.9]+[96,61,40,21,28],float)
lo=np.array([35,0,0,60,1,1,0.3,0,-0.5,-0.5,0,0.5,2,5,0.3,0,0.3,0,0,0,0,0]); hi=np.array([65,0.5,0.3,400,20,15,5,3,0.5,0.5,0.5,8,8,200,20,127,1,127,127,127,127,127])
best=[1e9,None]; D=S+'/fit/bd2'; os.makedirs(D,exist_ok=True)
def render(x,i):
    cal=",".join(f"{v:.6g}" for v in x[:15]); p=f"{D}/o{i}.raw"
    subprocess.run([R,p,'0',f"{x[16]:.4f}",'1.6',f"2={int(round(x[17+i]))}",f"3={int(round(x[15]))}",'bd='+cal],check=True)
    return load_raw(p)
def score(x, detail=False):
    tot=0; parts=[]
    for i,r in enumerate(refs):
        o=render(x,i); e0,f0r,h0=rf[i]
        e=env_db(o); fo=f0_track(o); ho=harmonics(o,np.nanmean(fo[1:3]))
        ee=np.sqrt(np.mean((e-e0)**2))/3.0
        fe=np.nanmean(np.abs(np.log2(fo/f0r)))*12/0.5 if np.all(np.isfinite(fo)) else 10
        he=np.sqrt(np.mean((np.maximum(ho,-70)-np.maximum(h0,-70))**2))/4.0
        ae=attack_err(r,o)/0.05
        parts.append((round(ee,2),round(fe,2),round(he,2),round(ae,2))); tot+=ee+fe+he+ae
    return (tot,parts) if detail else tot
def f(z):
    x=np.clip(z,lo,hi); s=score(x)+np.sum(np.maximum(lo-z,0)+np.maximum(z-hi,0))*5
    if s<best[0]:
        best[0]=s; best[1]=x.tolist(); json.dump(best,open(D+'/best.json','w')); print(round(s,3),flush=True)
    return s
if 'eval' in sys.argv:
    x=np.array(json.load(open(D+'/best.json'))[1]) if os.path.exists(D+'/best.json') else x0
    print(score(x,True)); print([round(v,4) for v in x]); sys.exit()
z=x0.copy()
step=np.array([3,0.05,0.02,30,2,1,0.3,0.2,0.05,0.05,0.02,0.5,0.5,15,1.0,20,0.15,15,15,10,8,8])
for r in range(3):
    sim=np.vstack([z]+[z+step*np.eye(len(z))[i] for i in range(len(z))])
    minimize(f,z,method='Nelder-Mead',options={'maxfev':int(sys.argv[1]),'initial_simplex':sim,'xatol':1e-4,'fatol':1e-4})
    z=np.array(best[1]); step=step*0.5
print('BEST',best)
