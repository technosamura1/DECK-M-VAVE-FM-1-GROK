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
    static const uint8_t ROWS[NPG] = {1, 2, 4, 2, 1, 5, 6, 1};
    return ROWS[p];
}
/* the three fields of the page (its row): HOME held, another three where a page has them */
static int page_field(int i, int home)
{
    const dk_track_t *k = cur();
    int row = ui.row[ui.page] % page_rows(ui.page);
    static const uint8_t KEYS_F[5][3] = {{F_ROOT, F_MODE, F_CHORD},     {F_WHITE, F_BLACK, F_QUALITY},
                                         {F_VOICING, F_STRUM, F_LEAD},  {F_LAYOUT, F_SPLIT, F_BASS},
                                         {F_KOCT, F_DIM, F_HARMTRK}};
    static const uint8_t PROJ_F[6][3] = {{F_SLOT, F_TEMPLATE, F_TEMPO}, {F_SWING, F_SONGSTYLE, F_DUST},
                                         {F_METRO, F_COUNT, F_CLICK},   {F_MDIV, F_MVOL, F_ONNOTE},
                                         {F_MIDIOUT, F_CLOCK, F_MIDIIN}, {F_ECHOTIME, F_SLEEP, F_CPU}};
    static const uint8_t SOUND_F[4][3] = {{F_P0, F_P1, F_P2}, {F_ATT, F_DEC, F_REL}, {F_FXTYPE, F_FXAMT, F_FXA},
                                          {F_MODTYPE, F_MODDEPTH, F_MODRATE}};
    switch (ui.page) {
    case PG_PLAY: {
        static const uint8_t F[3] = {F_TEMPO, F_MACRO, F_MOOD};
        return F[i];
    }
    case PG_SOUND:
        if (home && row == 2 && i == 2)
            return F_FXB;
        if (home && row == 3)
            return i == 0 ? F_MODSLOT : i == 1 ? F_MODTGT : F_MODRATE;
        if (home && row == 0) {              /* the engine's hidden ones (TYPE, ...) */
            static const uint8_t H[3] = {F_SEQTYPE, F_DRIFT, F_ROLE};
            return H[i];
        }
        return SOUND_F[row & 3][i];
    case PG_SEQ: {
        int drum = (cur_eng()->flags & EF_DRUM) != 0;
        if (row == 1) {
            static const uint8_t R2[3] = {F_SEQTYPE, F_TSWING, F_CLIPLEN};
            if (k->seq == SQ_STEP && i < 2)
                return i == 0 ? F_SRATCH : F_SPROB;
            return R2[i];
        }
        switch (k->seq) {
        case SQ_STEP:
            if (drum) {
                static const uint8_t F[3] = {F_LANE, F_SVEL, F_STEPPAGE};
                return F[i];
            } else {
                static const uint8_t F[3] = {F_CURSOR, F_SLEN, F_SVEL};
                return F[i];
            }
        case SQ_ENDLESS: {
            static const uint8_t F[3] = {F_DIV, F_QNT, F_TSWING};
            return F[i];
        }
        case SQ_ARP: {
            static const uint8_t F[3] = {F_ARPMODE, F_ARPDIV, F_ARPOCT};
            return home && i == 0 ? F_ARPLATCH : F[i];
        }
        case SQ_DRIVE: {
            static const uint8_t F[3] = {F_ENERGY, F_STYLE, F_VAR};
            return F[i];
        }
        case SQ_FINGER: {
            static const uint8_t F[3] = {F_FLATCH, F_TSWING, F_OCT};
            return F[i];
        }
        case SQ_HOLD: {
            static const uint8_t F[3] = {F_OCT, F_DRIFT, F_TSWING};
            return F[i];
        }
        case SQ_TOMBOLA: {
            static const uint8_t F[3] = {F_SPIN, F_GRAV, F_BOUNCE};
            return home && i == 0 ? F_TQUANT : F[i];
        }
        default: {
            static const uint8_t F[3] = {F_QNT, F_CLIPLEN, F_TSWING};
            return F[i];
        }
        }
    }
    case PG_MIX: {
        static const uint8_t F[2][3] = {{F_VOL, F_PAN, F_REV}, {F_ECHO, F_SIDECHAIN, F_SPACE}};
        return F[row & 1][i];
    }
    case PG_ROAD:
        if (ui.garage) {
            static const uint8_t F[3] = {F_GPART, F_GLABEL, F_GBARS};
            return F[i];
        } else {
            static const uint8_t F[3] = {F_ROADCUR, F_ROADPART, F_ROADREPS};
            return home && i == 0 ? F_PLAYMODE : F[i];
        }
    case PG_KEYS: return KEYS_F[row % 5][i];
    case PG_PROJECT: return PROJ_F[row % 6][i];
    case PG_REC: {
        static const uint8_t F[3] = {F_QNT, F_RECTRK, F_MVOL};
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
            return cur_eng()->flags & EF_DRUM ? "WHITE STEPS  BLACK LANES  SEL ROW" : "KEYS AT CURSOR  SAME NOTE = ERASE";
        if (k->seq == SQ_FINGER)
            return "WHITE PATTERNS  BLACK VARIATIONS";
        if (k->seq == SQ_DRIVE)
            return "HOME+SEQ = BAKE TO STEP";
        if (k->seq == SQ_TOMBOLA)
            return "PLAY TO DROP  HOLD REC = EMPTY";
        return "HOLD SEL + WHITE = SEQUENCER";
    case PG_SOUND: return "FX = NEXT TAB  HOLD LFO+TURN = MOD";
    case PG_MIX: return "WHITE MUTE  BLACK SOLO  LFO = ROW";
    case PG_ROAD:
        return ui.garage ? "HOLD ENV = BACK TO ROAD" : "WHITE ADD  BLACK TRICKS  REC = LIVE";
    case PG_KEYS: return "EDIT = NEXT ROW  HOLD EDIT = UNDO";
    case PG_PROJECT: return "W1-4 OPEN  W5 NEW  W6 DEMO  B1 EXPORT";
    case PG_REC: return "REC = NEW PASS  HOLD EDIT = UNDO PASS";
    default: return "";
    }
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
    if (ui.page == PG_PLAY) {            /* PLAY: the part's name and the chord, in the header (not on the meadow) */
        int pcs[4], n = dk_harm_n ? dk_harm_n : 1, i;
        char c[12];
        text_box(70, 4, 60, 0, &FONT_XS, &FONT_XS, LABEL_NAME[dk_song.part[part & 7].label & 7], TH_TEXT);
        for (i = 0; i < 4; i++)
            pcs[i] = 60 + dk_harm_pcs[i];
        dk_chord_name(pcs, n, c);
        text_box(128, 1, 56, 1, &FONT_B, &FONT_XS, chord_deg >= 0 || dk_playing ? (have_chord && !dk_playing ? chord_name : c) : "", TH_ENC[0]);
    } else {
        char tl[16];
        int k = (int)(itoa_b(dk_song.cur_track + 1, tl) - tl);
        tl[k] = ' ';
        cpy(tl + k + 1, track_label(dk_song.cur_track));
        text_box(70, 4, 96, 0, &FONT_XS, &FONT_XS, ui.page == PG_SEQ || ui.page == PG_SOUND || ui.page == PG_REC ? tl : PAGE_NAME[ui.page], TH_GRAY);
    }
    itoa_b(dk_song.tempo, s);
    text_box(184, 4, 22, 2, &FONT_XS, &FONT_XS, s, TH_TEXT);
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

/* the foot: the three fields (a ring for a number, a dot for a choice), the hint */
static void draw_footer(int y)
{
    int i;
    uint32_t now = plat_ms();
    for (i = 0; i < 3; i++) {
        int f = page_field(i, held(B_HOME)), x = 8 + i * 78, lo, hi, v, lit = ui.touched == i && now < ui.touch_until;
        char s[24];
        if (f == F_NONE)
            continue;
        field_range(f, &lo, &hi);
        v = field_get(f);
        field_text(f, s);
        if (hi - lo > 8)
            ring(x + 6, y + 6, 5, (float)(v - lo) / (float)(hi - lo), TH_ENC[i]);
        else
            disc(x + 6, y + 6, 3, TH_ENC[i]);
        text_box(x + 16, y + 1, 56, 0, &FONT_T, &FONT_T, field_label(f), lit ? TH_ENC[i] : TH_GRAY);
        text_box(x, y + 15, 74, 0, &FONT_M, &FONT_XS, s, TH_TEXT);
    }
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

static void draw_sound(void)
{
    static const char *const TAB[4] = {"ENG", "ENV", "FX", "MOD"};
    const dk_track_t *k = cur();
    const dk_engine_t *e = cur_eng();
    dk_preset_t tmp;
    int tab = ui.row[PG_SOUND] & 3, i, x = 132;
    uint32_t now = plat_ms();
    for (i = 0; i < 4; i++) {            /* the tabs */
        uint16_t c = i == tab ? TH_TEXT : TH_GRAY;
        cv_text(x, 26, &FONT_XS, TAB[i], c);
        if (i == tab)
            hline(x, 39, text_w(&FONT_XS, TAB[i]), TH_TEXT);
        x += text_w(&FONT_XS, TAB[i]) + 10;
    }
    text_box(8, 44, 224, 1, &FONT_T, &FONT_T, e->name, TH_GRAY);
    text_box(8, 58, 224, 1, &FONT_L, &FONT_B, preset_of(k->eng, k->preset, &tmp)->name, TH_TEXT);
    switch (tab) {
    case 0:                              /* the engine's three, each its picture in its colour */
        for (i = 0; i < 3 && i < e->nparams; i++) {
            const dk_param_t *pp = &e->params[i];
            float v = pp->hi > pp->lo ? (float)(k->p[i] - pp->lo) / (float)(pp->hi - pp->lo) : 0.0f;
            viz(pp->viz, 8 + i * 76, 100, 72, 70, v, TH_ENC[i], now);
            if (pp->viz == VZ_TYPE && pp->names)
                text_box(8 + i * 76, 126, 72, 1, &FONT_B, &FONT_T, pp->names[k->p[i] - pp->lo], TH_ENC[i]);
        }
        break;
    case 1: {                            /* ENV: one envelope, its three parts coloured */
        int a = 10 + k->env[0] * 60 / 100, d = 20 + k->env[1] * 70 / 100, r = 10 + k->env[2] * 60 / 100;
        int x0 = 14, yb = 168, yt = 108, ys = 140;
        cv_line(x0, yb, x0 + a, yt, TH_ENC[0]);
        cv_line(x0 + a, yt, x0 + a + d, ys, TH_ENC[1]);
        hline(x0 + a + d, ys, 40, TH_TEXT);
        cv_line(x0 + a + d + 40, ys, x0 + a + d + 40 + r, yb, TH_ENC[2]);
        hline(10, yb, 220, TH_LINE);
        break;
    }
    case 2: {                            /* FX: the sound before (grey) and after (its colours) */
        const dk_fx_t *fx = TAPE.fx[k->fx % TAPE.nfx];
        int px = 12, py = 140, py2 = 140;
        float amt = (float)k->fxamt * 0.01f, fa = (float)k->fxa * 0.01f;
        text_box(8, 98, 224, 1, &FONT_B, &FONT_XS, fx->name, TH_ENC[0]);
        for (i = 0; i <= 216; i += 2) {
            float s = fm_sinf((float)i * 0.09f), y2;
            int y = 140 - (int)(s * 18.0f);
            y2 = s * (1.0f - 0.4f * amt) + (k->fx ? amt * 0.35f * fm_sinf((float)i * (0.6f + fa) + (float)now * 0.003f) : 0.0f);
            if (i) {
                cv_line(px, py, 12 + i, y, TH_LINE);
                cv_line(px, py2, 12 + i, 140 - (int)(y2 * 18.0f), TH_ENC[1]);
            }
            px = 12 + i;
            py = y;
            py2 = 140 - (int)(y2 * 18.0f);
        }
        break;
    }
    default: {                           /* MOD: its shape, its depth, its rate; where it goes */
        const uint8_t *m = k->mod[ui_mod_slot];
        int px = 12, py = 140, cyc = 1 + m[2] / 20;
        float depth = (float)m[1] * 0.01f;
        for (i = 0; i <= 216; i += 2) {
            float ph = (float)i / 216.0f * (float)cyc, f = ph - fm_floorf(ph), s;
            int y;
            switch (m[0]) {
            case MOD_DICE: s = (float)((uint32_t)(ph * 4.0f) * 2654435761u >> 24) / 128.0f - 1.0f; break;
            case MOD_PEDAL: s = (float)i / 108.0f - 1.0f; break;
            case MOD_FOLLOW: s = f < 0.1f ? f * 10.0f - 0.0f : 1.0f - (f - 0.1f) * 1.1f; s = s * 2.0f - 1.0f; break;
            case MOD_OFF: s = 0.0f; break;
            default: s = fm_sinf(6.2831853f * ph); break;
            }
            y = 140 - (int)(s * depth * 26.0f);
            if (i)
                cv_line(px, py, 12 + i, y, m[0] == MOD_OFF ? TH_LINE : TH_ENC[1]);
            px = 12 + i;
            py = y;
        }
        text_box(8, 98, 224, 1, &FONT_XS, &FONT_T, TARGET_NAME[m[3] % T_N], TH_ENC[2]);
        break;
    }
    }
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
    cv_text(8, 26, &FONT_XS, SEQ_NAME[k->seq % SQ_N], TH_GRAY);
    switch (k->seq) {
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
    case SQ_ARP: {
        int n = dk_held_n[dk_song.cur_track];
        text_box(8, 50, 224, 1, &FONT_L, &FONT_B, ARP_NAME[k->arp[0] % 6], TH_TEXT);
        for (i = 0; i < 16; i++) {           /* its shape: the steps going up and down */
            static const int8_t SH[6][16] = {{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}, {3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0},
                                             {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 3}, {2, 0, 3, 1, 1, 3, 0, 2, 3, 1, 2, 0, 0, 2, 1, 3},
                                             {0, 2, 1, 3, 0, 2, 1, 3, 0, 2, 1, 3, 0, 2, 1, 3}, {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}};
            int v = SH[k->arp[0] % 6][i], cx = 22 + i * 13;
            disc(cx, 150 - v * 9, 3, i == (int)(dk_clock / 24u % 16u) && n ? TH_ENC[0] : TH_GRAY);
        }
        for (i = 0; i < n && i < 8; i++)
            text_box(8 + i * 28, 102, 28, 1, &FONT_XS, &FONT_T, DK_NOTE_NAME[dk_held[dk_song.cur_track][i] % 12], TH_ENC[1]);
        if (!n)
            text_box(8, 102, 224, 1, &FONT_XS, &FONT_T, k->arp[3] ? "LATCH: PLAY A CHORD" : "HOLD NOTES", TH_GRAY);
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
                cv_rect(cx, cy, 54, 28, dimc(TH_ENC[0], 4));
            hline(cx, cy, 54, col);
            hline(cx, cy + 27, 54, col);
            text_box(cx + 3, cy + 9, 48, 0, &FONT_T, &FONT_T, set_ ? set_[i].name : "", on ? TH_TEXT : TH_GRAY);
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
static void draw_mix(void)
{
    int t;
    char s[16];
    cv_text(8, 26, &FONT_XS, "MASTER", TH_GRAY);
    itoa_b((int)(dk_master_level * 100.0f), s);
    cv_text(60, 26, &FONT_XS, s, TH_TEXT);
    for (t = 0; t < DK_NTRACK; t++) {
        int x = 12 + t * 38, top = 46, h = 110, fy = top + h - (int)dk_song.trk[t].vol * h / 100,
            m = (int)(fm_minf(1.0f, dk_level[t] * 3.0f) * (float)h), sel = t == dk_song.cur_track;
        vline(x + 14, top, h, TH_LINE);
        cv_rect(x + 22, top + h - m, 3, m, dimc(TH_TRACK[t], 8));
        cv_rect(x + 4, fy - 2, 21, 4, sel ? TH_ENC[0] : dk_song.trk[t].mute ? TH_GRAY : TH_TEXT);
        text_box(x, top + h + 6, 36, 1, &FONT_T, &FONT_T, track_label(t), sel ? TH_TEXT : dk_song.trk[t].mute ? TH_LINE : TH_GRAY);
        if (sel)
            hline(x + 4, top + h + 19, 28, TH_TEXT);
        if (dk_song.trk[t].solo)
            cv_text(x + 2, top - 12, &FONT_T, "S", TH_ENC[2]);
        if (dk_song.trk[t].pan)                /* the pan: a tick left or right of the line */
            hline(dk_song.trk[t].pan < 0 ? x + 14 + dk_song.trk[t].pan / 6 : x + 15, top - 3,
                  (dk_song.trk[t].pan < 0 ? -dk_song.trk[t].pan : dk_song.trk[t].pan) / 6 + 1, TH_ENC[1]);
    }
}

/* ------------------------------------------------------------------- ROAD --- */
static void draw_car(int cx, int cy)        /* a small pixel car, from behind */
{
    cv_rect(cx - 9, cy - 6, 18, 7, TH_TEXT);
    cv_rect(cx - 6, cy - 10, 12, 5, TH_TEXT);
    cv_rect(cx - 5, cy - 9, 10, 3, TH_BG);
    cv_rect(cx - 8, cy - 4, 3, 2, TH_REC);
    cv_rect(cx + 5, cy - 4, 3, 2, TH_REC);
    cv_rect(cx - 10, cy + 1, 4, 3, TH_GRAY);
    cv_rect(cx + 6, cy + 1, 4, 3, TH_GRAY);
}
static void draw_road(void)
{
    int i, hy = 44, by = 176;
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
    {
        int cur_b = dk_playing && dk_song.play_mode ? dk_road_i : ui_road_cur, z;
        /* the road from the car to the horizon, the blocks ahead as bands of their parts' colours */
        cv_line(118, hy, 30, by, TH_GRAY);
        cv_line(122, hy, 210, by, TH_GRAY);
        for (z = 5; z >= 0; z--) {
            int bi = cur_b + z, y1, y2, w1, w2, y;
            float f1 = 1.0f / (1.0f + (float)z * 0.7f), f2 = 1.0f / (1.0f + (float)(z + 1) * 0.7f), prog = 0.0f;
            if (dk_playing && dk_song.play_mode && dk_part_len)
                prog = (float)dk_part_tick / (float)dk_part_len;
            f1 = 1.0f / (1.0f + ((float)z - prog) * 0.7f);
            f2 = 1.0f / (1.0f + ((float)z + 1.0f - prog) * 0.7f);
            if (bi >= dk_song.nroad)
                continue;
            y1 = hy + (int)((float)(by - hy) * f1);
            y2 = hy + (int)((float)(by - hy) * f2);
            if (y1 > by)
                y1 = by;
            for (y = y2; y < y1; y++) {
                float f = (float)(y - hy) / (float)(by - hy);
                w1 = (int)(4.0f + f * 176.0f);
                w2 = w1 / 2;
                if (y == y2 || y == y2 + 1)
                    cv_rect(120 - w2, y, w1, 1, bi == ui_road_cur ? TH_ENC[0] : TH_TEXT);
                else if ((y & 1) == 0)
                    cv_rect(120 - w2, y, w1, 1, dimc(TH_PART[dk_song.road[bi].part & 7], 5));
            }
            if (y1 - y2 > 8) {
                const dk_block_t *b = &dk_song.road[bi];
                int ty = (y1 + y2) / 2 - 6;
                s[0] = (char)('A' + (b->part & 7));
                s[1] = 0;
                cv_text(4, ty, &FONT_XS, s, TH_PART[b->part & 7]);
                text_box(16, ty, 60, 0, &FONT_T, &FONT_T, LABEL_NAME[dk_song.part[b->part & 7].label & 7], TH_GRAY);
                if (b->reps > 1) {
                    s[0] = 'X';
                    itoa_b(b->reps, s + 1);
                    cv_text(196, ty, &FONT_T, s, TH_TEXT);
                }
                if (b->tricks || b->drop) {      /* its tricks, in short */
                    char tk[12];
                    int p = 0;
                    if (b->tricks & TR_HALF) tk[p++] = 'H';
                    if (b->tricks & TR_DOUBLE) tk[p++] = 'D';
                    if (b->tricks & TR_REVERSE) tk[p++] = 'R';
                    if (b->tricks & TR_UP) tk[p++] = '+';
                    if (b->tricks & TR_DOWN) tk[p++] = '-';
                    if (b->tricks & TR_STOP) tk[p++] = 'S';
                    if (b->drop) tk[p++] = 'X';
                    tk[p] = 0;
                    cv_text(214, ty, &FONT_T, tk, TH_ENC[2]);
                }
            }
        }
        draw_car(120, by - 2);
        if (!dk_song.nroad)
            text_box(8, 90, 224, 1, &FONT_XS, &FONT_T, "WHITE A-H: ADD A BLOCK", TH_GRAY);
        cv_text(8, 26, &FONT_XS, dk_song.play_mode ? "SONG" : "PART", TH_GRAY);
        if (ui.road_rec)
            cv_text(48, 26, &FONT_XS, "REC", TH_REC);
        itoa_b(dk_song.nroad, s);
        text_box(180, 26, 52, 2, &FONT_XS, &FONT_XS, s, TH_GRAY);
    }
}

/* ------------------------------------------------------------------- KEYS --- */
static void draw_keys(void)
{
    char s[32];
    int i, minor = dk_song.mode == 1 || dk_song.mode == 2 || dk_song.mode == 3 || dk_song.mode == 6 ||
                   dk_song.mode == 7 || dk_song.mode == 9 || dk_song.mode == 10;
    const char *nm = have_chord ? chord_name : "";
    cv_text(8, 26, &FONT_XS, "KEYS", TH_GRAY);
    text_box(120, 26, 112, 2, &FONT_XS, &FONT_T, dk_song.white ? "WHITE = CHORDS" : "WHITE = NOTES", TH_GRAY);
    if (nm[0]) {
        int w = text2x_w(nm);
        if (w <= 224)
            text2x((240 - w) / 2, 40, nm, TH_TEXT);
        else
            text_box(8, 56, 224, 1, &FONT_L, &FONT_B, nm, TH_TEXT);
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
static void draw_project(void)
{
    char s[40], *p;
    int i;
    cv_text(8, 26, &FONT_XS, "PROJECT", TH_GRAY);
    text_box(8, 42, 224, 1, &FONT_M, &FONT_B, dk_song.name, TH_TEXT);
    p = cpy(s, "SLOT ");
    p = itoa_b(set.proj + 1, p);
    p = cpy(p, "   ");
    p = cpy(p, DK_NOTE_NAME[dk_song.root % 12]);
    p = cpy(p, " ");
    cpy(p, DK_MODE_NAME[dk_song.mode % DK_NMODES]);
    text_box(8, 74, 224, 1, &FONT_XS, &FONT_T, s, TH_GRAY);
    p = itoa_b(dk_song.nev, s);
    p = cpy(p, " NOTES   ");
    p = itoa_b(dk_song.nroad, p);
    p = cpy(p, " BLOCKS   ");
    p = itoa_b(dk_song.nev * 100 / DK_NEV, p);
    cpy(p, "% FULL");
    text_box(8, 92, 224, 1, &FONT_XS, &FONT_T, s, TH_GRAY);
    for (i = 0; i < DK_NPROJ; i++) {         /* the four slots */
        char nm[12];
        int x = 14 + i * 54, on = i == set.proj;
        hline(x, 114, 48, on ? TH_ENC[0] : TH_LINE);
        itoa_b(i + 1, s);
        cv_text(x, 120, &FONT_XS, s, on ? TH_TEXT : TH_GRAY);
        text_box(x, 136, 48, 0, &FONT_T, &FONT_T, project_used(i) ? project_name(i, nm) : "EMPTY", on ? TH_TEXT : TH_GRAY);
    }
    if (export_busy()) {                     /* the export's progress */
        int w = export_pct() * 200 / 100;
        cv_rect(20, 160, 200, 3, TH_LINE);
        cv_rect(20, 160, w, 3, TH_ENC[1]);
        cv_text(20, 166, &FONT_T, "EXPORT", TH_ENC[1]);
    } else if (dk_stems) {
        cv_text(20, 162, &FONT_T, "STEMS: RECORD THE OUTPUT", TH_ENC[2]);
    } else {
        p = cpy(s, "CPU ");
        p = itoa_b((int)plat_cpu_pct(), p);
        p = cpy(p, "%   VOICES ");
        p = itoa_b(dk_voices_on, p);
        p = cpy(p, "   ");
        cpy(p, DK_VERSION);
        text_box(8, 162, 224, 1, &FONT_T, &FONT_T, s, dimc(TH_GRAY, 12));
    }
}

/* ------------------------------------------------------------- REC: the tape --- */
static void reel(int cx, int cy, float amount, int phase)
{
    int i, r = 9 + (int)(amount * 14.0f);
    disc(cx, cy, r, CS_TAPE);                /* the tape wound on it */
    disc(cx, cy, 9, CS_LABEL);
    circle(cx, cy, 9, CS_BODY_D);
    disc(cx, cy, 4, CS_WINDOW);
    for (i = 0; i < 6; i++) {                /* the hub's teeth: eight phases a turn */
        float a = 6.2831853f * ((float)i / 6.0f + (float)phase / 48.0f);
        cv_rect(cx + (int)(6.0f * fm_cosf(a)) - 1, cy + (int)(6.0f * fm_sinf(a)) - 1, 2, 2, CS_WINDOW);
    }
}
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
    default: break;
    }
}

static void draw_page_band(int band)
{
    band_begin(band * BAND_H, BAND_H);
    draw_header();
    draw_body();
    draw_footer(186);
    draw_toast();
    band_end();
}

static void draw_frame(void)
{
    if (ui.page == PG_PLAY) {             /* the garden every other frame; the header and the foot between */
        if (ui.frame & 1u) {
            tape_play_draw(28);
            if (plat_ms() < ui.msg_until) {   /* the message over the meadow */
                band_begin(92, 40);
                draw_toast();
                band_end();
            }
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
