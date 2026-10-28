/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The song's data, edited from the main loop (the render only reads it, between dk_lock and dk_unlock): the events
 * (sorted by clip, then by position), the tracks' engines and presets, the clips' operations, undo and redo (a
 * clip's or the road's snapshot), the recording's events put in place, the templates. Shared by the app (unity
 * build) and the tests. */
#include "song.h"

dk_user_t dk_user DK_POOL;                       /* the user's presets: 4 an engine */
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
/* SNAPSHOT: one part (its six clips, bars, mutes) kept in memory to come back to after an experiment (one slot; gone with
 * the song). Its notes keep their frozen sounds alive like the undo's do (snd_in_use). CLEAR ROAD's one undo step uses
 * the same bytes for the whole song (a second event array does not fit the pool): the events go in ubuf+rbuf, 1536 of
 * them. CLEAR ALL never arms that step. */
#define SNAP_EV 768
static union {
    dk_song_t song;
    dk_ev_t ev[SNAP_EV];
} snapb DK_POOL;
#define sbuf snapb.ev
static uint8_t wundo, wredo, wlive, wbusy;   /* the wiped road can be undone / redone; the part that was live; a wipe is running */
static uint16_t wnev;
static uint32_t wedit;
static void wipe_apply(void);
static int wipe_restore(void);
static struct {
    uint8_t ok, part, bars, mute;
    uint16_t n[DK_NTRACK], len[DK_NTRACK];
    uint8_t layers[DK_NTRACK];
    uint32_t lmute[DK_NTRACK];
} snp;

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

/* every event of the clip, the automation too (the 2.5 s track erase, a part wiped, CLEAR ALL) */
void clip_clear_all(int clip)
{
    clip_put(clip, 0, 0);
    dk_song.clip[clip].layers = 0;
    dk_song.lmute[clip] = 0;
    if (dk_lsolo_clip == clip)
        dk_lsolo_clip = 0xFF;
}
/* AUTOMATION: a curve's point is a motion event with EV_RAMP; `target` 0: all of them. One pass over the pool, so the lock is short. */
static int ev_is_ramp(const dk_ev_t *e, int target)
{
    return (e->fl & (EV_MOTION | EV_RAMP)) == (EV_MOTION | EV_RAMP) && (!target || e->note == target);
}
int auto_count(int clip, int target)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, n = 0;
    for (i = 0; i < c->n; i++)
        n += ev_is_ramp(&dk_ev[c->at + i], target);
    return n;
}
/* the clip's first w events kept (the rest were dropped by compacting them out): the pool closed up behind them */
static void clip_close(int clip, int w)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int i, gone = c->n - w;
    for (i = c->at + c->n; i < dk_song.nev; i++)
        dk_ev[i - gone] = dk_ev[i];
    c->n = (uint16_t)w;
    for (i = clip + 1; i < DK_NCLIP; i++)
        dk_song.clip[i].at = (uint16_t)(dk_song.clip[i].at - gone);
    dk_song.nev = (uint16_t)(dk_song.nev - gone);
    song_edits++;
}
/* events dropped in one pass: the ramps of `target` (0: all) when drop_notes is 0, else everything that is not a ramp */
static int clip_filter(int clip, int drop_notes, int target)
{
    dk_clip_t *c = &dk_song.clip[clip];
    int i, w = 0, gone, n = c->n;
    for (i = 0; i < n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        int ramp = ev_is_ramp(e, target);
        if (drop_notes ? ramp : !ramp)
            dk_ev[c->at + w++] = *e;
    }
    gone = n - w;
    if (gone)
        clip_close(clip, w);
    return gone;
}
/* the automation of a target (0: all) out of the clip */
int auto_clear(int clip, int target)
{
    return clip_filter(clip, 0, target);
}
/* the notes (and the other motion) out of the clip; the automation stays: the REC hold erase, CLEAR (SEQ+REC), BAKE, the arp */
void clip_clear(int clip)
{
    dk_clip_t *c = &dk_song.clip[clip];
    if (!auto_count(clip, 0)) {
        clip_clear_all(clip);
        return;
    }
    clip_filter(clip, 1, 0);
    c->layers = 0;
    dk_song.lmute[clip] = 0;
    if (dk_lsolo_clip == clip)
        dk_lsolo_clip = 0xFF;
}

/* a point of a curve: the target's value `val` (0..100) at `pos`. The target's older points the knob has passed (after `from`,
 * the gesture's last point, up to pos; round the clip's end if pos < from) are dropped: what is turned replaces what was
 * there. Returns 0, or -1 when the pool is (nearly) full. */
