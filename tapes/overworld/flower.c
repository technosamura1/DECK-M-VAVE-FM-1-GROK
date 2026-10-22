/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD contributors */
/* The flower: one big bloom seen from the front, filling the garden, grown by what is played. Part of the unity build
 * (in ui.c, after the garden's pixels and particles).
 *
 * Notes grow it: its leaves unfurl round a small green bud, the bud swells and takes its colour, then the petals open
 * ring by ring, the outer first. Each note lights a stamen by its pitch and a petal; a chord runs a wave round the
 * petals; the pad lays a halo behind it, the bells scatter sparkles, the bass lights the leaves, the kick makes it
 * pulse; the louder the music the more it breathes. It turns, slowly. Left in silence it holds a while, then drops
 * its petals one by one and a new bud comes, another of its kind (a few petals more or fewer, another tint). Each
 * scene its own flower: a rose, a cherry blossom, a bluebell, a fire lily, an edelweiss, a lotus, a moonflower, a
 * frost star, a dahlia (and now and then a poppy or a daisy); FLOWER (ATMOS) picks one for good.
 *
 * Drawn in polar terms, a pixel at a time: which petal (of which ring, or which leaf) is on top there, how far along
 * it, how far off its middle; then a second pass gives the edges their lines, the upper-left edges their light, the
 * petals under others their shadow, and the faces their shade (a 4x4 ordered dither between the palette's steps), the
 * way the pixel artists do it by hand. */

enum { SH_ROUND, SH_NOTCH, SH_POINT, SH_FLAME, SH_FRINGE, SH_FUSED, SH_LEAF, NSHAPES };
enum { CEN_DISK, CEN_FLORETS, CEN_POD, CEN_SPIRAL };
enum { SP_ROSE, SP_CHERRY, SP_BLUEBELL, SP_FIRELILY, SP_EDELWEISS, SP_LOTUS, SP_MOON, SP_POPPY, SP_DAISY, SP_FROST,
       SP_DAHLIA, NSPECIES };

typedef struct {
    uint8_t shape, layers;             /* petal rings, the outer first */
    uint8_t npet[4];
    float rad[4], hw[4], rot[4];       /* each ring's reach (of R), petals' half-width (of R), turn (of a petal) */
    float twist;                       /* a curl along the petal (the fire lily's flames) */
    uint8_t invert;                    /* lighter at the base than at the tips */
    uint8_t glow;                      /* lights itself: no night on it, a halo */
    uint8_t center;
    float core;                        /* the centre's radius, of R */
    uint8_t nst;                       /* stamens */
    float stl;                         /* their reach, of R */
    uint8_t pal[6][3];                 /* outline, deep, mid, light, pale, highlight */
    uint8_t cen[3][3];                 /* the centre: dark, mid, light */
    uint8_t fil[3], anth[2][3];        /* stamens: the filament, the anthers */
    uint8_t halo[3];
    uint8_t vein;                      /* a light midrib down each petal */
} species_t;

