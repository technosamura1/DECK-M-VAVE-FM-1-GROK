/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK's panel: the buttons by the map (ui_map.c), the keys (keys.c, and each page's own use of them), the encoders
 * (the page's three fields, coloured; the free knobs), recording (the tape page, the count-in), the road's live
 * recording, undo, saving by itself in the silence, the lights, MIDI in, the export's stream. The pages are drawn in
 * pages.c. Part of the unity build. */

enum { PG_PLAY, PG_SEQ, PG_SOUND, PG_MIX, PG_ROAD, PG_KEYS, PG_PROJECT, PG_REC, PG_ARP, PG_GLOB, PG_ATMO, PG_TRACKS, PG_SOUNDS, NPG };
/* LFO and EDIT trade places (the user's ask): the panel's LFO is the deck's EDIT (KEYS, hold UNDO, the EDIT list) and the
 * panel's EDIT the deck's LFO (MIX, hold + a SOUND knob = MOD). Swapped where the buttons come in and the lights go out. */
static uint32_t btn_swap(uint32_t m)
{
    uint32_t a = m >> B_LFO & 1u, e = m >> B_EDIT & 1u;
    m &= ~((1u << B_LFO) | (1u << B_EDIT));
    return m | e << B_LFO | a << B_EDIT;
}
static const char *const PAGE_NAME[NPG] = {"PLAY", "SEQ", "SOUND", "MIX", "ROAD", "KEYS", "PROJECT", "REC", "ARP", "GLOBAL",
                                          "REC SETUP", "TRACKS", "SOUNDS"};

static struct {
    uint8_t page, prev, row[NPG], garage, dirty, baking, road_rec, root_sel;
    uint32_t btn, keys, down_t[NB];
    uint8_t used[NB], held_done[NB];
    uint16_t combo;                      /* buttons a key or knob was used with while held (their tap on release does not follow) */
    char msg[2][32];
    uint8_t msg_tag;                     /* the popup is a preset's label: msg[0] the block (green), msg[1] the name (white) */
    uint32_t msg_until;
    int8_t touched;
    uint32_t touch_until, frame, last_ms, change_t, edits_seen, rec_end, root_until;
    uint32_t tap_t[6];
    uint8_t tap_n, band, step_key, road_part, road_loops_part, export_on;
    int8_t step_held;                    /* SEQ: the step key held (for VEL/PROB, a p-lock), -1 none */
    uint16_t road_last_tick;
    uint32_t road_loops;
    uint8_t midi_held[6][128];
    uint32_t fit_over;                   /* texts that did not fit their place even abbreviated (the audit) */
    dk_block_t road_clip;
    uint8_t road_clip_ok;
    uint8_t screen_dirty;
    uint32_t draw_t, garden_t, notes_until, page_t, save_t;   /* page_t: the page or its row changed (the hint shows
                                                                * 3 s); save_t: the last save by itself */
    char notes[28];
    uint32_t trk_until;                  /* SEL: the six tracks shown, the one chosen bright */
    uint8_t notes_k;                     /* a PERFORM key held (+1): its name stays up top until it is let go */
    uint8_t auto_fix, auto_prev;         /* (the AUTO take running: ui_auto_on, fields.c) */         /* the tape on AUTO: the first take's bars to set; the bars before it */
    uint16_t auto_max;                   /* the furthest tick of it (a wrap past 16 bars: 16 it is) */
    uint32_t why_t;                      /* a note of the hand: when (why_tick looks 250 ms later whether its track is heard) */
    uint8_t why_k, why_trk;
    uint32_t clr_t;                      /* PROJECT, black 4 (CLEAR ALL): when it was pressed once (0: not asked); again within 3 s clears */
    uint8_t dup_from, dup_to;            /* GARAGE: DUPLICATE PART TO a part with notes: asked once (when: dup_t), the second press overwrites */
    uint32_t dup_t;
    uint8_t pb_back, aud_n, aud_trk;     /* SOUNDS: the page to go back to; the preset heard (note + 1, its track) */
    uint32_t aud_t;                      /* ... till when */
    uint8_t lay, lay_cur;                /* TRACKS: 0 the rows, 1 the EDIT list, 2 the LAYERS list (EDIT: rows <-> EDIT list; back from LAYERS); its cursor */
    uint8_t ed_ask;                      /* the EDIT list's erase item asked once (+1; 0 none): the second press (3 s) does it */
    uint32_t ed_t;
    uint8_t arm_trk;                     /* the tracks armed for recording (bit t; MIX / TRACKS: black 8 arms the selected one) */
    uint8_t armed;                       /* REC ARM: REC pressed, the tape waits for PLAY (or a note on ON NOTE) */
    uint8_t rec_parts[8], rec_np;        /* the looper: the parts a take went into (their blocks come after it) */
    uint8_t endl_on;                     /* ENDLESS: the steps being played in (REC on, REC off) */
    uint32_t endl_t;                     /* the last step's moment (keys within 60 ms: one chord, one step) */
} ui;

#define AUTOSAVE_MS 300000u                /* the autosave: no more often than this (5 min; the first one at once) */
static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 31u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 31u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_tag = 0;
    ui.msg_until = plat_ms() + 700u;     /* a VHS-plate's 0.6 s, and a little */
    if (ann_on()) {
        char t[72];
        uint32_t k = 0;
        for (i = 0; ui.msg[0][i]; i++)
            t[k++] = ui.msg[0][i];
        t[k++] = ' ';
        for (i = 0; ui.msg[1][i]; i++)
            t[k++] = ui.msg[1][i];
        t[k] = 0;
        ann_say(t, 0, ANN_NOW);
    }
}
void ui_say(const char *a, const char *b) { say(a, b); }

static void mark_dirty(void);
/* MIX: a track's mute IN THE PART (the one playing, or the one chosen when stopped). Stopped, or now: at once; playing: it
 * waits for the bar's end (the sequencer applies it; blinking on MIX). Toasts "B: PAD MUTED" */
static void part_mute_toggle(int t, int now)
{
    int p = (dk_playing ? dk_cur_part : dk_song.cur_part) & 7, mute, pend;
    char s[28] = "A: ";
    const char *l = track_label(t);
    uint32_t i = 3;
    if (t < 0 || t >= DK_NTRACK)
        return;
    pend = dk_pm_x && dk_pm_part == p ? (dk_pm_x >> t & 1) : 0;
    mute = dk_song.part[p].mute >> t & 1u;
    s[0] = (char)('A' + p);
    while (*l && i < 26u)
        s[i++] = *l++;
    s[i] = 0;
    if (dk_playing && !now) {
        dk_cmd(C_PMUTE, t, p, 0);
        say(s, (mute ^ pend ^ 1) ? "MUTE AT BAR END" : "ON AT BAR END");
    } else {
        dk_song.part[p].mute ^= (uint8_t)(1u << t);
        say(s, dk_song.part[p].mute >> t & 1u ? "MUTED" : "ON");
    }
    mark_dirty();
}

static void mark_dirty(void)
{
    ui.dirty = 1;
    ui.change_t = plat_ms();
}

static int held(int b) { return (int)(ui.btn >> b & 1u); }

/* SAVE held + white 1: SNAPSHOT of the part (before an experiment), + white 2: RECALL it (an undo step: EDIT holds it back) */
static void snapshot_key(int w)
{
    char s[16] = "PART A";
    int part = dk_playing ? dk_cur_part : dk_song.cur_part, r;
    if (dk_recording || dk_counting) {
        say("SNAPSHOT", "NOT WHILE RECORDING");
        return;
    }
    if (w == 0) {
        s[5] = (char)('A' + (part & 7));
        say("SNAPSHOT", snapshot_take(part) ? s : "PART TOO BIG");
        return;
    }
    r = snapshot_recall();
    if (r == 1) {
        s[5] = (char)('A' + snapshot_part());
        mark_dirty();
        say("RECALL", s);
    } else
        say("RECALL", r ? "NO ROOM" : "NO SNAPSHOT YET");
}

static const char *preset_blk(int slot, int sh);   /* the preset's block, for its label (green) */
/* the keys read between the bands of a frame too: a frame (a whole page, the picture) can take longer than a key may
 * wait for its sound. Each band is whole before the next is drawn, so the state may change between them. */
static void input(void);
static uint8_t in_input, ui_ready;
#define DK_GR_DICE 10                    /* GROOVE: black 11 */
static int pb_user_id(int e, int k);     /* (SOUNDS, for its page) */
static int pb_shown(void);
static int pb_place(void);
static int pb_count(int e, int b);
void ui_mid_poll(void)
{
    if (!in_input && ui_ready)            /* (not before ui_init: the splash) */
        input();
}
#include "pages.c"

/* ------------------------------------------------------------------ actions --- */
static void page_go(int p)
{
    ui.page_t = plat_ms();
    if (ui.page != p)
        ui.lay = 0, ui.ed_ask = 0;
    ui.clr_t = 0;                        /* (CLEAR ALL asked once: forgotten) */
    if (ui.page == p) {                  /* again: its next tab or row */
        ui.row[p] = (uint8_t)((ui.row[p] + 1) % page_rows(p));
        if (p == PG_SOUND) {
            static const char *const TAB[5] = {"FAVORITES", "MORE", "FX", "FX TYPE", "MOD"};
            ui.touched = -1;
            say("SOUND", TAB[ui.row[p] % 5]);
        }
    } else {                             /* from another page: its own first screen (as OP-1's modes; no row left hidden) */
        if (p != PG_REC)
            ui.prev = (uint8_t)p;
        if (p != PG_SEQ || !seq_edit_row())
            ui.row[p] = 0;
        ui.page = (uint8_t)p;
    }
    ui.garage = 0;
}

/* ENDLESS entering: a rest; or the last step's notes a step longer */
static void endl_rest_tie(int tie)
{
    int clip = cur_clip(), steps = dk_song.clip[clip].len / DK_T16, i;
    const dk_clip_t *c = &dk_song.clip[clip];
    if (steps >= 128)
        return;
    undo_push(clip);
    dk_lock();
    if (tie && steps > 0)                     /* the notes still sounding at the last step: on through the next */
        for (i = 0; i < c->n; i++) {
            dk_ev_t *e = &dk_ev[c->at + i];
            if (!(e->fl & EV_MOTION) && e->pos + e->dur + 2 >= steps * DK_T16)
                e->dur = (uint16_t)(e->dur + DK_T16);
        }
    dk_song.clip[clip].len = (uint16_t)((steps + 1) * DK_T16);
    dk_unlock();
    ui.endl_t = 0;
    say("ENDLESS", tie ? "TIE" : "REST");
}
static void track_select(int t)
{
    keys_all_up();
    ui.endl_on = 0;
    t %= DK_NTRACK;
    dk_song.cur_track = (uint8_t)t;
    ui_step_ev = -1;
    if (ui.page != PG_MIX && ui.page != PG_TRACKS) {   /* the track bar instead of a message (MIX and TRACKS show it already:
                                                        * there the bar, the track and its instrument over the mixer, read as choosing one) */
        ui.trk_until = plat_ms() + 1400u;
        ui.msg_until = 0;
    }
    if (dk_recording || dk_counting)     /* the hand follows the track: a new one records too, not only the one REC began on */
        dk_cmd(C_RECTRK, t, 1, 0);
}

/* the KEYS track (TAPE.keys_set): its twelve presets are on three engines */
static int cur_blk(void);
static int keys_group(void)              /* (the track on one of the set's engines: another one, say WINDS, its own) */
{
    int i;
    if (!TAPE.keys_set || (cur()->role != TAPE.keys_role && cur()->role != R_BELL) || cur_blk() > 0)
        return 0;
    for (i = 0; i < DK_NFACT; i++)
        if (TAPE.keys_set[i][0] == cur()->eng)
            return 1;
    return 0;
}
static int keys_group_at(void)           /* where the track is in it, -1 off it */
{
    int i;
    for (i = 0; i < DK_NFACT; i++)
        if (TAPE.keys_set[i][0] == cur()->eng && TAPE.keys_set[i][1] == cur()->preset)
            return i;
    return -1;
}
/* the preset BLOCKS (AMBIENT LOFI, 8-16 BIT, SYNTHWAVE, CINEMATIC): twelve presets an engine in each, HOME + FX / SEL
 * steps along them; a block an engine cannot do (it has no presets there) is skipped */
static uint8_t blk_seen[DK_NTRACK];      /* the block a track's user preset was last in (a user one has none of its own) */
static int blk_of_track(int t)
{
    int b = preset_block(dk_song.trk[t].preset);
    return b >= 0 && b < DK_NBLK ? b : blk_seen[t] % DK_NBLK;
}
static int cur_blk(void) { return blk_of_track(dk_song.cur_track); }
static const char *blk_name(int b) { return TAPE.block_name ? TAPE.block_name[b % DK_NBLK] : ""; }
static int is_factory(int id) { return id < DK_NFACT || id >= DK_PBASE; }
/* the preset's label, the BLOCK first: "SYNTHWAVE - 909 TECHNO", the block green and the name white (drawn by tag_box, pages.c).
 * The block of a preset (a user one: "USER"; sh: the short one for the TRACKS rows) */
