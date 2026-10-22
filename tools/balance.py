#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""the demo's mix (CLAUDE_TASK 9.6): each track's loudness, section by section (the road's blocks), against the mix:
nothing sticking out, the bass not booming, the piano in front.  python tools/balance.py build/test"""
import os
import sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sound_check import SR, kweight, load  # noqa: E402

d = sys.argv[1] if len(sys.argv) > 1 else "build/test"
names = ["DRUMS", "BASS", "PIANO", "PAD", "BELLS", "NATURE"]
bars = [8, 8, 16, 16, 8, 8, 16, 8]
labels = ["INTRO", "PRE", "VERSE", "CHORUS", "BRIDGE", "BREAK", "CHORUS", "OUTRO"]
spb = 60.0 / 76 * 4
edges = np.cumsum([0] + bars) * spb
mix = kweight(load(os.path.join(d, "stem_MIX.wav")))
st = {n: kweight(load(os.path.join(d, f"stem_{n}.wav"))) for n in names}


def loud(x, a, b):
    s = x[int(a * SR):int(b * SR)]
    return 10 * np.log10((s ** 2).mean() + 1e-12) if len(s) else -99


print("section   MIX   " + "  ".join(f"{n:>6s}" for n in names))
issues = []
for i, lab in enumerate(labels):
    a, b = edges[i], edges[i + 1]
    lm = loud(mix, a, b)
    row = {n: loud(st[n], a, b) - lm for n in names}
    print(f"{lab:8s} {lm:5.1f}  " + "  ".join(f"{row[n]:6.1f}" for n in names))
    if row["PIANO"] > -30 and max(row.values()) > row["PIANO"] + 1.0 and max(row, key=row.get) not in ("PIANO",):
        top = max(row, key=row.get)
        if top in ("BASS", "DRUMS", "PAD"):
            issues.append(f"{lab}: {top} louder than the piano")
    if row["BASS"] > -4:
        issues.append(f"{lab}: the bass booms")
print("BALANCED" if not issues else "\n".join(issues))
