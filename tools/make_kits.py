#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""BEATKIT's kits: drum samples made here (synthesis after the classic machines, not their ROMs), each machine's grain
kept clean (the grain is BEATKIT's CRUSH and the track's effect), packed as 8-bit mu-law into
tapes/overworld/kits.h. Your own: a folder of WAVs named after the slots (kick.wav snare.wav ...), mono or stereo,
16-bit, any rate:
  python tools/make_kits.py                      the six kits -> tapes/overworld/kits.h
  python tools/make_kits.py --wav DIR NAME       ... and DIR's WAVs as a seventh kit called NAME
  python tools/make_kits.py --listen out/kits    each kit's samples as WAVs too (to hear them)"""
import os
import sys
import wave

import numpy as np

SR = 44100
# sampled: what carries a machine's character; its toms, cowbell, open hat, cymbals and shaker are synthesized by the
# engine (as the machines' own were)
SLOTS = ["kick", "snare", "rim", "clap", "hat", "perc"]
rng = np.random.default_rng(707)


# ------------------------------------------------------------------ building blocks
def t_of(sec):
    return np.arange(int(sec * SR)) / SR


def env(sec, tau, att=0.0005):
    t = t_of(sec)
    e = np.exp(-t / tau)
    if att > 0:
        e *= np.minimum(1.0, t / att)
    return e


def sine_sweep(sec, f0, f1, tau_f, phase=0.0):
    t = t_of(sec)
    f = f1 + (f0 - f1) * np.exp(-t / tau_f)
    return np.sin(2 * np.pi * np.cumsum(f) / SR + phase)


def noise(sec):
    return rng.uniform(-1, 1, int(sec * SR))


def onepole_lp(x, fc):
    k = 1 - np.exp(-2 * np.pi * fc / SR)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s += k * (x[i] - s)
        y[i] = s
    return y


def onepole_hp(x, fc):
    return x - onepole_lp(x, fc)


def biquad(x, kind, fc, q):
    w = 2 * np.pi * fc / SR
    cw, sw = np.cos(w), np.sin(w)
    a = sw / (2 * q)
    if kind == "bp":
        b0, b1, b2 = a, 0.0, -a
    elif kind == "hp":
        b0, b1, b2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2
    else:
        b0, b1, b2 = (1 - cw) / 2, 1 - cw, (1 - cw) / 2
    a0, a1, a2 = 1 + a, -2 * cw, 1 - a
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    y = np.empty_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        v = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, x[i], y1, v
        y[i] = v
    return y


def square(sec, f):
    t = t_of(sec)
    return np.sign(np.sin(2 * np.pi * f * t + 0.3))


def metal(sec, scale=1.0):
    """six squares at the 808's odd ratios: the cymbals' and hats' ring"""
    return sum(square(sec, f * scale) for f in (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)) / 6


def mixlen(*xs):
    n = max(len(x) for x in xs)
    out = np.zeros(n)
    for x in xs:
        out[: len(x)] += x
    return out


def sat(x, drive):
    return np.tanh(x * drive) / np.tanh(drive)


def room(x, amt, size=1.0):
    """a small room: three combs and an all-pass (the dusty snare's space)"""
    y = x.copy()
    for d, g in ((int(0.0113 * SR * size), 0.62), (int(0.0171 * SR * size), 0.55), (int(0.0237 * SR * size), 0.48)):
        buf = np.zeros(len(x) + d)
        for i in range(len(x)):
            buf[i + d] = x[i] + g * buf[i]
        y += amt * buf[d:] * 0.33
    return y


def resample(x, ratio):
    """x at SR played ratio times faster (0.92: pitched down a little, as a sampler's knob)"""
    n = int(len(x) / ratio)
    pos = np.arange(n) * ratio
    i = np.minimum(pos.astype(int), len(x) - 2)
    f = pos - i
    return x[i] * (1 - f) + x[i + 1] * f


