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
static int page_rows(int p)
{
    static const uint8_t ROWS[NPG] = {1, 2, 4, 1, 1, 4, 1, 1, 2, 4};
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
    static const uint8_t GLOB_F[4][5] = {{F_TEMPO, F_SWING, F_DUST, F_SLEEP, F_SIDECHAIN},
                                         {F_METRO, F_COUNT, F_CLICK, F_MVOL, F_MDIV},
                                         {F_MIDIOUT, F_CLOCK, F_MIDIIN, F_ONNOTE, F_NONE},
                                         {F_SIDECHAIN, F_ECHOTIME, F_CPU, F_VERSION, F_NONE}};
    (void)home;
    switch (ui.page) {
    case PG_PLAY: {                          /* the garden: the track you play, its sound and its space; ALGO the mood */
        static const uint8_t F[5] = {F_MACRO, F_REV, F_ECHO, F_VOL, F_MOOD};
        return F[i];
    }
    case PG_ARP: {                           /* the arpeggiator's own page */
        static const uint8_t F[2][5] = {{F_ARPMODE, F_ARPDIV, F_ARPRHY, F_ARPLATCH, F_ARPOCT},
                                        {F_ARPOCT, F_ARPGATE, F_ARPSRC, F_ARPON, F_OCT}};
        return F[row & 1][i];
    }
    case PG_SOUND:
        switch (row & 3) {
        case 0:                              /* the engine: its first four, ALGO the fifth (or the drift) */
            if (i < 4)
                return i < e->nparams ? F_P0 + i : i == 3 ? F_DRIFT : F_NONE;
            return e->nparams > 4 ? F_P4 : e->nparams < 4 ? F_NONE : F_DRIFT;
        case 1: {
            static const uint8_t F[5] = {F_ATT, F_DEC, F_REL, F_VOL, F_MACRO};
            return F[i];
        }
        case 2: {
            static const uint8_t F[5] = {F_FXTYPE, F_FXAMT, F_FXA, F_FXB, F_REV};
            return F[i];
        }
        default: {
            static const uint8_t F[5] = {F_MODTYPE, F_MODDEPTH, F_MODRATE, F_MODTGT, F_MODSLOT};
            return F[i];
        }
        }
    case PG_SEQ: {
        int drum = (e->flags & EF_DRUM) != 0;
        if (row == 1) {
            static const uint8_t F[5] = {F_SEQTYPE, F_CLIPLEN, F_TSWING, F_QNT, F_OCT};
            return F[i];
        }
        switch (k->seq) {
        case SQ_STEP:
            if (drum) {
                static const uint8_t F[5] = {F_LANE, F_SVEL, F_SPROB, F_SRATCH, F_STEPPAGE};
                return F[i];
            } else {
                static const uint8_t F[5] = {F_CURSOR, F_SLEN, F_SVEL, F_SPROB, F_SRATCH};
                return F[i];
            }
        case SQ_DRIVE: {
            static const uint8_t F[5] = {F_ENERGY, F_STYLE, F_VAR, F_TSWING, F_CLIPLEN};
            return F[i];
        }
        case SQ_FINGER: {
            static const uint8_t F[5] = {F_FLATCH, F_TSWING, F_OCT, F_CLIPLEN, F_NONE};
            return F[i];
        }
        case SQ_TOMBOLA: {
            static const uint8_t F[5] = {F_SPIN, F_GRAV, F_BOUNCE, F_TQUANT, F_NONE};
            return F[i];
        }
        default: {
            static const uint8_t F[5] = {F_QNT, F_CLIPLEN, F_TSWING, F_OCT, F_NONE};
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
    case PG_GLOB: return GLOB_F[row & 3][i];
    case PG_REC: {
        static const uint8_t F[5] = {F_QNT, F_RECTRK, F_MVOL, F_METRO, F_NONE};
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
        if (k->seq == SQ_STEP)
            return cur_eng()->flags & EF_DRUM ? "WHITE STEPS  BLACK LANES  SEQ ROW" : "KEYS AT CURSOR  SAME NOTE = ERASE";
        if (k->seq == SQ_FINGER)
            return "WHITE PATTERNS  BLACK VARIATIONS";
        if (k->seq == SQ_DRIVE)
            return "HOME+SEQ = BAKE TO STEP";
        if (k->seq == SQ_TOMBOLA)
            return "PLAY TO DROP  HOLD REC = EMPTY";
        return "HOLD SEQ + WHITE = SEQUENCER";
    case PG_SOUND: return "FX = NEXT TAB  HOLD LFO+TURN = MOD";
    case PG_MIX: return "WHITE MUTE  HOLD = CLEAR  BLACK SOLO";
    case PG_ROAD:
        return ui.garage ? "HOLD ENV = BACK TO ROAD" : "WHITE ADD  BLACK TRICKS  REC = LIVE";
    case PG_KEYS: return "EDIT = NEXT ROW  HOLD EDIT = UNDO";
    case PG_PROJECT: return "W1-4 OPEN  W5 NEW  SAVE = SAVE NOW";
    case PG_GLOB: return "GLO = NEXT ROW  HOLD GLO = FILL";
    case PG_REC: return "REC = NEW PASS  HOLD EDIT = UNDO PASS";
    case PG_ARP: return "HOLD NOTES  ARP = ROW  HOME+ARP = OFF";
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
    cv_rect(22, 3, 13, 15, TH_PART[part & 7]);
    s[0] = (char)('A' + (part & 7));
    s[1] = 0;
    cv_text(25, 4, &FONT_XS, s, TH_BG);
    bar = dk_playing ? dk_part_tick / DK_TPB + 1 : 1;
    beat = dk_playing ? dk_part_tick % DK_TPB / DK_PPQ + 1 : 1;
    x = (int)(itoa_b(bar, s) - s);
    s[x] = '.';
    itoa_b(beat, s + x + 1);
    cv_text(40, 4, &FONT_XS, s, TH_GRAY);
    if (plat_ms() < ui.notes_until)      /* what you play, for a moment: its notes (or the drum) */
        text_box(70, 4, 92, 0, &FONT_XS, &FONT_T, ui.notes, TH_ENC[1]);
    else if (ui.page == PG_PLAY) {       /* PLAY: the part's name and the chord, in the header (not on the meadow) */
        int pcs[4], n = dk_harm_n ? dk_harm_n : 1, i;
        char c[12];
        text_box(70, 4, 50, 0, &FONT_XS, &FONT_XS, LABEL_NAME[dk_song.part[part & 7].label & 7], TH_TEXT);
        for (i = 0; i < 4; i++)
            pcs[i] = 60 + dk_harm_pcs[i];
        dk_chord_name(pcs, n, c);
        text_box(122, 4, 42, 1, &FONT_XS, &FONT_T, chord_deg >= 0 || dk_playing ? (have_chord && !dk_playing ? chord_name : c) : "", TH_ENC[0]);
    } else {
        char tl[16];
        int k = (int)(itoa_b(dk_song.cur_track + 1, tl) - tl);
        tl[k] = ' ';
        cpy(tl + k + 1, track_label(dk_song.cur_track));
        text_box(70, 4, 94, 0, &FONT_XS, &FONT_XS, ui.page == PG_SEQ || ui.page == PG_SOUND || ui.page == PG_REC || ui.page == PG_ARP ? tl : PAGE_NAME[ui.page], TH_GRAY);
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
    itoa_b(dk_song.tempo, s);
    text_box(182, 4, 24, 2, &FONT_XS, &FONT_XS, s, TH_TEXT);
    for (t = 0; t < DK_NTRACK; t++) {    /* the six tracks: the one you are on bright, a muted one hollow */
        int cx = 211 + (t % 3) * 9, cy = 6 + (t / 3) * 8;
        uint16_t c = t == dk_song.cur_track ? TH_TEXT : dk_level[t] > 0.01f ? TH_TRACK[t] : TH_GRAY;
        if (dk_song.trk[t].mute)
            circle(cx, cy, 2, TH_GRAY);
        else
            disc(cx, cy, 2, c);
    }
    if (dk_beat_flash || (dk_playing && dk_metro.mode && dk_part_tick % DK_PPQ < DK_PPQ / 6))
        cv_rect(200, 9, 2, 2, TH_ENC[2]);
}

/* the foot: the four knobs K1..K4 (a ring for a number, a dot for a choice; their colours), the hint */
static uint16_t knob_col(int i) { return i < 3 ? TH_ENC[i] : RGB(250, 150, 70); }   /* green blue yellow orange */
static void draw_footer(int y)
{
    int i;
    uint32_t now = plat_ms();
    for (i = 0; i < 4 && ui.page != PG_SOUND; i++) {   /* (SOUND: its knobs are on its plate) */
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
    if (now - ui.page_t < 3000u || held(B_HOME))
        text_box(4, y + 43, 232, 1, &FONT_T, &FONT_T, hint(), dimc(TH_GRAY, 12));
}

/* a VHS-plate's message: what just happened, for 0.6 s */
static void draw_toast(void)
{
    int w, x, y = 96;
    if (plat_ms() >= ui.msg_until)
        return;
    w = text_w(&FONT_B, ui.msg[0]) + (ui.msg[1][0] ? text_w(&FONT_S, ui.msg[1]) + 12 : 0) + 24;
    if (w > 228)
        w = 228;
    x = (240 - w) / 2;
    cv_rect(x, y, w, 30, TH_BG);
    hline(x, y, w, TH_TEXT);
    hline(x, y + 29, w, TH_TEXT);
    vline(x, y, 30, TH_TEXT);
    vline(x + w - 1, y, 30, TH_TEXT);
    if (ui.msg[1][0]) {
        int w0 = text_w(&FONT_B, ui.msg[0]);
        text_box(x + 12, y + 7, w0 < w - 24 ? w0 : w - 24, 0, &FONT_B, &FONT_XS, ui.msg[0], TH_TEXT);
        text_box(x + 12 + w0 + 12, y + 7, w - 36 - w0 > 0 ? w - 36 - w0 : 1, 0, &FONT_S, &FONT_XS, ui.msg[1], TH_ENC[0]);
    } else
        text_box(x + 12, y + 7, w - 24, 1, &FONT_B, &FONT_XS, ui.msg[0], TH_TEXT);
}

/* ------------------------------------------------------------------- SOUND --- */
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
        hline(x0 + 4, by, w - 8, TH_LINE);
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
        cv_rect(x0 + 6, cy - 2, w - 12, 5, TH_LINE);
        cv_rect(x0 + 6, cy - 2, bw, 5, c);
        break;
    }
    }
}

/* the hardware look (the cassette's colours): a plate with its screws, a knob with its value's arc */
static void plate(int x, int y, int w, int h)
{
    cv_rect(x, y, w, h, CS_BODY);
    hline(x, y + h - 1, w, CS_BODY_D);
    vline(x + w - 1, y, h, CS_BODY_D);
    disc(x + 5, y + 5, 2, CS_BODY_D);
    disc(x + w - 6, y + 5, 2, CS_BODY_D);
    disc(x + 5, y + h - 6, 2, CS_BODY_D);
    disc(x + w - 6, y + h - 6, 2, CS_BODY_D);
}
static void reel(int cx, int cy, float amount, int phase)
{
    int i, r = 8 + (int)(amount * 6.0f + 0.5f);   /* the tape wound on it: kept inside the window (r <= 14) */
    disc(cx, cy, 15, RGB(58, 50, 44));       /* the reel itself, the same size full or empty */
    circle(cx, cy, 15, RGB(86, 76, 66));
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
        cv_rect(x - 1, y - 1, 3, 3, t <= frac ? col : CS_BODY_D);
    }
    disc(cx, cy, r, lit ? RGB(64, 58, 52) : CS_WINDOW);
    circle(cx, cy, r, RGB(20, 18, 16));
    a = 2.3561945f + frac * 4.712389f;
    cv_line(cx + (int)((float)(r / 3) * fm_cosf(a)), cy + (int)((float)(r / 3) * fm_sinf(a)),
            cx + (int)((float)(r - 2) * fm_cosf(a)), cy + (int)((float)(r - 2) * fm_sinf(a)), RGB(240, 236, 226));
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
        text_box(cx - 28, y - 32, 56, 1, &FONT_T, &FONT_T, field_label(f), lit ? CS_INK : CS_BODY_D);
        knob(cx, y, 15, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, knob_col(i), lit);
        text_box(cx - 28, y + 22, 56, 1, &FONT_XS, &FONT_T, t, CS_INK);
    }
}
static void draw_sound(void)                /* a synth module: the preset in its window, the tab's picture, 4 knobs */
{
    static const char *const TAB[4] = {"ENG", "ENV", "FX", "MOD"};
    const dk_track_t *k = cur();
    const dk_engine_t *e = cur_eng();
    dk_preset_t tmp;
    int tab = ui.row[PG_SOUND] & 3, i, x = 132, wy = 60, wh = 58;
    uint32_t now = plat_ms();
    for (i = 0; i < 4; i++) {            /* the tabs */
        uint16_t c = i == tab ? TH_TEXT : TH_GRAY;
        cv_text(x, 26, &FONT_XS, TAB[i], c);
        if (i == tab)
            hline(x, 39, text_w(&FONT_XS, TAB[i]), TH_TEXT);
        x += text_w(&FONT_XS, TAB[i]) + 10;
    }
    plate(4, 42, 232, 186);
    text_box(18, 47, 150, 0, &FONT_T, &FONT_T, e->name, CS_INK);   /* the engine's name, as if printed on it */
    cv_rect(10, wy, 220, wh, CS_WINDOW);     /* the window */
    text_box(14, wy + 4, 212, 1, &FONT_B, &FONT_XS, preset_of(k->eng, k->preset, &tmp)->name, TH_TEXT);
    switch (tab) {
    case 0: {                            /* the engine's first knob's picture, small, under the name */
        if (e->nparams) {
            const dk_param_t *pp = &e->params[0];
            float v = pp->hi > pp->lo ? (float)(k->p[0] - pp->lo) / (float)(pp->hi - pp->lo) : 0.0f;
            if (pp->viz == VZ_TYPE && pp->names)
                text_box(14, wy + 32, 212, 1, &FONT_XS, &FONT_T, pp->names[k->p[0] - pp->lo], TH_ENC[0]);
            else
                for (i = 0; i < 200; i += 4)   /* a strip that brightens with it */
                    cv_rect(20 + i, wy + 40, 3, 6, (float)i < v * 200.0f ? TH_ENC[0] : TH_LINE);
        }
        break;
    }
    case 1: {                            /* ENV: one envelope, its three parts coloured */
        int a = 6 + k->env[0] * 50 / 100, d = 10 + k->env[1] * 60 / 100, r = 6 + k->env[2] * 50 / 100;
        int x0 = 20, yb = wy + wh - 4, yt = wy + 26, ys = wy + 38;
        cv_line(x0, yb, x0 + a, yt, TH_ENC[0]);
        cv_line(x0 + a, yt, x0 + a + d, ys, TH_ENC[1]);
        hline(x0 + a + d, ys, 40, TH_TEXT);
        cv_line(x0 + a + d + 40, ys, x0 + a + d + 40 + r, yb, TH_ENC[2]);
        break;
    }
    case 2: {                            /* FX: the sound before (grey) and after (its colours) */
        int px = 16, py = wy + 40, py2 = wy + 40, mid = wy + 40;
        float amt = (float)k->fxamt * 0.01f, fa = (float)k->fxa * 0.01f;
        for (i = 0; i <= 208; i += 2) {
            float sv = fm_sinf((float)i * 0.09f), y2;
            int y = mid - (int)(sv * 12.0f);
            y2 = sv * (1.0f - 0.4f * amt) + (k->fx ? amt * 0.35f * fm_sinf((float)i * (0.6f + fa) + (float)now * 0.003f) : 0.0f);
            if (i) {
                cv_line(px, py, 16 + i, y, TH_LINE);
                cv_line(px, py2, 16 + i, mid - (int)(y2 * 12.0f), TH_ENC[1]);
            }
            px = 16 + i;
            py = y;
            py2 = mid - (int)(y2 * 12.0f);
        }
        break;
    }
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
                cv_line(px, py, 16 + i, y, m[0] == MOD_OFF ? TH_LINE : TH_ENC[1]);
            px = 16 + i;
            py = y;
        }
        break;
    }
    }
    knob_row(166);
}

