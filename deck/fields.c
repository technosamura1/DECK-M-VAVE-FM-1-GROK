/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The fields: every value a page shows on its three coloured encoders, by id: its name, its range, how it reads, how
 * it is set (the song's, the track's, the metronome's...). The pages list which three they show (pages.c). Part of
 * the unity build. */

enum {
    F_NONE,
    /* PLAY */ F_TEMPO, F_MACRO, F_MOOD,
    /* SOUND */ F_P0, F_P1, F_P2, F_P3, F_P4, F_P5, F_ATT, F_DEC, F_REL, F_FXTYPE, F_FXAMT, F_FXA, F_FXB, F_MODTYPE, F_MODDEPTH, F_MODRATE,
    F_MODTGT, F_MODSLOT,
    /* SEQ */ F_LANE, F_SVEL, F_SPROB, F_SRATCH, F_STEPPAGE, F_CURSOR, F_SLEN, F_CLIPLEN, F_QNT, F_TSWING, F_DIV,
    F_ARPMODE, F_ARPDIV, F_ARPOCT, F_ARPLATCH, F_ARPON, F_ARPRHY, F_ARPGATE, F_ARPSRC, F_ENERGY, F_STYLE, F_VAR, F_SPIN, F_GRAV, F_BOUNCE, F_TQUANT, F_FLATCH,
    F_SEQTYPE, F_OCT, F_DRIFT, F_MIDICH, F_ROLE,
    /* MIX */ F_VOL, F_PAN, F_REV, F_ECHO, F_SIDECHAIN, F_SPACE, F_DUST, F_SLEEP,
    /* ROAD */ F_ROADCUR, F_ROADPART, F_ROADREPS, F_GPART, F_GLABEL, F_GBARS, F_PLAYMODE,
    /* KEYS */ F_ROOT, F_MODE, F_CHORD, F_WHITE, F_BLACK, F_QUALITY, F_VOICING, F_STRUM, F_LEAD, F_LAYOUT, F_SPLIT,
    F_BASS, F_KOCT, F_DIM, F_HARMTRK,
    /* PROJECT */ F_SLOT, F_TEMPLATE, F_SWING, F_SONGSTYLE, F_METRO, F_COUNT, F_CLICK, F_MDIV, F_MVOL, F_ONNOTE,
    F_MIDIOUT, F_CLOCK, F_MIDIIN, F_ECHOTIME, F_CPU, F_VERSION,
    /* REC */ F_RECTRK,
    F_N
};

static const char *const SEQ_NAME[SQ_N] = {"STEP", "LIVE", "ENDLESS", "ARP", "DRIVE", "FINGER", "HOLD", "TOMBOLA"};
/* the sequencers offered (ARP is a layer over them now; ENDLESS and HOLD folded into LIVE and the ARP's LATCH) */
#define NSEQ 5
static const uint8_t SQ_LIST[NSEQ] = {SQ_STEP, SQ_LIVE, SQ_DRIVE, SQ_FINGER, SQ_TOMBOLA};
static int seq_index(int sq)
{
    int i;
    for (i = 0; i < NSEQ; i++)
        if (SQ_LIST[i] == sq)
            return i;
    return 1;
}
static const char *const MOD_NAME[MOD_N] = {"OFF", "SWING", "DICE", "PEDAL", "FOLLOW", "TREMOLO"};
static const char *const ARP_NAME[6] = {"UP", "DOWN", "UPDN", "RAND", "ORDER", "CHORD"};
static const char *const DIV_NAME[DK_ARP_NDIV] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const ARPR_NAME[ARPR_N] = {"STRAIGHT", "PULSE", "OFFBEAT", "DOTTED", "GALLOP", "RISE", "SKIP"};
static const char *const ARPG_NAME[ARPG_N] = {"SHORT", "NORMAL", "LONG", "TIE"};   /* as the encoder turns */
static const uint8_t ARPG_ORDER[ARPG_N] = {ARPG_SHORT, ARPG_NORMAL, ARPG_LONG, ARPG_TIE};
static const char *const ARPSRC_NAME[2] = {"KEYS", "CHORD"};
static const char *const EDIV_NAME[4] = {"1/4", "1/8", "1/16", "1/8T"};
static const char *const VOICING_NAME[VC_N] = {"CLOSE", "OPEN", "SPREAD", "DROP2", "POWER"};
static const char *const WHITE_NAME[2] = {"NOTES", "CHORDS"};
static const char *const BLACK_NAME[3] = {"CHORDS", "PERFORM", "OCTAVE"};
static const char *const LAYOUT_NAME[2] = {"CHROMA", "SCALE"};
static const char *const QUAL_NAME[2] = {"DIATONIC", "FIXED"};
static const char *const ONOFF[2] = {"OFF", "ON"};
static const char *const SPACE_NAME[4] = {"ROOM", "HALL", "CAVE", "SHIMMER"};
static const char *const LABEL_NAME[8] = {"INTRO", "VERSE", "PRE", "CHORUS", "DROP", "BRIDGE", "BREAK", "OUTRO"};
static const char *const TEMPLATE_NAME[5] = {"EMPTY", "POP", "CLUB", "AMBIENT", "DEMO"};
static const char *const METRO_MODE[3] = {"OFF", "REC", "ALWAYS"};
static const char *const METRO_SND[METRO_N] = {"CLICK", "WOOD", "HAT", "COWBELL", "RIM"};
static const char *const MDIV_NAME[3] = {"1/4", "1/8", "1/16"};
static const char *const QUANT3[3] = {"OFF", "1/16", "1/8"};
static const char *const PMODE_NAME[2] = {"PART", "SONG"};
static const char *const ECHO_T[4] = {"1/8.", "1/4", "1/8", "1/4."};
static const char *const TARGET_NAME[T_N] = {"-", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", "ATTACK", "DECAY",
                                             "RELEASE", "FX AMT", "FX A", "FX B", "REVERB", "ECHO", "VOLUME", "PAN",
                                             "SWING"};
static const char *const ROLE_NAME[R_N] = {"DRUM", "BASS", "KEYS", "ARP", "PAD", "LEAD", "BELL", "NATURE"};

static dk_track_t *cur(void) { return &dk_song.trk[dk_song.cur_track % DK_NTRACK]; }
static const dk_engine_t *cur_eng(void) { return TAPE.eng[cur()->eng % TAPE.neng]; }
/* a modulation target's name: an engine knob by its own name (CUTOFF, not P1) */
static const char *target_label(int v)
{
    const dk_engine_t *e = cur_eng();
    v %= T_N;
    if (v >= T_P0 && v <= T_P7 && v - T_P0 < e->nparams)
        return e->params[v - T_P0].name;
    return TARGET_NAME[v];
}
static int ui_tmp_template, ui_slot_sel = -1;
static uint8_t ui_mod_slot, ui_lane, ui_cursor, ui_step_page, ui_slen = 1, ui_svel = 100, ui_road_cur, ui_gpart;
static int8_t ui_step_ev = -1;          /* the step last touched (its event in the clip), for VEL/PROB */

static int cur_clip(void) { return dk_song.cur_part % DK_NPART * DK_NTRACK + dk_song.cur_track % DK_NTRACK; }
static dk_ev_t *step_ev(void)
{
    const dk_clip_t *c = &dk_song.clip[cur_clip()];
    return ui_step_ev >= 0 && ui_step_ev < c->n ? &dk_ev[c->at + ui_step_ev] : 0;
}

static void field_range(int f, int *lo, int *hi)
{
    *lo = 0;
    *hi = 100;
    switch (f) {
    case F_TEMPO: *lo = 40; *hi = 200; break;
    case F_MOOD: *hi = TAPE.nmood - 1; break;
    case F_P0:
    case F_P1:
    case F_P2:
    case F_P3:
    case F_P4:
    case F_P5: {
        const dk_engine_t *e = cur_eng();
        int i = f - F_P0;
        if (i < e->nparams) {
            *lo = e->params[i].lo;
            *hi = e->params[i].hi;
        }
        break;
    }
    case F_FXTYPE: *hi = TAPE.nfx - 1; break;
    case F_MODTYPE: *hi = MOD_N - 1; break;
    case F_MODTGT: *hi = T_N - 1; break;
    case F_MODSLOT: *hi = 1; break;
    case F_LANE: *hi = (cur_eng()->nlanes ? cur_eng()->nlanes : 8) - 1; break;
    case F_SVEL: *lo = 1; *hi = 127; break;
    case F_SPROB: *hi = 3; break;
    case F_SRATCH: *hi = 3; break;
    case F_STEPPAGE: *hi = 15; break;
    case F_CURSOR: *hi = 255; break;
    case F_SLEN: *lo = 1; *hi = 16; break;
    case F_CLIPLEN: *lo = 1; *hi = 16; break;
    case F_DIV: *hi = 3; break;
    case F_ARPMODE: *hi = 5; break;
    case F_ARPDIV: *hi = DK_ARP_NDIV - 1; break;
    case F_ARPRHY: *hi = ARPR_N - 1; break;
    case F_ARPGATE: *hi = ARPG_N - 1; break;
    case F_ARPSRC: *hi = 1; break;
    case F_ARPON: *hi = 1; break;
    case F_ARPOCT: *lo = 1; *hi = 3; break;
    case F_ARPLATCH:
    case F_FLATCH:
    case F_LEAD:
    case F_SPLIT:
    case F_BASS:
    case F_DIM:
    case F_ONNOTE:
    case F_MIDIOUT:
    case F_CLOCK:
    case F_MIDIIN:
    case F_LAYOUT:
    case F_WHITE:
    case F_QUALITY:
    case F_PLAYMODE: *hi = 1; break;
    case F_STYLE:
    case F_SONGSTYLE: *hi = TAPE.nstyle - 1; break;
    case F_TQUANT: *hi = 2; break;
    case F_SEQTYPE: *hi = NSEQ - 1; break;
    case F_OCT:
    case F_KOCT: *lo = -3; *hi = 3; break;
    case F_MIDICH: *lo = 1; *hi = 16; break;
    case F_ROLE: *hi = R_N - 1; break;
    case F_PAN: *lo = -50; *hi = 50; break;
    case F_SPACE: *hi = 3; break;
    case F_ROADCUR: *hi = dk_song.nroad ? dk_song.nroad - 1 : 0; break;
    case F_ROADPART:
    case F_GPART: *hi = 7; break;
    case F_ROADREPS: *lo = 1; *hi = 8; break;
    case F_GLABEL: *hi = 7; break;
    case F_GBARS: *lo = 1; *hi = 16; break;
    case F_ROOT: *hi = 11; break;
    case F_MODE: *hi = DK_NMODES - 1; break;
    case F_CHORD: *hi = DK_NCHORD - 1; break;
    case F_BLACK: *hi = 2; break;
    case F_VOICING: *hi = VC_N - 1; break;
    case F_STRUM: *hi = 120; break;
    case F_HARMTRK: *hi = DK_NTRACK - 1; break;
    case F_SLOT: *hi = DK_NPROJ - 1; break;
    case F_TEMPLATE: *hi = 4; break;
    case F_SWING: *hi = 60; break;
    case F_METRO: *hi = 2; break;
    case F_COUNT: *lo = 1; *hi = 2; break;
    case F_CLICK: *hi = METRO_N - 1; break;
    case F_MDIV: *hi = 2; break;
    case F_ECHOTIME: *hi = 3; break;
    case F_RECTRK: *hi = DK_NTRACK - 1; break;
    case F_TSWING: *hi = 60; break;
    default: break;
    }
}

static int field_get(int f)
{
    dk_track_t *k = cur();
    dk_ev_t *e;
    switch (f) {
    case F_TEMPO: return dk_song.tempo;
    case F_MACRO: return k->macro;
    case F_MOOD: return dk_song.mood;
    case F_P0:
    case F_P1:
    case F_P2:
    case F_P3:
    case F_P4:
    case F_P5: return k->p[f - F_P0];
    case F_ATT: return k->env[0];
    case F_DEC: return k->env[1];
    case F_REL: return k->env[2];
    case F_FXTYPE: return k->fx;
    case F_FXAMT: return k->fxamt;
    case F_FXA: return k->fxa;
    case F_FXB: return k->fxb;
    case F_MODTYPE: return k->mod[ui_mod_slot][0];
    case F_MODDEPTH: return k->mod[ui_mod_slot][1];
    case F_MODRATE: return k->mod[ui_mod_slot][2];
    case F_MODTGT: return k->mod[ui_mod_slot][3];
    case F_MODSLOT: return ui_mod_slot;
    case F_LANE: return ui_lane;
    case F_SVEL: return (e = step_ev()) ? e->vel : ui_svel;
    case F_SPROB: return (e = step_ev()) ? e->fl & EV_PROB : 0;
    case F_SRATCH: return (e = step_ev()) ? (e->fl & EV_RATCH) >> 2 : 0;
    case F_STEPPAGE: return ui_step_page;
    case F_CURSOR: return ui_cursor;
    case F_SLEN: return ui_slen;
    case F_CLIPLEN: return clip_len(cur_clip()) / DK_TPB;
    case F_QNT: return k->qnt;
    case F_TSWING: return k->swing;
    case F_DIV: return k->div & 3;
    case F_ARPMODE: return k->arp[0] % 6;
    case F_ARPDIV: return k->arp[1] % DK_ARP_NDIV;
    case F_ARPRHY: return k->arp_rhy % ARPR_N;
    case F_ARPGATE: {
        int i;
        for (i = 0; i < ARPG_N; i++)
            if (ARPG_ORDER[i] == (k->arp_opt & 3))
                return i;
        return 1;
    }
    case F_ARPSRC: return (k->arp_opt >> 2) & 1;
    case F_ARPON: return dk_arp_on(k);
    case F_ARPOCT: return k->arp[2] ? k->arp[2] : 1;
    case F_ARPLATCH:
    case F_FLATCH: return k->arp[3] ? 1 : 0;
    case F_ENERGY: return k->drv[0];
    case F_STYLE: return k->drv[1];
    case F_VAR: return k->drv[2];
    case F_SPIN: return k->tmb[0];
    case F_GRAV: return k->tmb[1];
    case F_BOUNCE: return k->tmb[2];
    case F_TQUANT: return k->tmb[3] % 3;
    case F_SEQTYPE: return seq_index(k->seq);
    case F_OCT: return k->oct;
    case F_DRIFT: return k->drift;
    case F_MIDICH: return k->midi_ch ? k->midi_ch : 1;
    case F_ROLE: return k->role;
    case F_VOL: return k->vol;
    case F_PAN: return k->pan;
    case F_REV: return k->rev;
    case F_ECHO: return k->echo;
    case F_SIDECHAIN: return dk_song.sidechain;
    case F_SPACE: return dk_song.space & 3;
    case F_DUST: return dk_song.dust;
    case F_SLEEP: return dk_song.sleep;
    case F_ROADCUR: return ui_road_cur;
    case F_ROADPART: return dk_song.nroad ? dk_song.road[ui_road_cur].part : 0;
    case F_ROADREPS: return dk_song.nroad ? dk_song.road[ui_road_cur].reps : 1;
    case F_GPART: return ui_gpart;
    case F_GLABEL: return dk_song.part[ui_gpart].label & 7;
    case F_GBARS: return dk_song.part[ui_gpart].bars;
    case F_PLAYMODE: return dk_song.play_mode ? 1 : 0;
    case F_ROOT: return dk_song.root % 12;
    case F_MODE: return dk_song.mode;
    case F_CHORD: return dk_song.chord;
    case F_WHITE: return dk_song.white ? 1 : 0;
    case F_BLACK: return dk_song.black % 3;
    case F_QUALITY: return dk_song.quality ? 1 : 0;
    case F_VOICING: return dk_song.voicing % VC_N;
    case F_STRUM: return dk_song.strum;
    case F_LEAD: return dk_song.lead ? 1 : 0;
    case F_LAYOUT: return dk_song.layout ? 1 : 0;
    case F_SPLIT: return dk_song.split ? 1 : 0;
    case F_BASS: return dk_song.bass ? 1 : 0;
    case F_KOCT: return (int8_t)dk_song.koct;
    case F_DIM: return dk_song.dim ? 1 : 0;
    case F_HARMTRK: return dk_song.harm_track % DK_NTRACK;
    case F_SLOT: return ui_slot_sel >= 0 ? ui_slot_sel : set.proj;
    case F_TEMPLATE: return ui_tmp_template;
    case F_SWING: return dk_song.swing;
    case F_SONGSTYLE: return dk_song.style;
    case F_METRO: return dk_metro.mode;
    case F_COUNT: return dk_metro.count;
    case F_CLICK: return dk_metro.sound;
    case F_MDIV: return dk_metro.div;
    case F_MVOL: return dk_metro.vol;
    case F_ONNOTE: return dk_metro.on_note;
    case F_MIDIOUT: return dk_midi_notes;
    case F_CLOCK: return dk_midi_clock;
    case F_MIDIIN: return set.midi_in;
    case F_ECHOTIME: return dk_song.echo_time & 3;
    case F_CPU: return (int)plat_cpu_pct();
    case F_RECTRK: return dk_rec_trk;
    default: return 0;
    }
}

static void say(const char *a, const char *b);
static void mark_dirty(void);

static char *cpy(char *d, const char *s);

static void field_set(int f, int v)
{
    dk_track_t *k = cur();
    dk_ev_t *e;
    int lo, hi;
    field_range(f, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (field_get(f) == v && f != F_SLOT && f != F_TEMPLATE)
        return;
    switch (f) {
    case F_TEMPO: dk_song.tempo = (uint8_t)v; break;
    case F_MACRO: k->macro = (int8_t)v; break;
    case F_MOOD:
        song_mood(v);
        dk_cmd(C_RETUNE, 0, 0, 0);
        {                                /* the mood, and the key it brings */
            char kb[24];
            cpy(cpy(cpy(kb, DK_NOTE_NAME[dk_song.root % 12]), " "), DK_MODE_NAME[dk_song.mode % DK_NMODES]);
            say(TAPE.moods[v].name, kb);
        }
        break;
    case F_P0:
    case F_P1:
    case F_P2:
    case F_P3:
    case F_P4:
    case F_P5: k->p[f - F_P0] = (int8_t)v; break;
    case F_ATT: k->env[0] = (int8_t)v; break;
    case F_DEC: k->env[1] = (int8_t)v; break;
    case F_REL: k->env[2] = (int8_t)v; break;
    case F_FXTYPE: k->fx = (uint8_t)v; break;
    case F_FXAMT: k->fxamt = (int8_t)v; break;
    case F_FXA: k->fxa = (int8_t)v; break;
    case F_FXB: k->fxb = (int8_t)v; break;
    case F_MODTYPE: k->mod[ui_mod_slot][0] = (uint8_t)v; break;
    case F_MODDEPTH: k->mod[ui_mod_slot][1] = (uint8_t)v; break;
    case F_MODRATE: k->mod[ui_mod_slot][2] = (uint8_t)v; break;
    case F_MODTGT: k->mod[ui_mod_slot][3] = (uint8_t)v; break;
    case F_MODSLOT: ui_mod_slot = (uint8_t)v; break;
    case F_LANE: ui_lane = (uint8_t)v; break;
    case F_SVEL:
        ui_svel = (uint8_t)v;
        if ((e = step_ev()) != 0)
            e->vel = (uint8_t)v;
        break;
    case F_SPROB:
        if ((e = step_ev()) != 0)
            e->fl = (uint8_t)((e->fl & ~EV_PROB) | v);
        break;
    case F_SRATCH:
        if ((e = step_ev()) != 0)
            e->fl = (uint8_t)((e->fl & ~EV_RATCH) | v << 2);
        break;
    case F_STEPPAGE: ui_step_page = (uint8_t)(v % (clip_len(cur_clip()) / DK_TPB > 0 ? clip_len(cur_clip()) / DK_TPB : 1)); break;
    case F_CURSOR: ui_cursor = (uint8_t)(v % (clip_len(cur_clip()) / DK_T16)); break;
    case F_SLEN: ui_slen = (uint8_t)v; break;
    case F_CLIPLEN:
        undo_push(cur_clip());
        dk_lock();
        dk_song.clip[cur_clip()].len = (uint16_t)(v * DK_TPB);
        dk_unlock();
        break;
    case F_QNT: k->qnt = (uint8_t)v; break;
    case F_TSWING: k->swing = (uint8_t)v; break;
    case F_DIV: k->div = (uint8_t)v; break;
    case F_ARPMODE: k->arp[0] = (uint8_t)v; break;
    case F_ARPDIV: k->arp[1] = (uint8_t)v; break;
    case F_ARPRHY: k->arp_rhy = (uint8_t)v; break;
    case F_ARPGATE: k->arp_opt = (uint8_t)((k->arp_opt & ~3u) | ARPG_ORDER[v % ARPG_N]); break;
    case F_ARPSRC: k->arp_opt = (uint8_t)((k->arp_opt & ~4u) | (v ? 4u : 0u)); break;
    case F_ARPON:
        k->arp_opt = (uint8_t)(v ? k->arp_opt | ARPO_ON : k->arp_opt & ~ARPO_ON);
        if (!v)
            dk_cmd(C_ARPCLR, dk_song.cur_track, 0, 0);
        break;
    case F_ARPOCT: k->arp[2] = (uint8_t)v; break;
    case F_ARPLATCH:
    case F_FLATCH:
        k->arp[3] = (uint8_t)v;
        if (!v)
            dk_cmd(C_ARPCLR, dk_song.cur_track, 0, 0);
        break;
    case F_ENERGY: k->drv[0] = (uint8_t)v; break;
    case F_STYLE: k->drv[1] = (uint8_t)v; break;
    case F_VAR: k->drv[2] = (uint8_t)v; break;
    case F_SPIN: k->tmb[0] = (uint8_t)v; break;
    case F_GRAV: k->tmb[1] = (uint8_t)v; break;
    case F_BOUNCE: k->tmb[2] = (uint8_t)v; break;
    case F_TQUANT: k->tmb[3] = (uint8_t)v; break;
    case F_SEQTYPE:
        k->seq = SQ_LIST[v % NSEQ];
        dk_cmd(C_ARPCLR, dk_song.cur_track, 0, 0);
        dk_cmd(C_HOLDCLR, dk_song.cur_track, 0, 0);
        break;
    case F_OCT: k->oct = (int8_t)v; break;
    case F_DRIFT: k->drift = (uint8_t)v; break;
    case F_MIDICH: k->midi_ch = (uint8_t)v; break;
    case F_ROLE: k->role = (uint8_t)v; break;
    case F_VOL: k->vol = (uint8_t)v; break;
    case F_PAN: k->pan = (int8_t)v; break;
    case F_REV: k->rev = (uint8_t)v; break;
    case F_ECHO: k->echo = (uint8_t)v; break;
    case F_SIDECHAIN: dk_song.sidechain = (uint8_t)v; break;
    case F_SPACE: dk_song.space = (uint8_t)v; break;
    case F_DUST: dk_song.dust = (uint8_t)v; break;
    case F_SLEEP: dk_song.sleep = (uint8_t)v; break;
    case F_ROADCUR: ui_road_cur = (uint8_t)v; break;
    case F_ROADPART:
        if (dk_song.nroad) {
            undo_push_road();
            dk_song.road[ui_road_cur].part = (uint8_t)v;
        }
        break;
    case F_ROADREPS:
        if (dk_song.nroad) {
            undo_push_road();
            dk_song.road[ui_road_cur].reps = (uint8_t)v;
        }
        break;
    case F_GPART: ui_gpart = (uint8_t)v; break;
    case F_GLABEL: dk_song.part[ui_gpart].label = (uint8_t)v; break;
    case F_GBARS: dk_song.part[ui_gpart].bars = (uint8_t)v; break;
    case F_PLAYMODE: dk_song.play_mode = (uint8_t)v; break;
    case F_ROOT:
    case F_MODE:
        if (f == F_ROOT)
            dk_song.root = (uint8_t)v;
        else
            dk_song.mode = (uint8_t)v;
        dk_cmd(C_RETUNE, 0, 0, 0);
        {                                /* the key, said plainly: A MINOR */
            char kb[24];
            cpy(cpy(cpy(kb, DK_NOTE_NAME[dk_song.root % 12]), " "), DK_MODE_NAME[dk_song.mode % DK_NMODES]);
            say("KEY", kb);
        }
        break;
    case F_CHORD: dk_song.chord = (uint8_t)v; break;
    case F_WHITE: dk_song.white = (uint8_t)v; break;
    case F_BLACK: dk_song.black = (uint8_t)v; break;
    case F_QUALITY: dk_song.quality = (uint8_t)v; break;
    case F_VOICING: dk_song.voicing = (uint8_t)v; break;
    case F_STRUM: dk_song.strum = (uint8_t)v; break;
    case F_LEAD: dk_song.lead = (uint8_t)v; break;
    case F_LAYOUT: dk_song.layout = (uint8_t)v; break;
    case F_SPLIT: dk_song.split = (uint8_t)v; break;
    case F_BASS: dk_song.bass = (uint8_t)v; break;
    case F_KOCT: dk_song.koct = (uint8_t)(int8_t)v; break;
    case F_DIM: dk_song.dim = (uint8_t)v; break;
    case F_HARMTRK: dk_song.harm_track = (uint8_t)v; break;
    case F_SLOT: ui_slot_sel = v; break;
    case F_TEMPLATE: ui_tmp_template = v; break;
    case F_SWING: dk_song.swing = (uint8_t)v; break;
    case F_SONGSTYLE:
        dk_song.style = (uint8_t)v;
        dk_cmd(C_RETUNE, 0, 0, 0);
        break;
    case F_METRO: dk_metro.mode = (uint8_t)v; settings_save(); break;
    case F_COUNT: dk_metro.count = (uint8_t)v; settings_save(); break;
    case F_CLICK: dk_metro.sound = (uint8_t)v; settings_save(); break;
    case F_MDIV: dk_metro.div = (uint8_t)v; settings_save(); break;
    case F_MVOL: dk_metro.vol = (uint8_t)v; break;
    case F_ONNOTE: dk_metro.on_note = (uint8_t)v; settings_save(); break;
    case F_MIDIOUT: dk_midi_notes = (uint8_t)v; settings_save(); break;
    case F_CLOCK: dk_midi_clock = (uint8_t)v; settings_save(); break;
    case F_MIDIIN: set.midi_in = (uint8_t)v; settings_save(); break;
    case F_ECHOTIME: dk_song.echo_time = (uint8_t)v; break;
    case F_RECTRK: dk_cmd(C_RECTRK, v, 0, 0); break;
    default: return;
    }
    if (f != F_MVOL && f != F_LANE && f != F_CURSOR && f != F_ROADCUR && f != F_GPART && f != F_SLOT &&
        f != F_TEMPLATE && f != F_MODSLOT && f != F_STEPPAGE && f != F_SLEN)
        mark_dirty();
}

static const char *field_label(int f)
{
    switch (f) {
    case F_TEMPO: return "TEMPO";
    case F_MACRO: return "MACRO";
    case F_MOOD: return "MOOD";
    case F_P0:
    case F_P1:
    case F_P2:
    case F_P3:
    case F_P4:
    case F_P5: {
        const dk_engine_t *e = cur_eng();
        return f - F_P0 < e->nparams ? e->params[f - F_P0].name : "-";
    }
    case F_ATT: return "ATTACK";
    case F_DEC: return "DECAY";
    case F_REL: return "RELEASE";
    case F_FXTYPE: return "EFFECT";
    case F_FXAMT: return "AMOUNT";
    case F_FXA: return TAPE.fx[cur()->fx % TAPE.nfx]->pa[0] ? TAPE.fx[cur()->fx % TAPE.nfx]->pa : "-";
    case F_FXB: return TAPE.fx[cur()->fx % TAPE.nfx]->pb[0] ? TAPE.fx[cur()->fx % TAPE.nfx]->pb : "-";
    case F_MODTYPE: return ui_mod_slot ? "MOD 2" : "MOD 1";
    case F_MODDEPTH: return "DEPTH";
    case F_MODRATE: return cur()->mod[ui_mod_slot][0] == MOD_FOLLOW ? "SOURCE" : "RATE";
    case F_MODTGT: return "TARGET";
    case F_MODSLOT: return "SLOT";
    case F_LANE: return "LANE";
    case F_SVEL: return "VEL";
    case F_SPROB: return "PROB";
    case F_SRATCH: return "RATCHET";
    case F_STEPPAGE: return "BAR";
    case F_CURSOR: return "STEP";
    case F_SLEN: return "LENGTH";
    case F_CLIPLEN: return "BARS";
    case F_QNT: return "QUANT";
    case F_TSWING: return "SWING";
    case F_DIV: return "DIVISION";
    case F_ARPMODE: return "MODE";
    case F_ARPDIV: return "RATE";
    case F_ARPRHY: return "RHYTHM";
    case F_ARPGATE: return "GATE";
    case F_ARPSRC: return "PLAYS";
    case F_ARPON: return "ARP";
    case F_ARPOCT: return "OCTAVES";
    case F_ARPLATCH:
    case F_FLATCH: return "LATCH";
    case F_ENERGY: return "ENERGY";
    case F_STYLE: return "STYLE";
    case F_VAR: return "VARIATION";
    case F_SPIN: return "SPIN";
    case F_GRAV: return "GRAVITY";
    case F_BOUNCE: return "BOUNCE";
    case F_TQUANT: return "QUANT";
    case F_SEQTYPE: return "SEQUENCER";
    case F_OCT: return "OCTAVE";
    case F_DRIFT: return "DRIFT";
    case F_MIDICH: return "MIDI CH";
    case F_ROLE: return "ROLE";
    case F_VOL: return "VOL";
    case F_PAN: return "PAN";
    case F_REV: return "REVERB";
    case F_ECHO: return "ECHO";
    case F_SIDECHAIN: return "SIDECHAIN";
    case F_SPACE: return "SPACE";
    case F_DUST: return "DUST";
    case F_SLEEP: return "SLEEP";
    case F_ROADCUR: return "BLOCK";
    case F_ROADPART: return "PART";
    case F_ROADREPS: return "REPEATS";
    case F_GPART: return "PART";
    case F_GLABEL: return "LABEL";
    case F_GBARS: return "BARS";
    case F_PLAYMODE: return "PLAY";
    case F_ROOT: return "ROOT";
    case F_MODE: return "MODE";
    case F_CHORD: return "CHORD";
    case F_WHITE: return "WHITE";
    case F_BLACK: return "BLACK";
    case F_QUALITY: return "QUALITY";
    case F_VOICING: return "VOICING";
    case F_STRUM: return "STRUM";
    case F_LEAD: return "LEAD";
    case F_LAYOUT: return "LAYOUT";
    case F_SPLIT: return "SPLIT";
    case F_BASS: return "BASS";
    case F_KOCT: return "OCT";
    case F_DIM: return "DIM SKIP";
    case F_HARMTRK: return "HARMONY";
    case F_SLOT: return "PROJECT";
    case F_TEMPLATE: return "TEMPLATE";
    case F_SWING: return "SWING";
    case F_SONGSTYLE: return "STYLE";
    case F_METRO: return "METRO";
    case F_COUNT: return "COUNT IN";
    case F_CLICK: return "CLICK";
    case F_MDIV: return "M DIV";
    case F_MVOL: return "M VOL";
    case F_ONNOTE: return "ON NOTE";
    case F_MIDIOUT: return "MIDI OUT";
    case F_CLOCK: return "CLOCK";
    case F_MIDIIN: return "MIDI IN";
    case F_ECHOTIME: return "ECHO TIME";
    case F_CPU: return "CPU";
    case F_VERSION: return "VERSION";
    case F_RECTRK: return "TRACK";
    default: return "";
    }
}

static char *itoa_b(int v, char *b)
{
    char t[12];
    int n = 0, neg = v < 0;
    unsigned u = (unsigned)(neg ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}
static char *cpy(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
/* a track's name: its role's (the tape's layout: DRUMS BASS PIANO KEYS BELLS NATURE) */
static const char *track_label(int t)
{
    static const char *const RN[R_N] = {"DRUMS", "BASS", "PIANO", "KEYS", "PAD", "LEAD", "KEYS", "NATURE"};
    return RN[dk_song.trk[t % DK_NTRACK].role % R_N];
}

static void field_text(int f, char *b)
{
    int v = field_get(f);
    b[0] = 0;
    switch (f) {
    case F_MOOD: cpy(b, TAPE.moods[v % TAPE.nmood].name); break;
    case F_P0:
    case F_P1:
    case F_P2:
    case F_P3:
    case F_P4:
    case F_P5: {
        const dk_engine_t *e = cur_eng();
        int i = f - F_P0;
        if (i < e->nparams && e->params[i].names)
            cpy(b, e->params[i].names[(unsigned)v <= (unsigned)e->params[i].hi ? v : 0]);
        else
            itoa_b(v, b);
        break;
    }
    case F_FXTYPE: cpy(b, TAPE.fx[v % TAPE.nfx]->name); break;
    case F_MODTYPE: cpy(b, MOD_NAME[v % MOD_N]); break;
    case F_MODRATE:
        if (cur()->mod[ui_mod_slot][0] == MOD_FOLLOW)
            cpy(b, track_label(v % DK_NTRACK));
        else
            itoa_b(v, b);
        break;
    case F_MODTGT: cpy(b, target_label(v)); break;
    case F_MODSLOT: itoa_b(v + 1, b); break;
    case F_LANE: cpy(b, cur_eng()->nlanes ? cur_eng()->lane_name[v % cur_eng()->nlanes] : "-"); break;
    case F_SVEL: cpy(b, v > 110 ? "ACCENT" : v < 60 ? "SOFT" : "NORMAL"); break;
    case F_SPROB: itoa_b(100 - 25 * v, b); break;
    case F_SRATCH: b[0] = 'X'; itoa_b(v + 1, b + 1); break;
    case F_STEPPAGE: itoa_b(v + 1, b); break;
    case F_CURSOR: itoa_b(v + 1, b); break;
    case F_DIV: cpy(b, EDIV_NAME[v & 3]); break;
    case F_ARPMODE: cpy(b, ARP_NAME[v % 6]); break;
    case F_ARPDIV: cpy(b, DIV_NAME[v % DK_ARP_NDIV]); break;
    case F_ARPRHY: cpy(b, ARPR_NAME[v % ARPR_N]); break;
    case F_ARPGATE: cpy(b, ARPG_NAME[v % ARPG_N]); break;
    case F_ARPSRC: cpy(b, ARPSRC_NAME[v & 1]); break;
    case F_ARPON: cpy(b, v ? "ON" : "OFF"); break;
    case F_STYLE:
    case F_SONGSTYLE: cpy(b, TAPE.style_name[v % TAPE.nstyle]); break;
    case F_TQUANT: cpy(b, QUANT3[v % 3]); break;
    case F_SEQTYPE: cpy(b, SEQ_NAME[SQ_LIST[v % NSEQ]]); break;
    case F_ROLE: cpy(b, ROLE_NAME[v % R_N]); break;
    case F_OCT:
    case F_KOCT:
        if (v > 0)
            *b++ = '+';
        itoa_b(v, b);
        break;
    case F_PAN:
        if (!v)
            cpy(b, "C");
        else {
            *b++ = v < 0 ? 'L' : 'R';
            itoa_b(v < 0 ? -v : v, b);
        }
        break;
    case F_SPACE: cpy(b, SPACE_NAME[v & 3]); break;
    case F_ROADCUR: itoa_b(v + 1, b); break;
    case F_ROADPART:
    case F_GPART: b[0] = (char)('A' + v); b[1] = 0; break;
    case F_ROADREPS: b[0] = 'X'; itoa_b(v, b + 1); break;
    case F_GLABEL: cpy(b, LABEL_NAME[v & 7]); break;
    case F_PLAYMODE: cpy(b, PMODE_NAME[v & 1]); break;
    case F_ROOT: cpy(b, DK_NOTE_NAME[v % 12]); break;
    case F_MODE: cpy(b, DK_MODE_NAME[v % DK_NMODES]); break;
    case F_CHORD: cpy(b, DK_CHORD_NAME[v % DK_NCHORD]); break;
    case F_WHITE: cpy(b, WHITE_NAME[v & 1]); break;
    case F_BLACK: cpy(b, BLACK_NAME[v % 3]); break;
    case F_QUALITY: cpy(b, QUAL_NAME[v & 1]); break;
    case F_VOICING: cpy(b, VOICING_NAME[v % VC_N]); break;
    case F_STRUM: itoa_b(v, b); break;
    case F_LAYOUT: cpy(b, LAYOUT_NAME[v & 1]); break;
    case F_HARMTRK:
    case F_RECTRK: itoa_b(v + 1, b); break;
    case F_SLOT: {
        char nm[12];
        b = itoa_b(v + 1, b);
        if (project_used(v)) {
            *b++ = ' ';
            cpy(b, project_name(v, nm));
        } else
            cpy(b, " NEW");
        break;
    }
    case F_TEMPLATE: cpy(b, TEMPLATE_NAME[v % 5]); break;
    case F_METRO: cpy(b, METRO_MODE[v % 3]); break;
    case F_CLICK: cpy(b, METRO_SND[v % METRO_N]); break;
    case F_MDIV: cpy(b, MDIV_NAME[v % 3]); break;
    case F_ECHOTIME: cpy(b, ECHO_T[v & 3]); break;
    case F_CPU: b = itoa_b(v, b); cpy(b, "%"); break;
    case F_VERSION: cpy(b, DK_VERSION); break;
    case F_ARPLATCH:
    case F_FLATCH:
    case F_LEAD:
    case F_SPLIT:
    case F_BASS:
    case F_DIM:
    case F_ONNOTE:
    case F_MIDIOUT:
    case F_CLOCK:
    case F_MIDIIN: cpy(b, ONOFF[v & 1]); break;
    default: itoa_b(v, b); break;
    }
}
