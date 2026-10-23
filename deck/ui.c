/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK's panel: the buttons by the map (ui_map.c), the keys (keys.c, and each page's own use of them), the encoders
 * (the page's three fields, coloured; the free knobs), recording (the tape page, the count-in), the road's live
 * recording, undo, saving by itself in the silence, the lights, MIDI in, the export's stream. The pages are drawn in
 * pages.c. Part of the unity build. */

enum { PG_PLAY, PG_SEQ, PG_SOUND, PG_MIX, PG_ROAD, PG_KEYS, PG_PROJECT, PG_REC, PG_ARP, NPG };
static const char *const PAGE_NAME[NPG] = {"PLAY", "SEQ", "SOUND", "MIX", "ROAD", "KEYS", "PROJECT", "REC", "ARP"};

static struct {
    uint8_t page, prev, row[NPG], garage, dirty, baking, road_rec, root_sel;
    uint32_t btn, keys, down_t[NB];
    uint8_t used[NB], held_done[NB];
    char msg[2][24];
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
    uint32_t draw_t, garden_t, notes_until, page_t;   /* page_t: the page or its row changed (the hint shows 3 s) */
    char notes[28];
    uint8_t mix_key;                     /* MIX: the white key down (its track + 1): a tap mutes, held 1 s clears */
    uint32_t mix_t;
} ui;

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 700u;     /* a VHS-plate's 0.6 s, and a little */
    if (ann_on()) {
        char t[48];
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

static void mark_dirty(void)
{
    ui.dirty = 1;
    ui.change_t = plat_ms();
}

static int held(int b) { return (int)(ui.btn >> b & 1u); }

#include "pages.c"

/* ------------------------------------------------------------------ actions --- */
static void page_go(int p)
{
    ui.page_t = plat_ms();
    if (ui.page == p) {                  /* again: its next tab or row */
        ui.row[p] = (uint8_t)((ui.row[p] + 1) % page_rows(p));
        if (p == PG_SOUND) {
            static const char *const TAB[4] = {"ENGINE", "ENV", "FX", "MOD"};
            say("SOUND", TAB[ui.row[p] & 3]);
        }
    } else {
        if (p != PG_REC)
            ui.prev = (uint8_t)p;
        ui.page = (uint8_t)p;
    }
    ui.garage = 0;
}

static void track_select(int t)
{
    keys_all_up();
    dk_song.cur_track = (uint8_t)(t % DK_NTRACK);
    ui_step_ev = -1;
    say(track_label(t), cur_eng()->name);
}

/* the KEYS track (TAPE.keys_set): its twelve presets are on three engines */
static int keys_group(void)
{
    return TAPE.keys_set && (cur()->role == TAPE.keys_role || cur()->role == R_BELL);
}
static int keys_group_at(void)           /* where the track is in it, -1 off it */
{
    int i;
    for (i = 0; i < DK_NFACT; i++)
        if (TAPE.keys_set[i][0] == cur()->eng && TAPE.keys_set[i][1] == cur()->preset)
            return i;
    return -1;
}
static void preset_select(int slot)
{
    dk_preset_t tmp;
    if (slot < DK_NFACT && keys_group()) {
        keys_all_up();
        cur()->eng = TAPE.keys_set[slot][0];
        slot = TAPE.keys_set[slot][1];
    }
    track_preset(dk_song.cur_track, slot);
    mark_dirty();
    say(preset_of(cur()->eng, slot, &tmp)->name, slot >= DK_NFACT ? "USER" : cur_eng()->name);
}

static void preset_step(int d)          /* PRESET: the next one (the user's only where saved) */
{
    int s = cur()->preset, n;
    if (keys_group()) {                  /* KEYS: along its twelve */
        int g = keys_group_at();
        preset_select(((g < 0 ? 0 : g) + d + DK_NFACT) % DK_NFACT);
        return;
    }
    for (n = 0; n < DK_NFACT + 4; n++) {
        s = (s + d + DK_NFACT + 4) % (DK_NFACT + 4);
        if (s < DK_NFACT || dk_user.p[cur()->eng & 7][(s - DK_NFACT) & 3].used)
            break;
    }
    preset_select(s);
}

static void preset_save(int slot)        /* SAVE held + white 13..16 */
{
    dk_upreset_t *u = &dk_user.p[cur()->eng & 7][slot & 3];
    const dk_track_t *k = cur();
    dk_preset_t tmp;
    const dk_preset_t *from = preset_of(k->eng, k->preset, &tmp);
    int i;
    char s[16] = "SAVED TO 13";
    for (i = 0; i < 11 && from->name[i]; i++)
        u->name[i] = from->name[i];
    if (k->preset < DK_NFACT && i < 9) {   /* a copy of a factory one: its name and a star */
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
    cur()->preset = (uint8_t)(DK_NFACT + (slot & 3));
    s[10] = (char)('3' + (slot & 3));
    say(s, u->name);
}

static void rec_start(void)
{
    int t = dk_song.cur_track, clip = cur_clip(), t2 = dk_song.split && dk_song.harm_track != t ? dk_song.harm_track : 0xFF;
    rec_reset();
    if (cur()->seq == SQ_ENDLESS) {      /* ENDLESS: a new loop, from its first note */
        undo_push(clip);
        dk_lock();
        clip_clear(clip);
        dk_song.clip[clip].len = 0;
        dk_unlock();
    }
    dk_cmd(C_REC, 1, 0, t | t2 << 16);
    ui.rec_end = 0;
    if (ui.page != PG_REC) {
        ui.prev = ui.page;
        ui.page = PG_REC;
    }
    say("REC", dk_playing ? "NEW PASS" : dk_metro.on_note ? "PLAY A NOTE" : "COUNT IN");
}

static void rec_stop_ui(void)
{
    dk_cmd(C_REC, 0, 0, 0);
    say("REC", "OFF");
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

static void action(int a, int b)
{
    switch (a) {
    case A_PAGE_SOUND: page_go(PG_SOUND); break;
    case A_PAGE_SEQ: page_go(PG_SEQ); break;
    case A_PAGE_ROAD: page_go(PG_ROAD); break;
    case A_PAGE_MIX: page_go(PG_MIX); break;
    case A_PAGE_KEYS: page_go(PG_KEYS); break;
    case A_PAGE_PROJECT:
        page_go(PG_PROJECT);
        ui_slot_sel = -1;
        break;
    case A_PAGE_PLAY: page_go(PG_PLAY); break;
    case A_PLAY:
        if (dk_playing || dk_counting) {
            dk_cmd(C_PLAY, 0, 0, 0);
            ui.change_t = plat_ms() - 3000u;   /* saved soon after the stop */
            ui.road_rec = 0;
        } else
            dk_cmd(C_PLAY, 1, 0, 0);
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
        if (ui.page == PG_ROAD && !dk_song.play_mode) {   /* the road, recorded from the parts you play */
            ui.road_rec ^= 1u;
            if (ui.road_rec) {
                undo_push_road();
                dk_song.nroad = 0;
                ui.road_part = dk_cur_part;
                ui.road_loops = 0;
                ui.road_last_tick = dk_part_tick;
                if (!dk_playing)
                    dk_cmd(C_PLAY, 1, 0, 0);
            }
            say("ROAD", ui.road_rec ? "REC" : "DONE");
        } else if (dk_recording || dk_counting)
            rec_stop_ui();
        else
            rec_start();
        break;
    case A_CLEAR:
        if (cur()->seq == SQ_TOMBOLA) {
            dk_cmd(C_TOMBOLA, 0, 0, 0);
            dk_song.nball = 0;
            say("TOMBOLA", "EMPTIED");
        } else {
            undo_push(cur_clip());
            dk_lock();
            clip_clear(cur_clip());
            dk_unlock();
            mark_dirty();
            say("CLIP", "CLEARED");
        }
        break;
    case A_METRO:
        dk_metro.mode = (uint8_t)(dk_metro.mode ? 0 : 1);
        settings_save();
        say("METRO", dk_metro.mode ? "ON" : "OFF");
        break;
    case A_NEXT_TRACK: track_select((dk_song.cur_track + 1) % DK_NTRACK); break;
    case A_MUTE:
        cur()->mute ^= 1u;
        mark_dirty();
        {
            char s[16] = "TRACK 1";
            s[6] = (char)('1' + dk_song.cur_track);
            say(s, cur()->mute ? "MUTED" : "ON");
        }
        break;
    case A_ARP: {                         /* the ARP's page (and the ARP on); again there: its next row */
        dk_track_t *k = cur();
        if (cur_eng()->flags & EF_DRUM) {
            say("ARP", "NOT ON DRUMS");
            break;
        }
        if (!dk_arp_on(k)) {
            keys_all_up();
            k->arp_opt |= ARPO_ON;
            mark_dirty();
            ui.row[PG_ARP] = 0;
            if (ui.page != PG_ARP)
                page_go(PG_ARP);
            say("ARP", "ON");
        } else
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
        dk_cmd(C_PART, p, 0, 0);
        if (!dk_playing)
            dk_song.cur_part = (uint8_t)p;
        s[5] = (char)('A' + p);
        say(s, LABEL_NAME[dk_song.part[p].label & 7]);
        break;
    }
    case A_FILL:
        dk_cmd(C_FILL, b, 0, 0);
        if (b)
            say("FILL", "");
        break;
    case A_BAKE:
        if (cur()->seq == SQ_DRIVE) {
            int bars = dk_song.part[dk_song.cur_part].bars > 4 ? 4 : dk_song.part[dk_song.cur_part].bars;
            undo_push(cur_clip());
            dk_lock();
            clip_clear(cur_clip());
            dk_song.clip[cur_clip()].len = (uint16_t)(bars * DK_TPB);
            dk_unlock();
            rec_reset();
            dk_cmd(C_REC, 1, 0, dk_song.cur_track | (bars ? bars : 1) << 8 | 0xFF << 16);
            ui.baking = 1;
            say("BAKE", "DRIVE TO STEP");
        } else
            say("BAKE", "ONLY DRIVE");
        break;
    case A_UNDO:
    case A_REDO: {
        int what;
        if (a == A_UNDO ? song_undo(&what) : song_redo(&what)) {
            mark_dirty();
            say(a == A_UNDO ? "UNDO" : "REDO", what < 0 ? "ROAD" : "CLIP");
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
    if (kh[k].kind == 2 || kh[k].kind == 3) {
        const dk_finger_t *set_ = (e->flags & EF_DRUM) ? TAPE.finger_drum : TAPE.finger_mel;
        if (kh[k].kind == 2 && set_)
            cpy(p, set_[kh[k].notes[0] & 15].name);
        else
            cpy(p, "VARIATION");
    } else if (kh[k].kind == 1)
        return;
    else if (e->flags & EF_DRUM) {
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

static void key_down(int k, int vel)
{
    int w = white_of(k), b = black_of(k);
    const dk_track_t *trk = cur();
    const dk_engine_t *e = cur_eng();
    if (held(B_HOME)) {
        ui.used[B_HOME] = 1;
        if (w >= 0)
            preset_select(w);
        else if (b < DK_NTRACK)
            track_select(b);
        else {                           /* the clip's operations */
            static const char *const OPN[5] = {"COPY", "PASTE", "DOUBLE", "VARIATE", "HUMANIZE"};
            int c = cur_clip();
            switch (b - DK_NTRACK) {
            case 0: clip_copy(c); break;
            case 1: clip_paste(c); break;
            case 2: clip_double(c); break;
            case 3: clip_variate(c, plat_ms() * 2654435761u); break;
            default: clip_humanize(c, plat_ms() * 2246822519u); break;
            }
            if (b - DK_NTRACK)
                mark_dirty();
            say("CLIP", OPN[(b - DK_NTRACK) % 5]);
        }
        return;
    }
    if (held(B_SAVE)) {
        ui.used[B_SAVE] = 1;
        if (w >= 12)
            preset_save(w - 12);
        else
            say("SAVE", "WHITE 13-16");
        return;
    }
    if (held(B_ARP) && w >= 0 && w < 8) {
        char s[8] = "PART A";
        ui.used[B_ARP] = 1;
        dk_cmd(C_PART, w, 0, 0);
        if (!dk_playing)
            dk_song.cur_part = (uint8_t)w;
        s[5] = (char)('A' + w);
        say(s, dk_playing ? "NEXT" : LABEL_NAME[dk_song.part[w].label & 7]);
        return;
    }
    if (held(B_SEQ) && w >= 0 && w < NSEQ) {
        ui.used[B_SEQ] = 1;
        field_set(F_SEQTYPE, w);
        say("SEQUENCER", SEQ_NAME[SQ_LIST[w]]);
        return;
    }
    switch (ui.page) {
    case PG_ROAD:
        if (ui.garage)
            break;
        if (w >= 0 && w < 8) {
            road_insert(w, 0);
            say("ROAD", "BLOCK ADDED");
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
                say("DROP", track_label(dk_song.cur_track));
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
        if (b == 2) {
            tape_ui_keep();
            say("SAVE", project_save(set.proj) == 0 ? "DONE" : "STOP FIRST");
            ui.dirty = 0;
            return;
        }
        break;
    case PG_MIX:
        if (w >= 0 && w < DK_NTRACK) {       /* the mute on the release; held 1 s: the track's clip cleared */
            ui.mix_key = (uint8_t)(w + 1);
            ui.mix_t = plat_ms();
            return;
        }
        if (b >= 0 && b < DK_NTRACK) {
            dk_song.trk[b].solo ^= 1u;
            mark_dirty();
            say(track_label(b), dk_song.trk[b].solo ? "SOLO" : "SOLO OFF");
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
        } else if (trk->seq == SQ_STEP) {   /* melodic: the key's note (or chord) at the cursor, then on */
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
                else {
                    dk_ev_t ev;
                    ev.pos = (uint16_t)pos;
                    ev.dur = (uint16_t)(ui_slen * DK_T16 - 2);
                    ev.note = (uint8_t)nt[i];
                    ev.vel = (uint8_t)vel;
                    ev.fl = 0;
                    ev.layer = 1;
                    ui_step_ev = (int8_t)dk_song_insert(clip, &ev);
                }
            }
            dk_unlock();
            ui.step_key = (uint8_t)(k + 1);
            mark_dirty();
            return;
        }
        break;
    case PG_KEYS:
        if (plat_ms() < ui.root_until) {     /* ROOT chosen: the key's note is the root */
            int pc = (53 + k) % 12;
            field_set(F_ROOT, pc);
            say("ROOT", DK_NOTE_NAME[pc]);
            ui.root_until = 0;
            return;
        }
        break;
    default: break;
    }
    keys_down(k, vel);
    notes_show(k);
    if (cur()->seq == SQ_TOMBOLA && dk_song.nball < 12)
        dk_song.ball[dk_song.nball++] = (uint8_t)(53 + k);
}

static void key_up(int k)
{
    if (ui.mix_key && white_of(k) == ui.mix_key - 1) {
        int w = ui.mix_key - 1;
        ui.mix_key = 0;
        dk_song.trk[w].mute ^= 1u;
        mark_dirty();
        say(track_label(w), dk_song.trk[w].mute ? "MUTED" : "ON");
        return;
    }
    if (ui.page == PG_SEQ && ui.step_key == k + 1) {
        ui.step_key = 0;
        ui_cursor = (uint8_t)((ui_cursor + ui_slen) % (clip_len(cur_clip()) / DK_T16));
    }
    if (ui.page == PG_SEQ && white_of(k) == ui.step_held)
        ui.step_held = -1;
    keys_up(k);
}

/* -------------------------------------------------------------- the knobs --- */
static void turn(int i, int e)
{
    int f, lo, hi, step = 1;
    uint32_t now = plat_ms();
    if (ui.page == PG_SOUND && held(B_LFO) && ui.row[PG_SOUND] == 0 && i < 4) {   /* LFO held: MOD 1 onto this knob */
        dk_track_t *k = cur();
        ui.used[B_LFO] = 1;
        if (k->mod[0][0] == MOD_OFF)
            k->mod[0][0] = MOD_SWING;
        k->mod[0][3] = (uint8_t)(T_P0 + i);
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
    field_set(f, field_get(f) + e * step);
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
    /* recording LIVE: the knob's move into the clip */
    if (dk_recording && dk_rec_trk == dk_song.cur_track && ui.page == PG_SOUND) {
        int tgt = f >= F_P0 && f <= F_P5 ? T_P0 + (f - F_P0) : f == F_ATT ? T_ATT : f == F_DEC ? T_DEC :
                  f == F_REL ? T_REL : f == F_FXAMT ? T_FXAMT : 0;
        if (tgt) {
            dk_ev_t ev;
            int clip = cur_clip(), len = clip_len(clip), v = field_get(f);
            field_range(f, &lo, &hi);
            ev.pos = (uint16_t)(dk_part_tick % len);
            ev.dur = 0;
            ev.note = (uint8_t)tgt;
            ev.vel = (uint8_t)(hi > lo ? (v - lo) * 100 / (hi - lo) : 0);
            ev.fl = EV_MOTION | EV_FRESH;
            ev.layer = dk_pass;
            dk_lock();
            dk_song_insert(clip, &ev);
            dk_unlock();
        }
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

int app_sysex(const uint8_t *p, uint32_t n)
{
    (void)p;
    (void)n;
    return 0;                            /* (the export goes out; nothing comes in yet) */
}

/* ------------------------------------------------------------------- input --- */
static void lights(void)
{
    static const uint8_t PAGE_BTN[NPG] = {B_HOME, B_SEQ, B_FX, B_LFO, B_ENV, B_EDIT, B_SAVE, B_REC, B_ARP};
    uint32_t now = plat_ms(), b = 1u << PAGE_BTN[ui.page], keys = ui.keys, dimk = 0, dimb = 0;
    int k;
    const dk_track_t *trk = cur();
    const dk_engine_t *e = cur_eng();
    if (dk_recording || (dk_counting && ((now / 250u) & 1u)))
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
    } else if (trk->seq == SQ_FINGER) {
        for (k = 0; k < 16; k++)
            if (dk_finger_latch[dk_song.cur_track] >> k & 1u)
                keys |= 1u << WHITE_K[k];
        dimk = 0xFFFFu & 0;
        for (k = 0; k < 16; k++)
            dimk |= 1u << WHITE_K[k];
    } else {                             /* the scale's notes glow, the root brighter, the beat on the root */
        int root = dk_song.root % 12;
        uint32_t pool = DK_MODE_POOL[dk_song.mode % DK_NMODES] | 1u;
        for (k = 0; k < NKEYS; k++) {
            int pc = ((53 + k) - root + 120) % 12;
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
    plat_lights(b, keys, dimb | ((1u << NB) - 1u), dimk, 2);
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms();
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            int v = touch_vel(k);
            if (!v)
                v = touch_vel_contact(k);
            if (!v)
                v = 88;
            v = 24 + v * 3 / 4;
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
            ui.held_done[k] = 0;
            ui.btn |= bit;
            if (k != B_HOME && (btn >> B_HOME & 1u) && m->home != A_NONE) {
                ui.used[B_HOME] = 1;
                ui.used[k] = 1;
                action(m->home, 1);
            } else if (m->tap == A_FILL)
                action(A_FILL, 1);
            else if (m->tap == A_PLAY)             /* PLAY starts on the press (in time); held on: PART/SONG */
                action(A_PLAY, 1);
            else if (!m->hold_ms)
                action(m->tap, 1), ui.used[k] = 1;
        } else if ((btn & bit) && !ui.used[k] && m->hold_ms && m->hold_ms != HOLD_MOD) {   /* held */
            if (now - ui.down_t[k] >= m->hold_ms) {
                ui.used[k] = 1;
                action(m->hold, 1);
            }
        } else if (!(btn & bit) && (ui.btn & bit)) {             /* up */
            ui.btn &= ~bit;
            if (m->tap == A_FILL)
                action(A_FILL, 0);
            else if (!ui.used[k] && m->tap != A_PLAY)
                action(m->tap, 1);
        }
    }
    ui.btn = btn;
    if (ui.mix_key && now - ui.mix_t >= 1000u) {   /* MIX: a white held 1 s, its track's clip in this part cleared */
        int t = ui.mix_key - 1, c = dk_song.cur_part % DK_NPART * DK_NTRACK + t;
        char s[16] = "TRACK 1";
        ui.mix_key = 0;
        undo_push(c);
        dk_lock();
        clip_clear(c);
        dk_unlock();
        if (dk_song.trk[t].seq == SQ_TOMBOLA && t == dk_song.cur_track) {
            dk_cmd(C_TOMBOLA, 0, 0, 0);
            dk_song.nball = 0;
        }
        mark_dirty();
        s[6] = (char)('1' + t);
        say(s, "CLEARED");
    }
    midi_in();
    /* K1..K4: the page's four; ALGO its extra (PLAY: the MOOD); SELECT the track, PRESET the preset */
    for (k = 0; k < 4; k++)
        if ((e = plat_enc(EN_K1 + (int)k)) != 0)
            turn((int)k, e);
    if ((e = plat_enc(EN_ALGO)) != 0)
        turn(4, e);
    if ((e = plat_enc(EN_SELECT)) != 0)
        enc_side(1, e);
    if ((e = plat_enc(EN_PRESET)) != 0)
        enc_side(0, e);
    lights();
}

void ui_input_only(void) { input(); }

/* --------------------------------------------------------------------- frame --- */
void ui_init(void)
{
    int k;
    touch_on(1);
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
    /* BAKE done: the track to STEP */
    if (ui.baking == 1 && dk_recording)    /* (the render has started it) */
        ui.baking = 2;
    if (ui.baking == 2 && !dk_recording) {
        ui.baking = 0;
        cur()->seq = SQ_STEP;
        say("BAKED", "NOW STEP");
        mark_dirty();
    }
    /* the tape page goes back a bar after the recording */
    if (ui.page == PG_REC && !dk_recording && !dk_counting) {
        if (!ui.rec_end)
            ui.rec_end = now + (uint32_t)bar_ms();
        else if (now >= ui.rec_end) {
            ui.page = ui.prev;
            ui.rec_end = 0;
        }
    } else
        ui.rec_end = 0;
    /* the road recorded live: each part's loops become a block when the next one starts */
    if (ui.road_rec && dk_playing) {
        if (dk_part_tick < ui.road_last_tick && dk_cur_part == ui.road_part)
            ui.road_loops++;
        if (dk_cur_part != ui.road_part) {
            if (dk_song.nroad < DK_NROAD) {
                dk_song.road[dk_song.nroad].part = ui.road_part;
                dk_song.road[dk_song.nroad].reps = (uint8_t)(ui.road_loops + 1 > 8 ? 8 : ui.road_loops + 1);
                dk_song.road[dk_song.nroad].tricks = 0;
                dk_song.road[dk_song.nroad].drop = 0;
                dk_song.nroad++;
            }
            ui.road_part = dk_cur_part;
            ui.road_loops = 0;
            mark_dirty();
        }
        ui.road_last_tick = dk_part_tick;
    }
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
    /* saving by itself: a few seconds after a change, while all rests and is quiet (an erase stops the sound) */
    if (ui.dirty && !dk_playing && !dk_recording && !dk_counting && now - ui.change_t > 4000u &&
        dk_master_level < 0.004f && !ui.keys) {
        tape_ui_keep();
        if (project_save(set.proj) == 0)
            ui.dirty = 0;
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
    housekeeping(now);
    if (dk_recording && ui.page != PG_REC) {   /* the tape page whenever it records */
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
