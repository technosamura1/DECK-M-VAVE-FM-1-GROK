#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""CLAUDE_TASK 9.3: the same phrase on OVERWORLD's instrument and on the DECK engine that took it over, compared.
  python tools/compare_ow.py out/compare
For each pair (ow_N.wav, deck_N.wav): both brought to the same loudness, then their spectra in third-octave bands
(the difference, dB RMS), their brightness (centroid), how the sound dies away (the energy's fall after the last
note), the clicks; a verdict: the new no worse than the old (no more clicks, no harsher top, its colour close)."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from sound_check import SR, check, load  # noqa: E402

NAMES = ["PIANO (FELT)", "PAD (AIRPAD SWEDEN)", "BELL (GLASS CELESTA)", "BASS (SOFTBASS SUB)"]


def bands(x):
    m = x.mean(axis=1)
    spec = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
    f = np.fft.rfftfreq(len(m), 1 / SR)
    edges = 25.0 * 2 ** (np.arange(0, 30) / 3.0)
    out = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        sel = (f >= lo) & (f < hi)
        out.append(spec[sel].sum() + 1e-18)
    out = np.array(out)
    return 10 * np.log10(out / out.sum()), edges


def decay_s(x):
    """seconds for the last loud moment's energy to fall 30 dB"""
    m = np.abs(x).mean(axis=1)
    blk = int(SR * 0.02)
    n = len(m) // blk
    e = 20 * np.log10(m[: n * blk].reshape(n, blk).max(axis=1) + 1e-9)
    top = e.max()
    loud = np.where(e > top - 6)[0]
    last = loud[-1]
    after = np.where(e[last:] < e[last] - 30)[0]
    return (after[0] if len(after) else n - last) * 0.02


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "out/compare"
    ok_all = True
    lines = []
    for i, name in enumerate(NAMES):
        a, b = os.path.join(d, f"ow_{i}.wav"), os.path.join(d, f"deck_{i}.wav")
        if not (os.path.exists(a) and os.path.exists(b)):
            continue
        ra, rb = check(a), check(b)
        ba, edges = bands(load(a))
        bb, _ = bands(load(b))
        w = ba > ba.max() - 40                     # the bands that matter
        diff = float(np.sqrt(np.mean((ba[w] - bb[w]) ** 2)))
        da, db = decay_s(load(a)), decay_s(load(b))
        verdict = []
        if rb["clicks"] > ra["clicks"]:
            verdict.append("more clicks")
        if rb["top_db"] > ra["top_db"] + 3:
            verdict.append("harsher top")
        if diff > 5.0:
            verdict.append("another colour")
        if rb["peak_db"] > -1.0:
            verdict.append("too hot")
        good = not verdict
        ok_all &= good
        s = (f"{name:22s} colour diff {diff:4.1f} dB  centroid {ra['centroid_hz']:5.0f} -> {rb['centroid_hz']:5.0f} Hz  "
             f"top {ra['top_db']:6.1f} -> {rb['top_db']:6.1f}  decay {da:4.2f} -> {db:4.2f} s  clicks {ra['clicks']} -> "
             f"{rb['clicks']}   {'NO WORSE' if good else 'WORSE: ' + ', '.join(verdict)}")
        print(s)
        lines.append(s)
    print("ALL NO WORSE" if ok_all else "SOME WORSE")
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())
