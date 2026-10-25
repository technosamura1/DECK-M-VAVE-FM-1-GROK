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
static uint8_t clip_src_t;               /* the track the clipboard came from (its frozen sounds are that track's) */
typedef struct {
    uint8_t kind, clip;                  /* 0 a clip, 1 the road, 2 a part's bars and track mutes (clip = the part; len, layers) */
    uint16_t n, len, at;                 /* events, the clip's length; where in the buffer */
    uint8_t layers, nroad;
    uint8_t grp;                         /* snapshots of one edit over several clips: undone together (0 alone) */
} snap_t;
/* a road snapshot keeps the road's blocks in the event buffer (as bytes, ROAD_EV events' room): ~8 KB of RAM less than a
 * road array in every snapshot */
#define ROAD_EV ((DK_NROAD * (int)sizeof(dk_block_t) + (int)sizeof(dk_ev_t) - 1) / (int)sizeof(dk_ev_t))
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
    dk_song.lmute[clip] = 0;
    if (dk_lsolo_clip == clip)
        dk_lsolo_clip = 0xFF;
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
    int need = road == 1 ? ROAD_EV : road ? 0 : dk_song.clip[clip].n, used = 0, i;
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
    s->kind = (uint8_t)road;
    s->clip = (uint8_t)clip;
    s->at = (uint16_t)used;
    s->n = (uint16_t)need;
    if (road == 2) {
        s->len = dk_song.part[clip & 7].bars;
        s->layers = dk_song.part[clip & 7].mute;
    } else if (road) {
        uint8_t *d = (uint8_t *)(buf + used);
        const uint8_t *r = (const uint8_t *)dk_song.road;
        s->nroad = dk_song.nroad;
        for (i = 0; i < DK_NROAD * (int)sizeof(dk_block_t); i++)
            d[i] = r[i];
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
    if (s->kind == 2) {
        dk_song.part[s->clip & 7].bars = (uint8_t)s->len;
        dk_song.part[s->clip & 7].mute = s->layers;
    } else if (s->kind) {
        uint8_t *d = (uint8_t *)dk_song.road;
        const uint8_t *r = (const uint8_t *)(buf + s->at);
        dk_song.nroad = s->nroad;
        for (i = 0; i < DK_NROAD * (int)sizeof(dk_block_t); i++)
            d[i] = r[i];
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
void undo_push_part(int p)                /* a part's bars and track mutes */
{
    if (snap_take(usnap, &un, NSNAP, ubuf, 1024, p & 7, 2) == 0)
        usnap[un - 1].grp = grp_now;
    rn = 0;
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
    *what = s.kind == 2 ? -2 : s.kind ? -1 : s.clip;
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
    *what = s.kind == 2 ? -2 : s.kind ? -1 : s.clip;
    return 1;
}

/* ---------------------------------------------------------- frozen sounds --- */
/* LOCK TAKE (layers like the OP-1's tape): when a take ends, the sound the track has then (its engine, knobs and MACRO) is
 * kept in dk_song.snd[track] and the take's notes point to it (the layer byte's bits 5..7): the notes play with it whatever
 * the track's preset, engine or knobs become. The next take is recorded with the new sound */
static uint8_t take_pend[DK_NCLIP];      /* a clip: the take (pass + 1) with notes in it not frozen yet; 0 none */
uint8_t song_layers_full;                /* the sounds' table was full: the UI says it, once */

void takes_forget(void)                  /* a new song: no take waits to be frozen */
{
    int c;
    for (c = 0; c < DK_NCLIP; c++)
        take_pend[c] = 0;
}

static int snd_scan(const dk_ev_t *b, int n, int id)
{
    int i;
    for (i = 0; i < n; i++)
        if (DK_L_SND(b[i].layer) == (unsigned)id)
            return 1;
    return 0;
}
/* a place of the track's table that its notes (or the undo's and redo's) still point to */
static int snd_in_use(int t, int id)
{
    int c, i;
    for (c = t; c < DK_NCLIP; c += DK_NTRACK)
        if (snd_scan(dk_ev + dk_song.clip[c].at, dk_song.clip[c].n, id))
            return 1;
    for (i = 0; i < un; i++)
        if (!usnap[i].kind && usnap[i].clip % DK_NTRACK == t && snd_scan(ubuf + usnap[i].at, usnap[i].n, id))
            return 1;
    for (i = 0; i < rn; i++)
        if (!rsnap[i].kind && rsnap[i].clip % DK_NTRACK == t && snd_scan(rbuf + rsnap[i].at, rsnap[i].n, id))
            return 1;
    return 0;
}
static int snd_same(const dk_snd_t *a, const dk_snd_t *b)
{
    int i;
    if (a->eng != b->eng || a->drift != b->drift)
        return 0;
    for (i = 0; i < DK_NP; i++)
        if (a->p[i] != b->p[i])
            return 0;
    for (i = 0; i < 3; i++)
        if (a->env[i] != b->env[i])
            return 0;
    return 1;
}
static void snd_capture(int t, dk_snd_t *s)   /* the track's sound now */
{
    const dk_track_t *k = &dk_song.trk[t];
    const dk_engine_t *e = TAPE.eng[k->eng % TAPE.neng];
    int i;
    for (i = 0; i < (int)sizeof *s; i++)
        ((uint8_t *)s)[i] = 0;
    s->used = 1;
    s->eng = (uint8_t)(k->eng % TAPE.neng);
    s->drift = k->drift;
    for (i = 0; i < DK_NP; i++)
        s->p[i] = i < e->nparams ? (int8_t)dk_param_static(t, T_P0 + i) : 0;
    s->env[0] = (int8_t)dk_param_static(t, T_ATT);
    s->env[1] = (int8_t)dk_param_static(t, T_DEC);
    s->env[2] = (int8_t)dk_param_static(t, T_REL);
}
/* the place of this sound in the track's table: the same one if it is there, else a free one (or one nothing points to);
 * 0: the table is full */
static int snd_make(int t, const dk_snd_t *s)
{
    int i;
    for (i = 0; i < DK_NSND; i++)
        if (dk_song.snd[t][i].used && snd_same(&dk_song.snd[t][i], s))
            return i + 1;
    for (i = 0; i < DK_NSND; i++)
        if (!dk_song.snd[t][i].used) {
            dk_song.snd[t][i] = *s;
            return i + 1;
        }
    for (i = 0; i < DK_NSND; i++)
        if (!snd_in_use(t, i + 1)) {
            dk_song.snd[t][i] = *s;
            return i + 1;
        }
    return 0;
}
/* a frozen sound moved to another track's table (a paste): its place there, 0 the track's own */
static int snd_move(int from, int id, int to)
{
    if (id < 1 || id > DK_NSND || !dk_song.snd[from][id - 1].used)
        return 0;
    return snd_make(to, &dk_song.snd[from][id - 1]);
}

/* the take's notes (clip, pass) with the track's sound now: how many were frozen (the song is locked) */
static int take_freeze(int clip, int pass)
{
    int t = clip % DK_NTRACK, take = pass > 31 ? 31 : pass, n = 0, i, id;
    const dk_clip_t *c = &dk_song.clip[clip];
    dk_snd_t s;
    if (!dk_lock_take || (TAPE.eng[dk_song.trk[t].eng % TAPE.neng]->flags & (EF_DRUM | EF_CONT)))
        return 0;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        n += !(e->fl & EV_MOTION) && DK_L_TAKE(e->layer) == (unsigned)take && !DK_L_SND(e->layer);
    }
    if (!n)
        return 0;
    snd_capture(t, &s);
    id = snd_make(t, &s);
    if (!id) {
        song_layers_full = 1;
        return 0;
    }
    for (i = 0; i < c->n; i++) {
        dk_ev_t *e = &dk_ev[c->at + i];
        if (!(e->fl & EV_MOTION) && DK_L_TAKE(e->layer) == (unsigned)take && !DK_L_SND(e->layer))
            e->layer = DK_L_MAKE(take, id);
    }
    song_edits++;
    return n;
}
/* the takes that have ended (the recording went on to the next pass, or stopped; a track's: all, force) are frozen */
static int takes_settle(int rec_on, int pass, int only_t)
{
    int c, n = 0;
    for (c = 0; c < DK_NCLIP; c++) {
        int take = take_pend[c] - 1;
        if (!take_pend[c] || (only_t >= 0 && c % DK_NTRACK != only_t))
            continue;
        if (only_t < 0 && rec_on && take == (pass > 31 ? 31 : pass))   /* (still being played) */
            continue;
        take_pend[c] = 0;
        n += take_freeze(c, take);
    }
    return n;
}
static int takes_waiting(void)
{
    int c;
    for (c = 0; c < DK_NCLIP; c++)
        if (take_pend[c])
            return 1;
    return 0;
}

/* --------------------------------------------------------- the recording --- */
static uint32_t rec_r;
static uint8_t snp_pass[DK_NCLIP];       /* per clip: the pass whose undo snapshot is taken (0xFF none) */
static uint8_t rep_pass[DK_NCLIP];       /* REPLACE, per clip: the pass the erase is in (0xFF none) */
static uint8_t rep_started;               /* REPLACE: the first look is done (tracks armed later start from where they are) */
static uint16_t rep_pos[DK_NCLIP];       /*   and how far it has erased (the take has passed over up to here) */
/* one undo snapshot per (pass, clip): the first change a pass makes to the clip (a note in, or REPLACE's erase) */
static void rec_snap(int clip, int pass)
{
    if (snp_pass[clip] == pass)
        return;
    snap_take(usnap, &un, NSNAP, ubuf, 1024, clip, 0);
    rn = 0;
    snp_pass[clip] = (uint8_t)pass;
}
/* REPLACE: the notes of the clip from `from` up to `to` that were there before this REC press go (the song is locked) */
static int rep_erase(int clip, int pass, int from, int to)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, n = 0, take0 = dk_rec_pass0 > 31 ? 31 : dk_rec_pass0;
    for (i = c->n - 1; i >= 0; i--) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (!(e->fl & EV_MOTION) && e->pos >= from && e->pos < to && DK_L_TAKE(e->layer) < (unsigned)take0) {
            if (!n)
                rec_snap(clip, pass);
            ev_delete(clip, i);
            n++;
        }
    }
    return n;
}
/* REPLACE: as the recording goes round, the armed tracks' old notes under it are erased (a pass: one undo step) */
static void rep_run(void)
{
    int t;
    for (t = 0; t < DK_NTRACK; t++) {
        int c, len, now;
        if (!(dk_rec_mask >> t & 1u))
            continue;
        c = (dk_cur_part & 7) * DK_NTRACK + t;
        len = clip_len(c);
        now = (int)(dk_part_tick % (uint16_t)(len ? len : 1));
        if (rep_pass[c] != dk_pass) {    /* the first look, or the next pass: the rest of the last one first */
            if (rep_pass[c] != 0xFF) {
                rep_erase(c, rep_pass[c], rep_pos[c], len);
                rep_pos[c] = 0;
            } else
                rep_pos[c] = (uint16_t)(rep_started || dk_pass != dk_rec_pass0 ? now : dk_rec_tick0 % (len ? len : 1));
            rep_pass[c] = dk_pass;
        }
        if (now > rep_pos[c]) {
            rep_erase(c, dk_pass, rep_pos[c], now);
            rep_pos[c] = (uint16_t)now;
        }
    }
    rep_started = 1;
}
/* the render's recorded events into the song (a snapshot first for each new pass of a clip: its undo); only_t >= 0: also
 * freeze that track's take at once (its sound is about to change) */
