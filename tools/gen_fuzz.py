#!/usr/bin/env python3
"""A random button/knob/key scenario for the host simulator: python3 tools/gen_fuzz.py SEED EVENTS > tests/scenarios/fuzzN.dk
Presses, holds (REC, FX, EDIT, ... long and short), key taps, knob turns across all pages; every 80 events the song's
invariants are checked (expect inv 0: events in the pool, clips sorted, lengths sane, targets valid, voices within the pool)."""
import random, sys
seed, n = int(sys.argv[1]), int(sys.argv[2])
auto = len(sys.argv) > 3 and sys.argv[3] == "auto"
snd = len(sys.argv) > 3 and sys.argv[3] == "sound"   # SOUND: block / preset / engine / kit / FILTER cutoff switches, REC layers and automation, user presets, save+load round-trips   # AUTOMATION: mostly REC held + knobs on SOUND / MIX, tracks and parts switched, copies
r = random.Random(seed)
BTN = "FX SEL ENV LFO EDIT GLO HOME ARP SEQ PLAY REC OCT- OCT+".split()   # (no SAVE: the project's pages are a wipe away)
ENC = "SELECT ALGO PRESET K1 K2 K3 K4".split()
out = ["# FUZZ seed %d, %d events (tools/gen_fuzz.py): random buttons, holds, keys and knobs on every page; the song's invariants hold" % (seed, n),
       "wait 300", "demo", "wait 100"]
held = []
if auto:
    out += ["tap PLAY", "wait 400", "tap FX"]
def sound_event():
    """one event of the SOUND mode: the preset blocks, engines, kits, user slots, the FX page and the project round-trip"""
    y = r.random()
    o = []
    if y < 0.14:
        o += ["press HOME", "tap %s" % r.choice(["FX", "SEL"]), "release HOME"]                    # the next / previous block
    elif y < 0.22:
        o += ["press HOME", "turn PRESET %d 40" % r.choice([-3, -1, 1, 2]), "release HOME"]         # the next melodic engine
    elif y < 0.34:
        o.append("turn PRESET %d 40" % r.choice([-9, -3, -1, 1, 2, 5, 13, 40]))                     # along every block, the user's
    elif y < 0.44:
        o.append("trkset %d %d %d %d" % (r.randrange(1, 7), r.randrange(0, 30), r.randrange(0, 9), r.randrange(0, 30)))   # any engine, block, place
    elif y < 0.52:
        o += ["press SAVE", "tapkey w%d" % r.choice([0, 1, 2, 12, 13, 14, 15]), "release SAVE"]          # a user preset (it keeps the sound as it is); w0-2: the part snapshot, its recall
    elif y < 0.58:
        o.append("tapkey w%d" % r.randrange(0, 16) if r.random() < 0.9 else "ballast %d" % r.choice([300, 900, 2500]))   # a white key: the block's place or a user slot; now and then the song memory fills
    elif y < 0.72:
        o += ["tap FX", "tap FX", "turn %s %d 40" % (r.choice(["K1", "K2", "K3", "K4"]), r.choice([-9, -4, -1, 2, 6, 12]))]   # the FX page: the cutoff and the rest
    elif y < 0.80:
        o += ["tap FX", "turn ALGO %d 40" % r.choice([-3, -1, 1, 2, 5])]                           # the effect type (FILTER among them)
    elif y < 0.88:
        o += ["turn K4 %d 40" % r.choice([-9, -3, 3, 9]), "turn K3 %d 40" % r.choice([-5, 5])]
    elif y < 0.94:
        o += ["roundtrip", "expect inv 0"]                                                          # saved and loaded again
    else:
        o += ["press REC", "turn %s %d 60" % (r.choice(["K1", "K2", "K3", "K4"]), r.choice([-5, 5])), "wait %d" % r.choice([300, 1200]),
              "tapkey w%d" % r.randrange(0, 12), "release REC", "wait 50"]
    return o
