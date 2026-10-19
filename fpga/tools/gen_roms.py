#!/usr/bin/env python3
import numpy as np, pathlib
OUT = pathlib.Path(__file__).resolve().parent.parent / "de1soc_makara"   # ROMs sit next to the RTL
def write_hex(name, values, nyb):
    with open(OUT/name,"w") as f:
        for v in values: f.write(f"{int(v) & ((1<<(4*nyb))-1):0{nyb}x}\n")
    print(f"{name:22s} {len(values):6d} words")
N=1024
write_hex("sine_q12.hex", np.round(2047*np.sin(2*np.pi*np.arange(N)/N)).astype(int), 4)
n=np.arange(N)/(N-1)
W={"rect":np.ones(N),"hann":0.5-0.5*np.cos(2*np.pi*n),
   "hamming":0.54-0.46*np.cos(2*np.pi*n),
   "blackman":0.42-0.5*np.cos(2*np.pi*n)+0.08*np.cos(4*np.pi*n)}
a=0.25; tk=np.ones(N)
e=n<a/2;      tk[e]=0.5*(1+np.cos(2*np.pi/a*(n[e]-a/2)))
e=n>=1-a/2;   tk[e]=0.5*(1+np.cos(2*np.pi/a*(n[e]-1+a/2)))
W["tukey"]=tk
for k,w in W.items(): write_hex(f"window_{k}.hex", np.round(np.clip(w,0,1)*32767), 4)
def mackenzie(T,S,D):
    return (1448.96+4.591*T-5.304e-2*T**2+2.374e-4*T**3+1.340*(S-35)
            +1.630e-2*D+1.675e-7*D**2-1.025e-2*T*(S-35)-7.139e-13*T*D**3)
ss=[mackenzie(T,S,D) for T in np.linspace(0,35,32)
                     for S in np.linspace(0,40,16)
                     for D in np.linspace(0,300,16)]
write_hex("sound_speed.hex", np.round(np.array(ss)*16), 4)
def absorption(f,T,S,D,pH=8.0):
    c=1412+3.21*T+1.19*S+0.0167*D
    A1=8.86/c*10**(0.78*pH-5); f1=2.8*np.sqrt(S/35)*10**(4-1245/(T+273))
    A2=21.44*S/c*(1+0.025*T);  f2=8.17*10**(8-1990/(T+273))/(1+0.0018*(S-35))
    P2=1-1.37e-4*D+6.2e-9*D**2; P3=1-3.83e-5*D+4.9e-10*D**2
    A3=(4.937e-4-2.59e-5*T+9.11e-7*T**2-1.50e-8*T**3) if T<=20 else \
       (3.964e-4-1.146e-5*T+1.45e-7*T**2-6.5e-10*T**3)
    return (A1*f1*f**2/(f1**2+f**2)+A2*P2*f2*f**2/(f2**2+f**2)+A3*P3*f**2)
ab=[min(absorption(f,T,35.0,D),255.99) for f in np.linspace(80,550,64)
     for T in np.linspace(0,35,16) for D in np.linspace(0,300,4)]
write_hex("absorb.hex", np.round(np.array(ab)*256), 4)
