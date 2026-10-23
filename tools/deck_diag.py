#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""DECK's diagnostics: asks the FM-1 (F0 7D 44 4B 53 F7) twice a second and logs its answer: the render's time
(of 5805 us a block), the dropouts, the UI's slowest frame, the voices, how long interrupts were off, the key scan's
longest gap. Play what stutters while it runs.   python deck_diag.py [SECONDS] [LOG]"""
import re
import sys
import time

import mido

PORT_RE = re.compile(r"fm-?1|felucca|composite|sinco|usb-midi", re.I)
ASK = [0x7D, 0x44, 0x4B, 0x53]


def port():
    ins, outs = mido.get_input_names(), mido.get_output_names()
    for n in ins:
        if PORT_RE.search(n):
            base = re.sub(r"\s+\d+$", "", n)
            o = next((o for o in outs if re.sub(r"\s+\d+$", "", o) == base), outs[0] if len(outs) == 1 else None)
            if o:
                return n, o
    sys.exit("deck_diag: FM-1 not found (MIDI ports: %s). Close Reaper and other MIDI apps." % ins)


def main():
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 60
    log = sys.argv[2] if len(sys.argv) > 2 else "deck_diag.txt"
    i_name, o_name = port()
    worst = {"rmax": 0, "ui_max_ms": 0, "irqoff_max_us": 0, "scan_gap_max_us": 0}
    first_x = last_x = None
    with mido.open_input(i_name) as i, mido.open_output(o_name) as o, open(log, "w", encoding="utf-8") as f:
        f.write("port %s / %s\n" % (i_name, o_name))
        print("Пишу %d с в %s. Играй то, что лагает." % (secs, log))
        end = time.time() + secs
        got = 0
        while time.time() < end:
            o.send(mido.Message("sysex", data=ASK))
            t = time.time() + 0.4
            line = None
            while time.time() < t:
                m = i.poll()
                if m is None:
                    time.sleep(0.005)
                    continue
                if m.type == "sysex" and list(m.data[:4]) == ASK:
                    line = bytes(m.data[4:]).decode("ascii", "replace").strip()
                    break
            if line:
                got += 1
                f.write(line + "\n")
                f.flush()
                kv = dict(x.split("=", 1) for x in line.split() if "=" in x)
                for k in worst:
                    worst[k] = max(worst[k], int(kv.get(k, 0)))
                x = int(kv.get("xrun", 0))
                first_x = x if first_x is None else first_x
                last_x = x
                print("\r cpu %3s%%  render max %5s us (из 5805)  xrun %4s  ui %4s ms   " %
                      (kv.get("cpu"), kv.get("rmax"), x, kv.get("ui_max_ms")), end="", flush=True)
            time.sleep(0.1)
        summary = "\nSUMMARY answers=%d xruns_during=%s worst=%s\n" % (got, (last_x - first_x) if got else "-", worst)
        f.write(summary)
        print(summary)
        if not got:
            print("Ответа нет: на FM-1 стоит прошивка без диагностики?")


if __name__ == "__main__":
    main()
