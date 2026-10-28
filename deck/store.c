/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The projects in the data area (a slot each: a head, the song, its events; the head written last, so a slot is
 * valid only once whole; FNV-1a over the rest), the settings and the user's presets in the store. Saving waits for
 * the transport to rest (an erase stops the sound for a moment). Part of the unity build. */

settings_t set;

#define PROJ_HEAD_MAGIC 0x4A525044u      /* "DPRJ" */
typedef struct {
    uint32_t magic, format, nsong, nev, sum;
    uint32_t rsv[3];
} phead_t;

static uint32_t fnv(uint32_t s, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 0x01000193u;
    return s;
}
static uint32_t song_sum_n(uint32_t nsong)   /* (an older project: its shorter song) */
{
    uint32_t s = fnv(0x811C9DC5u, &dk_song, nsong);
    return fnv(s, dk_ev, (uint32_t)dk_song.nev * sizeof(dk_ev_t));
}
static uint32_t song_sum(void)
{
    uint32_t s = fnv(0x811C9DC5u, &dk_song, sizeof dk_song);
    return fnv(s, dk_ev, (uint32_t)dk_song.nev * sizeof(dk_ev_t));
}

static const phead_t *head_of(int slot)
{
    const phead_t *h = (const phead_t *)plat_data(DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN);
    if (!h || h->magic != PROJ_HEAD_MAGIC || h->format != DK_FORMAT || h->nsong < DK_SONG_V1 || h->nsong > sizeof(dk_song_t) ||
        h->nev > DK_NEV)
        return 0;
    return h;
}

int project_used(int slot) { return (unsigned)slot < DK_NPROJ && head_of(slot) != 0; }

const char *project_name(int slot, char *name)
{
    const phead_t *h = head_of(slot);
    int i;
    name[0] = 0;
    if (h) {
        const dk_song_t *s = (const dk_song_t *)plat_data(DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN + sizeof(phead_t));
        for (i = 0; s && i < 11 && s->name[i]; i++)
            name[i] = s->name[i];
        name[i] = 0;
    }
    return name;
}

/* the events of the pool in the order of the clips again (a permuted clip table points at them out of order, and every edit
 * of the pool - insert, close, reindex - counts on clip i + 1 following clip i): each clip's block rotated into its place, no
 * second buffer (reversals, 8 bytes an event) */
static void ev_rev(uint32_t a, uint32_t b)
{
    while (a + 1 < b) {
        dk_ev_t t = dk_ev[a];
        dk_ev[a++] = dk_ev[--b];
        dk_ev[b] = t;
    }
}
static void clips_repack(void)
{
    uint32_t cum = 0, tot = 0, i, j;
    for (i = 0; i < DK_NCLIP; i++)
        tot += dk_song.clip[i].n;
    if (tot != dk_song.nev || tot > DK_NEV)
        return;
    for (i = 0; i < DK_NCLIP; i++) {
        dk_clip_t *c = &dk_song.clip[i];
        uint32_t a = c->at, n = c->n;
        if (n && a != cum && a > cum && a + n <= tot) {
            ev_rev(cum, a);                       /* [cum, a) [a, a + n) -> [a, a + n) [cum, a) */
            ev_rev(a, a + n);
            ev_rev(cum, a + n);
            for (j = i + 1; j < DK_NCLIP; j++)
                if (dk_song.clip[j].n && dk_song.clip[j].at >= cum && dk_song.clip[j].at < a)
                    dk_song.clip[j].at = (uint16_t)(dk_song.clip[j].at + n);
        }
        c->at = (uint16_t)cum;
        cum += n;
    }
}

/* older projects: the tracks in the order of today's (PIANO KEYS PAD BASS LEAD DRUMKIT), by their roles, when the
 * six roles are six different places; the clips (their layer mutes), the frozen sounds, the parts' mutes, the road's drops,
 * the bypasses, SPLIT's track and FOLLOW's sources go with them */
