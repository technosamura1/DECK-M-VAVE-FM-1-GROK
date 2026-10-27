/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The pages, drawn after OP-1's quiet manner (DECK_SPEC 9.1): black, thin lines, much air; a small grey header, one
 * big thing, three coloured values at the foot (a dot or a ring in the encoder's colour, its name, its value), a hint.
 * The PLAY page is the tape's art (the garden), the REC page a cassette (9.1a). Drawn in bands (the canvas holds 240 x
 * 80): a page's every frame draws one band, the PLAY page's garden every other frame. Every text keeps to its place:
 * a smaller font, then shortened, then cut (counted: the audit). Part of the unity build (ui.c). */

#define BAND_H 80
static int32_t band_y0;                  /* the band being drawn: screen rows band_y0 .. +80 */

static void band_begin(int y0, int h)
{
    band_y0 = y0;
    cv_begin(240, (uint32_t)h, TH_BG);
    cv_oy = -y0;                         /* draw in screen coordinates; only this band's rows land */
    cv_y0 = 0;
    cv_y1 = h;
}
static void band_end(void)
{
    cv_oy = 0;
    cv_blit(0, (uint32_t)band_y0);
    ui_mid_poll();
}

/* --------------------------------------------------------------- drawing --- */
static void hline(int x, int y, int w, uint16_t c) { cv_rect(x, y, w, 1, c); }
static void vline(int x, int y, int h, uint16_t c) { cv_rect(x, y, 1, h, c); }
static void circle(int cx, int cy, int r, uint16_t c)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        cv_pset(cx + x, cy + y, c);
        cv_pset(cx + y, cy + x, c);
        cv_pset(cx - y, cy + x, c);
        cv_pset(cx - x, cy + y, c);
        cv_pset(cx - x, cy - y, c);
        cv_pset(cx - y, cy - x, c);
        cv_pset(cx + y, cy - x, c);
        cv_pset(cx + x, cy - y, c);
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}
static void disc(int cx, int cy, int r, uint16_t c)
{
    int y;
    for (y = -r; y <= r; y++) {
        int w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r + r)
            w++;
        cv_rect(cx - w, cy + y, 2 * w + 1, 1, c);
    }
}
/* a ring: grey, its first frac (from the top, clockwise) in c */
static void ring(int cx, int cy, int r, float frac, uint16_t c)
{
    int i, n = 64;
    for (i = 0; i < n; i++) {
        float a = 6.2831853f * ((float)i / (float)n) - 1.5707963f;
        int x = cx + (int)((float)r * fm_cosf(a) + 0.5f), y = cy + (int)((float)r * fm_sinf(a) + 0.5f);
        uint16_t col = (float)i < frac * (float)n ? c : TH_LINE;
        cv_rect(x, y, 2, 2, col);
    }
}
static uint16_t dimc(uint16_t c, int k16)   /* a colour darker: k16/16 of it */
{
    int r = (c >> 11) * k16 / 16, g = ((c >> 5) & 63) * k16 / 16, b = (c & 31) * k16 / 16;
    return (uint16_t)(r << 11 | g << 5 | b);
}

/* the track's mute in the part now (the one playing, or the one chosen): the part's own (MIX, white key tap) */
static int part_muted(int t) { return dk_song.part[(dk_playing ? dk_cur_part : dk_song.cur_part) & 7].mute >> t & 1; }
/* ... and a change waiting for the bar's end: the state it will be in (else -1) */
static int part_mute_pending(int t)
{
    int p = (dk_playing ? dk_cur_part : dk_song.cur_part) & 7;
    return dk_pm_x && dk_pm_part == p && (dk_pm_x >> t & 1) ? !part_muted(t) : -1;
}

/* ------------------------------------------------------------------ texts --- */
static const felucca_font_t *const FONTS[5] = {&FONT_L, &FONT_M, &FONT_B, &FONT_S, &FONT_XS};
static int font_rank(const felucca_font_t *f)
{
    int i;
    for (i = 0; i < 5; i++)
        if (FONTS[i] == f)
            return i;
    return 4;
}
/* s shortened to fit w in f: its vowels go (from the end, the first letter kept), then it is cut */
static int shorten(const felucca_font_t *f, const char *s, char *out, int w)
{
    int n = 0, i, cut = 0;
    for (i = 0; s[i] && n < 30; i++)
        out[n++] = s[i];
    out[n] = 0;
    for (i = n - 1; i > 0 && text_w(f, out) > w; i--) {
        char c = out[i];
        if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
            int j;
            for (j = i; out[j]; j++)
                out[j] = out[j + 1];
        }
    }
    while (out[0] && text_w(f, out) > w) {
        int e = 0;
        while (out[e])
            e++;
        out[e - 1] = 0;
        cut = 1;
    }
    return cut;
}
/* a text in a box (x, w; align 0 left 1 centre 2 right), the biggest font from pref down to min that fits */
static void text_box(int x, int y, int w, int align, const felucca_font_t *pref, const felucca_font_t *min,
                     const char *s, uint16_t c)
{
    const felucca_font_t *f = pref;
    char b[32];
    int i, tw, r0 = font_rank(pref), r1 = font_rank(min);
    const char *t = s;
    for (i = r0; i <= r1; i++) {
        f = FONTS[i];
        if (text_w(f, s) <= w)
            break;
    }
    if (text_w(f, s) > w) {
        if (shorten(f, s, b, w))
            ui.fit_over++;
        t = b;
    }
    tw = text_w(f, t);
    cv_text(align == 1 ? x + (w - tw) / 2 : align == 2 ? x + w - tw : x, y + (pref->h - f->h) / 2, f, t, c);
}
/* a preset's label: the BLOCK green, then "- NAME" white ("SYNTHWAVE - 909 TECHNO"), in one box (x, w; align 0/1/2), the biggest font from
 * pref down to min that fits; too long still: the NAME is cut (its vowels first), never the block */
static void tag_box(int x, int y, int w, int align, const felucca_font_t *pref, const felucca_font_t *min, const char *blk, const char *nm,
                    uint16_t cb, uint16_t cn)
{
    const felucca_font_t *f = pref;
    char all[72], tail[40], cut[40];
    int i, r0 = font_rank(pref), r1 = font_rank(min), bw, tw;
    const char *t = tail;
    if (!blk[0]) {
        text_box(x, y, w, align, pref, min, nm, cn);
        return;
    }
    cpy(cpy(tail, "- "), nm);
    cpy(cpy(cpy(all, blk), " "), tail);
    for (i = r0; i <= r1; i++) {
        f = FONTS[i];
        if (text_w(f, all) <= w)
            break;
    }
    cpy(cpy(all, blk), " ");
    bw = text_w(f, all);                    /* the block and the space after it */
    if (bw + text_w(f, tail) > w) {
        if (shorten(f, tail, cut, w - bw))
            ui.fit_over++;
        t = cut;
    }
    tw = bw + text_w(f, t);
    x = align == 1 ? x + (w - tw) / 2 : align == 2 ? x + w - tw : x;
    y += (pref->h - f->h) / 2;
    cv_text(x, y, f, blk, cb);
    cv_text(x + bw, y, f, t, cn);
}
/* FONT_L at twice its size (the KEYS page's chord): each pixel 2 x 2 */
static int text2x_w(const char *s) { return 2 * text_w(&FONT_L, s); }
static void text2x(int x, int y, const char *s, uint16_t c)
{
    const felucca_font_t *f = &FONT_L;
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd_ = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t a = gd_[gy * bpr + gx / 2u];
                a = (gx & 1u) ? (a & 15u) : (a >> 4);
                if (a > 7u)
                    cv_rect(x + 2 * ((int32_t)gx - f->pad), y + 2 * (int32_t)gy, 2, 2, c);
            }
        x += 2 * f->adv[gi];
    }
}

/* ---------------------------------------------------------------- the frame --- */
static int seq_edit_row(void);
static int page_rows(int p)
{
    static const uint8_t ROWS[NPG] = {1, 2, 5, 1, 1, 4, 1, 1, 2, 7, 2, 1};   /* (GLOBAL: its 5, then ATMO's 2) */
    if (p == PG_SEQ && ((cur_eng()->flags & EF_DRUM) || cur()->seq == SQ_STEP || cur()->seq == SQ_LIVE))
        return 3;                            /* drums: the sequencer, its sounds (KIT), the sequencer's settings;
                                              * STEP and LIVE: their own, the settings, the step editor */
    return ROWS[p];
}
/* the knobs of the page (its row): K1..K4 (i 0..3) the four main ones, i 4 ALGO's extra. SELECT is the track and
 * PRESET the preset everywhere but PLAY (TEMPO, MACRO, MOOD there) */
static int page_field(int i, int home)
{
    const dk_track_t *k = cur();
    const dk_engine_t *e = cur_eng();
    int row = ui.row[ui.page] % page_rows(ui.page);
    /* KEYS: the key and notes/chords first (K1 ROOT, K2 MODE, K3 WHITE = NOTES/CHORDS, K4 the chord) */
    static const uint8_t KEYS_F[4][5] = {{F_ROOT, F_MODE, F_WHITE, F_CHORD, F_VOICING},
                                         {F_BLACK, F_QUALITY, F_VOICING, F_STRUM, F_LEAD},
                                         {F_SPLIT, F_BASS, F_HARMTRK, F_KOCT, F_DIM},
                                         {F_LAYOUT, F_LEAD, F_DIM, F_NONE, F_NONE}};
    static const uint8_t PROJ_F[1][5] = {{F_SLOT, F_TEMPLATE, F_SONGSTYLE, F_NONE, F_NONE}};
    /* GLOBAL (GLO): the song's time and air, the metronome, MIDI, the rest */
    static const uint8_t GLOB_F[5][5] = {{F_TEMPO, F_SWING, F_DUST, F_SLEEP, F_SIDECHAIN},
                                         {F_METRO, F_COUNT, F_CLICK, F_MVOL, F_MDIV},
                                         {F_MIDIOUT, F_CLOCK, F_MIDIIN, F_ONNOTE, F_NEWPART},
                                         {F_RECLAT, F_ECHOTIME, F_CPU, F_VERSION, F_LOCKTAKE},
                                         {F_RECMODE, F_RECARM, F_NONE, F_NONE, F_NONE}};
    (void)home;
    switch (ui.page) {
    case PG_PLAY: {                          /* the garden: the track you play, its sound and its space; ALGO the mood */
        static const uint8_t F[5] = {F_MACRO, F_REV, F_ECHO, F_VOL, F_NONE};   /* (MOOD gone: the sound stays clean) */
        return F[i];
    }
    case PG_TRACKS: {                        /* the selected track's level and pan */
        static const uint8_t F[5] = {F_VOL, F_PAN, F_RECMODE, F_RECARM, F_NONE};
        return F[i];
    }
    case PG_ARP: {                           /* the arpeggiator's own page */
        static const uint8_t F[2][5] = {{F_ARPMODE, F_ARPDIV, F_ARPGATE, F_ARPON, F_ARPRHY},   /* K1-K4: MODE RATE GATE ARP (on/off: the page switches it) */
                                        {F_ARPOCT, F_ARPLATCH, F_ARPSRC, F_NONE, F_NONE}};
        return F[(ui.page == PG_GLOB ? row - 5 : row) & 1][i];
    }
    case PG_SOUND: {
        int fav[4], more[5], drum = (e->flags & EF_DRUM) != 0;
        switch (row % 5) {
        case 0:                              /* FAVORITES: the preset's four that matter (what its MACRO moves first); the fifth, MACRO */
            sound_lists(fav, more);
            return i < 4 ? fav[i] : F_MACRO;
        case 1:                              /* MORE: the rest of the sound's own */
            sound_lists(fav, more);
            return more[i];
        case 2: {                            /* FX: always TONE, DEPTH (FILTER: RES, CUTOFF), SPACE (reverb), SEND (echo); on a drum track: the kit's, or one sound's */
            static const uint8_t F[5] = {F_FXA, F_FXAMT, F_REV, F_ECHO, F_NONE};
            static const uint8_t ONE[5] = {F_LCRUSH, F_LLEVEL, F_LREV, F_LECHO, F_FXFOR};
            if (drum)
                return ui_fx_for ? ONE[i] : i == 4 ? F_FXFOR : F[i];
            return F[i];
        }
        case 3: {                            /* FX TYPE: the effect, its second knob (a drum sound: its pan, tune, decay) */
            static const uint8_t F[5] = {F_FXTYPE, F_FXB, F_NONE, F_NONE, F_NONE};
            static const uint8_t ONE[5] = {F_FXFOR, F_LPAN, F_LTUNE, F_LDECAY, F_NONE};
            if (drum)
                return ui_fx_for ? ONE[i] : i == 2 ? F_FXFOR : F[i];
            return F[i];
        }
        default: {
            static const uint8_t F[5] = {F_MODTYPE, F_MODDEPTH, F_MODRATE, F_MODTGT, F_MODSLOT};
            return F[i];
        }
        }
    }
    case PG_SEQ: {
        int drum = (e->flags & EF_DRUM) != 0;
        if (drum && row == 1) {              /* the kit's sounds, each its own: kick quieter, snare up */
            static const uint8_t F[5] = {F_LANE, F_LLEVEL, F_LTUNE, F_LDECAY, F_LPAN};
            return F[i];
        }
        if (row == (drum ? 2 : 1)) {
            static const uint8_t F[5] = {F_SEQTYPE, F_CLIPLEN, F_TSWING, F_QNT, F_NONE};   /* every sequencer's: here only */
            return F[i];
        }
        if (!drum && row == 2) {             /* the step editor: walk, a note in/out, longer/cut, its pitch */
            static const uint8_t F[5] = {F_CURSOR, F_SENOTE, F_SETIE, F_SEPITCH, F_SVEL};
            return F[i];
        }
        switch (k->seq) {
        case SQ_STEP:
            if (drum) {
                static const uint8_t F[5] = {F_LANE, F_SVEL, F_SPROB, F_SRATCH, F_STEPPAGE};
                return F[i];
            } else {                         /* a melody: the clip a step on, a scale step up, a new note's length, its force */
                static const uint8_t F[5] = {F_SHIFT, F_MOVE, F_SLEN, F_SVEL, F_SPROB};
                return F[i];
            }
        case SQ_ENDLESS: {                   /* how fast a step, which way, how long a note; the track's level */
            static const uint8_t F[5] = {F_ERATE, F_EDIR, F_EGATE, F_VOL, F_NONE};
            return F[i];
        }
        default: {
            static const uint8_t F[5] = {F_SHIFT, F_MOVE, F_VOL, F_REV, F_NONE};   /* the clip a step on, a scale step up */
            return F[i];
        }
        }
    }
    case PG_MIX: {
        static const uint8_t F[5] = {F_VOL, F_PAN, F_REV, F_ECHO, F_SPACE};
        return F[i];
    }
    case PG_ROAD:
        if (ui.garage) {
            static const uint8_t F[5] = {F_GPART, F_GLABEL, F_GBARS, F_NONE, F_NONE};
            return F[i];
        } else {
            static const uint8_t F[5] = {F_ROADCUR, F_ROADPART, F_ROADREPS, F_PLAYMODE, F_NONE};
            return F[i];
        }
    case PG_KEYS: return KEYS_F[row & 3][i];
    case PG_PROJECT: return PROJ_F[0][i];
    case PG_GLOB:
        if (row < 5)
            return GLOB_F[row][i];
        /* fall through */ /* GLOBAL's rows 6, 7 are ATMO's */
    case PG_ATMO: {                          /* ATMO (SEL): the place's layers, then the noise and the tape */
        static const uint8_t F[2][5] = {{F_ATMO0, F_ATMO1, F_ATMO2, F_ATMO3, F_ALVL},
                                        {F_ATMO4, F_ATMO5, F_DUST, F_ATAPE, F_ALVL}};
        return F[row & 1][i];
    }
    case PG_REC: {
        static const uint8_t F[5] = {F_RECBARS, F_QNT, F_RECMODE, F_METRO, F_ONNOTE};   /* BARS first: how long a take; ALGO how it starts */
        return F[i];
    }
    default: return F_NONE;
    }
}

