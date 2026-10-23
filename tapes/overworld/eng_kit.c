/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* DUSTKIT: OVERWORLD's drum machine made dusty: a soft kick, a brushed snare, a rim, a shaker, a closed and an open
 * hat, a dusty clap, a wood block (and its toms, cowbell, tambourine, cymbals for MIDI). Each sound made (a tone that
 * falls, filtered noise, six square waves for the metal); the track's sum through 12 bits and a low-pass (DUST).
 * KIT: 707 (clean), DUSTY (lower, darker, a sampler's worn copy), BRUSH (wire brushes, soft hats, a felt beater).
 * Part of tape_dsp.c. */

enum { D_BD1, D_BD2, D_SD1, D_SD2, D_RIM, D_CLAP, D_CH, D_OH, D_LT, D_MT, D_HT, D_COW, D_TAMB, D_CRASH, D_RIDE,
       D_SHAKER, D_WOOD, ND };
typedef struct {
    uint32_t mph[6], minc[6];
    float t, vel, gl, gr;
    float ph, f, fend, fmul, ph2, inc2;
    float at, atm, an, anm;
    float lo, bp, sf, sq;
    float tone_l, tone2_l, noise_l, metal_l, sqr_l, filt, kind, rise, held;
} drum_v;
CHECK_SIZE(drum_v);

static const char *const KIT_NAMES[] = {"707", "DUSTY", "BRUSH"};
static const dk_param_t KIT_P[] = {
    {"TUNE", 0, 100, 50, VZ_BAR, 0}, {"DECAY", 0, 100, 50, VZ_ENV, 0}, {"DUST", 0, 100, 45, VZ_DUST, 0},
    {"KIT", 0, 2, 1, VZ_TYPE, KIT_NAMES},
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

static void drum_on(float *st, int note, float vel, const float *p, int retrig)
{
    static const float PAN[ND] = {0.0f, 0.0f, 0.05f, -0.05f, -0.25f, 0.15f, 0.35f, 0.35f, -0.4f, 0.0f, 0.4f, -0.45f,
                                  0.5f, -0.35f, 0.4f, -0.3f, 0.3f};
    drum_v *d = (drum_v *)st;
    int k = drum_of(note), j, kit = (int)p[3];
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
    }
    d->ph = d->ph2 = d->lo = d->bp = 0.0f;
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
    int j, m, k = (int)d->kind;
    float a = d->vel * 0.44f;
    /* the state in registers for the loop (the outputs might alias it otherwise) */
    float t = d->t, gl = d->gl, gr = d->gr, ph = d->ph, f = d->f, fend = d->fend, fmul = d->fmul, ph2 = d->ph2;
    float inc2 = d->inc2, at = d->at, atm = d->atm, an = d->an, anm = d->anm, lo = d->lo, bp = d->bp, sf = d->sf;
    float sq = d->sq, tone_l = d->tone_l, tone2_l = d->tone2_l, noise_l = d->noise_l, metal_l = d->metal_l;
    float sqr_l = d->sqr_l, filt = d->filt, rise = d->rise;
    uint32_t mph[6], minc[6];
    (void)p;
    for (m = 0; m < 6; m++) {
        mph[m] = d->mph[m];
        minc[m] = d->minc[m];
    }
    for (j = 0; j < n; j++) {
        float x = 0.0f, y = 0.0f, hp;
        if (at > 1e-4f) {
            f = fend + (f - fend) * fmul;
            ph = dk_wrap1(ph + f);
            x = dk_sinx(ph) * tone_l;
            if (inc2 > 0.0f) {
                ph2 = dk_wrap1(ph2 + inc2);
                x += dk_sinx(ph2) * tone2_l;
            }
            x *= at;
            at *= atm;
        }
        if (an > 1e-4f) {
            float s = 0.0f;
            if (noise_l > 0.0f)
                s = wnoise() * noise_l;
            if (metal_l > 0.0f) {
                float mm = 0.0f;
                for (m = 0; m < 6; m++) {
                    mph[m] += minc[m];
                    mm += (mph[m] & 0x80000000u) ? 1.0f : -1.0f;
                }
                s += mm * (1.0f / 6.0f) * metal_l;
            }
            if (sqr_l > 0.0f) {
                ph = dk_wrap1(ph + f);
                ph2 = dk_wrap1(ph2 + inc2);
                s += ((ph < 0.5f ? 1.0f : -1.0f) + (ph2 < 0.5f ? 0.8f : -0.8f)) * sqr_l;
            }
            lo += sf * bp;
            hp = s - lo - sq * bp;
            bp = fm_flush(bp + sf * hp);
            lo = fm_flush(lo);
            s = filt != 0.0f ? hp : bp;
            if (k == D_TAMB)
                s *= 0.55f + 0.45f * dk_sinx(t * (31.0f / DK_SR));
            if (rise < 1.0f) {                             /* the shaker's beads gather */
                rise = fm_minf(1.0f, rise + 1.0f / (0.012f * DK_SR));
                s *= rise;
            } else
                an *= anm;
            y = s * an;
        }
        if (k == D_CLAP) {
            uint32_t ti = (uint32_t)t;
            if (ti == (uint32_t)(0.010f * DK_SR) || ti == (uint32_t)(0.020f * DK_SR))
                an = 0.85f;
            else if (ti == (uint32_t)(0.030f * DK_SR)) {
                an = 0.7f;
                anm = dk_decay(0.13f);
            }
        }
        t += 1.0f;
        x = (x + y) * a;
        l[j] += x * gl;
        r[j] += x * gr;
    }
    for (m = 0; m < 6; m++)
        d->mph[m] = mph[m];
    d->t = t;
    d->ph = ph;
    d->f = f;
    d->ph2 = ph2;
    d->at = at;
    d->an = an;
    d->anm = anm;
    d->lo = lo;
    d->bp = bp;
    d->rise = rise;
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
          k = dk_lpk(fm_maxf(1800.0f, 13000.0f * fm_exp2f(-3.0f * g) * ((int)p[3] == 1 ? 0.6f : 1.0f)));
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
const dk_engine_t ENG_DUSTKIT = {"DUSTKIT", EF_DRUM | EF_POLY, 6, 9, 4, KIT_P, drum_on, drum_off, drum_render,
                                 drum_level, kit_bus, 8, KIT_LANE, KIT_NOTE, R_DRUM};
