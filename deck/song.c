/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The song's data, edited from the main loop (the render only reads it, between dk_lock and dk_unlock): the events
 * (sorted by clip, then by position), the tracks' engines and presets, the clips' operations, undo and redo (a
 * clip's or the road's snapshot), the recording's events put in place, the templates. Shared by the app (unity
 * build) and the tests. */
#include "song.h"

dk_user_t dk_user;                       /* the user's presets: 4 an engine */
static dk_ev_t ubuf[1024] DK_POOL;       /* undo: snapshots back to back (entries below) */
static dk_ev_t rbuf[512] DK_POOL;        /* redo */
static dk_ev_t clipboard[384] DK_POOL;
static int clip_n = -1, clipb_len;
typedef struct {
    uint8_t kind, clip;                  /* 0 a clip, 1 the road */
    uint16_t n, len, at;                 /* events, the clip's length; where in the buffer */
    uint8_t layers, nroad;
    uint8_t grp;                         /* snapshots of one edit over several clips: undone together (0 alone) */
    dk_block_t road[DK_NROAD];
} snap_t;
static uint8_t grp_now, grp_seq;
#define NSNAP 24
static snap_t usnap[NSNAP], rsnap[8];
static int un, rn;                       /* entries */
uint32_t song_edits;                     /* changes (autosave, the screen) */

/* ------------------------------------------------------------------ events --- */
static void reindex(void)
{
    int c, at = 0;
    for (c = 0; c < DK_NCLIP; c++) {
        dk_song.clip[c].at = (uint16_t)at;
        at += dk_song.clip[c].n;
    }
    dk_song.nev = (uint16_t)at;
}

int dk_song_insert(int clip, const dk_ev_t *e)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int i, p, k;
    if ((unsigned)clip >= DK_NCLIP || dk_song.nev >= DK_NEV)
        return -1;
    p = c->at;
    for (k = 0; k < c->n && (dk_ev[p].pos < e->pos || (dk_ev[p].pos == e->pos && !(e->fl & EV_MOTION))); k++)
        p++;
    for (i = dk_song.nev; i > p; i--)
        dk_ev[i] = dk_ev[i - 1];
    dk_ev[p] = *e;
    c->n++;
    for (k = clip + 1; k < DK_NCLIP; k++)
        dk_song.clip[k].at++;
    dk_song.nev++;
    if (DK_L_TAKE(e->layer) > c->layers)
        c->layers = (uint8_t)DK_L_TAKE(e->layer);
    song_edits++;
    return p - c->at;
}

void ev_delete(int clip, int k)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int i, p = c->at + k;
    if (k < 0 || k >= c->n)
        return;
    for (i = p; i < dk_song.nev - 1; i++)
        dk_ev[i] = dk_ev[i + 1];
    c->n--;
    for (i = clip + 1; i < DK_NCLIP; i++)
        dk_song.clip[i].at--;
    dk_song.nev--;
    song_edits++;
}

/* a clip's events replaced (n of them, sorted) */
static int clip_put(int clip, const dk_ev_t *src, int n)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int d = n - c->n, i, p = c->at;
    if (dk_song.nev + d > DK_NEV)
        n = c->n + (DK_NEV - dk_song.nev), d = n - c->n;
    if (d > 0)
        for (i = dk_song.nev - 1 + d; i >= p + n; i--)
            dk_ev[i] = dk_ev[i - d];
    else if (d < 0)
        for (i = p + n; i < dk_song.nev + d; i++)
            dk_ev[i] = dk_ev[i - d];
    for (i = 0; i < n; i++)
        dk_ev[p + i] = src[i];
    c->n = (uint16_t)n;
    reindex();
    song_edits++;
    return n;
}

void clip_clear(int clip)
{
    clip_put(clip, 0, 0);
    dk_song.clip[clip].layers = 0;
}

/* the events sorted by position (after an edit that moved them) */
static void clip_sort(int clip)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int i, j;
    for (i = 1; i < c->n; i++) {
        dk_ev_t v = dk_ev[c->at + i];
        j = i - 1;
        while (j >= 0 && dk_ev[c->at + j].pos > v.pos) {
            dk_ev[c->at + j + 1] = dk_ev[c->at + j];
            j--;
        }
        dk_ev[c->at + j + 1] = v;
    }
}