def crush(x, rate, bits):
    """a sampler's grain: held at rate, bits deep"""
    step = SR / rate
    idx = (np.floor(np.arange(len(x)) / step) * step).astype(int)
    y = x[np.minimum(idx, len(x) - 1)]
    q = 2 ** (bits - 1)
    return np.round(y * q) / q


def fade_end(x, sec=0.004):
    n = min(len(x), int(sec * SR))
    x = x.copy()
    x[len(x) - n:] *= np.linspace(1, 0, n)
    return x


def trim(x, floor_db=-54):
    """the silence at the end cut"""
    a = np.abs(x)
    th = a.max() * 10 ** (floor_db / 20)
    k = np.nonzero(a > th)[0]
    end = k[-1] + 1 if len(k) else len(x)
    return fade_end(x[:end])


# ------------------------------------------------------------------ the voices
def kick(tone, sec=0.5, f0=150, f1=48, tau_f=0.03, tau=0.25, click=0.4, drive=1.0):
    body = sine_sweep(sec, f0, f1, tau_f) * env(sec, tau, 0.001)
    ck = onepole_lp(noise(0.006), 6000) * env(0.006, 0.0015, 0) * click * 2
    k = mixlen(body, ck)
    return sat(k, drive) if drive > 1 else k


def snare(sec=0.3, f1=185, f2=330, tone_tau=0.06, nz_tau=0.12, nz_bp=2600, nz_amt=0.8, tone_amt=0.6):
    tone = (sine_sweep(sec, f1 * 1.3, f1, 0.01) * 0.6 + sine_sweep(sec, f2 * 1.2, f2, 0.01) * 0.4) * env(sec, tone_tau)
    nz = onepole_hp(biquad(noise(sec), "bp", nz_bp, 0.6), 600) * env(sec, nz_tau) * 1.6
    return tone * tone_amt + nz * nz_amt


def rim(sec=0.05):
    x = (np.sin(2 * np.pi * 500 * t_of(sec)) * 0.7 + np.sin(2 * np.pi * 1720 * t_of(sec)) * 0.5) * env(sec, 0.009)
    return x + biquad(noise(sec), "bp", 3000, 2) * env(sec, 0.003) * 0.8


def clap(sec=0.35, bp=1150, tail=0.11):
    nz = biquad(noise(sec), "bp", bp, 1.2)
    t = t_of(sec)
    e = np.zeros_like(t)
    for k, d in enumerate((0.0, 0.010, 0.020, 0.030)):
        on = t >= d
        e[on] = np.maximum(e[on], np.exp(-(t[on] - d) / (0.004 if k < 3 else tail)))
    return nz * e * 2.2


def hat(sec, tau, scale=1.0, bp=8000, nz=0.35):
    m = metal(sec, scale) + noise(sec) * nz
    return onepole_hp(biquad(m, "bp", bp, 0.9), 5500) * env(sec, tau, 0.0003) * 1.8


def tom(f, sec=0.4, tau=0.22):
    return sine_sweep(sec, f * 1.6, f, 0.06) * env(sec, tau, 0.001) + \
        biquad(noise(sec), "bp", f * 5, 1) * env(sec, 0.02) * 0.25


def crash(sec=1.2, tau=0.55, scale=1.0):
    m = metal(sec, scale * 1.35) * 0.6 + noise(sec)
    return onepole_hp(biquad(m, "bp", 6500, 0.5), 3000) * env(sec, tau, 0.001) * 1.4


def cowbell(sec=0.35, tau=0.09):
    x = square(sec, 540) * 0.6 + square(sec, 800) * 0.4
    return biquad(x, "bp", 2400, 1.5) * (env(sec, tau, 0.0005) * 0.7 + env(sec, 0.01) * 0.3) * 2


def shaker(sec=0.16):
    t = t_of(sec)
    e = np.minimum(1, t / 0.02) * np.exp(-np.maximum(0, t - 0.02) / 0.04)
    return onepole_hp(biquad(noise(sec), "bp", 7000, 0.8), 4000) * e * 1.6