static const char *hint(void)
{
    const dk_track_t *k = cur();
    switch (ui.page) {
    case PG_PLAY: return "HOME+WHITE PRESET  HOME+BLACK TRACK";
    case PG_SEQ:
        if (seq_edit_row())
            return "K2 NOTE +/-  K3 TIE/CUT  K4 PITCH";
        if (k->seq == SQ_STEP)
            return cur_eng()->flags & EF_DRUM ? "WHITE STEPS  BLACK LANES  SEQ ROW" : "SEQ AGAIN x2 = STEP EDIT";
        if (k->seq == SQ_ENDLESS)
            return ui.endl_on ? "OCT- REST  OCT+ TIE  REC = DONE" : "REC = PLAY IN STEPS  KEY = TRANSPOSE";
        return "HOME x2, EDIT = COOK RANDOM QUANTIZE";
    case PG_SOUND:
        if (plat_ms() / 3000u % 3u == 1u)
            return "HOLD REC + KNOB = AUTOMATION";
        if (plat_ms() / 3000u % 3u == 2u && auto_count(cur_clip(), 0))
            return "HOME x2, EDIT = ERASE AUTOMATION";
        return "FX = ROW  HOLD FX+WHITE 1-4 = BYPASS";
    case PG_MIX: return "WHITE TRACK  BLACK MUTE  B7 SOLO  B8 ARM";
    case PG_ROAD:
        return ui.garage ? "W1-8 = COPY PART TO  HOLD ENV = BACK" : "W1-8 ADD  W9 COPY  BLACK TRICKS  REC = LIVE";
    case PG_KEYS: return "EDIT = NEXT ROW  HOLD EDIT = UNDO";
    case PG_PROJECT: return "W1-4 OPEN  W5 NEW  B1 EXPORT B2 STEMS B3 SAVE B4 CLEAR ALL";
    case PG_GLOB: return ui.row[PG_GLOB] % 7 >= 5 ? "ATMO  GLO = NEXT ROW" : "GLO = NEXT ROW  HOLD GLO = FILL";
    case PG_ATMO: return "SEL = NEXT ROW  KNOBS = HOW MUCH";
    case PG_REC:
        if (ui_auto_on)
            return "REC AGAIN = THE LOOP ENDS HERE";
        return "K1 BARS  K4 METRONOME  HOLD EDIT = UNDO";
    case PG_TRACKS:
        if (ui.lay)
            return "HOLD EDIT = UNDO  HOME = BACK";
        return "EDIT = EDIT LIST  K3 MODE  HOME = BACK";
    case PG_ARP: return "K4 ON/OFF  ARP=ROW  HOME+ARP=OFF";
    default: return "";
    }
}

/* the battery, 0..100 % (-1: no reading yet), from the divider's reading smoothed. The ends are a guess until
 * measured on the device: BAT_LO a nearly flat battery, BAT_HI a full one (the PROJECT page shows the raw reading) */
#define BAT_LO 520
#define BAT_HI 640
static int batt_pct(void)
{
    static int32_t avg = -1;
    int32_t r = plat_battery();
    if (r < 0)
        return -1;
    avg = avg < 0 ? r * 16 : avg + r - avg / 16;
    r = avg / 16;
    r = (r - BAT_LO) * 100 / (BAT_HI - BAT_LO);
    return r < 0 ? 0 : r > 100 ? 100 : (int)r;
}

/* the header: the transport, the part, the bar and beat, the page, the tempo, the six tracks */
static void draw_header(void)
{
    char s[24];
    int part = dk_playing ? dk_cur_part : dk_song.cur_part, bar, beat, x, t;
    uint32_t now = plat_ms();
    int mw = dk_song.nev * 10 >= DK_NEV * 9 ? 24 : 0;   /* the event pool (clips and automation) 90 % full: "MEM" at the right */
    if (dk_recording || dk_counting) {
        if ((now / 400u) & 1u)
            disc(12, 10, 4, TH_REC);
        else
            circle(12, 10, 4, TH_REC);
    } else if (dk_playing) {
        int i;
        for (i = 0; i < 9; i++)
            vline(8 + i, 10 - (8 - i) / 2, 8 - i < 1 ? 1 : (8 - i), TH_TEXT);
    } else
        cv_rect(8, 6, 8, 8, TH_GRAY);
    {
        int lp = part & 7;
        if (dk_playing && dk_next_part != 0xFF && (dk_next_part & 7) != lp && (now / 230u & 1u))
            lp = dk_next_part & 7;       /* a part queued (for the bar's end): its letter blinks in the part's place */
        cv_rect(22, 3, 13, 15, TH_PART[lp]);
        s[0] = (char)('A' + lp);
    }
    s[1] = 0;
    cv_text(25, 4, &FONT_XS, s, TH_BG);
    if (ui.dirty)                        /* unsaved changes: a small dot (it goes out by itself with the autosave) */
        cv_rect(3, 9, 2, 2, TH_GRAY);
    bar = dk_playing ? dk_part_tick / DK_TPB + 1 : 1;
    beat = dk_playing ? dk_part_tick % DK_TPB / DK_PPQ + 1 : 1;
    x = (int)(itoa_b(bar, s) - s);
    s[x] = '.';
    itoa_b(beat, s + x + 1);
    cv_text(40, 4, &FONT_XS, s, TH_GRAY);
    if (plat_ms() < ui.notes_until)      /* what you play, for a moment: its notes (or the drum) */
        text_box(70, 4, 92 - mw, 0, &FONT_XS, &FONT_T, ui.notes, ui.notes_k ? RGB(250, 150, 70) : TH_ENC[1]);
    else if (held(B_HOME)) {             /* HOME held: the load, for a look: the voices of 24, the CPU */
        char ld[24], *q = itoa_b(dk_voices_on, ld);
        q = cpy(q, "/");
        q = itoa_b(DK_NVOICE, q);
        q = cpy(q, mw ? " V " : " V  CPU ");
        q = itoa_b((int)plat_cpu_pct(), q);
        cpy(q, "%");
        text_box(70, 4, 94 - mw, 0, &FONT_XS, &FONT_XS, ld, plat_cpu_pct() > 80u ? TH_REC : TH_ENC[2]);
    } else if (ui.page == PG_PLAY) {       /* PLAY: the part's name and the chord, in the header (not on the meadow) */
        int pcs[4], n = dk_harm_n ? dk_harm_n : 1, i;
        char c[12];
        text_box(70, 4, 50 - mw, 0, &FONT_XS, &FONT_XS, LABEL_NAME[dk_song.part[part & 7].label & 7], TH_TEXT);
        for (i = 0; i < 4; i++)
            pcs[i] = 60 + dk_harm_pcs[i];
        dk_chord_name(pcs, n, c);
        text_box(122 - mw, 4, 42, 1, &FONT_XS, &FONT_T, chord_deg >= 0 || dk_playing ? (have_chord && !dk_playing ? chord_name : c) : "", TH_ENC[0]);
    } else {
        char tl[16];
        int k = (int)(itoa_b(dk_song.cur_track + 1, tl) - tl);
        tl[k] = ' ';
        cpy(tl + k + 1, track_label(dk_song.cur_track));
        text_box(70, 4, 94 - mw, 0, &FONT_XS, &FONT_XS, ui.page == PG_SEQ || ui.page == PG_SOUND || ui.page == PG_REC || ui.page == PG_ARP ? tl : ui.page == PG_TRACKS && ui.lay ? (ui.lay == 2 ? "LAYERS" : "EDIT") : PAGE_NAME[ui.page], TH_GRAY);
    }
    {                                    /* the battery: shown only when low or HOME held (a quiet header) */
        int pct = batt_pct(), w;
        if (pct < 0 || (pct >= 30 && !held(B_HOME)))
            goto no_batt;
        hline(167, 6, 11, TH_GRAY);
        hline(167, 13, 11, TH_GRAY);
        vline(167, 6, 8, TH_GRAY);
        vline(177, 6, 8, TH_GRAY);
        cv_rect(178, 8, 2, 4, TH_GRAY);
        if (pct >= 0) {
            w = (pct * 9 + 50) / 100;
            cv_rect(168, 7, w < 1 ? 1 : w, 6, pct < 15 ? TH_REC : pct < 35 ? TH_ENC[2] : TH_TEXT);
        }
    }
no_batt:
    if (mw)                              /* the song memory nearly full: orange from 90 %, red from 97 % */
        text_box(143, 4, 22, 2, &FONT_XS, &FONT_XS, "MEM", dk_song.nev * 100 >= DK_NEV * 97 ? TH_REC : RGB(250, 150, 70));
    itoa_b(dk_song.tempo, s);
    text_box(182, 4, 24, 2, &FONT_XS, &FONT_XS, s, TH_TEXT);
    for (t = 0; t < DK_NTRACK; t++) {    /* the six tracks: the one you are on bright, a muted one hollow */
        int cx = 211 + (t % 3) * 9, cy = 6 + (t / 3) * 8;
        uint16_t c = t == dk_song.cur_track ? TH_TEXT : dk_level[t] > 0.01f ? TH_TRACK[t] : TH_GRAY;
        if (part_muted(t))
            circle(cx, cy, 2, TH_GRAY);
        else
            disc(cx, cy, 2, c);
    }
    if (dk_beat_flash || (dk_playing && dk_metro.mode && dk_part_tick % DK_PPQ < DK_PPQ / 6))
        cv_rect(200, 9, 2, 2, TH_ENC[2]);
}

/* a button held a moment (and not used for anything yet): what it does, in one line where the page's hint is. HOME (its
 * own hint and the battery) and SEQ (a tap on the press) have none here */
#define HOLD_HINT_MS 450u
static const char *hold_hint(uint32_t now)
{
    static const char *const H[NB] = {
        "FX: SOUND  +W1-4 BYPASS", "SEL: TAPE  AGAIN = TRACKS",
        "ENV: ROAD  REC ON: NEXT PART", "LFO: MIX  +SOUND KNOB = MOD 1",
        "EDIT: HOLD UNDO  HOME+ REDO", "GLO: GLOBAL  HOLD = FILL",
        0, "SAVE: +W1 SNAPSHOT  +W2 RECALL",
        "ARP: PAGE  K4 ON/OFF  +WHITE PART", 0,
        "PLAY/STOP  HOLD PART/SONG", "REC +OCT UNDO  +W1-6 LAYER",
        "OCT-: OCTAVE  HOME+ SCALE STEP", "OCT+: OCTAVE  HOME+ SCALE STEP"};
    int b, best = -1;
    for (b = 0; b < NB; b++)
        if (H[b] && held(b) && !ui.used[b] && !ui.held_done[b] && now - ui.down_t[b] >= HOLD_HINT_MS && !held(B_HOME) &&
            (best < 0 || ui.down_t[b] > ui.down_t[best]))
            best = b;
    return best < 0 ? 0 : H[best];
}

/* the foot: the four knobs K1..K4 (a ring for a number, a dot for a choice; their colours), the hint */
static uint16_t knob_col(int i) { return i < 3 ? TH_ENC[i] : RGB(250, 150, 70); }   /* green blue yellow orange */
static void draw_footer(int y)
{
    int i;
    uint32_t now = plat_ms();
    for (i = 0; i < 4 && (ui.page == PG_PLAY || ui.page == PG_REC); i++) {   /* (the modules: their knobs on their plate) */
        int f = page_field(i, 0), x = 4 + i * 59, lo, hi, v, lit = ui.touched == i && now < ui.touch_until;
        char s[24];
        if (f == F_NONE)
            continue;
        field_range(f, &lo, &hi);
        v = field_get(f);
        field_text(f, s);
        if (hi - lo > 8)
            ring(x + 5, y + 6, 4, (float)(v - lo) / (float)(hi - lo), knob_col(i));
        else
            disc(x + 5, y + 6, 3, knob_col(i));
        text_box(x + 13, y + 1, 43, 0, &FONT_T, &FONT_T, field_label(f), lit ? knob_col(i) : TH_GRAY);
        text_box(x, y + 15, 56, 0, &FONT_S, &FONT_XS, s, TH_TEXT);
    }
    {
        const char *hh = hold_hint(now);
        if (hh)
            text_box(4, y + 43, 232, 1, &FONT_T, &FONT_T, hh, TH_TEXT);
        else if (now - ui.page_t < 3000u || held(B_HOME))
            text_box(4, y + 43, 232, 1, &FONT_T, &FONT_T, hint(), TH_GRAY);
    }
}

/* a VHS-plate's message: what just happened, for 0.6 s */
static void draw_toast(void)
{
    int w, x, y = 96;
    if (plat_ms() >= ui.msg_until)
        return;
    if (ui.msg_tag) {                    /* a preset's label: the block green, "- name" white */
        char all[72];
        cpy(cpy(cpy(all, ui.msg[0]), " - "), ui.msg[1]);
        w = text_w(&FONT_B, all) + 24;
    } else
        w = text_w(&FONT_B, ui.msg[0]) + (ui.msg[1][0] ? text_w(&FONT_S, ui.msg[1]) + 12 : 0) + 24;
    if (w > 228)
        w = 228;
    x = (240 - w) / 2;
    cv_rect(x, y, w, 30, TH_BG);
    hline(x, y, w, TH_TEXT);
    hline(x, y + 29, w, TH_TEXT);
    vline(x, y, 30, TH_TEXT);
    vline(x + w - 1, y, 30, TH_TEXT);
    if (ui.msg_tag)
        tag_box(x + 12, y + 7, w - 24, 1, &FONT_B, &FONT_XS, ui.msg[0], ui.msg[1], TH_ENC[0], TH_TEXT);
    else if (ui.msg[1][0]) {
        int w0 = text_w(&FONT_B, ui.msg[0]);
        text_box(x + 12, y + 7, w0 < w - 24 ? w0 : w - 24, 0, &FONT_B, &FONT_XS, ui.msg[0], TH_TEXT);
        text_box(x + 12 + w0 + 12, y + 7, w - 36 - w0 > 0 ? w - 36 - w0 : 1, 0, &FONT_S, &FONT_XS, ui.msg[1], TH_ENC[0]);
    } else
        text_box(x + 12, y + 7, w - 24, 1, &FONT_B, &FONT_XS, ui.msg[0], TH_TEXT);
}

