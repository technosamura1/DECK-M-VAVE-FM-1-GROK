#!/usr/bin/env python3
"""Per-voice table of every drum machine rendered by `deck_test kitvoices DIR`: peak, decay, centroid, pitch."""
import sys, glob, os, wave
import numpy as np

def load(fn):
    w = wave.open(fn)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float) / 32768.0
    return x.reshape(-1, w.getnchannels()).mean(1), w.getframerate()

def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "out/kits"
    for m in sorted(os.listdir(d)):
        print("==", m)
        for fn in sorted(glob.glob(f"{d}/{m}/*.wav")):
            x, sr = load(fn)
            pk = np.abs(x).max()
            env = np.abs(x)
            win = int(sr * 0.005)
            e = np.convolve(env, np.ones(win) / win, "same")
            t = np.where(e > pk * 0.01)[0]  # -40 dB
            dur = (t[-1] / sr) if len(t) else 0
            t2 = np.where(e > pk * 0.1)[0]
            d20 = (t2[-1] / sr) if len(t2) else 0
            seg = x[: int(sr * 0.15)]
            sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
            fr = np.fft.rfftfreq(len(seg), 1 / sr)
            cen = (sp * fr).sum() / (sp.sum() + 1e-9)
            lo = sp[(fr > 20) & (fr < 1500)]
            f0 = fr[(fr > 20) & (fr < 1500)][lo.argmax()]
            nm = os.path.basename(fn)[3:-4]
            print(f"  {nm:9s} peak {20*np.log10(pk+1e-9):6.1f}  -20dB {d20*1000:5.0f} ms  -40dB {dur*1000:5.0f} ms  cen {cen:6.0f} Hz  peakfreq {f0:5.0f}")

main()