def tamb(sec=0.25):
    return hat(sec, 0.07, 2.6, 9000, 0.6) + shaker(sec) * 0.4


def conga(f=330, sec=0.3):
    return sine_sweep(sec, f * 1.25, f, 0.015) * env(sec, 0.09, 0.0008) + \
        biquad(noise(sec), "bp", 2000, 1) * env(sec, 0.006) * 0.3


def clave(f=2500, sec=0.08):
    return np.sin(2 * np.pi * f * t_of(sec)) * env(sec, 0.018, 0.0002)


def woodblock(sec=0.12):
    return (np.sin(2 * np.pi * 820 * t_of(sec)) * 0.8 + np.sin(2 * np.pi * 2222 * t_of(sec)) * 0.25) * env(sec, 0.035)


def snap(sec=0.12):
    return biquad(noise(sec), "bp", 2400, 1.5) * env(sec, 0.012, 0.0003) * 2 + clave(1600, sec) * 0.2


# ------------------------------------------------------------------ the kits
def kit_707():
    k = {
        "kick": kick(0, 0.35, 120, 52, 0.022, 0.13, 0.7),
        "snare": snare(0.25, 205, 360, 0.04, 0.09, 3200, 0.95, 0.45),
        "rim": rim(),
        "clap": clap(0.3, 1300, 0.09),
        "hat": hat(0.08, 0.025, 1.15),
        "open": hat(0.45, 0.16, 1.15),
        "ltom": tom(110, 0.35, 0.16),
        "htom": tom(190, 0.3, 0.13),
        "crash": crash(1.0, 0.45, 1.1),
        "cow": cowbell(0.25, 0.06),
        "shaker": tamb(),
        "perc": clave(2300),
    }
    return k


def kit_808():
    return {
        "kick": kick(0, 1.1, 95, 47, 0.04, 0.42, 0.15),
        "snare": snare(0.3, 175, 330, 0.05, 0.11, 2600, 0.8, 0.7),
        "rim": rim(0.04),
        "clap": clap(0.4, 1100, 0.12),
        "hat": hat(0.07, 0.022),
        "open": hat(0.6, 0.22),
        "ltom": tom(95, 0.5, 0.26),
        "htom": tom(165, 0.4, 0.2),
        "crash": crash(1.4, 0.65),
        "cow": cowbell(0.4, 0.11),
        "shaker": shaker(),
        "perc": conga(320),
    }


def kit_909():
    return {
        "kick": kick(0, 0.5, 210, 52, 0.018, 0.2, 1.0, 1.8),
        "snare": snare(0.3, 190, 340, 0.045, 0.14, 4200, 1.1, 0.5),
        "rim": rim(0.05),
        "clap": clap(0.35, 1250, 0.13),
        "hat": hat(0.09, 0.03, 1.25, 9500, 0.6),
        "open": hat(0.55, 0.2, 1.25, 9500, 0.6),
        "ltom": tom(120, 0.4, 0.18),
        "htom": tom(200, 0.35, 0.15),
        "crash": crash(1.4, 0.7, 1.2),
        "cow": cowbell(0.3, 0.08),
        "shaker": tamb(),
        "perc": clave(2500),
    }


def kit_cr78():
    k = {
        "kick": kick(0, 0.3, 90, 62, 0.02, 0.09, 0.05),
        "snare": snare(0.2, 240, 420, 0.03, 0.06, 3800, 0.55, 0.6),
        "rim": mixlen(clave(1800, 0.06) * 0.8, rim(0.04) * 0.3),
        "clap": clap(0.25, 1500, 0.07) * 0.8,
        "hat": hat(0.06, 0.012, 1.4, 10000, 0.8),
        "open": hat(0.3, 0.09, 1.4, 10000, 0.8),
        "ltom": conga(180, 0.3),
        "htom": conga(270, 0.25),
        "crash": crash(0.8, 0.3, 1.4) * 0.8,
        "cow": cowbell(0.2, 0.05),
        "shaker": shaker(0.1),
        "perc": woodblock(),
    }
    return {n: onepole_lp(x, 9000) for n, x in k.items()}