int clip_len(int clip)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int b = dk_song.part[clip / DK_NTRACK].bars;
    return c->len ? c->len : (b < 1 ? 1 : b) * DK_TPB;
}

/* the event at a step (a sixteenth) of a note, or -1 */
int clip_find(int clip, int pos, int note, int tol)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (!(e->fl & EV_MOTION) && (note < 0 || e->note == note) && e->pos >= pos - tol && e->pos <= pos + tol)
            return i;
    }
    return -1;
}

/* ------------------------------------------------------------------- undo --- */
static int snap_take(snap_t *tab, int *cnt, int max, dk_ev_t *buf, int bufn, int clip, int road)
{
    snap_t *s;
    int need = road ? 0 : dk_song.clip[clip].n, used = 0, i;
    if (*cnt > 0)
        used = tab[*cnt - 1].at + tab[*cnt - 1].n;
    while (*cnt > 0 && (*cnt >= max || used + need > bufn)) {   /* full: the oldest forgotten */
        int drop = tab[0].n;
        for (i = 0; i < used - drop; i++)
            buf[i] = buf[i + drop];
        for (i = 1; i < *cnt; i++) {
            tab[i - 1] = tab[i];
            tab[i - 1].at = (uint16_t)(tab[i - 1].at - drop);
        }
        (*cnt)--;
        used -= drop;
    }
    if (need > bufn)
        return -1;
    s = &tab[(*cnt)++];
    s->kind = (uint8_t)(road ? 1 : 0);
    s->clip = (uint8_t)clip;
    s->at = (uint16_t)used;
    s->n = (uint16_t)need;
    if (road) {
        s->nroad = dk_song.nroad;
        for (i = 0; i < DK_NROAD; i++)
            s->road[i] = dk_song.road[i];
    } else {
        s->len = dk_song.clip[clip].len;
        s->layers = dk_song.clip[clip].layers;
        for (i = 0; i < need; i++)
            buf[used + i] = dk_ev[dk_song.clip[clip].at + i];
    }
    return 0;
}
static void snap_restore(const snap_t *s, const dk_ev_t *buf)
{
    int i;
    if (s->kind) {
        dk_song.nroad = s->nroad;
        for (i = 0; i < DK_NROAD; i++)
            dk_song.road[i] = s->road[i];
    } else {
        clip_put(s->clip, buf + s->at, s->n);
        dk_song.clip[s->clip].len = s->len;
        dk_song.clip[s->clip].layers = s->layers;
    }
    song_edits++;
}
void undo_push(int clip)
{
    if (snap_take(usnap, &un, NSNAP, ubuf, 1024, clip, 0) == 0)
        usnap[un - 1].grp = grp_now;
    rn = 0;
}
void undo_group(int on)                  /* the pushes until undo_group(0) are one undo step */
{
    if (on) {
        if (!++grp_seq)
            grp_seq = 1;
        grp_now = grp_seq;
    } else
        grp_now = 0;
}
void undo_push_road(void) { snap_take(usnap, &un, NSNAP, ubuf, 1024, 0, 1), rn = 0; }
int undo_count(void) { return un; }
/* 1 = done; what: the clip (or -1 the road) */
int song_undo(int *what)
{
    snap_t s;
    uint8_t g;
    if (!un)
        return 0;
    do {                                 /* (a group: all of it) */
        s = usnap[un - 1];
        g = s.grp;
        dk_lock();
        if (snap_take(rsnap, &rn, 8, rbuf, 512, s.clip, s.kind) == 0)
            rsnap[rn - 1].grp = g;
        snap_restore(&s, ubuf);
        dk_unlock();
        un--;
    } while (g && un && usnap[un - 1].grp == g);
    *what = s.kind ? -1 : s.clip;
    return 1;
}
int song_redo(int *what)
{
    snap_t s;
    int keep;
    uint8_t g;
    if (!rn)
        return 0;
    do {
        s = rsnap[rn - 1];
        g = s.grp;
        dk_lock();
        keep = rn;
        if (snap_take(usnap, &un, NSNAP, ubuf, 1024, s.clip, s.kind) == 0)
            usnap[un - 1].grp = g;
        rn = keep;
        snap_restore(&s, rbuf);
        dk_unlock();
        rn--;
    } while (g && rn && rsnap[rn - 1].grp == g);
    *what = s.kind ? -1 : s.clip;
    return 1;
}