for i in range(n):
    x = r.random()
    if snd and x < 0.5:
        out += sound_event()
        out.append("wait %d" % r.choice([5, 30, 120]))
        if i % 25 == 24:
            out += ["expect inv 0", "expect voices <25"]
        continue
    if auto and x < 0.55:
        out += ["press REC"]
        for _ in range(r.randrange(2, 9)):
            out.append("turn %s %d %d" % (r.choice(["K1", "K2", "K3", "K4"]), r.choice([-6, -3, -1, 1, 3, 6]), r.choice([20, 60])))
            out.append("wait %d" % r.choice([20, 90, 250]))
        if r.random() < 0.08:
            out += ["tap FX"]                 # the erase combo
        out += ["wait %d" % r.choice([50, 300, 1100, 2700]), "release REC", "wait %d" % r.choice([20, 400])]
        if r.random() < 0.5:
            out.append("turn %s %d 60" % (r.choice(["K1", "K2", "K3", "K4"]), r.choice([-4, 4])))   # a hand over a curve
        if r.random() < 0.3:
            out.append(r.choice(["tap FX", "tap LFO", "tap FX", "tap SEQ"]))
        if r.random() < 0.15:
            out.append("turn SELECT %d" % r.choice([-1, 1, 2]))                                      # another track
        if r.random() < 0.10:
            out += ["press ARP", "tapkey w%d" % r.randrange(0, 8), "release ARP"]                  # another part (a copy when empty)
        if r.random() < 0.05:
            out += ["press EDIT", "wait 900", "release EDIT"]                                       # undo
        if r.random() < 0.04:
            out += ["press ENV", "wait 700", "tapkey w%d" % r.randrange(0, 8), "release ENV"]       # the garage: copy a part
        if r.random() < 0.03:
            out += ["press REC", "wait %d" % r.choice([1200, 2800]), "release REC"]                 # erase by holding
        out.append("expect inv 0") if i % 20 == 19 else None
        continue
    if x < 0.30:
        out.append("turn %s %d %d" % (r.choice(ENC), r.choice([-6, -3, -1, 1, 2, 4, 9]), r.choice([10, 40, 80])))
    elif x < 0.50:
        out.append("tapkey %s%d" % (r.choice("wwb"), r.randrange(0, 11)))
    elif x < 0.62:
        out.append("key w%d down %d" % (r.randrange(0, 16), r.randrange(20, 127)))
        out.append("wait %d" % r.choice([20, 100, 400]))
        out.append("key w%d up" % r.randrange(0, 16))
    elif x < 0.80:
        b = r.choice(BTN)
        out.append("tap %s" % b)
    elif x < 0.92:
        # a hold with knobs turned inside it (REC + a knob is AUTOMATION; REC + FX its erase)
        b = r.choice(["REC", "REC", "REC", "FX", "EDIT", "HOME", "SEQ", "ENV", "LFO", "GLO", "ARP"])
        out.append("press %s" % b)
        for _ in range(r.randrange(0, 6)):
            out.append(r.choice(["turn %s %d 60" % (r.choice(ENC), r.choice([-5, -2, 2, 5])), "tapkey w%d" % r.randrange(0, 8),
                                 "tap %s" % r.choice(["FX", "SEQ", "EDIT"]) if b == "REC" else "wait 50"]))
            out.append("wait %d" % r.choice([30, 120, 300]))
        out.append("wait %d" % r.choice([100, 400, 1200, 2700]))
        out.append("release %s" % b)
    else:
        out.append("wait %d" % r.choice([5, 50, 200, 800, 2000]))
    out.append("wait %d" % r.choice([5, 20, 60]))
    if i % 80 == 79:
        out.append("expect inv 0")
        out.append("expect voices <25")
out += ["wait 500", "expect inv 0"]
if auto and n <= 1000:                      # (a long run may hold REC 2.5 s: the ERASE gesture, by design, wipes the track's curves)
    out.append("expect autoall >0")
print("\n".join(out))
