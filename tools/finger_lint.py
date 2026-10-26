#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Checks tapes/overworld/finger_banks.c (and FD_SRC / FM_SRC in tape_dsp.c): the FINGER banks' text patterns.
  python tools/finger_lint.py
Every set 16 patterns; strings at most 16 characters of the right alphabet; names at most 10 characters and unique in
the set; a drum set's last three are fills/breaks (the second half has a note or the pattern is a stop); no two
patterns of a set alike; melodic sets have a note in every pattern; the density does not drop by more than a third
between neighbours in the first thirteen (simple to dense). Prints the table; exit 1 on a problem."""
import re
import sys

SRC = open("tapes/overworld/finger_banks.c").read() + open("tapes/overworld/tape_dsp.c").read()


def arrays():
    out = {}
    for m in re.finditer(r"static const char \*const (\w+)\[16\]\[(\d)\] = \{(.*?)\n\};", SRC, re.S):
        name, w, body = m.group(1), int(m.group(2)), m.group(3)
        rows = []
        for r in re.finditer(r"\{((?:\s*\"[^\"]*\"\s*,?)+)\s*\}", body):
            rows.append(re.findall(r"\"([^\"]*)\"", r.group(1)))
        out[name] = (w, rows)
    return out


def main():
    bad = []
    arr = arrays()
    for name in ("FD_SRC", "FM_SRC", "FD_8BIT", "FM_8BIT", "FB_8BIT", "FP_8BIT", "FD_SYNTH", "FM_SYNTH", "FB_SYNTH",
                 "FP_SYNTH", "FD_CINEMA", "FM_CINEMA", "FB_CINEMA", "FP_CINEMA"):
        if name not in arr:
            bad.append(f"{name}: missing")
            continue
        w, rows = arr[name]
        drum = name.startswith("FD")
        if len(rows) != 16:
            bad.append(f"{name}: {len(rows)} patterns, not 16")
        names, seen, dens = set(), {}, []
        for i, r in enumerate(rows):
            r = r + [""] * (w - len(r))
            if len(r) != w:
                bad.append(f"{name}[{i}]: {len(r)} strings, not {w}")
                continue
            if not r[0] or len(r[0]) > 10:
                bad.append(f"{name}[{i}]: name '{r[0]}' empty or over 10")
            if r[0] in names:
                bad.append(f"{name}[{i}]: duplicate name {r[0]}")
            names.add(r[0])
            for j, s in enumerate(r[1:], 1):
                if len(s) > 16:
                    bad.append(f"{name}[{i}] string {j} longer than 16: '{s}'")
                if drum:
                    ok = re.fullmatch(r"[.1-9]*", s)
                else:
                    alpha = {1: r"[.1-9a-c]*", 2: r"[.1-9a-g]*", 3: r"[.dDuU]*", 4: r"[.1-9]*", 5: r"[.1-9a-c]*"}[j]
                    ok = re.fullmatch(alpha, s)
                if not ok:
                    bad.append(f"{name}[{i}] string {j} has a bad character: '{s}'")
            key = tuple(r[1:])
            if key in seen:
                bad.append(f"{name}[{i}] ({r[0]}) is the same as {seen[key]}")
            seen[key] = r[0]
            if drum:
                n = sum(len(re.sub(r"\.", "", s)) for s in r[1:])
                if i >= 13 and not any(r[1:]):
                    bad.append(f"{name}[{i}]: an empty fill")
            else:
                n = len(re.sub(r"\.", "", r[1])) + len(re.sub(r"\.", "", r[5] if len(r) > 5 else ""))
                if not n:
                    bad.append(f"{name}[{i}]: no notes")
            dens.append(n)
        for i in range(1, 13):
            if dens[i] * 3 < dens[i - 1] and dens[i - 1] > 6:
                bad.append(f"{name}: pattern {i} ({rows[i][0]}) is much thinner than the one before")
        print(f"{name:10s} " + " ".join(f"{d:2d}" for d in dens))
    for b in bad:
        print("LINT:", b)
    print("finger lint:", "FAIL" if bad else "ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