/* --------------------------------------------------------- the recording --- */
static uint32_t rec_r;
static uint8_t rec_snapped_pass = 0xFF, rec_snapped_clip = 0xFF;
/* the render's recorded events into the song (a snapshot first for each new pass: its undo) */
int rec_flush(void)
{
    uint32_t w = dk_recq_w;
    int n = 0;
    if (w - rec_r > DK_RECQ)
        rec_r = w - DK_RECQ;
    if (rec_r == w)
        return 0;
    dk_lock();
    while (rec_r != w) {
        dk_recev_t r;
        r.clip = dk_recq[rec_r % DK_RECQ].clip;
        r.kind = dk_recq[rec_r % DK_RECQ].kind;
        r.pass = dk_recq[rec_r % DK_RECQ].pass;
        r.ev.pos = dk_recq[rec_r % DK_RECQ].ev.pos;
        r.ev.dur = dk_recq[rec_r % DK_RECQ].ev.dur;
        r.ev.note = dk_recq[rec_r % DK_RECQ].ev.note;
        r.ev.vel = dk_recq[rec_r % DK_RECQ].ev.vel;
        r.ev.fl = dk_recq[rec_r % DK_RECQ].ev.fl;
        r.ev.layer = dk_recq[rec_r % DK_RECQ].ev.layer;
        rec_r++;
        if (r.clip >= DK_NCLIP)
            continue;
        if (r.pass != rec_snapped_pass || r.clip != rec_snapped_clip) {
            snap_take(usnap, &un, NSNAP, ubuf, 1024, r.clip, 0);
            rn = 0;
            rec_snapped_pass = r.pass;
            rec_snapped_clip = r.clip;
        }
        if (r.kind == 1) {                 /* ENDLESS: the clip's length */
            dk_song.clip[r.clip].len = r.ev.pos;
            continue;
        }
        {
            /* played again this pass only if it lies ahead of the clip's time now (else it waits a turn) */
            int len = clip_len(r.clip), now = dk_part_tick % (len ? len : 1);
            if (r.pass == dk_pass && dk_recording && r.ev.pos > now)
                r.ev.fl |= EV_FRESH;
        }
        if (dk_song.clip[r.clip].len == 0 && r.ev.pos + r.ev.dur > clip_len(r.clip))
            r.ev.dur = (uint16_t)(clip_len(r.clip) - r.ev.pos);
        dk_song_insert(r.clip, &r.ev);
        n++;
    }
    dk_unlock();
    return n;
}
void rec_reset(void)
{
    rec_r = dk_recq_w;
    rec_snapped_pass = 0xFF;
}

