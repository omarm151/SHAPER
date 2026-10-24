"""Writes reference cases from the Python port of the M4L gen~ code (DSP v0.18.3)."""
import sys, json, numpy as np
sys.path.insert(0,'.')
from port import Gen, build_table
out_dir=sys.argv[1]
sr=44100
rng=np.random.default_rng(5)
def mirror(pos):
    p=[[0,0,0]]
    for q in pos:
        if q[0]==0: p[0][2]=q[2]
        if q[0]>0: p.append(list(q))
    neg=[[-p[j][0],-p[j][1],-p[j-1][2]] for j in range(len(p)-1,0,-1)]
    return neg+p
SOFT=[[-1,-1,1.6],[0,0,-1.6],[1,1,0]]
STEP=mirror([[0,0,0],[1e-12,1,0],[1,1,0]])
ASYM=[[-1,-0.9,0],[-0.4,-0.2,1.1],[0.1,0.55,0],[0.6,0.7,-1.3],[1,0.95,0]]
LIN=[[-1,-1,0],[0,0,0],[1,1,0]]
N=6000
t=np.arange(N)/sr
noise=rng.standard_normal((N,2))*0.3
tone=np.stack([0.5*np.exp(-6*t)*np.sin(2*np.pi*180*t)]*2,1)
hot=np.stack([2.0*np.sin(2*np.pi*1000*t), 1.5*np.sin(2*np.pi*313*t)],1)
cases=[
 dict(name='soft_2x_pre12', curve=SOFT, sym=True, x=noise, pre_db=12, mix=100, post_db=0, dc=0, mode=1, bias=0),
 dict(name='step_1x_preneg24', curve=STEP, sym=True, x=tone, pre_db=-24, mix=100, post_db=0, dc=0, mode=0, bias=0),
 dict(name='asym_4x_bias_dc', curve=ASYM, sym=False, x=noise, pre_db=6, mix=70, post_db=-3, dc=1, mode=2, bias=24),
 dict(name='linear_1x_clip', curve=LIN, sym=True, x=hot, pre_db=0, mix=100, post_db=0, dc=0, mode=0, bias=0),
 dict(name='soft_4x_mix50_dc', curve=SOFT, sym=True, x=hot*0.5, pre_db=3, mix=50, post_db=2, dc=1, mode=2, bias=0),
]
index=[]
for c in cases:
    tab=build_table(c['curve'],c['sym'])
    g=Gen(sr); y=np.zeros_like(c['x'])
    pre=10**(c['pre_db']/20); post=10**(c['post_db']/20); bias=c['bias']*0.005
    for n in range(len(c['x'])):
        y[n]=g.tick(c['x'][n,0],c['x'][n,1],pre,c['mix']/100,post,c['dc'],0,c['mode'],tab,tab,bias)
    pts=c['curve']
    curve_str='2 %d %d '%(1 if c['sym'] else 0,len(pts))+' '.join('%.17g %.17g %.17g'%tuple(p) for p in pts)
    c['x'].astype('<f8').tofile(f"{out_dir}/{c['name']}_in.f64")
    y.astype('<f8').tofile(f"{out_dir}/{c['name']}_out.f64")
    index.append(dict(name=c['name'],curve=curve_str,frames=N,pre_db=c['pre_db'],mix=c['mix'],post_db=c['post_db'],dc=c['dc'],mode=c['mode'],bias=c['bias']))
    print(c['name'],'done, peak out %.3f'%np.abs(y).max())
# simple text index the C++ test can parse: one case per line
with open(f'{out_dir}/cases.txt','w') as f:
    for c in index:
        f.write(f"{c['name']}|{c['frames']}|{c['pre_db']}|{c['mix']}|{c['post_db']}|{c['dc']}|{c['mode']}|{c['bias']}|{c['curve']}\n")
