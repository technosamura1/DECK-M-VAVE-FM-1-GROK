/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK in fm1-base's host simulator: its checks for "expect" and its commands.
 *   expect: playing recording counting page track part next tempo root mode seq nev events(of the clip now) eventsAB
 *   (part A..H, track 1..6: events of that clip) nroad pass rectrk dirty voices clicks metro fill perform harmroot
 *   textfit (texts cut short: 0) undo playmode; the step editor's cursor senotes selen sepitch; keywN (white key N's
 *   note)
 *   parts: pmask (the part's track mutes, the one now) pmaskA..H pmpend (a mute waiting for the bar's end) newpart (NEW PART 1 COPY, 0 EMPTY)
 *   barsA..H psndA1 (the first event's frozen sound, part A track 1) clenA1 (clip length) pmgen
 *   commands: seed P T N SND (N events into part P (A=0) track T (1..6), sound id SND), holdseed P T N DUR, setbars P N, setpmask P MASK, newpart N,
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

static int auto_edge(int tgt, int last)
{
    const dk_clip_t *c = &dk_song.clip[cur_clip()];
    int i, v = -1;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if ((e->fl & (EV_MOTION | EV_RAMP)) == (EV_MOTION | EV_RAMP) && e->note == tgt) {
            v = e->vel;
            if (!last)
                break;
        }
    }
    return v;
}
static int host_oldck;                    /* the last old-format check (projv1, userold): 0 = it loaded as it should */
/* the song's invariants, 0 = all hold (fuzz); the bits: 1 events within the pool, 2 each clip inside it and sorted, 4 lengths sane,
 * 8 motion targets and velocities valid, 16 voices within the pool, 32 every parameter a track hears inside its range, 64 every
 * track's engine exists and its preset id is a place that engine has, 128 the clips follow one another in the pool in the table's order */