def kit_dusty():
    """boom bap: thick, a little slower and lower, the snare in a small room"""
    k = {
        "kick": kick(0, 0.55, 110, 46, 0.035, 0.22, 0.6, 2.6),
        "snare": room(snare(0.35, 175, 300, 0.07, 0.16, 2200, 0.85, 0.75), 0.6),
        "rim": rim(0.06),
        "clap": room(clap(0.35, 1000, 0.1), 0.5),
        "hat": hat(0.09, 0.03, 0.95, 7000, 0.6) * 0.8,
        "open": hat(0.4, 0.13, 0.95, 7000, 0.6) * 0.8,
        "ltom": tom(85, 0.45, 0.2),
        "htom": tom(140, 0.4, 0.17),
        "crash": crash(1.0, 0.45, 0.9),
        "cow": cowbell(0.25, 0.07),
        "shaker": shaker(),
        "perc": snap(),
    }
    out = {}
    for n, x in k.items():
        x = resample(sat(x / np.abs(x).max(), 1.4), 0.9)              # 10 % slower: lower, longer
        out[n] = onepole_lp(x, 12000)
    return out


def kit_tape():
    """a cassette's drums: soft, rounded, the top worn, squeezed"""
    k = {
        "kick": kick(0, 0.45, 100, 50, 0.03, 0.18, 0.2, 1.5),
        "snare": snare(0.3, 200, 340, 0.05, 0.12, 1800, 0.7, 0.7),
        "rim": rim(0.05),
        "clap": clap(0.3, 950, 0.09),
        "hat": hat(0.08, 0.028, 1.0, 6000, 0.9) * 0.7,
        "open": hat(0.35, 0.12, 1.0, 6000, 0.9) * 0.7,
        "ltom": tom(100, 0.4, 0.2),
        "htom": tom(160, 0.35, 0.16),
        "crash": crash(0.9, 0.4, 0.9) * 0.7,
        "cow": cowbell(0.25, 0.07) * 0.8,
        "shaker": shaker(0.14),
        "perc": woodblock(),
    }
    return {n: onepole_lp(sat(onepole_lp(x / np.abs(x).max(), 9000), 1.5), 8000) for n, x in k.items()}


KITS = [("707", kit_707), ("808", kit_808), ("909", kit_909), ("CR78", kit_cr78), ("DUSTY", kit_dusty),
        ("TAPE", kit_tape)]
# the mix inside a kit: the kick on top, the hats behind
GAIN = {"kick": 1.0, "snare": 0.8, "rim": 0.55, "clap": 0.7, "hat": 0.42, "open": 0.4, "ltom": 0.75, "htom": 0.7,
        "crash": 0.38, "cow": 0.45, "shaker": 0.4, "perc": 0.55}


def read_wav(path):
    w = wave.open(path)
    n, ch, sw, fr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    if sw != 2:
        raise SystemExit(f"{path}: 16-bit WAVs only")
    x = np.frombuffer(w.readframes(n), np.int16).astype(float).reshape(-1, ch).mean(axis=1) / 32768
    return resample(x, fr / SR) if fr != SR else x


def kit_wav(folder):
    return {s: read_wav(os.path.join(folder, s + ".wav")) for s in SLOTS}


# ------------------------------------------------------------------ mu-law, 8 bits
def mulaw(y):
    """-1..1 to the G.711-style mu-law byte the engine expands (a 256 entry table): 38 dB, even across the level"""
    mu = 255.0
    c = np.sign(y) * np.log1p(mu * np.abs(y)) / np.log1p(mu)
    q = np.clip(np.round(c * 127), -127, 127).astype(int)
    dec = np.sign(q) * ((1 + mu) ** (np.abs(q) / 127) - 1) / mu
    return bytes((q & 255).tolist()), dec