static int rec_flush_do(int only_t)
{
    /* (the pass and the recording as they are before the ring is read: the render pushes a take's last notes before it goes
     * on to the next pass, so a take that is over by now is all in the ring) */
    int pass = dk_pass, rec_on = dk_recording;
    uint32_t w;
    int n = 0;
    __asm__ volatile("" ::: "memory");
    if (rec_on && dk_rec_mode && only_t < 0) {
        dk_lock();
        rep_run();
        dk_unlock();
    }
    w = dk_recq_w;
    if (w - rec_r > DK_RECQ)
        rec_r = w - DK_RECQ;
    if (rec_r == w) {
        if (!takes_waiting())
            return 0;
        dk_lock();
        n = takes_settle(rec_on, pass, only_t);
        dk_unlock();
        return n;
    }
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
        rec_snap(r.clip, r.pass);
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
        if (dk_song_insert(r.clip, &r.ev) >= 0 && !(r.ev.fl & EV_MOTION) && !DK_L_SND(r.ev.layer))
            take_pend[r.clip] = (uint8_t)(DK_L_TAKE(r.ev.layer) + 1u);
        n++;
    }
    n += takes_settle(rec_on, pass, only_t);
    dk_unlock();
    return n;
}
int rec_flush(void) { return rec_flush_do(-1); }
/* the track's sound is about to change (a preset, an engine): the take being recorded on it is frozen with the old one */
void track_sound_to(int t)
{
    rec_flush_do(t);
}
void rec_reset(void)
{
    int c;
    rep_started = 0;
    rec_r = dk_recq_w;
    for (c = 0; c < DK_NCLIP; c++) {
        snp_pass[c] = 0xFF;
        rep_pass[c] = 0xFF;
        rep_pos[c] = 0;
    }
}

