/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* DUSTKIT: OVERWORLD's drum machine made dusty: a soft kick, a brushed snare, a rim, a shaker, a closed and an open
 * hat, a dusty clap, a wood block (and its toms, cowbell, tambourine, cymbals for MIDI). Each sound made (a tone that
 * falls, filtered noise, six square waves for the metal); the track's sum through 12 bits and a low-pass (DUST).
 * KIT: 707 (clean), DUSTY (lower, darker, a sampler's worn copy), BRUSH (wire brushes, soft hats, a felt beater), CHIP, SYNTH, WAR,
 * and four Roland-style machines, SYNTHESIZED (no samples): 808, 909, 606, CR-78 (and 707 itself, now from the same table).
 * Their voices are one table (RK): per machine, per voice, a handful of numbers (tone, its fall, noise, the six squares, a filter).
 * Part of tape_dsp.c. */

enum { D_BD1, D_BD2, D_SD1, D_SD2, D_RIM, D_CLAP, D_CH, D_OH, D_LT, D_MT, D_HT, D_COW, D_TAMB, D_CRASH, D_RIDE,
       D_SHAKER, D_WOOD, D_CONGA, ND };
typedef struct {
    uint32_t mph[6], minc[6];
    float t, vel, gl, gr;
    float ph, f, fend, fmul, ph2, inc2;
    float at, atm, an, anm;
    float lo, bp, sf, sq;
    float tone_l, tone2_l, noise_l, metal_l, sqr_l, filt, kind, rise, held;
    float wave, nrate, nc, nv;                             /* a chip's: the tone's shape, the noise held (1 bit) */
    float drv, amph, aminc, amd, cb, cc, cgap, ctail;      /* a drum machine's: the tone's drive, a tremolo, the clap's bursts */
} drum_v;
CHECK_SIZE(drum_v);

static const char *const KIT_NAMES[] = {"707", "DUSTY", "BRUSH", "CHIP", "SYNTH", "WAR", "808", "909", "606", "CR-78"};
static const dk_param_t KIT_P[] = {
    {"TUNE", 0, 100, 50, VZ_BAR, 0}, {"DECAY", 0, 100, 50, VZ_ENV, 0}, {"DUST", 0, 100, 45, VZ_DUST, 0},
    {"KIT", 0, 9, 1, VZ_TYPE, KIT_NAMES},
};
static const char *const KIT_LANE[8] = {"KICK", "SNARE", "RIM", "SHAKER", "HAT", "OPEN", "CLAP", "WOOD"};
static const uint8_t KIT_NOTE[8] = {36, 38, 37, 70, 42, 46, 39, 76};
static const float METAL[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};

static int drum_of(int note)
{
    static const int8_t DRUM_OF[25] = {D_BD2, D_BD1, D_RIM, D_SD1, D_CLAP, D_SD2, D_LT, D_CH, D_LT, D_CH, D_MT, D_OH,
                                       D_MT, D_HT, D_CRASH, D_HT, D_RIDE, D_CRASH, D_RIDE, D_TAMB, D_CRASH, D_COW,
                                       D_CRASH, D_COW, D_RIDE};   /* GM 35..59 */
    if (note == 69 || note == 70 || note == 82)
        return D_SHAKER;
    if (note == 76 || note == 77 || note == 75)
        return D_WOOD;
    while (note < 35)
        note += 12;
    while (note > 59)
        note -= 12;
    return DRUM_OF[note - 35];
}

/* the drum machines' voices, one row each (the Hz and ms are the machine's, as near as a few numbers get): the tone (its start, its end,
 * the fall's time, its decay, its level), a second tone, the noise (decay, level), the six squares (level, pitch), two squares for a
 * cowbell, the filter (Hz, damping, 1 = high-pass), the tone's drive, a tremolo (Hz, depth), the tone's shape, the clap's bursts
 * (how many, the gap, the tail). Rows: BD SD RIM CLAP CH OH TOM COW TAMB CRASH RIDE SHAKER WOOD CONGA. */
typedef struct {
    float f0, f1, ft, td, tl, f2, l2, nd, nl, ml, mm, cl, sf, q, hp, drv, amr, amd, wave, cn, cg, ct;
} rk_t;
enum { RK_BD, RK_SD, RK_RIM, RK_CLAP, RK_CH, RK_OH, RK_TOM, RK_COW, RK_TAMB, RK_CRASH, RK_RIDE, RK_SHAKER, RK_WOOD, RK_CONGA, NRK };
static const rk_t RK[5][NRK] = {
    {   /* TR-808: a long sine kick, a snare of two tones and noise, hats of the six squares, a four-burst clap */
        {175, 47, 14, 480, 1.0f, 0, 0, 4, 0.30f, 0, 0, 0, 1800, 0.8f, 0, 0.5f, 0, 0, 0, 0, 0, 0},
        {180, 180, 0, 90, 0.45f, 330, 0.30f, 190, 0.85f, 0, 0, 0, 1900, 0.9f, 1, 0, 0, 0, 0, 0, 0, 0},
        {480, 480, 0, 9, 0.5f, 1750, 0.4f, 8, 0.35f, 0, 0, 0, 2400, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 4.5f, 1.6f, 0, 0, 0, 1050, 0.6f, 0, 0, 0, 0, 0, 3, 9.5f, 160},
        {0, 0, 0, 0, 0, 0, 0, 38, 0, 1.0f, 1.0f, 0, 7200, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 320, 0, 1.0f, 1.0f, 0, 7200, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0},
        {130, 100, 22, 330, 0.9f, 0, 0, 3, 0.10f, 0, 0, 0, 900, 1.0f, 0, 0.15f, 0, 0, 0, 0, 0, 0},
        {540, 540, 0, 0, 0, 800, 0, 130, 0, 0, 0, 0.55f, 1300, 0.6f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 150, 0, 0.90f, 2.6f, 0, 6500, 1.1f, 1, 0, 31, 0.45f, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 1100, 0.12f, 0.8f, 1.0f, 0, 5200, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 650, 0.10f, 0.6f, 1.45f, 0, 6200, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 28, 0.6f, 0, 0, 0, 4800, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0},
        {2450, 2450, 0, 28, 0.7f, 3700, 0.12f, 3, 0.12f, 0, 0, 0, 3000, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0},
        {330, 285, 12, 190, 0.85f, 0, 0, 4, 0.10f, 0, 0, 0, 900, 1.0f, 0, 0.1f, 0, 0, 0, 0, 0, 0},
    },
    {   /* TR-909: a punchy kick with a click and a hard edge, a snappier snare, bright gritty hats, a clap with a longer tail */
        {240, 50, 9, 300, 1.0f, 0, 0, 5, 0.45f, 0, 0, 0, 2600, 0.7f, 1, 3.0f, 0, 0, 0, 0, 0, 0},
        {190, 190, 0, 70, 0.5f, 330, 0.35f, 230, 1.0f, 0, 0, 0, 1500, 0.9f, 1, 0.6f, 0, 0, 0, 0, 0, 0},
        {500, 500, 0, 7, 0.5f, 1800, 0.5f, 5, 0.5f, 0, 0, 0, 2600, 0.6f, 0, 0.5f, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 5, 1.8f, 0, 0, 0, 1250, 0.6f, 0, 0, 0, 0, 0, 3, 11, 230},
        {0, 0, 0, 0, 0, 0, 0, 34, 0.30f, 0.45f, 1.9f, 0, 7600, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 300, 0.30f, 0.45f, 1.9f, 0, 7600, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0},
        {165, 105, 35, 300, 0.9f, 0, 0, 3, 0.12f, 0, 0, 0, 1100, 1.0f, 0, 0.4f, 0, 0, 0, 0, 0, 0},
        {560, 560, 0, 0, 0, 840, 0, 100, 0, 0, 0, 0.5f, 1400, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 140, 0.35f, 0.20f, 3.0f, 0, 6800, 1.1f, 1, 0, 31, 0.45f, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 1500, 0.40f, 0.45f, 1.9f, 0, 4600, 1.1f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 1000, 0.15f, 0.6f, 1.5f, 0, 5800, 1.1f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 40, 0.6f, 0, 0, 0, 5500, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0},
        {1100, 1100, 0, 14, 0.6f, 1650, 0.3f, 4, 0.3f, 0, 0, 0, 2200, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0},
        {300, 260, 10, 160, 0.85f, 0, 0, 4, 0.10f, 0, 0, 0, 1000, 1.0f, 0, 0.2f, 0, 0, 0, 0, 0, 0},
    },
    {   /* TR-606: small and tight: a short kick, a snare of little body, ticking hats */
        {150, 62, 10, 170, 1.0f, 0, 0, 3, 0.20f, 0, 0, 0, 1800, 0.8f, 0, 0.8f, 0, 0, 0, 0, 0, 0},
        {250, 250, 0, 45, 0.4f, 340, 0.30f, 105, 0.9f, 0, 0, 0, 2400, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0},
        {800, 800, 0, 7, 0.5f, 1900, 0.4f, 5, 0.4f, 0, 0, 0, 2600, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 4, 1.5f, 0, 0, 0, 1300, 0.7f, 0, 0, 0, 0, 0, 2, 8, 90},
        {0, 0, 0, 0, 0, 0, 0, 26, 0.15f, 0.55f, 1.2f, 0, 8000, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 180, 0.15f, 0.55f, 1.2f, 0, 8000, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {170, 130, 14, 170, 0.9f, 0, 0, 3, 0.10f, 0, 0, 0, 1000, 1.0f, 0, 0.2f, 0, 0, 0, 0, 0, 0},
        {600, 600, 0, 0, 0, 880, 0, 80, 0, 0, 0, 0.45f, 1500, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 100, 0.30f, 0.25f, 2.4f, 0, 7000, 1.2f, 1, 0, 31, 0.4f, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 850, 0.25f, 0.5f, 1.2f, 0, 6000, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 450, 0.15f, 0.6f, 1.3f, 0, 6500, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 22, 0.5f, 0, 0, 0, 6000, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0},
        {1500, 1500, 0, 12, 0.6f, 2300, 0.15f, 3, 0.15f, 0, 0, 0, 2800, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0},
        {340, 300, 8, 110, 0.85f, 0, 0, 3, 0.10f, 0, 0, 0, 1200, 1.0f, 0, 0.1f, 0, 0, 0, 0, 0, 0},
    },
    {   /* CR-78: a soft analog rhythm box: a wooden kick, a muted snare, tick hats, claves, a guiro */
        {135, 66, 14, 120, 1.0f, 0, 0, 6, 0.18f, 0, 0, 0, 900, 0.9f, 0, 0.4f, 0, 0, 0, 0, 0, 0},
        {210, 210, 0, 55, 0.5f, 0, 0, 140, 1.3f, 0, 0, 0, 1900, 1.2f, 0, 0, 0, 0, 0, 0, 0, 0},
        {900, 900, 0, 10, 0.5f, 1450, 0.4f, 8, 0.3f, 0, 0, 0, 2200, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 8, 1.3f, 0, 0, 0, 1500, 1.0f, 0, 0, 0, 0, 0, 2, 12, 80},
        {0, 0, 0, 0, 0, 0, 0, 22, 0.15f, 0.95f, 1.35f, 0, 7000, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 150, 0.15f, 0.95f, 1.35f, 0, 7000, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0},
        {240, 190, 10, 150, 0.85f, 0, 0, 3, 0.10f, 0, 0, 0, 1200, 1.0f, 0, 0.1f, 0, 0, 0, 0, 0, 0},
        {560, 560, 0, 0, 0, 840, 0, 110, 0, 0, 0, 0.5f, 1100, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 170, 0.60f, 0.40f, 2.8f, 0, 6000, 1.0f, 0, 0, 31, 0.45f, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 800, 0.25f, 0.4f, 1.4f, 0, 5500, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 500, 0.15f, 0.5f, 1.3f, 0, 6000, 1.2f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 150, 0.8f, 0, 0, 0, 3300, 0.8f, 0, 0, 38, 0.8f, 0, 0, 0, 0},
        {2050, 2050, 0, 60, 0.75f, 2900, 0.15f, 3, 0.10f, 0, 0, 0, 2600, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0},
        {300, 260, 9, 140, 0.8f, 0, 0, 3, 0.08f, 0, 0, 0, 1100, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    {   /* 707: a tight, punchy kick, a short snappy snare, bright hats and a clap (the samples' grit comes from DUST) */
        {200, 56, 13, 240, 1.0f, 0, 0, 4, 0.30f, 0, 0, 0, 2200, 0.8f, 0, 1.0f, 0, 0, 0, 0, 0, 0},
        {195, 195, 0, 70, 0.45f, 330, 0.28f, 150, 0.9f, 0, 0, 0, 2200, 0.9f, 1, 0.3f, 0, 0, 0, 0, 0, 0},
        {520, 520, 0, 10, 0.55f, 1700, 0.30f, 12, 0.5f, 0, 0, 0, 2300, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 4.5f, 1.6f, 0, 0, 0, 1150, 0.7f, 0, 0, 0, 0, 0, 3, 10, 130},
        {0, 0, 0, 0, 0, 0, 0, 40, 0.40f, 0.55f, 1.7f, 0, 7200, 1.3f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 300, 0.40f, 0.55f, 1.7f, 0, 7200, 1.3f, 1, 0, 0, 0, 0, 0, 0, 0},
        {150, 100, 20, 280, 0.9f, 0, 0, 3, 0.15f, 0, 0, 0, 600, 1.0f, 0, 0.2f, 0, 0, 0, 0, 0, 0},
        {540, 540, 0, 0, 0, 800, 0, 110, 0, 0, 0, 0.55f, 860, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 170, 0.55f, 0.35f, 3.1f, 0, 7200, 1.3f, 1, 0, 31, 0.45f, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 1150, 0.45f, 0.55f, 1.7f, 0, 4800, 1.3f, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 950, 0.25f, 0.75f, 1.35f, 0, 6200, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 55, 0.55f, 0, 0, 0, 5200, 0.9f, 1, 0, 0, 0, 0, 0, 0, 0},
        {820, 820, 0, 45, 0.75f, 2222, 0.18f, 4, 0.25f, 0, 0, 0, 1800, 0.6f, 0, 0, 0, 0, 0, 0, 0, 0},
        {330, 285, 12, 190, 0.85f, 0, 0, 4, 0.10f, 0, 0, 0, 900, 1.0f, 0, 0.1f, 0, 0, 0, 0, 0, 0},
    },
};
static int rk_row(int k)
{
    static const uint8_t ROW[ND] = {RK_BD, RK_BD, RK_SD, RK_SD, RK_RIM, RK_CLAP, RK_CH, RK_OH, RK_TOM, RK_TOM, RK_TOM, RK_COW,
                                    RK_TAMB, RK_CRASH, RK_RIDE, RK_SHAKER, RK_WOOD, RK_CONGA};
    return ROW[k];
}
/* one voice of a machine: the row, with the note's pitch (a tom, a conga), the knobs' TUNE (a pitch) and DECAY (a time) */
static void rk_voice(drum_v *d, int k, int note, int machine, float tune, float dec)
{
    const rk_t *r = &RK[machine][rk_row(k)];
    float sr1 = 1.0f / DK_SR, pf = 1.0f, df = 1.0f, nf = 1.0f, fm;
    int j;
    if (k == D_BD2) {
        pf = 0.9f;
        df = 1.25f;
    } else if (k == D_SD2) {
        pf = 1.15f;
        nf = 0.85f;
    } else if (k == D_LT || k == D_MT || k == D_HT)
        pf = fm_exp2f((float)(note < 41 ? 0 : note > 50 ? 9 : note - 41) * 0.1f);
    else if (k == D_CONGA)
        pf = note == 62 ? 1.35f : note == 63 ? 1.0f : 0.78f;
    pf *= tune;
    d->wave = r->wave;
    d->f = r->f0 * pf * sr1;
    d->fend = r->f1 * pf * sr1;
    d->fmul = dk_decay(r->ft * 0.001f);
    d->inc2 = r->f2 * pf * sr1;
    d->at = r->tl > 0.0f ? 1.0f : 0.0f;
    d->atm = dk_decay(r->td * 0.001f * dec * df);
    d->tone_l = r->tl;
    d->tone2_l = r->l2;
    d->an = 1.0f;
    d->anm = dk_decay(r->nd * 0.001f * dec * nf);
    d->noise_l = r->nl;
    d->metal_l = r->ml;
    d->sqr_l = r->cl;
    d->filt = r->hp;
    d->sq = r->q;
    fm = 1.05f * dk_svf_fmax(r->q);                        /* the filter's own limit: a high corner at a low damping would run away */
    d->sf = fm_minf(dk_svf_f(r->sf), fm);
    d->drv = r->drv;
    d->amd = r->amd;
    d->aminc = r->amr * sr1;
    d->cb = r->cn;
    d->cgap = r->cg * 0.001f * DK_SR;
    d->cc = d->cgap;
    d->ctail = dk_decay(r->ct * 0.001f);
    d->nrate = 0.0f;
    d->rise = 1.0f;
    if (r->ml > 0.0f)
        for (j = 0; j < 6; j++) {
            d->minc[j] = (uint32_t)(METAL[j] * r->mm * tune * sr1 * 4294967296.0f);
            d->mph[j] = rnd();
        }
}

static void drum_on(float *st, int note, float vel, const float *p, int retrig)
{
    static const float PAN[ND] = {0.0f, 0.0f, 0.05f, -0.05f, -0.25f, 0.15f, 0.35f, 0.35f, -0.4f, 0.0f, 0.4f, -0.45f,
                                  0.5f, -0.35f, 0.4f, -0.3f, 0.3f, -0.2f};
    drum_v *d = (drum_v *)st;
    int k = drum_of(note), j, kit = (int)p[3];
    if (kit >= 6 && note >= 62 && note <= 64)
        k = D_CONGA;
    float sr1 = 1.0f / DK_SR, tune = fm_exp2f((p[0] - 50.0f) * (1.0f / 60.0f)),
          dec = fm_exp2f((p[1] - 50.0f) * (1.0f / 40.0f)) * env_dec(p), pn;
    (void)retrig;
    d->kind = (float)k;
    d->t = 0.0f;
    d->vel = vel;
    pn = PAN[k];
    d->gl = 0.5f - 0.5f * pn;
    d->gr = 0.5f + 0.5f * pn;
    d->sq = 1.0f;
    d->held = 1.0f;
    d->rise = 1.0f;
    d->wave = d->nrate = 0.0f;
    d->drv = d->amd = d->aminc = d->amph = d->cb = 0.0f;
    if (k == D_CLAP) {                                     /* the clap: three bursts and a tail */
        d->cb = 3.0f;
        d->cgap = d->cc = 0.010f * DK_SR;
        d->ctail = dk_decay(0.13f);
    } else if (k == D_TAMB) {
        d->amd = 0.45f;
        d->aminc = 31.0f * sr1;
    }
    switch (k) {
    case D_BD1:
    case D_BD2:
        d->f = (k == D_BD1 ? 150.0f : 112.0f) * sr1;
        d->fend = (k == D_BD1 ? 50.0f : 42.0f) * sr1;
        d->fmul = dk_decay(k == D_BD1 ? 0.030f : 0.045f);
        d->at = 1.0f;
        d->atm = dk_decay((k == D_BD1 ? 0.30f : 0.50f) * dec);
        d->tone_l = 1.0f;
        d->an = 1.0f;
        d->anm = dk_decay(0.0025f);
        d->noise_l = 0.30f;
        d->filt = 1.0f;
        d->sf = dk_svf_f(1500.0f);
        break;
    case D_SD1:
    case D_SD2:
        d->f = d->fend = (k == D_SD1 ? 185.0f : 235.0f) * sr1;
        d->inc2 = (k == D_SD1 ? 330.0f : 440.0f) * sr1;
        d->at = 1.0f;
        d->atm = dk_decay((k == D_SD1 ? 0.09f : 0.07f) * dec);
        d->tone_l = 0.45f;
        d->tone2_l = 0.25f;
        d->an = 1.0f;
        d->anm = dk_decay((k == D_SD1 ? 0.15f : 0.21f) * dec);
        d->noise_l = k == D_SD1 ? 0.75f : 0.85f;
        d->filt = k == D_SD2 ? 1.0f : 0.0f;
        d->sf = dk_svf_f(k == D_SD1 ? 2800.0f : 1900.0f);
        d->sq = 1.2f;
        break;
    case D_RIM:
        d->f = d->fend = 520.0f * sr1;
        d->inc2 = 1700.0f * sr1;
        d->at = 1.0f;
        d->atm = dk_decay(0.014f * dec);
        d->tone_l = 0.55f;
        d->tone2_l = 0.30f;
        d->an = 1.0f;
        d->anm = dk_decay(0.012f);
        d->noise_l = 0.5f;
        d->sf = dk_svf_f(2300.0f);
        d->sq = 0.4f;
        break;
    case D_WOOD:                                           /* a wood block: two rounded tones, a knock */
        d->f = d->fend = 820.0f * sr1;
        d->inc2 = 820.0f * 2.71f * sr1;
        d->at = 1.0f;
        d->atm = dk_decay(0.045f * dec);
        d->tone_l = 0.75f;
        d->tone2_l = 0.18f;
        d->an = 1.0f;
        d->anm = dk_decay(0.004f);
        d->noise_l = 0.25f;
        d->sf = dk_svf_f(1800.0f);
        d->sq = 0.6f;
        break;
    case D_SHAKER:                                         /* beads: a soft rise, a hiss */
        d->an = 1.0f;
        d->rise = 0.0f;
        d->anm = dk_decay(0.055f * dec);
        d->noise_l = 0.55f;
        d->filt = 1.0f;
        d->sf = dk_svf_f(5200.0f);
        d->sq = 0.9f;
        break;
    case D_CLAP:
        d->an = 1.0f;
        d->anm = dk_decay(0.0045f);
        d->noise_l = 1.1f;
        d->sf = dk_svf_f(1150.0f);
        d->sq = 0.7f;
        break;
    case D_CH:
    case D_OH:
    case D_CRASH:
    case D_RIDE:
    case D_TAMB:
        d->an = 1.0f;
        d->anm = dk_decay((k == D_CH ? 0.045f : k == D_OH ? 0.36f : k == D_CRASH ? 1.15f : k == D_RIDE ? 0.95f : 0.17f) * dec);
        d->noise_l = k == D_RIDE ? 0.25f : k == D_TAMB ? 0.55f : 0.45f;
        d->metal_l = k == D_RIDE ? 0.75f : k == D_TAMB ? 0.35f : 0.55f;
        d->filt = k != D_RIDE ? 1.0f : 0.0f;
        d->sf = dk_svf_f(k == D_CRASH ? 4800.0f : k == D_RIDE ? 6200.0f : 7200.0f);
        d->sq = k == D_RIDE ? 0.9f : 1.3f;
        for (j = 0; j < 6; j++) {
            float mf = METAL[j] * (k == D_TAMB ? 3.1f : k == D_RIDE ? 1.35f : 1.7f) * tune;
            d->minc[j] = (uint32_t)(mf * sr1 * 4294967296.0f);
            d->mph[j] = rnd();
        }
        break;
    case D_LT:
    case D_MT:
    case D_HT: {
        float base = k == D_LT ? 92.0f : k == D_MT ? 128.0f : 172.0f;
        d->f = base * 1.55f * sr1;
        d->fend = base * sr1;
        d->fmul = dk_decay(0.09f);
        d->at = 1.0f;
        d->atm = dk_decay((k == D_LT ? 0.34f : k == D_MT ? 0.28f : 0.23f) * dec);
        d->tone_l = 0.9f;
        d->an = 1.0f;
        d->anm = dk_decay(0.03f);
        d->noise_l = 0.15f;
        d->sf = dk_svf_f(base * 5.0f);
        break;
    }
    default:
        d->f = d->fend = 540.0f * sr1;
        d->inc2 = 800.0f * sr1;
        d->an = 1.0f;
        d->anm = dk_decay(0.11f * dec);
        d->sqr_l = 0.55f;
        d->sf = dk_svf_f(860.0f);
        d->sq = 0.5f;
        break;
    }
    d->f *= tune;
    d->fend *= tune;
    d->inc2 *= tune;
    if (kit == 1) {                                        /* DUSTY */
        d->f *= 0.86f;
        d->fend *= 0.86f;
        d->inc2 *= 0.86f;
        d->sf = fm_minf(d->sf, dk_svf_f(k == D_SHAKER ? 4200.0f : 3200.0f));
        for (j = 0; j < 6; j++)
            d->minc[j] = (uint32_t)((float)d->minc[j] * 0.8f);
    } else if (kit == 2) {                                 /* BRUSH */
        if (k == D_SD1 || k == D_SD2 || k == D_CLAP) {
            d->kind = (float)D_SD2;
            d->tone_l *= 0.2f;
            d->tone2_l *= 0.15f;
            d->noise_l = 0.6f;
            d->an = 1.0f;
            d->anm = dk_decay((k == D_CLAP ? 0.45f : 0.30f) * dec);
            d->filt = 0.0f;
            d->sf = dk_svf_f(3400.0f);
            d->sq = 1.5f;
        } else if (k == D_CH || k == D_OH || k == D_RIDE || k == D_CRASH) {
            d->metal_l *= 0.45f;
            d->noise_l *= 1.1f;
            d->anm = dk_decay((k == D_CH ? 0.035f : k == D_OH ? 0.24f : 0.8f) * dec);
        } else if (k == D_BD1 || k == D_BD2) {
            d->atm = dk_decay(0.22f * dec);
            d->noise_l = 0.08f;
        } else if (k != D_SHAKER && k != D_WOOD)
            d->noise_l *= 0.4f;
    } else if (kit == 3) {                                 /* CHIP: a triangle kick, 1-bit noise snare and hats, pulse blocks */
        d->wave = 1.0f;
        d->metal_l = 0.0f;
        if (k == D_BD1 || k == D_BD2) {
            d->wave = 2.0f;
            d->f = (k == D_BD1 ? 135.0f : 105.0f) * sr1 * tune;
            d->fend = 46.0f * sr1 * tune;
            d->fmul = dk_decay(0.022f);
            d->atm = dk_decay((k == D_BD1 ? 0.16f : 0.24f) * dec);
            d->noise_l = 0.22f;
            d->nrate = 0.3f;
        } else if (k == D_SD1 || k == D_SD2 || k == D_CLAP) {
            d->tone_l = k == D_CLAP ? 0.0f : 0.2f;
            d->tone2_l = 0.0f;
            d->inc2 = 0.0f;
            d->noise_l = 1.0f;
            d->nrate = k == D_SD1 ? 0.42f : 0.24f;
            d->atm = dk_decay(0.03f * dec);
            d->anm = dk_decay((k == D_CLAP ? 0.16f : k == D_SD1 ? 0.12f : 0.17f) * dec);
            d->filt = 0.0f;
            d->sf = dk_svf_f(4200.0f);
            d->sq = 0.9f;
        } else if (k == D_CH || k == D_OH || k == D_CRASH || k == D_RIDE || k == D_TAMB || k == D_SHAKER) {
            d->noise_l = 0.42f;
            d->nrate = k == D_CH ? 0.9f : k == D_SHAKER ? 0.6f : 0.7f;
            d->filt = 1.0f;
            d->sf = dk_svf_f(6000.0f);
            d->anm = dk_decay((k == D_CH ? 0.035f : k == D_OH ? 0.22f : k == D_CRASH ? 0.7f : k == D_RIDE ? 0.5f : 0.1f) * dec);
        } else if (k == D_LT || k == D_MT || k == D_HT) {
            d->wave = 2.0f;
            d->noise_l = 0.0f;
            d->fmul = dk_decay(0.05f);
            d->atm = dk_decay((k == D_LT ? 0.22f : k == D_MT ? 0.18f : 0.15f) * dec);
        }
    } else if (kit == 4) {                                /* SYNTH: the electro kit: a longer kick, zapped toms, gated snares */
        if (k == D_BD1 || k == D_BD2) {
            d->f = (k == D_BD1 ? 190.0f : 150.0f) * sr1 * tune;
            d->fend = 46.0f * sr1 * tune;
            d->fmul = dk_decay(0.05f);
            d->atm = dk_decay((k == D_BD1 ? 0.34f : 0.5f) * dec);
            d->noise_l = 0.45f;
        } else if (k == D_SD1 || k == D_SD2) {
            d->tone_l = 0.62f;
            d->tone2_l = 0.30f;
            d->atm = dk_decay(0.12f * dec);
            d->anm = dk_decay((k == D_SD1 ? 0.24f : 0.32f) * dec);
            d->noise_l *= 1.15f;
            d->sf = dk_svf_f(k == D_SD1 ? 3600.0f : 2600.0f);
        } else if (k == D_LT || k == D_MT || k == D_HT) {
            float base = (k == D_LT ? 92.0f : k == D_MT ? 128.0f : 172.0f) * sr1 * tune;
            d->f = base * 3.2f;
            d->fend = base;
            d->fmul = dk_decay(0.2f);
            d->atm = dk_decay((k == D_LT ? 0.44f : k == D_MT ? 0.36f : 0.3f) * dec);
        } else if (k == D_CLAP) {
            d->noise_l = 1.25f;
            d->sf = dk_svf_f(1600.0f);
        } else if (k == D_CH || k == D_OH) {
            d->noise_l = 0.5f;
            d->metal_l = 0.22f;
            d->sf = dk_svf_f(6400.0f);
        }
    } else if (kit == 5) {                                 /* WAR: the big skins, the low hits, a dark anvil for the metal */
        if (k == D_BD1 || k == D_BD2) {
            d->f = (k == D_BD1 ? 98.0f : 80.0f) * sr1 * tune;
            d->fend = (k == D_BD1 ? 36.0f : 31.0f) * sr1 * tune;
            d->fmul = dk_decay(0.07f);
            d->atm = dk_decay((k == D_BD1 ? 0.8f : 1.1f) * dec);
            d->noise_l = 0.4f;
            d->sf = dk_svf_f(650.0f);
        } else if (k == D_SD1 || k == D_SD2 || k == D_CLAP) {
            d->f = d->fend = (k == D_SD1 ? 120.0f : k == D_SD2 ? 150.0f : 90.0f) * sr1 * tune;
            d->inc2 = 0.0f;
            d->tone_l = k == D_CLAP ? 0.35f : 0.6f;
            d->tone2_l = 0.0f;
            d->at = 1.0f;
            d->atm = dk_decay(0.38f * dec);
            d->noise_l = 0.6f;
            d->filt = 0.0f;
            d->sf = dk_svf_f(560.0f);
            d->sq = 0.9f;
            d->anm = dk_decay(0.32f * dec);
        } else if (k == D_LT || k == D_MT || k == D_HT) {
            float base = (k == D_LT ? 62.0f : k == D_MT ? 80.0f : 104.0f) * sr1 * tune;
            d->f = base * 1.4f;
            d->fend = base;
            d->fmul = dk_decay(0.12f);
            d->atm = dk_decay((k == D_LT ? 0.9f : k == D_MT ? 0.75f : 0.6f) * dec);
            d->noise_l = 0.35f;
            d->sf = dk_svf_f(500.0f);
        } else if (k == D_CH || k == D_OH || k == D_CRASH || k == D_RIDE || k == D_TAMB) {
            for (j = 0; j < 6; j++)
                d->minc[j] = (uint32_t)((float)d->minc[j] * 0.42f);
            d->noise_l *= 0.35f;
            d->filt = 0.0f;
            d->sf = dk_svf_f(2600.0f);
            d->sq = 0.6f;
            d->anm = dk_decay((k == D_CH ? 0.2f : k == D_OH ? 0.9f : k == D_CRASH ? 2.2f : k == D_RIDE ? 1.6f : 0.5f) * dec);
        } else if (k == D_WOOD || k == D_RIM) {
            d->f *= 0.55f;
            d->fend *= 0.55f;
            d->inc2 *= 0.55f;
        }
    }
    if (kit == 0 || kit >= 6)
        rk_voice(d, k, note, kit == 0 ? 4 : kit - 6, tune, dec);
    d->ph = d->ph2 = d->lo = d->bp = 0.0f;
    d->nc = 1.0f;
    d->nv = 1.0f;
}
static void drum_off(float *st, const float *p)       /* a hit again, or a hat choked: it dies at once */
{
    drum_v *d = (drum_v *)st;
    (void)p;
    if (d->held == 0.0f)
        return;
    d->held = 0.0f;
    if (d->t > 0.0f) {
        d->anm = fm_minf(d->anm, dk_decay(0.008f));
        d->atm = fm_minf(d->atm, dk_decay(0.012f));
    }
}
static int drum_render(float *st, float *l, float *r, int n, const float *p)
{
    drum_v *d = (drum_v *)st;
    int j;
    float a = d->vel * 0.44f;
    (void)p;
    for (j = 0; j < n; j++) {
        float x = 0.0f, y = 0.0f, hp;
        if (d->at > 1e-4f) {
            d->f = d->fend + (d->f - d->fend) * d->fmul;
            d->ph = dk_wrap1(d->ph + d->f);
            x = d->wave == 0.0f ? dk_sinx(d->ph) : d->wave == 1.0f ? (d->ph < 0.5f ? 0.7f : -0.7f) :
                d->ph < 0.5f ? 4.0f * d->ph - 1.0f : 3.0f - 4.0f * d->ph;
            if (d->drv > 0.0f)                             /* a machine's edge: the tone squeezed (a sine toward a square) */
                x = x * (1.0f + d->drv) / (1.0f + d->drv * fm_fabsf(x));
            x *= d->tone_l;
            if (d->inc2 > 0.0f) {
                d->ph2 = dk_wrap1(d->ph2 + d->inc2);
                x += (d->wave == 1.0f ? (d->ph2 < 0.5f ? 0.7f : -0.7f) : dk_sinx(d->ph2)) * d->tone2_l;
            }
            x *= d->at;
            d->at *= d->atm;
        }
        if (d->an > 1e-4f) {
            float s = 0.0f;
            if (d->noise_l > 0.0f) {
                if (d->nrate > 0.0f) {                     /* a chip's noise: one bit, held (a lower pitch for a lower rate) */
                    d->nc += d->nrate;
                    if (d->nc >= 1.0f) {
                        d->nc -= 1.0f;
                        d->nv = wnoise() > 0.0f ? 0.8f : -0.8f;
                    }
                    s = d->nv * d->noise_l;
                } else
                    s = wnoise() * d->noise_l;
            }
            if (d->metal_l > 0.0f) {
                int m;
                float mm = 0.0f;
                for (m = 0; m < 6; m++) {
                    d->mph[m] += d->minc[m];
                    mm += (d->mph[m] & 0x80000000u) ? 1.0f : -1.0f;
                }
                s += mm * (1.0f / 6.0f) * d->metal_l;
            }
            if (d->sqr_l > 0.0f) {
                d->ph = dk_wrap1(d->ph + d->f);
                d->ph2 = dk_wrap1(d->ph2 + d->inc2);
                s += ((d->ph < 0.5f ? 1.0f : -1.0f) + (d->ph2 < 0.5f ? 0.8f : -0.8f)) * d->sqr_l;
            }
            d->lo += d->sf * d->bp;
            hp = s - d->lo - d->sq * d->bp;
            d->bp = fm_flush(d->bp + d->sf * hp);
            d->lo = fm_flush(d->lo);
            s = d->filt != 0.0f ? hp : d->bp;
            if (d->amd > 0.0f) {                           /* a tambourine's shiver, a guiro's scrape */
                d->amph = dk_wrap1(d->amph + d->aminc);
                s *= 1.0f - d->amd + d->amd * dk_sinx(d->amph);
            }
            if (d->rise < 1.0f) {                          /* the shaker's beads gather */
                d->rise = fm_minf(1.0f, d->rise + 1.0f / (0.012f * DK_SR));
                s *= d->rise;
            } else
                d->an *= d->anm;
            y = s * d->an;
        }
        if (d->cb > 0.0f) {                                /* the clap's hands, one after the other, then its room */
            d->cc -= 1.0f;
            if (d->cc <= 0.0f) {
                d->cc += d->cgap;
                d->cb -= 1.0f;
                if (d->cb <= 0.0f) {
                    d->an = 0.7f;
                    d->anm = d->ctail;
                } else
                    d->an = 0.85f;
            }
        }
        d->t += 1.0f;
        x = (x + y) * a;
        l[j] += x * d->gl;
        r[j] += x * d->gr;
    }
    return !(d->at <= 1e-4f && d->an <= 1e-4f && d->t > 0.04f * DK_SR);
}
static float drum_level(const float *st)
{
    const drum_v *d = (const drum_v *)st;
    return (d->at + d->an) * d->vel;
}
/* DUST: the machine's grain (a lower rate held, fewer bits: 12 at least) and a worn top */
static void kit_bus(float *bs, float *l, float *r, int n, const float *p)
{
    float g = p[2] * 0.01f, step = 1.0f / (1.0f + g * 1.8f), q = fm_exp2f(11.0f - g * 4.0f), iq = 1.0f / q,
          k = dk_lpk(fm_maxf(1800.0f, ((int)p[3] >= 6 ? 17000.0f : 13000.0f) * fm_exp2f(-3.0f * g) * ((int)p[3] == 1 ? 0.6f : (int)p[3] == 5 ? 0.7f : (int)p[3] == 7 ? 0.65f : 1.0f)));
    if ((int)p[3] == 3) {                                  /* CHIP: a coarser grain (about 6 bits), no worn top */
        q = fm_exp2f(7.0f - g * 2.0f);
        iq = 1.0f / q;
        step = 1.0f / (1.0f + g * 0.8f);
        k = dk_lpk(8500.0f);
    }
    int j;
    for (j = 0; j < n; j++) {
        float a = l[j], b = r[j];
        if (g > 0.0f) {
            bs[0] += step;
            if (bs[0] >= 1.0f) {
                bs[0] -= 1.0f;
                bs[1] = fm_floorf(a * q + 0.5f) * iq;
                bs[2] = fm_floorf(b * q + 0.5f) * iq;
            }
            a = bs[1];
            b = bs[2];
        }
        bs[3] += k * (a - bs[3]);                          /* two poles: the top worn away */
        bs[4] += k * (b - bs[4]);
        bs[5] += k * (bs[3] - bs[5]);
        bs[6] += k * (bs[4] - bs[6]);
        l[j] = bs[5];
        r[j] = bs[6];
    }
}
const dk_engine_t ENG_DUSTKIT = {"DRUMKIT", EF_DRUM | EF_POLY, 6, 9, 4, KIT_P, drum_on, drum_off, drum_render,
                                 drum_level, kit_bus, 8, KIT_LANE, KIT_NOTE, R_DRUM};
