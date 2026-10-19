#!/usr/bin/env python3
"""Read the simulator's sample dump and produce the evidence:
a time-domain plot, an FFT, a spectrogram, and the measured sweep limits.

    cd sim && vvp tb_dds.out && python analyze.py LFM_window_on

The word after analyze.py becomes the picture's title and file name, so
every run gets its own picture and nothing is overwritten.
"""
import sys, numpy as np, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

NAME = sys.argv[1] if len(sys.argv) > 1 else "pulse_analysis"

FS = 4_000_000.0
x  = np.loadtxt("dds_out.txt") / 2047.0
t  = np.arange(len(x)) / FS

# ---- measured sweep limits, from the FFT at the -6 dB points
X  = np.abs(np.fft.rfft(x))
fr = np.fft.rfftfreq(len(x), 1/FS) / 1e3
band = X > X.max() * 0.5
f_lo, f_hi = fr[band].min(), fr[band].max()

# ---- instantaneous frequency by zero crossing, eight blocks
print(f"samples      {len(x)}")
print(f"duration     {len(x)/FS*1e3:.3f} ms")
print(f"-6 dB band   {f_lo:.1f} - {f_hi:.1f} kHz")
print(f"peak         {np.abs(x).max():.4f} of full scale")
print("\nblock  t (ms)   f (kHz)")
for b in range(8):
    n = len(x)//8
    seg = x[b*n:(b+1)*n]
    zc  = np.where(np.diff(np.signbit(seg)))[0]
    print(f"  {b}    {b*n/FS*1e3:6.3f}   {(len(zc)/2)/(len(seg)/FS)/1e3:7.1f}")

fig, ax = plt.subplots(3, 1, figsize=(9, 9))

ax[0].plot(t*1e3, x, lw=0.4)
ax[0].set_xlabel("time (ms)"); ax[0].set_ylabel("normalised amplitude")
ax[0].set_title(f"{NAME}: transmitted pulse, full envelope")

ax[1].plot(fr, 20*np.log10(X/X.max() + 1e-12), lw=0.7)
ax[1].set_xlim(0, 800); ax[1].set_ylim(-80, 5)
ax[1].axvline(f_lo, ls="--", lw=0.8); ax[1].axvline(f_hi, ls="--", lw=0.8)
ax[1].set_xlabel("frequency (kHz)"); ax[1].set_ylabel("dB")
ax[1].set_title(f"Spectrum, -6 dB band {f_lo:.0f}-{f_hi:.0f} kHz")

ax[2].specgram(x, NFFT=512, Fs=FS/1e3, noverlap=448, cmap="magma")
ax[2].set_ylim(0, 800)
ax[2].set_xlabel("time (ms)"); ax[2].set_ylabel("frequency (kHz)")
ax[2].set_title("Spectrogram - this is the picture that goes in the deck")

plt.tight_layout()
plt.savefig(f"{NAME}.png", dpi=140)
print(f"\nwrote {NAME}.png")
