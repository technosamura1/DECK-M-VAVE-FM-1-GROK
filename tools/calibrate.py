#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Level the tape's presets (CLAUDE_TASK 9.2): from sound_check's JSON, each preset's trim (half dB steps, the last
number of its line in tapes/NAME/tape_dsp.c) moved so that it sits at its engine's target loudness.
The twelve of each engine's first block (PRESETS) and the others (PRESETS2: the blocks 1..3, as many as PCOUNT
says) are both read; the first 108 (the nine old engines' first block) are left as they are unless --all is given.
  python tools/calibrate.py build/test/check.json tapes/overworld/tape_dsp.c [--all]"""
import json
import re
import sys

# each engine's loudness (gated, K-weighted, dB): the piano the reference, the pads a little under, the world behind
TARGET = {"FELT": -20.5, "TINE": -21.5, "KALIMBA": -21.5, "GLASS": -21.5, "AIRPAD": -23.0, "SOFTBASS": -23.5,
          "DUSTKIT": -24.0, "WORLD": -33.0, "WINDS": -22.0, "SYNTH": -22.5, "PLUCK": -22.0}
ORDER = ["FELT", "TINE", "AIRPAD", "GLASS", "SOFTBASS", "DUSTKIT", "WORLD", "KALIMBA", "WINDS", "SYNTH", "PLUCK"]
OLD = 9 * 12  # the presets the first release shipped: their trims stay put
RX = re.compile(r'^(\s*\{"[^"]*", \d+, .*, )(-?\d+)\},\s*$')


def pcount(src):
    a = src.index("static const uint8_t PCOUNT")
    b = src.index("};", a)
    nums = [int(x) for x in re.findall(r"\d+", src[src.index("{", a) + 1:b])]
    if len(nums) != 4 * len(ORDER):
        raise SystemExit(f"PCOUNT has {len(nums)} numbers")
    return nums


def walk(src, name, rows, res, order_of_row, keep):
    """rows: the lines of one array; order_of_row(k) -> (engine, file index); returns the new text and a count."""
    a = src.index(name)
    b = src.index("};", a)
    out = []
    k = 0
    for ln in src[a:b].split("\n"):
        m = RX.match(ln)
        if m:
            eng, fi = order_of_row(k)
            if not keep(k):
                r = res[eng][fi]
                new = int(m.group(2)) + int(round((TARGET[eng] - r["loud_db"]) * 2))
                ln = f"{m.group(1)}{max(-24, min(24, new))}}},"
            k += 1
        out.append(ln)
    return src[:a] + "\n".join(out) + src[b:], k


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    every = "--all" in sys.argv
    res = json.load(open(args[0]))
    path = args[1]
    src = open(path, newline="").read()
    pc = pcount(src)
    # the files of an engine come as b0_*, b1_*, b2_*, b3_*: its first twelve, then the others' in PCOUNT's order
    first = lambda k: (ORDER[k // 12], k % 12)
    src, n1 = walk(src, "static const dk_preset_t PRESETS[", None, res, first, lambda k: (not every) and k < OLD)
    seq = []
    for e in range(len(ORDER)):
        for blk in (1, 2, 3):
            seq += [(ORDER[e], 12 + sum(pc[4 * e + 1:4 * e + blk]) + i) for i in range(pc[4 * e + blk])]
    src, n2 = walk(src, "static const dk_preset_t PRESETS2[", None, res, lambda k: seq[k], lambda k: False)
    if n1 != 12 * len(ORDER) or n2 != len(seq):
        raise SystemExit(f"found {n1} + {n2} presets, expected {12 * len(ORDER)} + {len(seq)}")
    open(path, "w", newline="").write(src)
    print("trims set for", n2 + n1 - (0 if every else OLD), "presets")


if __name__ == "__main__":
    main()
