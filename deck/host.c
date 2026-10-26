/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK in fm1-base's host simulator: its checks for "expect" and its commands.
 *   expect: playing recording counting page track part next tempo root mode seq nev events(of the clip now) eventsAB
 *   (part A..H, track 1..6: events of that clip) nroad pass rectrk dirty voices clicks metro fill perform harmroot
 *   textfit (texts cut short: 0) undo playmode; the step editor's cursor senotes selen sepitch; keywN (white key N's
 *   note)
 *   parts: pmask (the part's track mutes, the one now) pmaskA..H pmpend (a mute waiting for the bar's end) newpart (NEW PART 1 COPY, 0 EMPTY)
 *   barsA..H psndA1 (the first event's frozen sound, part A track 1) clenA1 (clip length) pmgen tmute0..5 (a track's old global mute)
 *   commands: seed P T N SND (N events into part P (A=0) track T (1..6), sound id SND), setbars P N, setpmask P MASK, newpart N,
 *   ballast N (N events into part H's track 6: the pool nearly full), demo (the tape's demo song), export FILE (the song's SysEx stream to a .syx file), textaudit (every page,
 *   row, track and engine drawn: the texts that did not fit counted), page N, track N, wavsolo T */
static int host_events(int part, int trk) { return dk_song.clip[(part & 7) * DK_NTRACK + (trk % DK_NTRACK)].n; }
static uint32_t host_mark;                   /* "mark": the clip now, to compare with ("same": 1 if it is as marked) */
static uint32_t host_clip_hash(void)
{
    const dk_clip_t *c = &dk_song.clip[cur_clip()];
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        h = (h ^ e->pos) * 16777619u;
        h = (h ^ e->note) * 16777619u;
        h = (h ^ e->vel) * 16777619u;
        h = (h ^ e->dur) * 16777619u;
    }
    return h;
}