/* ------------------------------------------------------------------- SOUND --- */
/* the module's own greys: the theme's are for black, on the window's brown they vanish */
#define W_BG RGB(0, 0, 0)
#define W_LINE RGB(77, 77, 77)
#define W_GRAY RGB(147, 147, 147)
#define W_INK RGB(10, 10, 10)
static void viz(int kind, int x0, int y0, int w, int h, float v, uint16_t c, uint32_t now)
{
    int i, cx = x0 + w / 2, cy = y0 + h / 2;
    float t = (float)now * 0.001f;
    switch (kind) {
    case VZ_SUN: {                       /* a sun behind the horizon: the brighter the higher */
        int r = 8 + (int)(v * 16.0f), hy = y0 + h - 14, y;
        for (y = -r; y <= 0; y++) {
            int ww = 0;
            while ((ww + 1) * (ww + 1) + y * y <= r * r)
                ww++;
            if (hy + y + (int)((1.0f - v) * 12.0f) < hy)
                cv_rect(cx - ww, hy + y + (int)((1.0f - v) * 12.0f), 2 * ww + 1, 1, c);
        }
        hline(x0 + 4, hy, w - 8, TH_TEXT);
        break;
    }
    case VZ_WAVE:
    case VZ_WOBBLE: {                    /* two saws apart (chorus, tremolo, drift): the more, the further */
        int k;
        for (k = 0; k < 2; k++) {
            int px = x0 + 4, py = cy;
            for (i = 0; i <= w - 8; i += 2) {
                float ph = (float)i / 18.0f + (k ? v * 0.5f : 0.0f) + (kind == VZ_WOBBLE ? 0.15f * v * fm_sinf(t * 2.0f + (float)i * 0.1f) : 0.0f);
                int y = cy - 10 + (int)(20.0f * (ph - fm_floorf(ph))) + k * 4;
                if (i)
                    cv_line(px, py, x0 + 4 + i, y, k ? dimc(c, 7) : c);
                px = x0 + 4 + i;
                py = y;
            }
        }
        break;
    }
    case VZ_ENV:
    case VZ_SWELL: {                     /* an envelope: the rise, the fall */
        int ax = x0 + 6 + (int)((kind == VZ_SWELL ? v : 0.1f) * (float)(w / 2)), by = y0 + h - 10;
        int dx = ax + 8 + (int)((kind == VZ_ENV ? v : 0.5f) * (float)(w / 2 - 10));
        cv_line(x0 + 6, by, ax, y0 + 10, c);
        cv_line(ax, y0 + 10, dx > x0 + w - 4 ? x0 + w - 4 : dx, by, c);
        hline(x0 + 4, by, w - 8, W_LINE);
        break;
    }
    case VZ_DUST:                        /* felt, dust, wood: grains */
        for (i = 0; i < (int)(v * 40.0f) + 2; i++) {
            uint32_t hh = (uint32_t)i * 2654435761u;
            cv_rect(x0 + 6 + (int)(hh % (uint32_t)(w - 12)), y0 + 8 + (int)((hh >> 12) % (uint32_t)(h - 16)), 2, 2, c);
        }
        break;
    case VZ_DROP:                        /* amount: drops falling */
        for (i = 0; i < 2 + (int)(v * 10.0f); i++) {
            uint32_t hh = (uint32_t)i * 2246822519u;
            int x = x0 + 6 + (int)(hh % (uint32_t)(w - 12)), y = y0 + 4 + (int)(((hh >> 8) + now / 16u) % (uint32_t)(h - 10));
            vline(x, y, 4, c);
        }
        break;
    case VZ_BELL:                        /* a tine's ping: rings, the more the wider */
        for (i = 0; i < 3; i++)
            circle(cx, cy, 4 + i * (3 + (int)(v * 6.0f)), i ? dimc(c, 12 - 3 * i) : c);
        break;
    case VZ_GLIDE: {
        int y1 = cy + 12, y2 = cy - 12, gx = (int)(v * (float)(w - 30));
        hline(x0 + 6, y1, 10, c);
        cv_line(x0 + 16, y1, x0 + 16 + gx, y2, c);
        hline(x0 + 16 + gx, y2, w - 22 - gx, c);
        break;
    }
    case VZ_TYPE:
        break;                           /* its name says it (the value) */
    default: {                           /* a bar */
        int bw = (int)(v * (float)(w - 12));
        cv_rect(x0 + 6, cy - 2, w - 12, 5, W_LINE);
        cv_rect(x0 + 6, cy - 2, bw, 5, c);
        break;
    }
    }
}

/* the hardware look (the cassette's colours): a plate with its screws, a knob with its value's arc */
static void plate(int x, int y, int w, int h)   /* a manga panel: paper, a heavy black frame, a hairline inside */
{
    cv_rect(x, y, w, h, CS_BODY);
    cv_rect(x, y, w, 2, CS_INK);
    cv_rect(x, y + h - 2, w, 2, CS_INK);
    cv_rect(x, y, 2, h, CS_INK);
    cv_rect(x + w - 2, y, 2, h, CS_INK);
    hline(x + 4, y + h - 5, w - 8, CS_INK);
    vline(x + 4, y + 4, h - 8, CS_INK);
    vline(x + w - 5, y + 4, h - 8, CS_INK);
}
static void reel(int cx, int cy, float amount, int phase)
{
    int i, r = 8 + (int)(amount * 6.0f + 0.5f);   /* the tape wound on it: kept inside the window (r <= 14) */
    disc(cx, cy, 15, RGB(51, 51, 51));       /* the reel itself, the same size full or empty */
    circle(cx, cy, 15, RGB(77, 77, 77));
    disc(cx, cy, r, CS_TAPE);
    disc(cx, cy, 7, CS_LABEL);
    circle(cx, cy, 7, CS_BODY_D);
    disc(cx, cy, 3, CS_WINDOW);
    for (i = 0; i < 6; i++) {                /* the hub's teeth: eight phases a turn */
        float a = 6.2831853f * ((float)i / 6.0f + (float)phase / 48.0f);
        cv_rect(cx + (int)(5.0f * fm_cosf(a)) - 1, cy + (int)(5.0f * fm_sinf(a)) - 1, 2, 2, CS_WINDOW);
    }
}
static void knob(int cx, int cy, int r, float frac, uint16_t col, int lit)
{
    int i, n = 48;
    float a;
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 1.0f)
        frac = 1.0f;
    for (i = 0; i <= n; i++) {               /* the arc: 270 degrees, from the lower left round to the lower right */
        float t = (float)i / (float)n;
        int x, y;
        a = 2.3561945f + t * 4.712389f;
        x = cx + (int)((float)(r + 4) * fm_cosf(a) + 0.5f);
        y = cy + (int)((float)(r + 4) * fm_sinf(a) + 0.5f);
        if (t <= frac)
            cv_rect(x - 1, y - 1, 3, 3, col);
        else if (i % 2 == 0)                     /* the rest of the arc: screentone dots */
            cv_rect(x, y, 1, 1, CS_BODY_D);
    }
    disc(cx, cy, r, lit ? RGB(59, 59, 59) : CS_WINDOW);
    circle(cx, cy, r, RGB(18, 18, 18));
    a = 2.3561945f + frac * 4.712389f;
    cv_line(cx + (int)((float)(r / 3) * fm_cosf(a)), cy + (int)((float)(r / 3) * fm_sinf(a)),
            cx + (int)((float)(r - 2) * fm_cosf(a)), cy + (int)((float)(r - 2) * fm_sinf(a)), RGB(236, 236, 236));
}
/* the four knobs of the page as a row of real ones on the plate: each its name above, its value under */
static void knob_row(int y)
{
    int i;
    uint32_t now = plat_ms();
    for (i = 0; i < 4; i++) {
        int f = page_field(i, 0), cx = 34 + i * 57, lo, hi, v, lit = ui.touched == i && now < ui.touch_until;
        char t[24];
        if (f == F_NONE)
            continue;
        field_range(f, &lo, &hi);
        v = field_get(f);
        field_text(f, t);
        text_box(cx - 28, y - 31, 56, 1, &FONT_T, &FONT_T, field_label(f), lit ? RGB(15, 15, 15) : W_INK);
        knob(cx, y, 15, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, knob_col(i), lit);
        if (ui.page == PG_SOUND && macro_moves(f))   /* the preset's MACRO moves this one: a dot */
            cv_rect(cx + 23, y - 28, 4, 4, TH_ENC[0]);
        if ((ui.page == PG_SOUND || ui.page == PG_MIX) && field_target(f) && auto_count(cur_clip(), field_target(f))) {
            /* a curve is recorded on this knob (this track, this part): a small wave; bright while it is what you hear */
            uint16_t ac = dk_auto_live(dk_song.cur_track, field_target(f)) >= 0 ? TH_REC : dimc(TH_REC, 9);
            int d;
            for (d = 0; d < 2; d++) {            /* (two pixels thick: seen at a glance) */
                cv_line(cx - 28, y - 13 + d, cx - 25, y - 20 + d, ac);
                cv_line(cx - 25, y - 20 + d, cx - 22, y - 13 + d, ac);
                cv_line(cx - 22, y - 13 + d, cx - 19, y - 20 + d, ac);
            }
        }
        text_box(cx - 28, y + 21, 56, 1, &FONT_XS, &FONT_T, t, RGB(21, 21, 21));
    }
}
/* ------------------------------------------------------------ the module --- */
/* every page but the garden and the tape is a module like SOUND's: the cassette's plate with its screws, its name
 * printed on it (left, and a word on the right), a dark window with the page's picture, the four knobs under it */
#define WX 6
#define WY 52
#define WW 228
#define WH 100
#define KNOB_Y 186
static void module(const char *left, const char *right)
{
    plate(1, 38, 238, 189);
    if (left) {                              /* the page's name in a grey caption box, as a manga's narration */
        int tw = text_w(&FONT_T, left);
        tw = tw > 116 ? 116 : tw;
        cv_rect(9, 39, tw + 10, 12, RGB(104, 104, 104));
        text_box(14, 41, 116, 0, &FONT_T, &FONT_T, left, CS_LABEL);
    }
    if (right)
        text_box(116, 41, 110, 2, &FONT_T, &FONT_T, right, W_INK);
    cv_rect(WX, WY, WW, WH, W_BG);           /* the window: a black panel */
    {                                        /* the page's rows: a dot each on the plate's right edge, the one you are on dark */
        int n = page_rows(ui.page), r = ui.row[ui.page] % (n ? n : 1), k;
        if (n > 1)
            for (k = 0; k < n; k++)
                cv_rect(WX + WW - 5, WY + 4 + k * 7, 3, 4, k == r ? CS_LABEL : RGB(84, 84, 84));   /* (inside the window) */
    }
}

/* the sound's chain, in the order the audio takes it: the insert effect, the echo, the reverb, the tape (the whole song's) */
static int chain_state(int st, int *val, const char **name)
{
    const dk_track_t *k = cur();
    unsigned by = DK_BYP(dk_song.cur_track);
    switch (st) {
    case 0: *val = k->fx ? k->fxamt : 0; *name = TAPE.fx[k->fx % TAPE.nfx]->name; return (by & DK_BYP_FX) != 0;
    case 1: *val = k->echo; *name = "ECHO"; return (by & DK_BYP_ECHO) != 0;
    case 2: *val = k->rev; *name = "REVERB"; return (by & DK_BYP_REV) != 0;
    default: *val = dk_song.tapesim; *name = "TAPE"; return DK_BYP_TAPE() != 0;
    }
}

static void draw_sound(void)                /* SOUND: the source on the left, the effect chain on the right, the knob you touched big under */
{
    static const char *const TAB[5] = {"FAV", "MORE", "FX", "TYPE", "MOD"};
    const dk_track_t *k = cur();
    const dk_engine_t *e = cur_eng();
    dk_preset_t tmp;
    int tab = ui.row[PG_SOUND] % 5, i, x = 100, wy = WY, wh = WH;
    uint32_t now = plat_ms();
    for (i = 0; i < 5; i++) {            /* the tabs */
        uint16_t c = i == tab ? TH_TEXT : W_GRAY;
        cv_text(x, 26, &FONT_XS, TAB[i], c);
        if (i == tab)
            hline(x, 39, text_w(&FONT_XS, TAB[i]), TH_TEXT);
        x += text_w(&FONT_XS, TAB[i]) + 7;
    }
    module(e->name, 0);                      /* the engine's name, as if printed on it; the window */
    tag_box(14, wy + 2, 212, 1, &FONT_B, &FONT_XS, preset_blk(k->preset, 0), preset_of(k->eng, k->preset, &tmp)->name, TH_ENC[0], TH_TEXT);
                                         /* the block (green), then the preset (white): "SYNTHWAVE - 909 TECHNO" */
    if (tab < 4) {
        int f = page_field(ui.touched >= 0 && ui.touched < 4 ? ui.touched : 0, 0), lo, hi, v, j, nm = 0;
        char num[24];
        const char *unit[1], *st_name;
        static const char *const STG[4] = {"1", "2", "3", "4"};
        /* left: the source and its MACRO, and what the MACRO moves */
        cv_text(14, wy + 20, &FONT_XS, "MACRO", W_GRAY);
        {
            char t[8];
            cpy(itoa_b(k->macro, t), "%");
            text_box(64, wy + 20, 46, 2, &FONT_XS, &FONT_XS, t, TH_ENC[0]);
        }
        cv_rect(14, wy + 34, 96, 4, W_LINE);
        cv_rect(14, wy + 34, k->macro * 96 / 100, 4, TH_ENC[0]);
        for (i = 0; i < 3; i++)
            if (k->ma[i] && k->mt[i] && nm < 3) {
                char t[24], *q;
                q = cpy(t, target_label(k->mt[i]));
                *q++ = ' ';
                if (k->ma[i] > 0)
                    *q++ = '+';
                itoa_b(k->ma[i], q);
                text_box(14, wy + 40 + nm * 12, 98, 0, &FONT_XS, &FONT_XS, t, TH_ENC[1]);
                nm++;
            }
        if (!nm)
            cv_text(14, wy + 40, &FONT_XS, "NO MACRO", W_LINE);
        vline(116, wy + 22, 58, W_LINE);
        /* right: the chain */
        cv_text(122, wy + 20, &FONT_XS, "CHAIN", W_GRAY);
        for (i = 0; i < 4; i++) {
            int val, off = chain_state(i, &val, &st_name), live = tab == 2 || (tab == 3 && i == 0);
            uint16_t c = off ? W_LINE : live ? TH_TEXT : W_GRAY;
            int y = wy + 32 + i * 12;
            cv_text(122, y, &FONT_XS, STG[i], W_LINE);
            text_box(132, y, 56, 0, &FONT_XS, &FONT_XS, st_name, c);
            cv_rect(190, y + 4, 36, 4, W_LINE);
            if (!off && val > 0)
                cv_rect(190, y + 4, val * 36 / 100, 4, i == 0 ? TH_ENC[1] : TH_ENC[2]);
            if (off)
                hline(130, y + 6, 96, W_GRAY);
        }
        /* under: the knob you touched, its number big, its unit, its bar (the MACRO's range on it) */
        if (f != F_NONE) {
            int mm = macro_moves(f);
            field_range(f, &lo, &hi);
            v = field_get(f);
            field_big(f, num, unit);
            text_box(14, wy + 82, 54, 0, &FONT_XS, &FONT_XS, field_label(f), knob_col(ui.touched >= 0 && ui.touched < 4 ? ui.touched : 0));
            text_box(66, wy + 82, 60, 2, &FONT_B, &FONT_S, num, TH_TEXT);
            text_box(128, wy + 84, 28, 0, &FONT_XS, &FONT_XS, unit[0], W_GRAY);
            cv_rect(158, wy + 87, 66, 5, W_LINE);
            if (hi > lo)
                cv_rect(158, wy + 87, (v - lo) * 66 / (hi - lo), 5, TH_TEXT);
            if (mm) {                    /* the MACRO's reach: a tick where full MACRO takes it */
                int at = v - lo + mm * (hi - lo) / 100;
                at = at < 0 ? 0 : at > hi - lo ? hi - lo : at;
                cv_rect(158 + (hi > lo ? at * 64 / (hi - lo) : 0), wy + 84, 2, 11, TH_ENC[0]);
            }
        }
        (void)j;
        (void)wh;
        (void)now;
        return;
    }
    switch (tab) {
    default: {                           /* MOD: its shape, its depth, its rate */
        const uint8_t *m = k->mod[ui_mod_slot];
        int px = 16, py = wy + 40, cyc = 1 + m[2] / 20, mid = wy + 40;
        float depth = (float)m[1] * 0.01f;
        for (i = 0; i <= 208; i += 2) {
            float ph = (float)i / 208.0f * (float)cyc, f = ph - fm_floorf(ph), sv;
            int y;
            switch (m[0]) {
            case MOD_DICE: sv = (float)((uint32_t)(ph * 4.0f) * 2654435761u >> 24) / 128.0f - 1.0f; break;
            case MOD_PEDAL: sv = (float)i / 104.0f - 1.0f; break;
            case MOD_FOLLOW: sv = f < 0.1f ? f * 10.0f : 1.0f - (f - 0.1f) * 1.1f; sv = sv * 2.0f - 1.0f; break;
            case MOD_OFF: sv = 0.0f; break;
            default: sv = fm_sinf(6.2831853f * ph); break;
            }
            y = mid - (int)(sv * depth * 14.0f);
            if (i)
                cv_line(px, py, 16 + i, y, m[0] == MOD_OFF ? W_LINE : TH_ENC[1]);
            px = 16 + i;
            py = y;
        }
        break;
    }
    }
}