static const char *preset_blk(int slot, int sh)
{
    static const char *const SH[DK_NBLK] = {"AMB", "8BIT", "SYN", "CINE"};
    int b = preset_block(slot);
    if (b < 0)
        return "USER";
    return sh ? SH[b % DK_NBLK] : blk_name(b);
}
static void say_preset(int slot)
{
    dk_preset_t tmp;
    const char *blk = preset_blk(slot, 0), *n = preset_of(cur()->eng, slot, &tmp)->name;
    if (!blk[0]) {
        say(n, "");
        return;
    }
    say(blk, n);
    ui.msg_tag = 1;
}
static void preset_apply(int slot)       /* (the take being recorded has been frozen with the old sound) */
{
    track_preset(dk_song.cur_track, slot);
    if (preset_block(slot) >= 0)
        blk_seen[dk_song.cur_track] = (uint8_t)preset_block(slot);
    mark_dirty();
    if (ui.page != PG_SOUNDS)            /* (SOUNDS shows it itself) */
        say_preset(slot);
}
static void preset_select_id(int slot)
{
    track_sound_to(dk_song.cur_track);
    preset_apply(slot);
}
static void preset_select(int slot)      /* a white key 0..15: 0..11 the block's place, 12..15 the user's */
{
    if (slot < DK_NFACT && keys_group()) {
        keys_all_up();
        track_sound_to(dk_song.cur_track);
        cur()->eng = TAPE.keys_set[slot][0];
        preset_apply(TAPE.keys_set[slot][1]);
        return;
    } else if (slot < DK_NFACT) {
        int b = cur_blk();
        if (slot >= preset_count(cur()->eng, b)) {
            say(blk_name(b), "NO PRESET HERE");
            return;
        }
        slot = preset_id(b, slot);
    }
    preset_select_id(slot);
}
static void block_step(int d)            /* HOME + FX / SEL: the next / previous block, the same place in it */
{
    int e = cur()->eng, b = cur_blk(), p = cur()->preset, i, n, nb = b;
    i = preset_block(p) >= 0 ? (p < DK_NFACT ? p : (p - DK_PBASE) % DK_NFACT) : 0;
    for (n = 0; n < DK_NBLK; n++) {
        nb = (nb + d + DK_NBLK) % DK_NBLK;
        if (preset_count(e, nb) > 0)
            break;
    }
    if (nb == b || preset_count(e, nb) <= 0) {
        say(cur_eng()->name, "ONE BLOCK ONLY");
        return;
    }
    keys_all_up();
    if (i >= preset_count(e, nb))
        i = preset_count(e, nb) - 1;
    preset_select_id(preset_id(nb, i));
}

/* ARP -> SEQ: the arpeggio of the notes held (or latched, or the song's chord) as notes in the clip, the length of
 * the clip, the way the ARP would play them; the track then STEP and the ARP off (one undo step) */
static void arp_bake(void)
{
    static const float GATE[ARPG_N] = {0.55f, 0.25f, 0.85f, 1.02f};
    dk_track_t *k = cur();
    int t = dk_song.cur_track, n = dk_held_n[t], srt[8], i, clip = cur_clip(), len = clip_len(clip), s, hits = 0;
    int div = DK_ARP_DIV[k->arp[1] % DK_ARP_NDIV], octs = k->arp[2] < 1 ? 1 : k->arp[2] > 3 ? 3 : k->arp[2], total;
    float gate = GATE[k->arp_opt & 3];
    uint32_t seed = 0x2545F491u;
    if (!n && ((k->arp_opt >> 2) & 1) && dk_harm_n) {   /* PLAYS CHORD: the song's chord */
        int prev = -1;
        n = dk_harm_n > 4 ? 4 : dk_harm_n;
        for (i = 0; i < n; i++) {
            int v = 60 + 12 * k->oct + dk_harm_pcs[i] % 12;
            while (v <= prev)
                v += 12;
            srt[i] = prev = v;
        }
    } else {
        if (n > 8)
            n = 8;
        for (i = 0; i < n; i++)
            srt[i] = dk_held[t][i];
    }
    if (!n) {
        say("ARP", "HOLD OR LATCH NOTES");
        return;
    }
    {
        int order[8];
        for (i = 0; i < n; i++)
            order[i] = srt[i];
        for (i = 1; i < n; i++) {            /* sorted (ORDER keeps the order played) */
            int v = srt[i], j = i - 1;
            while (j >= 0 && srt[j] > v) {
                srt[j + 1] = srt[j];
                j--;
            }
            srt[j + 1] = v;
        }
        undo_push(clip);
        keys_all_up();
        dk_lock();
        clip_clear(clip);
        total = n * octs;
        for (s = 0; s * div < len; s++) {
            int hit = DK_ARP_RHY[k->arp_rhy % ARPR_N][s & 15], idx, note;
            dk_ev_t ev;
            seed = seed * 1664525u + 1013904223u;
            if (k->arp_rhy == ARPR_SKIP && (s & 3) && (seed >> 28) == 0)
                hit = 0;
            if (!hit)
                continue;
            switch (k->arp[0] % 6) {
            case 1: idx = total - 1 - hits % total; break;
            case 2: {
                int per = total > 1 ? 2 * total - 2 : 1, x = hits % per;
                idx = x < total ? x : per - x;
                break;
            }
            case 3: idx = (int)((seed >> 8) % (uint32_t)total); break;
            default: idx = hits % total; break;
            }
            hits++;
            ev.pos = (uint16_t)(s * div);
            ev.dur = (uint16_t)((int)((float)div * gate) > 1 ? (int)((float)div * gate) : 1);
            ev.vel = (uint8_t)(hit == 2 ? 92 : 74);
            ev.fl = 0;
            ev.layer = 0;
            if (k->arp[0] % 6 == 5) {        /* CHORD: the whole stack each hit */
                for (i = 0; i < n; i++) {
                    ev.note = (uint8_t)srt[i];
                    dk_song_insert(clip, &ev);
                }
                continue;
            }
            note = (k->arp[0] % 6 == 4 ? order[idx % n] : srt[idx % n]) + 12 * (idx / n);
            ev.note = (uint8_t)(note > 127 ? 127 : note);
            dk_song_insert(clip, &ev);
        }
        dk_unlock();
    }
    k->arp_opt &= (uint8_t)~ARPO_ON;
    dk_cmd(C_ARPCLR, t, 0, 0);
    field_set(F_SEQTYPE, 0);                 /* STEP: the notes on its grid, to edit */
    mark_dirty();
    page_go(PG_SEQ);
    say("ARP -> STEP", "COPIED");
}

/* PRESET: along the engine's presets, the blocks one after the other, then the user's saved ones */
static int pseq_len(int e)
{
    int b, n = 0;
    for (b = 0; b < DK_NBLK; b++)
        n += preset_count(e, b);
    return n;
}
static int pseq_id(int e, int k)         /* the k'th of the presets */
{
    int b, c;
    for (b = 0; b < DK_NBLK; b++) {
        c = preset_count(e, b);
        if (k < c)
            return preset_id(b, k);
        k -= c;
    }
    return DK_NFACT + k;                 /* past them: a user slot */
}
static void pb_audition(void);
/* PRESET: along the presets of the block the track is in (twelve at most, round); a user preset: along the user's saved
 * ones. The other blocks: on the SOUNDS page (HOME + PRESET), or HOME + FX / SEL */
static void preset_step(int d)
{
    int e = cur()->eng, s = cur()->preset, b, n, i, k;
    if (keys_group()) {                  /* KEYS: along its twelve */
        int g = keys_group_at();
        preset_select(((g < 0 ? 0 : g) + d + DK_NFACT) % DK_NFACT);
        pb_audition();
        return;
    }
    b = preset_block(s);
    if (b < 0) {                         /* a user one: the next saved */
        k = (s - DK_NFACT) & 3;
        for (i = 0; i < 4; i++) {
            k = (k + d + 4) % 4;
            if (dk_user.p[e % DK_UENG][k].used)
                break;
        }
        preset_select_id(DK_NFACT + k);
        pb_audition();
        return;
    }
    n = preset_count(e, b);
    if (n <= 0)
        return;
    k = s < DK_NFACT ? s : (s - DK_PBASE) % DK_NFACT;
    preset_select_id(preset_id(b, (k + d + n * 4) % n));
    pb_audition();
}

/* ------------------------------------------------------------------ SOUNDS --- */
/* SOUNDS (HOME + PRESET, OP-1's): the track's instrument, its preset block and the twelve presets of it on one screen.
 * K1 / OCT- OCT+ the instrument (a melodic track: the melodic ones; drums keep theirs), K2 / black 1-4 the block (5: the
 * user's saved ones), K3 / PRESET / white 1-12 the preset. Each choice sounds a short note (stopped, no key down). HOME: back. */
static int pb_user_n(int e)
{
    int i, n = 0;
    for (i = 0; i < 4; i++)
        n += dk_user.p[e % DK_UENG][i].used ? 1 : 0;
    return n;
}
static int pb_user_id(int e, int k)      /* the k'th saved user preset */
{
    int i;
    for (i = 0; i < 4; i++)
        if (dk_user.p[e % DK_UENG][i].used && k-- == 0)
            return DK_NFACT + i;
    return DK_NFACT;
}
static int pb_blocks(int e, uint8_t *bl)  /* the blocks with presets (DK_NBLK: the user's), in order */
{
    int b, n = 0;
    for (b = 0; b < DK_NBLK; b++)
        if (preset_count(e, b) > 0)
            bl[n++] = (uint8_t)b;
    if (pb_user_n(e))
        bl[n++] = DK_NBLK;
    return n;
}
static int pb_shown(void)                 /* the block the track is in (DK_NBLK: a user preset) */
{
    return preset_block(cur()->preset) < 0 ? DK_NBLK : cur_blk();
}
static int pb_engs(uint8_t *el)           /* the instruments the track may take */
{
    int e, n = 0;
    if (cur_eng()->flags & (EF_DRUM | EF_CONT)) {
        el[0] = cur()->eng;
        return 1;
    }
    for (e = 0; e < TAPE.neng; e++)
        if (!(TAPE.eng[e]->flags & (EF_DRUM | EF_CONT)))
            el[n++] = (uint8_t)e;
    return n;
}
static int pb_place(void)                 /* the preset's place in its block */
{
    int s = cur()->preset, e = cur()->eng, i;
    if (preset_block(s) < 0) {
        for (i = 0; i < pb_user_n(e); i++)
            if (pb_user_id(e, i) == s)
                return i;
        return 0;
    }
    return s < DK_NFACT ? s : (s - DK_PBASE) % DK_NFACT;
}
static int pb_count(int e, int b) { return b >= DK_NBLK ? pb_user_n(e) : preset_count(e, b); }
static void pb_pick(int b, int i)         /* block b's preset i, heard */
{
    int e = cur()->eng, n = pb_count(e, b);
    if (n <= 0) {
        say(b >= DK_NBLK ? "USER" : blk_name(b), "NO PRESETS");
        return;
    }
    if (i >= n)
        i = n - 1;
    if (keys_group())
        keys_all_up();
    preset_select_id(b >= DK_NBLK ? pb_user_id(e, i) : preset_id(b, i));
    pb_audition();
}
static void pb_engine(int e)              /* the instrument: the same block if it has one, else its first; the first preset */
{
    uint8_t bl[DK_NBLK + 1];
    int b = pb_shown(), n, i, ok = 0;
    if (e == cur()->eng)
        return;
    track_sound_to(dk_song.cur_track);
    keys_all_up();
    cur()->eng = (uint8_t)e;
    n = pb_blocks(e, bl);
    for (i = 0; i < n; i++)
        ok |= bl[i] == b;
    if (!ok)
        b = n ? bl[0] : 0;
    track_preset(dk_song.cur_track, b >= DK_NBLK ? pb_user_id(e, 0) : preset_id(b, 0));
    if (b < DK_NBLK)
        blk_seen[dk_song.cur_track] = (uint8_t)b;
    mark_dirty();
    pb_audition();
}
static void pb_engine_step(int d)
{
    uint8_t el[16];
    int n = pb_engs(el), i, k = 0;
    if (n <= 1) {
        say(cur_eng()->name, "ITS OWN");
        return;
    }
    for (i = 0; i < n; i++)
        if (el[i] == cur()->eng)
            k = i;
    pb_engine(el[(k + d + n) % n]);
}
static int pb_hi(int f)
{
    uint8_t l[16];
    int n = f == F_PBENG ? pb_engs(l) : f == F_PBBLK ? pb_blocks(cur()->eng, l) : pb_count(cur()->eng, pb_shown());
    return n > 0 ? n - 1 : 0;
}
static int pb_get(int f)
{
    uint8_t l[16];
    int n, i;
    if (f == F_PBPRE)
        return pb_place();
    n = f == F_PBENG ? pb_engs(l) : pb_blocks(cur()->eng, l);
    for (i = 0; i < n; i++)
        if (l[i] == (f == F_PBENG ? cur()->eng : pb_shown()))
            return i;
    return 0;
}
static void pb_set(int f, int v)
{
    uint8_t l[16];
    if (f == F_PBPRE)
        pb_pick(pb_shown(), v);
    else if (f == F_PBBLK) {
        if (pb_blocks(cur()->eng, l) > v)
            pb_pick(l[v], pb_place());
    } else if (pb_engs(l) > v)
        pb_engine(l[v]);
}
static void pb_text(int f, char *b)
{
    dk_preset_t tmp;
    int sh = pb_shown();
    if (f == F_PBENG)
        cpy(b, cur_eng()->name);
    else if (f == F_PBBLK)
        cpy(b, sh >= DK_NBLK ? "USER" : blk_name(sh));
    else
        cpy(b, preset_of(cur()->eng, cur()->preset, &tmp)->name);
}
/* the choice heard: a short note on the track (stopped, nothing held; the bass and the drums low) */
static void pb_audition(void)
{
    int t = dk_song.cur_track, n;
    if (dk_playing || dk_recording || dk_counting || ui.keys)
        return;
    if (ui.aud_n)
        dk_note(ui.aud_trk, ui.aud_n - 1, 0, 0);
    n = (cur_eng()->flags & EF_DRUM) || cur_eng()->role == R_BASS || cur()->role == R_BASS ? 36 : 60;
    dk_note(t, n, 96, 0);
    ui.aud_n = (uint8_t)(n + 1);
    ui.aud_trk = (uint8_t)t;
    ui.aud_t = plat_ms() + 450u;
}
static void pb_audition_tick(uint32_t now)
{
    if (ui.aud_n && (int32_t)(now - ui.aud_t) >= 0) {
        dk_note(ui.aud_trk, ui.aud_n - 1, 0, 0);
        ui.aud_n = 0;
    }
}
static void pb_open(void)
{
    if (ui.page == PG_SOUNDS)
        return;
    ui.pb_back = ui.page;
    page_go(PG_SOUNDS);
}
static int pb_key(int w, int b)           /* SOUNDS: white 1-12 a preset of the block, black 1-5 the block */
{
    uint8_t l[DK_NBLK + 1];
    if (w >= 0) {
        if (w < DK_NFACT)
            pb_pick(pb_shown(), w);
        return 1;
    }
    if (b >= 0 && b <= DK_NBLK) {
        int n = pb_blocks(cur()->eng, l), i;
        for (i = 0; i < n; i++)
            if (l[i] == b) {
                pb_pick(b, pb_place());
                return 1;
            }
        say(b >= DK_NBLK ? "USER" : blk_name(b), b >= DK_NBLK ? "SAVE ONE: SAVE+W13" : "NO PRESETS");
        return 1;
    }
    return 1;
}

