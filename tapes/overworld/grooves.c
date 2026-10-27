/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* GROOVE (the drums played as EZdrummer's): white 1-15 a groove, five song sections of three (light, more, full),
 * white 16 the ending; the black keys fills, short to long. Part of tape_dsp.c.
 * A groove: rows "L" + 16 or 32 steps (one or two bars, a 1/16 each), separated by '|'. L is the drum (GM):
 *   K kick 36, S snare 38, R rim 37, P clap 39, H closed hat 42, O open hat 46, L low tom 45, M mid tom 47, T high tom 50,
 *   X crash 49, Y ride 51, Z shaker 70, W wood 76
 * A step: '.' nothing, '1'..'9' how hard (30 + 10 x). A fill is the bar's last steps: it plays from the next beat (or
 * where its notes start, if later); the groove under it stops there. */

static const dk_groove_t GROOVES[15] = {
    /* INTRO: the hat, the toms, nothing heavy */
    {"INTRO 1", "H6.4.6.4.6.4.6.4.|Z..3...3...3...3."},
    {"INTRO 2", "H6.4.6.4.6.4.6.4.|L7.......5.......7.......5...5...|Z..3...3...3...3."},
    {"INTRO 3", "H5.3.5.3.5.3.5.3.|L6.....4.6.......6.....4.6...5.5.|M..........4.....|R....4.......4..."},
    /* VERSE: the kick and the snare come in */
    {"VERSE 1", "K7.......7.......|S....6.......6...|H4.4.4.4.4.4.4.4."},
    {"VERSE 2", "K7.....5.7.......7.....5.7..5....|S....6.......6.......6.......6..2|H5.3.5.3.5.3.5.3."},
    {"VERSE 3", "K7.....5.7..5....7.....5.7..5..5.|S....7..2....7.......7..2....7.2.|H5353535353535353|Z..3...3...3...3."},
    /* CHORUS: everything, the crash on the one, the open hat */
    {"CHORUS 1", "X8...............................|K8.....6.8.......8.....6.8..6....|S....8.......8...|O..5...5...5...5."},
    {"CHORUS 2", "X8...............................|K8..6..6.8..6....8..6..6.8..6..6.|S....8.......8...|Y6.4.6.4.6.4.6.4.|P....5.......5..."},
    {"CHORUS 3", "X9...............................|K8.6...6.8.6...6.|S....8..3....8.3.|H6464646464646464|O..............6."},
    /* BRIDGE: half time, the toms */
    {"BRIDGE 1", "K7...............|S........7.......|H4...4...4...4..."},
    {"BRIDGE 2", "K7.......7.......|L6..4..6...4.6...|M....5.......5...|Z3.3.3.3.3.3.3.3."},
    {"BRIDGE 3", "K7...7...7...7...|S........6...6.6.6...6...6...6666|T..........4.5.6.|H5.5.5.5.5.5.5.5."},
    /* OUTRO: thinning out */
    {"OUTRO 1", "K6.......5.......|R....5.......5...|Y4...3...4...3..."},
    {"OUTRO 2", "R....5.......5...|H4...4...4...4...|L5...............3..............."},
    {"OUTRO 3", "K6...............|H3.......3.......|Z2...2...2...2..."},
};

/* the black keys: short to long; the last five (6..11) end in a crash on the next one */
static const dk_groove_t FILLS[11] = {
    {"FLAM", "S............7.8."},
    {"SNARE 4", "S............6789"},
    {"KICK SNARE", "K........7...7...|S............7.8."},
    {"TOMS DOWN", "T............7...|M..............7.|L...............8"},
    {"LIFT", "O..............8.|K............7..."},
    {"ROLL HALF", "S........56677889|K........7.......|X1"},
    {"TOMS HALF", "T........7.7.....|M............7.7.|L..............88|X1"},
    {"CLAPS", "P........7...7.78|K........7...7...|X1"},
    {"ROLL BAR", "S4455667788889999|K8.......8.......|X1"},
    {"TOMS BAR", "T7.7.7.7.........|M........7.7.....|L............7788|K8...8...8...8...|X1"},
    {"BIG", "S7.7.7.7.7.7.8888|K8...8...8...8..8|L............8.8.|X1"},
};

/* white 16: the last hit (the crash, the kick, on the next one), then quiet */
static const dk_groove_t GROOVE_END = {"END", "X9|K9"};