/* -------------------------------------------------------------------- SEQ --- */
static int take_live(int t)
{
    int v = (unsigned)t < DK_NTRACK ? dk_rec_take[t] : 0;
    if (v < 1 || v > 31)
        v = dk_pass > 31 ? 31 : dk_pass;
    return v;
}
static void roll(const dk_clip_t *c, int x0, int y0, int w, int h, int len, uint16_t col, int pass_red)
{
    int i, lo = 127, hi = 0;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (e->fl & EV_MOTION)
            continue;
        if (e->note < lo)
            lo = e->note;
        if (e->note > hi)
            hi = e->note;
    }
    if (hi < lo)
        return;
    if (hi - lo < 12) {
        lo -= (12 - (hi - lo)) / 2;
        hi = lo + 12;
    }
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        int x, ww, y;
        if (e->fl & EV_MOTION)
            continue;
        x = x0 + e->pos * w / (len ? len : 1);
        ww = e->dur * w / (len ? len : 1);
        y = y0 + h - 2 - (e->note - lo) * (h - 3) / (hi - lo ? hi - lo : 1);
        if (x < WX || x >= WX + WW)
            continue;
        if (x + ww > WX + WW)
            ww = WX + WW - x;
        cv_rect(x, y, ww < 2 ? 2 : ww, 2, pass_red && DK_L_TAKE(e->layer) == (unsigned)take_live(dk_song.cur_track) && dk_recording ? TH_REC : col);
    }
}

static void draw_arp_view(const dk_track_t *k)   /* the rhythm across, each note at its height (the real order) */
{
    int t = dk_song.cur_track, n = dk_held_n[t], src = (k->arp_opt >> 2) & 1, octs = k->arp[2] < 1 ? 1 : k->arp[2],
        mode = k->arp[0] % 6, total, live, cur_step = dk_arp_rpos[t] & 15, hits = 0, i, base = WY + WH - 9;
    const uint8_t *rhy = DK_ARP_RHY[k->arp_rhy % ARPR_N];
    char nm[24];
    if (!n && src && dk_harm_n)
        n = dk_harm_n > 4 ? 4 : dk_harm_n;
    live = n && (dk_playing || dk_held_n[t]);
    total = (n ? n : 3) * (octs > 3 ? 3 : octs);
    text_box(WX + 6, WY + 4, 100, 0, &FONT_B, &FONT_XS, ARP_NAME[mode], TH_TEXT);
    if (dk_held_n[t]) {                      /* what it plays: the notes held, or the song's chord */
        int p = 0, j;
        for (j = 0; j < dk_held_n[t] && j < 6; j++) {
            const char *q = DK_NOTE_NAME[dk_held[t][j] % 12];
            while (*q)
                nm[p++] = *q++;
            nm[p++] = ' ';
        }
        nm[p ? p - 1 : 0] = 0;
    } else if (src) {
        int pcs[4], j;
        for (j = 0; j < 4; j++)
            pcs[j] = 60 + dk_harm_pcs[j];
        dk_chord_name(pcs, dk_harm_n > 4 ? 4 : dk_harm_n ? dk_harm_n : 1, nm);
    } else
        cpy(nm, k->arp[3] ? "PLAY A CHORD" : "HOLD NOTES");
    text_box(WX + 100, WY + 6, 114, 2, &FONT_XS, &FONT_T, nm, dk_held_n[t] || src ? TH_ENC[1] : W_GRAY);
    hline(WX + 6, base + 2, WW - 12, W_LINE);
    for (i = 0; i < 16; i++) {
        int cx = WX + 16 + i * 13, h = rhy[i], idx, y, on = live && i == cur_step;
        uint16_t col = on ? TH_ENC[0] : h == 2 ? TH_TEXT : dimc(TH_TEXT, 12);
        if (!h || (k->arp_rhy == ARPR_SKIP && i % 4 == 3)) {
            cv_rect(cx - 2, base, 4, 4, on ? TH_ENC[0] : W_LINE);   /* a rest */
            continue;
        }
        switch (mode) {
        case 1: idx = total - 1 - hits % total; break;
        case 2: {
            int per = total > 1 ? 2 * total - 2 : 1, x2 = hits % per;
            idx = x2 < total ? x2 : per - x2;
            break;
        }
        case 3: idx = (int)(((uint32_t)hits * 2654435761u >> 16) % (uint32_t)total); break;
        default: idx = hits % total; break;
        }
        hits++;
        if (mode == 5) {                     /* CHORD: the whole stack each hit */
            int j;
            for (j = 0; j < (n ? n : 3); j++)
                disc(cx, base - 5 - j * 11, h == 2 ? 4 : 3, col);
            continue;
        }
        y = base - 5 - (total > 1 ? idx * 52 / (total - 1) : 0);
        cv_rect(cx - 1, y, 2, base + 2 - y, on ? TH_ENC[0] : W_LINE);
        disc(cx, y, h == 2 ? 4 : 3, col);
    }
}

/* the step editor's grid (SEQ's third row; STEP's own view on a melodic track): the cursor's bar, 16 steps across, the
 * notes at their pitch as bars (a tie one long bar), the step you are on lit, an empty one showing what K2 puts in */
static void draw_step_grid(const dk_clip_t *c, int len)
{
    int bar = ui_cursor / 16, cw = 13, x0 = WX + 10, gy = WY + 8, gh = WH - 20, lo = 127, hi = 0, i, b0 = bar * DK_TPB,
        cur = ui_cursor % 16, ghost = se_default();
    for (i = 0; i < c->n; i++) {             /* the pitch range: the clip's notes and the next one */
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (e->fl & EV_MOTION)
            continue;
        if (e->note < lo)
            lo = e->note;
        if (e->note > hi)
            hi = e->note;
    }
    if (ghost < lo)
        lo = ghost;
    if (ghost > hi)
        hi = ghost;
    if (hi - lo < 12) {
        lo -= (12 - (hi - lo)) / 2;
        hi = lo + 12;
    }
    for (i = 0; i < 16; i++) {               /* the steps: a beat's four together, the cursor's lit */
        int x = x0 + i * cw;
        if (b0 + i * DK_T16 >= len)
            break;
        if (i == cur)
            cv_rect(x, gy - 5, cw - 1, gh + 8, dimc(TH_ENC[0], 6));
        else if ((i / 4) & 1)
            cv_rect(x, gy - 5, cw - 1, gh + 8, RGB(36, 36, 36));
        if (i) {                             /* the cells' edges: dotted, a beat's solid */
            int yy;
            for (yy = gy - 5; yy < gy + gh + 3; yy += i % 4 ? 3 : 1)
                cv_rect(x - 1, yy, 1, 1, i % 4 ? RGB(57, 57, 57) : W_LINE);
        }
        cv_rect(x + cw / 2 - 1, WY + WH - 8, i == cur ? 3 : 2, i % 4 ? 3 : 5, i == cur ? TH_ENC[0] : i % 4 ? W_LINE : W_GRAY);
    }
    for (i = 0; i < c->n; i++) {             /* the notes: a bar each, from its step to where it reaches */
        const dk_ev_t *e = &dk_ev[c->at + i];
        int a, z, y, at_cur;
        if (e->fl & EV_MOTION)
            continue;
        a = e->pos - b0;
        z = a + e->dur + 2;
        if (z <= 0 || a >= DK_TPB)
            continue;
        y = gy + gh - 5 - (e->note - lo) * (gh - 5) / (hi - lo);
        at_cur = e->pos < (int)(ui_cursor + 1) * DK_T16 && e->pos + e->dur > (int)ui_cursor * DK_T16;
        a = a < 0 ? x0 : x0 + a * cw / DK_T16;
        z = z > DK_TPB ? x0 + 16 * cw : x0 + z * cw / DK_T16 - 2;
        if (z < a + 3)
            z = a + 3;
        cv_rect(a, y, z - a, 5, at_cur ? TH_TEXT : TH_TRACK[dk_song.cur_track]);
        if (e->pos >= b0)                    /* its start: a notch */
            cv_rect(a, y - 2, 2, 9, at_cur ? TH_TEXT : TH_TRACK[dk_song.cur_track]);
    }
    {                                        /* an empty step: the note K2 would put in, an outline */
        int idx[SE_MAX], clip = cur_clip();
        if (!se_at(clip, ui_cursor * DK_T16, idx) && !se_over(clip, ui_cursor * DK_T16, idx)) {
            int y = gy + gh - 5 - (ghost - lo) * (gh - 5) / (hi - lo), x = x0 + cur * cw;
            hline(x + 1, y, cw - 3, TH_ENC[0]);
            hline(x + 1, y + 4, cw - 3, TH_ENC[0]);
            vline(x + 1, y, 5, TH_ENC[0]);
            vline(x + cw - 3, y, 5, TH_ENC[0]);
        }
    }
    if (dk_playing) {
        int ph = (int)(dk_part_tick % (uint16_t)len);
        if (ph / DK_TPB == bar)
            vline(x0 + (ph % DK_TPB) * cw / DK_T16, WY + 1, WH - 2, TH_TEXT);
    }
}
static int seq_editable(void)            /* a melodic clip the step editor walks (STEP, LIVE) */
{
    return !(cur_eng()->flags & EF_DRUM) && (cur()->seq == SQ_STEP || cur()->seq == SQ_LIVE);
}
static int seq_edit_row(void)            /* SEQ's third row on one: the step editor */
{
    return ui.page == PG_SEQ && seq_editable() && ui.row[PG_SEQ] % 3 == 2;
}

/* ENDLESS as on OP-1: its rate up left, how many steps big (orange), the way it goes as an arrow, the steps under it
 * (a note's step lit, a tie a line on, the one playing bright); entering: REC blinking; transposed: by how much */
static void draw_endless(const dk_clip_t *c)
{
    static const char *const RATEN[6] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
    static const char *const DIRN[4] = {"FORWARD", "BACKWARD", "PINGPONG", "RANDOM"};
    const uint16_t OR = RGB(246, 120, 64);
    const dk_track_t *k = cur();
    int t = dk_song.cur_track % DK_NTRACK, n = c->len / DK_T16, dir = k->drv[0] & 3, i, nw, x, y = WY + 16, ax, ay,
        cs = dk_endl_step[t], tr = dk_endl_tr[t];
    char s[8];
    uint32_t now = plat_ms();
    text_box(WX + 6, WY + 3, 60, 0, &FONT_T, &FONT_T, RATEN[k->drv[2] % 6], W_GRAY);
    if (ui.endl_on) {                         /* entering: REC */
        if ((now / 400u) & 1u)
            disc(WX + WW - 52, WY + 8, 3, TH_REC);
        text_box(WX + WW - 46, WY + 3, 42, 2, &FONT_T, &FONT_T, "ENTER", TH_REC);
    } else
        text_box(WX + WW - 86, WY + 3, 82, 2, &FONT_T, &FONT_T, DIRN[dir], W_GRAY);
    if (tr && !ui.endl_on) {                  /* transposed by a key */
        s[0] = tr > 0 ? '+' : '-';
        itoa_b(tr > 0 ? tr : -tr, s + 1);
        text_box(WX + 6, WY + 16, 40, 0, &FONT_T, &FONT_T, s, TH_ENC[2]);
    }
    itoa_b(n, s);
    nw = text2x_w(s);
    x = WX + (WW - nw - 34) / 2;
    text2x(x, y, s, n ? OR : W_LINE);
    ax = x + nw + 10;                        /* the arrow: the way it goes */
    ay = y + 30;
    if (dir == 3)
        text_box(ax, ay - 16, 24, 0, &FONT_L, &FONT_B, "?", OR);
    else {
        int kk;
        for (kk = 0; kk < 2; kk++) {
            hline(ax, ay + kk, 22, OR);
            if (dir != 1) {                  /* a head to the right */
                cv_line(ax + 22, ay + kk, ax + 15, ay - 7 + kk, OR);
                cv_line(ax + 22, ay + kk, ax + 15, ay + 7 + kk, OR);
            }
            if (dir != 0) {                  /* to the left */
                cv_line(ax, ay + kk, ax + 7, ay - 7 + kk, OR);
                cv_line(ax, ay + kk, ax + 7, ay + 7 + kk, OR);
            }
        }
    }
    if (!n) {
        text_box(WX, WY + WH - 18, WW, 1, &FONT_T, &FONT_XS, ui.endl_on ? "PLAY THE STEPS" : "REC TO PLAY IN STEPS", W_GRAY);
        return;
    }
    {                                        /* the steps */
        int x0 = WX + 6, w = WW - 12, sy = WY + WH - 14;
        for (i = 0; i < n; i++) {
            int a = x0 + i * w / n, b = x0 + (i + 1) * w / n - 1, j, on = 0, tied = 0;
            for (j = 0; j < c->n; j++) {
                const dk_ev_t *e = &dk_ev[c->at + j];
                if (e->fl & EV_MOTION)
                    continue;
                if (e->pos == i * DK_T16)
                    on = 1;
                else if (e->pos < i * DK_T16 && e->pos + e->dur + 2 > i * DK_T16)
                    tied = 1;
            }
            if (b <= a)
                b = a + 1;
            if (on)
                cv_rect(a, sy, b - a, 8, dk_playing && i == cs ? OR : TH_TEXT);
            else if (tied)
                cv_rect(a, sy + 3, b - a + 1, 2, dk_playing && i == cs ? OR : W_GRAY);
            else
                cv_rect(a, sy + 6, b - a, 2, W_LINE);   /* a rest */
        }
    }
}