def to_rate(x, rate):
    """down to the stored rate: a gentle low-pass (FFT) under its Nyquist, then resampled"""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X *= np.clip((rate * 0.47 - f) / (rate * 0.06), 0, 1)
    return resample(np.fft.irfft(X, len(x)), SR / rate)


def pick_rate(x):
    """the lowest rate that loses nothing heard: 11 kHz (kicks), 16 kHz, else 22 kHz"""
    X = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    for rate in (11025, 16000):
        if X[f > rate * 0.41].sum() < X.sum() * 0.002:
            return rate
    return 22050


def main():
    args = sys.argv[1:]
    kits = list(KITS)
    listen = None
    while args:
        a = args.pop(0)
        if a == "--wav":
            folder, name = args.pop(0), args.pop(0)
            kits.append((name.upper()[:8], lambda folder=folder: kit_wav(folder)))
        elif a == "--listen":
            listen = args.pop(0)
    data = bytearray()
    rows, total, snr = [], 0, []
    for name, make in kits:
        smp = make()
        row = []
        for s in SLOTS:
            x = trim(np.asarray(smp[s], float), -50)
            x = x / np.abs(x).max() * 0.92
            rate = pick_rate(x)
            y = to_rate(x, rate)
            y = y / max(1e-9, np.abs(y).max()) * 0.92
            nib, dec = mulaw(y)
            snr.append(10 * np.log10(np.sum(y ** 2) / max(1e-12, np.sum((y - dec) ** 2))))
            row.append((len(data), len(y), rate, GAIN[s]))
            data += nib
            if listen:
                os.makedirs(listen, exist_ok=True)
                w = wave.open(os.path.join(listen, f"{name}_{s}.wav"), "wb")
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(rate)
                w.writeframes((dec * 32767).astype(np.int16).tobytes())
                w.close()
        rows.append((name, row))
        total += sum(r[1] for r in row)
    out = os.path.join(os.path.dirname(__file__), "..", "tapes", "overworld", "kits.h")
    with open(out, "w", newline="\n") as f:
        f.write("/* SPDX-License-Identifier: MPL-2.0\n * Copyright (C) 2026 DECK contributors */\n")
        f.write("/* BEATKIT's kits: written by tools/make_kits.py (do not edit). Samples made by synthesis after the classic\n"
                " * machines (no ROM of theirs), 8-bit mu-law. */\n")
        f.write(f"#define BK_NKIT {len(rows)}\n#define BK_NSLOT {len(SLOTS)}\n")
        f.write("static const char *const BK_KIT_NAME[BK_NKIT] = {" + ", ".join(f'"{n}"' for n, _ in rows) + "};\n")
        f.write("static const bk_smp_t BK_SMP[BK_NKIT][BK_NSLOT] = {\n")
        for name, row in rows:
            f.write("    {" + ", ".join(f"{{{o}, {n}, {r}, {g:.2f}f}}" for o, n, r, g in row) + "},\n")
        f.write("};\n")
        f.write(f"static const uint8_t BK_DATA[{len(data)}] = {{\n")
        for i in range(0, len(data), 24):
            f.write("    " + ",".join(str(b) for b in data[i:i + 24]) + ",\n")
        f.write("};\n")
    print(f"mu-law: signal to noise {min(snr):.1f} dB at worst, {np.median(snr):.1f} dB typical")
    print(f"{len(rows)} kits, {total} samples, {len(data)} bytes -> {os.path.normpath(out)}")
    for name, row in rows:
        print(f"  {name:6s} " + " ".join(f"{s}:{n / r:.2f}s@{r // 1000}k" for s, (o, n, r, g) in zip(SLOTS, row)))


if __name__ == "__main__":
    main()