int auto_point(int clip, int target, int pos, int val, int from, int len)
{
    dk_clip_t *c = &dk_song.clip[clip];
    dk_ev_t e;
    int i, w = 0, n = c->n, gone;
    (void)len;
    for (i = 0; i < n; i++) {
        const dk_ev_t *o = &dk_ev[c->at + i];
        int p = o->pos, del = 0;
        if (ev_is_ramp(o, target))
            del = from < 0 || from == pos ? p == pos : from < pos ? (p > from && p <= pos) : (p > from || p <= pos);
        if (!del)
            dk_ev[c->at + w++] = *o;
    }
    gone = n - w;
    if (gone)
        clip_close(clip, w);
    if (dk_song.nev + 64 > DK_NEV)
        return -1;
    e.pos = (uint16_t)pos;
    e.dur = 0;
    e.note = (uint8_t)target;
    e.vel = (uint8_t)(val < 0 ? 0 : val > 100 ? 100 : val);
    e.fl = EV_MOTION | EV_RAMP;
    e.layer = 0;
    return dk_song_insert(clip, &e) < 0 ? -1 : 0;
}
/* a gesture's points thinned: the Douglas-Peucker way, a point stays only if the curve without it would be more than `tol`
 * % of the range off; the first and last stay. Over a long gesture the tolerance grows till at most 24 points a bar remain
 * (the pool is shared: 4096 events). Points of `target` with pos in [from, to]. */