/* -------------------------------------------------------------------- SEQ --- */
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
        cv_rect(x, y, ww < 2 ? 2 : ww, 2, pass_red && (e->layer & 0x7F) == dk_pass && dk_recording ? TH_REC : col);
    }
}

static void draw_seq(void)
{
    const dk_track_t *k = cur();
    const dk_engine_t *e = cur_eng();
    const dk_clip_t *c = &dk_song.clip[cur_clip()];
    int len = clip_len(cur_clip()), i, x;
    uint32_t now = plat_ms();
    char s[24];
    int view = ui.page == PG_ARP ? SQ_ARP : k->seq;   /* the ARP's page draws its picture here too */
    if (view == SQ_ARP && !dk_arp_on(k))
        cv_text(8, 26, &FONT_XS, "ARP OFF", TH_REC);
    else
        cv_text(8, 26, &FONT_XS, view == SQ_ARP ? "ARP" : SEQ_NAME[k->seq % SQ_N], TH_GRAY);
    if (view != SQ_ARP && dk_arp_on(k))
        text_box(150, 26, 82, 2, &FONT_XS, &FONT_T, "+ ARP", TH_ENC[1]);
    switch (view) {
    case SQ_STEP:
        if (e->flags & EF_DRUM) {            /* the lanes, the steps: on a big dot, off a small grey one */
            int lanes = e->nlanes ? e->nlanes : 8, l, ph = (int)(dk_part_tick % (uint16_t)len) / DK_T16 - ui_step_page * 16;
            x = (int)(itoa_b(ui_step_page + 1, s) - s);
            s[x] = '/';
            itoa_b(len / DK_TPB, s + x + 1);
            text_box(180, 26, 52, 2, &FONT_XS, &FONT_XS, s, TH_GRAY);
            for (l = 0; l < lanes; l++) {
                int y = 48 + l * 16, on_lane = l == ui_lane % lanes;
                text_box(8, y - 5, 40, 0, &FONT_T, &FONT_T, e->lane_name[l], on_lane ? TH_ENC[0] : TH_GRAY);
                for (i = 0; i < 16; i++) {
                    int cx = 54 + i * 11, pos = (ui_step_page * 16 + i) * DK_T16, hit = clip_find(cur_clip(), pos, e->lane_note[l], 0);
                    if (pos >= len)
                        break;
                    if (hit >= 0)
                        disc(cx, y, 4, on_lane ? TH_ENC[0] : TH_TEXT);
                    else
                        cv_rect(cx - 1, y - 1, 2, 2, i % 4 ? TH_LINE : dimc(TH_GRAY, 10));
                }
            }
            if (dk_playing && ph >= 0 && ph < 16)
                vline(54 + ph * 11, 40, 8 * 16 + 4, TH_TEXT);
        } else {                             /* melodic: the bar's sixteenths, the notes by pitch, the cursor */
            int bar = ui_cursor / 16, x0 = 12, w = 216;
            dk_clip_t sub = *c;
            cv_rect(x0 + (ui_cursor % 16) * w / 16, 44, w / 16 - 1, 128, dimc(TH_ENC[0], 4));
            for (i = 0; i <= 16; i++)
                vline(x0 + i * w / 16, 44, i % 4 ? 2 : 6, TH_LINE);
            (void)bar;
            roll(&sub, x0 - bar * w, 52, w * (len / DK_TPB), 118, len, TH_TEXT, 0);
            if (dk_playing) {
                int ph = (int)(dk_part_tick % (uint16_t)len);
                if (ph / DK_TPB == bar)
                    vline(x0 + (ph % DK_TPB) * w / DK_TPB, 44, 128, TH_TEXT);
            }
        }
        break;
    case SQ_LIVE:
    case SQ_ENDLESS:
        roll(c, 12, 50, 216, 116, len, TH_TRACK[dk_song.cur_track], 1);
        hline(12, 168, 216, TH_LINE);
        for (i = 0; i <= len / DK_TPB; i++)
            vline(12 + i * DK_TPB * 216 / (len ? len : 1), 164, 5, TH_GRAY);
        if (dk_playing)
            vline(12 + (int)(dk_part_tick % (uint16_t)len) * 216 / len, 46, 124, TH_TEXT);
        if (!c->n)
            text_box(8, 98, 224, 1, &FONT_S, &FONT_XS, k->seq == SQ_ENDLESS ? "REC, THEN PLAY NOTES" : "REC TO RECORD", TH_GRAY);
        break;
    case SQ_ARP: {                           /* one picture: the rhythm across, each note at its height (the real order) */
        int t = dk_song.cur_track, n = dk_held_n[t], src = (k->arp_opt >> 2) & 1, octs = k->arp[2] < 1 ? 1 : k->arp[2],
            mode = k->arp[0] % 6, total, live, cur_step = dk_arp_rpos[t] & 15, hits = 0;
        const uint8_t *rhy = DK_ARP_RHY[k->arp_rhy % ARPR_N];
        char nm[24];
        if (!n && src && dk_harm_n)
            n = dk_harm_n > 4 ? 4 : dk_harm_n;
        live = n && (dk_playing || dk_held_n[t]);
        total = (n ? n : 3) * (octs > 3 ? 3 : octs);
        text_box(8, 44, 224, 1, &FONT_L, &FONT_B, ARP_NAME[mode], TH_TEXT);
        if (k->arp[3])
            text_box(172, 26, 60, 2, &FONT_XS, &FONT_XS, "LATCH", TH_ENC[0]);
        if (dk_held_n[t]) {                  /* what it plays: the notes held, or the song's chord */
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
        text_box(8, 78, 224, 1, &FONT_XS, &FONT_T, nm, dk_held_n[t] || src ? TH_ENC[1] : TH_GRAY);
        hline(14, 160, 212, TH_LINE);
        for (i = 0; i < 16; i++) {
            int cx = 20 + i * 13, h = rhy[i], idx, y, on = live && i == cur_step;
            uint16_t col = on ? TH_ENC[0] : h == 2 ? TH_TEXT : dimc(TH_TEXT, 10);
            if (!h || (k->arp_rhy == ARPR_SKIP && i % 4 == 3)) {
                cv_rect(cx - 1, 159, 2, 3, on ? TH_ENC[0] : TH_GRAY);
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
            if (mode == 5) {                 /* CHORD: the whole stack each hit */
                int j;
                for (j = 0; j < (n ? n : 3); j++)
                    disc(cx, 150 - j * 10, h == 2 ? 3 : 2, col);
                continue;
            }
            y = 150 - (total > 1 ? idx * 56 / (total - 1) : 0);
            vline(cx, y, 160 - y, on ? TH_ENC[0] : TH_LINE);
            disc(cx, y, h == 2 ? 4 : 3, col);
        }
        break;
    }
    case SQ_DRIVE: {
        text_box(8, 50, 224, 1, &FONT_L, &FONT_B, TAPE.style_name[k->drv[1] % TAPE.nstyle], TH_TEXT);
        for (i = 0; i < 24; i++) {           /* how busy: bars of the energy, wandering with VARIATION */
            uint32_t hh = (uint32_t)i * 2654435761u + (dk_ticks / DK_TPB) * 40503u;
            int h = 4 + (int)((float)k->drv[0] * 0.5f * (0.4f + 0.6f * (float)((hh >> 24) & 255u) / 255.0f * (0.3f + (float)k->drv[2] * 0.007f)));
            cv_rect(14 + i * 9, 166 - h, 5, h, i % 4 == 0 ? TH_ENC[0] : dimc(TH_ENC[0], 9));
        }
        break;
    }
    case SQ_FINGER: {
        const dk_finger_t *set_ = (e->flags & EF_DRUM) ? TAPE.finger_drum : TAPE.finger_mel;
        for (i = 0; i < 16; i++) {           /* the sixteen patterns: held or latched, lit */
            int cx = 8 + (i % 4) * 57, cy = 44 + (i / 4) * 32, on = (dk_finger_latch[dk_song.cur_track] >> i & 1u) != 0;
            uint16_t col = on ? TH_ENC[0] : TH_LINE;
            if (on)
                cv_rect(cx, cy, 54, 28, dimc(TH_ENC[0], 3));
            hline(cx, cy + 27, 54, col);
            if (set_) {                      /* the pattern itself, small: hits per lane, or a melody's contour */
                const dk_finger_t *f = &set_[i];
                uint16_t dc = on ? TH_TEXT : dimc(TH_TEXT, 11);
                int st, l;
                for (st = 0; st < 16; st++) {
                    int x = cx + 3 + st * 3;
                    if (e->flags & EF_DRUM) {
                        for (l = 0; l < 4; l++)
                            if (f->lanes[l][st])
                                cv_rect(x, cy + 3 + l * 6, 2, 4, f->lanes[l][st] > 6 ? dc : dimc(dc, 8));
                    } else if (f->lanes[0][st]) {
                        int len = f->lanes[1][st] ? f->lanes[1][st] : 1, y = cy + 21 - (f->lanes[0][st] - 1) * 2
                                  - (f->lanes[2][st] > 1 ? 4 : 0);
                        if (y < cy + 3)
                            y = cy + 3;
                        cv_rect(x, y, len * 3 - 1 > 51 - st * 3 ? 51 - st * 3 : len * 3 - 1, 2, dc);
                    }
                }
            }
        }
        if (set_ && dk_finger_latch[dk_song.cur_track])   /* the latched one's name, once */
            for (i = 0; i < 16; i++)
                if (dk_finger_latch[dk_song.cur_track] >> i & 1u) {
                    text_box(70, 26, 162, 2, &FONT_XS, &FONT_T, set_[i].name, TH_ENC[0]);
                    break;
                }
        break;
    }
    case SQ_HOLD: {
        int n = dk_held_n[dk_song.cur_track], p = 0;
        s[0] = 0;
        for (i = 0; i < n && i < 4; i++) {
            const char *nm = DK_NOTE_NAME[dk_held[dk_song.cur_track][i] % 12];
            while (*nm)
                s[p++] = *nm++;
            s[p++] = ' ';
        }
        s[p ? p - 1 : 0] = 0;
        text_box(8, 70, 224, 1, &FONT_L, &FONT_XS, n ? s : "PLAY A CHORD", n ? TH_TEXT : TH_GRAY);
        for (i = 0; i < 216; i += 3)          /* the drone: a slow line */
            cv_rect(12 + i, 140 + (int)(4.0f * fm_sinf((float)i * 0.05f + (float)now * 0.001f)), 2, 1, n ? TH_ENC[1] : TH_LINE);
        break;
    }
    case SQ_TOMBOLA: {                       /* the drum turning, the balls in it */
        float a0 = dk_tmb_angle * 6.2831853f;
        int cx = 120, cy = 104, R = 62;
        for (i = 0; i < 6; i++) {
            float a = a0 + (float)i * 1.0471976f, b = a + 1.0471976f;
            cv_line(cx + (int)((float)R * fm_cosf(a)), cy + (int)((float)R * fm_sinf(a)), cx + (int)((float)R * fm_cosf(b)),
                    cy + (int)((float)R * fm_sinf(b)), TH_TEXT);
        }
        for (i = 0; i < dk_tmb_n; i++) {
            int bx = cx + dk_tmb_x[i] * R / 1000, by = cy + dk_tmb_y[i] * R / 1000;
            disc(bx, by, 4, dk_tmb_hit[i] ? TH_ENC[2] : TH_ENC[1]);
        }
        if (!dk_tmb_n)
            text_box(8, 98, 224, 1, &FONT_XS, &FONT_T, "PLAY NOTES TO DROP BALLS", TH_GRAY);
        break;
    }
    default: break;
    }
}

/* -------------------------------------------------------------------- MIX --- */
/* MIX: a little desk on the cassette's plate: a pan knob each, the fader in its slot, the meter's lights, the tape
 * strip with the number at the foot (muted: grey and M; solo: S) */
static void draw_mix(void)
{
    int t;
    char s[16];
    text_box(8, 26, 150, 0, &FONT_XS, &FONT_T, track_label(dk_song.cur_track), TH_TRACK[dk_song.cur_track]);
    itoa_b((int)(dk_master_level * 100.0f), s);
    text_box(160, 26, 72, 2, &FONT_XS, &FONT_T, s, TH_GRAY);
    plate(4, 42, 232, 140);
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_track_t *k = &dk_song.trk[t];
        int x = 10 + t * 37, cx = x + 16, top = 74, h = 72, fy = top + h - (int)k->vol * h / 100, sel = t == dk_song.cur_track,
            lv = (int)(fm_minf(1.0f, dk_level[t] * 3.0f) * 9.0f), i;
        if (sel)
            cv_rect(x, 46, 33, 132, RGB(228, 222, 206));
        knob(cx, 58, 7, (float)(k->pan + 64) / 128.0f, sel ? TH_ENC[1] : CS_BODY_D, 0);
        cv_rect(cx - 1, top, 3, h, CS_WINDOW);       /* the slot */
        for (i = 0; i < 9; i++)                       /* the meter: nine lights, green, then yellow, then red */
            cv_rect(x + 27, top + h - 6 - i * 8, 3, 5,
                    i < lv ? (i < 6 ? TH_ENC[0] : i < 8 ? TH_ENC[2] : TH_REC) : CS_BODY_D);
        cv_rect(cx - 9, fy - 5, 19, 10, sel ? RGB(60, 56, 50) : RGB(96, 90, 82));   /* the cap */
        hline(cx - 9, fy, 19, sel ? TH_ENC[0] : RGB(236, 232, 220));
        cv_rect(x + 3, 156, 27, 16, k->mute ? CS_BODY_D : TH_TRACK[t]);         /* the strip of tape */
        s[0] = (char)('1' + t);
        s[1] = 0;
        if (k->mute)
            s[0] = 'M';
        text_box(x + 3, 157, 27, 1, &FONT_XS, &FONT_T, s, k->mute ? CS_INK : TH_BG);
        if (k->solo)
            text_box(x + 3, 47, 12, 0, &FONT_T, &FONT_T, "S", TH_REC);
    }
}

/* ------------------------------------------------------------------- ROAD --- */
static void draw_road(void)
{
    int i;
    char s[16];
    if (ui.garage) {                        /* the garage: the eight parts, their labels, their lengths, their clips */
        for (i = 0; i < DK_NPART; i++) {
            int y = 30 + i * 18, t, sel = i == ui_gpart;
            cv_rect(10, y, 14, 14, TH_PART[i]);
            s[0] = (char)('A' + i);
            s[1] = 0;
            cv_text(13, y + 1, &FONT_XS, s, TH_BG);
            text_box(32, y + 1, 80, 0, &FONT_XS, &FONT_T, LABEL_NAME[dk_song.part[i].label & 7], sel ? TH_ENC[1] : TH_TEXT);
            itoa_b(dk_song.part[i].bars, s);
            text_box(116, y + 1, 30, 2, &FONT_XS, &FONT_T, s, sel ? TH_ENC[2] : TH_GRAY);
            for (t = 0; t < DK_NTRACK; t++)
                disc(162 + t * 12, y + 7, 3, CLIP(i, t)->n ? TH_TRACK[t] : TH_LINE);
            if (sel)
                hline(10, y + 16, 220, TH_ENC[0]);
        }
        return;
    }
    {   /* the song as a tape: the blocks are pieces of coloured label stuck on it, running under the head */
        int cur_b = dk_playing && dk_song.play_mode ? dk_road_i : ui_road_cur, bi, head = 84, ty = 108, th = 32;
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
        /* the reels, up in the corners, the tape coming down from them */
        plate(10, 28, 220, 62);                /* the cassette: SONG or PART, the blocks, the reels winding */
        text_box(24, 34, 90, 0, &FONT_T, &FONT_T, dk_song.play_mode ? "SONG" : "PART", CS_INK);
        {
            char *p = itoa_b(dk_song.nroad, s);
            cpy(p, dk_song.nroad == 1 ? " BLOCK" : " BLOCKS");
            text_box(114, 34, 102, 2, &FONT_T, &FONT_T, s, CS_INK);
        }
        if (ui.road_rec)
            text_box(24, 34, 90, 0, &FONT_T, &FONT_T, "REC", TH_REC);
        cv_rect(54, 46, 132, 36, CS_WINDOW);
        reel(74, 64, 1.0f - wound, phase);
        reel(166, 64, wound, phase);
        cv_rect(0, ty - 2, 240, th + 4, CS_TAPE);
        for (bi = 0; bi < dk_song.nroad; bi++) {
            const dk_block_t *b = &dk_song.road[bi];
            int x = head + start[bi] - pos, w = start[bi + 1] - start[bi] - 2, on = bi == ui_road_cur;
            uint16_t c = TH_PART[b->part & 7];
            if (x > 240 || x + w < 0)
                continue;
            if (bi < cur_b || (bi == cur_b && 0))
                c = dimc(c, 9);              /* played: a little faded */
            cv_rect(x, ty, w, th, c);
            if (on) {                        /* the block you are on: a frame inside it */
                int j;
                for (j = 0; j < 2; j++) {
                    hline(x + j, ty + j, w - 2 * j, TH_TEXT);
                    hline(x + j, ty + th - 1 - j, w - 2 * j, TH_TEXT);
                    vline(x + j, ty + j, th - 2 * j, TH_TEXT);
                    vline(x + w - 1 - j, ty + j, th - 2 * j, TH_TEXT);
                }
            }
            s[0] = (char)('A' + (b->part & 7));
            s[1] = 0;
            text_box(x + 5, ty + 3, 14, 0, &FONT_XS, &FONT_T, s, TH_BG);
            text_box(x + 5, ty + 19, w - 8 > 4 ? w - 8 : 4, 0, &FONT_T, &FONT_T,
                     LABEL_NAME[dk_song.part[b->part & 7].label & 7], TH_BG);
            if (b->reps > 1) {
                s[0] = 'X';
                itoa_b(b->reps, s + 1);
                text_box(x + w - 26, ty + 4, 22, 2, &FONT_T, &FONT_T, s, TH_BG);
            }
            if (b->tricks || b->drop) {      /* its tricks: a row of dots under it */
                int d = 0, j;
                for (j = 0; j < 7; j++)
                    if ((j < 6 ? b->tricks >> j : b->drop) & 1u)
                        disc(x + 6 + 6 * d++, ty + th + 7, 2, TH_ENC[2]);
            }
        }
        /* the head: where the song is */
        {                                    /* the head: a marker above and under the tape, a hairline across */
            uint16_t hc = dk_playing ? TH_REC : TH_TEXT;
            int j;
            for (j = 0; j < 5; j++) {
                hline(head - 4 + j, ty - 9 + j, 9 - 2 * j, hc);
                hline(head - 4 + j, ty + th + 8 - j, 9 - 2 * j, hc);
            }
            vline(head, ty - 4, th + 8, hc);
        }
        if (!dk_song.nroad)
            text_box(8, 112, 224, 1, &FONT_XS, &FONT_T, "WHITE A-H: ADD A BLOCK", TH_TEXT);
        /* the whole song, small: each block's colour, where you are */
        for (bi = 0; bi < dk_song.nroad && total; bi++) {
            int x0 = 10 + start[bi] * 220 / total, x1 = 10 + start[bi + 1] * 220 / total;
            cv_rect(x0, 166, x1 - x0 - 1 > 1 ? x1 - x0 - 1 : 1, 5, TH_PART[dk_song.road[bi].part & 7]);
        }
        if (total)
            cv_rect(10 + pos * 220 / total - 1, 162, 3, 13, TH_TEXT);
    }
}

/* ------------------------------------------------------------------- KEYS --- */
static void draw_keys(void)
{
    char s[32];
    int i, minor = dk_song.mode == 1 || dk_song.mode == 2 || dk_song.mode == 3 || dk_song.mode == 6 ||
                   dk_song.mode == 7 || dk_song.mode == 9 || dk_song.mode == 10;
    const char *nm = have_chord ? chord_name : "";
    int drum = (TAPE.eng[cur()->eng % TAPE.neng]->flags & EF_DRUM) != 0;
    cv_text(8, 26, &FONT_XS, "KEYS", TH_GRAY);
    text_box(120, 26, 112, 2, &FONT_XS, &FONT_T, dk_song.white ? "WHITE = CHORDS" : "WHITE = NOTES", TH_GRAY);
    if (nm[0] && plat_ms() >= ui.root_until) {
        int w = text2x_w(nm);
        if (w <= 224)
            text2x((240 - w) / 2, 40, nm, TH_TEXT);
        else
            text_box(8, 56, 224, 1, &FONT_L, &FONT_B, nm, TH_TEXT);
    } else {                                 /* nothing played yet: the key itself, big (A MINOR) */
        char *p = cpy(s, DK_NOTE_NAME[dk_song.root % 12]);
        *p++ = ' ';
        cpy(p, DK_MODE_NAME[dk_song.mode % DK_NMODES]);
        text_box(8, 50, 224, 1, &FONT_L, &FONT_B, s, plat_ms() < ui.root_until ? TH_ENC[0] : TH_TEXT);
    }
    {
        char *p = s;
        if (chord_deg >= 0)
            p = cpy(cpy(p, degree_name(chord_deg, minor)), "  ");
        p = cpy(p, DK_NOTE_NAME[dk_song.root % 12]);
        *p++ = ' ';
        cpy(p, DK_MODE_NAME[dk_song.mode % DK_NMODES]);
        text_box(8, 112, 224, 1, &FONT_S, &FONT_T, s, plat_ms() < ui.root_until ? TH_ENC[0] : TH_GRAY);
    }
    for (i = 0; i < 16; i++) {               /* the keys: outlines, the sounding ones filled */
        int x = 9 + i * 14, k = WHITE_K[i], lit = kh[k].trk >= 0 && kh[k].kind == 0 && kh[k].n;
        cv_rect(x, 134, 13, 44, lit ? TH_ENC[0] : TH_BG);
        hline(x, 134, 13, TH_LINE);
        hline(x, 177, 13, TH_LINE);
        vline(x, 134, 44, TH_LINE);
        vline(x + 12, 134, 44, TH_LINE);
        if (!drum && dk_song.layout) {       /* SCALE: each key's note (or the chord's degree), the tonic marked */
            int nt = key_note_of(k, dk_song.cur_track), ton = nt % 12 == dk_song.root % 12 && !dk_song.white;
            char dg[2] = {(char)('1' + i % 7), 0};
            const char *lb = dk_song.white ? dg : DK_NOTE_NAME[nt % 12];
            if (dk_song.white && i % 7 == 0)
                ton = 1;
            text_box(x + 1, 166, 11, 1, &FONT_T, &FONT_T, lb, lit ? TH_BG : ton ? TH_ENC[2] : TH_GRAY);
            if (ton)
                cv_rect(x + 4, 174, 5, 2, lit ? TH_BG : TH_ENC[2]);
        }
    }
    for (i = 0; i < 11; i++) {
        int k = BLACK_K[i], w = white_of(k - 1), x = 9 + w * 14 + 9, lit = kh[k].trk >= 0 && kh[k].n;
        cv_rect(x, 134, 9, 26, lit ? TH_ENC[1] : TH_BG);
        hline(x, 159, 9, TH_GRAY);
        vline(x, 134, 26, TH_GRAY);
        vline(x + 8, 134, 26, TH_GRAY);
    }
}

/* ---------------------------------------------------------------- PROJECT --- */
/* PROJECT: the four projects as cassettes on a shelf; the one open pulled out, its reels turning while it plays */
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
static void mini_cassette(int x, int y, int w, int slot, int open, int sel, int phase)
{
    char nm[12], s[4];
    int used = project_used(slot), h = 58, i;
    s[0] = (char)('1' + slot);
    s[1] = 0;
    if (!used && !open) {                    /* an empty place on the shelf: the outline, its number */
        for (i = 0; i < w; i += 6) {
            hline(x + i, y, 3, TH_LINE);
            hline(x + i, y + h - 1, 3, TH_LINE);
        }
        for (i = 0; i < h; i += 6) {
            vline(x, y + i, 3, TH_LINE);
            vline(x + w - 1, y + i, 3, TH_LINE);
        }
        text_box(x, y + 18, w, 1, &FONT_S, &FONT_XS, s, sel ? TH_ENC[0] : TH_GRAY);
        text_box(x, y + 38, w, 1, &FONT_T, &FONT_T, "EMPTY", TH_LINE);
        return;
    }
    if (open)
        y -= 4;                              /* pulled out of the shelf */
    cv_rect(x, y, w, h, CS_BODY);
    hline(x, y + h - 1, w, CS_BODY_D);
    disc(x + 4, y + 4, 1, CS_BODY_D);
    disc(x + w - 5, y + 4, 1, CS_BODY_D);
    cv_rect(x + 5, y + 6, w - 10, 30, CS_LABEL);
    for (i = 0; i < 3; i++)
        cv_rect(x + 5, y + 30 + i, w - 10, 1, CS_STRIPE[i]);
    text_box(x + 8, y + 8, w - 30, 0, &FONT_XS, &FONT_T, open ? dk_song.name : project_name(slot, nm), CS_INK);
    text_box(x + w - 22, y + 8, 14, 2, &FONT_XS, &FONT_T, s, CS_INK);
    cv_rect(x + w / 2 - 22, y + 40, 44, 12, CS_WINDOW);
    mini_reel(x + w / 2 - 12, y + 46, open ? phase : 0, open);
    mini_reel(x + w / 2 + 12, y + 46, open ? phase : 0, open);
    if (sel) {                               /* K1's choice: a frame */
        hline(x - 2, y - 2, w + 4, TH_ENC[0]);
        hline(x - 2, y + h + 1, w + 4, TH_ENC[0]);
        vline(x - 2, y - 2, h + 4, TH_ENC[0]);
        vline(x + w + 1, y - 2, h + 4, TH_ENC[0]);
    }
}
static void draw_project(void)
{
    char s[40];
    int i, fill = dk_song.nev * 200 / DK_NEV, sel = field_get(F_SLOT),
        phase = dk_playing ? (int)(dk_ticks * 8u / DK_TPB) % 24 : 0;
    hline(6, 98, 228, CS_BODY_D);            /* the shelves */
    hline(6, 99, 228, TH_LINE);
    hline(6, 168, 228, CS_BODY_D);
    hline(6, 169, 228, TH_LINE);
    for (i = 0; i < DK_NPROJ; i++)
        mini_cassette(i & 1 ? 124 : 10, i & 2 ? 108 : 38, 106, i, i == set.proj, i == sel && sel != set.proj, phase);
    if (ui.dirty)                            /* not saved yet: a dot by the open one */
        disc(230, 30, 3, TH_ENC[2]);
    if (export_busy()) {                     /* the export's progress */
        int w = export_pct() * 200 / 100;
        cv_rect(20, 174, 200, 3, TH_LINE);
        cv_rect(20, 174, w, 3, TH_ENC[1]);
    } else if (dk_stems)
        text_box(8, 172, 224, 1, &FONT_T, &FONT_T, "STEMS: RECORD THE OUTPUT", TH_ENC[2]);
    else {
        cv_rect(20, 176, 200, 2, TH_LINE);   /* how full the open one is */
        cv_rect(20, 176, fill > 200 ? 200 : fill, 2, fill > 160 ? TH_REC : dimc(TH_ENC[1], 10));
    }
    (void)s;
}

/* GLOBAL: a wooden metronome swinging in time, the tempo big, the swing and the dust under it */
static void draw_global(void)
{
    char s[16];
    int px = 70, py = 162, i;
    float a = 0.0f;
    static const uint16_t WOOD = RGB(150, 98, 58), WOOD_D = RGB(104, 66, 38);
    if (dk_playing) {
        float b = (float)(dk_ticks % (2u * DK_PPQ)) / (float)DK_PPQ;   /* 0..2: there and back, two beats */
        a = 0.45f * fm_cosf(3.1415927f * b);
    }
    for (i = 0; i < 110; i++) {              /* the body: a tall trapezoid */
        int half = 14 + i * 22 / 110;
        hline(px - half, 52 + i, 2 * half, i < 3 ? WOOD_D : WOOD);
    }
    cv_rect(px - 30, 162, 60, 6, WOOD_D);
    cv_rect(px - 10, 70, 20, 70, RGB(232, 222, 196));   /* the scale behind the arm */
    for (i = 0; i < 7; i++)
        hline(px - 6, 76 + i * 9, 12, RGB(150, 140, 120));
    {
        int tx = px + (int)(96.0f * fm_sinf(a)), ty = py - 14 - (int)(96.0f * fm_cosf(a));
        cv_line(px, py - 14, tx, ty, RGB(60, 60, 60));
        cv_line(px + 1, py - 14, tx + 1, ty, RGB(60, 60, 60));
        {
            int wx = px + (int)(60.0f * fm_sinf(a)), wy = py - 14 - (int)(60.0f * fm_cosf(a));
            cv_rect(wx - 6, wy - 4, 12, 8, dk_beat_flash ? TH_ENC[2] : RGB(200, 200, 200));
        }
        disc(px, py - 14, 3, RGB(60, 60, 60));
    }
    itoa_b(dk_song.tempo, s);
    text_box(120, 60, 112, 1, &FONT_L, &FONT_B, s, TH_TEXT);
    text_box(120, 104, 112, 1, &FONT_XS, &FONT_T, "BPM", TH_GRAY);
    {
        static const char *const MM[3] = {"METRO OFF", "METRO REC", "METRO ON"};
        text_box(120, 126, 112, 1, &FONT_T, &FONT_T, MM[dk_metro.mode % 3], dk_metro.mode ? TH_ENC[0] : TH_GRAY);
    }
    {
        char *p = cpy(s, "CPU ");
        p = itoa_b((int)plat_cpu_pct(), p);
        p = cpy(p, "%  X");
        itoa_b((int)plat_xruns(), p);
        text_box(120, 144, 112, 1, &FONT_T, &FONT_T, s, dimc(TH_GRAY, 12));
    }
}

/* ------------------------------------------------------------- REC: the tape --- */
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
        char *p = cpy(s, "PASS ");
        itoa_b(dk_pass, p);
        text_box(160, 26, 72, 2, &FONT_XS, &FONT_XS, s, TH_GRAY);
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
    text_box(150, 52, 60, 2, &FONT_XS, &FONT_T, side_b ? "SIDE B" : "SIDE A", CS_INK);
    cv_rect(54, 66, 132, 32, CS_WINDOW);      /* the window, the reels */
    reel(80, 82, 1.0f - pos, phase);
    reel(160, 82, pos, phase);
    cv_rect(104, 74, 32, 16, CS_TAPE);
    if (dk_counting) {                        /* the count-in, big on the label */
        itoa_b(dk_counting, s);
        cv_rect(108, 68, 24, 28, CS_LABEL);
        text_box(108, 70, 24, 1, &FONT_L, &FONT_B, s, TH_REC);
    }
    text_box(24, 113, 100, 0, &FONT_T, &FONT_T, TAPE_TITLE, CS_INK);
    itoa_b(dk_song.tempo, s);
    text_box(150, 113, 60, 2, &FONT_T, &FONT_T, s, CS_INK);
    /* the head: red, a flash on each note recorded */
    {
        static uint32_t hits, flash;
        if (dk_rec_hits != hits) {
            hits = dk_rec_hits;
            flash = now + 120u;
        }
        cv_rect(110, 126, 20, 12, now < flash ? RGB(255, 140, 120) : dk_recording ? TH_REC : CS_BODY_D);
    }
    /* the six tracks as tapes under it: the current part's notes, this pass's red, the one recording lit */
    for (t = 0; t < DK_NTRACK; t++) {
        int y = 150 + t * 6, cl = dk_cur_part * DK_NTRACK + t;
        const dk_clip_t *c = &dk_song.clip[cl];
        int clen = clip_len(cl);
        if (t == dk_rec_trk && (dk_recording || dk_counting))
            cv_rect(28, y - 1, 204, 6, dimc(TH_REC, 3));
        cv_rect(8, y + 1, 4, 3, TH_TRACK[t]);
        for (i = 0; i < c->n; i++) {
            const dk_ev_t *e = &dk_ev[c->at + i];
            int x = 30 + e->pos * 200 / clen, w = e->dur * 200 / clen;
            if (e->fl & EV_MOTION)
                continue;
            cv_rect(x, y + 1, w < 2 ? 2 : w, 3, (e->layer & 0x7F) == dk_pass && t == dk_rec_trk ? TH_REC : TH_TRACK[t]);
        }
        if (t == dk_rec_trk)
            for (i = 0; i < dk_open_n; i++)
                cv_rect(30 + dk_open_pos[i] * 200 / clen, y + 1, 3, 3, TH_REC);
    }
    if (dk_playing)
        vline(30 + (int)(dk_part_tick % (uint16_t)clip_len(dk_cur_part * DK_NTRACK + dk_rec_trk)) * 200 /
                       clip_len(dk_cur_part * DK_NTRACK + dk_rec_trk), 148, 36, TH_TEXT);
}

/* -------------------------------------------------------------------- frame --- */
static void draw_body(void)
{
    switch (ui.page) {
    case PG_SEQ: draw_seq(); break;
    case PG_SOUND: draw_sound(); break;
    case PG_MIX: draw_mix(); break;
    case PG_ROAD: draw_road(); break;
    case PG_KEYS: draw_keys(); break;
    case PG_PROJECT: draw_project(); break;
    case PG_REC: draw_tape(); break;
    case PG_ARP: draw_seq(); break;
    case PG_GLOB: draw_global(); break;
    default: break;
    }
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
        uint16_t c = dk_song.trk[t].mute ? TH_GRAY : TH_TRACK[t];
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
    if (plat_ms() >= ui.msg_until && plat_ms() >= ui.trk_until)
        return;
    cv_oy = -y0;
    cv_y0 = 0;
    cv_y1 = h;
    draw_toast();
    draw_trackbar();
    cv_oy = 0;
}

static void draw_page_band(int band)
{
    band_begin(band * BAND_H, BAND_H);
    draw_header();
    draw_body();
    draw_footer(186);
    draw_toast();
    draw_trackbar();
    band_end();
}

static void draw_frame(void)
{
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