static const species_t SPECIES[NSPECIES] = {
    /* ROSE: four rings wound tight, a dark swirl in the middle */
    {SH_ROUND, 4, {5, 5, 5, 4}, {1.0f, 0.80f, 0.60f, 0.40f}, {0.62f, 0.60f, 0.58f, 0.55f}, {0.0f, 0.5f, 0.2f, 0.75f}, 0.0f,
     0, 0, CEN_SPIRAL, 0.16f, 0, 0.0f,
     {{120, 14, 40}, {170, 24, 56}, {210, 44, 76}, {236, 82, 108}, {248, 132, 150}, {255, 196, 204}},
     {{110, 10, 34}, {150, 20, 48}, {196, 40, 70}}, {0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {255, 150, 170}, 0},
    /* CHERRY: five notched petals, white at the tips, a red heart and its stamens */
    {SH_NOTCH, 2, {5, 5}, {1.0f, 0.78f}, {0.70f, 0.62f}, {0.0f, 0.5f}, 0.0f, 0, 0, CEN_DISK, 0.17f, 18, 0.56f,
     {{240, 150, 186}, {244, 172, 202}, {248, 196, 218}, {252, 218, 232}, {255, 236, 244}, {255, 250, 252}},
     {{196, 8, 58}, {226, 30, 84}, {244, 70, 120}}, {226, 40, 96}, {{255, 214, 60}, {255, 150, 40}}, {255, 200, 225}, 0},
    /* BLUEBELL: many narrow fringed petals in two rings */
    {SH_FRINGE, 2, {10, 9}, {1.0f, 0.66f}, {0.30f, 0.32f}, {0.0f, 0.5f}, 0.0f, 0, 0, CEN_DISK, 0.22f, 10, 0.40f,
     {{30, 44, 140}, {52, 78, 190}, {78, 118, 226}, {110, 154, 244}, {152, 190, 252}, {210, 226, 255}},
     {{50, 20, 90}, {84, 40, 140}, {120, 70, 180}}, {96, 56, 156}, {{236, 236, 255}, {176, 146, 240}}, {150, 180, 255}, 1},
    /* FIRE LILY: curling flames, bright at the core, glowing in the dark */
    {SH_FLAME, 3, {6, 6, 5}, {1.0f, 0.76f, 0.52f}, {0.40f, 0.38f, 0.36f}, {0.0f, 0.5f, 0.2f}, 0.35f, 1, 1, CEN_DISK, 0.14f, 8,
     0.30f, {{120, 24, 8}, {190, 52, 16}, {232, 96, 24}, {250, 150, 40}, {255, 206, 90}, {255, 244, 180}},
     {{255, 250, 210}, {255, 226, 120}, {255, 180, 60}}, {255, 200, 80}, {{255, 255, 220}, {255, 230, 120}}, {255, 140, 40}, 1},
    /* EDELWEISS: felted white stars round a cluster of little yellow florets */
    {SH_POINT, 2, {9, 7}, {1.0f, 0.70f}, {0.28f, 0.30f}, {0.0f, 0.5f}, 0.0f, 0, 0, CEN_FLORETS, 0.25f, 0, 0.0f,
     {{124, 140, 166}, {178, 190, 210}, {206, 214, 228}, {226, 232, 242}, {242, 245, 250}, {255, 255, 255}},
     {{140, 140, 56}, {214, 196, 80}, {240, 226, 130}}, {0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {220, 235, 255}, 1},
    /* LOTUS: three rings of pointed petals, pale at the base, its seed pod in the middle */
    {SH_POINT, 3, {8, 8, 6}, {1.0f, 0.80f, 0.56f}, {0.34f, 0.34f, 0.36f}, {0.0f, 0.5f, 0.15f}, 0.0f, 1, 0, CEN_POD, 0.18f, 14,
     0.30f, {{196, 76, 126}, {230, 120, 164}, {242, 160, 192}, {248, 196, 216}, {252, 224, 234}, {255, 246, 250}},
     {{196, 156, 36}, {240, 206, 70}, {252, 232, 130}}, {250, 220, 110}, {{255, 240, 150}, {240, 190, 60}}, {255, 200, 220}, 1},
    /* MOONFLOWER: five petals fused to a round, a star in its throat; it shines */
    {SH_FUSED, 2, {5, 5}, {1.0f, 0.46f}, {0.60f, 0.50f}, {0.0f, 0.5f}, 0.0f, 0, 1, CEN_DISK, 0.12f, 5, 0.30f,
     {{86, 106, 186}, {140, 160, 226}, {180, 196, 242}, {212, 224, 250}, {236, 242, 255}, {255, 255, 255}},
     {{170, 226, 160}, {216, 248, 196}, {248, 255, 228}}, {226, 248, 214}, {{255, 255, 200}, {214, 255, 196}}, {160, 190, 255}, 0},
    /* POPPY: four wide scarlet petals, a black heart */
    {SH_ROUND, 2, {4, 4}, {1.0f, 0.84f}, {0.95f, 0.90f}, {0.0f, 0.5f}, 0.0f, 0, 0, CEN_DISK, 0.20f, 14, 0.33f,
     {{124, 8, 10}, {180, 20, 18}, {220, 40, 30}, {240, 70, 50}, {250, 110, 80}, {255, 170, 140}},
     {{18, 18, 22}, {40, 40, 46}, {74, 70, 60}}, {30, 30, 34}, {{64, 64, 60}, {96, 92, 70}}, {255, 120, 90}, 0},
    /* DAISY: a ring of narrow white petals, a wide golden middle */
    {SH_ROUND, 2, {13, 13}, {1.0f, 0.93f}, {0.20f, 0.20f}, {0.0f, 0.5f}, 0.0f, 0, 0, CEN_FLORETS, 0.32f, 0, 0.0f,
     {{146, 152, 168}, {200, 204, 214}, {226, 228, 234}, {240, 242, 246}, {250, 250, 252}, {255, 255, 255}},
     {{196, 124, 16}, {240, 180, 30}, {255, 222, 84}}, {0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {255, 255, 220}, 1},
    /* FROST STAR: three rings of thin crystal blades, light from within, a cold glow */
    {SH_POINT, 3, {8, 8, 6}, {1.0f, 0.72f, 0.46f}, {0.20f, 0.22f, 0.24f}, {0.0f, 0.5f, 0.25f}, 0.0f, 1, 1, CEN_DISK, 0.12f, 6,
     0.36f, {{60, 84, 168}, {104, 146, 220}, {146, 196, 240}, {188, 228, 250}, {224, 245, 255}, {255, 255, 255}},
     {{210, 246, 255}, {150, 222, 250}, {110, 176, 240}}, {180, 232, 255}, {{255, 255, 255}, {196, 255, 236}}, {110, 255, 200}, 1},
    /* DAHLIA: four rings of many petals, magenta deep inside, gold at the tips */
    {SH_ROUND, 4, {12, 12, 10, 8}, {1.0f, 0.82f, 0.62f, 0.42f}, {0.27f, 0.27f, 0.28f, 0.30f}, {0.0f, 0.5f, 0.25f, 0.75f}, 0.0f,
     0, 0, CEN_FLORETS, 0.14f, 0, 0.0f,
     {{140, 26, 60}, {196, 46, 82}, {232, 84, 92}, {248, 132, 100}, {255, 184, 126}, {255, 228, 176}},
     {{196, 104, 20}, {240, 168, 40}, {255, 214, 90}}, {0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {255, 170, 120}, 1},
};
/* each scene's flower, and its kin that may come instead (DAWN BLOSSOM RAIN EMBER SNOW LAGOON NIGHT AURORA DUSK) */
static const uint8_t SCENE_SPECIES[9][3] = {
    {SP_ROSE, SP_POPPY, SP_DAISY},   {SP_CHERRY, SP_DAISY, SP_CHERRY}, {SP_BLUEBELL, SP_BLUEBELL, SP_POPPY},
    {SP_FIRELILY, SP_FIRELILY, SP_DAHLIA}, {SP_EDELWEISS, SP_EDELWEISS, SP_FROST}, {SP_LOTUS, SP_LOTUS, SP_CHERRY},
    {SP_MOON, SP_MOON, SP_BLUEBELL}, {SP_FROST, SP_FROST, SP_MOON},   {SP_DAHLIA, SP_DAHLIA, SP_ROSE}};
/* the leaves behind it: their angle (turns, from the right toward the bottom), reach (of R), half-width */
static const float LEAF_ANG[3] = {0.10f, 0.40f, 0.83f}, LEAF_LEN[3] = {1.50f, 1.42f, 1.15f}, LEAF_HW[3] = {0.36f, 0.36f, 0.28f};

#define FR 31                          /* the bloom's reach, garden pixels */
#define FCY 36                         /* its centre's row */
#define FBOX 100                       /* the box it is drawn in (the leaves reach out of the bloom) */
#define NLAYER 5                       /* the leaves (layer 0) and the petals' rings */
#define NPETAL 16
#define NSTAMEN 24

static struct {
    species_t sp;                      /* the one growing: its kind, varied */
    uint8_t kind, bio;
    float grow;                        /* 0..1 the leaves and a green bud, 1..2.6 the opening, to 3 full */
    float shown;                       /* grow as drawn: it follows smoothly */
    float carry;                       /* growth waiting for the next bud (played while the petals fell) */
    uint16_t alive[NLAYER];            /* the petals still on, a bit each */
    uint8_t shedding;                  /* 1 dropping its petals; 2 quickly (a note, a new scene) */
    uint32_t shed_next, last_note;
    float pglow[NLAYER][NPETAL], sglow[NSTAMEN];
    float halo, pulse, leaf, turn;
    uint32_t wave_t, chord_t[3];
    float axc[NLAYER][NPETAL], axs[NLAYER][NPETAL];   /* each petal's axis (cos, sin), this frame */
    float stang[NSTAMEN], stlen[NSTAMEN];
    uint32_t seed;
} fl;
static uint8_t frank[FBOX][FBOX] PLAT_POOL;    /* the box: what is on top at each pixel (0 nothing; layer * 16 + petal + 1) */
static uint8_t fshade[FBOX][FBOX] PLAT_POOL;   /* its shade 0..255, or the centre's distance */
static float WPROF[NSHAPES][33];       /* each shape's half-width along the petal */

static float frandu(uint32_t *s)
{
    *s = hash32(*s + 0x9E3779B9u);
    return (float)(*s >> 8) * (1.0f / 16777216.0f);
}

static float atan2_turns(float y, float x)      /* 0..1, from +x, toward +y (down on the screen) */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a, s, r;
    if (ax < 1e-6f && ay < 1e-6f)
        return 0.0f;
    a = ax > ay ? ay / ax : ax / ay;
    s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.5707963f - r;
    if (x < 0.0f)
        r = 3.1415927f - r;
    r *= 0.15915494f;
    return y < 0.0f ? 1.0f - r : r;
}

static void flower_profiles(void)
{
    int i;
    for (i = 0; i <= 32; i++) {
        float t = (float)i * (1.0f / 32.0f), e, p;
        e = (t - 0.55f) / 0.45f;                                    /* a round petal: an ellipse to the tip */
        WPROF[SH_ROUND][i] = t < 0.1f ? 0.0f : fm_sqrtf(fm_maxf(0.0f, 1.0f - e * e));
        WPROF[SH_NOTCH][i] = WPROF[SH_ROUND][i];
        p = fm_sinf(3.1415927f * fm_powf(fm_maxf(t, 1e-4f), 0.8f)); /* pointed: widest two fifths along */
        WPROF[SH_POINT][i] = fm_powf(fm_maxf(p, 0.0f), 0.75f);
        WPROF[SH_FLAME][i] = WPROF[SH_POINT][i] * (1.0f + 0.14f * fm_sinf(12.0f * t)) * (t < 0.15f ? t / 0.15f : 1.0f);
        e = (t - 0.5f) / 0.5f;
        WPROF[SH_FRINGE][i] = fm_sqrtf(fm_maxf(0.0f, 1.0f - e * e));
        WPROF[SH_FUSED][i] = fm_powf(fm_maxf(0.0f, 1.0f - e * e), 0.3f);
        p = fm_sinf(3.1415927f * fm_powf(fm_maxf(t, 1e-4f), 0.65f)); /* a leaf: a stalk, a blade, a long tip */
        WPROF[SH_LEAF][i] = t < 0.12f ? 0.12f : fm_powf(fm_maxf(p, 0.0f), 0.9f);
    }
}

/* the tint of a variety: the palette's hue turned (in YIQ), by a radians */
static void hue_turn(uint8_t *c, float a)
{
    float r = (float)c[0], g = (float)c[1], b = (float)c[2], ca = fm_cosf(a), sa = fm_sinf(a);
    float y = 0.299f * r + 0.587f * g + 0.114f * b, i = 0.596f * r - 0.274f * g - 0.322f * b,
          q = 0.211f * r - 0.523f * g + 0.312f * b, i2 = i * ca - q * sa, q2 = i * sa + q * ca;
    rgb_t o = mk((int)(y + 0.956f * i2 + 0.621f * q2), (int)(y - 0.272f * i2 - 0.647f * q2), (int)(y - 1.106f * i2 + 1.703f * q2));
    c[0] = o.r;
    c[1] = o.g;
    c[2] = o.b;
}

static void flower_make(int bio, int kind, int fresh)   /* a new bud of this kind (fresh: from nothing) */
{
    uint32_t s = fl.seed = hash32(fl.seed + plat_ms() + 7u);
    int k, l;
    float r, hue = 0.0f;
    fl.kind = (uint8_t)kind;
    fl.bio = (uint8_t)bio;
    fl.sp = SPECIES[kind];
    for (l = 0; l < fl.sp.layers; l++) {          /* a few petals more or fewer, each ring turned its own way */
        int n = fl.sp.npet[l] + (int)(frandu(&s) * 3.0f) - 1;
        if (kind == SP_POPPY || kind == SP_MOON)
            n = fl.sp.npet[l];
        fl.sp.npet[l] = (uint8_t)(n < 4 ? 4 : n > NPETAL ? NPETAL : n);
        fl.sp.rot[l] += frandu(&s) * 0.3f;
    }
    if (fl.sp.npet[1] == fl.sp.npet[0] && fl.sp.layers > 1)
        fl.sp.rot[1] = fl.sp.rot[0] + 0.5f;
    fl.sp.twist *= 0.7f + 0.6f * frandu(&s);
    r = frandu(&s);                               /* the tint: mostly its own, sometimes another */
    if (r > 0.55f)
        hue = (frandu(&s) - 0.5f) * (kind == SP_ROSE || kind == SP_LOTUS || kind == SP_DAHLIA ? 1.0f : 0.6f);
    if (hue != 0.0f) {
        for (k = 0; k < 6; k++)
            hue_turn(fl.sp.pal[k], hue);
        hue_turn(fl.sp.halo, hue);
    }
    for (k = 0; k < NSTAMEN; k++) {
        fl.stang[k] = ((float)k + (frandu(&s) - 0.5f) * 0.6f) / (float)(fl.sp.nst ? fl.sp.nst : 1);
        fl.stlen[k] = 0.72f + 0.28f * frandu(&s);
    }
    for (l = 0; l < NLAYER; l++)
        fl.alive[l] = 0xFFFFu;
    fl.shedding = 0;
    if (fresh)
        fl.grow = fl.shown = 0.0f;
    else {                                        /* the leaves stay; a small bud again */
        fl.grow = fm_minf(fl.grow, 0.75f);
        fl.shown = fm_minf(fl.shown, 0.75f);
    }
    fl.grow = fm_minf(2.4f, fl.grow + fl.carry);
    fl.carry = 0.0f;
}

static void flower_new(int bio, int fresh)       /* a new bud of the scene's kind (now and then its kin), or FLOWER's */
{
    float r = (float)(hash32(fl.seed ^ plat_ms()) >> 8) * (1.0f / 16777216.0f);
    int pick = proj.par[P_FLOWER];
    if (pick > 0 && pick <= NSPECIES)
        flower_make(bio, pick - 1, fresh);
    else
        flower_make(bio, SCENE_SPECIES[(unsigned)bio < 9u ? bio : 0][r < 0.70f ? 0 : r < 0.88f ? 1 : 2], fresh);
}

static void flower_init(int bio)
{
    flower_profiles();
    fl.seed = 335597u;
    fl.carry = 0.0f;
    flower_new(bio, 1);
    fl.grow = fl.shown = 0.3f;                    /* its leaves just showing */
    fl.last_note = plat_ms();
}

static void flower_scene(int bio)                 /* a new scene, or FLOWER turned: the old one lets go quickly */
{
    fl.bio = (uint8_t)bio;
    if (fl.grow > 1.15f) {
        fl.carry = fm_minf(1.6f, fl.grow - 0.75f);
        fl.shedding = 2;
    } else
        flower_new(bio, 0);
}

static float bloom_of(float g) { return fm_clampf((g - 1.0f) * (1.0f / 1.6f), 0.0f, 1.0f); }
static int bloom_on(void) { return bloom_of(fl.shown) > 0.06f; }
static float fl_R(void) { return (float)FR * (0.30f + 0.70f * bloom_of(fl.shown)); }

static void flower_note(int note, int inst, int vel, int src, uint32_t now)
{
    float v = (float)vel * (1.0f / 127.0f), add;
    int k;
    if (inst == I_DRUM) {
        int d = ow_drum_of(note);
        if (d == D_BD1 || d == D_BD2)
            fl.pulse = 1.0f;
        else if ((d == D_SD1 || d == D_SD2 || d == D_CLAP) && bloom_on())
            for (k = 0; k < NPETAL; k++)
                fl.pglow[1][k] = fm_maxf(fl.pglow[1][k], 0.15f);
        if (src == 3)                             /* the beat is weather, not playing */
            return;
        add = 0.008f;
    } else {
        add = (0.02f + 0.035f * v) * (src == 0 ? 1.35f : src == 1 ? 0.8f : 0.7f);
        fl.last_note = now;
    }
    if (fl.shedding) {
        fl.carry = fm_minf(1.6f, fl.carry + add);
        fl.shedding = 2;
    } else
        fl.grow = fm_minf(3.0f, fl.grow + add);
    if (inst == I_DRUM)
        return;
    if (fl.sp.nst)                                /* the note's stamen, by its pitch */
        fl.sglow[(note * 7) % fl.sp.nst] = 1.0f;
    {                                             /* and a petal */
        int l = 1 + (note % 3 < fl.sp.layers ? note % 3 : 0), n = fl.sp.npet[l - 1], p = (note * 5) % (n ? n : 1);
        fl.pglow[l][p] = fm_maxf(fl.pglow[l][p], 0.30f + 0.45f * v);
    }
    if (inst == I_PAD)
        fl.halo = fm_minf(1.0f, fl.halo + 0.35f);
    if (inst == I_BASS)
        fl.leaf = 1.0f;
    if (inst == I_BELL && bloom_on()) {           /* sparkles round the bloom */
        float a = (float)((note * 37) % 100) * 0.01f, rr = fl_R() * (0.95f + 0.25f * v);
        particle((float)FCX + rr * fm_cosf(6.2831853f * a), (float)FCY + rr * fm_sinf(6.2831853f * a), 0.0f, -0.02f, 60, 5);
    }
    if (inst == I_PIANO || inst == I_PAD) {       /* three notes together: a chord, a wave round the petals */
        fl.chord_t[0] = fl.chord_t[1];
        fl.chord_t[1] = fl.chord_t[2];
        fl.chord_t[2] = now;
        if (now - fl.chord_t[0] < 90u && now - fl.wave_t > 400u)
            fl.wave_t = now;
    }
}

static void flower_step(uint32_t dt, uint32_t now)
{
    int l, k;
    float s = (float)dt * 0.001f;
    fl.shown += (fl.grow - fl.shown) * fm_minf(1.0f, s * 2.2f);   /* what is drawn follows the growth, gently */
    fl.turn += s * 0.0045f;                                         /* a slow turn: once in some four minutes */
    if (fl.turn >= 1.0f)
        fl.turn -= 1.0f;
    for (l = 0; l < NLAYER; l++)
        for (k = 0; k < NPETAL; k++)
            fl.pglow[l][k] *= fm_maxf(0.0f, 1.0f - s * 2.5f);
    for (k = 0; k < NSTAMEN; k++)
        fl.sglow[k] *= fm_maxf(0.0f, 1.0f - s * 1.8f);
    fl.halo *= fm_maxf(0.0f, 1.0f - s * 0.25f);
    fl.pulse *= fm_maxf(0.0f, 1.0f - s * 8.0f);
    fl.leaf *= fm_maxf(0.0f, 1.0f - s * 1.5f);
    if (now - fl.wave_t < 900u)                   /* the chord's wave: each petal in turn, the outer ring first */
        for (l = 1; l <= fl.sp.layers; l++) {
            int n = fl.sp.npet[l - 1];
            for (k = 0; k < n; k++) {
                int at = (int)(now - fl.wave_t) - k * 420 / n - (l - 1) * 90;
                if (at >= 0 && at < 120)
                    fl.pglow[l][k] = fm_maxf(fl.pglow[l][k], 0.5f);
            }
        }
    /* silence: it holds, then lets its petals go, then a new bud */
    if (!fl.shedding && fl.grow > 1.5f && now - fl.last_note > 16000u) {
        fl.shedding = 1;
        fl.shed_next = now;
    }
    if (fl.shedding && (int32_t)(now - fl.shed_next) >= 0) {
        int pl = 0, cnt = 0, pick = -1, i;
        for (l = 1; l <= fl.sp.layers && !cnt; l++)   /* the outer ring goes first, a petal at random */
            for (k = 0; k < fl.sp.npet[l - 1]; k++)
                if (fl.alive[l] >> k & 1u) {
                    cnt++;
                    pl = l;
                }
        if (cnt && bloom_on()) {
            i = (int)(hash32(now * 2654435761u) % (uint32_t)cnt);
            for (k = 0; k < fl.sp.npet[pl - 1] && pick < 0; k++)
                if ((fl.alive[pl] >> k & 1u) && i-- == 0)
                    pick = k;
        }
        if (pick >= 0) {
            float a = fl.turn + (fl.sp.rot[pl - 1] + (float)pick + 0.5f) / (float)fl.sp.npet[pl - 1];
            float rr = fl_R() * fl.sp.rad[pl - 1] * 0.65f;
            fl.alive[pl] &= (uint16_t)~(1u << pick);
            particle_c((float)FCX + rr * fm_cosf(6.2831853f * a), (float)FCY + rr * fm_sinf(6.2831853f * a),
                       fm_cosf(6.2831853f * a) * 0.06f + 0.02f, 0.035f, 900, 6,
                       mk(fl.sp.pal[3][0], fl.sp.pal[3][1], fl.sp.pal[3][2]));
            fl.shed_next = now + (fl.shedding == 2 ? 60u : 360u + (hash32(now) & 255u));
        } else {
            flower_new(fl.bio, 0);
            fl.last_note = now;
        }
    }
}

/* ------------------------------------------------------------------ drawing --- */
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static rgb_t prgb(const uint8_t *c, float light) { return mk((int)((float)c[0] * light), (int)((float)c[1] * light), (int)((float)c[2] * light)); }

static void flower_draw(float light, uint32_t now, rgb_t glowc)
{
    const species_t *sp = &fl.sp;
    float g = fl.shown, bl = bloom_of(g), leafo = fm_clampf(g * 1.4f, 0.0f, 1.0f), lit = sp->glow ? 1.0f : 0.42f + 0.58f * light;
    float breathe = 1.0f + 0.012f * fm_sinf((float)now * 0.0016f) + 0.05f * fm_minf(1.0f, ow_level * 2.5f) + 0.04f * fl.pulse;
    float R = fl_R() * breathe, Rleaf = (float)FR * (0.55f + 0.45f * leafo), open[NLAYER], cx = (float)FCX, cy = (float)FCY;
    int x, y, l, k, x0 = FCX - FBOX / 2, y0 = FCY - FBOX / 2, nl = sp->layers + 1;
    if (g <= 0.02f)
        return;
    /* the halo behind: the pad's air, or a glowing kind's own light, in the scene's glow */
    {
        float h = fm_maxf(fl.halo * 0.55f, sp->glow ? 0.28f + 0.2f * (1.0f - light) : 0.10f) * fm_maxf(bl, 0.2f);
        rgb_t hc = mix(mk(sp->halo[0], sp->halo[1], sp->halo[2]), glowc, 0.35f);
        float rr = R + 12.0f;
        for (y = (int)(cy - rr); y <= (int)(cy + rr); y++)
            for (x = FCX - (int)rr; x <= FCX + (int)rr; x++) {
                float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy) / rr, a;
                if (d >= 1.0f || (unsigned)x >= GW || (unsigned)y >= GH)
                    continue;
                a = h * (1.0f - d) * (1.0f - d);
                if ((float)BAYER[y & 3][x & 3] * (1.0f / 16.0f) < a * 2.2f)
                    lfb[y][x] = c565(mix(gget(x, y), hc, fm_minf(0.55f, a * 1.5f)));
            }
    }
    /* the open of each ring: the outer first; at the start all of them a closed bud */
    open[0] = leafo;
    for (l = 1; l < nl; l++) {
        float o = fm_clampf((bl * 1.3f - (float)(l - 1) * 0.14f) / 0.7f, 0.0f, 1.0f);
        open[l] = o * o * (3.0f - 2.0f * o);
    }
    for (l = 0; l < nl; l++) {                    /* this frame's axes: the leaves still, the petals turning */
        int n = l ? sp->npet[l - 1] : 3;
        for (k = 0; k < n; k++) {
            float a = 6.2831853f * (l ? fl.turn + ((float)k + 0.5f + sp->rot[l - 1]) / (float)n : LEAF_ANG[k]);
            fl.axc[l][k] = fm_cosf(a);
            fl.axs[l][k] = fm_sinf(a);
        }
    }
    /* which is on top where */
    for (y = 0; y < FBOX; y++)
        for (x = 0; x < FBOX; x++) {
            float dx = (float)(x0 + x) + 0.5f - cx, dy = (float)(y0 + y) + 0.5f - cy;
            float r = fm_sqrtf(dx * dx + dy * dy), th = -1.0f, rc = R * sp->core * (0.5f + 0.5f * bl);
            uint8_t best = 0, sh = 0;
            frank[y][x] = 0;
            if ((unsigned)(y0 + y) >= GH || r > fm_maxf(R, Rleaf * LEAF_LEN[0]) + 1.0f)
                continue;
            if (bl > 0.0f && r < rc && bl > 0.25f) {   /* the centre */
                frank[y][x] = 0xF0;
                fshade[y][x] = (uint8_t)(255.0f * r / rc);
                continue;
            }
            for (l = nl - 1; l >= 0 && !best; l--) {    /* the inner rings lie on top, the leaves under all */
                int n = l ? sp->npet[l - 1] : 3, c = 0, cand[3], nc, ci, hit = -1, shape = l ? sp->shape : SH_LEAF;
                float Rl, u = 0.0f, hv = 0.0f;
                if (!l && leafo <= 0.0f)
                    continue;
                Rl = l ? R * sp->rad[l - 1] * (0.30f + 0.70f * open[l]) : Rleaf;
                if (!l) {
                    cand[0] = 0;
                    cand[1] = 1;
                    cand[2] = 2;
                    nc = 3;
                } else {
                    float a;
                    if (r > Rl + 0.5f)
                        continue;
                    if (th < 0.0f)
                        th = atan2_turns(dy, dx);
                    a = (th - fl.turn) * (float)n - sp->rot[l - 1] - sp->twist * r / Rl;
                    a -= fm_floorf(a / (float)n) * (float)n;
                    c = (int)a;
                    u = a - (float)c - 0.5f;
                    cand[0] = c % n;
                    cand[1] = (c + (u > 0.0f ? 1 : n - 1)) % n;
                    nc = 2;
                }
                for (ci = 0; ci < nc; ci++) {
                    int p = cand[ci];
                    float xa, ya, t, w, q, v, L = l ? Rl : Rl * LEAF_LEN[p], hwp = l ? sp->hw[l - 1] * R : LEAF_HW[p] * Rleaf;
                    if (l && !(fl.alive[l] >> p & 1u))
                        continue;
                    xa = dx * fl.axc[l][p] + dy * fl.axs[l][p];
                    ya = -dx * fl.axs[l][p] + dy * fl.axc[l][p];
                    if (sp->twist > 0.0f && l)
                        ya -= sp->twist * 0.9f * Rl * (xa / Rl) * (xa / Rl);
                    if (!l)                     /* the leaves bend a little */
                        ya -= 0.10f * L * (xa / L) * (xa / L) * (p == 1 ? -1.0f : 1.0f);
                    t = xa / L;
                    if (t <= 0.0f || t > 1.0f)
                        continue;
                    w = WPROF[shape][(int)(t * 32.0f)] * hwp * (l ? 0.45f + 0.55f * open[l] : 1.0f);
                    if (fm_fabsf(ya) > w)
                        continue;
                    if (shape == SH_NOTCH && t > 0.86f && fm_fabsf(ya) < (t - 0.86f) * Rl * 1.3f)
                        continue;
                    if (shape == SH_FRINGE && t > 0.80f && t > 0.86f + 0.14f * fm_fabsf(fm_cosf(9.42f * ya / (w > 0.5f ? w : 0.5f))))
                        continue;
                    if (l && hit >= 0 && p != (hit + 1) % n)   /* of two, the later petal lies over the earlier */
                        continue;
                    q = ya / (w > 0.3f ? w : 0.3f);
                    /* the shade: along it, turned to the light (upper left), its midrib, a sheen, its glow */
                    if (!l) {
                        v = 0.30f + 0.45f * t - 0.18f * q * (fl.axc[0][p] - fl.axs[0][p] > 0.0f ? 1.0f : -1.0f);
                        if (fm_fabsf(q) < 0.12f && t > 0.08f && t < 0.9f)
                            v += 0.35f;         /* the midrib */
                        else if (fm_fabsf(fm_fabsf(q) * 0.9f - (t * 5.0f - fm_floorf(t * 5.0f))) < 0.08f)
                            v += 0.15f;         /* its veins */
                        v += fl.leaf * 0.3f;
                    } else {
                        v = sp->invert ? 0.95f - 0.55f * t : 0.22f + 0.62f * t;
                        v += -0.08f * (fl.axc[l][p] + fl.axs[l][p]) - 0.05f * (float)(l > 1 ? l - 1 : 0);
                        if (shape == SH_FUSED && fm_fabsf(q) < 0.14f && t > 0.25f)
                            v += 0.22f;         /* the moonflower's star: its creases */
                        else if (sp->vein && fm_fabsf(q) < 0.10f && t > 0.15f && t < 0.85f)
                            v += 0.14f;
                        if (t > 0.55f && t < 0.82f && q < -0.25f && q > -0.75f)
                            v += 0.15f;
                        if (t < 0.25f && l < nl - 1)
                            v -= 0.12f;         /* deep in the cup, under the next ring */
                        v += fl.pglow[l][p];
                    }
                    hit = p;
                    hv = v;
                    if (!l)
                        break;
                }
                if (hit >= 0) {
                    best = (uint8_t)(l * 16 + hit + 1);
                    sh = (uint8_t)(fm_clampf(hv, 0.0f, 1.0f) * 255.0f);
                }
            }
            frank[y][x] = best;
            fshade[y][x] = sh;
        }
    /* the colours: an outline against what is behind, a line where a petal lies over another, light on the upper-left
     * edges, a shadow under the ring above, then the faces */
    {
        rgb_t P[6], S[6], C[3];
        float ll = 0.45f + 0.55f * light;
        for (k = 0; k < 6; k++)
            P[k] = prgb(sp->pal[k], lit);
        S[0] = dim(mk(22, 66, 40), ll);
        S[1] = dim(mk(38, 100, 52), ll);
        S[2] = dim(mk(56, 132, 62), ll);
        S[3] = dim(mk(82, 162, 74), ll);
        S[4] = dim(mk(120, 192, 92), ll);
        S[5] = dim(mk(172, 224, 136), ll);
        for (k = 0; k < 3; k++)
            C[k] = prgb(sp->cen[k], sp->glow ? 1.0f : lit);
        for (y = 1; y < FBOX - 1; y++)
            for (x = 1; x < FBOX - 1; x++) {
                uint8_t id = frank[y][x], o;
                int gx = x0 + x, gy = y0 + y, lay, edge = 0, inner = 0, nb, v, step;
                const rgb_t *pal;
                if (!id)
                    continue;
                if (id == 0xF0) {             /* the centre */
                    float rr = (float)fshade[y][x] * (1.0f / 255.0f);
                    rgb_t c;
                    if (sp->center == CEN_FLORETS)
                        c = (hash32((uint32_t)(gx * 31 + gy * 977)) & 3u) == 0 ? C[0] : (((gx + gy) & 1) ? C[1] : C[2]);
                    else if (sp->center == CEN_POD)
                        c = ((gx * 3 + gy * 5) % 7 == 0 && rr < 0.8f) ? C[0] : rr > 0.8f ? C[1] : C[2];
                    else if (sp->center == CEN_SPIRAL) {
                        float a = atan2_turns((float)gy + 0.5f - cy, (float)gx + 0.5f - cx) - fl.turn + rr * 1.6f;
                        c = ((int)((a - fm_floorf(a)) * 3.0f) & 1) ? C[1] : C[0];
                        if (rr > 0.85f)
                            c = C[2];
                    } else
                        c = rr < 0.45f ? C[0] : rr < 0.8f ? C[1] : C[2];
                    if (rr < 0.35f && sp->center != CEN_SPIRAL && x > 0 && frank[y - 1][x - 1] == 0xF0 && ((gx + gy) & 3) == 0)
                        c = mix(c, mk(255, 255, 255), 0.25f);   /* a glint */
                    gp(gx, gy, c);
                    continue;
                }
                lay = (id - 1) >> 4;
                pal = lay ? P : S;
                for (nb = 0; nb < 4; nb++) {
                    static const int8_t DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
                    o = frank[y + DY[nb]][x + DX[nb]];
                    if (!o)
                        edge = 1;
                    else if (o != 0xF0 && o != id) {   /* a line on the upper of two */
                        int ol = (o - 1) >> 4, op = (o - 1) & 15, n = lay ? sp->npet[lay - 1] : 3;
                        if (ol < lay || (ol == lay && lay && (((id - 1) & 15) == (op + 1) % n)))
                            inner = 1;
                    }
                }
                if (edge) {
                    gp(gx, gy, pal[0]);
                    continue;
                }
                if (inner) {
                    gp(gx, gy, pal[1]);
                    continue;
                }
                v = fshade[y][x] * 5 / 16;
                o = frank[y - 1][x - 1];
                if (!o || (o != 0xF0 && ((o - 1) >> 4) < lay))
                    v += 22;                    /* the edge that faces the light */
                if (y >= 2 && x >= 2) {
                    uint8_t up = frank[y - 2][x - 2];
                    if (up && up != 0xF0 && ((up - 1) >> 4) > lay)
                        v -= 20;                /* the shadow of the ring above */
                }
                v = v < 0 ? 0 : v > 79 ? 79 : v;
                step = v / 16 + (BAYER[gy & 3][gx & 3] < (v & 15) ? 1 : 0);
                gp(gx, gy, pal[1 + (step > 4 ? 4 : step)]);
            }
    }
    /* before it opens: a small green bud in the leaves, its colour coming at the tip */
    if (bl <= 0.0f) {
        float br = 2.0f + 4.0f * fm_clampf(g, 0.0f, 1.0f);
        rgb_t gc = dim(mk(70, 150, 66), 0.45f + 0.55f * light), gh = dim(mk(140, 210, 110), 0.45f + 0.55f * light),
              go = dim(mk(30, 80, 40), 0.45f + 0.55f * light), pc = prgb(sp->pal[3], lit);
        for (y = -(int)br - 1; y <= (int)br + 1; y++)
            for (x = -(int)br - 1; x <= (int)br + 1; x++) {
                float d = fm_sqrtf((float)(x * x + y * y)) / br;
                rgb_t c;
                if (d > 1.0f)
                    continue;
                c = d > 0.8f ? go : (x + y < -1 ? gh : gc);
                if (d < 0.55f && g > 0.6f && ((x * 2 + y * 3 + 9) % 5) < (int)((g - 0.6f) * 9.0f))
                    c = pc;                     /* the petals' colour showing */
                gp(FCX + x, FCY + y, c);
            }
    }
    /* the stamens: from the centre out, each lit by its note */
    if (sp->nst && bl > 0.45f) {
        float grow_s = fm_clampf((bl - 0.45f) * 2.2f, 0.0f, 1.0f), rc = R * sp->core * 0.6f;
        rgb_t fc = prgb(sp->fil, sp->glow ? 1.0f : lit);
        for (k = 0; k < sp->nst && k < NSTAMEN; k++) {
            float a = 6.2831853f * (fl.stang[k] + fl.turn), ca = fm_cosf(a), sa = fm_sinf(a),
                  L = R * sp->stl * fl.stlen[k] * grow_s + fl.sglow[k] * 2.5f, rr;
            int tx = 0, ty = 0;
            rgb_t an = prgb(sp->anth[k & 1], sp->glow ? 1.0f : lit);
            if (fl.sglow[k] > 0.05f)
                an = mix(an, mk(255, 255, 230), fl.sglow[k]);
            for (rr = rc; rr <= L; rr += 0.7f) {
                tx = (int)(cx + ca * rr);
                ty = (int)(cy + sa * rr);
                gp(tx, ty, ((int)(rr * 1.4f) & 1) ? fc : dim(fc, 0.82f));
            }
            if (L > rc + 1.0f) {
                gp(tx, ty, an);
                if (fl.sglow[k] > 0.3f || (k % 3) == 0) {
                    gp(tx + (ca > 0.0f ? 1 : -1), ty, an);
                    gp(tx, ty + (sa > 0.0f ? 1 : -1), dim(an, 0.85f));
                }
            }
        }
    }
    /* a full bloom sparkles now and then */
    if (bl > 0.95f && (hash32(now / 120u) % 19u) == 0u) {
        uint32_t h = hash32(now);
        float a = (float)(h & 255u) * (6.2831853f / 256.0f), rr = R * (0.8f + 0.45f * (float)((h >> 8) & 255u) / 255.0f);
        particle(cx + rr * fm_cosf(a), cy + rr * fm_sinf(a), 0.0f, -0.01f, 50, 5);
    }
}
