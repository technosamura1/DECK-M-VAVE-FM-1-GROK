/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors; voice leading after AMB-1 (C) 2026 Charles Vestal */
/* The harmony's pure parts: scales, chords on a degree or a root, voicings and voice leading, names. Part of the
 * core's unit (core.c), used by the UI (the keys) and the core (DRIVE, ARP). */

const char *const DK_NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *const DK_MODE_NAME[DK_NMODES] = {"MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD",
                                             "HARM MIN", "MEL MIN", "PENTA MAJ", "PENTA MIN", "BLUES"};
#define P(a) (1u << (a))
const uint16_t DK_MODE_POOL[DK_NMODES] = {
    P(0) | P(2) | P(4) | P(5) | P(7) | P(9) | P(11), P(0) | P(2) | P(3) | P(5) | P(7) | P(8) | P(10),
    P(0) | P(2) | P(3) | P(5) | P(7) | P(9) | P(10), P(0) | P(1) | P(3) | P(5) | P(7) | P(8) | P(10),
    P(0) | P(2) | P(4) | P(6) | P(7) | P(9) | P(11), P(0) | P(2) | P(4) | P(5) | P(7) | P(9) | P(10),
    P(0) | P(2) | P(3) | P(5) | P(7) | P(8) | P(11), P(0) | P(2) | P(3) | P(5) | P(7) | P(9) | P(11),
    P(0) | P(2) | P(4) | P(7) | P(9),               P(0) | P(3) | P(5) | P(7) | P(10),
    P(0) | P(3) | P(5) | P(6) | P(7) | P(10)};
#undef P
const char *const DK_CHORD_NAME[DK_NCHORD] = {"TRIAD", "5", "SUS2", "SUS4", "6", "M6", "7", "MAJ7", "MIN7", "DOM7",
                                              "M7B5", "ADD9", "MADD9", "9", "MAJ9", "MIN9", "11", "MIN11", "13",
                                              "7SUS4", "QUARTAL", "OCT", "CLUSTER"};