void auto_thin(int clip, int target, int from, int to, int tol)
{
    enum { MAXP = 96 };
    dk_clip_t *c = &dk_song.clip[clip];
    int idx[MAXP], np = 0, i, maxn, keepn;
    uint8_t keep[MAXP];
    for (i = 0; i < c->n && np < MAXP; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (ev_is_ramp(e, target) && e->pos >= from && e->pos <= to)
            idx[np++] = i;
    }
    if (np < 3)
        return;
    maxn = 24 * ((to - from) / DK_TPB + 1);
    for (;;) {
        int stack[MAXP][2], sp = 0;
        for (i = 0; i < np; i++)
            keep[i] = 0;
        keep[0] = keep[np - 1] = 1;
        stack[sp][0] = 0;
        stack[sp++][1] = np - 1;
        while (sp) {
            int a = stack[--sp][0], b = stack[sp][1], worst = -1, wd = tol, k;
            const dk_ev_t *ea = &dk_ev[c->at + idx[a]], *eb = &dk_ev[c->at + idx[b]];
            for (k = a + 1; k < b; k++) {
                const dk_ev_t *ek = &dk_ev[c->at + idx[k]];
                int span = eb->pos - ea->pos, ip = span > 0 ? ea->vel + (eb->vel - ea->vel) * (ek->pos - ea->pos) / span : ea->vel;
                int d = ek->vel > ip ? ek->vel - ip : ip - ek->vel;
                if (d > wd) {
                    wd = d;
                    worst = k;
                }
            }
            if (worst >= 0) {
                keep[worst] = 1;
                stack[sp][0] = a;
                stack[sp++][1] = worst;
                stack[sp][0] = worst;
                stack[sp++][1] = b;
            }
        }
        for (i = keepn = 0; i < np; i++)
            keepn += keep[i];
        if (keepn <= maxn || tol >= 50)
            break;
        tol *= 2;
    }
    {
        int w = 0, j = 0, n = c->n;
        for (i = 0; i < n; i++) {
            if (j < np && idx[j] == i) {
                if (!keep[j++])
                    continue;
            }
            dk_ev[c->at + w++] = dk_ev[c->at + i];
        }
        if (w != n)
            clip_close(clip, w);
    }
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
    if (wbusy)                           /* the wipe is using these buffers */
        return -1;
    wundo = wredo = 0;                   /* a normal step reuses them: the wiped road cannot come back after */
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
    if (wundo) {                         /* CLEAR ROAD: notes, every part, the road, the instruments */
        dk_cmd(C_PART, wipe_restore(), 0, 0);   /* the sequencer on the part that was current */
        *what = -1;
        return 1;
    }
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
    if (wredo) {
        if (song_edits != wedit)         /* something changed since the undo: do not wipe over it */
            wredo = 0;
        else {
            wbusy = 1;
            wipe_apply();                /* the copy in the buffers stays, so UNDO can bring it back again */
            wbusy = 0;
            wundo = 1;
            wredo = 0;
            dk_cmd(C_PART, 0, 0, 0);
            *what = -1;
            return 1;
        }
    }
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
/* ERASE THE LAST TAKE (hold REC): per clip, the lowest take number the latest REC press put notes in (last_sess = that press's number) */
static uint8_t last_sess[DK_NCLIP], last_from[DK_NCLIP], rec_sess;
static uint8_t take_pend[DK_NCLIP];      /* a clip: the take (pass + 1) with notes in it not frozen yet; 0 none */
uint8_t song_layers_full;                /* the sounds' table was full: the UI says it, once */

void takes_forget(void)                  /* a new song: no take waits to be frozen (and no snapshot) */
{
    int c;
    snp.ok = 0;
    for (c = 0; c < DK_NCLIP; c++)
        take_pend[c] = last_sess[c] = 0;
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
    if (snp.ok) {
        int at = 0;
        for (i = 0; i < t; i++)
            at += snp.n[i];
        if (snd_scan(sbuf + at, snp.n[t], id))
            return 1;
    }
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
        {
            int live = dk_rec_take[c % DK_NTRACK];
            if (live < 1 || live > 31)
                live = pass > 31 ? 31 : pass;
            if (only_t < 0 && rec_on && take == live)   /* (still being played) */
                continue;
        }
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
static uint8_t rep_wrap[DK_NCLIP];       /*   the next pass told before the loop's tick came round to 0: wait for it */
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
static int rep_groove(int t) { return dk_song.trk[t].seq == SQ_GROOVE && dk_gr_cur[t]; }
static int rep_erase(int clip, int pass, int from, int to)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, n = 0, take0 = dk_rec_floor[clip % DK_NTRACK];
    if (rep_groove(clip % DK_NTRACK))        /* GROOVE: the drummer's last pass is the clip (its earlier passes go too) */
        take0 = dk_rec_take[clip % DK_NTRACK];
    else if (take0 < 1)
        take0 = dk_rec_pass0 > 31 ? 31 : dk_rec_pass0;
    if (take0 > 31)
        take0 = 31;
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
        if (!(dk_rec_mask >> t & 1u) || !(dk_rec_mode || rep_groove(t)))
            continue;                    /* (REPLACE: every armed track; OVERDUB: a GROOVE track with its drummer playing) */
        c = (dk_cur_part & 7) * DK_NTRACK + t;
        len = clip_len(c);
        now = (int)(dk_part_tick % (uint16_t)(len ? len : 1));
        if (rep_pass[c] != dk_pass) {    /* the first look, or the next pass: the rest of the last one first */
            if (rep_pass[c] != 0xFF) {
                if (!rep_groove(t))      /* (GROOVE: the last pass's tail is the drummer's own, just played: it stays) */
                    rep_erase(c, rep_pass[c], rep_pos[c], len);
                rep_pos[c] = 0;
                rep_wrap[c] = (uint8_t)(now > len / 2);   /* (the pass counted a tick early: the loop's end is still under it,
                                                           * and [0, now) would be the whole of the last pass) */
            } else
                rep_pos[c] = (uint16_t)(rep_started || dk_pass != dk_rec_pass0 ? now : dk_rec_tick0 % (len ? len : 1));
            rep_pass[c] = dk_pass;
        }
        if (rep_wrap[c]) {
            if (now > len / 2)
                continue;
            rep_wrap[c] = 0;
        }
        if (now > rep_pos[c]) {
            rep_erase(c, dk_pass, rep_pos[c], now);
            rep_pos[c] = (uint16_t)now;
        }
    }
    rep_started = 1;
}
/* THE POOL IS FULL while a take records: the oldest take of the clip with the most layers goes (its notes only: the
 * automation and the take being recorded stay), one undo step; 0: nothing to drop (every clip has a single take).
 * The UI says it (song_mem_dropped / song_mem_full). The song is locked */
