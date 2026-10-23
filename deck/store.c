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

/* older projects: the tracks in the order of today's (PIANO KEYS PAD BASS DRUMS NATURE), by their roles, when the
 * six roles are six different places; the clips, the road's drops, SPLIT's track and FOLLOW's sources go with them */
static void song_track_order(void)
{
    static const uint8_t RANK[R_N] = {4, 3, 0, 1, 2, 1, 1, 5};   /* DRUM BASS KEYS(piano) ARP PAD LEAD BELL NATURE */
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
    for (j = 0; j < DK_NROAD; j++) {
        uint8_t d = dk_song.road[j].drop, n = 0;
        for (t = 0; t < DK_NTRACK; t++)
            if (d >> t & 1u)
                n |= (uint8_t)(1u << to[t]);
        dk_song.road[j].drop = n;
    }
    dk_song.harm_track = to[dk_song.harm_track % DK_NTRACK];
    dk_song.cur_track = to[dk_song.cur_track % DK_NTRACK];
    dk_song.ball_trk = to[dk_song.ball_trk % DK_NTRACK];
}

int project_load(int slot)
{
    const phead_t *h = head_of(slot);
    uint32_t at = DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN;
    const uint8_t *src;
    uint8_t *dst;
    uint32_t i, n;
    dk_lock();
    if (!h)
        goto empty;
    src = plat_data(at + sizeof(phead_t));
    if (!src)
        goto empty;
    dst = (uint8_t *)&dk_song;
    for (i = 0; i < sizeof dk_song; i++)   /* an older, shorter song: the rest zeros (the defaults) */
        dst[i] = i < h->nsong ? src[i] : 0;
    n = h->nev * sizeof(dk_ev_t);
    src = plat_data(at + sizeof(phead_t) + h->nsong);
    dst = (uint8_t *)dk_ev;
    for (i = 0; src && i < n; i++)
        dst[i] = src[i];
    if (!src || dk_song.magic != DK_MAGIC || dk_song.nev != h->nev || song_sum_n(h->nsong) != h->sum)
        goto empty;
    for (i = 0; i < dk_song.nev; i++)      /* nothing waits for a pass that is long gone */
        dk_ev[i].fl &= (uint8_t)~EV_FRESH;
    song_track_order();
    for (i = 0; i < DK_NTRACK; i++) {      /* older projects: ARP was a sequencer, ENDLESS and HOLD too */
        dk_track_t *k = &dk_song.trk[i];
        if (k->seq == SQ_ARP)
            k->arp_opt |= ARPO_ON;
        if (k->seq == SQ_ARP || k->seq == SQ_ENDLESS || k->seq == SQ_HOLD)
            k->seq = SQ_LIVE;
    }
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

int project_save(int slot)
{
    static phead_t h;
    uint32_t at = DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN, s;
    const phead_t *old = head_of(slot);
    if (dk_playing || dk_recording || (unsigned)slot >= DK_NPROJ)
        return -1;
    h.magic = PROJ_HEAD_MAGIC;
    h.format = DK_FORMAT;
    h.nsong = sizeof dk_song;
    h.nev = dk_song.nev;
    h.sum = song_sum();
    if (old && old->sum == h.sum && old->nev == h.nev)
        return 0;                          /* unchanged: spare the flash */
    /* only the sectors that changed: an erase holds the interrupts ~17 ms (the sound, the keys' scan and the lights
     * stop: the device blinked and stuttered when all nine went at once), and a preset or a knob changes one */
    {
        uint32_t len = (uint32_t)sizeof h + sizeof dk_song + (uint32_t)dk_song.nev * sizeof(dk_ev_t);
        for (s = 0; s < len && s < DK_PROJ_SPAN; s += 0x1000u) {
            uint32_t end = s + 0x1000u < len ? s + 0x1000u : len, from = s ? s : (uint32_t)sizeof h;
            if (s && img_same(at, s, end))
                continue;
            if (plat_data_erase(at + s) || img_write(at, from, end))
                return -1;
        }
    }
    if (plat_data_write(at, &h, sizeof h))  /* the head last (its sector was erased: it always changes) */
        return -1;
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
    set.metro[5] = dk_metro.on_note;
    set.midi_notes = dk_midi_notes;
    set.midi_clock = dk_midi_clock;
    plat_store_save(OBJ_SET, &set, sizeof set);
}
static void settings_load(void)
{
    if (plat_store_load(OBJ_SET, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        set.magic = SET_MAGIC;
        set.proj = 0;
        set.midi_notes = 1;
        set.midi_clock = 1;
        set.midi_in = 1;
        set.metro[0] = 1;
        set.metro[1] = 1;
        set.metro[2] = TAPE.metro_sound;
        set.metro[3] = 0;
        set.metro[4] = 55;
        set.metro[5] = 0;
    }
    dk_metro.mode = set.metro[0] % 3;
    dk_metro.count = set.metro[1] ? set.metro[1] : 1;
    dk_metro.sound = set.metro[2] % METRO_N;
    dk_metro.div = set.metro[3] % 3;
    dk_metro.vol = set.metro[4] > 100 ? 100 : set.metro[4];
    dk_metro.on_note = set.metro[5] ? 1 : 0;
    dk_midi_notes = set.midi_notes;
    dk_midi_clock = set.midi_clock;
    if (set.proj >= DK_NPROJ)
        set.proj = 0;
}
void user_save(void)
{
    dk_user.magic = USER_MAGIC;
    plat_store_save(OBJ_USER, &dk_user, sizeof dk_user);
}
static void user_load(void)
{
    int n = plat_store_load(OBJ_USER, &dk_user, sizeof dk_user), old = (int)(sizeof dk_user.magic +
                                                                               DK_UENG_V1 * sizeof dk_user.p[0]);
    if ((n != (int)sizeof dk_user && n != old) || dk_user.magic != USER_MAGIC) {
        uint8_t *b = (uint8_t *)&dk_user;
        uint32_t i;
        for (i = 0; i < sizeof dk_user; i++)
            b[i] = 0;
        dk_user.magic = USER_MAGIC;
    } else if (n == old) {                       /* saved with eight engines: the new ones' places empty */
        uint8_t *b = (uint8_t *)&dk_user;
        uint32_t i;
        for (i = (uint32_t)old; i < sizeof dk_user; i++)
            b[i] = 0;
    }
}