static void song_track_order(void)
{
    static const uint8_t RANK[R_N] = {5, 3, 0, 1, 2, 4, 1, 4};   /* DRUM BASS KEYS(piano) ARP PAD LEAD BELL NATURE (-> LEAD) */
    static dk_track_t tk[DK_NTRACK];
    static dk_clip_t cl[DK_NCLIP];
    uint8_t to[DK_NTRACK], seen = 0, same = 1;
    int t, p, j;
    for (t = 0; t < DK_NTRACK; t++) {
        to[t] = RANK[dk_song.trk[t].role % R_N];
        if (seen >> to[t] & 1u)
            return;                          /* two of a kind: left as it is */
        seen |= (uint8_t)(1u << to[t]);
        same &= to[t] == t;
    }
    if (same)
        return;
    {
        static dk_lane_t ln[DK_NTRACK][8];
        for (t = 0; t < DK_NTRACK; t++)
            for (j = 0; j < 8; j++)
                ln[to[t]][j] = dk_song.lane[t][j];
        for (t = 0; t < DK_NTRACK; t++)
            for (j = 0; j < 8; j++)
                dk_song.lane[t][j] = ln[t][j];
    }
    for (t = 0; t < DK_NTRACK; t++)
        tk[to[t]] = dk_song.trk[t];
    for (p = 0; p < DK_NPART; p++)
        for (t = 0; t < DK_NTRACK; t++)
            cl[p * DK_NTRACK + to[t]] = dk_song.clip[p * DK_NTRACK + t];
    for (t = 0; t < DK_NTRACK; t++) {
        dk_song.trk[t] = tk[t];
        for (j = 0; j < 2; j++)
            if (dk_song.trk[t].mod[j][0] == MOD_FOLLOW)
                dk_song.trk[t].mod[j][2] = to[dk_song.trk[t].mod[j][2] % DK_NTRACK];
    }
    for (p = 0; p < DK_NCLIP; p++)
        dk_song.clip[p] = cl[p];
    {                                    /* the rest that is a track's: layer mutes (per clip), frozen sounds, bypasses, banks */
        static uint32_t lm[DK_NCLIP];
        static dk_snd_t sn[DK_NTRACK][DK_NSND];
        uint8_t rs[DK_NTRACK], fb[DK_NTRACK];
        for (p = 0; p < DK_NPART; p++)
            for (t = 0; t < DK_NTRACK; t++)
                lm[p * DK_NTRACK + to[t]] = dk_song.lmute[p * DK_NTRACK + t];
        for (p = 0; p < DK_NCLIP; p++)
            dk_song.lmute[p] = lm[p];
        for (t = 0; t < DK_NTRACK; t++) {
            for (j = 0; j < DK_NSND; j++)
                sn[to[t]][j] = dk_song.snd[t][j];
            rs[to[t]] = dk_song.rsv[t];
            fb[to[t]] = dk_song.fbank[t];
        }
        for (t = 0; t < DK_NTRACK; t++) {
            for (j = 0; j < DK_NSND; j++)
                dk_song.snd[t][j] = sn[t][j];
            dk_song.rsv[t] = rs[t];
            dk_song.fbank[t] = fb[t];
        }
        for (p = 0; p < DK_NPART; p++) {
            uint8_t m = dk_song.part[p].mute, n = 0;
            for (t = 0; t < DK_NTRACK; t++)
                if (m >> t & 1u)
                    n |= (uint8_t)(1u << to[t]);
            dk_song.part[p].mute = n;
        }
    }
    clips_repack();
    for (j = 0; j < DK_NROAD; j++) {
        uint8_t d = dk_song.road[j].drop, n = 0;
        for (t = 0; t < DK_NTRACK; t++)
            if (d >> t & 1u)
                n |= (uint8_t)(1u << to[t]);
        dk_song.road[j].drop = n;
    }
    dk_song.harm_track = to[dk_song.harm_track % DK_NTRACK];
    dk_song.cur_track = to[dk_song.cur_track % DK_NTRACK];
}