int app_expect(const char *what, int *got)
{
    int g = 0;
    if (!strcmp(what, "playing")) g = dk_playing;
    else if (!strcmp(what, "recording")) g = dk_recording;
    else if (!strcmp(what, "counting")) g = dk_counting;
    else if (!strcmp(what, "page")) g = ui.page;
    else if (!strcmp(what, "track")) g = dk_song.cur_track;
    else if (!strcmp(what, "part")) g = dk_playing ? dk_cur_part : dk_song.cur_part;
    else if (!strcmp(what, "next")) g = dk_next_part == 0xFF ? -1 : dk_next_part;
    else if (!strcmp(what, "tempo")) g = dk_song.tempo;
    else if (!strcmp(what, "root")) g = dk_song.root;
    else if (!strcmp(what, "mode")) g = dk_song.mode;
    else if (!strcmp(what, "seq")) g = cur()->seq;
    else if (!strcmp(what, "nev")) g = dk_song.nev;
    else if (!strcmp(what, "events")) g = dk_song.clip[cur_clip()].n;
    else if (!strcmp(what, "same")) g = host_clip_hash() == host_mark;
    else if (!strncmp(what, "events", 6) && what[6] && what[7]) g = host_events(what[6] - 'A', what[7] - '1');
    else if (!strcmp(what, "nroad")) g = dk_song.nroad;
    else if (!strcmp(what, "pass")) g = dk_pass;
    else if (!strcmp(what, "lat")) g = dk_rec_lat;
    else if (!strcmp(what, "evpos")) g = dk_song.clip[cur_clip()].n ? dk_ev[dk_song.clip[cur_clip()].at].pos : -1;   /* the clip's first event */
    else if (!strcmp(what, "evlast")) g = dk_song.clip[cur_clip()].n ? dk_ev[dk_song.clip[cur_clip()].at + dk_song.clip[cur_clip()].n - 1].pos : -1;
    else if (!strcmp(what, "evdur")) g = dk_song.clip[cur_clip()].n ? dk_ev[dk_song.clip[cur_clip()].at].dur : -1;
    else if (!strcmp(what, "evsnd")) g = dk_song.clip[cur_clip()].n ? (int)DK_L_SND(dk_ev[dk_song.clip[cur_clip()].at].layer) : -1;   /* the first event's frozen sound */
    else if (!strcmp(what, "evlay")) g = dk_song.clip[cur_clip()].n ? (int)DK_L_TAKE(dk_ev[dk_song.clip[cur_clip()].at].layer) : -1;
    else if (!strcmp(what, "nfrozen") || !strcmp(what, "nlive")) {      /* the clip's notes with a frozen sound / with the track's own */
        const dk_clip_t *c = &dk_song.clip[cur_clip()];
        int i;
        for (i = 0; i < c->n; i++)
            if (!(dk_ev[c->at + i].fl & EV_MOTION) && (DK_L_SND(dk_ev[c->at + i].layer) != 0) == (what[1] == 'f'))
                g++;
    }
    else if (!strcmp(what, "snds")) {                                   /* the track's frozen sounds */
        int i;
        for (i = 0; i < DK_NSND; i++)
            g += dk_song.snd[dk_song.cur_track][i].used;
    }
    else if (!strcmp(what, "sndeng")) g = dk_song.snd[dk_song.cur_track][0].used ? dk_song.snd[dk_song.cur_track][0].eng : -1;
    else if (!strcmp(what, "allsnds")) {                                /* the frozen sounds of all tracks */
        int i, t;
        for (t = 0; t < DK_NTRACK; t++)
            for (i = 0; i < DK_NSND; i++)
                g += dk_song.snd[t][i].used;
    }
    else if (!strcmp(what, "clrask")) g = ui.clr_t ? 1 : 0;           /* CLEAR ALL asked once */
    else if (!strcmp(what, "vfrozen")) g = dk_voices_frozen;
    else if (!strcmp(what, "locktake")) g = dk_lock_take;
    else if (!strcmp(what, "recarm")) g = dk_rec_arm;
    else if (!strcmp(what, "armed")) g = ui.armed;
    else if (!strcmp(what, "rectrk")) g = dk_rec_trk;
    else if (!strcmp(what, "recmask")) g = dk_rec_mask;           /* the tracks the render records (bit t) */
    else if (!strcmp(what, "nlay")) g = lay_count();                 /* LAYERS: the current clip's layers */
    else if (!strcmp(what, "lmute")) g = (int)dk_song.lmute[lay_clip()];   /* its muted takes (bit = take) */
    else if (!strcmp(what, "lsolo")) g = dk_lsolo_clip == lay_clip() ? dk_lsolo_take + 1 : 0;
    else if (!strcmp(what, "laycur")) g = ui.lay_cur;
    else if (!strcmp(what, "layview")) g = ui.lay;
    else if (!strcmp(what, "fired")) g = (int)dk_fired;              /* the takes the sequencer has played (bit = take) */
    else if (!strcmp(what, "armtrk")) g = ui.arm_trk;              /* the tracks armed on TRACKS (bit t) */
    else if (!strcmp(what, "recmode")) g = dk_rec_mode;            /* 0 OVERDUB, 1 REPLACE */
    else if (!strcmp(what, "dirty")) g = ui.dirty;
    else if (!strcmp(what, "voices")) g = dk_voices_on;
    else if (!strcmp(what, "clicks")) g = (int)dk_metro_clicks;
    else if (!strcmp(what, "metro")) g = dk_metro.mode;
    else if (!strcmp(what, "fill")) g = dk_fill;
    else if (!strcmp(what, "perform")) g = dk_perform;
    else if (!strcmp(what, "harmroot")) g = dk_harm_pcs[0];
    else if (!strcmp(what, "textfit")) g = (int)ui.fit_over;
    else if (!strcmp(what, "undo")) g = undo_count();
    else if (!strcmp(what, "playmode")) g = dk_song.play_mode;
    else if (!strcmp(what, "pmask")) g = dk_song.part[(dk_playing ? dk_cur_part : dk_song.cur_part) & 7].mute;
    else if (!strncmp(what, "pmask", 5) && what[5] >= 'A' && what[5] <= 'H') g = dk_song.part[what[5] - 'A'].mute;
    else if (!strncmp(what, "tmute", 5) && what[5] >= '0' && what[5] <= '5') g = dk_song.trk[what[5] - '0'].mute;
    else if (!strncmp(what, "solo", 4) && what[4] >= '0' && what[4] <= '5') g = dk_song.trk[what[4] - '0'].solo;
    else if (!strcmp(what, "pmpend")) g = dk_pm_x;
    else if (!strcmp(what, "pmgen")) g = dk_pm_gen;
    else if (!strcmp(what, "newpart")) g = dk_newpart_copy;
    else if (!strncmp(what, "bars", 4) && what[4] >= 'A' && what[4] <= 'H') g = dk_song.part[what[4] - 'A'].bars;
    else if (!strncmp(what, "psnd", 4) && what[4] >= 'A' && what[4] <= 'H' && what[5] >= '1') {
        const dk_clip_t *c = &dk_song.clip[(what[4] - 'A') * DK_NTRACK + (what[5] - '1') % DK_NTRACK];
        g = c->n ? (int)DK_L_SND(dk_ev[c->at].layer) : -1;
    }
    else if (!strncmp(what, "clen", 4) && what[4] >= 'A' && what[4] <= 'H' && what[5] >= '1')
        g = dk_song.clip[(what[4] - 'A') * DK_NTRACK + (what[5] - '1') % DK_NTRACK].len;
    else if (!strcmp(what, "fbank")) g = dk_fb_bank(dk_song.cur_track);        /* FINGER's bank of the track */
    else if (!strncmp(what, "fbank", 5) && what[5] >= '0' && what[5] <= '5') g = dk_song.fbank[what[5] - '0'];   /* the song's byte, raw */
    else if (!strcmp(what, "preset")) g = cur()->preset;
    else if (!strcmp(what, "eng")) g = cur()->eng;
    else if (!strcmp(what, "mute")) g = cur()->mute;
    else if (!strcmp(what, "proj")) g = set.proj;
    else if (!strcmp(what, "bars")) g = dk_song.part[dk_song.cur_part & 7].bars;   /* the part's */
    else if (!strcmp(what, "cursor")) g = ui_cursor;                 /* the step editor's */
    else if (!strcmp(what, "senotes")) g = field_get(F_SENOTE);
    else if (!strcmp(what, "selen")) g = se_len_of();
    else if (!strcmp(what, "sepitch")) g = field_get(F_SEPITCH);
    else if (!strncmp(what, "keyw", 4)) g = key_note_of(WHITE_K[atoi(what + 4) & 15], dk_song.cur_track);   /* a white key's note */
    else
        return 0;
    *got = g;
    return 1;
}

