/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The keys (the KEYS page's settings): the scale on the white keys, chords on the white or the black ones (a degree's
 * chord, the tape's borrowed ones), the voicing led from the last chord, the strum, SPLIT (the chords to the harmony
 * track, the melody to the track you are on), the black keys' performance effects and octaves; the drums' lanes; the
 * FINGER patterns. Part of the unity build. */

static const uint8_t WHITE_K[16] = PLAT_WHITE_KEYS, BLACK_K[11] = PLAT_BLACK_KEYS;
typedef struct {
    int8_t trk;                         /* the track the notes went to (-1 none) */
    uint8_t n, kind;                    /* kind 0 notes, 1 a performance effect, 2 a FINGER pattern, 3 a variation */
    uint8_t notes[9];
} keyhold_t;
static keyhold_t kh[NKEYS];
static int last_chord[8] = {55, 59, 62, 67, 71, 74, 76, 79}, have_chord;
static char chord_name[12];
static int chord_deg = -1;              /* the degree of the chord last played (-1: none, 10+: borrowed) */

static int white_of(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE_K[i] == k)
            return i;
    return -1;
}
static int black_of(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK_K[i] == k)
            return i;
    return -1;
}
static int trk_oct(int t) { return dk_song.trk[t].oct + (int8_t)dk_song.koct; }

/* SCALE as on a piano: a white key keeps its letter (the C key plays the scale's C, C# or Cb; the D key, between the two
 * black ones, its D). On a seven-note scale each letter is one degree, counted from the root's letter (flat keys
 * spelled with flats: B flat's E key plays E flat); on the others the scale's note nearest the key */