int project_load(int slot)
{
    const phead_t *h = head_of(slot);
    uint32_t at = DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN;
    const uint8_t *src;
    uint8_t *dst;
    uint32_t i, n;
    dk_lock();
    takes_forget();
    if (!h)
        goto empty;
    src = plat_data(at + sizeof(phead_t));
    if (!src)
        goto empty;
    dst = (uint8_t *)&dk_song;
    for (i = 0; i < sizeof dk_song; i++)   /* an older, shorter song: the rest zeros (the defaults) */
        dst[i] = i < h->nsong ? src[i] : 0;
    dk_lsolo_clip = 0xFF;
    n = h->nev * sizeof(dk_ev_t);
    src = plat_data(at + sizeof(phead_t) + h->nsong);
    dst = (uint8_t *)dk_ev;
    for (i = 0; src && i < n; i++)
        dst[i] = src[i];
    if (!src || dk_song.magic != DK_MAGIC || dk_song.nev != h->nev || song_sum_n(h->nsong) != h->sum)
        goto empty;
    for (i = 0; i < dk_song.nev; i++) {    /* nothing waits for a pass that is long gone */
        dk_ev[i].fl &= (uint8_t)~EV_FRESH;
        if (dk_ev[i].layer > 0x1Fu)        /* older projects: the take had 7 bits, now 5 (the sound's three are 0: the track's own) */
            dk_ev[i].layer = 0x1Fu;
    }
    for (i = 0; i < DK_NCLIP; i++)
        if (dk_song.clip[i].layers > 31)
            dk_song.clip[i].layers = 31;
    for (i = 0; i < DK_NPART; i++)         /* the parts' track mutes: six bits (older projects: 0, none) */
        dk_song.part[i].mute &= DK_PMUTE_ALL;
    dk_pm_x = 0;
    song_track_order();
    if (dk_song.dust == 25)                /* the old default dust (every song had it): off, the sound clean */
        dk_song.dust = 0;
    for (i = 0; i < DK_NTRACK; i++) {      /* older projects: ARP was a sequencer; LIVE, DREAM, FINGER, HOLD, TOMBOLA are SEQ now */
        dk_track_t *k = &dk_song.trk[i];
        if (k->seq == SQ_ARP)
            k->arp_opt |= ARPO_ON;
        if (k->seq != SQ_STEP && k->seq != SQ_ENDLESS &&
            !(k->seq == SQ_GROOVE && (TAPE.eng[k->eng % TAPE.neng]->flags & EF_DRUM)))   /* GROOVE: a drum track's */
            k->seq = SQ_STEP;
        if (k->role == R_NATURE || TAPE.eng[k->eng % TAPE.neng]->role == R_NATURE) {   /* NATURE is gone: LEAD there */
            int p;                                                         /* (its notes were the world's drones) */
            for (p = 0; p < DK_NPART; p++)
                clip_clear_all(p * DK_NTRACK + i);
            {
                int d, le = 11;                          /* the tape's default LEAD engine (where its LEAD track is) */
                for (d = 0; d < DK_NTRACK; d++)
                    if (TAPE.def_role[d] == R_LEAD)
                        le = TAPE.def_eng[d];
                dk_track_setup(i, le, 0, k->seq, R_LEAD);
            }
        }
    }
    if (dk_song.pinst_init)
        part_inst_boot();                    /* the tracks already are this part's instruments */
    else
        part_inst_capture(dk_song.cur_part); /* an older project: every part starts as the tracks are */
    dk_unlock();
    return 0;
empty:
    dk_song_clear();
    dk_unlock();
    return -1;
}