int dk_scale(int mode, int *steps)
{
    int i, m = 0;
    uint32_t pool = DK_MODE_POOL[(unsigned)mode < DK_NMODES ? mode : 0] | 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int dk_scale_note(int root, int mode, int idx)
{
    int st[12], m = dk_scale(mode, st), o = idx >= 0 ? idx / m : -((-idx + m - 1) / m);
    return 12 * o + (root % 12) + st[idx - o * m];
}

int dk_note_index(int root, int mode, int note)
{
    int st[12], m = dk_scale(mode, st), r = note - root % 12, o = r >= 0 ? r / 12 : -((-r + 11) / 12), w = r - 12 * o;
    int k, c = 0;
    for (k = 0; k < m; k++)
        if (st[k] <= w)
            c = k;
    return o * m + c;
}

int dk_nearest(int from, int pc, int lo, int hi)
{
    int n = from + ((pc - from) % 12 + 12) % 12;
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

/* fixed chords: semitones from the root */
static const int8_t FIXED[DK_NCHORD][7] = {
    {0, 4, 7, -1},         {0, 7, -1},           {0, 2, 7, -1},         {0, 5, 7, -1},
    {0, 4, 7, 9, -1},      {0, 3, 7, 9, -1},     {0, 4, 7, 10, -1},     {0, 4, 7, 11, -1},
    {0, 3, 7, 10, -1},     {0, 4, 7, 10, -1},    {0, 3, 6, 10, -1},     {0, 4, 7, 14, -1},
    {0, 3, 7, 14, -1},     {0, 4, 7, 10, 14, -1}, {0, 4, 7, 11, 14, -1}, {0, 3, 7, 10, 14, -1},
    {0, 7, 10, 14, 17, -1}, {0, 3, 7, 10, 14, 17, -1}, {0, 4, 7, 10, 14, 21, -1}, {0, 5, 7, 10, -1},
    {0, 5, 10, 15, -1},    {0, 12, -1},          {0, 2, 4, -1}};
/* diatonic chords: scale steps above the degree (the type's shape, the scale's quality) */
static const int8_t DIAT[DK_NCHORD][7] = {
    {0, 2, 4, -1},          {0, 4, -1},            {0, 1, 4, -1},          {0, 3, 4, -1},
    {0, 2, 4, 5, -1},       {0, 2, 4, 5, -1},      {0, 2, 4, 6, -1},       {0, 2, 4, 6, -1},
    {0, 2, 4, 6, -1},       {0, 2, 4, 6, -1},      {0, 2, 4, 6, -1},       {0, 2, 4, 8, -1},
    {0, 2, 4, 8, -1},       {0, 2, 4, 6, 8, -1},   {0, 2, 4, 6, 8, -1},    {0, 2, 4, 6, 8, -1},
    {0, 4, 6, 8, 10, -1},   {0, 2, 4, 6, 8, 10, -1}, {0, 2, 4, 6, 8, 12, -1}, {0, 3, 4, 6, -1},
    {0, 3, 6, 9, -1},       {0, 7, -1},            {0, 1, 2, -1}};

static int diat_dim(int root, int mode, int deg)   /* its triad's fifth is diminished */
{
    int a = dk_scale_note(root, mode, deg), b = dk_scale_note(root, mode, deg + 4);
    return b - a == 6;
}

int dk_chord(int root, int mode, int deg, int type, int diatonic, int dimskip, int *notes, int base)
{
    int n = 0, i, st[12], m = dk_scale(mode, st);
    type = (unsigned)type < DK_NCHORD ? type : 0;
    if (diatonic) {
        int d0 = dk_note_index(root, mode, base), r;
        if (dimskip && m == 7 && diat_dim(root, mode, deg)) {
            static const int8_t TRY[4] = {-2, 2, 3, -3};
            for (i = 0; i < 4; i++)
                if (!diat_dim(root, mode, deg + TRY[i])) {
                    deg += TRY[i];
                    break;
                }
        }
        d0 = d0 - ((d0 % m) + m) % m;          /* the octave of base, in scale steps */
        r = d0 + deg;
        for (i = 0; i < 7 && DIAT[type][i] >= 0; i++) {
            int k = DIAT[type][i];
            if (m < 7)                         /* a scale of five or six notes: thirds are its every other step */
                k = k * m / 7;
            notes[n++] = dk_scale_note(root, mode, r + k);
        }
        if (type == CH_OCT)
            notes[1] = notes[0] + 12;
    } else {
        int r = base + ((deg - base) % 12 + 12) % 12;   /* deg: here the root's pitch class */
        for (i = 0; i < 7 && FIXED[type][i] >= 0; i++)
            notes[n++] = r + FIXED[type][i];
    }
    /* no two the same */
    for (i = 1; i < n; i++)
        if (notes[i] <= notes[i - 1])
            notes[i] = notes[i - 1] + 1;
    return n;
}

/* the voicing: CLOSE as built (or led), OPEN its every other note up an octave, SPREAD wider still, DROP2 the second
 * from the top an octave down, POWER root fifth octave. prev (n notes, or 0): the last chord, for leading */
int dk_voice(const int *ch, int n, int voicing, int bass, const int *prev, int *out)
{
    int i, k = 0, t[8] = {0};
    for (i = 0; i < n && i < 7; i++)
        t[i] = ch[i];
    if (prev && voicing == VC_CLOSE && n >= 3) {    /* the inversion nearest the last chord */
        int best = 1 << 30, inv, bi = 0, cand[8];
        for (inv = 0; inv < n; inv++) {
            int c = 0, j, s;
            for (j = 0; j < n; j++)
                cand[j] = t[(j + inv) % n] + (j + inv >= n ? 12 : 0);
            s = 0;
            while (cand[0] + s > prev[0] + 6)
                s -= 12;
            while (cand[0] + s < prev[0] - 6)
                s += 12;
            for (j = 0; j < n; j++)
                c += cand[j] + s > prev[j < 7 ? j : 6] ? cand[j] + s - prev[j] : prev[j] - cand[j] - s;
            if (c < best) {
                best = c;
                bi = inv;
            }
        }
        {
            int s = 0, j;
            for (j = 0; j < n; j++)
                cand[j] = t[(j + bi) % n] + (j + bi >= n ? 12 : 0);
            while (cand[0] + s > prev[0] + 6)
                s -= 12;
            while (cand[0] + s < prev[0] - 6)
                s += 12;
            for (j = 0; j < n; j++)
                t[j] = cand[j] + s;
        }
    }
    switch (voicing) {
    case VC_OPEN:
        for (i = 0; i < n; i++)
            out[k++] = t[i] + ((i & 1) ? 12 : 0);
        break;
    case VC_SPREAD:
        for (i = 0; i < n; i++)
            out[k++] = t[i] + (i == 0 ? -12 : (i & 1) ? 0 : 12);
        break;
    case VC_DROP2:
        for (i = 0; i < n; i++)
            out[k++] = t[i] - (n >= 3 && i == n - 2 ? 12 : 0);
        break;
    case VC_POWER:
        out[k++] = t[0];
        out[k++] = t[0] + 7;
        out[k++] = t[0] + 12;
        break;
    default:
        for (i = 0; i < n; i++)
            out[k++] = t[i];
        break;
    }
    if (bass) {
        int lo = out[0], j;
        for (j = 1; j < k; j++)
            if (out[j] < lo)
                lo = out[j];
        out[k++] = t[0] - 12 < lo - 3 ? t[0] - 12 : t[0] - 24;
    }
    /* sorted, low first */
    for (i = 1; i < k; i++) {
        int v = out[i], j = i - 1;
        while (j >= 0 && out[j] > v) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = v;
    }
    return k;
}

/* a chord's name from its notes: the root that explains them best */
static const struct {
    uint16_t iv;                         /* intervals present (bit i), the root's bit 0 implied */
    const char *q;
} QUAL[] = {
    {1u << 4 | 1u << 7 | 1u << 11 | 1u << 2 | 1u << 9, "MAJ13"}, {1u << 4 | 1u << 7 | 1u << 11 | 1u << 2, "MAJ9"},
    {1u << 3 | 1u << 7 | 1u << 10 | 1u << 2 | 1u << 5, "M11"},   {1u << 3 | 1u << 7 | 1u << 10 | 1u << 2, "M9"},
    {1u << 4 | 1u << 7 | 1u << 10 | 1u << 2 | 1u << 9, "13"},    {1u << 4 | 1u << 7 | 1u << 10 | 1u << 2, "9"},
    {1u << 7 | 1u << 10 | 1u << 2 | 1u << 5, "11"},             {1u << 4 | 1u << 7 | 1u << 11, "MAJ7"},
    {1u << 3 | 1u << 7 | 1u << 10, "M7"},                         {1u << 4 | 1u << 7 | 1u << 10, "7"},
    {1u << 3 | 1u << 6 | 1u << 10, "M7B5"},                       {1u << 5 | 1u << 7 | 1u << 10, "7SUS4"},
    {1u << 4 | 1u << 7 | 1u << 2, "ADD9"},                        {1u << 3 | 1u << 7 | 1u << 2, "MADD9"},
    {1u << 4 | 1u << 7 | 1u << 9, "6"},                           {1u << 3 | 1u << 7 | 1u << 9, "M6"},
    {1u << 4 | 1u << 7, ""},                                      {1u << 3 | 1u << 7, "M"},
    {1u << 3 | 1u << 6, "DIM"},                                   {1u << 4 | 1u << 8, "AUG"},
    {1u << 2 | 1u << 7, "SUS2"},                                  {1u << 5 | 1u << 7, "SUS4"},
    {1u << 4 | 1u << 11, "MAJ7"},                                 {1u << 3 | 1u << 10, "M7"},
    {1u << 4 | 1u << 10, "7"},                                    {1u << 4, ""},
    {1u << 3, "M"},                                               {1u << 7, "5"},
};
const char *dk_chord_name(const int *notes, int n, char *buf)
{
    uint32_t pcs = 0;
    int i, r, best = -1, bq = 0, bs = -100, lo = 1000, k = 0;
    const char *s;
    buf[0] = 0;
    if (n <= 0)
        return buf;
    for (i = 0; i < n; i++) {
        pcs |= 1u << (((notes[i] % 12) + 12) % 12);
        if (notes[i] < lo)
            lo = notes[i];
    }
    for (r = 0; r < 12; r++) {
        uint32_t iv = 0;
        unsigned q;
        if (!(pcs >> r & 1u))
            continue;
        for (i = 0; i < 12; i++)
            if (pcs >> i & 1u && i != r)
                iv |= 1u << ((i - r + 12) % 12);
        for (q = 0; q < sizeof QUAL / sizeof QUAL[0]; q++)
            if ((iv & QUAL[q].iv) == QUAL[q].iv) {
                int sc = 0, b;
                for (b = 0; b < 12; b++)
                    sc += (QUAL[q].iv >> b & 1u) ? 3 : (iv >> b & 1u) ? -2 : 0;
                if (r == ((lo % 12) + 12) % 12)
                    sc += 2;
                if (sc > bs) {
                    bs = sc;
                    best = r;
                    bq = (int)q;
                }
                break;
            }
    }
    if (best < 0) {
        best = ((lo % 12) + 12) % 12;
        s = DK_NOTE_NAME[best];
        while (*s)
            buf[k++] = *s++;
        buf[k] = 0;
        return buf;
    }
    s = DK_NOTE_NAME[best];
    while (*s)
        buf[k++] = *s++;
    s = QUAL[bq].q;
    while (*s && k < 10)
        buf[k++] = *s++;
    buf[k] = 0;
    return buf;
}

/* the scale degree whose chord best fits a set of pitch classes */
int dk_fit_degree(int root, int mode, uint32_t pcs, int *score)
{
    int st[12], m = dk_scale(mode, st), d, best = -1, bs = -100;
    for (d = 0; d < m; d++) {
        int t[7], n = dk_chord(root, mode, d, CH_7, 1, 0, t, 60), s = 0, i, hit = 0;
        uint32_t cm = 0;
        for (i = 0; i < n; i++)
            cm |= 1u << (t[i] % 12);
        for (i = 0; i < 12; i++)
            if (pcs >> i & 1u) {
                if (cm >> i & 1u) {
                    s += 2;
                    hit++;
                } else
                    s -= 2;
            }
        if (pcs >> (t[0] % 12) & 1u)
            s += 1;
        if (hit < 2)
            s -= 4;
        if (s > bs) {
            bs = s;
            best = d;
        }
    }
    if (score)
        *score = bs;
    return best;
}