static void draw_seq(void)
{
    const dk_track_t *k = cur();
    const dk_engine_t *e = cur_eng();
    const dk_clip_t *c = &dk_song.clip[cur_clip()];
    int len = clip_len(cur_clip()), i, x;
    uint32_t now = plat_ms();
    char s[24], r[24];
    int view = ui.page == PG_ARP ? SQ_ARP : k->seq, drum = (e->flags & EF_DRUM) != 0;
    if (view == SQ_ARP) {
        cpy(cpy(r, k->arp[3] ? "LATCH  " : ""), dk_arp_on(k) ? "" : "OFF");
        module("ARP", r);
        draw_arp_view(k);
        return;
    }
    r[0] = 0;
    if (seq_edit_row()) {                /* the step editor: which bar */
        x = (int)(itoa_b(ui_cursor / 16 + 1, cpy(r, "BAR ")) - r);
        r[x] = '/';
        itoa_b(len / DK_TPB, r + x + 1);
    }
    if (drum && k->seq == SQ_STEP) {
        x = (int)(itoa_b(ui_step_page + 1, r) - r);
        r[x] = '/';
        itoa_b(len / DK_TPB, r + x + 1);
    }
    if (dk_arp_on(k)) {
        char *p = r;
        while (*p)
            p++;
        if (p != r)
            p = cpy(p, "  ");
        cpy(p, "+ ARP");
    }
    {
        char lt[24];
        const char *nm = drum && ui.row[PG_SEQ] % 3 == 1 ? "KIT" : seq_edit_row() ? "STEP EDIT" : SEQ_NAME[k->seq % SQ_N];
        if (drum && e->nlanes)               /* drums: the sound you are on, by its name */
            cpy(cpy(cpy(lt, nm), "  "), e->lane_name[ui_lane % e->nlanes]);
        else
            cpy(lt, nm);
        module(lt, r);
    }
    if (seq_edit_row()) {
        draw_step_grid(c, len);
        return;
    }
    switch (view) {
    case SQ_STEP:
        if (drum) {                          /* the lanes, the steps: on a dot, off a small grey one */
            int lanes = e->nlanes ? e->nlanes : 8, l, ph = (int)(dk_part_tick % (uint16_t)len) / DK_T16 - ui_step_page * 16;
            if (lanes > 8)
                lanes = 8;
            if (ui.row[PG_SEQ] % 3 == 1) {   /* KIT: each sound's level (a bar), its tune, decay and its own FX */
                for (l = 0; l < lanes; l++) {
                    const dk_lane_t *ln = &dk_song.lane[dk_song.cur_track][l];
                    int y = WY + 8 + l * 12, on_lane = l == ui_lane % lanes, w = (ln->level + 24) * 110 / 36;
                    text_box(WX + 4, y - 4, 40, 0, &FONT_T, &FONT_T, e->lane_name[l], on_lane ? TH_ENC[0] : W_GRAY);
                    cv_rect(WX + 48, y - 1, 110, 3, W_LINE);
                    cv_rect(WX + 48, y - 1, w, 3, on_lane ? TH_ENC[1] : W_GRAY);
                    vline(WX + 48 + 24 * 110 / 36, y - 3, 7, TH_TEXT);   /* 0 dB */
                    if (ln->tune)
                        text_box(WX + 162, y - 4, 16, 0, &FONT_T, &FONT_T, ln->tune > 0 ? "T+" : "T-", TH_ENC[2]);
                    if (ln->decay)
                        text_box(WX + 180, y - 4, 16, 0, &FONT_T, &FONT_T, ln->decay > 0 ? "D+" : "D-", RGB(250, 150, 70));
                    if (ln->rev || ln->echo || ln->crush)
                        text_box(WX + 198, y - 4, 20, 0, &FONT_T, &FONT_T, ln->crush ? "FX8" : "FX", TH_ENC[1]);
                }
                break;
            }
            for (l = 0; l < lanes; l++) {
                int y = WY + 8 + l * 12, on_lane = l == ui_lane % lanes;
                {                            /* two letters a lane (the one you are on by its name, above) */
                    char ab[3] = {e->lane_name[l][0], e->lane_name[l][1], 0};
                    text_box(WX + 7, y - 4, 20, 0, &FONT_T, &FONT_T, ab, on_lane ? TH_ENC[0] : W_GRAY);
                    if (on_lane)
                        cv_rect(WX + 1, y - 2, 3, 4, TH_ENC[0]);
                }
                for (i = 0; i < 16; i++) {
                    int cx = WX + 33 + i * 12 + i / 4 * 2, pos = (ui_step_page * 16 + i) * DK_T16,
                        hit = clip_find(cur_clip(), pos, e->lane_note[l], 0);
                    if (pos >= len)
                        break;
                    if (hit >= 0)
                        cv_rect(cx - 4, y - 4, 9, 9, on_lane ? TH_ENC[0] : TH_TEXT);
                    else
                        cv_rect(cx - 3, y - 2, 6, 5, on_lane ? dimc(TH_ENC[0], 7)
                                                    : i % 4 ? RGB(60, 60, 60) : RGB(97, 97, 97));
                }
            }
            if (dk_playing && ph >= 0 && ph < 16)
                vline(WX + 33 + ph * 12 + ph / 4 * 2, WY + 1, WH - 2, TH_TEXT);
        } else {                             /* melodic: the clip as a DAW's roll (the step editor is its third row) */
            roll(c, WX + 4, WY + 4, WW - 8, WH - 14, len, TH_TRACK[dk_song.cur_track], 1);
            hline(WX + 4, WY + WH - 6, WW - 8, W_LINE);
            for (i = 0; i <= len / DK_TPB; i++)
                vline(WX + 4 + i * DK_TPB * (WW - 8) / (len ? len : 1), WY + WH - 9, 5, W_GRAY);
            if (dk_playing)
                vline(WX + 4 + (int)(dk_part_tick % (uint16_t)len) * (WW - 8) / len, WY + 1, WH - 2, TH_TEXT);
            if (!c->n)
                text_box(WX, WY + 34, WW, 1, &FONT_XS, &FONT_T, "REC TO RECORD", W_GRAY);
        }
        break;
    case SQ_LIVE:                        /* (an old project's LIVE: the same as SEQ) */
        roll(c, WX + 4, WY + 4, WW - 8, WH - 14, len, TH_TRACK[dk_song.cur_track], 1);
        hline(WX + 4, WY + WH - 6, WW - 8, W_LINE);
        for (i = 0; i <= len / DK_TPB; i++)
            vline(WX + 4 + i * DK_TPB * (WW - 8) / (len ? len : 1), WY + WH - 9, 5, W_GRAY);
        if (dk_playing)
            vline(WX + 4 + (int)(dk_part_tick % (uint16_t)len) * (WW - 8) / len, WY + 1, WH - 2, TH_TEXT);
        if (!c->n)
            text_box(WX, WY + 34, WW, 1, &FONT_XS, &FONT_T, "REC TO RECORD", W_GRAY);
        break;
    case SQ_ENDLESS:
        draw_endless(c);
        break;
    default:
        (void)now;
        (void)s;
        break;
    }
}

/* -------------------------------------------------------------------- MIX --- */
/* MIX: a little desk in the window: a pan knob each, the fader in its slot, the meter's lights, the tape strip with
 * the number at its foot (muted: grey and M; solo: S) */
static void draw_mix(void)
{
    int t;
    char s[16], ttl[28];
    int pp = (dk_playing ? dk_cur_part : dk_song.cur_part) & 7, blink = (plat_ms() / 230u) & 1u, i0 = 0;
    itoa_b((int)(dk_master_level * 100.0f), s);
    ttl[i0++] = (char)('A' + pp);              /* the part the mutes are in */
    ttl[i0++] = ' ';
    {
        const char *l = track_label(dk_song.cur_track);
        while (*l && i0 < 26)
            ttl[i0++] = *l++;
    }
    ttl[i0] = 0;
    module(ttl, s);
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_track_t *k = &dk_song.trk[t];
        int pm = part_muted(t), pend = part_mute_pending(t), show = pend >= 0 && blink ? pend : pm;   /* (a change waiting: it blinks) */
        int x = WX + 2 + t * 36, cx = x + 15, top = WY + 20, h = 44, fy = top + h - (int)k->vol * h / 100,
            sel = t == dk_song.cur_track, lv = (int)(fm_minf(1.0f, dk_level[t] * 3.0f) * 7.0f), i;
        if (sel)
            cv_rect(x, WY + 1, 34, WH - 2, RGB(47, 47, 47));
        knob(cx, WY + 9, 5, (float)(k->pan + 64) / 128.0f, sel ? TH_ENC[1] : W_GRAY, 0);
        cv_rect(cx - 1, top, 3, h, RGB(14, 14, 14));      /* the slot */
        for (i = 0; i < 7; i++)                            /* the meter: seven lights */
            cv_rect(x + 27, top + h - 5 - i * 6, 3, 4, i < lv ? (i < 4 ? TH_ENC[0] : i < 6 ? TH_ENC[2] : TH_REC) : RGB(55, 55, 55));
        cv_rect(cx - 8, fy - 4, 17, 8, sel ? CS_LABEL : CS_BODY_D);   /* the cap */
        hline(cx - 8, fy, 17, sel ? TH_ENC[0] : CS_INK);
        cv_rect(x + 4, WY + WH - 14, 24, 12, show ? CS_BODY_D : TH_TRACK[t]);   /* the strip of tape: dark when silent */
        s[0] = (char)('1' + t);                    /* the part's mute: the strip dark */
        s[1] = 0;
        text_box(x + 4, WY + WH - 14, 24, 1, &FONT_T, &FONT_T, s, show ? CS_INK : TH_BG);
        if (show) {
            int i;
            for (i = 0; i < 24; i++)
                cv_rect(x + 4 + i, WY + WH - 4 - i * 10 / 23, 1, 2, TH_REC);
        }
        if (pend >= 0)                             /* waiting for the bar's end: a red edge above the strip */
            hline(x + 4, WY + WH - 16, 24, TH_REC);
        if (k->solo)
            text_box(x + 2, WY + 1, 10, 0, &FONT_T, &FONT_T, "S", TH_REC);
    }
}

/* ------------------------------------------------------------------- ROAD --- */
static void draw_road(void)
{
    int i;
    char s[16], r[20];
    if (ui.garage) {                        /* the garage: the eight parts, their labels, their lengths, their clips */
        module("GARAGE", "THE PARTS");
        for (i = 0; i < DK_NPART; i++) {
            int x = WX + 3 + (i / 4) * 113, y = WY + 3 + (i % 4) * 24, t, sel = i == ui_gpart;
            if (sel)
                cv_rect(x, y, 109, 22, RGB(53, 53, 53));
            if (ui.dup_t && plat_ms() - ui.dup_t < 3000u && i == ui.dup_to)   /* the overwrite asked: the target red */
                cv_rect(x, y, 109, 22, dimc(TH_REC, 4));
            cv_rect(x + 3, y + 4, 14, 14, TH_PART[i]);
            s[0] = (char)('A' + i);
            s[1] = 0;
            text_box(x + 3, y + 7, 14, 1, &FONT_T, &FONT_T, s, TH_BG);
            text_box(x + 21, y + 7, 46, 0, &FONT_T, &FONT_T, LABEL_NAME[dk_song.part[i].label & 7], sel ? TH_ENC[1] : TH_TEXT);
            itoa_b(dk_song.part[i].bars, s);
            text_box(x + 64, y + 7, 13, 2, &FONT_T, &FONT_T, s, sel ? TH_ENC[2] : W_GRAY);
            for (t = 0; t < DK_NTRACK; t++)
                cv_rect(x + 82 + t * 4, y + 10, 3, 3, CLIP(i, t)->n ? TH_TRACK[t] : W_LINE);
        }
        return;
    }
    {   /* the song as a tape between two reels: the blocks pieces of coloured label on it, running under the head */
        int cur_b = dk_playing && dk_song.play_mode ? dk_road_i : ui_road_cur, bi, head = WX + 84, ty = WY + 42, th = 32;
        int start[DK_NROAD + 1], total = 0, pos, phase = dk_playing ? (int)(dk_ticks * 8u / DK_TPB) % 48 : 0;
        float prog = 0.0f, wound;
        for (bi = 0; bi < dk_song.nroad; bi++) {
            const dk_block_t *b = &dk_song.road[bi];
            int w = dk_song.part[b->part & 7].bars * (b->reps ? b->reps : 1) * 9;
            start[bi] = total;
            total += w < 30 ? 30 : w > 200 ? 200 : w;
        }
        start[dk_song.nroad] = total;
        if (dk_playing && dk_song.play_mode && dk_part_len)
            prog = (float)dk_part_tick / (float)dk_part_len;
        pos = cur_b < dk_song.nroad ? start[cur_b] + (int)(prog * (float)(start[cur_b + 1] - start[cur_b])) : total;
        wound = total ? (float)pos / (float)total : 0.0f;
        cpy(itoa_b(dk_song.nroad, r), dk_song.nroad == 1 ? " BLOCK" : " BLOCKS");
        module(dk_song.play_mode ? "SONG" : "PART", r);
        reel(WX + 20, WY + 18, 1.0f - wound, phase);    /* the reels, the tape between them */
        reel(WX + WW - 20, WY + 18, wound, phase);
        hline(WX + 20, WY + 3, WW - 40, CS_TAPE);
        cv_rect(WX, ty, WW, th, CS_TAPE);
        for (bi = 0; bi < dk_song.nroad; bi++) {
            const dk_block_t *b = &dk_song.road[bi];
            int x = head + start[bi] - pos, w = start[bi + 1] - start[bi] - 2, on = bi == ui_road_cur, x0, x1;
            uint16_t c = TH_PART[b->part & 7];
            if (x > WX + WW || x + w < WX)
                continue;
            x0 = x < WX ? WX : x;
            x1 = x + w > WX + WW ? WX + WW : x + w;
            if (bi < cur_b)
                c = dimc(c, 9);              /* played: a little faded */
            cv_rect(x0, ty + 2, x1 - x0, th - 4, c);
            if (on) {                        /* the block you are on: a frame inside it */
                hline(x0, ty + 2, x1 - x0, TH_TEXT);
                hline(x0, ty + th - 3, x1 - x0, TH_TEXT);
                if (x >= WX)
                    vline(x, ty + 2, th - 4, TH_TEXT);
                if (x + w <= WX + WW)
                    vline(x + w - 1, ty + 2, th - 4, TH_TEXT);
            }
            if (x >= WX && x + 40 <= WX + WW) {
                s[0] = (char)('A' + (b->part & 7));
                s[1] = 0;
                text_box(x + 3, ty + 3, 12, 0, &FONT_T, &FONT_T, s, TH_BG);
                text_box(x + 3, ty + 13, w - 6 > 4 ? (x1 - x - 6 > 4 ? x1 - x - 6 : 4) : 4, 0, &FONT_T, &FONT_T,
                         LABEL_NAME[dk_song.part[b->part & 7].label & 7], TH_BG);
                if (b->reps > 1 && x + w <= WX + WW) {
                    s[0] = 'X';
                    itoa_b(b->reps, s + 1);
                    text_box(x + w - 22, ty + 3, 20, 2, &FONT_T, &FONT_T, s, TH_BG);
                }
            }
            if ((b->tricks || b->drop) && x >= WX) {   /* its tricks: dots under it */
                int d = 0, j;
                for (j = 0; j < 7; j++)
                    if ((j < 6 ? b->tricks >> j : b->drop) & 1u)
                        disc(x + 5 + 5 * d++, ty + th + 3, 1, TH_ENC[2]);
            }
        }
        {                                    /* the head: a marker above and under the tape */
            uint16_t hc = dk_playing ? TH_REC : TH_TEXT;
            int j;
            for (j = 0; j < 4; j++) {
                hline(head - 3 + j, ty - 5 + j, 7 - 2 * j, hc);
                hline(head - 3 + j, ty + th + 4 - j, 7 - 2 * j, hc);
            }
            vline(head, ty, th, hc);
        }
        if (!dk_song.nroad)
            text_box(WX, ty + 7, WW, 1, &FONT_T, &FONT_T, "WHITE A-H: ADD A BLOCK", TH_TEXT);
        for (bi = 0; bi < dk_song.nroad && total; bi++) {   /* the whole song, small */
            int x0 = WX + 30 + start[bi] * (WW - 60) / total, x1 = WX + 30 + start[bi + 1] * (WW - 60) / total;
            cv_rect(x0, WY + WH - 6, x1 - x0 - 1 > 1 ? x1 - x0 - 1 : 1, 3, TH_PART[dk_song.road[bi].part & 7]);
        }
        if (total)
            cv_rect(WX + 30 + pos * (WW - 60) / total - 1, WY + WH - 9, 2, 8, TH_TEXT);
    }
}