/* ------------------------------------------------------------------ parts --- */
int part_has_events(int p)
{
    int t;
    for (t = 0; t < DK_NTRACK; t++)
        if (dk_song.clip[(p & 7) * DK_NTRACK + t].n)
            return 1;
    return 0;
}
static uint8_t pc_part = 0xFF, pc_grp;   /* the part a copy was just made into, and its undo step */
static uint32_t pc_edits;               /* (the edits then) */
int part_copy_x(int from, int to, int wipe);
int part_copy(int from, int to) { return part_copy_x(from, to, 0); }
/* wipe: the target's own clips go first (DUPLICATE PART TO a part with notes, after the second press); one undo step */
int part_copy_x(int from, int to, int wipe)
{
    int t, need = 0, i;
    from &= 7;
    to &= 7;
    if (from != to)
        rec_flush();                       /* (what was just recorded is in the clips: it goes along) */
    if (from == to || (!wipe && part_has_events(to)) || !part_has_events(from))   /* never over a part with notes, unless asked */
        return 0;
    for (t = 0; t < DK_NTRACK; t++)
        need += dk_song.clip[from * DK_NTRACK + t].n - (wipe ? (int)dk_song.clip[to * DK_NTRACK + t].n : 0);
    if (dk_song.nev + need > DK_NEV)
        return -1;
    undo_group(1);
    pc_grp = grp_now;
    for (t = 0; t < DK_NTRACK; t++)       /* (its undo: the clips of `to` as they are: empty; and the part's bars and mutes) */
        if (dk_song.clip[from * DK_NTRACK + t].n || (wipe && dk_song.clip[to * DK_NTRACK + t].n))
            undo_push(to * DK_NTRACK + t);
    undo_push_part(to);
    undo_group(0);
    dk_lock();
    pc_part = (uint8_t)to;
    if (wipe)
        for (t = 0; t < DK_NTRACK; t++)
            if (dk_song.clip[to * DK_NTRACK + t].n)
                clip_clear(to * DK_NTRACK + t);
    for (t = 0; t < DK_NTRACK; t++) {
        int c = to * DK_NTRACK + t, src = from * DK_NTRACK + t, n = dk_song.clip[src].n;
        if (n) {                           /* `c` is empty: its place opened (the events behind move up), the events copied in */
            for (i = dk_song.nev - 1; i >= dk_song.clip[c].at; i--)
                dk_ev[i + n] = dk_ev[i];
            dk_song.clip[c].n = (uint16_t)n;
            reindex();                     /* (the source's place has moved if it lay behind) */
            for (i = 0; i < n; i++)
                dk_ev[dk_song.clip[c].at + i] = dk_ev[dk_song.clip[src].at + i];
            dk_song.clip[c].len = dk_song.clip[src].len;
            dk_song.clip[c].layers = dk_song.clip[src].layers;
            dk_song.lmute[c] = dk_song.lmute[src];
            take_pend[c] = take_pend[src];
        }
    }
    dk_song.part[to].bars = dk_song.part[from].bars;
    dk_song.part[to].mute = dk_song.part[from].mute;
    song_edits++;
    pc_edits = song_edits;
    dk_unlock();
    return 1;
}
int part_copy_undo(int to)
{
    uint8_t g;
    if (pc_part != (uint8_t)(to & 7) || pc_edits != song_edits || !un || !pc_grp || usnap[un - 1].grp != pc_grp)
        return 0;
    g = pc_grp;
    pc_part = 0xFF;
    do {                                 /* (no redo kept: the copy never was) */
        dk_lock();
        snap_restore(&usnap[un - 1], ubuf);
        dk_unlock();
        un--;
    } while (un && usnap[un - 1].grp == g);
    return 1;
}