/* ------------------------------------------------------------- clip edits --- */
/* each with its undo; the song locked */
void clip_copy(int clip)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i;
    clip_n = c->n > 384 ? 384 : c->n;
    for (i = 0; i < clip_n; i++)
        clipboard[i] = dk_ev[c->at + i];
    clipb_len = clip_len_raw(clip);
}
int clip_len_raw(int clip) { return dk_song.clip[clip].len; }
int clip_paste(int clip)
{
    if (clip_n < 0)
        return 0;
    undo_push(clip);
    dk_lock();
    clip_put(clip, clipboard, clip_n);
    dk_song.clip[clip].len = (uint16_t)clipb_len;
    dk_unlock();
    return 1;
}
void clip_double(int clip)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int len = clip_len(clip), n = c->n, i;
    static dk_ev_t tmp[384];
    if (n > 192 || len * 2 > 16 * DK_TPB)
        return;
    undo_push(clip);
    for (i = 0; i < n; i++) {
        tmp[i] = dk_ev[c->at + i];
        tmp[n + i] = tmp[i];
        tmp[n + i].pos = (uint16_t)(tmp[i].pos + len);
    }
    dk_lock();
    clip_put(clip, tmp, 2 * n);
    dk_song.clip[clip].len = (uint16_t)(len * 2);
    dk_unlock();
}
/* by scale steps (or semitones for a chromatic keyboard); _nu: without its undo step (the caller's) */
static void clip_transpose_nu(int clip, int steps);
void clip_transpose(int clip, int steps)
{
    undo_push(clip);
    clip_transpose_nu(clip, steps);
}
static void clip_transpose_nu(int clip, int steps)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i;
    dk_lock();
    for (i = 0; i < c->n; i++) {
        dk_ev_t *e = &dk_ev[c->at + i];
        int n;
        if (e->fl & EV_MOTION)
            continue;
        if (dk_song.layout)
            n = dk_scale_note(dk_song.root, dk_song.mode, dk_note_index(dk_song.root, dk_song.mode, e->note) + steps);
        else
            n = e->note + steps;
        if (n >= 0 && n < 128)
            e->note = (uint8_t)n;
    }
    dk_unlock();
    song_edits++;
}
/* VARIATE: a few notes move a step in the scale (or to a neighbour beat), the harmony kept */
void clip_variate(int clip, uint32_t seed)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, len = clip_len(clip);
    undo_push(clip);
    dk_lock();
    for (i = 0; i < c->n; i++) {
        dk_ev_t *e = &dk_ev[c->at + i];
        seed = seed * 1664525u + 1013904223u;
        if (e->fl & EV_MOTION || (seed >> 24) > 70)
            continue;
        if ((seed >> 8 & 3u) == 0 && e->dur >= DK_T16) {
            int p = e->pos + ((seed >> 10 & 1u) ? DK_T16 : -DK_T16);
            if (p >= 0 && p < len)
                e->pos = (uint16_t)p;
        } else {
            int idx = dk_note_index(dk_song.root, dk_song.mode, e->note) + ((seed >> 12 & 1u) ? 1 : -1);
            e->note = (uint8_t)dk_scale_note(dk_song.root, dk_song.mode, idx);
        }
    }
    clip_sort(clip);
    dk_unlock();
    song_edits++;
}
void clip_humanize(int clip, uint32_t seed)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, len = clip_len(clip);
    undo_push(clip);
    dk_lock();
    for (i = 0; i < c->n; i++) {
        dk_ev_t *e = &dk_ev[c->at + i];
        int p, v;
        seed = seed * 1664525u + 1013904223u;
        if (e->fl & EV_MOTION)
            continue;
        p = e->pos + (int)(seed >> 29) - 3;
        v = e->vel + (int)(seed >> 12 & 15u) - 8;
        e->pos = (uint16_t)(p < 0 ? 0 : p >= len ? len - 1 : p);
        e->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
    }
    clip_sort(clip);
    dk_unlock();
    song_edits++;
}

/* COOK: the clip a little different each time, staying in the key: some notes a scale step up or down, the dynamics
 * breathing, a note a sixteenth longer or shorter; on drums the hits' strength, one off the beat moved, now and then
 * a ghost. Each one is an undo step (SEQ + OCT- walks back to where it began) */