/* ------------------------------------------------------------------- KEYS --- */
static void draw_keys(void)
{
    char s[32], r[32];
    int i, minor = dk_song.mode == 1 || dk_song.mode == 2 || dk_song.mode == 3 || dk_song.mode == 6 ||
                   dk_song.mode == 7 || dk_song.mode == 9 || dk_song.mode == 10;
    const char *nm = have_chord ? chord_name : "";
    int drum = (TAPE.eng[cur()->eng % TAPE.neng]->flags & EF_DRUM) != 0, ky = WY + 44, kh_ = WH - 46, root_lit = plat_ms() < ui.root_until;
    {
        char *p = r;
        if (chord_deg >= 0)
            p = cpy(cpy(p, degree_name(chord_deg, minor)), "  ");
        p = cpy(p, DK_NOTE_NAME[dk_song.root % 12]);
        *p++ = ' ';
        cpy(p, DK_MODE_NAME[dk_song.mode % DK_NMODES]);
    }
    module(dk_song.white ? "WHITE = CHORDS" : "WHITE = NOTES", r);
    if (nm[0] && !root_lit)
        text_box(WX, WY + 2, WW, 1, &FONT_L, &FONT_B, nm, TH_TEXT);
    else {                                   /* nothing played yet (or the root turning): the key itself, big */
        char *p = cpy(s, DK_NOTE_NAME[dk_song.root % 12]);
        *p++ = ' ';
        cpy(p, DK_MODE_NAME[dk_song.mode % DK_NMODES]);
        text_box(WX, WY + 2, WW, 1, &FONT_L, &FONT_B, s, root_lit ? TH_ENC[0] : TH_TEXT);
    }
    for (i = 0; i < 16; i++) {               /* the keys: outlines, the sounding ones filled */
        int x = WX + 2 + i * 13 + 3, k = WHITE_K[i], lit = kh[k].trk >= 0 && kh[k].kind == 0 && kh[k].n;
        cv_rect(x, ky, 12, kh_, lit ? TH_ENC[0] : CS_LABEL);
        vline(x + 11, ky, kh_, CS_BODY_D);
        if (!drum && dk_song.layout) {       /* SCALE: each key's note (or the chord's degree), the tonic marked */
            int nt = (key_note_of(k, dk_song.cur_track) % 12 + 12) % 12,       /* (a note below zero, a low octave: its pitch class) */
                ton = nt == dk_song.root % 12 && !dk_song.white,
                dgi = white_degree(i) >= 0 ? white_degree(i) : i % 7;
            char dg[2] = {(char)('1' + dgi), 0};
            const char *lb = dk_song.white ? dg : DK_NOTE_NAME[nt];
            if (dk_song.white && dgi == 0)
                ton = 1;
            text_box(x, ky + kh_ - 12, 11, 1, &FONT_T, &FONT_T, lb, lit ? TH_BG : ton ? RGB(200, 140, 30) : CS_INK);
            if (ton)
                cv_rect(x + 3, ky + kh_ - 3, 5, 2, lit ? TH_BG : RGB(200, 140, 30));
        }
    }
    for (i = 0; i < 11; i++) {
        int k = BLACK_K[i], w = white_of(k - 1), x = WX + 2 + w * 13 + 3 + 8, lit = kh[k].trk >= 0 && kh[k].n;
        cv_rect(x, ky, 8, kh_ / 2 + 2, lit ? TH_ENC[1] : RGB(20, 20, 20));
    }
}

/* ---------------------------------------------------------------- PROJECT --- */
/* PROJECT: the four projects as cassettes in a row; the one open pulled up, its reels turning while it plays */
static void mini_reel(int cx, int cy, int phase, int on)
{
    int i;
    disc(cx, cy, 6, on ? CS_TAPE : CS_BODY_D);
    disc(cx, cy, 3, CS_LABEL);
    for (i = 0; i < 3; i++) {
        float a = 6.2831853f * ((float)i / 3.0f + (float)phase / 24.0f);
        cv_rect(cx + (int)(2.0f * fm_cosf(a)), cy + (int)(2.0f * fm_sinf(a)), 1, 1, CS_BODY_D);
    }
}
static void draw_project(void)
{
    char nm[12], s[4];
    int i, fill = dk_song.nev * (WW - 20) / DK_NEV, sel = field_get(F_SLOT),
        phase = dk_playing ? (int)(dk_ticks * 8u / DK_TPB) % 24 : 0;
    module("PROJECTS", ui.clr_t && plat_ms() - ui.clr_t <= 3000u ? "CLEAR ALL: PRESS AGAIN" : export_busy() ? "EXPORT..." : dk_stems ? "STEMS: RECORD IT" : ui.dirty ? "NOT SAVED" : "SAVED");
    for (i = 0; i < DK_NPROJ; i++) {
        int x = WX + 6 + i * 55, y = WY + 18, w = 51, h = 48, open = i == set.proj, used = project_used(i) || open, j;
        s[0] = (char)('1' + i);
        s[1] = 0;
        if (open)
            y -= 6;                          /* pulled up */
        if (!used) {                         /* an empty place: its outline, its number */
            for (j = 0; j < w; j += 5)
                hline(x + j, y, 2, W_LINE), hline(x + j, y + h - 1, 2, W_LINE);
            for (j = 0; j < h; j += 5)
                vline(x, y + j, 2, W_LINE), vline(x + w - 1, y + j, 2, W_LINE);
            text_box(x, y + 16, w, 1, &FONT_XS, &FONT_T, s, i == sel ? TH_ENC[0] : W_GRAY);
        } else {
            cv_rect(x, y, w, h, CS_BODY);
            hline(x, y + h - 1, w, CS_BODY_D);
            cv_rect(x + 3, y + 3, w - 6, 20, CS_LABEL);
            for (j = 0; j < 3; j++)
                cv_rect(x + 3, y + 19 + j, w - 6, 1, CS_STRIPE[j]);
            text_box(x + 3, y + 6, w - 6, 1, &FONT_T, &FONT_T, s, CS_INK);  /* its number; the name under the shelf */
            cv_rect(x + 9, y + 27, w - 18, 12, W_BG);
            mini_reel(x + 17, y + 33, open ? phase : 0, open);
            mini_reel(x + w - 17, y + 33, open ? phase : 0, open);
        }
        if (i == sel && !open) {             /* K1's choice: a frame */
            hline(x - 2, y - 2, w + 4, TH_ENC[0]);
            hline(x - 2, y + h + 1, w + 4, TH_ENC[0]);
            vline(x - 2, y - 2, h + 4, TH_ENC[0]);
            vline(x + w + 1, y - 2, h + 4, TH_ENC[0]);
        }
    }
    {                                        /* K1's cassette by its name */
        char line[24];
        const char *n = sel == set.proj ? dk_song.name : project_used(sel) ? project_name(sel, nm) : "EMPTY";
        s[0] = (char)('1' + sel);
        s[1] = 0;
        cpy(cpy(cpy(line, s), "  "), n);
        text_box(WX + 4, WY + 74, WW - 8, 1, &FONT_T, &FONT_T, line, TH_ENC[0]);
    }
    if (export_busy()) {
        cv_rect(WX + 10, WY + WH - 8, WW - 20, 3, W_LINE);
        cv_rect(WX + 10, WY + WH - 8, export_pct() * (WW - 20) / 100, 3, TH_ENC[1]);
    } else {                                 /* how full the open one is */
        cv_rect(WX + 10, WY + WH - 7, WW - 20, 2, W_LINE);
        cv_rect(WX + 10, WY + WH - 7, fill > WW - 20 ? WW - 20 : fill, 2, fill > (WW - 20) * 4 / 5 ? TH_REC : dimc(TH_ENC[1], 10));
    }
}

/* ATMO: eight tiles in the window, each a little picture of the layer (it moves while it plays), its name, how much;
 * the row the knobs are on framed in their colours */
static void atmo_icon(int kind, int cx, int cy, int on, uint32_t now)
{
    uint16_t c = on ? TH_TEXT : W_LINE;
    int i;
    switch (kind) {
    case 0:                                  /* RAIN: a cloud, the drops falling */
        disc(cx - 7, cy - 6, 6, on ? RGB(159, 159, 159) : W_LINE);
        disc(cx + 3, cy - 8, 8, on ? RGB(179, 179, 179) : W_LINE);
        disc(cx + 10, cy - 5, 5, on ? RGB(159, 159, 159) : W_LINE);
        for (i = 0; i < 4; i++) {
            int y = cy + 1 + (int)((now / 60u + (uint32_t)i * 5u) % 8u);
            vline(cx - 9 + i * 6, on ? y : cy + 4, 3, on ? RGB(110, 186, 244) : W_LINE);
        }
        break;
    case 1:                                  /* FIRE: the flame flickering */
        for (i = 0; i < 5; i++) {
            int fl = on ? (int)((now / 90u + (uint32_t)i * 7u) % 3u) - 1 : 0;
            disc(cx + fl, cy + 8 - i * 4, 8 - i, on ? (i < 2 ? RGB(232, 72, 64) : i < 4 ? RGB(250, 150, 70) : RGB(246, 214, 92)) : W_LINE);
        }
        hline(cx - 10, cy + 13, 20, on ? RGB(150, 98, 58) : W_LINE);
        break;
    case 2:                                  /* BIRDS: two of them, wings going */
        for (i = 0; i < 2; i++) {
            int bx = cx - 8 + i * 13, by = cy - 4 + i * 6, w = on ? (int)((now / 150u + (uint32_t)i) % 2u) * 2 : 0;
            cv_line(bx - 6, by - 3 + w, bx, by, c);
            cv_line(bx, by, bx + 6, by - 3 + w, c);
        }
        break;
    case 3:                                  /* OCEAN: waves rolling */
        for (i = 0; i < 2; i++) {
            int x, py = 0;
            for (x = -14; x <= 14; x++) {
                int y = cy - 4 + i * 9 + (int)(3.0f * fm_sinf((float)x * 0.35f + (on ? (float)now * 0.004f : 0.0f) + (float)i));
                if (x > -14)
                    cv_line(cx + x - 1, py, cx + x, y, on ? (i ? RGB(110, 186, 244) : RGB(150, 214, 170)) : W_LINE);
                py = y;
            }
        }
        break;
    case 4:                                  /* WIND: three gusts */
        for (i = 0; i < 3; i++) {
            int off = on ? (int)((now / 80u + (uint32_t)i * 9u) % 8u) - 4 : 0;
            hline(cx - 12 + off, cy - 8 + i * 7, 18 - i * 3, c);
            disc(cx + 6 - i * 3 + off, cy - 10 + i * 7, 2, c);
        }
        break;
    case 5:                                  /* CRICKETS: the moon, the stars blinking */
        disc(cx - 4, cy - 2, 9, on ? RGB(236, 236, 236) : W_LINE);
        disc(cx + 1, cy - 5, 8, on ? RGB(39, 39, 39) : W_BG);
        for (i = 0; i < 4; i++)
            if (!on || ((now / 300u + (uint32_t)i) % 3u))
                cv_rect(cx + 6 + (i & 1) * 5, cy - 9 + i * 5, 2, 2, on ? RGB(246, 214, 92) : W_LINE);
        break;
    case 6:                                  /* the DUST: a record turning, its crackle */
        disc(cx, cy, 11, on ? RGB(22, 22, 22) : W_BG);
        circle(cx, cy, 11, c);
        circle(cx, cy, 8, on ? W_GRAY : W_LINE);
        circle(cx, cy, 5, on ? W_GRAY : W_LINE);
        disc(cx, cy, 3, on ? RGB(232, 72, 64) : W_LINE);
        if (on) {
            float a = (float)now * 0.006f;
            cv_rect(cx + (int)(9.0f * fm_cosf(a)), cy + (int)(9.0f * fm_sinf(a)), 2, 2, TH_TEXT);
        }
        break;
    default: {                               /* TAPE: a little cassette, its reels turning */
        int ph = on ? (int)(now / 40u % 24u) : 0;
        cv_rect(cx - 15, cy - 10, 30, 20, on ? CS_BODY : W_LINE);
        cv_rect(cx - 11, cy - 3, 22, 8, W_BG);
        mini_reel(cx - 6, cy + 1, ph, on);
        mini_reel(cx + 6, cy + 1, ph, on);
        break;
    }
    }
}
static void draw_atmo(void)
{
    static const uint8_t FLD[8] = {F_ATMO0, F_ATMO1, F_ATMO2, F_ATMO3, F_ATMO4, F_ATMO5, F_DUST, F_ATAPE};
    int i, row = (ui.page == PG_GLOB ? ui.row[PG_GLOB] - 5 : ui.row[PG_ATMO]) & 1;
    uint32_t now = plat_ms();
    module("ATMO", row ? "5-6  DUST  TAPE" : "LAYERS 1-4");
    for (i = 0; i < 8; i++) {
        int x = WX + 1 + (i & 3) * 57, y = WY + 1 + (i >> 2) * 50, f = FLD[i], v = field_get(f), on = v > 0,
            mine = (i >> 2) == row;
        cv_rect(x, y, 55, 48, on ? RGB(46, 46, 46) : RGB(30, 30, 30));
        if (mine)
            cv_rect(x, y, 55, 2, knob_col(i & 3));
        atmo_icon(i, x + 27, y + 17, on, now);
        text_box(x + 1, y + 32, 53, 1, &FONT_T, &FONT_XS, field_label(f), on ? TH_TEXT : W_GRAY);
        cv_rect(x + 8, y + 44, 39, 2, W_LINE);
        cv_rect(x + 8, y + 44, v * 39 / 100, 2, mine ? knob_col(i & 3) : TH_TEXT);
    }
}

/* GLOBAL: a wooden metronome swinging in time, the tempo big, the metronome's mode */
static void draw_global(void)
{
    char s[16];
    int px = WX + 40, py = WY + WH - 6, i;
    float a = 0.0f;
    static const uint16_t WOOD = RGB(150, 98, 58), WOOD_D = RGB(104, 66, 38);
    {
        char *p = cpy(s, "CPU ");
        p = itoa_b((int)plat_cpu_pct(), p);
        cpy(p, "%");
    }
    module("GLOBAL", s);
    if (dk_playing) {
        float b = (float)(dk_ticks % (2u * DK_PPQ)) / (float)DK_PPQ;   /* there and back, two beats */
        a = 0.45f * fm_cosf(3.1415927f * b);
    }
    for (i = 0; i < 68; i++) {               /* the body: a tall trapezoid */
        int half = 9 + i * 14 / 68;
        hline(px - half, WY + 8 + i, 2 * half, i < 2 ? WOOD_D : WOOD);
    }
    cv_rect(px - 20, py - 2, 40, 4, WOOD_D);
    cv_rect(px - 6, WY + 18, 12, 44, RGB(222, 222, 222));   /* the scale behind the arm */
    for (i = 0; i < 6; i++)
        hline(px - 4, WY + 22 + i * 7, 8, RGB(140, 140, 140));
    {
        int tx = px + (int)(60.0f * fm_sinf(a)), ty = py - 8 - (int)(60.0f * fm_cosf(a));
        int wx = px + (int)(38.0f * fm_sinf(a)), wy = py - 8 - (int)(38.0f * fm_cosf(a));
        cv_line(px, py - 8, tx, ty, RGB(60, 60, 60));
        cv_line(px + 1, py - 8, tx + 1, ty, RGB(60, 60, 60));
        cv_rect(wx - 4, wy - 3, 8, 6, dk_beat_flash ? TH_ENC[2] : RGB(200, 200, 200));
        disc(px, py - 8, 2, RGB(60, 60, 60));
    }
    itoa_b(dk_song.tempo, s);
    text_box(WX + 90, WY + 10, 126, 1, &FONT_L, &FONT_B, s, TH_TEXT);
    text_box(WX + 90, WY + 46, 126, 1, &FONT_T, &FONT_T, "BPM", W_GRAY);
    {
        static const char *const MM[3] = {"METRO OFF", "METRO REC", "METRO ON"};
        text_box(WX + 90, WY + 62, 126, 1, &FONT_T, &FONT_T, MM[dk_metro.mode % 3], dk_metro.mode ? TH_ENC[0] : W_GRAY);
    }
}