static int white_degree(int w)               /* 0..6 the degree, -1 when the scale is not seven notes */
{
    static const int8_t LETTER_OF_WHITE[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   /* C . D . E F . G . A . B */
    static const int8_t LETTER_OF_ROOT[12] = {0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6};    /* C Db D Eb E F F# G Ab A Bb B */
    int st[12], m = dk_scale(dk_song.mode, st), pc = (53 + WHITE_K[w & 15]) % 12;
    if (m != 7)
        return -1;
    return ((LETTER_OF_WHITE[pc] - LETTER_OF_ROOT[dk_song.root % 12]) % 7 + 7) % 7;
}
static int white_pitch(int w, int t)         /* the note a white key plays in SCALE (near the key's own pitch) */
{
    int st[12], m = dk_scale(dk_song.mode, st), root = dk_song.root % 12, phys = 53 + WHITE_K[w & 15] + 12 * trk_oct(t);
    int d = white_degree(w), pc, p, i, best = 99;
    if (d >= 0)
        pc = (root + st[d]) % 12;
    else {                                   /* five or six notes: the nearest of them (a tie goes up) */
        pc = root;
        for (i = 0; i < m; i++) {
            int q = (root + st[i]) % 12, dd = ((q - phys % 12) % 12 + 12) % 12;
            if (dd > 6)
                dd -= 12;
            if ((dd < 0 ? -dd * 2 + 1 : dd * 2) < best) {
                best = dd < 0 ? -dd * 2 + 1 : dd * 2;
                pc = q;
            }
        }
    }
    p = phys - ((phys - pc) % 12 + 12) % 12;  /* at or below the key, then the nearer of the two */
    if (phys - p > 6)
        p += 12;
    return p;
}

/* a chord for degree d (0..6, or 10..13 a borrowed one) in the song's key, voiced and led, around base */
static int chord_build(int d, int base, int *out)
{
    int nt[8], n, i;
    int root = dk_song.root % 12;
    if (d >= 10) {
        int minorish = dk_song.mode == 1 || dk_song.mode == 2 || dk_song.mode == 3 || dk_song.mode == 6 ||
                       dk_song.mode == 7 || dk_song.mode == 9 || dk_song.mode == 10;
        const dk_borrow_t *b = &(minorish ? TAPE.borrow_minor : TAPE.borrow_major)[(d - 10) & 3];
        int r = base + ((root + b->off - base) % 12 + 12) % 12;
        for (n = 0, i = 0; i < 7 && b->iv[i] != 99; i++)
            nt[n++] = r + b->iv[i];
        for (i = 1; i < n; i++) {               /* sorted */
            int v = nt[i], j = i - 1;
            while (j >= 0 && nt[j] > v) {
                nt[j + 1] = nt[j];
                j--;
            }
            nt[j + 1] = v;
        }
    } else if (dk_song.quality == 0)
        n = dk_chord(root, dk_song.mode, d, dk_song.chord, 1, dk_song.dim, nt, base);
    else {
        int pc = dk_scale_note(root, dk_song.mode, d) % 12;
        n = dk_chord(root, dk_song.mode, pc, dk_song.chord, 0, 0, nt, base);
    }
    if (n > 7)
        n = 7;
    {
        int prev[8];
        for (i = 0; i < 8; i++)
            prev[i] = last_chord[i < 8 ? i : 7];
        n = dk_voice(nt, n, dk_song.voicing, dk_song.bass, dk_song.lead && have_chord ? prev : 0, out);
    }
    for (i = 0; i < n && i < 8; i++)
        last_chord[i] = out[i];
    for (; i < 8; i++)
        last_chord[i] = last_chord[i - 1] + 4;
    have_chord = 1;
    chord_deg = d;
    dk_chord_name(out, n, chord_name);
    return n;
}

static void notes_post(int k, int t, const int *nt, int n, int vel, int strum)
{
    int i;
    kh[k].trk = (int8_t)t;
    kh[k].kind = 0;
    kh[k].n = 0;
    for (i = 0; i < n && i < 9; i++) {
        int note = nt[i];
        if (note < 0 || note > 127)
            continue;
        dk_note(t, note, vel - (strum ? i * 2 : 0), strum ? i * (int)(dk_song.strum * 44.1f) : 0);
        kh[k].notes[kh[k].n++] = (uint8_t)note;
    }
}

/* the note a key plays on a track (no chords): -1 none */
static int key_note_of(int k, int t)
{
    const dk_engine_t *e = TAPE.eng[dk_song.trk[t].eng % TAPE.neng];
    int w = white_of(k), b = black_of(k);
    if (e->flags & EF_DRUM) {
        static const uint8_t EXTRA_W[8] = {45, 47, 50, 49, 51, 56, 54, 39};
        static const uint8_t EXTRA_B[11] = {42, 46, 44, 37, 70, 76, 75, 49, 51, 54, 56};
        if (w >= 0)
            return w < e->nlanes ? e->lane_note[w] : EXTRA_W[(w - e->nlanes) & 7];
        return EXTRA_B[b];
    }
    if (!dk_song.layout || w < 0)               /* CHROMA (or a black key in it): the key's own pitch */
        return 53 + k + 12 * trk_oct(t);
    return white_pitch(w, t);
}

/* a key down: what it plays (vel 1..127) */
void keys_down(int k, int vel)
{
    int t = dk_song.cur_track, w = white_of(k), b = black_of(k), nt[10], n;
    const dk_track_t *trk = &dk_song.trk[t];
    const dk_engine_t *e = TAPE.eng[trk->eng % TAPE.neng];
    int harm = dk_song.split ? dk_song.harm_track : t;
    kh[k].trk = -1;
    kh[k].n = 0;
    if (trk->seq == SQ_FINGER) {                /* FINGER: patterns on the white keys, variations on the black */
        if (w >= 0) {
            dk_cmd(C_FINGER, t, trk->arp[3] ? 2 : 1, w);
            kh[k].kind = 2;
            kh[k].notes[0] = (uint8_t)w;
            kh[k].trk = (int8_t)t;
        } else if (b < 6) {
            dk_cmd(C_FVAR, t, 1, b);
            kh[k].kind = 3;
            kh[k].notes[0] = (uint8_t)b;
            kh[k].trk = (int8_t)t;
        }
        return;
    }
    if (e->flags & EF_DRUM) {
        nt[0] = key_note_of(k, t);
        notes_post(k, t, nt, 1, vel, 0);
        return;
    }
    if (dk_song.layout && b >= 0) {             /* the black keys: chords, performance, octaves */
        switch (dk_song.black) {
        case 1:                                  /* PERFORM */
            if (TAPE.perform_act && b < 11) {
                dk_cmd(C_PERFORM, TAPE.perform_act[b], 1, 0);
                kh[k].kind = 1;
                kh[k].notes[0] = TAPE.perform_act[b];
                kh[k].trk = 0;
            }
            return;
        case 2:                                  /* NOTES: the black keys play their own pitch (the sharps and flats) */
            nt[0] = 53 + k + 12 * trk_oct(t);
            notes_post(k, t, nt, 1, vel, 0);
            return;
        default:                                 /* CHORDS: the degrees, then the borrowed four */
            n = chord_build(b < 7 ? b : 10 + (b - 7), 48 + 12 * trk_oct(harm), nt);
            notes_post(k, harm, nt, n, vel, dk_song.strum > 0);
            return;
        }
    }
    if (w >= 0 && dk_song.layout && dk_song.white) {   /* WHITE = CHORDS: a chord a degree (the key's letter) */
        int deg = white_degree(w) >= 0 ? white_degree(w) : w % 7;
        n = chord_build(deg, 48 + 12 * trk_oct(harm) + (53 + WHITE_K[w] >= 65 ? 12 : 0), nt);
        notes_post(k, harm, nt, n, vel, dk_song.strum > 0);
        return;
    }
    nt[0] = key_note_of(k, t);
    notes_post(k, t, nt, 1, vel, 0);
}

void keys_up(int k)
{
    int i;
    if (kh[k].trk < 0)
        return;
    switch (kh[k].kind) {
    case 1:
        if (kh[k].notes[0] == 0xFF)
            dk_cmd(C_FILL, 0, 0, 0);
        else
            dk_cmd(C_PERFORM, kh[k].notes[0], 0, 0);
        break;
    case 2:
        if (!dk_song.trk[kh[k].trk].arp[3])
            dk_cmd(C_FINGER, kh[k].trk, 0, kh[k].notes[0]);
        break;
    case 3: dk_cmd(C_FVAR, kh[k].trk, 0, kh[k].notes[0]); break;
    default:
        for (i = 0; i < kh[k].n; i++)
            dk_note(kh[k].trk, kh[k].notes[i], 0, 0);
        break;
    }
    kh[k].trk = -1;
    kh[k].n = 0;
}

void keys_all_up(void)
{
    int k;
    for (k = 0; k < NKEYS; k++)
        keys_up(k);
}

/* the chord a white key's degree makes (for the screen, the step entry): its notes */
int keys_notes_of(int k, int *out)
{
    int i, n = kh[k].n;
    for (i = 0; i < n; i++)
        out[i] = kh[k].notes[i];
    return kh[k].kind == 0 ? n : 0;
}

/* the screen's roman numeral for a degree */
static const char *degree_name(int d, int minor)
{
    static const char *const UP[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const LO[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const BOR[4] = {"BOR 1", "BOR 2", "BOR 3", "BOR 4"};
    if (d >= 10)
        return BOR[(d - 10) & 3];
    return minor ? LO[d % 7] : UP[d % 7];
}
