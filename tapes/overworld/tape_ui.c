/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's face for DECK (the UI's unity build, from DECK's app.c): its theme (the encoders green, blue and
 * yellow, a warm white), the cassette's colours, and the PLAY page's art: OVERWORLD's garden, as it was (its big
 * flower grown by what is played, its biomes by day and by night, its weather and little lives, the light going round
 * the flower with the loop), with the shell's few joins: the notes of all six tracks grow it (the flower's sense of
 * them by the track's engine, as OVERWORLD's by its instrument), the mood is the biome, the garden is kept in the
 * song; the chord and the part go in the shell's header, not on the meadow. The splash is OVERWORLD's. */

/* ------------------------------------------------------------------- theme --- */
#define TH_BG RGB(0, 0, 0)
#define TH_TEXT RGB(236, 232, 220)
#define TH_GRAY RGB(124, 124, 124)
#define TH_LINE RGB(50, 50, 50)
#define TH_REC RGB(232, 72, 64)
static const uint16_t TH_ENC[3] = {RGB(124, 204, 96), RGB(110, 186, 244), RGB(246, 214, 92)};
/* the parts' colours (A..H) and the tracks' (OVERWORLD's instruments') */
static const uint16_t TH_PART[8] = {RGB(124, 204, 96), RGB(110, 186, 244), RGB(246, 214, 92), RGB(244, 170, 200),
                                    RGB(222, 98, 80),  RGB(170, 140, 240), RGB(150, 214, 170), RGB(250, 160, 90)};
static const uint16_t TH_TRACK[DK_NTRACK] = {RGB(150, 214, 170), RGB(222, 98, 80), RGB(244, 170, 200),
                                             RGB(170, 140, 240), RGB(246, 214, 92), RGB(124, 204, 96)};
/* the cassette (the REC page): a light body, a warm label */
#define CS_BODY RGB(214, 208, 192)
#define CS_BODY_D RGB(170, 162, 146)
#define CS_LABEL RGB(246, 236, 210)
#define CS_INK RGB(90, 70, 50)
#define CS_WINDOW RGB(40, 34, 30)
#define CS_TAPE RGB(70, 50, 36)
static const uint16_t CS_STRIPE[3] = {RGB(124, 204, 96), RGB(246, 214, 92), RGB(244, 170, 200)};
#define TAPE_TITLE "OVERWORLD"

/* ------------------------------------------------------------------ colours --- */
typedef struct {
    uint8_t r, g, b;
} rgb_t;
static inline uint16_t c565(rgb_t c) { return RGB(c.r, c.g, c.b); }
static inline rgb_t mk(int r, int g, int b)
{
    rgb_t c;
    c.r = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
    c.g = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
    c.b = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
    return c;
}
static inline rgb_t mix(rgb_t a, rgb_t b, float t)
{
    return mk((int)((float)a.r + ((float)b.r - (float)a.r) * t), (int)((float)a.g + ((float)b.g - (float)a.g) * t),
              (int)((float)a.b + ((float)b.b - (float)a.b) * t));
}
static inline rgb_t dim(rgb_t a, float k) { return mk((int)((float)a.r * k), (int)((float)a.g * k), (int)((float)a.b * k)); }

/* --------------------------------------------------- OVERWORLD's names, here --- */
enum { I_PIANO, I_PAD, I_BELL, I_BASS, I_DRUM };
enum { D_BD1, D_BD2, D_SD1, D_SD2, D_RIM, D_CLAP, D_OTHER };
#define GARDEN_FLOWER 0                /* the scene's own flower (OVERWORLD's FLOWER AUTO) */
static void garden_overlay(int32_t y0, int32_t h);   /* the shell's (pages.c): what goes over the art */
static int garden_drum_of(int note)
{
    return note == 36 ? D_BD1 : note == 35 ? D_BD2 : note == 38 ? D_SD1 : note == 40 ? D_SD2 : note == 39 ? D_CLAP : D_OTHER;
}
/* a track's notes, as which of OVERWORLD's instruments (by its engine: FELT TINE AIRPAD GLASS SOFTBASS DUSTKIT WORLD
 * KALIMBA); -1 the world, which grows nothing */
