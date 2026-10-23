#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""DECK's sound check (CLAUDE_TASK 9): every WAV of out/sounds checked by ear's proxies.

  python tools/sound_check.py [out/sounds] [--json out/sound_check.json] [--trims]

For each file: the peak (<= -1 dBFS), DC (|mean| < -54 dBFS), clicks (a jump the signal around it does not explain),
aliasing (the top octave's energy against the whole, the high notes' part), the tail (the last 50 ms below -60 dBFS),
the loudness (gated RMS, the quiet parts left out, K-weighted roughly). Then per engine: the presets' loudness within
+-2 dB. --trims prints, per preset, the trim (half dB steps) that brings it to its engine's level."""
import json
import os
import sys
import wave

import numpy as np

SR = 44100


def load(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        ch = w.getnchannels()
        x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768.0
    return x.reshape(-1, ch)


def kweight(x):
    """a rough K-weighting in the frequency domain: a low cut at 60 Hz, a +4 dB shelf above 2 kHz"""
    n = len(x)
    f = np.fft.rfftfreq(n, 1 / SR)
    hp = (f / 60.0) ** 2 / (1 + (f / 60.0) ** 2)
    shelf = 1 + (10 ** (4 / 20) - 1) * (f / 2000.0) ** 2 / (1 + (f / 2000.0) ** 2)
    g = np.sqrt(hp) * shelf
    return np.stack([np.fft.irfft(np.fft.rfft(x[:, c]) * g, n) for c in range(x.shape[1])], axis=1)


def onepole_env(m, ms):
    k = np.exp(-1.0 / (SR * ms / 1000.0))
    # a fast envelope by block maxima (cheap): 5 ms blocks
    blk = int(SR * 0.005)
    n = len(m) // blk
    return np.repeat(np.abs(m[: n * blk]).reshape(n, blk).max(axis=1), blk), k


def check(path):
    x = load(path)
    m = x.mean(axis=1)
    r = {"file": path}
    peak = np.abs(x).max()
    r["peak_db"] = 20 * np.log10(peak + 1e-12)
    r["dc_db"] = 20 * np.log10(np.abs(x.mean(axis=0)).max() + 1e-12)
    # gated loudness: 400 ms blocks, those within 20 dB of the loudest
    blk = int(SR * 0.4)
    xs = x[: len(x) // blk * blk]
    if len(xs) == 0:
        xs = x
        blk = len(x)
    xk = kweight(xs)
    pw = (xk ** 2).mean(axis=1).reshape(-1, blk).mean(axis=1)
    db = 10 * np.log10(pw + 1e-12)
    keep = db > db.max() - 20
    r["loud_db"] = float(10 * np.log10(pw[keep].mean() + 1e-12))
    # clicks: the second difference far above its neighbourhood (and above a floor)
    d2 = np.abs(np.diff(m, 2))
    win = int(SR * 0.004)
    nb = len(d2) // win
    clicks = 0
    where = []
    if nb > 4:
        blocks = d2[: nb * win].reshape(nb, win)
        bmax = blocks.max(axis=1)
        bmed = np.median(blocks, axis=1)
        for i in range(2, nb - 2):
            ctx = np.concatenate([blocks[i - 2], blocks[i - 1], blocks[i + 1], blocks[i + 2]])
            ref = np.percentile(ctx, 95) + 1e-9
            if bmax[i] > 0.03 and bmax[i] > 12 * ref and bmax[i] > 20 * (bmed[i] + 1e-9):
                clicks += 1
                where.append(round(i * win / SR, 3))
    r["clicks"] = clicks
    r["click_at"] = where[:5]
    # aliasing / harshness: the energy above 14 kHz against the whole (the loud part)
    seg = m[: min(len(m), SR * 8)]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / SR)
    tot = spec.sum() + 1e-18
    r["top_db"] = float(10 * np.log10(spec[f > 14000].sum() / tot + 1e-18))
    r["centroid_hz"] = float((spec * f).sum() / tot)
    # the tail rung out
    t = x[-int(SR * 0.05):]
    r["tail_db"] = float(20 * np.log10(np.abs(t).max() + 1e-12))
    # the attack: the time to the first loud moment
    env = np.abs(m)
    thr = env.max() * 0.5
    idx = np.argmax(env > thr)
    first = np.argmax(env > env.max() * 0.01)
    r["attack_ms"] = float((idx - first) / SR * 1000)
    problems = []
    if r["peak_db"] > -1.0:
        problems.append("peak above -1 dBFS")
    if r["dc_db"] > -54:
        problems.append("DC")
    if clicks and "WORLD" not in path:  # the world's crackles are its sound
        problems.append(f"{clicks} click(s) at {where[:3]} s")
    noisy = any(k in path for k in ("DUSTKIT", "BEATKIT", "WORLD"))   # cymbals and rain are bright by nature
    if r["top_db"] > (-14 if noisy else -30):
        problems.append("harsh top / aliasing")
    if r["tail_db"] > -60:
        problems.append("tail cut")
    r["problems"] = problems
    return r


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    root = args[0] if args else "out/sounds"
    out_json = None
    if "--json" in sys.argv:
        out_json = sys.argv[sys.argv.index("--json") + 1]
        if out_json in args:
            args.remove(out_json)
    res = {}
    for eng in sorted(os.listdir(root)):
        d = os.path.join(root, eng)
        if not os.path.isdir(d):
            continue
        res[eng] = [check(os.path.join(d, f)) for f in sorted(os.listdir(d)) if f.endswith(".wav")]
    bad = 0
    report = []
    for eng, rs in res.items():
        louds = [r["loud_db"] for r in rs]
        med = float(np.median(louds))
        spread = max(louds) - min(louds)
        line = f"{eng:9s} loudness {med:6.1f} dB (spread {spread:4.1f} dB)"
        report.append(line)
        print(line)
        for r in rs:
            dev = r["loud_db"] - med
            r["dev_db"] = dev
            r["trim_half_db"] = int(round(-dev * 2))
            flag = list(r["problems"])
            if abs(dev) > 2.0:
                flag.append(f"level {dev:+.1f} dB off the engine's")
            name = os.path.basename(r["file"])
            s = (f"   {name:28s} peak {r['peak_db']:6.1f}  loud {r['loud_db']:6.1f}  top {r['top_db']:6.1f}  "
                 f"centroid {r['centroid_hz']:6.0f} Hz  tail {r['tail_db']:6.1f}")
            if flag:
                bad += 1
                s += "   <-- " + "; ".join(flag)
            print(s)
            report.append(s)
    print("ALL CLEAN" if not bad else f"{bad} file(s) to look at")
    if out_json:
        with open(out_json, "w") as f:
            json.dump(res, f, indent=1)
    if "--trims" in sys.argv:
        for eng, rs in res.items():
            print(eng, [r["trim_half_db"] for r in rs])
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