static uint32_t ck_rnd(uint32_t *s) { return (*s = *s * 1664525u + 1013904223u) >> 8; }
static int scale_move(int note, int steps)
{
    int idx = dk_note_index(dk_song.root, dk_song.mode, note) + steps, n = dk_scale_note(dk_song.root, dk_song.mode, idx);
    return n < 24 ? note : n > 108 ? note : n;
}
void clip_cook(int clip, uint32_t seed, const dk_engine_t *e)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, len = clip_len(clip), drum = (e->flags & EF_DRUM) != 0, ghosts = 0;
    undo_push(clip);
    dk_lock();
    for (i = 0; i < c->n; i++) {
        dk_ev_t *ev = &dk_ev[c->at + i];
        int v = ev->vel + (int)(ck_rnd(&seed) % 25u) - 12, r = (int)(ck_rnd(&seed) % 100u);
        if (ev->fl & EV_MOTION)
            continue;
        ev->vel = (uint8_t)(v < 20 ? 20 : v > 127 ? 127 : v);
        if (drum) {
            if (r < 10 && ev->pos % DK_PPQ) {          /* off the beat: a sixteenth this way or that */
                int p = ev->pos + (ck_rnd(&seed) & 1u ? DK_T16 : -DK_T16);
                if (p >= 0 && p < len && clip_find(clip, p, ev->note, 0) < 0)
                    ev->pos = (uint16_t)p;
            }
            continue;
        }
        {
            int same = (i > 0 && dk_ev[c->at + i - 1].pos == ev->pos) ||
                       (i + 1 < c->n && dk_ev[c->at + i + 1].pos == ev->pos);   /* a chord's note */
            if (r < (same ? 12 : 28))
                ev->note = (uint8_t)scale_move(ev->note, r < 4 ? (r & 1 ? 2 : -2) : (r & 1 ? 1 : -1));
            else if (r > 90 && ev->dur >= 2 * DK_T16)
                ev->dur = (uint16_t)(ev->dur + (r & 1 ? DK_T16 : -DK_T16));
        }
    }
    clip_sort(clip);
    if (drum && e->nlanes > 1 && ck_rnd(&seed) % 100u < 45u)   /* now and then a ghost note */
        for (i = 0; i < 8 && ghosts < 1; i++) {
            dk_ev_t g;
            int l = 1 + (int)(ck_rnd(&seed) % (uint32_t)(e->nlanes - 1)), st = (int)(ck_rnd(&seed) % (uint32_t)(len / DK_T16));
            if (clip_find(clip, st * DK_T16, e->lane_note[l], 0) >= 0 || !(st & 1))
                continue;
            g.pos = (uint16_t)(st * DK_T16);
            g.dur = DK_T16;
            g.note = e->lane_note[l];
            g.vel = (uint8_t)(32 + ck_rnd(&seed) % 18u);
            g.fl = 0;
            g.layer = 0;
            dk_song_insert(clip, &g);
            ghosts++;
        }
    dk_unlock();
    song_edits++;
}
/* RANDOM: melodic, the same rhythm with new notes of the key (a chord moved whole, so it stays a chord); drums, each
 * sound as many hits as it had, scattered anew (the kick leaning to the beats) */
void clip_random(int clip, uint32_t seed, const dk_engine_t *e)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, len = clip_len(clip);
    undo_push(clip);
    dk_lock();
    if (e->flags & EF_DRUM) {
        int cnt[8] = {0}, l, steps = len / DK_T16;
        for (i = 0; i < c->n; i++)
            for (l = 0; l < e->nlanes && l < 8; l++)
                if (!(dk_ev[c->at + i].fl & EV_MOTION) && dk_ev[c->at + i].note == e->lane_note[l])
                    cnt[l]++;
        clip_clear(clip);
        for (l = 0; l < e->nlanes && l < 8; l++) {
            int tries = 0;
            if (!cnt[l] && l < 3)
                cnt[l] = l == 0 ? steps / 4 : l == 1 ? steps / 8 : steps / 2;   /* an empty clip: a beat to start from */
            while (cnt[l] > 0 && tries++ < 200) {
                int st = (int)(ck_rnd(&seed) % (uint32_t)steps);
                dk_ev_t g;
                if (l == 0 && (st & 3) && ck_rnd(&seed) % 100u < 60u)
                    st &= ~3;                    /* the kick: mostly on the beat */
                if (clip_find(clip, st * DK_T16, e->lane_note[l], 0) >= 0)
                    continue;
                g.pos = (uint16_t)(st * DK_T16);
                g.dur = DK_T16;
                g.note = e->lane_note[l];
                g.vel = (uint8_t)(st % 4 ? 60 + ck_rnd(&seed) % 30u : 90 + ck_rnd(&seed) % 30u);
                g.fl = 0;
                g.layer = 0;
                dk_song_insert(clip, &g);
                cnt[l]--;
            }
        }
    } else {
        int lo = 127, hi = 0, prev_pos = -1, shift = 0;
        for (i = 0; i < c->n; i++)
            if (!(dk_ev[c->at + i].fl & EV_MOTION)) {
                lo = dk_ev[c->at + i].note < lo ? dk_ev[c->at + i].note : lo;
                hi = dk_ev[c->at + i].note > hi ? dk_ev[c->at + i].note : hi;
            }
        for (i = 0; i < c->n; i++) {
            dk_ev_t *ev = &dk_ev[c->at + i];
            int n2, guard = 0;
            if (ev->fl & EV_MOTION)
                continue;
            if (ev->pos != prev_pos)             /* a new moment: a new move (a chord's notes share it) */
                shift = (int)(ck_rnd(&seed) % 9u) - 4;
            prev_pos = ev->pos;
            do
                n2 = scale_move(ev->note, shift);
            while ((n2 < lo - 5 || n2 > hi + 5) && (shift = -shift / 2, ++guard < 4));
            ev->note = (uint8_t)n2;
            ev->vel = (uint8_t)(55 + ck_rnd(&seed) % 50u);
        }
    }
    clip_sort(clip);
    dk_unlock();
    song_edits++;
}