/* ------------------------------------------------------------- clip edits --- */
/* each with its undo; the song locked */
void clip_copy(int clip)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i;
    clip_n = c->n > 384 ? 384 : c->n;
    clip_src_t = (uint8_t)(clip % DK_NTRACK);
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
    if (clip_src_t != clip % DK_NTRACK) {   /* on another track: the frozen sounds go with the notes (its table's places) */
        static dk_ev_t pt[384];
        int i, to = clip % DK_NTRACK;
        for (i = 0; i < clip_n; i++) {
            pt[i] = clipboard[i];
            if (DK_L_SND(pt[i].layer))
                pt[i].layer = DK_L_MAKE(DK_L_TAKE(pt[i].layer), snd_move(clip_src_t, (int)DK_L_SND(pt[i].layer), to));
        }
        clip_put(clip, pt, clip_n);
    } else
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
    takes_forget();
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

/* CLEAR ALL: the whole composition erased: every clip (8 parts x 6 tracks), the road, the frozen sounds, the parts' bars (4),
 * labels and track mutes, the undo and redo (they cannot hold a whole song). Kept: the tracks' sounds and setup, the tempo,
 * key, harmony, name, the settings, the project slot. The song is not playing (the caller checks) */
void song_clear_all(void)
{
    int i;
    rec_flush();                         /* (nothing waits in the recording's queue) */
    dk_lock();
    for (i = 0; i < DK_NCLIP; i++) {
        dk_song.clip[i].n = 0;
        dk_song.clip[i].len = 0;
        dk_song.clip[i].layers = 0;
        dk_song.lmute[i] = 0;
        dk_song.clip[i].at = 0;
    }
    dk_song.nev = 0;
    dk_lsolo_clip = 0xFF;
    for (i = 0; i < DK_NPART; i++) {
        dk_song.part[i].bars = 4;
        dk_song.part[i].label = (uint8_t)(i % 8);
        dk_song.part[i].mute = 0;
        dk_song.part[i].rsv = 0;
    }
    dk_song.nroad = 0;
    for (i = 0; i < DK_NROAD; i++) {
        dk_song.road[i].part = 0;
        dk_song.road[i].reps = 0;
        dk_song.road[i].tricks = 0;
        dk_song.road[i].drop = 0;
    }
    {
        uint8_t *b = (uint8_t *)dk_song.snd;
        for (i = 0; i < (int)sizeof dk_song.snd; i++)
            b[i] = 0;
    }
    dk_song.cur_part = 0;
    dk_pm_x = 0;
    takes_forget();
    un = 0;
    rn = 0;
    pc_part = 0xFF;
    song_edits++;
    dk_unlock();
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
        track_sound_to(t);
        if (md->eng[t] >= 0 && md->eng[t] != dk_song.trk[t].eng)   /* (the track keeps its role: PIANO, KEYS ...) */
            dk_song.trk[t].eng = (uint8_t)md->eng[t];
        if (md->preset[t] >= 0)
            track_preset(t, md->preset[t]);
        dk_song.trk[t].drv[1] = md->style;
    }
    song_edits++;
}