void app_host_reboot(void)
{
    uint8_t *b = (uint8_t *)&dk_song;
    uint32_t i;
    for (i = 0; i < sizeof dk_song; i++)
        b[i] = 0;
}

static void host_textaudit(void)
{
    int p, r, t, e, pr, keep_page = ui.page, keep_t = dk_song.cur_track, tab;
    dk_track_t keep[DK_NTRACK];
    for (t = 0; t < DK_NTRACK; t++)
        keep[t] = dk_song.trk[t];
    ui.fit_over = 0;
    for (e = 0; e < TAPE.neng; e++)          /* every engine's every preset on the SOUND page, every tab */
        for (pr = 0; pr < DK_NFACT; pr++) {
            dk_track_setup(dk_song.cur_track, e, pr, SQ_LIVE, TAPE.eng[e]->role);
            ui.page = PG_SOUND;
            for (tab = 0; tab < 4; tab++) {
                ui.row[PG_SOUND] = (uint8_t)tab;
                for (r = 0; r < 3; r++)
                    draw_page_band(r);
            }
            dk_song.trk[dk_song.cur_track].fx = (uint8_t)(pr % TAPE.nfx);
            ui.row[PG_SOUND] = 2;
            for (r = 0; r < 3; r++)
                draw_page_band(r);
        }
    for (t = 0; t < DK_NTRACK; t++)
        dk_song.trk[t] = keep[t];
    for (p = 0; p < NPG; p++)                /* every page, every row, every track, every sequencer */
        for (t = 0; t < DK_NTRACK; t++) {
            int sq;
            dk_song.cur_track = (uint8_t)t;
            for (sq = 0; sq < SQ_N; sq++) {
                if (p != PG_SEQ && sq)
                    break;
                if (p == PG_SEQ)
                    dk_song.trk[t].seq = (uint8_t)sq;
                ui.page = (uint8_t)p;
                for (r = 0; r < page_rows(p); r++) {
                    ui.row[p] = (uint8_t)r;
                    draw_page_band(0);
                    draw_page_band(1);
                    draw_page_band(2);
                }
                ui.garage = (uint8_t)(p == PG_ROAD);
                if (p == PG_ROAD)
                    draw_page_band(0), draw_page_band(1), draw_page_band(2);
                ui.garage = 0;
            }
            dk_song.trk[t].seq = keep[t].seq;
        }
    for (p = 0; p < TAPE.nmood; p++) {       /* every mood's name in the PLAY page's foot */
        dk_song.mood = (uint8_t)p;
        ui.page = PG_PLAY;
        band_begin(172, 68);
        draw_footer(176);
        band_end();
    }
    for (t = 0; t < DK_NTRACK; t++)
        dk_song.trk[t] = keep[t];
    dk_song.cur_track = (uint8_t)keep_t;
    ui.page = (uint8_t)keep_page;
    printf("textaudit: %u text(s) cut short\n", ui.fit_over);
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b;
    (void)c;
    (void)d;
    if (!strcmp(cmd, "mark")) {
        host_mark = host_clip_hash();
        return 1;
    }
    if (!strcmp(cmd, "qnt")) {                  /* qnt N: the track's QUANT (the latency scenario wants none) */
        cur()->qnt = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "onnote")) {               /* onnote N: ON NOTE (the first note starts the take) */
        dk_metro.on_note = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "recmode")) {              /* recmode N: REC MODE 0 OVERDUB / 1 REPLACE */
        dk_rec_mode = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "recarm")) {               /* recarm N: REC ARM (two-step REC) on/off */
        dk_rec_arm = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "locktake")) {             /* locktake N: LOCK TAKE on/off */
        dk_lock_take = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "lat")) {                  /* lat N: REC LAT in ms */
        dk_rec_lat = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "seed")) {                 /* seed P T N SND: N events into part P (0 = A), track T (1..6), with frozen sound SND */
        int pt = atoi(a) & 7, t = (atoi(b) - 1 + DK_NTRACK) % DK_NTRACK, n = atoi(c), i, snd = atoi(d);
        dk_lock();
        if (snd >= 1 && snd <= DK_NSND)         /* (the table's place: the events point to it) */
            dk_song.snd[t][snd - 1].used = 1;
        for (i = 0; i < n; i++) {
            dk_ev_t e = {0};
            e.pos = (uint16_t)(i * DK_T16 % (DK_TPB * 2));
            e.dur = DK_T16;
            e.note = (uint8_t)(48 + i % 12);
            e.vel = 90;
            e.layer = DK_L_MAKE(snd ? 1 : 0, snd);
            dk_song_insert(pt * DK_NTRACK + t, &e);
        }
        dk_unlock();
        return 1;
    }
    if (!strcmp(cmd, "seedl")) {                /* seedl P T N D: N events (an octave per take) of take D % 10 with frozen sound D / 10 */
        int pt = atoi(a) & 7, t = (atoi(b) - 1 + DK_NTRACK) % DK_NTRACK, n = atoi(c), i, tk = atoi(d) % 10, snd = atoi(d) / 10;
        dk_lock();
        if (snd >= 1 && snd <= DK_NSND)
            dk_song.snd[t][snd - 1].used = 1;
        for (i = 0; i < n; i++) {
            dk_ev_t e = {0};
            e.pos = (uint16_t)(i * DK_T16 * 2 % (DK_TPB * 2));
            e.dur = DK_T16;
            e.note = (uint8_t)(36 + tk * 5 + i % 3);
            e.vel = 90;
            e.layer = DK_L_MAKE(tk, snd);
            dk_song_insert(pt * DK_NTRACK + t, &e);
        }
        if (dk_song.clip[pt * DK_NTRACK + t].layers < tk)
            dk_song.clip[pt * DK_NTRACK + t].layers = (uint8_t)tk;
        dk_unlock();
        return 1;
    }
    if (!strcmp(cmd, "seedat")) {               /* seedat P T POS: one note (take 1) at tick POS of part P, track T */
        dk_ev_t e = {0};
        e.pos = (uint16_t)atoi(c);
        e.dur = DK_T16;
        e.note = 60;
        e.vel = 90;
        e.layer = DK_L_MAKE(1, 0);
        dk_lock();
        dk_song_insert((atoi(a) & 7) * DK_NTRACK + (atoi(b) - 1 + DK_NTRACK) % DK_NTRACK, &e);
        dk_unlock();
        return 1;
    }
    if (!strcmp(cmd, "reseq")) {                /* reseq T SQ: track T's sequencer value (a reserved one too), saved and loaded (the migration) */
        dk_lock();
        dk_song.trk[(atoi(a) - 1 + DK_NTRACK) % DK_NTRACK].seq = (uint8_t)atoi(b);
        dk_unlock();
        project_save(set.proj);
        project_load(set.proj);
        return 1;
    }
    if (!strcmp(cmd, "firedclr")) {
        dk_fired = 0;
        return 1;
    }
    if (!strcmp(cmd, "setbars")) {              /* setbars P N */
        dk_song.part[atoi(a) & 7].bars = (uint8_t)atoi(b);
        return 1;
    }
    if (!strcmp(cmd, "setpmask")) {             /* setpmask P MASK */
        dk_song.part[atoi(a) & 7].mute = (uint8_t)atoi(b);
        return 1;
    }
    if (!strcmp(cmd, "newpart")) {              /* newpart N: NEW PART COPY (1) / EMPTY (0) */
        dk_newpart_copy = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "ballast")) {              /* ballast N: N events into part H's track 6 (the pool nearly full) */
        int i, n = atoi(a);
        dk_lock();
        for (i = 0; i < n; i++) {
            dk_ev_t e = {0};
            e.pos = (uint16_t)(i % 1000);
            e.dur = 1;
            e.note = 60;
            e.vel = 50;
            if (dk_song_insert(7 * DK_NTRACK + 5, &e) < 0)
                break;
        }
        dk_unlock();
        return 1;
    }
    if (!strcmp(cmd, "demo")) {
        dk_lock();
        TAPE.demo();
        dk_unlock();
        tape_ui_init();
        dk_cmd(C_RETUNE, 0, 0, 0);
        return 1;
    }
    if (!strcmp(cmd, "export")) {               /* the stream as the device sends it, into a .syx file */
        FILE *f = fopen(a, "wb");
        static uint8_t fr[320];
        int n, frames = 0;
        if (!f)
            return 1;
        export_start();
        while ((n = export_next(fr)) > 0) {
            fwrite(fr, 1, (size_t)n, f);
            frames++;
        }
        fclose(f);
        printf("export: %d frames to %s\n", frames, a);
        return 1;
    }
    if (!strcmp(cmd, "textaudit")) {
        host_textaudit();
        return 1;
    }
    if (!strcmp(cmd, "row")) {               /* row N: the page's row (the screenshots) */
        ui.row[ui.page] = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "page")) {
        ui.page = (uint8_t)(atoi(a) % NPG);
        return 1;
    }
    if (!strcmp(cmd, "row")) {
        ui.row[ui.page] = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "garage")) {
        ui.garage = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "mood")) {
        song_mood(atoi(a));
        tape_mood(atoi(a));
        dk_cmd(C_RETUNE, 0, 0, 0);
        return 1;
    }
    if (!strcmp(cmd, "biome")) {
        dk_song.biome = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "dayphase")) {
        gd.phase = (float)atoi(a) * 0.01f;
        return 1;
    }
    if (!strcmp(cmd, "flower")) {
        fl.grow = fl.shown = (float)atoi(a) * 0.01f;
        return 1;
    }
    if (!strcmp(cmd, "uiprof")) {               /* the UI's cost: ms a frame, page by page (host) */
        int pg, i, keep = ui.page;
        for (pg = 0; pg < NPG; pg++) {
            clock_t c0;
            ui.page = (uint8_t)pg;
            c0 = clock();
            for (i = 0; i < 120; i++)
                ui_frame();
            printf("uiprof %-8s %.3f ms a frame\n", PAGE_NAME[pg], (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / 120.0);
        }
        ui.page = (uint8_t)keep;
        return 1;
    }
    if (!strcmp(cmd, "splash")) {
        app_splash();
        return 1;
    }
    if (!strcmp(cmd, "seqtype")) {
        cur()->seq = (uint8_t)atoi(a);
        return 1;
    }
    return 0;
}
