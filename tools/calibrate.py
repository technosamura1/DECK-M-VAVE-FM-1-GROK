#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Level the tape's presets (CLAUDE_TASK 9.2): from sound_check's JSON, each preset's trim (half dB steps, the last
number of its line in tapes/NAME/tape_dsp.c) moved so that it sits at its engine's target loudness.
  python tools/calibrate.py build/test/check.json tapes/overworld/tape_dsp.c"""
import json
import re
import sys

# each engine's loudness (gated, K-weighted, dB): the piano the reference, the pads a little under, the world behind
TARGET = {"FELT": -20.5, "TINE": -21.5, "KALIMBA": -21.5, "GLASS": -21.5, "AIRPAD": -23.0, "SOFTBASS": -23.5,
          "DUSTKIT": -24.0, "WORLD": -33.0, "BEATKIT": -23.0, "FM": -21.5, "PLUCK": -22.0}
ORDER = ["FELT", "TINE", "AIRPAD", "GLASS", "SOFTBASS", "DUSTKIT", "WORLD", "KALIMBA", "BEATKIT", "FM", "PLUCK"]


def main():
    res = json.load(open(sys.argv[1]))
    path = sys.argv[2]
    src = open(path, newline="").read()
    a = src.index("static const dk_preset_t PRESETS")
    b = src.index("};", a)
    block = src[a:b]
    lines = block.split("\n")
    rx = re.compile(r'^(\s*\{"[^"]*", \d+, .*, )(-?\d+)\},\s*$')
    k = 0
    out = []
    for ln in lines:
        m = rx.match(ln)
        if m:
            eng = ORDER[k // 12]
            r = res[eng][k % 12]
            cur = int(m.group(2))
            new = cur + int(round((TARGET[eng] - r["loud_db"]) * 2))
            new = max(-24, min(24, new))
            ln = f"{m.group(1)}{new}}},"
            k += 1
        out.append(ln)
    if k != 12 * len(ORDER):
        raise SystemExit(f"found {k} presets")
    open(path, "w", newline="").write(src[:a] + "\n".join(out) + src[b:])
    print("trims set for", k, "presets")


if __name__ == "__main__":
    main()