/* ------------------------------------------------------------- REC: the tape --- */
static int lay_clip(void);
static void draw_tape(void)
{
    char s[24];
    int i, t, len = dk_part_len ? dk_part_len : DK_TPB, side_b = (dk_playing ? dk_cur_part : dk_song.cur_part) >= 4;
    float pos = dk_playing ? (float)dk_part_tick / (float)len : 0.0f;
    uint32_t now = plat_ms();
    int phase = dk_playing && !dk_counting ? (int)(dk_ticks * 8u / DK_TPB) % 8 : 0;
    if (dk_song.play_mode && dk_song.nroad)
        pos = ((float)dk_road_i + pos) / (float)dk_song.nroad;
    /* the head: REC blinking, the part, the pass */
    if ((now / 500u) & 1u)
        disc(12, 32, 4, TH_REC);
    cv_text(20, 26, &FONT_XS, "REC", TH_REC);
    cv_rect(50, 25, 13, 15, TH_PART[dk_cur_part & 7]);
    s[0] = (char)('A' + (dk_cur_part & 7));
    s[1] = 0;
    cv_text(53, 26, &FONT_XS, s, TH_BG);
    cv_text(68, 26, &FONT_XS, LABEL_NAME[dk_song.part[dk_cur_part & 7].label & 7], TH_TEXT);
    {
        int show = (dk_rec_mask >> (dk_song.cur_track & 7) & 1u) ? dk_song.cur_track : dk_rec_trk;
        char *p = itoa_b(take_live(show), cpy(s, "TAKE "));    /* the take of the track you are on, and its frozen sounds of 7 */
        int used = 0, k;
        for (k = 0; k < DK_NSND; k++)
            used += dk_song.snd[dk_rec_trk % DK_NTRACK][k].used ? 1 : 0;
        p = itoa_b(used, cpy(p, "  L"));
        cpy(p, "/7");
        text_box(130, 26, 102, 2, &FONT_XS, &FONT_XS, s, TH_GRAY);
    }
    /* the cassette */
    cv_rect(14, 44, 212, 100, CS_BODY);
    hline(14, 44, 212, CS_BODY_D);
    hline(14, 143, 212, CS_BODY_D);
    for (i = 0; i < 4; i++)
        disc(i & 1 ? 220 : 20, i & 2 ? 138 : 50, 2, CS_BODY_D);
    cv_rect(24, 50, 192, 62, CS_LABEL);       /* the label */
    for (i = 0; i < 3; i++)
        cv_rect(24, 102 + i * 2, 192, 2, CS_STRIPE[i]);
    text_box(30, 52, 120, 0, &FONT_XS, &FONT_T, dk_song.name, CS_INK);
    text_box(130, 52, 80, 2, &FONT_XS, &FONT_T, dk_metro.mode ? "CLICK ON" : "CLICK OFF",
             dk_metro.mode ? CS_INK : RGB(170, 60, 50));   /* the metronome: K4 */
    (void)side_b;
    cv_rect(54, 66, 132, 32, CS_WINDOW);      /* the window, the reels */
    reel(80, 82, 1.0f - pos, phase);
    reel(160, 82, pos, phase);
    cv_rect(104, 74, 32, 16, CS_TAPE);
    if (dk_counting) {                        /* the count-in, big on the label */
        itoa_b(dk_counting, s);
        cv_rect(108, 68, 24, 28, CS_LABEL);
        text_box(108, 70, 24, 1, &FONT_L, &FONT_B, s, TH_REC);
    }
    {                                         /* the mode and the tracks it records (the armed ones, and the one the take began on) */
        char *p = cpy(s, dk_rec_mode ? "REPLACE " : "OVERDUB ");
        for (i = 0; i < DK_NTRACK; i++)
            if (dk_rec_mask >> i & 1u) {
                *p++ = (char)('1' + i);
                *p++ = ' ';
            }
        *p = 0;
        text_box(24, 113, 100, 0, &FONT_T, &FONT_T, s, CS_INK);
    }
    {                                         /* the bars: where it is of how many (AUTO: counting up) */
        int bars = dk_song.part[dk_cur_part & 7].bars, at = dk_playing ? (int)(dk_part_tick / DK_TPB) + 1 : 1;
        char *p = itoa_b(at, cpy(s, "BAR "));
        if (ui_auto_on)
            cpy(p, " AUTO");
        else
            itoa_b(bars, cpy(p, "/"));
        text_box(120, 113, 90, 2, &FONT_T, &FONT_T, s, ui_auto_on ? TH_REC : CS_INK);
    }
    /* the head: red, a flash on each note recorded */
    {
        static uint32_t hits, flash;
        if (dk_rec_hits != hits) {
            hits = dk_rec_hits;
            flash = now + 120u;
        }
        cv_rect(110, 126, 20, 12, now < flash ? RGB(255, 140, 120) : dk_recording ? TH_REC : CS_BODY_D);
    }
    {                                         /* the track's layers (takes), 1-6 as the white keys REC + white mutes them */
        uint8_t tk[40], sd[40];
        uint16_t cn[40];
        int clip = lay_clip(), n = layers_list(clip, tk, sd, cn, 40), k;
        for (k = 0; k < n && k < 6; k++) {
            int x = 24 + k * 14, y = 128, off = layer_muted(clip, tk[k]);
            char ln[2] = {(char)('1' + k), 0};
            cv_rect(x, y, 12, 10, off ? CS_BODY_D : TH_TRACK[dk_song.cur_track]);
            text_box(x, y + 1, 12, 1, &FONT_XS, &FONT_XS, ln, off ? CS_BODY : TH_BG);
            if (off)
                hline(x + 1, y + 5, 10, CS_BODY);
        }
        if (n > 6)
            text_box(100, 128, 10, 0, &FONT_XS, &FONT_XS, "+", CS_INK);
    }
    /* the six tracks as tapes under it: the current part's notes, this pass's red, the one recording lit */
    for (t = 0; t < DK_NTRACK; t++) {
        int y = 150 + t * 6, cl = dk_cur_part * DK_NTRACK + t;
        const dk_clip_t *c = &dk_song.clip[cl];
        int clen = clip_len(cl);
        if ((dk_rec_mask >> t & 1u) && (dk_recording || dk_counting))
            cv_rect(28, y - 1, 204, 6, dimc(TH_REC, 3));
        cv_rect(8, y + 1, 4, 3, TH_TRACK[t]);
        for (i = 0; i < c->n; i++) {
            const dk_ev_t *e = &dk_ev[c->at + i];
            int x = 30 + e->pos * 200 / clen, w = e->dur * 200 / clen;
            if (e->fl & EV_MOTION)
                continue;
            cv_rect(x, y + 1, w < 2 ? 2 : w, 3, DK_L_TAKE(e->layer) == (unsigned)take_live(t) && (dk_rec_mask >> t & 1u) ? TH_REC : TH_TRACK[t]);
        }
        if (dk_rec_mask >> t & 1u)
            for (i = 0; i < dk_open_n; i++)
                if (dk_open_trk[i] == (uint8_t)t)
                    cv_rect(30 + dk_open_pos[i] * 200 / clen, y + 1, 3, 3, TH_REC);
    }
    if (dk_playing)
        vline(30 + (int)(dk_part_tick % (uint16_t)clip_len(dk_cur_part * DK_NTRACK + dk_rec_trk)) * 200 /
                       clip_len(dk_cur_part * DK_NTRACK + dk_rec_trk), 148, 36, TH_TEXT);
}

/* ----------------------------------------------------------------- TRACKS --- */
/* TRACKS (HOME on PLAY): the six tracks as rows, like a DAW's: the number in the track's colour, its name and sound, the part's
 * mute (M, blinking while it waits for the bar's end), solo (S), the level; to the right the CURRENT PART's clip as a thumbnail
 * (notes of a frozen layer dim, the take being recorded red) and the playhead. The row recording is red. */
/* LAYERS: the list of the selected track's clip in the part now (EDIT on TRACKS); one row per take with notes */
static int lay_clip(void) { return ((dk_playing ? dk_cur_part : dk_song.cur_part) & 7) * DK_NTRACK + dk_song.cur_track; }
static int lay_count(void)
{
    uint8_t tk[40], sd[40];
    uint16_t cn[40];
    return layers_list(lay_clip(), tk, sd, cn, 40);
}
static void lay_do(int w)                /* white key w (0..) on the LAYERS list */
{
    uint8_t tk[40], sd[40];
    uint16_t cn[40];
    int clip = lay_clip(), n = layers_list(clip, tk, sd, cn, 40), i = ui.lay_cur;
    char s[16];
    if (w > 3)
        return;
    if (!n) {
        say("LAYERS", "NO NOTES");
        return;
    }
    if (i >= n)
        i = n - 1;
    s[0] = 'L';
    itoa_b(i + 1, s + 1);
    if (w == 0) {
        layer_mute_toggle(clip, tk[i]);
        say(s, layer_muted(clip, tk[i]) ? "MUTED" : "ON");
    } else if (w == 1) {
        layer_solo_toggle(clip, tk[i]);
        say(s, layer_soloed(clip, tk[i]) ? "SOLO" : "SOLO OFF");
    } else if (w == 2) {
        layer_delete(clip, tk[i]);
        if (ui.lay_cur > 0 && ui.lay_cur >= n - 1)
            ui.lay_cur--;
        say(s, "DELETED");
    } else {
        say("LAYERS", layers_merge(clip) ? "MERGED" : "ONE LAYER");
        ui.lay_cur = 0;
    }
}
static void draw_layers(void)
{
    uint8_t tk[40], sd[40];
    uint16_t cn[40];
    int clip = lay_clip(), n = layers_list(clip, tk, sd, cn, 40), cur = ui.lay_cur, top, r, t = dk_song.cur_track, i;
    const dk_clip_t *c = &dk_song.clip[clip];
    int clen = clip_len(clip);
    char s[40], *p;
    if (cur >= n)
        cur = n ? n - 1 : 0;
    top = cur > 5 ? cur - 5 : 0;
    p = cpy(s, track_label(t));
    p = cpy(p, "  ");
    *p++ = (char)('A' + (((dk_playing ? dk_cur_part : dk_song.cur_part) & 7)));
    *p = 0;
    text_box(4, 20, 232, 0, &FONT_T, &FONT_T, s, TH_TRACK[t]);
    if (!n)
        text_box(4, 90, 232, 1, &FONT_B, &FONT_B, "NO NOTES", TH_GRAY);
    for (r = 0; r < 6 && top + r < n; r++) {
        int k = top + r, ry = 30 + r * 25, muted = layer_muted(clip, tk[k]), solo = layer_soloed(clip, tk[k]), lo = 127, hi = 0, tx = 108,
            tw = 128, dim = muted || (dk_lsolo_clip == clip && !solo);
        uint16_t col = dim ? dimc(TH_TRACK[t], 5) : TH_TRACK[t];
        char *q;
        if (k == cur)
            cv_rect(0, ry, 240, 23, RGB(42, 42, 42));
        q = s;
        *q++ = 'L';
        q = itoa_b(k + 1, q);
        *q++ = ' ';
        q = cpy(q, sd[k] ? TAPE.eng[dk_song.snd[t][sd[k] & 7].eng % TAPE.neng]->name : "LIVE");
        text_box(4, ry + 1, 76, 0, &FONT_T, &FONT_T, s, dim ? TH_GRAY : TH_TEXT);
        q = itoa_b(cn[k], s);
        q = cpy(q, " N  TAKE ");
        itoa_b(tk[k], q);
        text_box(4, ry + 13, 76, 0, &FONT_T, &FONT_T, s, TH_GRAY);
        text_box(72, ry + 1, 10, 0, &FONT_T, &FONT_T, "M", muted ? TH_REC : dimc(TH_GRAY, 6));
        text_box(84, ry + 1, 10, 0, &FONT_T, &FONT_T, "S", solo ? TH_ENC[2] : dimc(TH_GRAY, 6));
        cv_rect(tx, ry + 2, tw, 19, RGB(18, 18, 18));
        for (i = 0; i < c->n; i++) {
            const dk_ev_t *e = &dk_ev[c->at + i];
            if (!(e->fl & EV_MOTION) && (int)DK_L_TAKE(e->layer) == tk[k]) {
                if (e->note < lo)
                    lo = e->note;
                if (e->note > hi)
                    hi = e->note;
            }
        }
        if (hi - lo < 12) {
            lo -= (12 - (hi - lo)) / 2;
            hi = lo + 12;
        }
        for (i = 0; i < c->n; i++) {
            const dk_ev_t *e = &dk_ev[c->at + i];
            int x, w, y;
            if ((e->fl & EV_MOTION) || (int)DK_L_TAKE(e->layer) != tk[k])
                continue;
            x = tx + e->pos * (tw - 2) / clen;
            w = e->dur * (tw - 2) / clen;
            y = ry + 2 + 16 - (e->note - lo) * 16 / (hi - lo);
            if (x + w > tx + tw)
                w = tx + tw - x;
            cv_rect(x, y, w < 2 ? 2 : w, 2, col);
        }
        if (dk_playing)
            vline(tx + (int)(dk_part_tick % (uint16_t)clen) * (tw - 2) / clen, ry, 23, TH_TEXT);
    }
    text_box(4, 183, 232, 0, &FONT_T, &FONT_T, "W1 MUTE  W2 SOLO  W3 DELETE  W4 MERGE", TH_GRAY);
    text_box(4, 195, 232, 0, &FONT_T, &FONT_T, "SEL = LAYER  EDIT = BACK", dimc(TH_GRAY, 8));
}
/* THE EDIT LIST (TRACKS: EDIT): the selected track's clip; one item per white key, two columns. An erase asked once is red. */
#define ED_N 14
static const char *const ED_NAME[ED_N] = {"QUANTIZE", "COPY BAR", "PASTE BAR", "DOUBLE", "VARIATE", "HUMANIZE", "COOK",
                                          "RANDOM", "COPY CLIP", "PASTE CLIP", "LAYERS", "ERASE NOTES", "ERASE AUTO", "ERASE ALL"};