uint8_t song_mem_dropped, song_mem_full;
static int mem_make_room(int pass)
{
    int c, best = -1, bn = 0, take, cur = pass > 31 ? 31 : pass, i, w;
    for (c = 0; c < DK_NCLIP; c++) {
        const dk_clip_t *k = &dk_song.clip[c];
        uint32_t seen = 0;
        int n = 0;
        for (i = 0; i < k->n; i++) {
            const dk_ev_t *e = &dk_ev[k->at + i];
            if (!(e->fl & EV_MOTION)) {
                seen |= 1u << DK_L_TAKE(e->layer);
                n++;
            }
        }
        if ((seen & (seen - 1u)) && n > bn) {      /* two takes or more */
            best = c;
            bn = n;
        }
    }
    if (best < 0)
        return 0;
    {
        dk_clip_t *k = &dk_song.clip[best];
        uint32_t seen = 0;
        for (i = 0; i < k->n; i++)
            if (!(dk_ev[k->at + i].fl & EV_MOTION))
                seen |= 1u << DK_L_TAKE(dk_ev[k->at + i].layer);
        for (take = 0; take < 32 && !(seen >> take & 1u); take++)
            ;
        if (take >= cur && (dk_rec_mask >> (best % DK_NTRACK) & 1u) && best / DK_NTRACK == (dk_cur_part & 7))
            return 0;                              /* (the oldest is the one being recorded) */
        snap_take(usnap, &un, NSNAP, ubuf, 1024, best, 0);
        rn = 0;
        for (i = 0, w = 0; i < k->n; i++) {
            const dk_ev_t *e = &dk_ev[k->at + i];
            if ((e->fl & EV_MOTION) || (int)DK_L_TAKE(e->layer) != take)
                dk_ev[k->at + w++] = *e;
        }
        if (w == k->n)
            return 0;
        clip_close(best, w);
    }
    song_mem_dropped = 1;
    return 1;
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
    if (rec_on && only_t < 0) {
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
            int cur = dk_rec_take[r.clip % DK_NTRACK];
            if (cur < 1 || cur > 31)
                cur = pass > 31 ? 31 : pass;
            if (r.pass == cur && dk_recording && r.ev.pos > now)
                r.ev.fl |= EV_FRESH;
        }
        if (dk_song.clip[r.clip].len == 0 && r.ev.pos + r.ev.dur > clip_len(r.clip))
            r.ev.dur = (uint16_t)(clip_len(r.clip) - r.ev.pos);
        if (!(r.ev.fl & EV_MOTION)) {    /* the same note at the same place in the same take: there once (a drummer's step
                                          * caught twice as the take began) */
            const dk_clip_t *cc = &dk_song.clip[r.clip];
            int i, dup = 0;
            for (i = 0; i < cc->n && !dup; i++) {
                const dk_ev_t *e = &dk_ev[cc->at + i];
                dup = !(e->fl & EV_MOTION) && e->pos == r.ev.pos && e->note == r.ev.note && DK_L_TAKE(e->layer) == DK_L_TAKE(r.ev.layer);
            }
            if (dup)
                continue;
        }
        {
            int tries = DK_NCLIP;
            while (dk_song.nev >= DK_NEV && tries-- > 0 && mem_make_room(pass))
                ;
            if (dk_song.nev >= DK_NEV) {       /* nothing to drop: said, not silent */
                song_mem_full = 1;
                continue;
            }
        }
        if (!(r.ev.fl & EV_MOTION))              /* a take is never born muted (a layer mute left on that number from before) */
            dk_song.lmute[r.clip] &= ~(1u << DK_L_TAKE(r.ev.layer));
        if (dk_song_insert(r.clip, &r.ev) >= 0 && !(r.ev.fl & EV_MOTION) && !DK_L_SND(r.ev.layer)) {
            take_pend[r.clip] = (uint8_t)(DK_L_TAKE(r.ev.layer) + 1u);
            if (last_sess[r.clip] != rec_sess) {     /* the first note of this REC press in the clip: its take is where it starts */
                last_sess[r.clip] = rec_sess;
                last_from[r.clip] = (uint8_t)DK_L_TAKE(r.ev.layer);
            }
        }
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
    if (++rec_sess == 0) {               /* (wrapped: nothing older is "the last take" any more) */
        rec_sess = 1;
        for (c = 0; c < DK_NCLIP; c++)
            last_sess[c] = 0;
    }
    rep_started = 0;
    rec_r = dk_recq_w;
    for (c = 0; c < DK_NCLIP; c++) {
        snp_pass[c] = 0xFF;
        rep_pass[c] = 0xFF;
        rep_wrap[c] = 0;
        rep_pos[c] = 0;
    }
}

