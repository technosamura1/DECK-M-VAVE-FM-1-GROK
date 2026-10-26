#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""BENCH's PC side: starts the measurement on the FM-1 over USB-MIDI (SysEx F0 7D 44 4B 53 <cmd> F7), waits, reads
every row and writes bench_result.txt (the numbers, and what they say). Send that file back.

  python deck_bench.py [--cpu1 | --no-cpu1] [--out FILE]      (no flag: asks before the second core)
  python deck_bench.py --selftest                              (no device: a made-up FM-1, checks the report)
"""
import argparse
import re
import sys
import time

PORT_RE = re.compile(r"fm-?1|felucca|composite|sinco|usb-midi", re.I)
ID = [0x7D, 0x44, 0x4B, 0x53]
VARS = ["I32", "F32", "FSIN", "C1-I32", "PAR-I32", "C1-F32", "C1-NOP"]
NL = [1, 4, 8, 12, 16, 24]
ST = {0: "idle", 1: "run0", 2: "done0", 3: "run1", 4: "done1"}
C1 = {0: "not started", 1: "RUNNING", 2: "DID NOT START (timeout)", 3: "HUNG during a job"}
RSTAT = {0: "pending", 1: "ok", 2: "skipped (too slow)", 3: "timeout", 4: "no CPU1"}
BLOCK_US = {128: 128 / 0.0441, 64: 64 / 0.0441}   # a block's length in us at 44.1 kHz


def kvs(line):
    return {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}


class Dev:
    def __init__(self, i, o):
        import mido
        self.mido, self.i, self.o = mido, i, o

    def ask(self, cmd=None, arg=None, wait=2.0):
        data = list(ID) + ([ord(cmd)] if cmd else []) + ([arg] if arg is not None else [])
        while self.i.poll():
            pass
        self.o.send(self.mido.Message("sysex", data=data))
        end = time.time() + wait
        while time.time() < end:
            m = self.i.poll()
            if m is None:
                time.sleep(0.003)
                continue
            if m.type == "sysex" and list(m.data[:4]) == ID:
                s = bytes(m.data[4:]).decode("ascii", "replace").strip()
                if s.startswith("BX "):
                    return s
        return None


def open_dev():
    import mido
    ins, outs = mido.get_input_names(), mido.get_output_names()
    for n in ins:
        if PORT_RE.search(n):
            base = re.sub(r"\s+\d+$", "", n)
            o = next((o for o in outs if re.sub(r"\s+\d+$", "", o) == base), outs[0] if len(outs) == 1 else None)
            if o:
                return Dev(mido.open_input(n), mido.open_output(o)), n
    sys.exit("FM-1 не найдена (MIDI-порты: %s). Закрой Reaper и другие MIDI-программы, кабель с данными." % ins)


def meta(dev, tries=6):
    for _ in range(tries):
        s = dev.ask(None, wait=1.5)
        if s and s.startswith("BX m"):
            return kvs(s)
    return None


def read_rows(dev, n, raw):
    rows = {}
    for i in range(n):
        s = None
        for _ in range(4):
            s = dev.ask("R", i, wait=1.0)
            if s and s.startswith("BX r") and kvs(s).get("i") == i:
                break
            s = None
        if s:
            raw.append(s)
            rows[i] = kvs(s)
    return rows


def wait_state(dev, want, label, limit, log):
    t0, last_t = time.time(), None
    while time.time() - t0 < limit:
        m = meta(dev, 2)
        if m is None:
            continue
        if last_t is not None and m["t"] < last_t:
            log("!!! FM-1 перезагрузилась во время теста (uptime сбросился). Так бывает, если 2-е ядро уронило прошивку.")
            return m, True
        last_t = m["t"]
        if m["st"] in want:
            return m, False
        log("\r  %s: строка %d из %d   " % (label, m["cur"], m["n"]))
        time.sleep(0.4)
    return None, False


def us(r):
    return r["tk"] / 24.0


def report(rows, m, extra=""):
    """the text: the table per variant and block, the derived numbers, what they mean"""
    by = {}
    for r in rows.values():
        by[(r["v"], r["b"], r["n"], r["o"])] = r
    out = []
    A = out.append
    A("BENCH result   uptime_ms=%s perf_counter=%s c1=%s irqoff_max_us=%s xrun=%s" %
      (m.get("t"), m.get("perf"), C1.get(m.get("c1"), m.get("c1")), m.get("irqoff_max_us"), m.get("xrun")))
    if extra:
        A(extra)
    A("")
    A("us per block (best of 5), 6 operators per voice; the block's length: 128 samples = 2902 us, 64 = 1451 us")
    A("%-9s %4s " % ("variant", "blk") + " ".join("%7s" % ("N=%d" % n) for n in NL))
    for v in (0, 1, 2, 3, 5, 4):
        for b in (128, 64):
            cells = []
            seen = False
            for n in NL:
                r = by.get((v, b, n, 6))
                if r is None or r["st"] != 1:
                    cells.append("%7s" % ("-" if r is None else {2: "S", 3: "T", 4: "x"}.get(r["st"], "-")))
                else:
                    seen = True
                    cells.append("%7.0f" % us(r))
            if seen or v < 3:
                A("%-9s %4d " % (VARS[v], b) + " ".join(cells))
    A("   (S too slow, T timeout, x no CPU1, - not measured)")
    A("")
    A("operator count, 1 voice, 128 samples (us):")
    for v in (0, 1, 2):
        cells = []
        for o in (1, 2, 4):
            r = by.get((v, 128, 1, o))
            cells.append("%d ops: %s" % (o, "%.1f" % us(r) if r and r["st"] == 1 else "-"))
        r6 = by.get((v, 128, 1, 6))
        cells.append("6 ops: %s" % ("%.1f" % us(r6) if r6 and r6["st"] == 1 else "-"))
        A("  %-5s " % VARS[v] + "   ".join(cells))
    A("")
    A("derived (N=24, 128 samples):")
    for v in (0, 1, 2):
        r = by.get((v, 128, 24, 6))
        if not r or r["st"] != 1:
            continue
        ns = r["tk"] / 24.0 * 1000.0 / (24 * 6 * 128)
        per_voice = us(r) / 24
        fit60 = 0.6 * BLOCK_US[128] / per_voice
        fit100 = BLOCK_US[128] / per_voice
        cyc = ""
        if r["cy"]:
            cyc = "  = %.1f cycles per operator-sample, clock about %.0f MHz" % (r["cy"] / (24 * 6 * 128.0), r["cy"] * 24.0 / r["tk"])
        A("  %-5s %.1f ns per operator-sample%s" % (VARS[v], ns, cyc))
        A("        %.1f us per voice (6 ops) -> %.0f voices fill 60%% of the block, %.0f fill 100%%" % (per_voice, fit60, fit100))
    ri, rf = by.get((0, 128, 24, 6)), by.get((1, 128, 24, 6))
    if ri and rf and ri["st"] == 1 and rf["st"] == 1:
        A("  float / integer time: %.2f  (below 1: float is faster)" % (rf["tk"] / float(ri["tk"])))
    hs = by.get((6, 128, 0, 6))
    c1rows = [r for r in rows.values() if r["v"] in (3, 4, 5, 6)]
    if c1rows:
        A("")
        A("CPU1 (second core):  state: %s" % C1.get(m.get("c1"), m.get("c1")))
        if hs and hs["st"] == 1:
            A("  hand-off (empty job, go -> done): %.1f us" % us(hs))
        for tag, v, ref in (("C1-I32 / I32", 3, 0), ("C1-F32 / F32", 5, 1)):
            r, rr = by.get((v, 128, 24, 6)), by.get((ref, 128, 24, 6))
            if r and rr and r["st"] == 1 and rr["st"] == 1:
                A("  %s time (N=24): %.2f  (1.0: CPU1 as fast as CPU0)   checksum equal: %s" %
                  (tag, r["tk"] / float(rr["tk"]), "YES" if r["ok"] else "NO"))
        r, rr = by.get((4, 128, 24, 6)), by.get((0, 128, 24, 6))
        if r and rr and r["st"] == 1 and rr["st"] == 1:
            A("  PAR (24 voices on EACH core at once) / I32 alone: %.2f  (1.0: perfect, 48 voices in the time of 24;"
              % (r["tk"] / float(rr["tk"])))
            A("      2.0: no gain from the second core)  -> 48 voices cost %.0f us per 128 samples; checksum equal: %s"
              % (us(r), "YES" if r["ok"] else "NO"))
        bad = [r for r in c1rows if r["st"] == 1 and not r["ok"]]
        if bad:
            A("  !!! %d CPU1 rows gave a different result than CPU0 (checksums differ): CPU1's maths is not the same" % len(bad))
        if m.get("c1") == 3:
            A("  !!! CPU1 stopped answering: its rows after that are 'timeout'. Power-cycle the FM-1.")
    A("")
    A("NOT measured: the envelope stages, operator-6 feedback, the output filter, the mix to stereo (the real engine adds them:")
    A("the numbers are somewhat LOW); the audio ISR's own cost; flash/XIP cache pressure from the rest of the firmware.")
    return "\n".join(out)


def run(dev, a, log):
    raw = []
    m = meta(dev)
    if not m:
        sys.exit("Ответа нет: на FM-1 стоит не BENCH. Установи через install-bench.bat.")
    log("BENCH найден. CPU0: начинаю замер (около 10-20 с)...\n")
    dev.ask("S", wait=2.0)
    m, _ = wait_state(dev, (2,), "CPU0", 180, log)
    if not m:
        sys.exit("Замер CPU0 не закончился за 3 минуты. Выключи и включи FM-1 и повтори.")
    rows = read_rows(dev, m["n0"], raw)
    text = report(rows, m)
    open(a.out, "w", encoding="utf-8").write("\n".join(raw) + "\n\n" + text + "\n")
    log("\nCPU0 готов, файл %s записан.\n" % a.out)
    want = a.cpu1
    if want is None:
        log("\nВторое ядро (CPU1) — БЕТА, на железе ещё не запускалось. Если зависнет — выключи и включи FM-1,\n"
            "прошивка DECK и откат целы. Результат CPU0 уже сохранён.\n")
        want = input("Запустить тест второго ядра? [y/N] ").strip().lower() in ("y", "д", "yes", "да")
    if want:
        while True:
            m = meta(dev)
            if not m or m["wait"] == 0:
                break
            log("\r  жду, пока пройдут 30 с после включения FM-1: ещё %d с   " % m["wait"])
            time.sleep(1)
        log("\nCPU1: старт...\n")
        dev.ask("C", wait=3.0)
        m2, reset = wait_state(dev, (4,), "CPU1", 180, log)
        if reset or not m2:
            note = "FM-1 перезагрузилась или не отвечает во время теста CPU1: вероятно, второе ядро уронило прошивку."
            open(a.out, "a", encoding="utf-8").write("\n" + note + "\n")
            log("\n" + note + "\n")
            return a.out
        m = m2
        rows = read_rows(dev, m["n"], raw)
        text = report(rows, m)
        open(a.out, "w", encoding="utf-8").write("\n".join(raw) + "\n\n" + text + "\n")
    log("\n" + text + "\n\nГотово. Пришли файл %s (или скопируй текст выше).\n" % a.out)
    return a.out


class Fake:   # a made-up FM-1 for --selftest: 40 ns per operator-sample on CPU0, CPU1 25 % slower
    def __init__(self):
        self.st, self.c1, self.t = 0, 0, 5000
        self.rows = []

    def build(self):
        def row(v, n, b, o, tk, ok=1, st=1):
            self.rows.append({"v": v, "n": n, "b": b, "o": o, "st": st, "tk": tk, "cy": int(tk * 4) if v < 3 else 0,
                              "rp": 5, "ck": 123, "ok": ok})
        for v, mul in ((0, 1.0), (1, 0.8), (2, 2.0)):
            for b in (128, 64):
                for n in NL:
                    row(v, n, b, 6, int(40e-3 * 24 * 6 * n * b * mul))
        for v, mul in ((0, 1.0), (1, 0.8), (2, 2.0)):
            for o in (1, 2, 4):
                row(v, 1, 128, o, int(40e-3 * 24 * o * 128 * mul))
        row(6, 0, 128, 6, 900)
        for v, mul in ((3, 1.25), (4, 1.1), (5, 1.0)):
            for b in (128, 64):
                for n in NL:
                    row(v, n, b, 6, int(40e-3 * 24 * 6 * n * b * mul) + 900)

    def ask(self, cmd=None, arg=None, wait=0):
        if not self.rows:
            self.build()
        if cmd == "S":
            self.st = 2
        elif cmd == "C":
            self.st, self.c1 = 4, 1
        if cmd == "R":
            r = self.rows[arg]
            return "BX r i=%d v=%d n=%d b=%d o=%d st=%d tk=%d cy=%d rp=%d ck=%d ok=%d" % (
                arg, r["v"], r["n"], r["b"], r["o"], r["st"], r["tk"], r["cy"], r["rp"], r["ck"], r["ok"])
        self.t += 100
        return "BX m ver=1 st=%d cur=82 n0=45 n=82 c1=%d hb=7 wait=0 perf=1 irqoff_max_us=5000 xrun=0 t=%d" % (
            self.st, self.c1, self.t)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpu1", dest="cpu1", action="store_true", default=None)
    ap.add_argument("--no-cpu1", dest="cpu1", action="store_false")
    ap.add_argument("--out", default="bench_result.txt")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    log = lambda s: (sys.stdout.write(s), sys.stdout.flush())
    if a.selftest:
        a.cpu1, a.out = True, "bench_selftest.txt"
        run(Fake(), a, lambda s: None)
        t = open(a.out, encoding="utf-8").read()
        assert "40.0 ns per operator-sample" in t and "hand-off" in t and "checksum equal: YES" in t, t
        print(t)
        print("SELFTEST OK")
        return
    dev, name = open_dev()
    log("Порт: %s\n" % name)
    run(dev, a, log)


if __name__ == "__main__":
    main()
