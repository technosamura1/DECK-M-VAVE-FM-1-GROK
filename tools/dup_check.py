#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""DECK's duplicate check: two presets that are the same sound under two names.

  python tools/dup_check.py [out/sounds] [--list] [--strict]

Reads out/sounds (deck_test sounds: every preset's WAV, the same notes for the same role, and presets.tsv with what each
is made of) and compares, per engine and then across all of them:
  * the parameters: the engine, its eight parameters, the envelope, the effect, the reverb and echo, the macro -- all the
    same but the name and the level trim: the same preset twice, whatever the audio says;
  * the sound: the spectrum (the average, in 1/6-octave bands, the level taken out), and how it moves (six bands' loudness
    over six seconds, the level taken out). Near-identical: both distances small (SPEC_DB, ENV_DB below).
The presets that are meant to be a pair (the same sound with one thing changed: the TWINS list) are the only exception.
Prints the pairs; exits 1 if there is one.  --list: also prints each engine's nearest neighbours (to look at the margin)."""
import os
import sys
import wave

import numpy as np

SR = 44100
SPEC_DB = 0.5   # mean absolute difference of the band spectra (dB), the level taken out: under it, the same timbre
ENV_DB = 1.3    # ... and of the bands' loudness over time (dB)
TWINS = set()   # {("ENGINE", "NAME A", "NAME B")} on purpose (none)
# pairs a player has named as the same sound: they must be clearly apart (the spectrum this far, or the motion this far)
NAMED = [("TINE", "VELVET", "MELLOW", 3.0, 4.0)]

EDGES = 40.0 * 2.0 ** (np.arange(0, 50) / 6.0)           # 1/6 octave from 40 Hz up to ~16 kHz
BANDS = [(60, 250), (250, 700), (700, 1800), (1800, 4000), (4000, 8000), (8000, 16000)]


def load(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        ch = w.getnchannels()
        x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768.0
    return x.reshape(-1, ch).mean(1)


def features(x):
    x = x[:SR * 8]
    # the average spectrum (Welch), in 1/6-octave bands
    nfft = 8192
    hop = nfft // 2
    win = np.hanning(nfft)
    acc = np.zeros(nfft // 2 + 1)
    cnt = 0
    for i in range(0, max(1, len(x) - nfft), hop):
        acc += np.abs(np.fft.rfft(x[i:i + nfft] * win)) ** 2
        cnt += 1
    acc /= max(cnt, 1)
    fr = np.fft.rfftfreq(nfft, 1 / SR)
    bands = []
    for a, b in zip(EDGES[:-1], EDGES[1:]):
        m = (fr >= a) & (fr < b)
        bands.append(acc[m].sum() if m.any() else 0.0)
    spec = 10 * np.log10(np.array(bands) + 1e-12)
    spec = np.maximum(spec, spec.max() - 70.0)            # nothing under -70 dB of the loudest band counts
    spec -= spec.mean()
    # the bands' loudness over time (250 ms frames), the level taken out
    fl = SR // 4
    nfr = min(24, len(x) // fl)
    env = np.full((len(BANDS), 24), -80.0)
    for k in range(nfr):
        seg = x[k * fl:(k + 1) * fl]
        sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
        f2 = np.fft.rfftfreq(len(seg), 1 / SR)
        for j, (a, b) in enumerate(BANDS):
            env[j, k] = 10 * np.log10(sp[(f2 >= a) & (f2 < b)].sum() + 1e-12)
    top = env.max()
    env = np.maximum(env, top - 60.0) - top
    return spec, env.ravel()


def read_tsv(d):
    rows = {}
    p = os.path.join(d, "presets.tsv")
    if os.path.exists(p):
        for ln in open(p, encoding="utf-8").read().splitlines():
            e, fn, pid, name, params = ln.split("\t")
            rows[(e, fn.split("/", 1)[1])] = (int(pid), name, params)
    return rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    d = args[0] if args else "out/sounds"
    show = "--list" in sys.argv
    tsv = read_tsv(d)
    items = []
    for e in sorted(os.listdir(d)):
        ed = os.path.join(d, e)
        if not os.path.isdir(ed):
            continue
        for fn in sorted(os.listdir(ed)):
            if fn.endswith(".wav"):
                spec, env = features(load(os.path.join(ed, fn)))
                pid, name, params = tsv.get((e, fn), (-1, fn[6:-4].replace("_", " "), None))
                items.append((e, fn, name, params, spec, env))
    found = []
    n = len(items)
    S = np.array([it[4] for it in items])
    E = np.array([it[5] for it in items])
    for i in range(n):
        ds = np.abs(S[i + 1:] - S[i]).mean(1)
        de = np.abs(E[i + 1:] - E[i]).mean(1)
        for k in np.nonzero((ds < SPEC_DB) & (de < ENV_DB))[0]:
            j = i + 1 + k
            a, b = items[i], items[j]
            same_params = a[3] is not None and a[0] == b[0] and a[3].split("|")[:4] == b[3].split("|")[:4]
            same_params = same_params and a[3].rsplit("|", 1)[0] == b[3].rsplit("|", 1)[0]
            near = ds[k] < SPEC_DB and de[k] < ENV_DB
            if (near or same_params) and (a[0], a[2], b[2]) not in TWINS:
                found.append((ds[k], de[k], same_params, a, b))
    # the same parameters under two names (the pairs the sound's distance does not see because it is a different engine's)
    byk = {}
    for it in items:
        if it[3] is not None:
            byk.setdefault((it[0], it[3].rsplit("|", 1)[0]), []).append(it)
    seen = {(f[3][0], f[3][1], f[4][1]) for f in found}
    for lst in byk.values():
        for a, b in zip(lst, lst[1:]):
            if (a[0], a[1], b[1]) not in seen:
                found.append((0.0, 0.0, True, a, b))
    for eng, na, nb, ms, me in NAMED:
        ia = [it for it in items if it[0] == eng and it[2] == na]
        ib = [it for it in items if it[0] == eng and it[2] == nb]
        for a in ia:
            for b in ib:
                ds = np.abs(a[4] - b[4]).mean()
                de = np.abs(a[5] - b[5]).mean()
                if ds < ms and de < me:
                    found.append((ds, de, False, a, b))
    found.sort(key=lambda t: t[0])
    for ds, de, sp, a, b in found:
        print("%-9s %-22s = %-22s spectrum %.2f dB, motion %.2f dB%s  (%s | %s)" %
              (a[0] if a[0] == b[0] else a[0] + "/" + b[0], a[2], b[2], ds, de, "  SAME PARAMETERS" if sp else "", a[1], b[1]))
    if show:
        print("--- nearest neighbour of each preset (spectrum dB), the 30 closest")
        near = []
        for i in range(n):
            ds = np.abs(S - S[i]).mean(1)
            ds[i] = 99
            j = int(ds.argmin())
            if j > i or ds[j] != 99:
                near.append((ds[j], i, j))
        for v, i, j in sorted(near)[:30]:
            print("  %.2f  %s %s  ~  %s %s" % (v, items[i][0], items[i][2], items[j][0], items[j][2]))
    print("%d presets, %d near-identical pair(s)" % (n, len(found)))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
