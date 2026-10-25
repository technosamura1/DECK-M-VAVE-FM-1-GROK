/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* EXPORT: the song as a SysEx stream over USB-MIDI, for tools/deck_export.py and web/export.html to write a .mid
 * (Type 1: the tempo, six named tracks, the parts as markers, the road's tricks applied, the clip's motion as CC).
 *
 * A frame: F0 7D 44 4B <kind> <seq lo> <seq hi> <payload, 7-bit> <sum> F7. kind 1 the head (tempo, PPQ, bars, the
 * name), 2 a track's name, 3 events, 4 a marker, 0x7F the end (how many events). The payload's 8-bit bytes are
 * packed seven to eight (a byte of high bits, then seven of the rest); the sum: their total & 0x7F. An event: its
 * tick (4 bytes, little end), the track, a MIDI status, two data bytes. A few frames a UI frame (the main loop). */

typedef struct {
    uint32_t tick;
    uint8_t trk, st, d1, d2;
} xev_t;

static struct {
    uint8_t on, stage;
    uint16_t seq;
    uint32_t done, total;                /* events sent, all of them */
    uint8_t trk, mark;
} xp;

static const char *const PART_LABEL[8] = {"INTRO", "VERSE", "PRE", "CHORUS", "DROP", "BRIDGE", "BREAK", "OUTRO"};

/* every event of the song, in a fixed order; the ones skip.. skip+max into out; how many in all through *all */
static int export_events(uint32_t skip, int max, xev_t *out, uint32_t *all)
{
    uint32_t k = 0, start = 0;
    int got = 0, b, nb = dk_song.nroad ? dk_song.nroad : 1;
    for (b = 0; b < nb; b++) {
        dk_block_t blk = dk_song.nroad ? dk_song.road[b] : (dk_block_t){dk_song.cur_part, 1, 0, 0};
        int part = blk.part & 7, reps = blk.reps ? blk.reps : 1, plen = (dk_song.part[part].bars ? dk_song.part[part].bars : 1) * DK_TPB, r, t;
        for (r = 0; r < reps; r++) {
            for (t = 0; t < DK_NTRACK; t++) {
                const dk_clip_t *c = CLIP(part, t);
                const dk_engine_t *e = TAPE.eng[dk_song.trk[t].eng % TAPE.neng];
                int clen = c->len ? c->len : plen, i, cyc;
                if ((blk.drop >> t & 1u) || (dk_song.part[part].mute >> t & 1u) || !c->n)   /* (dropped, or muted in the part: not in the file) */
                    continue;
                for (i = 0; i < c->n; i++) {
                    const dk_ev_t *ev = &dk_ev[c->at + i];
                    for (cyc = 0; cyc < 64; cyc++) {
                        int p = ev->pos, dur = ev->dur, rep2, note = ev->note;
                        uint32_t at;
                        if (blk.tricks & TR_REVERSE)
                            p = ((clen - ev->pos - (ev->dur < clen ? ev->dur : 1)) % clen + clen) % clen;
                        p += cyc * clen;
                        if (blk.tricks & TR_HALF) {
                            p *= 2;
                            dur *= 2;
                        }
                        if (blk.tricks & TR_DOUBLE) {
                            p /= 2;
                            dur /= 2;
                        }
                        if (p >= plen)
                            break;
                        at = start + (uint32_t)p;
                        if (!(e->flags & EF_DRUM)) {
                            if (blk.tricks & TR_UP)
                                note += 12;
                            if (blk.tricks & TR_DOWN)
                                note -= 12;
                        }
                        for (rep2 = 0; rep2 < 2; rep2++) {   /* a note: on, off; a move: a CC */
                            xev_t x;
                            x.tick = at;
                            x.trk = (uint8_t)t;
                            if (ev->fl & EV_MOTION) {
                                if (rep2 || (ev->note != T_P0 && ev->note != T_VOL))
                                    continue;
                                x.st = (uint8_t)(0xB0 | t);
                                x.d1 = ev->note == T_VOL ? 7 : 74;
                                x.d2 = (uint8_t)(ev->vel * 127 / 100);
                            } else if (!rep2) {
                                x.st = (uint8_t)(0x90 | (e->flags & EF_DRUM ? 9 : t));
                                x.d1 = (uint8_t)(note & 127);
                                x.d2 = ev->vel ? ev->vel : 1;
                            } else {
                                x.tick = at + (uint32_t)(dur > 1 ? dur - 1 : 1);
                                x.st = (uint8_t)(0x80 | (e->flags & EF_DRUM ? 9 : t));
                                x.d1 = (uint8_t)(note & 127);
                                x.d2 = 0;
                            }
                            if (k >= skip && got < max)
                                out[got++] = x;
                            k++;
                        }
                    }
                }
            }
            start += (uint32_t)plen;
        }
    }
    if (all)
        *all = k;
    return got;
}

static uint32_t export_length(void)
{
    uint32_t len = 0;
    int b;
    for (b = 0; b < (dk_song.nroad ? dk_song.nroad : 1); b++) {
        int part = dk_song.nroad ? dk_song.road[b].part & 7 : dk_song.cur_part, reps = dk_song.nroad ? dk_song.road[b].reps : 1;
        len += (uint32_t)((dk_song.part[part].bars ? dk_song.part[part].bars : 1) * DK_TPB * (reps ? reps : 1));
    }
    return len;
}