/* ------------------------------------------------------------- the tracks --- */
const dk_preset_t *preset_of(int eng, int slot, dk_preset_t *tmp)
{
    if (slot < DK_NFACT)
        return &TAPE.presets[eng * DK_NFACT + slot];
    {
        const dk_upreset_t *u = &dk_user.p[eng % DK_UENG][(slot - DK_NFACT) & 3];
        int i;
        if (!u->used)
            return &TAPE.presets[eng * DK_NFACT];
        tmp->name = u->name;
        tmp->eng = (uint8_t)eng;
        for (i = 0; i < DK_NP; i++)
            tmp->p[i] = u->p[i];
        for (i = 0; i < 3; i++) {
            tmp->env[i] = u->env[i];
            tmp->mt[i] = u->mt[i];
            tmp->ma[i] = u->ma[i];
        }
        tmp->fx = u->fx;
        tmp->fxamt = u->fxamt;
        tmp->fxa = u->fxa;
        tmp->fxb = u->fxb;
        tmp->rev = u->rev;
        tmp->echo = u->echo;
        tmp->vol = u->trim;
        return tmp;
    }
}

void track_preset(int t, int slot)
{
    dk_track_t *k = &dk_song.trk[t];
    dk_preset_t tmp;
    const dk_preset_t *p = preset_of(k->eng, slot, &tmp);
    const dk_engine_t *e = TAPE.eng[k->eng];
    int i;
    k->preset = (uint8_t)slot;
    for (i = 0; i < DK_NP; i++)
        k->p[i] = (int8_t)(i < e->nparams ? p->p[i] : 0);
    for (i = 0; i < 3; i++) {
        k->env[i] = p->env[i] ? p->env[i] : 50;
        k->mt[i] = p->mt[i];
        k->ma[i] = p->ma[i];
    }
    k->fx = p->fx;
    k->fxamt = (int8_t)p->fxamt;
    k->fxa = (int8_t)p->fxa;
    k->fxb = (int8_t)p->fxb;
    k->rev = p->rev;
    k->echo = p->echo;
    k->trim = p->vol;
    k->macro = 0;
    song_edits++;
}

void dk_track_setup(int t, int eng, int preset, int seq, int role)
{
    dk_track_t *k = &dk_song.trk[t];
    k->eng = (uint8_t)(eng % TAPE.neng);
    k->seq = (uint8_t)seq;
    k->role = (uint8_t)role;
    track_preset(t, preset);
}

static void track_defaults(int t)
{
    dk_track_t *k = &dk_song.trk[t];
    int i;
    for (i = 0; i < (int)sizeof *k; i++)
        ((uint8_t *)k)[i] = 0;
    k->vol = 72;
    k->pan = (int8_t)(t == 2 ? -12 : t == 1 ? 14 : 0);   /* the pad a little left, the keys right */
    k->midi_ch = (uint8_t)(t + 1);
    k->drift = 35;
    k->drv[0] = 45;
    k->drv[1] = dk_song.style;
    k->drv[2] = 50;
    k->arp[0] = 0;
    k->arp[1] = 3;                      /* 1/16 */
    k->arp[2] = 1;
    k->tmb[0] = 60;
    k->tmb[1] = 45;
    k->tmb[2] = 70;
    k->tmb[3] = 2;
    k->div = 1;
    k->qnt = 60;
    for (i = 0; i < 2; i++) {
        k->mod[i][1] = 30;
        k->mod[i][2] = 40;
        k->mod[i][3] = T_P0;
    }
    k->oct = (int8_t)(t == 1 ? -2 : t == 4 ? 1 : 0);
    dk_track_setup(t, TAPE.def_eng[t], TAPE.def_preset[t], TAPE.def_seq[t], TAPE.def_role[t]);
}