static void preset_save(int slot)        /* SAVE held + white 13..16 */
{
    dk_upreset_t *u = &dk_user.p[cur()->eng % DK_UENG][slot & 3];
    const dk_track_t *k = cur();
    dk_preset_t tmp;
    const dk_preset_t *from = preset_of(k->eng, k->preset, &tmp);
    int i;
    char s[16] = "SAVED TO 13";
    for (i = 0; i < 11 && from->name[i]; i++)
        u->name[i] = from->name[i];
    if (is_factory(k->preset) && i < 9) {   /* a copy of a factory one: its name and a star */
        u->name[i++] = '*';
    }
    u->name[i] = 0;
    u->used = 1;
    for (i = 0; i < DK_NP; i++)
        u->p[i] = k->p[i];
    for (i = 0; i < 3; i++) {
        u->env[i] = k->env[i];
        u->mt[i] = k->mt[i];
        u->ma[i] = k->ma[i];
    }
    u->fx = k->fx;
    u->fxamt = (uint8_t)k->fxamt;
    u->fxa = (uint8_t)k->fxa;
    u->fxb = (uint8_t)k->fxb;
    u->rev = k->rev;
    u->echo = k->echo;
    u->trim = k->trim;
    user_save();
    blk_seen[dk_song.cur_track] = (uint8_t)cur_blk();
    cur()->preset = (uint8_t)(DK_NFACT + (slot & 3));
    s[10] = (char)('3' + (slot & 3));
    say(s, u->name);
}

/* the tracks REC writes are heard: a mute in the part (or another track's solo) left on from before made them silent */
static void rec_audible(int t, int t2)
{
    int p = (dk_playing ? dk_cur_part : dk_song.cur_part) & 7, i, solo = 0;
    uint8_t m = (uint8_t)((1u << t) | (ui.arm_trk & 0x3F) | (t2 < DK_NTRACK ? 1u << t2 : 0u)), was;
    char s[28] = "A: ";
    const char *l = track_label(t);
    uint32_t j = 3;
    dk_lsolo_clip = 0xFF;                       /* (a layer soloed on LAYERS: the new take is heard too) */
    for (i = 0; i < DK_NTRACK; i++)
        solo |= dk_song.trk[i].solo;
    was = (uint8_t)(dk_song.part[p].mute & m);
    if (!was && !(solo && !dk_song.trk[t].solo))
        return;
    dk_lock();
    dk_song.part[p].mute &= (uint8_t)~m;
    if (dk_pm_x && dk_pm_part == p)
        dk_pm_x &= (uint8_t)~m;
    if (solo && !dk_song.trk[t].solo)
        for (i = 0; i < DK_NTRACK; i++)
            dk_song.trk[i].solo = 0;
    dk_unlock();
    s[0] = (char)('A' + p);
    while (*l && j < 26u)
        s[j++] = *l++;
    s[j] = 0;
    say(s, was ? "UNMUTED" : "SOLO OFF");
    mark_dirty();
}

static void rec_start(void)
{
    int t = dk_song.cur_track, clip = cur_clip(), t2 = dk_song.split && dk_song.harm_track != t ? dk_song.harm_track : 0xFF;
    rec_reset();
    ui_auto_on = 0;
    if (!dk_playing && ui_rec_auto && part_empty(dk_song.cur_part)) {   /* AUTO: open, up to 16 bars, REC ends it */
        ui.auto_prev = dk_song.part[dk_song.cur_part & 7].bars;
        part_bars_set(dk_song.cur_part, 16);
        ui_auto_on = 1;
        ui.auto_max = 0;
    }
    rec_audible(t, t2);
    dk_cmd(C_REC, 1, 0, t | t2 << 16 | (ui.arm_trk & 0x3F) << 24);   /* (the selected track is always in it) */
    ui.rec_end = 0;
    if (ui.page != PG_REC && ui.page != PG_TRACKS) {
        ui.prev = ui.page;
        ui.page = PG_REC;
    }
    ui.armed = 0;
    {
        char s[20] = "TAKE ";   /* "TAKE 31 L7/7" is 12 characters and the end */
        int n = dk_song.clip[clip].layers + 1, i, used = 0;
        for (i = 0; i < DK_NTRACK; i++)       /* (several armed: one number for them all, the highest + 1) */
            if ((ui.arm_trk >> i & 1) && dk_song.clip[dk_song.cur_part * DK_NTRACK + i].layers + 1 > n)
                n = dk_song.clip[dk_song.cur_part * DK_NTRACK + i].layers + 1;
        for (i = 0; i < DK_NSND; i++)
            used += dk_song.snd[t][i].used ? 1 : 0;
        if (n > 31)
            n = 31;
        itoa_b(n, s + 5);
        if (dk_playing) {
            if (used) {
                int l = 0;
                while (s[l])
                    l++;
                s[l++] = ' ';
                s[l++] = 'L';
                itoa_b(used, s + l);
                l = 0;
                while (s[l])
                    l++;
                s[l++] = '/';
                s[l++] = '7';
                s[l] = 0;
            }
            say("REC", s);
        } else
            say("REC", dk_metro.on_note ? "PLAY A NOTE" : "COUNT IN");
    }
}

static void auto_end(void)               /* AUTO's take ends (REC or PLAY): the loop at the nearest bar line */
{
    if (ui_auto_on && dk_playing && !dk_counting) {
        int b = (dk_part_tick + DK_TPB / 4) / DK_TPB;
        ui.auto_fix = (uint8_t)(b < 1 ? 1 : b > 16 ? 16 : b);
    } else if (ui_auto_on)                 /* stopped in the count-in: the part as it was */
        part_bars_set(dk_song.cur_part, ui.auto_prev);
    ui_auto_on = 0;
}
void ui_arm_reset(void) { ui.armed = 0; }
static void rec_stop_ui(void)
{
    int was_playing = dk_playing;
    ui.armed = 0;
    dk_cmd(C_REC, 0, 0, 0);               /* (the tape goes on playing: this is the punch-out) */
    auto_end();
    say("REC", was_playing && dk_recording ? "PUNCH OUT" : "OFF");
}
/* TRACKS, SEQ + white 1-6: arm / disarm a track for recording (live too: the render's set follows) */
static void rec_arm_toggle(int t)
{
    char s[16] = "ARM T";
    ui.arm_trk = (uint8_t)((ui.arm_trk ^ 1u << t) & 0x3F);
    if (dk_recording)
        dk_cmd(C_RECMASK, ui.arm_trk, 0, 0);
    s[5] = (char)('1' + t);
    s[6] = 0;
    say(ui.arm_trk >> t & 1 ? s : "DISARM", track_label(t));
    mark_dirty();
}
/* AUTO's end: the take's notes past the new end wrapped round to its start, the part that long */
static void auto_apply(void)
{
    int p = dk_song.cur_part & 7, len = ui.auto_fix * DK_TPB, t, i;
    char s[16];
    rec_flush();
    dk_lock();
    for (t = 0; t < DK_NTRACK; t++) {
        int c = p * DK_NTRACK + t;
        dk_clip_t *cl = &dk_song.clip[c];
        for (i = cl->n - 1; i >= 0; i--) {
            dk_ev_t *e = &dk_ev[cl->at + i];
            if (e->pos < len)
                continue;
            /* past the loop's end: folded to its start, unless it is a drummer's (it plays the bar again anyway) or the
             * same note is there already (a doubled hit) */
            if (e->pos < 2 * len && dk_song.trk[t].seq != SQ_GROOVE &&
                ((e->fl & EV_MOTION) || clip_find(c, e->pos - len, e->note, 0) < 0))
                e->pos = (uint16_t)(e->pos - len);
            else
                ev_delete(c, i);
        }
        clip_sort(c);
    }
    dk_unlock();
    part_bars_set(p, ui.auto_fix);
    bars_text(ui.auto_fix, s);
    say("LOOP", s);
    ui.auto_fix = 0;
    mark_dirty();
}

static int bar_ms(void) { return (int)(240000u / (dk_song.tempo ? dk_song.tempo : 120)); }

static void road_insert(int part, int dup)
{
    int i, at;
    if (dk_song.nroad >= DK_NROAD)
        return;
    undo_push_road();
    at = dk_song.nroad ? ui_road_cur + 1 : 0;
    for (i = dk_song.nroad; i > at; i--)
        dk_song.road[i] = dk_song.road[i - 1];
    if (dup && dk_song.nroad)
        dk_song.road[at] = dk_song.road[ui_road_cur];
    else {
        dk_song.road[at].part = (uint8_t)part;
        dk_song.road[at].reps = 1;
        dk_song.road[at].tricks = 0;
        dk_song.road[at].drop = 0;
    }
    dk_song.nroad++;
    ui_road_cur = (uint8_t)at;
    mark_dirty();
}
static int road_has(int p)              /* a block on the road plays part p */
{
    int i;
    for (i = 0; i < dk_song.nroad; i++)
        if (dk_song.road[i].part == p)
            return 1;
    return 0;
}
static void road_append(int p)          /* a block at the road's end (the order you make them in) */
{
    if (dk_song.nroad >= DK_NROAD)
        return;
    undo_push_road();
    dk_song.road[dk_song.nroad].part = (uint8_t)p;
    dk_song.road[dk_song.nroad].reps = 1;
    dk_song.road[dk_song.nroad].tricks = 0;
    dk_song.road[dk_song.nroad].drop = 0;
    ui_road_cur = dk_song.nroad;
    dk_song.nroad++;
    mark_dirty();
}
static void road_move(int d)             /* HOME + K1 on ROAD: the block a place left or right */
{
    int to = ui_road_cur + d;
    dk_block_t b;
    if (!dk_song.nroad || to < 0 || to >= dk_song.nroad)
        return;
    undo_push_road();
    b = dk_song.road[to];
    dk_song.road[to] = dk_song.road[ui_road_cur];
    dk_song.road[ui_road_cur] = b;
    ui_road_cur = (uint8_t)to;
    mark_dirty();
}
static void road_delete(int keep)
{
    int i;
    if (!dk_song.nroad)
        return;
    undo_push_road();
    if (keep) {
        ui.road_clip = dk_song.road[ui_road_cur];
        ui.road_clip_ok = 1;
    }
    for (i = ui_road_cur; i < dk_song.nroad - 1; i++)
        dk_song.road[i] = dk_song.road[i + 1];
    dk_song.nroad--;
    if (ui_road_cur >= dk_song.nroad && ui_road_cur)
        ui_road_cur--;
    mark_dirty();
}

/* GARAGE: white 1-8 = DUPLICATE the part on the cursor TO that part. An empty target takes it at once; a target with notes asks
 * once ("AGAIN = OVERWRITE", red), and only the second press (same pair, within 3 s) replaces it. One undo step. */
