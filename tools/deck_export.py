#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""DECK's EXPORT, received: the SysEx stream the FM-1 sends (PROJECT page, black key 1) written as a Type-1 .mid.

  python tools/deck_export.py --list                         the MIDI inputs
  python tools/deck_export.py --port "FM-1" song.mid         listen on an input (mido + python-rtmidi), write the .mid
  python tools/deck_export.py --syx song.syx song.mid        from a .syx file (the simulator's "export FILE")

The .mid: the tempo, 4/4, six named tracks (the engine and its preset), the parts as markers on the timeline, the
road's tricks already applied, the clips' motion as CC74 (the first knob) and CC7 (the volume). It opens in Reaper.
The frames are checked (their sum, their order); a lost frame is reported."""
import argparse
import struct
import sys

HEAD = bytes([0xF0, 0x7D, 0x44, 0x4B])


def frames_of(data):
    """the DECK frames in a byte stream (F0 .. F7)"""
    i = 0
    while True:
        a = data.find(0xF0, i)
        if a < 0:
            return
        b = data.find(0xF7, a)
        if b < 0:
            return
        yield data[a:b + 1]
        i = b + 1


def unpack(frame):
    """(kind, seq, payload bytes) of a frame, or None if it is not DECK's or its sum is wrong"""
    if frame[:4] != HEAD or len(frame) < 9:
        return None
    kind, seq = frame[4], frame[5] | frame[6] << 7
    body, s = frame[7:-2], frame[-2]
    if sum(body) & 0x7F != s:
        raise ValueError(f"frame {seq}: checksum")
    out = bytearray()
    i = 0
    while i < len(body):
        hi = body[i]
        chunk = body[i + 1:i + 8]
        for j, c in enumerate(chunk):
            out.append(c | ((hi >> j) & 1) << 7)
        i += 8
    return kind, seq, bytes(out)


class Song:
    def __init__(self):
        self.tempo, self.ppq, self.ntrk, self.length, self.name = 120, 96, 6, 0, "DECK"
        self.names, self.chan = {}, {}
        self.events, self.markers = [], []
        self.end, self.expected, self.last_seq = None, None, -1
        self.lost = 0

    def take(self, frame):
        r = unpack(frame)
        if r is None:
            return False
        kind, seq, p = r
        if self.last_seq >= 0 and seq != (self.last_seq + 1) & 0x3FFF:
            self.lost += 1
        self.last_seq = seq
        if kind == 1:
            self.tempo, self.ppq, self.ntrk = p[0], p[1], p[2]
            self.length = struct.unpack_from("<I", p, 3)[0]
            self.name = p[7:].decode("ascii", "replace")
        elif kind == 2:
            self.names[p[0]] = p[2:].decode("ascii", "replace")
            self.chan[p[0]] = p[1]
        elif kind == 4:
            self.markers.append((struct.unpack_from("<I", p, 0)[0], p[4:].decode("ascii", "replace")))
        elif kind == 3:
            for i in range(0, len(p) - 7, 8):
                tick, trk, st, d1, d2 = struct.unpack_from("<IBBBB", p, i)
                self.events.append((tick, trk, st, d1, d2))
        elif kind == 0x7F:
            self.expected = struct.unpack_from("<I", p, 0)[0]
            self.end = True
        return True


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def track_chunk(evs):
    """evs: (tick, bytes) sorted; the end of track appended"""
    out = bytearray()
    last = 0
    for t, b in evs:
        out += vlq(t - last) + b
        last = t
    out += vlq(0) + b"\xFF\x2F\x00"
    return b"MTrk" + struct.pack(">I", len(out)) + out


def meta(kind, text):
    b = text.encode("ascii", "replace")
    return bytes([0xFF, kind]) + vlq(len(b)) + b


def write_mid(song, path):
    tracks = []
    us = int(60_000_000 / max(30, song.tempo))
    t0 = [(0, meta(0x03, song.name)), (0, b"\xFF\x51\x03" + us.to_bytes(3, "big")), (0, b"\xFF\x58\x04\x04\x02\x18\x08")]
    for tick, text in sorted(song.markers):
        t0.append((tick, meta(0x06, text)))
    tracks.append(track_chunk(sorted(t0, key=lambda e: e[0])))
    for k in range(song.ntrk):
        evs = [(0, meta(0x03, song.names.get(k, f"TRACK {k + 1}")))]
        mine = [e for e in song.events if e[1] == k]
        # note-offs before note-ons at the same tick
        mine.sort(key=lambda e: (e[0], 0 if e[2] & 0xF0 == 0x80 else 1))
        for tick, _, st, d1, d2 in mine:
            evs.append((tick, bytes([st, d1, d2])))
        tracks.append(track_chunk(evs))
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 1, len(tracks), song.ppq))
        for t in tracks:
            f.write(t)


def validate(path, song):
    """with mido (if it is there): the file reads, six tracks and a tempo one, the markers, the notes"""
    try:
        import mido
    except ImportError:
        return "mido not installed: written, not checked"
    m = mido.MidiFile(path)
    names = [t.name for t in m.tracks]
    marks = [msg.text for msg in m.tracks[0] if msg.type == "marker"]
    notes = sum(1 for t in m.tracks for msg in t if msg.type == "note_on" and msg.velocity)
    assert len(m.tracks) == song.ntrk + 1, names
    assert marks, "no markers"
    return f"mido: {len(m.tracks)} tracks {names[1:]}, {len(marks)} markers {marks}, {notes} notes, {m.length:.1f} s"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?", default="deck.mid")
    ap.add_argument("--syx")
    ap.add_argument("--port")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    song = Song()
    if a.list:
        import mido
        print("\n".join(mido.get_input_names()))
        return 0
    if a.syx:
        for fr in frames_of(open(a.syx, "rb").read()):
            song.take(fr)
    elif a.port:
        import mido
        print(f"listening on {a.port}: on the FM-1, PROJECT page, black key 1 (EXPORT)")
        with mido.open_input(a.port) as inp:
            for msg in inp:
                if msg.type == "sysex":
                    song.take(bytes([0xF0]) + bytes(msg.data) + bytes([0xF7]))
                    if song.end:
                        break
    else:
        ap.error("--syx FILE or --port NAME")
    if not song.end:
        print("the stream did not end (its last frame lost?)")
    if song.expected is not None and song.expected != len(song.events):
        print(f"events: {len(song.events)} of {song.expected}")
    if song.lost:
        print(f"{song.lost} frame(s) lost")
    write_mid(song, a.out)
    print(f"{a.out}: {song.name}, {song.tempo} BPM, {len(song.events)} events, {len(song.markers)} markers")
    print(validate(a.out, song))
    return 0 if song.end and not song.lost and song.expected == len(song.events) else 1


if __name__ == "__main__":
    sys.exit(main())