void dk_song_clear(void)
{
    int i, t;
    uint8_t *b = (uint8_t *)&dk_song;
    for (i = 0; i < (int)sizeof dk_song; i++)
        b[i] = 0;
    dk_song.magic = DK_MAGIC;
    dk_song.format = DK_FORMAT;
    dk_song.name[0] = 'S';
    dk_song.name[1] = 'O';
    dk_song.name[2] = 'N';
    dk_song.name[3] = 'G';
    dk_song.tempo = 76;
    dk_song.root = 0;
    dk_song.mode = 0;
    dk_song.layout = 1;                  /* SCALE */
    dk_song.white = 1;                   /* the tape's: chords on the white keys */
    dk_song.black = 0;
    dk_song.chord = CH_MAJ7;
    dk_song.quality = 0;                 /* DIATONIC */
    dk_song.voicing = VC_OPEN;
    dk_song.lead = 1;
    dk_song.strum = 40;
    dk_song.dim = 1;
    dk_song.harm_track = 0;   /* SPLIT: the chords to the piano */
    dk_song.style = 0;
    dk_song.dust = 25;
    dk_song.sidechain = 15;
    dk_song.space = 1;
    dk_song.cur_track = 2;
    for (i = 0; i < DK_NPART; i++) {
        dk_song.part[i].bars = 4;
        dk_song.part[i].label = (uint8_t)(i % 8);
    }
    for (t = 0; t < DK_NTRACK; t++)
        track_defaults(t);
    reindex();
    song_edits++;
}

/* the templates: empty parts with their labels and a road of them */
void song_template(int tp)
{
    static const struct {
        uint8_t n, part[8], bars[8], label[8], road[12], nroad;
    } T[4] = {
        {1, {0}, {4}, {0}, {0}, 1},
        {5, {0, 1, 2, 3, 4}, {8, 16, 16, 8, 8}, {0, 1, 3, 5, 7}, {0, 1, 2, 1, 2, 3, 2, 4}, 8},
        {5, {0, 1, 2, 3, 4}, {8, 8, 8, 8, 8}, {0, 2, 4, 6, 7}, {0, 1, 2, 3, 1, 2, 4}, 7},
        {4, {0, 1, 2, 3}, {16, 16, 16, 16}, {0, 1, 5, 7}, {0, 1, 2, 1, 3}, 5},
    };
    int i;
    tp &= 3;
    dk_song.nroad = T[tp].nroad;
    for (i = 0; i < T[tp].n; i++) {
        dk_song.part[i].bars = T[tp].bars[i];
        dk_song.part[i].label = T[tp].label[i];
    }
    for (i = 0; i < T[tp].nroad; i++) {
        dk_song.road[i].part = T[tp].road[i];
        dk_song.road[i].reps = 1;
        dk_song.road[i].tricks = 0;
        dk_song.road[i].drop = 0;
    }
    song_edits++;
}

/* a mood: the tempo, the key, the style, the garden's biome, the tracks' sounds */
void song_mood(int m)
{
    const dk_mood_t *md = &TAPE.moods[m % TAPE.nmood];
    int t;
    dk_song.mood = (uint8_t)(m % TAPE.nmood);
    dk_song.tempo = md->tempo;
    dk_song.mode = md->mode;
    dk_song.style = md->style;
    dk_song.biome = md->biome;
    dk_song.dust = md->dust;
    dk_song.space = md->space;
    for (t = 0; t < DK_NTRACK; t++) {
        if (md->eng[t] >= 0 && md->eng[t] != dk_song.trk[t].eng)   /* (the track keeps its role: PIANO, KEYS ...) */
            dk_song.trk[t].eng = (uint8_t)md->eng[t];
        if (md->preset[t] >= 0)
            track_preset(t, md->preset[t]);
        dk_song.trk[t].drv[1] = md->style;
    }
    song_edits++;
}