/* ------------------------------------------------------------------ parts --- */
static uint8_t part_live = 0xFF;         /* which part's instruments are in dk_song.trk (0xFF: not bound yet) */
static void pinst_save(int p)
{
    int t;
    p &= 7;
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_track_t *k = &dk_song.trk[t];
        dk_pinst_t *d = &dk_song.pinst[p][t];
        int i;
        d->eng = k->eng;
        d->preset = k->preset;
        d->fx = k->fx;
        for (i = 0; i < DK_NP; i++)
            d->p[i] = k->p[i];
        for (i = 0; i < 3; i++)
            d->env[i] = k->env[i];
        d->macro = k->macro;
        d->fxamt = k->fxamt;
        d->fxa = k->fxa;
        d->fxb = k->fxb;
    }
}
static void pinst_load(int p)
{
    int t;
    p &= 7;
    for (t = 0; t < DK_NTRACK; t++) {
        dk_track_t *k = &dk_song.trk[t];
        const dk_pinst_t *d = &dk_song.pinst[p][t];
        int i;
        k->eng = d->eng;
        k->preset = d->preset;
        k->fx = d->fx;
        for (i = 0; i < DK_NP; i++)
            k->p[i] = d->p[i];
        for (i = 0; i < 3; i++)
            k->env[i] = d->env[i];
        k->macro = d->macro;
        k->fxamt = d->fxamt;
        k->fxa = d->fxa;
        k->fxb = d->fxb;
    }
}
/* the part that is about to sound: keep the one we leave, bring this one's instruments */
void part_inst_sync(int p)
{
    int i;
    p &= 7;
    if (!dk_song.pinst_init) {
        for (i = 0; i < DK_NPART; i++)
            pinst_save(i);               /* every part starts as the tracks are now */
        dk_song.pinst_init = 1;
        part_live = (uint8_t)p;
        return;
    }
    if (part_live == 0xFF)
        part_live = (uint8_t)(dk_song.cur_part & 7);
    if (part_live == (uint8_t)p)
        return;
    pinst_save(part_live);
    pinst_load(p);
    part_live = (uint8_t)p;
}
/* the tracks as they are now: every part starts from them (then each one keeps its own) */
void part_inst_capture(int p)
{
    int i;
    p &= 7;
    for (i = 0; i < DK_NPART; i++)
        pinst_save(i);
    dk_song.pinst_init = 1;
    part_live = (uint8_t)p;
    wundo = wredo = 0;                   /* a new song is not the wiped road (song_wipe arms the undo after this) */
}
/* what the tracks are playing, written into the part they belong to (a save must not keep a stale slot) */
void part_inst_flush(void)
{
    if (!dk_song.pinst_init)
        return;
    if (part_live == 0xFF)
        part_live = (uint8_t)(dk_song.cur_part & 7);
    pinst_save(part_live);
}
void part_inst_boot(void)
{
    part_live = dk_song.pinst_init ? (uint8_t)(dk_song.cur_part & 7) : 0xFF;
    wundo = wredo = 0;                   /* a project just loaded */
}
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
                clip_clear_all(to * DK_NTRACK + t);
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
    if (dk_song.pinst_init && part_live == (uint8_t)from)
        pinst_save(from);                /* the knobs turned since we arrived are still only in the tracks */
    part_inst_sync(from);
    for (t = 0; t < DK_NTRACK; t++)
        dk_song.pinst[to][t] = dk_song.pinst[from][t];
    if (part_live == (uint8_t)to)
        pinst_load(to);
    song_edits++;
    pc_edits = song_edits;
    dk_unlock();
    return 1;
}
/* SNAPSHOT: a part's clips taken (1), or not (0: more than SNAP_EV events); the one before is forgotten */
int snapshot_take(int part)
{
    int t, n = 0, i;
    part &= 7;
    rec_flush();
    snp.ok = 0;
    for (t = 0; t < DK_NTRACK; t++)
        n += dk_song.clip[part * DK_NTRACK + t].n;
    if (n > SNAP_EV)
        return 0;
    wundo = wredo = 0;                   /* those bytes are the wiped song's until now */
    n = 0;
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_clip_t *c = &dk_song.clip[part * DK_NTRACK + t];
        for (i = 0; i < c->n; i++)
            sbuf[n + i] = dk_ev[c->at + i];
        n += c->n;
        snp.n[t] = c->n;
        snp.len[t] = c->len;
        snp.layers[t] = c->layers;
        snp.lmute[t] = dk_song.lmute[part * DK_NTRACK + t];
    }
    snp.part = (uint8_t)part;
    snp.bars = dk_song.part[part].bars;
    snp.mute = dk_song.part[part].mute;
    snp.ok = 1;
    return 1;
}
int snapshot_part(void) { return snp.ok ? snp.part : -1; }
/* the part as it was taken: 1 done (one undo step), 0 no snapshot, -1 the events do not fit now */
int snapshot_recall(void)
{
    int t, pass, at, now = 0, tot = 0, p = snp.part;
    if (!snp.ok)
        return 0;
    rec_flush();
    for (t = 0; t < DK_NTRACK; t++) {
        now += dk_song.clip[p * DK_NTRACK + t].n;
        tot += snp.n[t];
    }
    if (dk_song.nev - now + tot > DK_NEV)
        return -1;
    undo_group(1);
    for (t = 0; t < DK_NTRACK; t++)
        if (dk_song.clip[p * DK_NTRACK + t].n || snp.n[t])
            undo_push(p * DK_NTRACK + t);
    undo_push_part(p);
    undo_group(0);
    dk_lock();
    for (pass = 0; pass < 2; pass++) {   /* (the clips that shrink first: the events always fit) */
        at = 0;
        for (t = 0; t < DK_NTRACK; t++) {
            int c = p * DK_NTRACK + t;
            if ((snp.n[t] <= dk_song.clip[c].n) == (pass == 0)) {
                clip_put(c, sbuf + at, snp.n[t]);
                dk_song.clip[c].len = snp.len[t];
                dk_song.clip[c].layers = snp.layers[t];
                dk_song.lmute[c] = snp.lmute[t];
            }
            at += snp.n[t];
        }
    }
    dk_song.part[p].bars = snp.bars;
    dk_song.part[p].mute = snp.mute;
    song_edits++;
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
        static dk_ev_t pt[384] DK_POOL;
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
    static dk_ev_t tmp[384] DK_POOL;
    if (n > 192 || len * 2 > 16 * DK_TPB)
        return;
    undo_push(clip);
    {                                       /* notes past the clip's end (after it was shortened) never sound: they would break the order, so they go */
        int m = 0;
        for (i = 0; i < n; i++)
            if (dk_ev[c->at + i].pos < len)
                tmp[m++] = dk_ev[c->at + i];
        for (i = 0; i < m; i++) {
            tmp[m + i] = tmp[i];
            tmp[m + i].pos = (uint16_t)(tmp[i].pos + len);
        }
        n = m;
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
/* the preset ids: 0..11 block 0, 12..15 the user's, 16.. blocks 1..3 (twelve each) */
int preset_block(int id)                 /* the block an id is in; the user's: -1 */
{
    return id < DK_NFACT ? 0 : id < DK_PBASE ? -1 : 1 + (id - DK_PBASE) / DK_NFACT;
}
int preset_id(int blk, int i)            /* the id of a place in a block */
{
    return blk <= 0 ? i : DK_PBASE + (blk - 1) * DK_NFACT + i;
}
int preset_count(int eng, int blk)       /* the presets of an engine in a block (0: none, it cannot) */
{
    if (blk < 0 || blk >= DK_NBLK || eng < 0 || eng >= TAPE.neng)
        return 0;
    return TAPE.pcount ? TAPE.pcount[eng * DK_NBLK + blk] : (blk ? 0 : DK_NFACT);
}
const dk_preset_t *preset_of(int eng, int slot, dk_preset_t *tmp)
{
    if (eng < 0 || eng >= TAPE.neng)
        eng = 0;
    if (slot < DK_NFACT)
        return &TAPE.presets[eng * DK_NFACT + slot];
    if (slot >= DK_PBASE) {              /* a block's: a stale id (a place that is empty) is the engine's first */
        int b = preset_block(slot), i = (slot - DK_PBASE) % DK_NFACT;
        if (b >= DK_NBLK || i >= preset_count(eng, b) || !TAPE.presets2)
            return &TAPE.presets[eng * DK_NFACT];
        {
            int e, bb, off = 0;                  /* the table has no padding: the places before it are the counts' sum */
            for (e = 0; e <= eng; e++)
                for (bb = 1; bb < DK_NBLK && (e < eng || bb < b); bb++)
                    off += TAPE.pcount[e * DK_NBLK + bb];
            return &TAPE.presets2[off + i];
        }
    }
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
    dk_song.dust = 0;                    /* the sound clean: no dust unless you add it (GLOBAL) */
    dk_song.sidechain = 15;
    dk_song.space = 1;
    dk_song.cur_track = 2;
    dk_song.pinst_init = 0;
    part_live = 0xFF;
    for (i = 0; i < DK_NPART; i++) {
        dk_song.part[i].bars = 4;
        dk_song.part[i].label = (uint8_t)(i % 8);
    }
    for (t = 0; t < DK_NTRACK; t++)
        track_defaults(t);
    reindex();
    song_edits++;
    part_inst_capture(0);                /* the tape's instruments: every part starts here */
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
    wundo = 0;
    wredo = 0;                           /* CLEAR ALL forgets the road's undo too; song_wipe arms it again after */
    pc_part = 0xFF;
    song_edits++;
    dk_unlock();
}

/* this part's instruments back to the tape's defaults (the other parts keep theirs); the sequencer and the mixer stay */
void part_inst_reset(int p)
{
    int t;
    p &= 7;
    dk_lock();
    part_inst_sync(p);
    for (t = 0; t < DK_NTRACK; t++) {
        int seq = dk_song.trk[t].seq, role = dk_song.trk[t].role;
        dk_track_setup(t, TAPE.def_eng[t], TAPE.def_preset[t], seq, role);
    }
    pinst_save(p);
    dk_unlock();
}

/* one track of this part, back to the tape; the part's other tracks and the other parts stay */
void part_inst_reset_track(int p, int t)
{
    int seq, role;
    p &= 7;
    if (t < 0 || t >= DK_NTRACK)
        return;
    dk_lock();
    part_inst_sync(p);
    seq = dk_song.trk[t].seq;
    role = dk_song.trk[t].role;
    dk_track_setup(t, TAPE.def_eng[t], TAPE.def_preset[t], seq, role);
    pinst_save(p);
    dk_unlock();
}

/* the song and its events, into the snapshot buffers (0: more than 1536 events, undo cannot hold it) */
static int wipe_capture(void)
{
    int n, i;
    dk_lock();
    part_inst_flush();
    n = dk_song.nev;
    if (n < 0)
        n = 0;
    if (n > 1024 + 512) {
        dk_unlock();
        return 0;
    }
    (void)sizeof(char[(sizeof(dk_song_t) <= sizeof snapb.ev) ? 1 : -1]);
    snp.ok = 0;                          /* this buffer is the song now, not a part snapshot */
    snapb.song = dk_song;
    for (i = 0; i < n && i < 1024; i++)
        ubuf[i] = dk_ev[i];
    for (; i < n; i++)
        rbuf[i - 1024] = dk_ev[i];
    wnev = (uint16_t)n;
    wlive = part_live;
    dk_unlock();
    return 1;
}
static int wipe_restore(void)            /* the part that was current */
{
    int i, n, part;
    dk_lock();
    dk_song = snapb.song;
    n = wnev;
    if (n > DK_NEV)
        n = DK_NEV;
    for (i = 0; i < n && i < 1024; i++)
        dk_ev[i] = ubuf[i];
    for (; i < n; i++)
        dk_ev[i] = rbuf[i - 1024];
    part_live = wlive;
    song_edits++;
    part = dk_song.cur_part & 7;
    dk_unlock();
    wundo = 0;
    wredo = 1;
    wedit = song_edits;
    return part;
}
static void wipe_apply(void)
{
    int t;
    song_clear_all();                    /* clips, the road, the frozen sounds; undo flags off; the copy stays */
    dk_lock();
    for (t = 0; t < DK_NTRACK; t++)
        track_defaults(t);
    part_inst_capture(0);
    dk_song.cur_part = 0;
    dk_unlock();
}
/* CLEAR ROAD: every part's notes, the road, and every part's instruments back to the tape. UNDO brings it all back */
void song_wipe(void)
{
    int ok;
    rec_flush();                         /* notes still queued are part of what comes back */
    ok = wipe_capture();
    wbusy = 1;
    wipe_apply();
    wbusy = 0;
    wundo = (uint8_t)ok;
    wredo = 0;
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
    dk_song.dust = 0;                    /* (a mood's dust no more: the sound clean) */
    dk_song.space = md->space;
    for (t = 0; t < DK_NTRACK; t++) {
        track_sound_to(t);
        if (md->eng[t] >= 0 && md->eng[t] != dk_song.trk[t].eng)   /* (the track keeps its role: PIANO, KEYS ...) */
            dk_song.trk[t].eng = (uint8_t)md->eng[t];
        if (dk_song.trk[t].role == R_NATURE && md->eng[t] >= 0 && md->eng[t] < TAPE.neng && TAPE.eng[md->eng[t]]->role == R_LEAD)
            dk_song.trk[t].role = R_LEAD;     /* (an older project's NATURE track becomes the LEAD) */
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
/* ERASE THE LAST TAKE (hold REC): the notes the latest REC press recorded (every pass of it, on every track it put notes on)
 * go; older notes, automation (motion) and the rest of the song stay. One undo step. Returns the notes erased (0: nothing) */
int take_erase_last(void)
{
    int c, i, n = 0, any = 0;
    rec_flush();                         /* (what was just played is in the clips) */
    for (c = 0; c < DK_NCLIP; c++) {
        const dk_clip_t *cl = &dk_song.clip[c];
        if (!last_sess[c] || last_sess[c] != rec_sess)
            continue;
        for (i = 0; i < cl->n && !any; i++)
            any = !(dk_ev[cl->at + i].fl & EV_MOTION) && DK_L_TAKE(dk_ev[cl->at + i].layer) >= last_from[c];
    }
    if (!any)
        return 0;
    undo_group(1);
    for (c = 0; c < DK_NCLIP; c++) {
        dk_clip_t *cl = &dk_song.clip[c];
        int top = 0, k = 0;
        if (!last_sess[c] || last_sess[c] != rec_sess)
            continue;
        for (i = 0; i < cl->n && !k; i++)
            k = !(dk_ev[cl->at + i].fl & EV_MOTION) && DK_L_TAKE(dk_ev[cl->at + i].layer) >= last_from[c];
        if (!k)                          /* (nothing of it is left: undone, or erased by hand) */
            continue;
        k = 0;
        undo_push(c);
        dk_lock();
        for (i = cl->n - 1; i >= 0; i--) {
            const dk_ev_t *e = &dk_ev[cl->at + i];
            if (!(e->fl & EV_MOTION) && DK_L_TAKE(e->layer) >= last_from[c]) {
                ev_delete(c, i);
                k++;
            }
        }
        for (i = 0; i < cl->n; i++)
            if ((int)DK_L_TAKE(dk_ev[cl->at + i].layer) > top)
                top = (int)DK_L_TAKE(dk_ev[cl->at + i].layer);
        cl->layers = (uint8_t)top;
        for (i = last_from[c]; i < 32; i++)
            dk_song.lmute[c] &= ~(1u << i);
        if (dk_lsolo_clip == c && dk_lsolo_take >= last_from[c])
            dk_lsolo_clip = 0xFF;
        take_pend[c] = 0;
        dk_unlock();
        last_sess[c] = 0;
        n += k;
    }
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

/* ------------------------------------------------------------ CLIP EDITING --- */
/* CLIP QUANTIZE: every note moved toward the nearest 1/16 by `strength` percent (the track's QUANT); one undo step.
 * Returns how many notes moved. */
int clip_quantize(int clip, int strength)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, len = clip_len(clip), moved = 0;
    if (strength <= 0 || !c->n)
        return 0;
    if (strength > 100)
        strength = 100;
    undo_push(clip);
    dk_lock();
    for (i = 0; i < c->n; i++) {
        dk_ev_t *e = &dk_ev[c->at + i];
        int p = e->pos, g = (p + DK_T16 / 2) / DK_T16 * DK_T16, np;
        if (e->fl & EV_MOTION)
            continue;
        np = p + (g - p) * strength / 100;
        if (np >= len)
            np = 0;
        if (np < 0)
            np = 0;
        if (np != p)
            moved++;
        e->pos = (uint16_t)np;
    }
    clip_sort(clip);
    dk_unlock();
    song_edits++;
    return moved;
}

/* BAR COPY / PASTE: one bar of a clip (its events, bar-relative) kept; pasted over another bar of a clip (that bar's own events go);
 * one undo step. At most BARBUF events. */
#define BARBUF 64
static dk_ev_t barbuf[BARBUF];
static int barbuf_n;
int clip_bars(int clip) { return (clip_len(clip) + DK_TPB - 1) / DK_TPB; }
int bar_copy(int clip, int bar)          /* the events kept (0: an empty bar, which is a copy too), -1 too many */
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, n = 0, lo = bar * DK_TPB, hi = lo + DK_TPB;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (e->pos < lo || e->pos >= hi)
            continue;
        if (n >= BARBUF)
            return -1;
        barbuf[n] = *e;
        barbuf[n].pos = (uint16_t)(e->pos - lo);
        n++;
    }
    barbuf_n = n;
    return n;
}
int bar_paste(int clip, int bar)         /* the events pasted, -1 no room */
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, gone = 0, lo = bar * DK_TPB, hi = lo + DK_TPB;
    for (i = 0; i < c->n; i++)
        if (dk_ev[c->at + i].pos >= lo && dk_ev[c->at + i].pos < hi)
            gone++;
    if (dk_song.nev - gone + barbuf_n > DK_NEV)
        return -1;
    undo_push(clip);
    dk_lock();
    for (i = c->n - 1; i >= 0; i--)
        if (dk_ev[c->at + i].pos >= lo && dk_ev[c->at + i].pos < hi)
            ev_delete(clip, i);
    for (i = 0; i < barbuf_n; i++) {
        dk_ev_t e = barbuf[i];
        e.pos = (uint16_t)(e.pos + lo);
        dk_song_insert(clip, &e);
    }
    dk_unlock();
    song_edits++;
    return barbuf_n;
}