static void garage_dup(int to)
{
    int from = ui_gpart % DK_NPART, wipe, r;
    char s[8] = "A->B";
    uint32_t now = plat_ms();
    s[0] = (char)('A' + from);
    s[3] = (char)('A' + to);
    if (dk_recording || dk_counting) {
        say("DUPLICATE", "STOP REC");
        return;
    }
    if (from == to) {
        say("DUPLICATE", "PICK ANOTHER");
        return;
    }
    if (!part_has_events(from)) {
        say(s, "EMPTY");
        return;
    }
    wipe = part_has_events(to);
    if (wipe && !(ui.dup_t && now - ui.dup_t < 3000u && ui.dup_from == from && ui.dup_to == to)) {
        ui.dup_t = now;
        ui.dup_from = (uint8_t)from;
        ui.dup_to = (uint8_t)to;
        say(s, "AGAIN = OVERWRITE");
        return;
    }
    ui.dup_t = 0;
    r = part_copy_x(from, to, wipe);
    if (r > 0) {
        mark_dirty();
        say(s, wipe ? "OVERWRITTEN" : "COPIED");
    } else
        say("NO ROOM", s);
}

/* CLIP QUANTIZE (every note toward the 1/16 by the track's QUANT %), COPY BAR / PASTE BAR. The bar: the one under the playhead while
 * playing, else the first; stopped, PASTE goes to the bar after the copied one, and the next press to the bar after that. */
static uint8_t bar_src, bar_next;
static char *put_n(char *p, int n)       /* n (0..99) as text, the end of it */
{
    if (n >= 10)
        *p++ = (char)('0' + n / 10 % 10);
    *p++ = (char)('0' + n % 10);
    return p;
}
static void clip_edit_key(int k)
{
    int c = cur_clip(), bars = clip_bars(c), bar = dk_playing ? (int)(dk_part_tick / DK_TPB) % (bars ? bars : 1) : 0, r;
    int q = dk_song.trk[dk_song.cur_track].qnt;
    char s[16];
    char *p = s;
    if (k != 2 && !dk_song.clip[c].n) {
        say("CLIP", "IS EMPTY");
        return;
    }
    if (dk_recording || dk_counting) {
        say("CLIP", "STOP REC");
        return;
    }
    if (k == 0) {
        if (q == 0) {
            say("QUANTIZE", "TRACK QUANT IS 0");
            return;
        }
        r = clip_quantize(c, q);
        *p++ = 'Q';
        *p++ = ' ';
        p = put_n(p, q > 99 ? 99 : q);
        *p++ = '%';
        *p = 0;
        mark_dirty();
        say(s, r ? "ON THE GRID" : "NOTHING TO MOVE");
        return;
    }
    if (k == 1) {
        r = bar_copy(c, bar);
        if (r < 0) {
            say("BAR", "TOO MANY NOTES");
            return;
        }
        bar_src = (uint8_t)bar;
        bar_next = (uint8_t)(bar + 1);
        *p++ = 'B';
        *p++ = 'A';
        *p++ = 'R';
        *p++ = ' ';
        p = put_n(p, bar + 1);
        *p = 0;
        say(s, "COPIED");
        return;
    }
    if (!dk_playing) {                   /* stopped: after the copied bar, then the next one */
        if (bars < 2) {
            say("BAR", "CLIP HAS ONE BAR");
            return;
        }
        bar = bar_next % bars;
        if (bar == bar_src)
            bar = (bar + 1) % bars;
    }
    r = bar_paste(c, bar);
    if (r < 0) {
        say("BAR", "NO ROOM");
        return;
    }
    if (!dk_playing)
        bar_next = (uint8_t)(bar + 1);
    mark_dirty();
    p = put_n(p, bar_src + 1);
    *p++ = '-';
    *p++ = '>';
    p = put_n(p, bar + 1);
    *p = 0;
    say(s, "BAR PASTED");
}

static void action(int a, int b)
{
    switch (a) {
    case A_PAGE_SOUND: page_go(PG_SOUND); break;
    case A_BLOCK_NEXT: block_step(1); break;
    case A_BLOCK_PREV: block_step(-1); break;
    case A_PAGE_SEQ:
        if (ui.page == PG_ARP && dk_arp_on(cur())) {   /* on the ARP's page: what it plays, written into SEQ's grid */
            arp_bake();
            break;
        }
        page_go(PG_SEQ);
        break;
    case A_PAGE_ROAD:
        if (dk_recording && dk_playing && !dk_song.play_mode) {   /* recording: on to the next empty part (a copy of this one) */
            int i, p = -1;
            for (i = 1; i < DK_NPART && p < 0; i++)
                if (!part_has_events((dk_cur_part + i) & 7))
                    p = (dk_cur_part + i) & 7;
            if (p < 0)
                say("PARTS", "ALL HAVE NOTES");
            else
                part_branch(p);
            break;
        }
        page_go(PG_ROAD);
        break;
    case A_PAGE_MIX:                     /* (the panel's EDIT): MIX; on TRACKS the EDIT list, as its name says */
        if (ui.page != PG_TRACKS) {
            page_go(PG_MIX);
            break;
        }
        /* fall through */
    case A_PAGE_KEYS:
        if (ui.page == PG_TRACKS) {          /* TRACKS: EDIT opens the EDIT list of the selected track's clip (again: the rows; from LAYERS: back) */
            ui.lay = (uint8_t)(ui.lay == 0 ? 1 : ui.lay == 2 ? 1 : 0);
            ui.page_t = plat_ms();
            ui.lay_cur = 0;
            ui.ed_ask = 0;
        } else
            page_go(PG_KEYS);
        break;
    case A_PAGE_PROJECT:
        if (ui.page == PG_PROJECT) {         /* SAVE again on its page: saved now */
            if (dk_playing || dk_recording || dk_counting)
                say("SAVE", "STOP FIRST");
            else {
                if (settings_due)
                    settings_save();
                tape_ui_keep();
                if (project_save(set.proj) == 0) {
                    ui.dirty = 0;
                    say("SAVED", "");
                } else
                    say("SAVE", "FAILED");
            }
            break;
        }
        page_go(PG_PROJECT);
        ui_slot_sel = -1;
        break;
    case A_PAGE_GLOBAL: page_go(PG_GLOB); break;
    case A_PAGE_ATMO:                    /* SEL: REC SETUP (metronome, REC MODE, NEW PART copy, START...); again: its next row */
        page_go(PG_ATMO);
        break;
    case A_PAGE_PLAY:                    /* HOME: PLAY; on PLAY: TRACKS; on TRACKS: back (to REC while it records) */
        if (ui.armed && !dk_recording && !dk_counting) {   /* armed and not recording: HOME lets it go */
            ui.armed = 0;
            say("REC", "OFF");
        }
        if (ui.page == PG_SOUNDS) {          /* SOUNDS: back where it was opened */
            page_go(ui.pb_back == PG_SOUNDS ? PG_PLAY : ui.pb_back);
            break;
        }
        if (ui.page == PG_PLAY || (ui.page == PG_REC && (dk_recording || dk_counting)))
            page_go(PG_TRACKS);
        else
            page_go(ui.page == PG_TRACKS && (dk_recording || dk_counting) ? PG_REC : PG_PLAY);
        break;
    case A_PLAY:
        if (dk_playing || dk_counting) {
            auto_end();
            dk_cmd(C_PLAY, 0, 0, 0);
            ui.change_t = plat_ms() - 3000u;   /* saved soon after the stop */
            ui.road_rec = 0;
        } else if (ui.armed)                 /* armed: PLAY starts the tape (the count-in, then the take) */
            rec_start();
        else                                 /* on ROAD: from the block chosen there */
            dk_cmd(C_PLAY, 1, ui.page == PG_ROAD && !ui.garage ? ui_road_cur : 0, 0);
        break;
    case A_PLAYMODE:
        dk_song.play_mode ^= 1u;
        mark_dirty();
        say("PLAY", dk_song.play_mode ? "SONG" : "PART");
        break;
    case A_TAP_TEMPO: {
        uint32_t now = plat_ms();
        if (ui.tap_n && now - ui.tap_t[(ui.tap_n - 1) % 6] > 2000u)
            ui.tap_n = 0;
        ui.tap_t[ui.tap_n % 6] = now;
        ui.tap_n++;
        if (ui.tap_n >= 3) {
            int n = ui.tap_n > 6 ? 6 : ui.tap_n, i;
            uint32_t sum = 0;
            char s[8];
            for (i = 1; i < n; i++)
                sum += ui.tap_t[(ui.tap_n - i) % 6] - ui.tap_t[(ui.tap_n - i - 1) % 6];
            if (sum) {
                field_set(F_TEMPO, (int)((60000u * (uint32_t)(n - 1) + sum / 2) / sum));
                itoa_b(dk_song.tempo, s);
                say("TEMPO", s);
            }
        } else
            say("TAP", "");
        break;
    }
    case A_REC:
        if (cur()->seq == SQ_ENDLESS && !dk_recording && !dk_counting && !ui.armed) {   /* ENDLESS: the steps played in */
            int clip = cur_clip();
            ui.endl_on ^= 1u;
            if (ui.endl_on) {
                undo_push(clip);
                dk_lock();
                clip_clear(clip);
                dk_song.clip[clip].len = 0;
                dk_unlock();
                dk_cmd(C_ENDLTR, dk_song.cur_track, 0, 0);
                ui.endl_t = 0;
                if (ui.page != PG_SEQ)
                    page_go(PG_SEQ);
                say("ENDLESS", "PLAY THE STEPS");
            } else {
                char st[16];
                cpy(itoa_b(dk_song.clip[clip].len / DK_T16, st), " STEPS");
                say("ENDLESS", st);
                mark_dirty();
            }
            break;
        }
        if (dk_recording || dk_counting)       /* (on ROAD too: a take into the part; its block on the road comes
                                                 * by itself when the take ends, see rec_parts) */
            rec_stop_ui();
        else if (ui.armed)                   /* armed: REC again records (PLAY too) */
            rec_start();
        else if (dk_rec_arm) {               /* two steps: REC arms on the tape (part, bars, quant to set), REC again records */
            ui.armed = 1;
            if (ui.page != PG_REC) {
                ui.prev = ui.page;
                ui.page = PG_REC;
            }
            say("REC ARMED", "REC AGAIN = START");
        } else
            rec_start();
        break;
    case A_METRO:
        dk_metro.mode = (uint8_t)(dk_metro.mode ? 0 : 1);
        settings_save_later();
        say("METRO", dk_metro.mode ? "ON" : "OFF");
        break;
    case A_NEXT_TRACK: track_select((dk_song.cur_track + 1) % DK_NTRACK); break;
    case A_MUTE:                          /* (the track's mute in this part) */
        part_mute_toggle(dk_song.cur_track, 0);
        break;
    case A_ARP: {                         /* the ARP's page (and the ARP on); again there: its next row */
        dk_track_t *k = cur();
        if (cur_eng()->flags & EF_DRUM) {
            say("ARP", "NOT ON DRUMS");
            break;
        }
        if (ui.page != PG_ARP && !dk_arp_on(k))   /* the button only opens the page (ARP is K4 there); off: the first row */
            ui.row[PG_ARP] = 0;
        page_go(PG_ARP);
        break;
    }
    case A_ARP_OFF: {
        dk_track_t *k = cur();
        keys_all_up();
        k->arp_opt &= (uint8_t)~ARPO_ON;
        dk_cmd(C_ARPCLR, dk_song.cur_track, 0, 0);
        mark_dirty();
        say("ARP", "OFF");
        break;
    }
    case A_NEXT_PART: {
        int p = ((dk_next_part != 0xFF ? dk_next_part : dk_song.cur_part) + 1) % DK_NPART;
        char s[16] = "PART A";
        s[5] = (char)('A' + p);
        if (!part_go(p))
            say(s, LABEL_NAME[dk_song.part[p].label & 7]);
        break;
    }
    case A_FILL:
        dk_cmd(C_FILL, b, 0, 0);
        if (b)
            say("FILL", "");
        break;
    case A_BAKE: break;                  /* (DREAM to STEP: DREAM is gone) */
    case A_UNDO:
    case A_REDO: {
        int what;
        if (a == A_UNDO ? song_undo(&what) : song_redo(&what)) {
            mark_dirty();
            if (what >= 0) {             /* which track's (and part's, if not this one) take it was */
                char s[20] = "T";
                int l = 1;
                s[l++] = (char)('1' + what % DK_NTRACK);
                s[l++] = ' ';
                l = (int)(cpy(s + l, track_label(what % DK_NTRACK)) - s);
                if (what / DK_NTRACK != dk_song.cur_part) {
                    s[l++] = ' ';
                    s[l++] = (char)('A' + what / DK_NTRACK);
                    s[l] = 0;
                }
                say(a == A_UNDO ? "UNDO" : "REDO", s);
            } else
                say(a == A_UNDO ? "UNDO" : "REDO", what == -1 ? "ROAD" : "PART");
        } else
            say(a == A_UNDO ? "UNDO" : "REDO", "NOTHING");
        break;
    }
    case A_GARAGE:
        ui.page = PG_ROAD;
        ui.garage ^= 1u;
        say("GARAGE", ui.garage ? "PARTS" : "ROAD");
        break;
    case A_TRANS_DOWN:
    case A_TRANS_UP:
        clip_transpose(cur_clip(), a == A_TRANS_UP ? 1 : -1);
        mark_dirty();
        say("TRANSPOSE", a == A_TRANS_UP ? "UP" : "DOWN");
        break;
    case A_OCT_DOWN:
    case A_OCT_UP: {
        int o = cur()->oct + (a == A_OCT_UP ? 1 : -1);
        char s[4];
        if (ui.page == PG_SOUNDS) {          /* SOUNDS: the instrument */
            pb_engine_step(a == A_OCT_UP ? 1 : -1);
            break;
        }
        if (ui.endl_on && cur()->seq == SQ_ENDLESS) {   /* entering ENDLESS: OCT- a rest, OCT+ the last step tied on */
            endl_rest_tie(a == A_OCT_UP);
            break;
        }
        if (o < -3 || o > 3)
            break;
        keys_all_up();
        cur()->oct = (int8_t)o;
        s[0] = o < 0 ? '-' : '+';
        s[1] = (char)('0' + (o < 0 ? -o : o));
        s[2] = 0;
        mark_dirty();
        say("OCTAVE", s);
        break;
    }
    default: break;
    }
}