/* the slot's image: the head, the song, the events, one after another (byte off of it, from RAM) */
static const uint8_t *img_at(uint32_t off, uint32_t *run)
{
    uint32_t hs = (uint32_t)sizeof(phead_t), ss = (uint32_t)sizeof dk_song;
    if (off < hs + ss) {
        *run = hs + ss - off;
        return (const uint8_t *)&dk_song + (off - hs);
    }
    *run = (uint32_t)dk_song.nev * sizeof(dk_ev_t) - (off - hs - ss);
    return (const uint8_t *)dk_ev + (off - hs - ss);
}
static int img_same(uint32_t at, uint32_t a, uint32_t b)   /* the flash already holds [a, b) of it (a past the head) */
{
    const uint8_t *f = plat_data(at + a);
    while (f && a < b) {
        uint32_t run, i;
        const uint8_t *m = img_at(a, &run);
        if (run > b - a)
            run = b - a;
        for (i = 0; i < run; i++)
            if (f[i] != m[i])
                return 0;
        a += run;
        f += run;
    }
    return f != 0;
}
static int img_write(uint32_t at, uint32_t a, uint32_t b)
{
    while (a < b) {
        uint32_t run;
        const uint8_t *m = img_at(a, &run);
        if (run > b - a)
            run = b - a;
        if (plat_data_write(at + a, m, run))
            return -1;
        a += run;
    }
    return 0;
}

/* around the flash's erases: the sound faded out first (core.c, dk_hush), so the ~17 ms stop is silent; nested */
static int hush_depth;
static void hush_begin(void)
{
    uint32_t t0;
    if (hush_depth++)
        return;
    dk_hush = 1;
#ifndef PLAT_HOST                                /* (the simulator renders between its steps: no waiting there) */
    t0 = plat_ms();
    while (dk_hushed < 3 && plat_ms() - t0 < 80u)   /* three silent blocks: the DMA's both halves are quiet */
        ;
#else
    (void)t0;
#endif
}
static void hush_end(void)
{
    if (hush_depth && !--hush_depth)
        dk_hush = 0;
}

int project_save(int slot)
{
    static phead_t h;
    uint32_t at = DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN, s;
    const phead_t *old = head_of(slot);
    if (dk_playing || dk_recording || (unsigned)slot >= DK_NPROJ)
        return -1;
    dk_lock();
    part_inst_flush();                       /* the knobs turned since this part was entered */
    dk_unlock();
    h.magic = PROJ_HEAD_MAGIC;
    h.format = DK_FORMAT;
    h.nsong = sizeof dk_song;
    h.nev = dk_song.nev;
    h.sum = song_sum();
    if (old && old->sum == h.sum && old->nev == h.nev)
        return 0;                          /* unchanged: spare the flash */
    /* only the sectors that changed: an erase holds the interrupts ~17 ms (the sound, the keys' scan and the lights
     * stop: the device blinked and stuttered when all nine went at once), and a preset or a knob changes one */
    hush_begin();
    {
        uint32_t len = (uint32_t)sizeof h + sizeof dk_song + (uint32_t)dk_song.nev * sizeof(dk_ev_t);
        for (s = 0; s < len && s < DK_PROJ_SPAN; s += 0x1000u) {
            uint32_t end = s + 0x1000u < len ? s + 0x1000u : len, from = s ? s : (uint32_t)sizeof h;
            if (s && img_same(at, s, end))
                continue;
            if (plat_data_erase(at + s) || img_write(at, from, end)) {
                hush_end();
                return -1;
            }
        }
    }
    if (plat_data_write(at, &h, sizeof h)) {  /* the head last (its sector was erased: it always changes) */
        hush_end();
        return -1;
    }
    hush_end();
    return head_of(slot) && head_of(slot)->sum == h.sum ? 0 : -1;
}

