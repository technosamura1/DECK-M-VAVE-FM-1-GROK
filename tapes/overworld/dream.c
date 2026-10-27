/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* DREAM: OVERWORLD's improviser as DECK's DRIVE, a tick at a time for each track in DRIVE, by its role and the style:
 * C418 (rare phrases of two to four notes, a bar or two of rest, a chord now and then below), LOFI (a lazy beat, the
 * root and fifth, sevenths and ninths on the "and"), AMBIENT (a drone, a pad, a rare chord, the bells), CAMPFIRE
 * (wood and a shaker, a plucked bass, a kalimba's broken chords). ENERGY is how much it plays, VARIATION how far the
 * melody wanders. What you play it hears: your chord becomes its harmony (DECK), your line its answer a bar or two
 * later. Part of tape_dsp.c. */

#define T8 (DK_PPQ / 2)
enum { ST_C418, ST_LOFI, ST_AMBIENT, ST_CAMPFIRE, NSTYLE };

typedef struct {
    int8_t idx[8], dur[8], pos[8];
    int n, base;
    uint8_t role[4];
    int last_mel, lh_style, pad[4], pad_on;
    uint32_t heard_used;
} dream_t;
static dream_t dr[DK_NTRACK];

/* a cell's rhythm for a two-bar phrase, sparse to busy (eighths) */
typedef struct {
    int8_t pos, dur;
} cell_t;
static const cell_t CELLS[8][7] = {
    {{0, 4}, {4, 4}, {8, 8}, {-1, 0}},
    {{0, 2}, {2, 2}, {4, 4}, {-1, 0}},
    {{0, 3}, {3, 1}, {4, 4}, {10, 6}, {-1, 0}},
    {{2, 2}, {4, 2}, {6, 2}, {8, 8}, {-1, 0}},
    {{0, 1}, {1, 1}, {2, 2}, {4, 4}, {12, 4}, {-1, 0}},
    {{0, 2}, {2, 1}, {3, 1}, {4, 2}, {6, 2}, {8, 6}, {-1, 0}},
    {{1, 1}, {2, 1}, {3, 1}, {4, 4}, {10, 2}, {12, 4}, {-1, 0}},
    {{0, 2}, {2, 2}, {4, 1}, {5, 1}, {6, 2}, {8, 2}, {10, 6}},
};

static int irnd(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }
static int hvel(int v) { return v + irnd(13) - 6; }
static int jit(void) { return (int)(frnd() * 0.018f * DK_SR * 0.3f); }

static int snap_chord(const dk_harm_t *h, int note)
{
    int i, best = note, bd = 99;
    for (i = 0; i < h->n; i++) {
        int n = dk_nearest(note, h->pcs[i], 0, 127), d = n > note ? n - note : note - n;
        if (d < bd) {
            bd = d;
            best = n;
        }
    }
    return best;
}
static int sc_note(const dk_harm_t *h, int idx) { return dk_scale_note(h->root, h->mode, idx); }
static int sc_idx(const dk_harm_t *h, int note) { return dk_note_index(h->root, h->mode, note); }
static int nsteps_of(const dk_harm_t *h)
{
    int st[12];
    return dk_scale(h->mode, st);
}

static void motif_new(dream_t *m, const dk_harm_t *h, int energy, int var, int lo, int hi)
{
    int c = (energy * 5 + var * 3) / 100 + irnd(3) - 1, i, k, step = 0, ns = nsteps_of(h);
    c = c < 0 ? 0 : c > 7 ? 7 : c;
    m->base = sc_idx(h, snap_chord(h, m->last_mel + irnd(5) - 2));
    while (sc_note(h, m->base) < lo)
        m->base += ns;
    while (sc_note(h, m->base) > hi)
        m->base -= ns;
    for (i = 0, k = 0; i < 7 && CELLS[c][i].pos >= 0; i++, k++) {
        m->pos[k] = CELLS[c][i].pos;
        m->dur[k] = CELLS[c][i].dur;
    }
    m->n = k;
    for (i = 0; i < m->n; i++) {
        int r = irnd(100);
        m->idx[i] = (int8_t)step;
        if (r < 34)
            step += 1;
        else if (r < 68)
            step -= 1;
        else if (r < 80)
            step += 2;
        else if (r < 88)
            step -= 2;
        else if (r < 88 + var / 10)
            step += 3 + irnd(2);
        if (step > 5)
            step -= 2;
        if (step < -4)
            step += 2;
    }
}
static void phrase_plan(dream_t *m, int style, int var)
{
    static const int8_t PLANS[6][4] = {{1, 2, 1, 0}, {1, 0, 2, 0}, {0, 1, 2, 2}, {1, 2, 0, 0}, {1, 2, 1, 2}, {0, 1, 0, 2}};
    int i, p = irnd(6), keep = 30 + var * 7 / 10;
    if (style == ST_C418)                       /* C418: more of the phrases rest */
        keep -= 15;
    for (i = 0; i < 4; i++) {
        int r = PLANS[p][i];
        if (r && irnd(100) > keep)
            r = 0;
        m->role[i] = (uint8_t)r;
    }
}
static int answer_from_heard(dream_t *m, const dk_harm_t *h, int lo, int hi)
{
    int i, b, n = h->nheard < 7 ? h->nheard : 7, ns = nsteps_of(h);
    if (n < 3 || h->heard_at == m->heard_used)
        return 0;
    b = sc_idx(h, h->heard[0]);
    while (sc_note(h, b) < lo)
        b += ns;
    while (sc_note(h, b) > hi)
        b -= ns;
    m->base = b;
    for (i = 0; i < n; i++) {
        m->pos[i] = (int8_t)(i * 2);
        m->dur[i] = (int8_t)(i + 1 < n ? 2 : 4);
        m->idx[i] = (int8_t)(sc_idx(h, h->heard[i]) - sc_idx(h, h->heard[0]));
    }
    m->n = n;
    m->heard_used = h->heard_at;
    return 1;
}

static void melody(const dk_drive_t *d, const dk_harm_t *h, dream_t *m, int lo, int hi, int vel0)
{
    uint32_t pt = d->tick % (2u * DK_TPB);
    int pi = (int)(d->tick / (2u * DK_TPB)), role, i;
    if (pt == 0) {
        if ((pi & 3) == 0)
            phrase_plan(m, d->style, d->var);
        role = m->role[pi & 3];
        if (role == 1 || (role == 2 && m->n == 0))
            motif_new(m, h, d->energy, d->var, lo, hi);
        if (h->nheard >= 3 && d->tick - h->heard_at > (uint32_t)DK_TPB && irnd(100) < 75 &&
            answer_from_heard(m, h, lo, hi))
            m->role[pi & 3] = 3;               /* your line, answered */
    }
    role = m->role[pi & 3];
    if (!role || pt % T8 || d->breath)
        return;
    if (h->heard_at && d->tick - h->heard_at < (uint32_t)DK_TPB)   /* you are playing: it listens */
        return;
    for (i = 0; i < m->n; i++)
        if ((uint32_t)(m->pos[i] * T8) == pt) {
            int idx = m->base + m->idx[i], n, last = i == m->n - 1;
            if (role == 2 && last && irnd(2))
                idx += irnd(2) ? 1 : -1;
            n = sc_note(h, idx);
            if (m->dur[i] >= 4 || last)
                n = snap_chord(h, n);
            m->last_mel = n;
            /* the piano under its pedal: the notes ring on */
            dk_emit(d->trk, n, hvel(i == 0 ? vel0 + 8 : vel0), m->dur[i] * T8 + (d->style == ST_C418 ? DK_TPB : T8),
                    jit());
        }
}

static void chord_play(const dk_drive_t *d, const dk_harm_t *h, int lo, int vel, int dur, int roll)
{
    int i;
    for (i = 0; i < h->n; i++) {
        int n = dk_nearest(lo + 5, h->pcs[i], lo, lo + 14);
        if (i > 0 && n <= dk_nearest(lo + 5, h->pcs[0], lo, lo + 14))
            n += 12;
        dk_emit(d->trk, n, hvel(vel - i * 3), dur, i * roll + jit());
    }
}

static void lh(const dk_drive_t *d, const dk_harm_t *h, dream_t *m)
{
    uint32_t b = d->bar_tick;
    int root = dk_nearest(48, h->pcs[0], 41, 53), fifth = root + 7;
    int third = h->n > 1 ? dk_nearest(root + 15, h->pcs[1], root + 9, root + 19) : root + 12;
    int top = h->n > 2 ? dk_nearest(root + 19, h->pcs[h->n - 1], root + 12, root + 24) : root + 12;
    if (h->user)                               /* your chord: the left hand leaves it be */
        return;
    if (h->fresh)
        m->lh_style = d->energy < 25 ? 0 : d->energy < 50 ? 1 : d->energy < 75 ? 2 : 3;
    switch (m->lh_style) {
    case 0:
        if (h->fresh) {
            dk_emit(d->trk, root, hvel(60), 2 * DK_TPB, jit());
            dk_emit(d->trk, third, hvel(52), 2 * DK_TPB, 900 + jit());
            dk_emit(d->trk, top, hvel(48), 2 * DK_TPB, 2000 + jit());
        }
        break;
    case 1:
        if (b == 0)
            dk_emit(d->trk, root, hvel(62), 2 * DK_PPQ, jit());
        else if (b == 2 * DK_PPQ) {
            dk_emit(d->trk, third, hvel(52), 2 * DK_PPQ, jit());
            dk_emit(d->trk, top, hvel(48), 2 * DK_PPQ, 600 + jit());
        }
        break;
    case 2: {
        static const int8_t W[4] = {0, 1, 2, 1};
        int nn[3] = {root, fifth, third};
        if (b % DK_PPQ == 0)
            dk_emit(d->trk, nn[W[b / DK_PPQ]], hvel(b == 0 ? 62 : 50), DK_PPQ * 2, jit());
        break;
    }
    default: {
        static const int8_t W[8] = {0, 1, 2, 3, 2, 1, 2, 3};
        int nn[4] = {root, fifth, third, top}, e = (int)(b / T8);
        if (b % T8 == 0 && (e == 0 || irnd(100) < 50 + d->energy / 2))
            dk_emit(d->trk, nn[W[e]], hvel(e == 0 ? 62 : 46), T8 * 3, jit());
        break;
    }
    }
}

static void drums(const dk_drive_t *d)
{
    static const int8_t LOFI_OF[6] = {1, 4, 6, 8, 10, 12}, CAMP_OF[3] = {2, 3, 5};
    int pat, step, lane;
    if (d->tick % DK_T16 || d->breath)
        return;
    switch (d->style) {
    case ST_LOFI: pat = LOFI_OF[d->energy * 5 / 100]; break;
    case ST_CAMPFIRE: pat = CAMP_OF[d->energy * 2 / 100]; break;
    default:                                   /* C418, AMBIENT: soft, ENERGY from a shaker to a lazy beat */
        if (d->energy < 10)
            return;
        pat = d->energy < 35 ? 0 : d->energy < 60 ? 1 : d->energy < 80 ? 3 : 4;
        break;
    }
    step = (int)(d->bar_tick / DK_T16);
    if (d->fill && step >= 12)
        pat = 13 + (int)((d->tick / DK_TPB) % 3u);
    for (lane = 0; lane < 8; lane++) {
        int c = dk_fch(FD_SRC[pat][lane + 1], step), v = c >= '1' && c <= '9' ? c - '0' : 0;
        if (v)
            dk_emit(d->trk, KIT_NOTE[lane], hvel(v * 13), DK_T16, jit() / 2 + ((step & 1) && d->style == ST_LOFI ? (int)(0.16f * dk_ctx.spb * 0.25f) : 0));
    }
}

static void bass(const dk_drive_t *d, const dk_harm_t *h)
{
    uint32_t b = d->bar_tick;
    int root = dk_nearest(40, h->pcs[0], 33, 45), fifth = root + 7 > 47 ? root - 5 : root + 7;
    if (d->breath)
        return;
    switch (d->style) {
    case ST_LOFI:
        if (b == 0)
            dk_emit(d->trk, root, hvel(96), DK_PPQ + T8, jit());
        else if (b == 2 * DK_PPQ + T8 && d->energy > 30)
            dk_emit(d->trk, irnd(3) ? fifth : root + 12, hvel(78), T8, jit());
        else if (b == 3 * DK_PPQ + T8 && d->energy > 60 && irnd(100) < 50)   /* a walk to the next */
            dk_emit(d->trk, root + (irnd(2) ? 2 : -1), hvel(70), T8, jit());
        break;
    case ST_CAMPFIRE:
        if (b == 0 || (b == 2 * DK_PPQ && d->energy > 25))
            dk_emit(d->trk, b == 0 ? root : fifth, hvel(88), DK_PPQ, jit());
        break;
    case ST_AMBIENT:
        if (h->fresh && (d->tick / DK_TPB) % 2u == 0)
            dk_emit(d->trk, root, hvel(72), 4 * DK_TPB, 0);
        break;
    default:                                    /* C418: rare, long */
        if (h->fresh && irnd(100) < 40 + d->energy / 2)
            dk_emit(d->trk, root, hvel(76), 2 * DK_TPB - T8, jit());
        break;
    }
}

static void pad(const dk_drive_t *d, const dk_harm_t *h, dream_t *m)
{
    int i, to[8], pcs[4], lead[4];
    if (!h->fresh || d->breath)
        return;
    if (d->style == ST_LOFI && d->energy < 30)
        return;
    if (!m->pad_on) {
        m->pad[0] = 55;
        m->pad[1] = 59;
        m->pad[2] = 62;
        m->pad[3] = 66;
        m->pad_on = 1;
    }
    for (i = 0; i < 4; i++) {
        pcs[i] = 48 + h->pcs[i < h->n ? i : 0];
        lead[i] = m->pad[i];
    }
    dk_voice(pcs, h->n < 4 ? h->n : 4, VC_CLOSE, 0, lead, to);
    for (i = 0; i < (h->n < 4 ? h->n : 4); i++) {
        while (to[i] < 50)
            to[i] += 12;
        while (to[i] > 74)
            to[i] -= 12;
        m->pad[i] = to[i];
        dk_emit(d->trk, to[i], 70, (TAPE.style_bars[d->style] * DK_TPB) - 8, i * 700);
    }
}

static void arp(const dk_drive_t *d, const dk_harm_t *h)
{
    static const int8_t UP[8] = {0, 1, 2, 3, 2, 1, 4, 2};
    uint32_t b = d->bar_tick;
    int div = d->style == ST_AMBIENT ? 2 * DK_PPQ : d->style == ST_C418 ? DK_PPQ : T8, e, tone, note;
    if (b % (uint32_t)div || d->breath)
        return;
    e = (int)(b / (uint32_t)div);
    if (e && irnd(100) > 35 + d->energy * 65 / 100)
        return;
    tone = UP[e & 7];
    note = dk_nearest(64 + (tone >= h->n ? 12 : 0), h->pcs[tone % h->n], 57, 86);
    dk_emit(d->trk, note, hvel(e == 0 ? 74 : 60), div * 2, jit());
}

static void bells(const dk_drive_t *d, const dk_harm_t *h)
{
    int chance;
    if (d->bar_tick || d->breath)
        return;
    chance = d->style == ST_AMBIENT ? 45 : d->style == ST_C418 ? 22 : d->style == ST_CAMPFIRE ? 12 : 6;
    chance += d->energy / 5;
    if (h->nheard && d->tick - h->heard_at > (uint32_t)DK_TPB && d->tick - h->heard_at < 2u * DK_TPB)
        chance += 40;                           /* an answer to you */
    if (irnd(100) < chance) {
        int n = snap_chord(h, 84 + irnd(9));
        dk_emit(d->trk, n, hvel(64), 6 * T8, (int)(frnd() * 1.6f * DK_SR));
        if (d->style == ST_AMBIENT && irnd(3) == 0)
            dk_emit(d->trk, snap_chord(h, n + 3 + irnd(5)), hvel(52), 6 * T8, (int)((0.8f + frnd()) * DK_SR));
    }
}

static void keys(const dk_drive_t *d, const dk_harm_t *h, dream_t *m)
{
    uint32_t b = d->bar_tick;
    if (d->breath)
        return;
    switch (d->style) {
    case ST_LOFI: {
        static const uint16_t HIT[4] = {T8 + DK_PPQ, 2 * DK_PPQ + T8, T8, 3 * DK_PPQ + T8};
        int i, hits = 1 + d->energy * 3 / 100;
        for (i = 0; i < hits && i < 4; i++)
            if (b == HIT[i])
                chord_play(d, h, 55, i == 0 ? 72 : 60, i == 0 ? DK_PPQ + T8 : T8 + 12, 450);
        if (d->var > 50)
            melody(d, h, m, 67, 84, 58);
        break;
    }
    case ST_AMBIENT:
        if (h->fresh && irnd(100) < 50 + d->energy / 2)
            chord_play(d, h, 52, 56, 2 * DK_TPB, 1800);
        if (d->var > 40)
            melody(d, h, m, 69, 86, 46);
        break;
    case ST_CAMPFIRE:
        arp(d, h);
        break;
    default:                                    /* C418 */
        melody(d, h, m, 62, 84, 62);
        if (d->energy > 15 || h->fresh)
            lh(d, h, m);
        break;
    }
}

static void tape_drive(const dk_drive_t *d, const dk_harm_t *h)
{
    dream_t *m = &dr[d->trk % DK_NTRACK];
    if (!m->last_mel)
        m->last_mel = 72;
    switch (d->role) {
    case R_DRUM: drums(d); break;
    case R_BASS: bass(d, h); break;
    case R_PAD: pad(d, h, m); break;
    case R_ARP: if (d->style == ST_LOFI) keys(d, h, m); else arp(d, h); break;
    case R_BELL: bells(d, h); break;
    case R_LEAD: if (!d->breath) melody(d, h, m, 69, 91, 66); break;
    case R_NATURE:
        if (d->tick % (8u * DK_TPB) == 0)
            dk_emit(d->trk, 60, 100, 9 * DK_TPB, 0);
        break;
    default: keys(d, h, m); break;
    }
}