/* ---------------------------------------------------------------- the keys --- */

/* what a key just played, for the header: its notes with their octaves (C4 E4 G4), a chord's name first; a drum's
 * name; a FINGER pattern's */
static void notes_show(int k)
{
    char *p = ui.notes;
    int i, n = kh[k].n;
    const dk_engine_t *e = TAPE.eng[dk_song.trk[kh[k].trk >= 0 ? kh[k].trk : 0].eng % TAPE.neng];
    ui.notes[0] = 0;
    if (kh[k].trk < 0)
        return;
    if (kh[k].kind == 1) {          /* PERFORM: the effect's name, up top for as long as it is held */
        int j;
        cpy(p, kh[k].notes[0] == 0xFF ? "FILL" : "PERFORM");
        for (j = 0; TAPE.perform_act && j < 11; j++)
            if (TAPE.perform_act[j] == kh[k].notes[0])
                cpy(p, TAPE.perform[j]);
        ui.notes_until = plat_ms() + 400u;
        ui.notes_k = (uint8_t)(k + 1);
        return;
    } else if (e->flags & EF_DRUM) {
        int l;
        cpy(p, "DRUM");
        for (l = 0; l < e->nlanes; l++)
            if (n && e->lane_note[l] == kh[k].notes[0])
                cpy(p, e->lane_name[l]);
    } else {
        if (n >= 3 && have_chord) {          /* a chord: its name only (the notes are on KEYS) */
            cpy(p, chord_name);
            n = 0;
        }
        for (i = 0; i < n && i < 4; i++) {
            int nt = kh[k].notes[i];
            p = cpy(p, DK_NOTE_NAME[nt % 12]);
            p = itoa_b(nt / 12 - 1, p);
            if (i + 1 < n && i < 3)
                *p++ = ' ', *p = 0;
        }
        if (n > 4)
            cpy(p, "+");
    }
    ui.notes_until = plat_ms() + 1500u;
}

static void auto_erase(void);
/* THE EDIT LIST (TRACKS: EDIT): the selected track's clip, one item per white key (the names: ED_NAME in pages.c). The erases ask
 * once (the item goes red, "AGAIN = ERASE"); the second press within 3 s does it, as one undo step (hold EDIT). */
static void edit_list_do(int w)
{
    static uint32_t seed = 0x9E3779B9u;
    static uint8_t cooks;
    int t = dk_song.cur_track, c = cur_clip(), p, any = 0;
    uint32_t now = plat_ms();
    char s[12] = "COOK ";
    if (w < 0 || w >= ED_N)
        return;
    if (w < 3) {                         /* QUANTIZE, COPY BAR, PASTE BAR */
        ui.ed_ask = 0;
        clip_edit_key(w);
        return;
    }
    if (w == 10) {                       /* LAYERS: the takes of the clip */
        ui.lay = 2;
        ui.lay_cur = 0;
        ui.ed_ask = 0;
        return;
    }
    if (dk_recording || dk_counting) {
        say("EDIT", "STOP REC");
        return;
    }
    if (w < 8) {                         /* DOUBLE, VARIATE, HUMANIZE, COOK, RANDOM: one undo step each */
        static const char *const NM[5] = {"DOUBLE", "VARIATE", "HUMANIZE", "COOK", "RANDOM"};
        ui.ed_ask = 0;
        if (!dk_song.clip[c].n) {
            say(NM[w - 3], "THE CLIP IS EMPTY");
            return;
        }
        seed = seed * 2654435761u + now;
        switch (w) {
        case 3: clip_double(c); break;
        case 4: clip_variate(c, seed); break;
        case 5: clip_humanize(c, seed ^ 2246822519u); break;
        case 6: clip_cook(c, seed, cur_eng()); break;
        default: clip_random(c, seed, cur_eng()); break;
        }
        if (w == 6) {
            cooks = (uint8_t)(cooks % 99 + 1);
            itoa_b(cooks, s + 5);
        }
        mark_dirty();
        say(w == 6 ? s : NM[w - 3], "UNDO: HOLD LFO");
        return;
    }
    if (w < 10) {                        /* COPY CLIP, PASTE CLIP */
        ui.ed_ask = 0;
        if (w == 8) {
            if (!dk_song.clip[c].n) {
                say("COPY", "THE CLIP IS EMPTY");
                return;
            }
            clip_copy(c);
            say("CLIP", "COPIED");
        } else {
            clip_paste(c);
            mark_dirty();
            say("CLIP", "PASTED");
        }
        return;
    }
    for (p = 0; p < DK_NPART; p++)       /* the erases: is there anything to go? */
        any |= w == 13 ? dk_song.clip[p * DK_NTRACK + t].n != 0 : p == dk_song.cur_part % DK_NPART &&
               (w == 11 ? dk_song.clip[c].n - auto_count(c, 0) > 0 : auto_count(c, 0) > 0);
    if (!any) {
        say(w == 12 ? "NO AUTOMATION" : "NOTHING TO ERASE", track_label(t));
        ui.ed_ask = 0;
        return;
    }
    if (ui.ed_ask != w + 1 || now - ui.ed_t >= 3000u) {   /* the first press asks */
        ui.ed_ask = (uint8_t)(w + 1);
        ui.ed_t = now;
        say(w == 11 ? "ERASE NOTES" : w == 12 ? "ERASE AUTOMATION" : "ERASE ALL PARTS", "AGAIN = ERASE");
        return;
    }
    ui.ed_ask = 0;
    if (w == 12) {
        auto_erase();
        return;
    }
    undo_group(1);
    for (p = 0; p < DK_NPART; p++) {
        int cl = p * DK_NTRACK + t;
        if ((w == 11 && p != dk_song.cur_part % DK_NPART) || !dk_song.clip[cl].n)
            continue;
        undo_push(cl);
        dk_lock();
        if (w == 11)
            clip_clear(cl);
        else
            clip_clear_all(cl);
        dk_unlock();
    }
    undo_group(0);
    mark_dirty();
    say(w == 11 ? "NOTES ERASED" : "ALL ERASED", "UNDO: HOLD LFO");
}