static int garden_inst(int trk)
{
    static const int8_t OF[8] = {I_PIANO, I_PIANO, I_PAD, I_BELL, I_BASS, I_DRUM, -1, I_PIANO};
    return OF[dk_song.trk[trk % DK_NTRACK].eng & 7];
}

/* ------------------------------------------------------------------- state --- */
#define NPART 56
typedef struct {
    float x, y, vx, vy;
    int16_t life, life0;
    uint8_t kind;                      /* 0 glint, 2 rain, 3 snow, 4 firefly, 5 sparkle, 6 petal, 7 ember, 8 ripple */
    rgb_t c;
} part_t;
static struct {
    float phase;                       /* time of day: 0 dawn, .25 noon, .5 dusk, .75 midnight */
    float cloud_x;
    part_t pt[NPART];
    uint32_t seen_r, last_note_ms;
} gd;

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

#define GW 120
#define GH 72
#define FCX 60
static uint16_t lfb[GH][GW];           /* the garden at its own scale (2 screen pixels a garden pixel) */

static void gp(int x, int y, rgb_t c)
{
    if ((unsigned)x < GW && (unsigned)y < GH)
        lfb[y][x] = c565(c);
}
static rgb_t gget(int x, int y)
{
    uint16_t c = (unsigned)x < GW && (unsigned)y < GH ? lfb[y][x] : 0;
    return mk((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3);
}
static void gblend(int x, int y, rgb_t c, float a)
{
    if ((unsigned)x < GW && (unsigned)y < GH && a > 0.0f)
        lfb[y][x] = c565(mix(gget(x, y), c, fm_minf(1.0f, a)));
}
static void particle_c(float x, float y, float vx, float vy, int life, int kind, rgb_t c)
{
    int i;
    for (i = 0; i < NPART; i++)
        if (gd.pt[i].life <= 0) {
            gd.pt[i].x = x;
            gd.pt[i].y = y;
            gd.pt[i].vx = vx;
            gd.pt[i].vy = vy;
            gd.pt[i].life = (int16_t)life;
            gd.pt[i].life0 = (int16_t)life;
            gd.pt[i].kind = (uint8_t)kind;
            gd.pt[i].c = c;
            return;
        }
}
static void particle(float x, float y, float vx, float vy, int life, int kind) { particle_c(x, y, vx, vy, life, kind, mk(255, 250, 220)); }

#include "flower.c"
#include "girl.c"

/* the biomes' worlds: a sky by day and by night (top, bottom), the flower's glow, what is in it */
enum { W_SUN = 1, W_CLOUDS = 2, W_STARS = 4, W_MOON = 8, W_AURORA = 16, W_RAIN = 32, W_SNOW = 64, W_EMBERS = 128,
       W_WATER = 256, W_PETALS = 512, W_FIREFLY = 1024, W_BOKEH = 2048, W_DARK = 4096, W_MIST = 8192 };
static const struct {
    uint8_t day[2][3], night[2][3], glow[3];
    uint16_t has;
    int8_t sun_x;
} WORLD[9] = {
    {{{118, 150, 222}, {255, 198, 170}}, {{20, 24, 60}, {72, 60, 112}}, {255, 220, 200}, W_SUN | W_CLOUDS | W_BOKEH | W_STARS | W_MOON, 9},
    {{{150, 200, 242}, {252, 230, 240}}, {{30, 30, 72}, {92, 70, 122}}, {255, 228, 240}, W_CLOUDS | W_PETALS | W_BOKEH | W_STARS | W_MOON, 0},
    {{{88, 102, 130}, {150, 160, 182}}, {{18, 24, 42}, {50, 58, 82}}, {180, 200, 255}, W_RAIN | W_MIST, 0},
    {{{18, 8, 12}, {74, 26, 16}}, {{18, 8, 12}, {74, 26, 16}}, {255, 140, 60}, W_EMBERS | W_DARK | W_BOKEH, 0},
    {{{150, 180, 222}, {232, 240, 250}}, {{24, 34, 72}, {82, 100, 142}}, {230, 240, 255}, W_SNOW | W_STARS | W_MOON | W_MIST, 0},
    {{{70, 168, 202}, {192, 240, 236}}, {{10, 40, 72}, {30, 90, 112}}, {200, 255, 240}, W_WATER | W_CLOUDS | W_STARS | W_MOON, 0},
    {{{6, 8, 30}, {32, 30, 82}}, {{6, 8, 30}, {32, 30, 82}}, {170, 190, 255}, W_STARS | W_MOON | W_FIREFLY | W_DARK, 0},
    {{{4, 10, 30}, {14, 42, 62}}, {{4, 10, 30}, {14, 42, 62}}, {120, 255, 200}, W_STARS | W_AURORA | W_DARK, 0},
    {{{70, 58, 140}, {255, 150, 92}}, {{20, 16, 50}, {92, 40, 62}}, {255, 190, 140}, W_SUN | W_BOKEH | W_STARS | W_FIREFLY, 52},
};
#define WHAS(b, f) (WORLD[b].has & (f))
static int biome(void) { return dk_song.biome % 9; }
static int world_fixed(int bio) { return WHAS(bio, W_DARK) != 0; }

static void garden_hear(void)          /* the notes the tracks played since the last frame */
{
    uint32_t w = dk_seen_w, now = plat_ms();
    int bio = biome();
    if (w - gd.seen_r > DK_SEEN)
        gd.seen_r = w - DK_SEEN;
    while (gd.seen_r != w) {
        uint32_t e = dk_seen[gd.seen_r % DK_SEEN];
        int note = (int)(e & 0x7F), trk = (int)(e >> 8 & 7), vel = (int)(e >> 16 & 0x7F), src = (int)(e >> 24 & 3),
            inst = garden_inst(trk);
        gd.seen_r++;
        if (inst < 0)
            continue;
        flower_note(note, inst, vel, src == 2 ? 2 : src, now);
        if (inst == I_DRUM)
            continue;
        gd.last_note_ms = now;
        if (WHAS(bio, W_WATER) && (inst == I_PIANO || inst == I_BELL))
            particle((float)((note * 37) % GW), (float)(GH - 6 + (note % 4)), 0.0f, 0.0f, 90, 8);
        else if (inst == I_BELL)
            particle((float)((note * 53) % GW), (float)(GH - 4), 0.0f, -0.10f, 200, 0);
    }
}

static float night_of(float ph) { return 0.5f - 0.5f * fm_cosf(6.2831853f * (ph - 0.25f)); }

static void sky_draw(int bio, float nf)
{
    int x, y;
    float tr, tg, tb, br, bg, bb;
    tr = (float)WORLD[bio].day[0][0] + ((float)WORLD[bio].night[0][0] - (float)WORLD[bio].day[0][0]) * nf;
    tg = (float)WORLD[bio].day[0][1] + ((float)WORLD[bio].night[0][1] - (float)WORLD[bio].day[0][1]) * nf;
    tb = (float)WORLD[bio].day[0][2] + ((float)WORLD[bio].night[0][2] - (float)WORLD[bio].day[0][2]) * nf;
    br = (float)WORLD[bio].day[1][0] + ((float)WORLD[bio].night[1][0] - (float)WORLD[bio].day[1][0]) * nf;
    bg = (float)WORLD[bio].day[1][1] + ((float)WORLD[bio].night[1][1] - (float)WORLD[bio].day[1][1]) * nf;
    bb = (float)WORLD[bio].day[1][2] + ((float)WORLD[bio].night[1][2] - (float)WORLD[bio].day[1][2]) * nf;
    for (y = 0; y < GH; y++) {
        float t = (float)y / (float)(GH - 1), r, g, b;
        t = t * t * (3.0f - 2.0f * t);
        r = tr + (br - tr) * t;
        g = tg + (bg - tg) * t;
        b = tb + (bb - tb) * t;
        for (x = 0; x < GW; x++) {
            float d = ((float)BAYER[y & 3][x & 3] - 7.5f) * (1.0f / 16.0f);
            int ri = (int)(r + d * 8.0f), gi = (int)(g + d * 4.0f), bi = (int)(b + d * 8.0f);
            lfb[y][x] = RGB(ri < 0 ? 0 : ri > 255 ? 255 : ri, gi < 0 ? 0 : gi > 255 ? 255 : gi, bi < 0 ? 0 : bi > 255 ? 255 : bi);
        }
    }
}

static void gdisc(float cx, float cy, float r, rgb_t c, float a)
{
    int x, y;
    for (y = (int)(cy - r - 1.0f); y <= (int)(cy + r + 1.0f); y++)
        for (x = (int)(cx - r - 1.0f); x <= (int)(cx + r + 1.0f); x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = dx * dx + dy * dy;
            if (d <= r * r)
                gblend(x, y, c, a);
        }
}

static void world_draw(int bio, float nf, uint32_t now)
{
    int x, y, i;
    float t = (float)now * 0.001f;
    if (WHAS(bio, W_STARS) && nf > 0.35f)
        for (i = 0; i < 46; i++) {
            uint32_t h = hash32((uint32_t)i * 7919u + 17u);
            int sx = (int)(h % GW), sy = (int)((h >> 8) % (GH - 18));
            float tw = (hash32((uint32_t)i + now / 260u) & 3u) ? 1.0f : 0.45f, a = (nf - 0.35f) * 1.6f * tw;
            rgb_t sc = (h >> 20) & 1u ? mk(255, 244, 220) : mk(220, 230, 255);
            gblend(sx, sy, sc, a);
            if ((h & 15u) == 3u && tw > 0.5f) {
                gblend(sx - 1, sy, sc, a * 0.5f);
                gblend(sx + 1, sy, sc, a * 0.5f);
                gblend(sx, sy - 1, sc, a * 0.5f);
                gblend(sx, sy + 1, sc, a * 0.5f);
            }
        }
    if (WHAS(bio, W_AURORA))
        for (x = 0; x < GW; x++) {
            float base = 26.0f + 6.0f * fm_sinf((float)x * 0.07f + t * 0.25f) + 3.0f * fm_sinf((float)x * 0.15f - t * 0.4f);
            float streak = 0.55f + 0.45f * fm_sinf((float)x * 0.9f + t * 1.3f) * fm_sinf((float)x * 0.31f - t * 0.5f);
            for (y = 0; y < (int)base + 3 && y < GH; y++) {
                float up = (base - (float)y) / 22.0f, a;
                rgb_t c;
                if (up < -0.1f || up > 1.0f)
                    continue;
                a = (up < 0.0f ? 1.0f + up * 10.0f : (1.0f - up) * (1.0f - up)) * (0.35f + 0.65f * streak) * 0.75f;
                c = mix(mk(90, 255, 170), mk(170, 110, 255), fm_clampf(up * 1.3f, 0.0f, 1.0f));
                gblend(x, y, c, a);
            }
        }
    if (WHAS(bio, W_SUN) && nf < 0.7f) {
        float sx = (float)(WORLD[bio].sun_x ? WORLD[bio].sun_x * 2 : 18), sy = 52.0f + 14.0f * nf, a = 1.0f - nf / 0.7f;
        gdisc(sx, sy, 17.0f, mk(255, 220, 170), 0.10f * a);
        gdisc(sx, sy, 12.0f, mk(255, 228, 180), 0.16f * a);
        gdisc(sx, sy, 7.5f, mk(255, 240, 200), 0.85f * a);
    }
    if (WHAS(bio, W_MOON) && nf > 0.4f) {
        float mx = 100.0f, my = 13.0f, a = fm_minf(1.0f, (nf - 0.4f) * 2.5f);
        gdisc(mx, my, 11.0f, mk(200, 210, 255), 0.10f * a);
        for (y = -6; y <= 6; y++)
            for (x = -6; x <= 6; x++) {
                float d1 = (float)(x * x + y * y), d2 = (float)((x + 3) * (x + 3) + (y + 2) * (y + 2));
                if (d1 <= 30.0f && d2 > 26.0f)
                    gblend((int)mx + x, (int)my + y, d1 > 22.0f ? mk(210, 216, 240) : mk(246, 246, 236), a);
            }
    }
    if (WHAS(bio, W_CLOUDS))
        for (i = 0; i < 3; i++) {
            static const int8_t CY[3] = {9, 20, 5}, CW[3] = {22, 16, 26};
            float cx = fm_floorf(gd.cloud_x * (0.5f + 0.2f * (float)i) + (float)(i * 53)), a = 0.55f - 0.35f * nf;
            int k;
            cx = cx - fm_floorf(cx / (float)(GW + 60)) * (float)(GW + 60) - 30.0f;
            for (k = 0; k < 4; k++) {
                float ox = (float)(k * CW[i] / 4), r = 4.0f + (float)((k * 7 + i * 3) % 4);
                gdisc(cx + ox, (float)CY[i] - (k == 1 || k == 2 ? 2.0f : 0.0f), r, mix(mk(255, 255, 255), mk(170, 180, 210), nf), a);
                gdisc(cx + ox, (float)CY[i] + 2.0f, r * 0.7f, mix(mk(232, 228, 240), mk(110, 116, 150), nf), a * 0.6f);
            }
        }
    if (WHAS(bio, W_BOKEH))
        for (i = 0; i < 6; i++) {
            uint32_t h = hash32((uint32_t)i * 977u + 5u);
            float bx = (float)(h % GW) + 6.0f * fm_sinf(t * 0.05f + (float)i), by = (float)((h >> 8) % GH) + 3.0f * fm_cosf(t * 0.07f + (float)i);
            gdisc(bx, by, 3.0f + (float)((h >> 16) % 5u), mk(WORLD[bio].glow[0], WORLD[bio].glow[1], WORLD[bio].glow[2]), 0.10f);
        }
    if (WHAS(bio, W_WATER))
        for (y = GH - 14; y < GH; y++)
            for (x = 0; x < GW; x++) {
                float d = (float)(y - (GH - 14)) / 14.0f;
                rgb_t w = mix(mk(40, 120, 150), mk(14, 50, 80), nf);
                gblend(x, y, w, 0.35f + 0.35f * d);
                if (((x + (int)(t * 6.0f) + y * 7) % 23) == 0 && (y & 1))
                    gblend(x, y, mk(230, 250, 255), 0.5f - 0.3f * nf);
            }
    if (WHAS(bio, W_MIST))
        for (y = GH - 22; y < GH; y++) {
            float a = (float)(y - (GH - 22)) / 22.0f * 0.30f;
            for (x = 0; x < GW; x++)
                if ((float)BAYER[y & 3][x & 3] * (1.0f / 16.0f) < a * 2.5f)
                    gblend(x, y, mix(mk(220, 228, 240), mk(90, 100, 130), nf), a);
        }
}

/* the corners a little darker (it draws the eye to the flower): a table made once a biome, then integer shading */
static uint8_t vig[GH][GW] DK_POOL;
static int8_t vig_dark = -1;
static void vignette(int bio)
{
    int x, y, dark = WHAS(bio, W_DARK) != 0;
    if (vig_dark != dark) {
        float k = dark ? 0.55f : 0.30f;
        for (y = 0; y < GH; y++)
            for (x = 0; x < GW; x++) {
                float dx = ((float)x - 59.5f) / 60.0f, dy = ((float)y - 35.5f) / 36.0f, d = dx * dx * 0.8f + dy * dy * 0.6f;
                vig[y][x] = (uint8_t)(d < 0.45f ? 0 : (int)(fm_minf(1.0f, (d - 0.45f) * k) * 255.0f));
            }
        vig_dark = (int8_t)dark;
    }
    for (y = 0; y < GH; y++)
        for (x = 0; x < GW; x++) {
            uint32_t a = vig[y][x], c, m;
            if (!a)
                continue;
            c = lfb[y][x];
            m = 256u - a;
            lfb[y][x] = (uint16_t)((((c >> 11) * m >> 8) << 11) | ((((c >> 5) & 63u) * m >> 8) << 5) | ((c & 31u) * m >> 8));
        }
}

static void garden_draw(void)
{
    uint32_t now = plat_ms();
    int bio = biome(), i;
    float nf = world_fixed(bio) ? 1.0f : night_of(gd.phase), light;
    if (bio == 2)
        nf = fm_maxf(nf, 0.2f);
    light = WHAS(bio, W_DARK) ? 0.62f : 1.0f - 0.58f * nf;
    sky_draw(bio, nf);
    world_draw(bio, nf, now);
    flower_draw(light, now, mk(WORLD[bio].glow[0], WORLD[bio].glow[1], WORLD[bio].glow[2]));
    if (dk_playing && dk_part_len) {           /* the loop: a light going round the flower, red while recording */
        float a = 6.2831853f * ((float)dk_part_tick / (float)dk_part_len - 0.25f), rr = fl_R() + 6.0f;
        int bx = FCX + (int)(rr * fm_cosf(a)), by = FCY + (int)(rr * fm_sinf(a));
        rgb_t c = dk_recording ? mk(255, 90, 80) : mk(255, 232, 140);
        gblend(bx - 1, by, c, 0.45f);
        gblend(bx + 1, by, c, 0.45f);
        gblend(bx, by - 1, c, 0.45f);
        gblend(bx, by + 1, c, 0.45f);
        gp(bx, by, mix(c, mk(255, 255, 255), 0.5f));
    }
    for (i = 0; i < NPART; i++) {
        part_t *p = &gd.pt[i];
        int px = (int)p->x, py = (int)p->y;
        float f = (float)p->life / (float)(p->life0 ? p->life0 : 1);
        if (p->life <= 0)
            continue;
        switch (p->kind) {
        case 0: gblend(px, py, mk(255, 250, 220), 0.4f + 0.6f * f); break;
        case 2:
            gblend(px, py, mk(190, 210, 245), 0.55f);
            gblend(px + 1, py - 1, mk(190, 210, 245), 0.35f);
            gblend(px + 1, py - 2, mk(190, 210, 245), 0.18f);
            break;
        case 3: {
            rgb_t c = mk(252, 252, 255);
            gblend(px, py, c, 0.9f);
            if (i % 3 == 0) {
                gblend(px - 1, py, c, 0.5f);
                gblend(px + 1, py, c, 0.5f);
                gblend(px, py - 1, c, 0.5f);
                gblend(px, py + 1, c, 0.5f);
            }
            break;
        }
        case 4: {
            float b = (hash32(now / 140u + (uint32_t)i) & 3u) ? 1.0f : 0.4f;
            rgb_t c = mk(220, 255, 130);
            gblend(px, py, c, 0.9f * b);
            gblend(px - 1, py, c, 0.3f * b);
            gblend(px + 1, py, c, 0.3f * b);
            gblend(px, py - 1, c, 0.3f * b);
            gblend(px, py + 1, c, 0.3f * b);
            break;
        }
        case 5: {
            int big = f > 0.3f && f < 0.8f;
            rgb_t c = mk(255, 236, 90);
            gp(px, py, big ? mk(255, 255, 230) : c);
            if (f > 0.15f) {
                gp(px - 1, py, c);
                gp(px + 1, py, c);
                gp(px, py - 1, c);
                gp(px, py + 1, c);
            }
            if (big) {
                gblend(px - 2, py, c, 0.6f);
                gblend(px + 2, py, c, 0.6f);
                gblend(px, py - 2, c, 0.6f);
                gblend(px, py + 2, c, 0.6f);
            }
            break;
        }
        case 6: {
            rgb_t c = dim(p->c, 0.55f + 0.45f * light);
            gp(px, py, c);
            gp(px + ((p->life / 20) & 1 ? 1 : 0), py + ((p->life / 20) & 1 ? 0 : 1), dim(c, 0.82f));
            break;
        }
        case 7: gblend(px, py, (p->life & 8) ? mk(255, 210, 100) : mk(255, 130, 50), 0.4f + 0.6f * f); break;
        case 8: {
            int rr = (90 - p->life) / 10 + 1, k;
            for (k = -rr * 2; k <= rr * 2; k++) {
                float e = 1.0f - (float)(k * k) / (float)(4 * rr * rr + 1);
                gblend(px + k, py - (int)(e * 1.5f), mk(230, 250, 255), 0.5f * f);
            }
            break;
        }
        default: break;
        }
    }
    vignette(bio);
}

static void garden_step(uint32_t dt)
{
    int i, bio = biome(), n = 0;
    float k = (float)dt * (1.0f / 16.0f), nf;
    uint32_t now = plat_ms();
    gd.phase += (float)dt * (dk_playing ? 1.0f : 0.3f) / 1200000.0f;   /* the day turns: twenty minutes of play */
    if (gd.phase >= 1.0f)
        gd.phase -= 1.0f;
    nf = world_fixed(bio) ? 1.0f : night_of(gd.phase);
    gd.cloud_x += 0.010f * k;
    if (gd.cloud_x > 4000.0f)
        gd.cloud_x -= 4000.0f;
    for (i = 0; i < NPART; i++) {
        part_t *p = &gd.pt[i];
        if (p->life <= 0)
            continue;
        n += p->kind == 2 || p->kind == 3 || p->kind == 4 || p->kind == 7;
        p->x += p->vx * k;
        p->y += p->vy * k;
        if (p->kind == 3 || p->kind == 6)
            p->x += 0.03f * fm_sinf((float)(now + (uint32_t)i * 700u) * 0.003f) * k;
        if (p->kind == 4) {
            p->vx += ((float)(hash32(now / 50u + (uint32_t)i) & 255u) - 127.5f) * 0.00004f * k;
            p->vy += ((float)(hash32(now / 50u + (uint32_t)i * 3u) & 255u) - 127.5f) * 0.00004f * k;
            p->vx = fm_clampf(p->vx, -0.05f, 0.05f);
            p->vy = fm_clampf(p->vy, -0.04f, 0.04f);
        }
        p->life = (int16_t)(p->life - (int)(k + 0.5f));
        if (p->kind == 2 && bloom_on()) {
            float dx = p->x - (float)FCX, dy = p->y - (float)FCY, rr = fl_R();
            if (dx * dx + dy * dy < rr * rr * 0.8f) {
                p->life = 0;
                if ((hash32(now + (uint32_t)i) & 3u) == 0)
                    particle(p->x, p->y - 1.0f, 0.0f, -0.03f, 14, 0);
            }
        }
        if (p->x < -3.0f || p->x > (float)GW + 3.0f || p->y < -3.0f || p->y > (float)GH + 2.0f)
            p->life = 0;
    }
    if (WHAS(bio, W_RAIN) && n < 30 && (hash32(now) & 1u))
        particle((float)(hash32(now * 3u) % (GW + 20)), -1.0f, -0.18f, 1.0f, 120, 2);
    if (WHAS(bio, W_SNOW) && n < 24 && (hash32(now) & 7u) == 0)
        particle((float)(hash32(now * 5u) % GW), -1.0f, 0.0f, 0.10f, 900, 3);
    if (WHAS(bio, W_EMBERS) && n < 14 && (hash32(now * 13u) & 7u) == 0)
        particle((float)(hash32(now) % GW), (float)GH, 0.0f, -0.07f - 0.04f * (float)(hash32(now * 7u) & 3u), 900, 7);
    if (WHAS(bio, W_FIREFLY) && nf > 0.6f && n < 9 && (hash32(now) & 31u) == 0)
        particle((float)(hash32(now * 7u) % GW), (float)(30 + (int)(hash32(now * 11u) % 40u)), 0.0f, 0.0f, 1100, 4);
    if (fl.kind == SP_FIRELILY && bloom_on() && (hash32(now * 17u) & 15u) == 0)
        particle((float)FCX + (float)((int)(hash32(now) % 21u) - 10), (float)FCY - 6.0f, 0.0f, -0.06f, 180, 7);
    if (WHAS(bio, W_PETALS) && (hash32(now * 17u) & 63u) == 0)
        particle_c((float)GW + 1.0f, (float)(hash32(now) % 50u), -0.11f, 0.04f, 1400, 6, mk(252, 210, 226));
    flower_step(dt, now);
    if (fl.bio != bio)
        flower_scene(bio);
}

/* the garden at twice its size, from screen row y0: two bands (the canvas holds 240 x 80) */
static void garden_blit(int32_t y0)
{
    int x, y, half;
    for (half = 0; half < 2; half++) {
        cv_begin(240, 72, 0);
        for (y = 0; y < GH / 2; y++) {
            uint16_t *row = &cv_px[(uint32_t)(y * 2) * 240u], *d = row + 240u;
            for (x = 0; x < GW; x++) {
                uint16_t c = swap16(lfb[y + half * (GH / 2)][x]);
                row[x * 2] = c;
                row[x * 2 + 1] = c;
            }
            for (x = 0; x < 240; x++)
                d[x] = row[x];
        }
        garden_overlay(y0 + half * GH, GH);      /* the shell's message, in the same picture (no flicker) */
        cv_blit(0, (uint32_t)(y0 + half * GH));
    }
}

/* ------------------------------------------------------------ the shell's joins --- */
static void tape_ui_init(void)
{
    gd.phase = dk_song.garden[0] > 0.0f && dk_song.garden[0] < 1.0f ? dk_song.garden[0] : (biome() == 8 ? 0.42f : 0.06f);
    flower_init(biome());
    if (dk_song.garden[1] > 0.0f)                  /* the flower as it was left */
        fl.grow = fl.shown = fm_clampf(dk_song.garden[1], 0.0f, 3.0f);
    gd.seen_r = dk_seen_w;
}
static void tape_ui_keep(void)                      /* into the song (saved with it) */
{
    dk_song.garden[0] = gd.phase;
    dk_song.garden[1] = fl.grow;
    dk_song.garden[2] = (float)fl.kind;
}
static void tape_ui_step(uint32_t dt)
{
    garden_hear();
    garden_step(dt);
}
/* the PLAY page's art: the garden, rows y0 .. y0 + 143 */
static void tape_play_draw(int32_t y0)       /* (the garden still makes the splash) */
{
    girl_draw(y0, plat_ms());
}
/* the mood changed: its biome */
static void tape_mood(int m)
{
    (void)m;
    if (biome() == 8)
        gd.phase = 0.42f;
    else if (gd.phase > 0.55f || gd.phase < 0.02f)
        gd.phase = 0.06f;
}

/* the splash: OVERWORLD's, a cherry blossom open in the morning, the name over it */
void app_splash(void)
{
    uint8_t keep = dk_song.biome;
    gd.phase = 0.07f;
    flower_init(1);
    flower_make(1, SP_CHERRY, 1);
    fl.grow = fl.shown = 3.0f;
    dk_song.biome = 1;
    garden_draw();
    dk_song.biome = keep;
    lcd_fill(0, 0, 240, 240, TH_BG);
    garden_blit(48);
    cv_begin(240, 40, TH_BG);
    cv_text((240 - text_w(&FONT_L, TAPE_TITLE)) / 2, 4, &FONT_L, TAPE_TITLE, RGB(120, 196, 92));
    cv_blit(0, 6);
    cv_begin(240, 40, TH_BG);
    cv_text((240 - text_w(&FONT_S, "A GARDEN THAT LISTENS")) / 2, 4, &FONT_S, "A GARDEN THAT LISTENS", TH_GRAY);
    cv_text((240 - text_w(&FONT_T, "DECK")) / 2, 24, &FONT_T, "DECK", TH_LINE + 0x2104u);
    cv_blit(0, 196);
    flower_init(keep % 9);
}
