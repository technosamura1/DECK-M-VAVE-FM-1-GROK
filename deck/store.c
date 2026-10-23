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
static uint32_t song_sum(void)
{
    uint32_t s = fnv(0x811C9DC5u, &dk_song, sizeof dk_song);
    return fnv(s, dk_ev, (uint32_t)dk_song.nev * sizeof(dk_ev_t));
}

static const phead_t *head_of(int slot)
{
    const phead_t *h = (const phead_t *)plat_data(DK_PROJ_AT + (uint32_t)slot * DK_PROJ_SPAN);
    if (!h || h->magic != PROJ_HEAD_MAGIC || h->format != DK_FORMAT || h->nsong != sizeof(dk_song_t) ||
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
    for (i = 0; i < sizeof dk_song; i++)
        dst[i] = src[i];
    n = h->nev * sizeof(dk_ev_t);
    src = plat_data(at + sizeof(phead_t) + sizeof dk_song);
    dst = (uint8_t *)dk_ev;
    for (i = 0; src && i < n; i++)
        dst[i] = src[i];
    if (!src || dk_song.magic != DK_MAGIC || dk_song.nev != h->nev || song_sum() != h->sum)
        goto empty;
    for (i = 0; i < dk_song.nev; i++)      /* nothing waits for a pass that is long gone */
        dk_ev[i].fl &= (uint8_t)~EV_FRESH;
    dk_unlock();
    return 0;
empty:
    dk_song_clear();
    dk_unlock();
    return -1;
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
    for (s = 0; s < DK_PROJ_SPAN; s += 0x1000u)
        if (plat_data_erase(at + s))
            return -1;
    if (plat_data_write(at + sizeof h, &dk_song, sizeof dk_song))
        return -1;
    if (dk_song.nev && plat_data_write(at + sizeof h + sizeof dk_song, dk_ev, (uint32_t)dk_song.nev * sizeof(dk_ev_t)))
        return -1;
    if (plat_data_write(at, &h, sizeof h))  /* the head last */
        return -1;
    return head_of(slot) && head_of(slot)->sum == h.sum ? 0 : -1;
}

void settings_save(void)
{
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
typedef char user_fits_store_[sizeof(dk_user_t) <= 4096u - 256u ? 1 : -1];   /* a store object's payload */
void user_save(void)
{
    dk_user.magic = USER_MAGIC;
    plat_store_save(OBJ_USER, &dk_user, sizeof dk_user);
}
static void user_load(void)
{
    if (plat_store_load(OBJ_USER, &dk_user, sizeof dk_user) != (int)sizeof dk_user || dk_user.magic != USER_MAGIC) {
        uint8_t *b = (uint8_t *)&dk_user;
        uint32_t i;
        for (i = 0; i < sizeof dk_user; i++)
            b[i] = 0;
        dk_user.magic = USER_MAGIC;
    }
}