static void key_down(int k, int vel)
{
    int w = white_of(k), b = black_of(k);
    const dk_track_t *trk = cur();
    const dk_engine_t *e = cur_eng();
    if (ui.page == PG_SOUND && held(B_FX) && w >= 0 && w < 4) {   /* FX held + white 1-4: a stage of the chain off / on, at once */
        int val;
        const char *nm;
        int was = chain_state(w, &val, &nm);
        ui.used[B_FX] = 1;
        if (w == 3)
            dk_song.rsv[DK_NTRACK] ^= 1u;
        else
            dk_song.rsv[dk_song.cur_track] ^= (uint8_t)(1u << w);
        mark_dirty();
        say(nm, was ? "ON" : "BYPASSED");
        return;
    }
    if (held(B_REC)) {                   /* REC held and a key: the take starts now and the key plays into it (REC acts on its
                                          * release: held while playing, the keys were REC + white = a layer's mute, nothing
                                          * recorded - "the keys do nothing in REC"). The layers' mutes: TRACKS > EDIT > LAYERS */
        ui.used[B_REC] = 1;
        if (!dk_recording && !dk_counting && !dk_rec_pending()) {
            const ui_map_t *m = map_of(B_REC);
            if (m)
                action(m->tap, 1);
        }
    }
    if (held(B_HOME)) {                  /* HOME + white: the preset; HOME + black 1-6: the track */
        ui.used[B_HOME] = 1;
        if (w >= 0)
            preset_select(w);
        else if (b < DK_NTRACK)
            track_select(b);
        return;
    }
    if (held(B_SAVE)) {
        ui.used[B_SAVE] = 1;
        if (w >= 12)
            preset_save(w - 12);
        else if (w < 2)
            snapshot_key(w);
        else
            say("SAVE", "W1 SNAP  W2 RECALL  W13-16");
        return;
    }
    if (held(B_ARP) && w >= 0 && w < 8) {
        char s[8] = "PART A";
        ui.used[B_ARP] = 1;
        s[5] = (char)('A' + w);
        if (!part_go(w))
            say(s, dk_playing ? "NEXT" : LABEL_NAME[dk_song.part[w].label & 7]);
        return;
    }
    if (ui.page == PG_SOUNDS && pb_key(w, b))   /* SOUNDS: the keys choose, the choice is heard */
        return;
    if (trk->seq == SQ_GROOVE && (e->flags & EF_DRUM) && TAPE.groove && ui.page != PG_MIX && ui.page != PG_TRACKS &&
        ui.page != PG_PROJECT && ui.page != PG_ROAD) {   /* GROOVE: white 1-15 the grooves, 16 the end, black the fills */
        int t = dk_song.cur_track;
        if (w >= 0 && w < TAPE.ngroove) {
            dk_cmd(C_GROOVE, t, 0, w + 1);
            say(TAPE.groove[w].name, dk_gr_cur[t] || dk_playing ? "NEXT BAR" : "");
        } else if (w >= 0) {
            dk_cmd(C_GROOVE, t, 0, 0);
            say("END", "NEXT BAR");
        } else if (b >= 0 && b < TAPE.nfill) {
            if (!dk_gr_cur[t])
                say("FILL", "PLAY A GROOVE FIRST");
            else {
                dk_cmd(C_GROOVE, t, 1, b + 1);
                say("FILL", TAPE.fill[b].name);
            }
        } else if (b == DK_GR_DICE) {          /* DICE: the same drums, other hits (its white again: the written one) */
            if (!dk_gr_cur[t])
                say("DICE", "PLAY A GROOVE FIRST");
            else {
                dk_cmd(C_GROOVE, t, 3, 0);
                say("DICE", "WHITE AGAIN = BACK");
            }
        }
        return;
    }
    if (ui.page == PG_TRACKS && ui.lay) {   /* the lists: the white keys are their numbered items (EDIT, LAYERS) */
        if (w >= 0)
            ui.lay == 1 ? edit_list_do(w) : lay_do(w);
        return;
    }
    if ((ui.page == PG_MIX || ui.page == PG_TRACKS) && !dk_recording && !dk_counting && !dk_rec_pending() && !ui.armed) {
        /* THE track keys (MIX and TRACKS alike) - not while a take records or waits for its first note: then they play
         * (a take begun on TRACKS stays there; its keys picked tracks and muted them, the take was silent) */
        /* white 1-6 select the track,
                                                        * black 1-6 mute it in the part, black 7 solo, black 8 arm the selected one */
        if (w >= 0 && w < DK_NTRACK) {
            track_select(w);
            return;
        }
        if (b >= 0 && b < DK_NTRACK) {
            part_mute_toggle(b, 0);
            return;
        }
        if (b == DK_NTRACK) {
            int t = dk_song.cur_track;
            dk_song.trk[t].solo ^= 1u;
            mark_dirty();
            say(track_label(t), dk_song.trk[t].solo ? "SOLO" : "SOLO OFF");
            return;
        }
        if (b == DK_NTRACK + 1) {
            rec_arm_toggle(dk_song.cur_track);
            return;
        }
    }
    if (trk->seq == SQ_ENDLESS && ui.endl_on) {   /* ENDLESS: each key a step (a key still held, or at once: one chord) */
        int clip = cur_clip(), nt[10], n, i, steps = dk_song.clip[clip].len / DK_T16, same;
        uint32_t now = plat_ms();
        keys_down(k, vel);
        notes_show(k);
        n = (e->flags & EF_DRUM) ? (kh[k].n ? 1 : 0) : keys_notes_of(k, nt);
        if (e->flags & EF_DRUM)
            nt[0] = kh[k].notes[0];
        same = steps > 0 && (ui.keys || (ui.endl_t && now - ui.endl_t < 60u));
        if (!n || (!same && steps >= 128))
            return;
        if (!same)
            undo_push(clip);
        dk_lock();
        for (i = 0; i < n; i++) {
            dk_ev_t ev;
            ev.pos = (uint16_t)((same ? steps - 1 : steps) * DK_T16);
            ev.dur = DK_T16 - 2;
            ev.note = (uint8_t)nt[i];
            ev.vel = (uint8_t)vel;
            ev.fl = 0;
            ev.layer = 1;
            if (clip_find(clip, ev.pos, ev.note, 0) < 0)
                dk_song_insert(clip, &ev);
        }
        if (!same)
            dk_song.clip[clip].len = (uint16_t)((steps + 1) * DK_T16);
        dk_unlock();
        ui.endl_t = now;
        mark_dirty();
        return;
    }
    if (trk->seq == SQ_ENDLESS && !(e->flags & EF_DRUM) && dk_song.clip[cur_clip()].n &&
        (ui.page == PG_PLAY || ui.page == PG_SEQ || ui.page == PG_REC || ui.page == PG_KEYS) && (w >= 0 || !dk_song.layout)) {
        int root = dk_endl_root(dk_song.cur_track), to = key_note_of(k, dk_song.cur_track);   /* a key: the steps moved to it */
        if (root >= 0 && to >= 0) {
            char st[8];
            int tr = to - root;
            while (tr > 24)
                tr -= 12;
            while (tr < -24)
                tr += 12;
            dk_cmd(C_ENDLTR, dk_song.cur_track, tr, 0);
            st[0] = tr < 0 ? '-' : '+';
            itoa_b(tr < 0 ? -tr : tr, st + 1);
            say("TRANSPOSE", st);
            return;
        }
    }
    switch (ui.page) {
    case PG_ROAD:
        if (ui.garage) {
            if (w >= 0 && w < 8)
                garage_dup(w);
            break;
        }
        if (w >= 0 && w < 8) {           /* a block at the end: the road in the order you build it */
            char st[8] = "PART A";
            road_append(w);
            st[5] = (char)('A' + w);
            say("BLOCK ADDED", st);
            return;
        }
        if (w == 8) {                    /* COPY (white 9): the block on the cursor into the clipboard, kept; PASTE (black 11) puts it in */
            if (dk_song.nroad) {
                ui.road_clip = dk_song.road[ui_road_cur];
                ui.road_clip_ok = 1;
                say("BLOCK", "COPIED");
            }
            return;
        }
        if (b >= 0) {
            static const uint8_t TR[6] = {TR_HALF, TR_DOUBLE, TR_REVERSE, TR_UP, TR_DOWN, TR_STOP};
            static const char *const TN[6] = {"HALF", "DOUBLE", "REVERSE", "UP 12", "DOWN 12", "TAPESTOP"};
            if (!dk_song.nroad && b < 10)
                return;
            if (b < 6) {
                undo_push_road();
                dk_song.road[ui_road_cur].tricks ^= TR[b];
                if (b == 3)
                    dk_song.road[ui_road_cur].tricks &= (uint8_t)~TR_DOWN;
                if (b == 4)
                    dk_song.road[ui_road_cur].tricks &= (uint8_t)~TR_UP;
                say(TN[b], dk_song.road[ui_road_cur].tricks & TR[b] ? "ON" : "OFF");
            } else if (b == 6) {
                undo_push_road();
                dk_song.road[ui_road_cur].drop ^= (uint8_t)(1u << dk_song.cur_track);
                {
                    char d[24] = "DROP ", *q = d + 5;
                    const char *l = track_label(dk_song.cur_track);
                    while (*l && q < d + 22)
                        *q++ = *l++;
                    *q = 0;
                    say(d, dk_song.road[ui_road_cur].drop >> dk_song.cur_track & 1u ? "ON: SILENT IN THIS BLOCK" : "OFF");
                }
            } else if (b == 7) {
                road_delete(0);
                say("BLOCK", "DELETED");
            } else if (b == 8) {
                road_insert(0, 1);
                say("BLOCK", "DOUBLED");
            } else if (b == 9) {
                road_delete(1);
                say("BLOCK", "CUT");
            } else if (ui.road_clip_ok) {
                road_insert(ui.road_clip.part, 0);
                dk_song.road[ui_road_cur] = ui.road_clip;
                say("BLOCK", "PASTED");
            }
            mark_dirty();
            return;
        }
        break;
    case PG_PROJECT:
        if (w >= 0 && w < DK_NPROJ) {
            if (dk_playing) {
                say("PROJECT", "STOP FIRST");
                return;
            }
            tape_ui_keep();
            project_save(set.proj);
            set.proj = (uint8_t)w;
            project_load(w);
            settings_save();
            tape_ui_init();
            dk_cmd(C_RETUNE, 0, 0, 0);
            ui.dirty = 0;
            ui.edits_seen = song_edits;
            {
                char s[12] = "PROJECT 1";
                s[8] = (char)('1' + w);
                say(s, dk_song.name);
            }
            return;
        }
        if (w == 4 || w == 5) {          /* NEW from the template, or the tape's demo */
            if (dk_playing) {
                say("PROJECT", "STOP FIRST");
                return;
            }
            dk_lock();
            if (w == 5 || ui_tmp_template == 4)
                TAPE.demo();
            else {
                dk_song_clear();
                song_template(ui_tmp_template);
            }
            dk_unlock();
            tape_ui_init();
            dk_cmd(C_RETUNE, 0, 0, 0);
            mark_dirty();
            say("NEW", w == 5 ? dk_song.name : TEMPLATE_NAME[ui_tmp_template % 5]);
            return;
        }
        if (b == 0) {
            export_start();
            ui.export_on = 1;
            say("EXPORT", "SENDING");
            return;
        }
        if (b == 1) {
            dk_cmd(C_STEMS, 1, 0, 0);
            say("STEMS", "7 PASSES");
            return;
        }
        if (b == 3) {                    /* CLEAR ALL: the whole composition; a second press within 3 s */
            if (dk_playing) {
                say("PROJECT", "STOP FIRST");
                return;
            }
            if (!ui.clr_t || plat_ms() - ui.clr_t > 3000u) {
                ui.clr_t = plat_ms() | 1u;
                say("CLEAR ALL", "PRESS AGAIN");
                return;
            }
            ui.clr_t = 0;
            song_clear_all();
            tape_ui_init();
            dk_cmd(C_RETUNE, 0, 0, 0);
            ui_road_cur = 0;
            mark_dirty();
            say("CLEAR ALL", "DONE - NO UNDO");
            return;
        }
        if (b == 2) {
            tape_ui_keep();
            say("SAVE", project_save(set.proj) == 0 ? "DONE" : "STOP FIRST");
            ui.dirty = 0;
            return;
        }
        break;
    case PG_SEQ:
        if (trk->seq == SQ_STEP && (e->flags & EF_DRUM)) {   /* the drum grid: the white keys are the steps */
            int clip = cur_clip();
            if (w >= 0) {
                int pos = (ui_step_page * 16 + w) * DK_T16, i;
                if (pos >= clip_len(clip))
                    return;
                i = clip_find(clip, pos, e->lane_note[ui_lane % e->nlanes], 0);
                undo_push(clip);
                dk_lock();
                if (i >= 0)
                    ev_delete(clip, i);
                else {
                    dk_ev_t ev;
                    ev.pos = (uint16_t)pos;
                    ev.dur = DK_T16;
                    ev.note = e->lane_note[ui_lane % e->nlanes];
                    ev.vel = ui_svel;
                    ev.fl = 0;
                    ev.layer = 1;
                    ui_step_ev = (int8_t)dk_song_insert(clip, &ev);
                    dk_note(dk_song.cur_track, ev.note, ui_svel, 0);
                }
                dk_unlock();
                ui.step_held = (int8_t)w;
                mark_dirty();
                return;
            }
            if (b >= 0 && b < e->nlanes) {
                ui_lane = (uint8_t)b;
                dk_note(dk_song.cur_track, e->lane_note[b], 90, 0);
                say("LANE", e->lane_name[b]);
                return;
            }
        } else if (trk->seq == SQ_STEP || seq_edit_row()) {   /* melodic: the key's note (or chord) at the cursor, then on */
            int clip = cur_clip(), nt[10], n, i, pos = ui_cursor * DK_T16;
            keys_down(k, vel);
            notes_show(k);
            n = keys_notes_of(k, nt);
            if (!n)
                return;
            undo_push(clip);
            dk_lock();
            for (i = 0; i < n; i++) {
                int at = clip_find(clip, pos, nt[i], 0);
                if (at >= 0)
                    ev_delete(clip, at);       /* the same note again: gone */
                else
                    ui_step_ev = (int8_t)se_put(clip, pos, nt[i], vel, ui_slen * DK_T16 - 2);
            }
            dk_unlock();
            se_pitch = (uint8_t)nt[0];
            se_f = -1;
            ui.step_key = (uint8_t)(k + 1);
            mark_dirty();
            return;
        }
        break;
    default: break;
    }
    keys_down(k, vel);
    notes_show(k);
    if (!(e->flags & EF_DRUM)) {         /* a note of the hand: heard? checked a moment later (why_tick) */
        ui.why_t = plat_ms() | 1u;
        ui.why_k = (uint8_t)k;
        ui.why_trk = (uint8_t)dk_song.cur_track;
    }
}
/* the hand plays and the track stays silent: say why (a mute, a solo, its VOL, a curve...), or that no voice started */
static int ui_why_last;                  /* (host: the last silence found: reason + 1; 9 no voice, 10 the sound itself) */
static void why_tick(uint32_t now)
{
    static const char *WHY[8] = {"", "MUTED IN PART", "OTHER TRACK SOLO", "DROPPED ON ROAD", "VOLUME AT 0",
                                 "AUTOMATION: VOL 0", "TREMOLO", "DRUMS MUTED"};
    int t = ui.why_trk, r;
    char a[28], b[28], *q;
    const char *l;
    if (!ui.why_t || now - ui.why_t < 250u)
        return;
    ui.why_t = 0;
    if (!(ui.keys >> ui.why_k & 1u) || t != dk_song.cur_track || t >= DK_NTRACK)
        return;
    if (dk_level[t] > 0.002f)
        return;
    r = dk_silent_why(t);
    ui_why_last = r ? r + 1 : !dk_trk_voices[t] ? 9 : 10;
    q = a;
    for (l = track_label(t); *l && q < a + 18; )
        *q++ = *l++;
    for (l = " SILENT"; *l; )
        *q++ = *l++;
    *q = 0;
    q = b;
    if (r)
        for (l = WHY[r]; *l; )
            *q++ = *l++;
    else if (!dk_trk_voices[t]) {        /* no voice started at all */
        for (l = "NO VOICE "; *l; )
            *q++ = *l++;
        q = put_n(q, dk_voices_on);
        *q++ = '/';
        q = put_n(q, DK_NVOICE);
    } else {                             /* voices on, nothing out: the sound itself (the effect, the engine) */
        for (l = "V"; *l; )
            *q++ = *l++;
        q = put_n(q, dk_trk_voices[t]);
        for (l = " FX"; *l; )
            *q++ = *l++;
        q = put_n(q, cur()->fx);
        for (l = " VOL"; *l; )
            *q++ = *l++;
        q = put_n(q, dk_param_value(t, T_VOL) > 99 ? 99 : dk_param_value(t, T_VOL));
    }
    *q = 0;
    say(a, b);
}

static void key_up(int k)
{
    if (ui.page == PG_SEQ && ui.step_key == k + 1) {
        ui.step_key = 0;
        ui_cursor = (uint8_t)((ui_cursor + ui_slen) % (clip_len(cur_clip()) / DK_T16));
    }
    if (ui.page == PG_SEQ && white_of(k) == ui.step_held)
        ui.step_held = -1;
    keys_up(k);
}

/* -------------------------------------------------------------- the knobs --- */
/* AUTOMATION: a knob turned while REC is held (playing), or during a take on this track, goes into the clip as a curve:
 * a point at least a 1/16 apart (12 ticks), the value in a straight line between; where the knob has passed, the older
 * points of this knob are replaced. A gesture (a target, turns less than 0.4 s apart) is one undo step and ends thinned
 * (au_end). Not recording: turning a knob that has a curve takes over till the bar's end. */
#define AU_GAP 12
static int au_dist(int from, int to, int len) { return (to - from + len) % len; }
static void au_write(int pos, int val)
{
    int r;
    dk_lock();
    r = auto_point(au.clip, au.tgt, pos, val, au.last, 0);
    dk_unlock();
    if (r < 0) {
        au.on = 0;
        say("MEMORY FULL", "AUTOMATION STOPPED");
        return;
    }
    au.last = (int16_t)pos;
    au.lastv = (int8_t)val;
    mark_dirty();
}
static void au_end(void)
{
    int len;
    if (!au.on)
        return;
    len = clip_len(au.clip);
    if (au.pp != au.last)
        au_write(au.pp, au.pv);
    if (au.on) {
        dk_lock();
        if (au.start <= au.last)
            auto_thin(au.clip, au.tgt, au.start, au.last, 2);
        else {
            auto_thin(au.clip, au.tgt, au.start, len - 1, 2);
            auto_thin(au.clip, au.tgt, 0, au.last, 2);
        }
        dk_unlock();
    }
    au.on = 0;
}
static void au_poll(uint32_t now)
{
    if (au.on && (now - au.ms > 400u || au.t != dk_song.cur_track ||
                  !(held(B_REC) || (dk_recording && dk_rec_trk == au.t))))
        au_end();
}
static void auto_turn(int f, int tgt, int before, uint32_t now)
{
    int lo, hi, rec, clip, len, pos, pct, v;
    if (!tgt)
        return;
    rec = (held(B_REC) && !dk_recording && !dk_counting) || (dk_recording && dk_rec_trk == dk_song.cur_track);
    if (held(B_REC)) {
        ui.used[B_REC] = 1;                  /* (let go now: no recording of notes) */
    }
    if (!rec || !dk_playing) {
        if (held(B_REC) && !dk_playing)
            say("PLAY FIRST", "THEN REC + KNOB");
        else
            dk_auto_hold(dk_song.cur_track, tgt);      /* a hand on a curve's knob: the hand wins till the bar's end */
        return;
    }
    field_range(f, &lo, &hi);
    clip = cur_clip();
    len = clip_len(clip);
    pos = dk_part_tick % len;
    v = field_get(f);
    pct = hi > lo ? (v - lo) * 100 / (hi - lo) : 0;
    dk_auto_hold(dk_song.cur_track, tgt);
    if (au.on && (au.tgt != tgt || au.t != dk_song.cur_track || au.clip != clip))
        au_end();
    if (!au.on) {                            /* a new gesture: one undo step; the knob's old value is where the curve starts */
        undo_push(clip);
        au.on = 1;
        au.t = (uint8_t)dk_song.cur_track;
        au.tgt = (uint8_t)tgt;
        au.clip = (uint8_t)clip;
        au.start = au.last = au.pp = (int16_t)pos;
        au.lastv = au.pv = (int8_t)(hi > lo ? (before - lo) * 100 / (hi - lo) : 0);
        au.last = -1;
        au_write(pos, au.lastv);
        if (!au.on)
            return;
        say("AUTOMATION", field_label(f));
    }
    au.ms = now;
    au.pp = (int16_t)pos;
    au.pv = (int8_t)pct;
    if (au_dist(au.last, pos, len) >= AU_GAP)
        au_write(pos, pct);
}