static void draw_edit(void)
{
    int t = dk_song.cur_track, i, ask = ui.ed_ask && plat_ms() - ui.ed_t < 3000u ? ui.ed_ask - 1 : -1;
    char s[40], *p;
    p = cpy(s, track_label(t));
    p = cpy(p, "  ");
    *p++ = (char)('A' + dk_song.cur_part % DK_NPART);
    *p = 0;
    text_box(4, 20, 232, 0, &FONT_T, &FONT_T, s, TH_TRACK[t]);
    for (i = 0; i < ED_N; i++) {
        int x = 4 + i / 7 * 118, y = 34 + i % 7 * 20, er = i >= 11, on = i == ask;
        uint16_t col = on || er ? TH_REC : TH_TRACK[t];
        char n[4];
        cv_rect(x, y, 20, 17, on ? TH_REC : er ? dimc(TH_REC, 4) : RGB(47, 47, 47));
        itoa_b(i + 1, n);
        text_box(x, y + 3, 20, 1, &FONT_T, &FONT_T, n, on ? TH_BG : col);
        text_box(x + 24, y + 3, 90, 0, &FONT_T, &FONT_T, ED_NAME[i], on ? TH_REC : TH_TEXT);
    }
    text_box(4, 183, 232, 0, &FONT_T, &FONT_T, ask >= 0 ? "AGAIN = ERASE  (3 S)" : "W1-14 = ITEM  ERASE: PRESS TWICE", ask >= 0 ? TH_REC : TH_GRAY);
    text_box(4, 195, 232, 0, &FONT_T, &FONT_T, "SEL = TRACK  EDIT = BACK", dimc(TH_GRAY, 8));
}
static int trk_rec(int t) { return (dk_recording || dk_counting) && (dk_rec_mask >> t & 1u); }
static void draw_tracks(void)
{
    if (ui.lay) {
        ui.lay == 2 ? draw_layers() : draw_edit();
        return;
    }
    int t, part = (dk_playing ? dk_cur_part : dk_song.cur_part) & 7, blink = (plat_ms() / 230u) & 1u;
    dk_preset_t tmp;
    char s[40];
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_track_t *k = &dk_song.trk[t];
        int ry = 30 + t * 25, cl = part * DK_NTRACK + t, clen = clip_len(cl), i, lo = 127, hi = 0, tx = 158, tw = 78, sel = t == dk_song.cur_track,
            pm = part_muted(t), pend = part_mute_pending(t), muted = pm, rec = trk_rec(t), n = dk_song.clip[cl].n;
        const dk_clip_t *c = &dk_song.clip[cl];
        uint16_t col = muted ? dimc(TH_TRACK[t], 5) : TH_TRACK[t];
        if (rec)
            cv_rect(0, ry, 240, 23, dimc(TH_REC, 4));
        else if (sel)
            cv_rect(0, ry, 240, 23, RGB(47, 47, 47));
        if (rec || (ui.arm_trk >> t & 1))      /* recording, or armed for it: the red stripe */
            cv_rect(0, rec ? ry : ry + 7, 3, rec ? 23 : 9, TH_REC);   /* (armed: a short dash, recording: the whole row) */
        cv_rect(4, ry + 1, 16, 21, col);
        s[0] = (char)('1' + t);
        s[1] = 0;
        text_box(4, ry + 5, 16, 1, &FONT_S, &FONT_S, s, TH_BG);
        text_box(23, ry + 1, 46, 0, &FONT_XS, &FONT_XS, track_label(t), muted ? TH_GRAY : TH_TEXT);
        tag_box(23, ry + 13, 130, 0, &FONT_T, &FONT_T, preset_blk(k->preset, 1), preset_of(k->eng, k->preset, &tmp)->name, muted ? dimc(TH_ENC[0], 6) : TH_ENC[0], muted ? TH_GRAY : TH_TEXT);
                                             /* the block (short, green), then the name (white) */
        {                                    /* M: the part's mute (or the temporary one), blinking while a change waits; S: solo */
            int show = pend >= 0 && blink ? pend : pm;
            text_box(72, ry + 1, 10, 0, &FONT_T, &FONT_T, "M", show ? TH_REC : dimc(TH_GRAY, 6));
            text_box(84, ry + 1, 10, 0, &FONT_T, &FONT_T, "S", k->solo ? TH_ENC[2] : dimc(TH_GRAY, 6));
        }
        cv_rect(97, ry + 3, 26, 4, RGB(30, 30, 30));       /* the level */
        cv_rect(97, ry + 3, (int)k->vol * 26 / 100, 4, muted ? TH_GRAY : TH_ENC[0]);
        cv_rect(tx, ry + 2, tw, 19, RGB(18, 18, 18));      /* the thumbnail's window */
        for (i = 0; i < n; i++) {
            const dk_ev_t *e = &dk_ev[c->at + i];
            if (e->fl & EV_MOTION)
                continue;
            if (e->note < lo)
                lo = e->note;
            if (e->note > hi)
                hi = e->note;
        }
        if (hi >= lo && hi - lo < 12) {
            lo -= (12 - (hi - lo)) / 2;
            hi = lo + 12;
        }
        for (i = 0; hi > lo && i < n; i++) {
            const dk_ev_t *e = &dk_ev[c->at + i];
            int x, w, y;
            uint16_t nc = col;
            if (e->fl & EV_MOTION)
                continue;
            x = tx + e->pos * (tw - 2) / clen;
            w = e->dur * (tw - 2) / clen;
            y = ry + 2 + 16 - (e->note - lo) * 16 / (hi - lo);
            if (x + w > tx + tw)
                w = tx + tw - x;
            if (DK_L_SND(e->layer))          /* a frozen layer: dim */
                nc = dimc(col, 8);
            if (rec && dk_recording && DK_L_TAKE(e->layer) == (unsigned)take_live(t))
                nc = TH_REC;
            cv_rect(x, y, w < 2 ? 2 : w, 2, nc);
        }
        if (rec)
            for (i = 0; i < dk_open_n; i++)
                if (dk_open_trk[i] == (uint8_t)t)
                    cv_rect(tx + dk_open_pos[i] * (tw - 2) / clen, ry + 2, 2, 19, TH_REC);
        if (dk_playing)                      /* the playhead, across the rows */
            vline(tx + (int)(dk_part_tick % (uint16_t)clen) * (tw - 2) / clen, ry, 23, TH_TEXT);
    }
    text_box(4, 183, 232, 0, &FONT_T, &FONT_T, "SEL/W1-6 TRACK  B1-6 MUTE  B7 SOLO  B8 ARM", TH_GRAY);
    {                                        /* the take's settings: the armed tracks and the mode (SEQ + white arms) */
        char *p = cpy(s, "ARM ");
        int any = 0;
        for (t = 0; t < DK_NTRACK; t++)
            if (ui.arm_trk >> t & 1) {
                *p++ = (char)('1' + t);
                any = 1;
            }
        if (!any)
            *p++ = '-';
        *p++ = ' ';
        *p++ = ' ';
        cpy(cpy(p, dk_rec_mode ? "REPLACE" : "OVERDUB"), "  K3 MODE");
        text_box(4, 195, 232, 0, &FONT_T, &FONT_T, s, TH_GRAY);
    }
}

/* -------------------------------------------------------------------- frame --- */
static void draw_body(void)
{
    fg_live = (uint8_t)(dk_playing && (ui.page == PG_SOUND || ui.page == PG_MIX));   /* a curve's value on the knobs */
    switch (ui.page) {
    case PG_SEQ: draw_seq(); break;
    case PG_SOUND: draw_sound(); break;
    case PG_MIX: draw_mix(); break;
    case PG_ROAD: draw_road(); break;
    case PG_KEYS: draw_keys(); break;
    case PG_PROJECT: draw_project(); break;
    case PG_REC: draw_tape(); break;
    case PG_ARP: draw_seq(); break;
    case PG_GLOB:
        if (ui.row[PG_GLOB] % 7 >= 5)        /* GLOBAL's last two rows: ATMO (the place's layers, the dust, the tape) */
            draw_atmo();
        else
            draw_global();
        break;
    case PG_ATMO: draw_atmo(); break;
    case PG_TRACKS: draw_tracks(); break;
    default: break;
    }
    if (ui.page != PG_PLAY && ui.page != PG_REC && ui.page != PG_TRACKS)
        knob_row(KNOB_Y);                    /* the module's four knobs, on its plate */
    fg_live = 0;
}

/* over the PLAY page's art, before each of its bands goes out: the message (in the band's rows y0 .. y0+h) */
/* SEL: the six tracks in a row, their colours and numbers, the one you are on raised with its name and sound */
static void draw_trackbar(void)
{
    int t, y = 88, cur_t = dk_song.cur_track;
    char s[24];
    if (plat_ms() >= ui.trk_until)
        return;
    cv_rect(10, y, 220, 58, TH_BG);
    hline(10, y, 220, TH_TEXT);
    hline(10, y + 57, 220, TH_TEXT);
    vline(10, y, 58, TH_TEXT);
    vline(229, y, 58, TH_TEXT);
    for (t = 0; t < DK_NTRACK; t++) {
        int cx = 30 + t * 36, on = t == cur_t, cy = y + 18;
        uint16_t c = part_muted(t) ? TH_GRAY : TH_TRACK[t];
        if (on) {
            disc(cx, cy, 11, c);
            circle(cx, cy, 13, TH_TEXT);
        } else
            disc(cx, cy, 7, dimc(c, 9));
        s[0] = (char)('1' + t);
        s[1] = 0;
        text_box(cx - 6, cy - 6, 12, 1, &FONT_T, &FONT_T, s, TH_BG);
    }
    {
        int i = 0;
        const char *a = track_label(cur_t), *b = cur_eng()->name;
        while (*a)
            s[i++] = *a++;
        s[i++] = ' ';
        s[i++] = ' ';
        while (*b && i < 22)
            s[i++] = *b++;
        s[i] = 0;
    }
    text_box(14, y + 36, 212, 1, &FONT_S, &FONT_XS, s, TH_TEXT);
}

static void garden_overlay(int32_t y0, int32_t h)
{
    if (plat_ms() >= ui.msg_until && plat_ms() >= ui.trk_until )
        return;
    cv_oy = -y0;
    cv_y0 = 0;
    cv_y1 = h;
    draw_toast();
    draw_trackbar();
    cv_oy = 0;
}

/* the tape, HOME held alone (0.4 s): the harmony of the track you record, bar by bar */
static int harm_shown(void)
{
    return ui.page == PG_REC && held(B_HOME) && !ui.keys && plat_ms() - ui.down_t[B_HOME] >= 400u;
}
/* the harmony of a clip: the notes sounding at a tick (their names, or the chord's) */
static int clip_notes_at(int clip, int tick, int *out)
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, n = 0;
    for (i = 0; i < c->n && n < 8; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (!(e->fl & EV_MOTION) && e->pos <= tick && e->pos + e->dur > tick)
            out[n++] = e->note;
    }
    return n;
}
static void notes_name(const int *nt, int n, char *b)   /* three and more: the chord; fewer: the notes */
{
    int i;
    b[0] = 0;
    if (n >= 3) {
        dk_chord_name(nt, n, b);
        return;
    }
    for (i = 0; i < n; i++) {
        char *p = b;
        while (*p)
            p++;
        if (i)
            *p++ = ' ';
        cpy(p, DK_NOTE_NAME[nt[i] % 12]);
    }
}
static void bar_harmony(int clip, int b0, char *b)      /* a bar's: its fullest moment (a chord, else its notes) */
{
    const dk_clip_t *c = &dk_song.clip[clip];
    int i, best = -1, bn = 0, nt[8], n, pcs = 0, k = 0, mel[4];
    for (i = 0; i < c->n; i++) {             /* the onset with the most notes sounding */
        const dk_ev_t *e = &dk_ev[c->at + i];
        if ((e->fl & EV_MOTION) || e->pos < b0 || e->pos >= b0 + DK_TPB)
            continue;
        n = clip_notes_at(clip, e->pos, nt);
        if (n > bn)
            bn = n, best = e->pos;
    }
    if (best < 0) {
        n = clip_notes_at(clip, b0, nt);     /* nothing begins here: what still sounds */
        if (!n) {
            cpy(b, "-");
            return;
        }
        notes_name(nt, n, b);
        return;
    }
    if (bn >= 3) {
        n = clip_notes_at(clip, best, nt);
        notes_name(nt, n, b);
        return;
    }
    for (i = 0; i < c->n && k < 3; i++) {    /* a line: its first notes, each once */
        const dk_ev_t *e = &dk_ev[c->at + i];
        if ((e->fl & EV_MOTION) || e->pos < b0 || e->pos >= b0 + DK_TPB || (pcs >> (e->note % 12) & 1))
            continue;
        pcs |= 1 << (e->note % 12);
        mel[k++] = e->note;
    }
    notes_name(mel, k, b);
}
/* the tape, HOME held: the recording track's harmony in this part, bar by bar (what you came up with) */
static void draw_harmony(void)
{
    int p = dk_cur_part & 7, t = dk_rec_trk % DK_NTRACK, clip = p * DK_NTRACK + t, bars = clip_len(clip) / DK_TPB, i,
        x = 8, y = 40, w = 224, rows, h;
    char s[24];
    if (!harm_shown())
        return;
    if (bars > 16)
        bars = 16;
    rows = (bars + 3) / 4;
    h = 34 + rows * 24 + 6;
    cv_rect(x, y, w, h, TH_BG);
    hline(x, y, w, TH_ENC[1]);
    hline(x, y + h - 1, w, TH_ENC[1]);
    vline(x, y, h, TH_ENC[1]);
    vline(x + w - 1, y, h, TH_ENC[1]);
    {
        char *q = cpy(s, track_label(t));
        q = cpy(q, "  PART ");
        *q++ = (char)('A' + p);
        *q = 0;
    }
    text_box(x + 8, y + 7, w - 16, 0, &FONT_S, &FONT_T, s, TH_TEXT);
    for (i = 0; i < bars; i++) {             /* four bars a row: its number, its chord */
        int cx = x + 6 + (i % 4) * 54, cy = y + 32 + (i / 4) * 24,
            now = dk_playing && (int)(dk_part_tick / DK_TPB) == i;
        char nb[4];
        bar_harmony(clip, i * DK_TPB, s);
        itoa_b(i + 1, nb);
        text_box(cx, cy + 4, 12, 0, &FONT_XS, &FONT_XS, nb, now ? TH_ENC[0] : TH_GRAY);
        text_box(cx + 12, cy, 40, 0, &FONT_T, &FONT_XS, s, now ? TH_ENC[0] : TH_TEXT);
    }
}

static void draw_page_band(int band)
{
    band_begin(band * BAND_H, BAND_H);
    draw_header();
    draw_body();
    draw_footer(186);
    draw_toast();
    draw_trackbar();
    draw_harmony();
    band_end();
}

static void draw_frame(void)
{
    static int drawn_page = -1;
    if (ui.page != drawn_page) {          /* the page changed: the WHOLE screen at once (a band a frame left the old page's bands under the
                                           * new one for up to 3 frames, longer when the frames run slow: the garden over TRACKS) */
        int b;
        drawn_page = ui.page;
        if (ui.page == PG_PLAY) {
            ui.garden_t = plat_ms();
            tape_play_draw(28);
            band_begin(0, 28);
            draw_header();
            band_end();
            band_begin(172, 68);
            draw_footer(176);
            band_end();
        } else
            for (b = 0; b < 3; b++)
                draw_page_band(b);
        return;
    }
    if (ui.page == PG_PLAY) {             /* the garden every other frame; the header and the foot between */
        uint32_t now = plat_ms();
        if (now - ui.garden_t >= 70u || now < ui.garden_t) {   /* the garden ~14 times a second, the rest between */
            ui.garden_t = now;
            tape_play_draw(28);              /* (its message drawn into it: garden_overlay) */
        } else {
            band_begin(0, 28);
            draw_header();
            band_end();
            band_begin(172, 68);
            draw_footer(176);
            band_end();
        }
        return;
    }
    draw_page_band((int)(ui.frame % 3u));
}