static int invariants(void)
{
    int bad = 0, c, i, t, g;
    if (dk_song.nev > DK_NEV)
        bad |= 1;
    for (c = 0; c < DK_NCLIP; c++) {
        const dk_clip_t *k = &dk_song.clip[c];
        if (k->n && (unsigned)k->at + k->n > DK_NEV)
            { bad |= 2; fprintf(stderr, "DBG clip %d out of pool at %d n %d\n", c, k->at, k->n); }
        if (k->len > DK_TPB * 32)
            bad |= 4;
        for (i = 0; i < k->n && (unsigned)k->at + k->n <= DK_NEV; i++) {
            const dk_ev_t *e = &dk_ev[k->at + i];
            if (i && e->pos < dk_ev[k->at + i - 1].pos)
                { bad |= 2; fprintf(stderr, "DBG clip %d unsorted i %d pos %d prev %d n %d\n", c, i, e->pos, dk_ev[k->at + i - 1].pos, k->n); }
            if ((e->fl & EV_MOTION) && e->note >= T_N)
                bad |= 8;
            if (e->vel > 127)
                bad |= 8;
        }
    }
    {                                      /* 7: the clips follow one another in the pool, in the order of the table (reindex and the edits count on it) */
        unsigned at = 0;
        for (c = 0; c < DK_NCLIP; c++) {
            if (dk_song.clip[c].n && dk_song.clip[c].at != at)
                bad |= 128;
            at += dk_song.clip[c].n;
        }
    }
    if (dk_voices_on > DK_NVOICE)
        bad |= 16;
    for (t = 0; t < DK_NTRACK; t++) {      /* 6: every track's engine exists and its preset id is one that engine has (a block's place not empty) */
        const dk_track_t *k = &dk_song.trk[t];
        int pb = preset_block(k->preset), pi = k->preset < DK_PBASE ? k->preset : (k->preset - DK_PBASE) % DK_NFACT;
        if (k->eng >= TAPE.neng)
            bad |= 64;
        else if (pb == 0 ? pi >= preset_count(k->eng, 0) : pb > 0 && (pb >= DK_NBLK || pi >= preset_count(k->eng, pb)))
            bad |= 64;
    }
    for (t = 0; t < DK_NTRACK; t++)
        for (g = 1; g < T_N; g++)
            if (g >= T_ATT && g <= T_PAN) {
                int v = dk_param_value(t, g);
                if (v < -1000 || v > 1000)
                    bad |= 32;
            }
    return bad;
}
int app_expect(const char *what, int *got)
{
    int g = 0;
    if (!strcmp(what, "playing")) g = dk_playing;
    else if (!strcmp(what, "recording")) g = dk_recording;
    else if (!strcmp(what, "counting")) g = dk_counting;
    else if (!strcmp(what, "page")) g = ui.page;
    else if (!strcmp(what, "rowof")) g = ui.row[ui.page];
    else if (!strcmp(what, "track")) g = dk_song.cur_track;
    else if (!strcmp(what, "part")) g = dk_playing ? dk_cur_part : dk_song.cur_part;
    else if (!strcmp(what, "ptick")) g = dk_part_tick;             /* where the loop is (a part branched while recording does not go back to 0) */
    else if (!strcmp(what, "adur")) {                              /* the longest note in part A, track 1 (a held note cut at the branch is short) */
        const dk_clip_t *c = &dk_song.clip[0];
        int i;
        for (i = 0; i < c->n; i++)
            if (!(dk_ev[c->at + i].fl & EV_MOTION) && dk_ev[c->at + i].dur > g)
                g = dk_ev[c->at + i].dur;
    }
    else if (!strcmp(what, "next")) g = dk_next_part == 0xFF ? -1 : dk_next_part;
    else if (!strcmp(what, "tempo")) g = dk_song.tempo;
    else if (!strcmp(what, "byp")) g = (int)DK_BYP(dk_song.cur_track);                /* SOUND: the chain's stages off (1 insert, 2 echo, 4 reverb) */
    else if (!strcmp(what, "bypt")) g = (int)DK_BYP_TAPE();                            /* ... and the tape (the song's) */
    else if (!strcmp(what, "root")) g = dk_song.root;
    else if (!strcmp(what, "mode")) g = dk_song.mode;
    else if (!strcmp(what, "seq")) g = cur()->seq;
    else if (!strcmp(what, "arp")) g = dk_arp_on(cur());           /* the ARP on this track */
    else if (!strcmp(what, "autoall")) {                                                  /* ... in the whole song */
        int c;
        for (c = 0; c < DK_NCLIP; c++)
            g += auto_count(c, 0);
    }
    else if (!strcmp(what, "inv")) g = invariants();
    else if (!strcmp(what, "oldck")) g = host_oldck;
    else if (!strcmp(what, "nev")) g = dk_song.nev;
    else if (!strcmp(what, "autos")) g = auto_count(cur_clip(), 0);                      /* AUTOMATION: the curve points of the track in this part */
    else if (!strncmp(what, "atgt", 4)) g = auto_count(cur_clip(), atoi(what + 4));      /* ... of one target (T_*) */
    else if (!strncmp(what, "alive", 5)) g = dk_auto_live(dk_song.cur_track, atoi(what + 5));   /* the curve's value now (tenths of %), -1 none */
    else if (!strncmp(what, "pval", 4)) g = dk_param_value(dk_song.cur_track, atoi(what + 4));  /* what the voices hear */
    else if (!strncmp(what, "afirst", 6)) g = auto_edge(atoi(what + 6), 0);              /* the target's first / last point's value (percent) */
    else if (!strncmp(what, "alast", 5)) g = auto_edge(atoi(what + 5), 1);
    else if (!strcmp(what, "events")) g = dk_song.clip[cur_clip()].n;
    else if (!strcmp(what, "same")) g = host_clip_hash() == host_mark;
    else if (!strncmp(what, "events", 6) && what[6] && what[7]) g = host_events(what[6] - 'A', what[7] - '1');
    else if (!strcmp(what, "nroad")) g = dk_song.nroad;
    else if (!strncmp(what, "road", 4) && what[4] >= '0' && what[4] <= '9') g = dk_song.road[atoi(what + 4) % DK_NROAD].part;   /* block N's part */
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
    else if (!strcmp(what, "grcur")) g = dk_gr_cur[dk_song.cur_track];     /* GROOVE: playing (0 none) */
    else if (!strcmp(what, "grnext")) g = dk_gr_next[dk_song.cur_track];   /* queued (255 none, 0 the end) */
    else if (!strcmp(what, "grfill")) g = dk_gr_fill[dk_song.cur_track];
    else if (!strcmp(what, "grhits")) g = (int)dk_gr_hits;
    else if (!strcmp(what, "grmod")) g = dk_gr_mod[dk_song.cur_track];
    else if (!strcmp(what, "grdens")) g = dk_gr_dens[dk_song.cur_track];
    else if (!strcmp(what, "grafill")) g = cur()->drv[0] & 3;
    else if (!strcmp(what, "seq")) g = cur()->seq;
    else if (!strcmp(what, "fired")) g = (int)dk_fired;              /* the takes the sequencer has played (bit = take) */
    else if (!strcmp(what, "armtrk")) g = ui.arm_trk;              /* the tracks armed on TRACKS (bit t) */
    else if (!strcmp(what, "recmode")) g = dk_rec_mode;            /* 0 OVERDUB, 1 REPLACE */
    else if (!strcmp(what, "dirty")) g = ui.dirty;
    else if (!strcmp(what, "voices")) g = dk_voices_on;
    else if (!strncmp(what, "tlevel", 6) && what[6] >= '1' && what[6] <= '6') g = (int)(dk_level[what[6] - '1'] * 1000.0f);   /* the track's output level now (thousandths): audible */
    else if (!strcmp(what, "livev")) g = dk_live_voices;      /* the hand's notes still held and sounding */
    else if (!strncmp(what, "tvoices", 7) && what[7] >= '1' && what[7] <= '6') g = dk_trk_voices[what[7] - '1'];   /* voices sounding on track N */
    else if (!strcmp(what, "clicks")) g = (int)dk_metro_clicks;
    else if (!strcmp(what, "metro")) g = dk_metro.mode;
    else if (!strcmp(what, "snap")) g = snapshot_part() + 1;      /* the snapshot's part + 1 (0 none) */
    else if (!strncmp(what, "tswing", 6)) g = dk_song.trk[atoi(what + 6) % DK_NTRACK].swing;
    else if (!strcmp(what, "fill")) g = dk_fill;
    else if (!strcmp(what, "arpstep")) g = dk_arp_step[dk_song.cur_track];   /* the ARP pattern's step on this track */
    else if (!strcmp(what, "garage")) g = ui.garage;                /* the garage open (hold ENV) */
    else if (!strcmp(what, "perform")) g = dk_perform;
    else if (!strcmp(what, "harmroot")) g = dk_harm_pcs[0];
    else if (!strcmp(what, "textfit")) g = (int)ui.fit_over;
    else if (!strcmp(what, "undo")) g = undo_count();
    else if (!strcmp(what, "playmode")) g = dk_song.play_mode;
    else if (!strcmp(what, "pmask")) g = dk_song.part[(dk_playing ? dk_cur_part : dk_song.cur_part) & 7].mute;
    else if (!strncmp(what, "pmask", 5) && what[5] >= 'A' && what[5] <= 'H') g = dk_song.part[what[5] - 'A'].mute;
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
    else if (!strcmp(what, "endlsteps")) g = dk_song.clip[cur_clip()].len / DK_T16;   /* ENDLESS: the steps played in */
    else if (!strcmp(what, "endltr")) g = dk_endl_tr[dk_song.cur_track];       /* ENDLESS: the transposition now */
    else if (!strcmp(what, "endlstep")) g = dk_endl_step[dk_song.cur_track];   /* ENDLESS: the step playing */
    else if (!strcmp(what, "endlon")) g = ui.endl_on;
    else if (!strncmp(what, "fbank", 5) && what[5] >= '0' && what[5] <= '5') g = dk_song.fbank[what[5] - '0'];   /* the song's byte, raw */
    else if (!strcmp(what, "preset")) g = cur()->preset;
    else if (!strcmp(what, "eng")) g = cur()->eng;
    else if (!strcmp(what, "blk")) g = cur_blk();                     /* the preset block of the track */
    else if (!strncmp(what, "pcnt", 4)) {                              /* "pcnt<engine>.<block>": the presets there */
        const char *d = what + 4;
        while (*d && *d != '.')
            d++;
        g = preset_count(atoi(what + 4), *d ? atoi(d + 1) : 0);
    }
    else if (!strcmp(what, "proj")) g = set.proj;
    else if (!strcmp(what, "firstpos"))        /* the clip's first event: where */
        g = dk_song.clip[cur_clip()].n ? dk_ev[dk_song.clip[cur_clip()].at].pos : -1;
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
        for (pr = 0; pr < DK_NBLK * DK_NFACT; pr++) {
            if ((pr % DK_NFACT) >= preset_count(e, pr / DK_NFACT))
                continue;
            dk_track_setup(dk_song.cur_track, e, preset_id(pr / DK_NFACT, pr % DK_NFACT), SQ_LIVE, TAPE.eng[e]->role);
            ui.page = PG_SOUND;
            for (tab = 0; tab < 5; tab++) {
                ui.row[PG_SOUND] = (uint8_t)tab;
                for (r = 0; r < 3; r++)
                    draw_page_band(r);
            }
            ui.page = PG_TRACKS;             /* the TRACKS rows: the short block and the name */
            for (r = 0; r < 3; r++)
                draw_page_band(r);
            ui.page = PG_SOUND;
            band_begin(90, 50);              /* the preset's popup, "BLOCK - NAME" */
            say_preset(preset_id(pr / DK_NFACT, pr % DK_NFACT));
            draw_toast();
            band_end();
            ui.msg_until = 0;
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
    for (t = 0; t < DK_NTRACK; t++) {        /* TRACKS' EDIT list (and its asked item) and the LAYERS list, each track */
        int lv, ask;
        dk_song.cur_track = (uint8_t)t;
        ui.page = PG_TRACKS;
        for (lv = 1; lv <= 2; lv++)
            for (ask = 0; ask < (lv == 1 ? 2 : 1); ask++) {
                ui.lay = (uint8_t)lv;
                ui.ed_ask = (uint8_t)(ask ? 13 : 0);
                ui.ed_t = plat_ms();
                draw_page_band(0);
                draw_page_band(1);
                draw_page_band(2);
            }
        ui.lay = 0;
        ui.ed_ask = 0;
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
    if (!strcmp(cmd, "dumpclip")) {             /* dumpclip: the current clip's notes (pos dur note vel layer), to the log */
        const dk_clip_t *cl = &dk_song.clip[cur_clip()];
        int i;
        printf("clip %d: n %d len %d\n", cur_clip(), cl->n, clip_len(cur_clip()));
        for (i = 0; i < cl->n; i++) {
            const dk_ev_t *e = &dk_ev[cl->at + i];
            printf("  %5d %5d %3d %3d %3d%s\n", e->pos, e->dur, e->note, e->vel, e->layer, (e->fl & EV_MOTION) ? " M" : "");
        }
        return 1;
    }
    if (!strcmp(cmd, "budget")) {               /* budget N: the voice budget held at N (0: the load moves it again) */
        dk_budget_force(atoi(a));
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
    if (!strcmp(cmd, "holdseed")) {             /* holdseed P T N DUR: N notes together at the start, DUR ticks, the track's own sound */
        int pt = atoi(a) & 7, t = (atoi(b) - 1 + DK_NTRACK) % DK_NTRACK, n = atoi(c), dur = atoi(d), i;
        if (dur < 1)
            dur = DK_T16;
        if (n > 8)
            n = 8;
        dk_lock();
        for (i = 0; i < n; i++) {
            dk_ev_t e = {0};
            e.pos = 0;
            e.dur = (uint16_t)dur;
            e.note = (uint8_t)(60 + i * 3);
            e.vel = 100;
            e.layer = DK_L_MAKE(1, 0);
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
    if (!strcmp(cmd, "projv1")) {               /* projv1: the song saved in the OLD format (an older, shorter song: no lanes, atmo, frozen sounds; no tail), loaded: it must come back whole, the new fields zero */
        static uint8_t img[DK_PROJ_SPAN];
        static phead_t h;
        uint32_t at = DK_PROJ_AT + (uint32_t)set.proj * DK_PROJ_SPAN, len, o, nev = dk_song.nev;
        uint8_t eng[DK_NTRACK], pre[DK_NTRACK], sv[DK_NTRACK];
        uint32_t n0 = 0, i;
        dk_lock();
        for (i = 0; i < DK_NTRACK; i++)
            eng[i] = dk_song.trk[i].eng, pre[i] = dk_song.trk[i].preset, sv[i] = dk_song.trk[i].seq;
        h.magic = PROJ_HEAD_MAGIC;
        h.format = DK_FORMAT;
        h.nsong = DK_SONG_V1;
        h.nev = nev;
        h.sum = song_sum_n(DK_SONG_V1);
        len = (uint32_t)sizeof h + DK_SONG_V1 + nev * (uint32_t)sizeof(dk_ev_t);
        for (i = 0; i < DK_PROJ_SPAN; i++)
            img[i] = 0xFF;
        for (i = 0; i < DK_SONG_V1; i++)
            img[sizeof h + i] = ((const uint8_t *)&dk_song)[i];
        for (i = 0; i < nev * sizeof(dk_ev_t); i++)
            img[sizeof h + DK_SONG_V1 + i] = ((const uint8_t *)dk_ev)[i];
        for (o = 0; o < len; o += 0x1000u) {
            uint32_t end = o + 0x1000u < len ? o + 0x1000u : len, from = o ? o : (uint32_t)sizeof h;
            if (plat_data_erase(at + o) || plat_data_write(at + from, img + from, end - from))
                host_oldck |= 1;
        }
        if (plat_data_write(at, &h, sizeof h))
            host_oldck |= 1;
        dk_unlock();
        host_oldck = host_oldck & 1;
        if (project_load(set.proj) != 0)
            host_oldck |= 2;
        if (dk_song.nev != nev)
            host_oldck |= 4;
        for (i = 0; i < DK_NTRACK; i++)               /* (the load may put the tracks in their role order: the same engines and presets, once each) */
            n0 += (uint32_t)(dk_song.trk[i].eng + 31 * dk_song.trk[i].preset);
        {
            uint32_t m0 = 0;
            for (i = 0; i < DK_NTRACK; i++)
                m0 += (uint32_t)(eng[i] + 31 * pre[i]);
            if (m0 != n0)
                host_oldck |= 8;
        }
        for (i = 0; i < DK_NTRACK * 8 * sizeof(dk_lane_t); i++)
            if (((const uint8_t *)dk_song.lane)[i])
                host_oldck |= 16;
        for (i = 0; i < 8; i++)
            if (dk_song.atmo[i])
                host_oldck |= 32;
        for (i = 0; i < DK_NCLIP; i++)
            if (dk_song.lmute[i])
                host_oldck |= 64;
        for (i = 0; i < DK_NTRACK; i++)
            if (dk_song.trk[i].seq != SQ_STEP && (sv[i] == SQ_LIVE || sv[i] == SQ_STEP))   /* (LIVE loads as SEQ now) */
                host_oldck |= 128;
        return 1;
    }
    if (!strcmp(cmd, "userold")) {              /* userold N: the user store saved as an old one (N engines: 8 or 10); every engine has its first place filled; loaded again */
        static dk_user_t keep;
        uint32_t per = (uint32_t)sizeof dk_user.p[0], n = (uint32_t)atoi(a), i, e;
        if (n > DK_UENG)
            n = DK_UENG;
        for (i = 0; i < sizeof keep; i++)
            ((uint8_t *)&keep)[i] = 0;
        keep.magic = USER_MAGIC;
        for (e = 0; e < DK_UENG; e++) {
            keep.p[e][0].used = 1;
            keep.p[e][0].name[0] = (char)('A' + e);
            keep.p[e][0].p[0] = (int8_t)(10 + e);
        }
        plat_store_save(OBJ_USER, &keep, (uint32_t)sizeof keep.magic + n * per);
        for (i = 0; i < sizeof dk_user; i++)      /* (what the RAM held before: the tail of an old store must not keep it) */
            ((uint8_t *)&dk_user)[i] = 0x5A;
        user_load();
        host_oldck = 0;
        for (e = 0; e < DK_UENG; e++) {
            int want = e < n;
            if (dk_user.p[e][0].used != want || (want && dk_user.p[e][0].p[0] != (int8_t)(10 + e)) || (!want && dk_user.p[e][0].p[0]))
                host_oldck |= 1;
            if (dk_user.p[e][1].used || dk_user.p[e][3].used)
                host_oldck |= 2;
        }
        if (dk_user.magic != USER_MAGIC)
            host_oldck |= 4;
        return 1;
    }
    if (!strcmp(cmd, "trkoct")) {               /* trkoct N: the current track's octave (SOUND's OCTAVE knob: -3..3) */
        dk_lock();
        dk_song.trk[dk_song.cur_track % DK_NTRACK].oct = (int8_t)atoi(a);
        dk_unlock();
        return 1;
    }
    if (!strcmp(cmd, "roundtrip")) {            /* roundtrip: the project saved and loaded again (the fuzz: the song stays the same, the invariants hold) */
        project_save(set.proj);
        project_load(set.proj);
        return 1;
    }
    if (!strcmp(cmd, "trkset")) {               /* trkset T ENG BLK I: track T's engine, and its place I of the block BLK (an empty block: the first block; a place past the end wraps) */
        int e = atoi(b) % TAPE.neng, bl = atoi(c) % DK_NBLK;
        if (preset_count(e, bl) <= 0)
            bl = 0;
        dk_lock();
        dk_track_setup((atoi(a) - 1 + DK_NTRACK) % DK_NTRACK, e, preset_id(bl, atoi(d) % preset_count(e, bl)), SQ_LIVE, TAPE.eng[e]->role);
        dk_unlock();
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
    if (!strcmp(cmd, "ballast")) {              /* ballast N [T]: N events into part H's track 6 (the pool nearly full), in T takes (take = i mod T) */
        int i, n = atoi(a), tk = atoi(b) > 0 ? atoi(b) : 1;
        dk_lock();
        for (i = 0; i < n; i++) {
            dk_ev_t e = {0};
            e.pos = (uint16_t)(i % 1000);
            e.dur = 1;
            e.note = 60;
            e.vel = 50;
            e.layer = DK_L_MAKE(i % tk, 0);
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
        ui.lay = 0;
        ui.ed_ask = 0;
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
    if (!strcmp(cmd, "grclr")) {
        dk_gr_hits = 0;
        return 1;
    }
    if (!strcmp(cmd, "seqtype")) {            /* seqtype N: the sequencer by its SEQ page index (0 STEP 1 LIVE 2 DRIVE 3 FINGER), as K1 of its row */
        field_set(F_SEQTYPE, atoi(a));
        return 1;
    }
    return 0;
}