static void turn(int i, int e)
{
    int f, lo, hi, step = 1;
    uint32_t now = plat_ms();
    if (ui.page == PG_SOUND && held(B_LFO) && ui.row[PG_SOUND] < 4 && i < 4 && field_target(page_field(i, 0))) {   /* LFO held: MOD 1 onto this knob */
        dk_track_t *k = cur();
        ui.used[B_LFO] = 1;
        if (k->mod[0][0] == MOD_OFF)
            k->mod[0][0] = MOD_SWING;
        k->mod[0][3] = (uint8_t)field_target(page_field(i, 0));
        k->mod[0][1] = (uint8_t)clamp(k->mod[0][1] + e * 3, 0, 100);
        {
            char s[8];
            itoa_b(k->mod[0][1], s);
            say("MOD 1 DEPTH", s);
        }
        mark_dirty();
        return;
    }
    f = page_field(i, held(B_HOME));
    if (held(B_HOME))
        ui.used[B_HOME] = 1;
    if (f == F_ROADCUR && held(B_HOME) && ui.page == PG_ROAD && !ui.garage) {   /* ROAD: HOME + K1 moves the block */
        road_move(e > 0 ? 1 : -1);
        say("BLOCK", e > 0 ? "MOVED RIGHT" : "MOVED LEFT");
        return;
    }
    if (f == F_NONE)
        return;
    field_range(f, &lo, &hi);
    if (hi - lo > 24 && (e > 2 || e < -2))
        step = 3;
    if (ui.page == PG_SEQ && ui.step_held >= 0 && i == 0) {   /* a step held: the knob locks P1 on it (p-lock) */
        dk_ev_t ev;
        int pos = (ui_step_page * 16 + ui.step_held) * DK_T16, v = clamp(field_get(F_P0) + e * 3, 0, 100);
        ev.pos = (uint16_t)pos;
        ev.dur = DK_T16;
        ev.note = T_P0;
        ev.vel = (uint8_t)v;
        ev.fl = EV_MOTION;
        ev.layer = 1;
        dk_lock();
        dk_song_insert(cur_clip(), &ev);
        dk_unlock();
        say("P-LOCK", field_label(F_P0));
        mark_dirty();
        return;
    }
    if (f == F_SENOTE || f == F_SETIE || f == F_SEPITCH || f == F_SHIFT || f == F_MOVE || (f == F_CURSOR && held(B_HOME))) {
        int d = e > 0 ? 1 : -1;             /* the step editor: each turn an edit (a fast one counts once) */
        if (f == F_SENOTE)
            se_note(d);
        else if (f == F_SETIE)
            se_tie(d);
        else if (f == F_SEPITCH)
            se_move(d, held(B_HOME));
        else if (f == F_SHIFT)
            se_rotate(d);
        else if (f == F_MOVE)
            se_transpose(d);
        else                                 /* HOME + K1: a bar at a time */
            field_set(F_CURSOR, (ui_cursor / 16 + d) * 16 + ui_cursor % 16 < 0 ? ui_cursor : (ui_cursor / 16 + d) * 16 + ui_cursor % 16);
        ui.touched = (int8_t)i;
        ui.touch_until = now + 800u;
        return;
    }
    {
        int tgt = ui.page == PG_SOUND || ui.page == PG_MIX ? field_target(f) : 0, before;
        if (tgt && !(au.on && au.t == dk_song.cur_track && au.tgt == tgt) && dk_auto_live(dk_song.cur_track, tgt) >= 0)
            field_set(f, lo + (dk_auto_live(dk_song.cur_track, tgt) * (hi - lo) + 500) / 1000);   /* from where the curve is */
        before = field_get(f);
        field_set(f, before + e * step);
        auto_turn(f, tgt, before, now);
    }
    if (f == F_ROOT)
        ui.root_until = now + 3000u;
    if (f == F_MOOD)
        tape_mood(dk_song.mood);
    if (i < 4) {
        ui.touched = (int8_t)i;
        ui.touch_until = now + 800u;
    } else {                             /* ALGO's extra: not on the screen, so a message */
        char t[24];
        field_text(f, t);
        say(field_label(f), t);
    }
}

/* SELECT and PRESET: on PLAY the tempo and the mood; elsewhere the track and its preset */
static void enc_side(int sel, int e)     /* SELECT (sel 1): the track; PRESET: the preset; HOME + SELECT the tempo */
{
    if (sel && held(B_HOME)) {
        char t[16];
        ui.used[B_HOME] = 1;
        field_set(F_TEMPO, field_get(F_TEMPO) + e);
        itoa_b(dk_song.tempo, t);
        say("TEMPO", t);
        return;
    }
    if (!sel && held(B_HOME)) {          /* HOME + PRESET: SOUNDS (there: the instrument) */
        ui.used[B_HOME] = 1;
        if (ui.page == PG_SOUNDS)
            pb_engine_step(e > 0 ? 1 : -1);
        else
            pb_open();
        return;
    }
    if (sel && held(B_ENV) && dk_recording) {   /* ENV + SELECT while recording: another part, this one copied into an empty one */
        int curp = (dk_playing ? dk_cur_part : dk_song.cur_part) & 7;
        int n = e > 0 ? e : -e, step = e > 0 ? 1 : -1;
        if (n > 7)
            n = 7;
        part_branch((curp + step * n) & 7);
        return;
    }
    if (sel && ui.page == PG_TRACKS && ui.lay == 2) {   /* LAYERS: SELECT moves the cursor */
        int n = lay_count();
        ui.lay_cur = (uint8_t)(n ? ((int)ui.lay_cur + n + (e > 0 ? 1 : -1)) % n : 0);
        return;
    }
    if (sel)
        track_select((dk_song.cur_track + DK_NTRACK + (e > 0 ? 1 : -1)) % DK_NTRACK);
    else
        preset_step(e > 0 ? 1 : -1);
}

/* ----------------------------------------------------------------- MIDI in --- */
static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24 & 0x7Fu, ch = st & 0x0Fu;
        int t = (int)ch;
        if (!set.midi_in)
            continue;
        if (ch == 9) {                   /* channel 10: the first drum track */
            int i;
            for (i = 0; i < DK_NTRACK; i++)
                if (TAPE.eng[dk_song.trk[i].eng % TAPE.neng]->flags & EF_DRUM) {
                    t = i;
                    break;
                }
        }
        if (t >= DK_NTRACK)
            continue;
        if ((st & 0xF0u) == 0x90u && d2) {
            ui.midi_held[t][d1] = 1;
            dk_note(t, (int)d1, (int)d2, 0);
        } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
            if (ui.midi_held[t][d1])
                dk_note(t, (int)d1, 0, 0);
            ui.midi_held[t][d1] = 0;
        } else if ((st & 0xF0u) == 0xB0u && d1 == 64u)
            dk_cmd(C_PEDAL, d2 >= 64u, 0, 0);
    }
}

/* the device's own diagnostics (tools/deck_diag.py): F0 7D 44 4B 53 F7 asks, the answer is one line of text in
 * F0 7D 44 4B 53 <ASCII> F7; the maxima are cleared by each ask. Filled by app.c (the render's and the frame's times) */
static volatile uint32_t dg_rmax, dg_rsum, dg_rn, dg_fmax, dg_fn, dg_vmax, dg_kgap;   /* kgap: longest wait between key reads */
static char *dg_kv(char *p, const char *k, uint32_t v)
{
    p = cpy(p, k);
    p = itoa_b((int)v, p);
    *p++ = ' ';
    *p = 0;
    return p;
}
int app_sysex(const uint8_t *p, uint32_t n)
{
    static uint8_t f[200];
    char *q;
    uint32_t i, k = 0;
    if (n < 4 || p[0] != 0x7D || p[1] != 0x44 || p[2] != 0x4B || p[3] != 0x53)
        return 0;
    f[k++] = 0xF0, f[k++] = 0x7D, f[k++] = 0x44, f[k++] = 0x4B, f[k++] = 0x53;
    q = (char *)&f[k];
    q = dg_kv(q, "t=", plat_ms());
    q = dg_kv(q, "cpu=", plat_cpu_pct());
    q = dg_kv(q, "rmax=", dg_rmax);
    q = dg_kv(q, "ravg=", dg_rn ? dg_rsum / dg_rn : 0);
    q = dg_kv(q, "xrun=", plat_xruns());
    q = dg_kv(q, "ui_max_ms=", dg_fmax);
    q = dg_kv(q, "frames=", dg_fn);
    q = dg_kv(q, "key_gap_ms=", dg_kgap);
    q = dg_kv(q, "voices_max=", dg_vmax);
    q = dg_kv(q, "budget=", (uint32_t)dk_get_budget());
    q = dg_kv(q, "play=", dk_playing);
    q = dg_kv(q, "page=", ui.page);
#ifndef PLAT_HOST
    q = dg_kv(q, "irqoff_max_us=", lock_max);
    q = dg_kv(q, "scan_gap_max_us=", fm1_in_stat.gap_max / FM1_TICKS_PER_US);
    lock_max = 0;
    fm1_in_stat.gap_max = 0;
#endif
    dg_rmax = dg_rsum = dg_rn = dg_fmax = dg_fn = dg_vmax = dg_kgap = 0;
    for (i = k; f[i]; i++)
        f[i] &= 0x7Fu;
    k = i;
    f[k++] = 0xF7;
    plat_sysex_send(f, k);
    return 1;
}

/* ------------------------------------------------------------------- input --- */
static void lights(void)
{
    static const uint8_t PAGE_BTN[NPG] = {B_HOME, B_SEQ, B_FX, B_LFO, B_ENV, B_EDIT, B_SAVE, B_REC, B_ARP, B_GLO, B_SEL, B_HOME, B_HOME};
    uint32_t now = plat_ms(), b = 1u << PAGE_BTN[ui.page], keys = ui.keys, dimk = 0, dimb = 0;
    int k;
    const dk_track_t *trk = cur();
    const dk_engine_t *e = cur_eng();
    if (dk_recording || (dk_counting && ((now / 250u) & 1u)) || (ui.armed && ((now / 500u) & 1u)))
        b |= 1u << B_REC;
    if (dk_playing)
        b |= LED_PLAY_GREEN;
    if (dk_fill)
        b |= 1u << B_GLO;
    if (dk_arp_on(trk))                  /* the ARP on this track: its button lit */
        b |= 1u << B_ARP;
    if (trk->oct > 0)                    /* the octave away from home: its button lit */
        b |= 1u << B_OCTUP;
    else if (trk->oct < 0)
        b |= 1u << B_OCTDN;
    if (ui.dirty)
        dimb |= 1u << B_SAVE;
    if (ui.page == PG_SEQ && trk->seq == SQ_STEP && (e->flags & EF_DRUM)) {   /* the steps on, the step running */
        const dk_clip_t *c = &dk_song.clip[cur_clip()];
        int i, run = (int)(dk_part_tick % (uint16_t)clip_len(cur_clip())) / DK_T16 - ui_step_page * 16;
        for (i = 0; i < c->n; i++) {
            const dk_ev_t *ev = &dk_ev[c->at + i];
            int s = ev->pos / DK_T16 - ui_step_page * 16;
            if (!(ev->fl & EV_MOTION) && ev->note == e->lane_note[ui_lane % e->nlanes] && s >= 0 && s < 16)
                keys |= 1u << WHITE_K[s];
        }
        if (dk_playing && run >= 0 && run < 16)
            dimk |= 1u << WHITE_K[run];
    } else {                             /* the scale's notes glow, the root brighter, the beat on the root */
        int root = dk_song.root % 12;
        uint32_t pool = DK_MODE_POOL[dk_song.mode % DK_NMODES] | 1u;
        for (k = 0; k < NKEYS; k++) {
            int pc = ((dk_song.layout && white_of(k) >= 0 ? key_note_of(k, dk_song.cur_track) : 53 + k + key_shift()) - root + 1200) % 12;
            if (dk_song.layout && white_of(k) >= 0)
                dimk |= 1u << k;
            else if (!dk_song.layout && (pool >> pc & 1u))
                dimk |= 1u << k;
            if (pc == 0 && dk_beat_flash && dk_metro.mode)
                keys |= 1u << k;
        }
    }
    dk_beat_flash = 0;
    plat_play_red(dk_recording ? 1 : 0);
    /* the glow at DIM HI (as OVERWORLD's): DIM LO's pulse is ~1.6 us, and a busy render (flash cache misses in the
     * scan's shift) made the glowing keys flicker on the device */
    plat_lights(btn_swap(b), keys, btn_swap(dimb | ((1u << NB) - 1u)), dimk, 2);
}