/* ------------------------------------------------------------------ LAYERS --- */
/* A clip's layers: one per take that has notes in it, in take order. take[i] 0..31, snd[i] the frozen sound of the take's
 * first note (0: the track's own), cnt[i] its notes. Returns how many (at most `max`). */
int layers_list(int clip, uint8_t *take, uint8_t *snd, uint16_t *cnt, int max)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, j, n = 0;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        int tk = (int)DK_L_TAKE(e->layer);
        if (e->fl & EV_MOTION)
            continue;
        for (j = 0; j < n && take[j] != tk; j++)
            ;
        if (j == n) {                         /* a new layer: kept in take order */
            if (n >= max)
                continue;
            for (j = n; j > 0 && take[j - 1] > tk; j--) {
                take[j] = take[j - 1];
                snd[j] = snd[j - 1];
                cnt[j] = cnt[j - 1];
            }
            take[j] = (uint8_t)tk;
            snd[j] = (uint8_t)DK_L_SND(e->layer);
            cnt[j] = 0;
            n++;
        }
        for (j = 0; j < n && take[j] != tk; j++)
            ;
        cnt[j]++;
    }
    return n;
}
int layer_muted(int clip, int take) { return (dk_song.lmute[clip] >> take & 1u) != 0; }
int layer_soloed(int clip, int take) { return dk_lsolo_clip == clip && dk_lsolo_take == take; }
void layer_mute_toggle(int clip, int take)
{
    dk_song.lmute[clip] ^= 1u << take;
    song_edits++;
}
void layer_solo_toggle(int clip, int take)
{
    if (layer_soloed(clip, take))
        dk_lsolo_clip = 0xFF;
    else {
        dk_lsolo_take = (uint8_t)take;
        dk_lsolo_clip = (uint8_t)clip;
    }
}
/* the layer's notes (every event of the take) removed: one undo step; the clip's top take number follows */
int layer_delete(int clip, int take)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int i, n = 0, top = 0;
    undo_group(1);
    undo_push(clip);
    dk_lock();
    for (i = c->n - 1; i >= 0; i--)
        if ((int)DK_L_TAKE(dk_ev[c->at + i].layer) == take) {
            ev_delete(clip, i);
            n++;
        }
    for (i = 0; i < c->n; i++)
        if ((int)DK_L_TAKE(dk_ev[c->at + i].layer) > top)
            top = (int)DK_L_TAKE(dk_ev[c->at + i].layer);
    c->layers = (uint8_t)top;
    dk_song.lmute[clip] &= ~(1u << take);
    if (dk_lsolo_clip == clip && dk_lsolo_take == take)
        dk_lsolo_clip = 0xFF;
    dk_unlock();
    undo_group(0);
    song_edits++;
    return n;
}
/* MERGE: every layer of the clip becomes one (the lowest take's number), with the track's own sound (id 0); one undo step.
 * 0 when there is only one layer (nothing to merge). */
int layers_merge(int clip)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    uint8_t tk[40], sd[40];
    uint16_t cn[40];
    int i, n = layers_list(clip, tk, sd, cn, 40);
    if (n < 2)
        return 0;
    undo_group(1);
    undo_push(clip);
    dk_lock();
    for (i = 0; i < c->n; i++)
        dk_ev[c->at + i].layer = DK_L_MAKE(tk[0], 0);
    dk_song.clip[clip].layers = tk[0];
    dk_song.lmute[clip] = 0;
    if (dk_lsolo_clip == clip)
        dk_lsolo_clip = 0xFF;
    dk_unlock();
    undo_group(0);
    song_edits++;
    return n;
}