static uint8_t settings_due;             /* a setting changed: written with the next save in the silence (ui.c) */
void settings_save_later(void) { settings_due = 1; }
void settings_save(void)
{
    settings_due = 0;
    set.magic = SET_MAGIC;
    set.metro[0] = dk_metro.mode;
    set.metro[1] = dk_metro.count;
    set.metro[2] = dk_metro.sound;
    set.metro[3] = dk_metro.div;
    set.metro[4] = dk_metro.vol;
    set.metro[5] = dk_metro.on_note ? 1 : 2;   /* (2 = COUNT IN; 0, as in any older store, is ON NOTE now: the default) */
    set.midi_notes = dk_midi_notes;
    set.midi_clock = dk_midi_clock;
    set.lat = (uint8_t)(dk_rec_lat + 1u);
    set.nolock = (uint8_t)!dk_lock_take;
    set.noarm_old = 0;
    set.recarm = dk_rec_arm ? 1 : 0;
    set.recmode = dk_rec_mode ? 1 : 0;
    set.emptypart = (uint8_t)!dk_newpart_copy;
    {                                    /* as it was last written: no erase */
        static uint8_t last[sizeof set];
        static int have;
        const uint8_t *b = (const uint8_t *)&set;
        uint32_t i, same = have;
        for (i = 0; i < sizeof set && same; i++)
            same = b[i] == last[i];
        if (same)
            return;
        hush_begin();
        plat_store_save(OBJ_SET, &set, sizeof set);
        hush_end();
        for (i = 0; i < sizeof set; i++)
            last[i] = b[i];
        have = 1;
    }
}
static void settings_load(void)
{
    if (plat_store_load(OBJ_SET, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        set.magic = SET_MAGIC;
        set.proj = 0;
        set.midi_notes = 1;
        set.midi_clock = 1;
        set.midi_in = 1;
        set.metro[0] = 0;                /* the click OFF (METRO: ON REC / ALWAYS), the count-in 1 bar, quiet */
        set.metro[1] = 1;
        set.metro[2] = TAPE.metro_sound;
        set.metro[3] = 0;
        set.metro[4] = 40;
        set.metro[5] = 0;
    }
    dk_metro.mode = set.metro[0] % 3;
    dk_metro.count = set.metro[1] ? set.metro[1] : 1;
    dk_metro.sound = set.metro[2] % METRO_N;
    dk_metro.div = set.metro[3] % 3;
    dk_metro.vol = set.metro[4] > 100 ? 100 : set.metro[4];
    dk_metro.on_note = set.metro[5] != 2;   /* REC stopped: the take starts with the first note, as on OP-1 */
    dk_midi_notes = set.midi_notes;
    dk_midi_clock = set.midi_clock;
    dk_rec_lat = set.lat ? (uint8_t)(set.lat - 1u > 30u ? 30u : set.lat - 1u) : 9u;
    dk_lock_take = set.nolock ? 0 : 1;
    dk_rec_arm = 1;                     /* REC arms (the tape), REC again records: always now (set.recarm no longer read) */
    dk_rec_mode = set.recmode == 1 ? 1 : 0;
    dk_newpart_copy = set.emptypart ? 0 : 1;
    if (set.proj >= DK_NPROJ)
        set.proj = 0;
}
void user_save(void)
{
    dk_user.magic = USER_MAGIC;
    hush_begin();
    plat_store_save(OBJ_USER, &dk_user, sizeof dk_user);
    hush_end();
}
static void user_load(void)
{
    uint32_t per = (uint32_t)sizeof dk_user.p[0], v1 = (uint32_t)sizeof dk_user.magic + DK_UENG_V1 * per,
             v2 = (uint32_t)sizeof dk_user.magic + DK_UENG_V2 * per;
    int n = plat_store_load(OBJ_USER, &dk_user, sizeof dk_user);
    if ((n != (int)sizeof dk_user && n != (int)v1 && n != (int)v2) || dk_user.magic != USER_MAGIC) {
        uint8_t *b = (uint8_t *)&dk_user;
        uint32_t i;
        for (i = 0; i < sizeof dk_user; i++)
            b[i] = 0;
        dk_user.magic = USER_MAGIC;
    } else if (n != (int)sizeof dk_user) {       /* saved with eight or ten engines: the new ones' places empty */
        uint8_t *b = (uint8_t *)&dk_user;
        uint32_t i;
        for (i = (uint32_t)n; i < sizeof dk_user; i++)
            b[i] = 0;
    }
}