/* ERASE AUTOMATION (the EDIT list): every curve of this track in this part (the notes stay); one undo step */
static void auto_erase(void)
{
    int clip = cur_clip();
    au_end();
    if (!auto_count(clip, 0)) {
        say("NO AUTOMATION", "ON THIS TRACK");
        return;
    }
    undo_push(clip);
    dk_lock();
    auto_clear(clip, 0);
    dk_unlock();
    mark_dirty();
    say("AUTOMATION CLEARED", "UNDO: HOLD LFO");
}
/* a key or a knob used while a button is held is a combo: that button's own tap (its page) and hold (UNDO, FILL...) no longer follow */
static void combo_used(void)
{
    int b;
    for (b = 0; b < NB; b++)
        if (held(b)) {
            ui.combo |= (uint16_t)(1u << b);
            if (b != B_GLO)              /* (GLO's FILL may still start: keys played over a fill are no combo of it) */
                ui.used[b] = 1;
        }
}
#define HOLD_ERASE_MS 600u               /* REC held this long (the hint comes at 450 ms) */
/* hold REC: the notes of the last take (the latest REC press, all its passes and tracks) erased; automation and older notes stay */
static void take_erase_ui(void)
{
    int n;
    au_end();
    n = take_erase_last();
    if (n) {
        mark_dirty();
        say("TAKE ERASED", "UNDO: HOLD LFO");
    } else
        say("NO TAKE", "RECORD FIRST");
}
#define KEY_BOUNCE 20u
static void input_do(void);
static void input(void)
{
    in_input = 1;
    input_do();
    in_input = 0;
}
static void input_do(void)
{
    static uint32_t last_in;
    uint32_t keys, btn = btn_swap(plat_buttons()), k, now = plat_ms();
    int32_t e;
    if (last_in && now - last_in > dg_kgap)
        dg_kgap = now - last_in;
    last_in = now;
    touch_poll();
    keys = touch_keys();
    {                                    /* a key let go and back within KEY_BOUNCE ms is one press (a contact's bounce: the mono
                                          * LEAD cut its note and began it again - the crackle at its start and end) */
        static uint32_t up_t[NKEYS];
        for (k = 0; k < NKEYS; k++) {
            uint32_t bit = 1u << k;
            if ((ui.keys & bit) && !(keys & bit)) {
                if (!up_t[k])
                    up_t[k] = now | 1u;
                if ((int32_t)(now - up_t[k]) < (int32_t)KEY_BOUNCE)   /* (signed: now | 1 may be a tick ahead) */
                    keys |= bit;         /* (still down, for now) */
                else
                    up_t[k] = 0;
            } else
                up_t[k] = 0;
        }
    }
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            int v = touch_vel(k);
            if (!v)
                v = touch_vel_contact(k);
            if (!v)
                v = 88;
            v = 24 + v * 3 / 4;
            combo_used();
            key_down((int)k, v);
        } else if (!(keys & bit) && (ui.keys & bit))
            key_up((int)k);
    }
    ui.keys = keys;
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        const ui_map_t *m = map_of((int)k);
        if (!m)
            continue;
        if ((btn & bit) && !(ui.btn & bit)) {                    /* down */
            ui.down_t[k] = now;
            ui.used[k] = 0;
            ui.combo &= (uint16_t)~(1u << k);
            ui.held_done[k] = 0;
            ui.btn |= bit;
            if ((k == B_OCTDN || k == B_OCTUP) && (btn >> B_REC & 1u)) {   /* REC + OCT-: the last take undone; OCT+: back */
                ui.used[B_REC] = 1;
                ui.used[k] = 1;
                action(k == B_OCTDN ? A_UNDO : A_REDO, 1);
            } else if (k != B_HOME && (btn >> B_HOME & 1u) && m->home != A_NONE) {
                ui.used[B_HOME] = 1;
                ui.used[k] = 1;
                action(m->home, 1);
            } else if (m->tap == A_FILL)
                action(A_FILL, 1);
            else if (m->tap == A_PLAY)             /* PLAY starts on the press (in time); held on: PART/SONG */
                action(A_PLAY, 1);
            else if (!m->hold_ms)
                action(m->tap, 1), ui.used[k] = 1;
        } else if (k == B_REC && (btn & bit) && !ui.used[k] && now - ui.down_t[k] >= HOLD_ERASE_MS && !held(B_HOME)) {
            ui.used[k] = 1;              /* REC held, no knob turned: the take just recorded goes (one undo step); it records on if it was */
            take_erase_ui();
        } else if ((btn & bit) && !ui.used[k] && m->hold_ms && m->hold_ms != HOLD_MOD) {   /* held */
            if (now - ui.down_t[k] >= m->hold_ms) {
                ui.used[k] = 1;
                action(m->hold, 1);
            }
        } else if (!(btn & bit) && (ui.btn & bit)) {             /* up */
            ui.btn &= ~bit;
            if (m->tap == A_FILL)
                action(A_FILL, 0);
            else if (m->hold == A_FILL && ui.used[k])   /* GLO held: the FILL ends with it */
                action(A_FILL, 0);
            else if (!ui.used[k] && !(ui.combo >> k & 1u) && m->tap != A_PLAY &&
                     !(now - ui.down_t[k] >= HOLD_HINT_MS &&
                       !(k == B_REC && (dk_recording || dk_counting || ui.armed || ui_auto_on))))
                action(m->tap, 1);       /* a tap is a short press: held past the hint (its hint or HOME's load box was up) = a look, letting go does nothing
                                            (REC while it records / counts / arms keeps its stop) */
        }
    }
    ui.btn = btn;
    se_hear_step(now);                   /* the step editor's note heard for a moment: off */
    if (ui_auto_on && dk_playing && !dk_counting) {   /* AUTO's take: past 16 bars it wraps, 16 it is */
        if (dk_part_tick >= ui.auto_max)
            ui.auto_max = dk_part_tick;
        else if (ui.auto_max > DK_TPB && dk_part_tick + DK_TPB < ui.auto_max)
            ui_auto_on = 0;
    }
    if (ui.auto_fix && !dk_recording)
        auto_apply();
    if (ui.notes_k) {                    /* a PERFORM key still held: its name kept */
        if (kh[ui.notes_k - 1].trk >= 0 && kh[ui.notes_k - 1].kind == 1)
            ui.notes_until = now + 300u;
        else
            ui.notes_k = 0;
    }
    midi_in();
    au_poll(now);
    /* K1..K4: the page's four; ALGO its extra (PLAY: the MOOD); SELECT the track, PRESET the preset */
    for (k = 0; k < 4; k++)
        if ((e = plat_enc(EN_K1 + (int)k)) != 0) {
            combo_used();
            turn((int)k, e);
        }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        combo_used();
        turn(4, e);
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {
        combo_used();
        enc_side(1, e);
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        combo_used();
        enc_side(0, e);
    }
    lights();
}

void ui_input_only(void) { input(); }

/* --------------------------------------------------------------------- frame --- */
void ui_init(void)
{
    int k;
    touch_on(1);
    ui_ready = 1;
    for (k = 0; k < NKEYS; k++)
        kh[k].trk = -1;
    ui.page = PG_PLAY;
    ui.prev = PG_PLAY;
    ui.touched = -1;
    ui.step_held = -1;
    ui.last_ms = plat_ms();
    ui.edits_seen = song_edits;
    tape_ui_init();
    lcd_fill(0, 0, 240, 240, TH_BG);
}

static void housekeeping(uint32_t now)
{
    /* the recording's notes into the song */
    if (rec_flush())
        mark_dirty();
    if (song_edits != ui.edits_seen) {
        ui.edits_seen = song_edits;
        mark_dirty();
    }
    {                                    /* a part's mutes changed at a bar's end (the sequencer did it): to be saved */
        static uint8_t pm_seen;
        if (dk_pm_gen != pm_seen) {
            pm_seen = dk_pm_gen;
            mark_dirty();
        }
    }
    /* BAKE done: the track to STEP */
    if (ui.baking == 1 && dk_recording)    /* (the render has started it) */
        ui.baking = 2;
    if (ui.baking == 2 && !dk_recording) {
        ui.baking = 0;
        cur()->seq = SQ_STEP;
        say("BAKED", "NOW STEP");
        mark_dirty();
    }
    /* the looper: the parts recorded into, in that order; when the take ends each one with notes and no block yet
     * becomes a block at the road's end (the road builds itself; on ROAD the blocks are moved, copied, deleted) */
    if (dk_recording) {
        int p = dk_cur_part & 7, i, have = 0;
        for (i = 0; i < ui.rec_np; i++)
            have |= ui.rec_parts[i] == p;
        if (!have && ui.rec_np < 8)
            ui.rec_parts[ui.rec_np++] = (uint8_t)p;
    } else if (ui.rec_np && !dk_counting) {
        int i, added = -1;
        rec_flush();
        for (i = 0; i < ui.rec_np; i++) {
            int p = ui.rec_parts[i];
            if (!part_empty(p) && !road_has(p) && dk_song.nroad < DK_NROAD) {
                road_append(p);
                added = p;
            }
        }
        ui.rec_np = 0;
        if (added >= 0) {
            char st[16] = "PART A";
            st[5] = (char)('A' + added);
            say("NEW BLOCK", st);
        }
    }
    /* the tape page goes back a bar after the recording */
    ui.rec_end = 0;                      /* (the tape page stays after a take: its settings to look at) */
    /* the export: a few frames a UI frame */
    if (ui.export_on) {
        int i;
        static uint8_t f[320];
        for (i = 0; i < 4; i++) {
            int n = export_next(f);
            if (!n) {
                ui.export_on = 0;
                say("EXPORT", "DONE");
                break;
            }
            plat_sysex_send(f, (uint32_t)n);
        }
    }
    /* saving by itself: once in 5 min at most (the first at once; SAVE on PROJECT saves now), six seconds after the last
     * change, while all rests and is quiet: nothing playing or recording, no key or button held, no knob just touched (an
     * erase stops the sound; it is faded out first). Nothing is shown but the small dot of unsaved changes going out */
    if ((ui.dirty || settings_due) && !dk_playing && !dk_recording && !dk_counting && now - ui.change_t > 6000u &&
        (!ui.save_t || now - ui.save_t >= AUTOSAVE_MS) && dk_master_level < 0.004f && !ui.keys && !ui.btn &&
        now >= ui.touch_until + 2000u && !ui.export_on) {
        ui.save_t = now | 1u;            /* (never 0: that is "not yet") */
        if (settings_due)
            settings_save();
        tape_ui_keep();
        if (ui.dirty && project_save(set.proj) == 0)
            ui.dirty = 0;
        ui.change_t = now;               /* (a failed save waits its turn again) */
    }
}

void ui_frame(void)
{
    uint32_t now = plat_ms(), dt = now - ui.last_ms;
    input();
    if (ann_on())
        ann_tick(now);
    ui.last_ms = now;
    if (dt > 200u)
        dt = 200u;
    {                                    /* the render's load, measured by the platform: the voices' budget follows */
        static uint32_t load_t;
        if (now - load_t >= 100u) {
            load_t = now;
            dk_set_load((int)plat_cpu_pct());
        }
    }
    tape_ui_step(dt);
    pb_audition_tick(now);
    why_tick(now);
    housekeeping(now);
    if (song_layers_full) {              /* the sounds' table of a track is full: the takes after it stay live */
        song_layers_full = 0;
        say("LAYERS FULL", "7 SOUNDS ON THIS TRACK");
    }
    if (song_mem_dropped || song_mem_full) {   /* the event pool filled up while recording (said at most every 2 s) */
        static uint32_t mem_t;
        if (now - mem_t > 2000u || !mem_t) {
            mem_t = now | 1u;
            if (song_mem_full)
                say("MEMORY FULL", "NOTES LOST");
            else
                say("MEMORY FULL", "OLDEST TAKE ERASED");
        }
        song_mem_dropped = song_mem_full = 0;
    }
    if (dk_recording && ui.page != PG_REC && ui.page != PG_TRACKS && ui.page != PG_SOUNDS) {   /* the tape page whenever it records (or TRACKS) */
        ui.prev = ui.page;
        ui.page = PG_REC;
    }
    /* the screen at ~30 frames a second (a band or the garden each): the panel's link and the CPU kept for the rest */
    if (now - ui.draw_t < 30u && now >= ui.draw_t)
        return;
    ui.draw_t = now;
    ui.frame++;
    draw_frame();
}