/* a frame from 8-bit payload bytes */
static int frame_make(uint8_t *f, int kind, const uint8_t *p, int n)
{
    int k = 0, i, sum = 0;
    f[k++] = 0xF0;
    f[k++] = 0x7D;
    f[k++] = 0x44;
    f[k++] = 0x4B;
    f[k++] = (uint8_t)kind;
    f[k++] = (uint8_t)(xp.seq & 0x7F);
    f[k++] = (uint8_t)(xp.seq >> 7 & 0x7F);
    for (i = 0; i < n; i += 7) {
        int j, hi = 0, m = n - i < 7 ? n - i : 7;
        for (j = 0; j < m; j++)
            hi |= (p[i + j] >> 7 & 1) << j;
        f[k++] = (uint8_t)hi;
        sum += hi;
        for (j = 0; j < m; j++) {
            f[k++] = (uint8_t)(p[i + j] & 0x7F);
            sum += p[i + j] & 0x7F;
        }
    }
    f[k++] = (uint8_t)(sum & 0x7F);
    f[k++] = 0xF7;
    xp.seq++;
    return k;
}

void export_start(void)
{
    xp.on = 1;
    xp.stage = 0;
    xp.seq = 0;
    xp.done = 0;
    xp.trk = 0;
    xp.mark = 0;
    export_events(0, 0, 0, &xp.total);
}
int export_busy(void) { return xp.on; }
int export_pct(void) { return xp.total ? (int)(xp.done * 100u / xp.total) : 0; }

/* the next frame (into f, up to 300 bytes); 0 when done */
int export_next(uint8_t *f)
{
    uint8_t p[200];
    int n = 0, i;
    if (!xp.on)
        return 0;
    switch (xp.stage) {
    case 0: {                                  /* the head: tempo, PPQ, length (ticks), the name */
        uint32_t len = export_length();
        p[n++] = dk_song.tempo;
        p[n++] = DK_PPQ;
        p[n++] = DK_NTRACK;
        for (i = 0; i < 4; i++)
            p[n++] = (uint8_t)(len >> (8 * i));
        for (i = 0; i < 11 && dk_song.name[i]; i++)
            p[n++] = (uint8_t)dk_song.name[i];
        xp.stage = 1;
        return frame_make(f, 1, p, n);
    }
    case 1: {                                  /* the tracks' names: the engine and the preset */
        const dk_track_t *k = &dk_song.trk[xp.trk];
        const dk_engine_t *e = TAPE.eng[k->eng % TAPE.neng];
        dk_preset_t tmp;
        const dk_preset_t *pr = preset_of(k->eng, k->preset, &tmp);
        const char *s;
        p[n++] = xp.trk;
        p[n++] = (uint8_t)(e->flags & EF_DRUM ? 9 : xp.trk);
        for (s = e->name; *s; s++)
            p[n++] = (uint8_t)*s;
        p[n++] = ' ';
        for (s = pr->name; *s && n < 40; s++)
            p[n++] = (uint8_t)*s;
        if (++xp.trk >= DK_NTRACK)
            xp.stage = 2;
        return frame_make(f, 2, p, n);
    }
    case 2: {                                  /* the markers: each block's part */
        uint32_t at = 0;
        int b, nb = dk_song.nroad ? dk_song.nroad : 1;
        const char *s;
        for (b = 0; b < xp.mark; b++) {
            int part = dk_song.nroad ? dk_song.road[b].part & 7 : dk_song.cur_part;
            at += (uint32_t)((dk_song.part[part].bars ? dk_song.part[part].bars : 1) * DK_TPB *
                             (dk_song.nroad && dk_song.road[b].reps ? dk_song.road[b].reps : 1));
        }
        {
            int part = dk_song.nroad ? dk_song.road[xp.mark].part & 7 : dk_song.cur_part;
            for (i = 0; i < 4; i++)
                p[n++] = (uint8_t)(at >> (8 * i));
            for (s = PART_LABEL[dk_song.part[part].label & 7]; *s; s++)
                p[n++] = (uint8_t)*s;
            p[n++] = ' ';
            p[n++] = (uint8_t)('A' + part);
        }
        if (++xp.mark >= nb)
            xp.stage = 3;
        return frame_make(f, 4, p, n);
    }
    case 3: {                                  /* the events, 24 a frame */
        xev_t x[24];
        int got = export_events(xp.done, 24, x, 0);
        for (i = 0; i < got; i++) {
            int b;
            for (b = 0; b < 4; b++)
                p[n++] = (uint8_t)(x[i].tick >> (8 * b));
            p[n++] = x[i].trk;
            p[n++] = x[i].st;
            p[n++] = x[i].d1;
            p[n++] = x[i].d2;
        }
        xp.done += (uint32_t)got;
        if (xp.done >= xp.total || !got)
            xp.stage = 4;
        if (!got)
            break;
        return frame_make(f, 3, p, n);
    }
    default: break;
    }
    for (i = 0; i < 4; i++)                    /* the end: the count */
        p[i] = (uint8_t)(xp.total >> (8 * i));
    xp.on = 0;
    return frame_make(f, 0x7F, p, 4);
}
