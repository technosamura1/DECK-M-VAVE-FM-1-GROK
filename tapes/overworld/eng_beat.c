/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* BEATKIT: a drum machine of samples (kits.h, made by tools/make_kits.py: after the 707, 808, 909, CR-78, a dusty boom
 * bap and a cassette; kept clean). What carries a machine's character is sampled (the kick, the snare, the rim, the
 * clap, the closed hat, a percussion); its toms, cowbell, open hat, cymbals and shaker are synthesized, as the machines'
 * own were (DUSTKIT's voices). TONE and CRUSH work on the track's sum (its bus), as a channel's EQ and a crusher would.
 * Part of tape_dsp.c, after eng_kit.c. */

typedef struct {
    uint32_t at, n;                            /* the first byte in BK_DATA, the samples */
    uint16_t rate;
    float gain;                                /* its place in the kit's mix */
} bk_smp_t;
#include "kits.h"
enum { BK_KICK, BK_SNARE, BK_RIM, BK_CLAP, BK_HAT, BK_PERC };
static float bk_mu[256] DK_POOL;               /* mu-law to -1..1 */
static void beat_tables(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        int q = i < 128 ? i : i - 256;
        float a = (float)(q < 0 ? -q : q) * (1.0f / 127.0f), y = (fm_exp2f(a * 8.0f) - 1.0f) * (1.0f / 255.0f);
        bk_mu[i] = q < 0 ? -y : y;
    }
}

typedef struct {
    float synth;                               /* 1: a synthesized voice (d) */
    float pos, step, env, envm, amp, gl, gr, held;
    float b0, b1, a1, a2, z1, z2;              /* the images of its rate taken away (a low-pass under its Nyquist) */
    uint32_t at, n;
    drum_v d;
} beat_v;
CHECK_SIZE(beat_v);

static const dk_param_t BEAT_P[] = {
    {"TUNE", 0, 100, 50, VZ_BAR, 0},  {"DECAY", 0, 100, 85, VZ_ENV, 0},       {"TONE", 0, 100, 50, VZ_SUN, 0},
    {"KIT", 0, BK_NKIT - 1, 0, VZ_TYPE, BK_KIT_NAME}, {"CRUSH", 0, 100, 0, VZ_DUST, 0}, {"KICK", 0, 100, 50, VZ_BAR, 0},
};
static const char *const BEAT_LANE[8] = {"KICK", "SNARE", "RIM", "SHAKER", "HAT", "OPEN", "CLAP", "PERC"};
/* each kit's synthesized voices: DUSTKIT's KIT (0 clean, 1 dusty) and their pitch */
static const uint8_t BK_SYN_KIT[BK_NKIT] = {0, 0, 0, 0, 1, 1};
static const float BK_SYN_TUNE[BK_NKIT] = {1.05f, 0.9f, 1.08f, 1.2f, 0.9f, 0.95f};

/* a note's sample (and its pitch), or -1: a synthesized voice */
static int beat_slot(int note, float *pitch)
{
    *pitch = 1.0f;
    switch (note) {
    case 35: *pitch = 0.94f; return BK_KICK;
    case 36: return BK_KICK;
    case 37: return BK_RIM;
    case 38: return BK_SNARE;
    case 40: *pitch = 1.06f; return BK_SNARE;
    case 39: return BK_CLAP;
    case 42: return BK_HAT;
    case 44: *pitch = 0.94f; return BK_HAT;
    case 75: *pitch = 1.26f; return BK_PERC;
    case 76: return BK_PERC;
    case 77: *pitch = 0.8f; return BK_PERC;
    case 60: case 61: case 62: case 63: case 64: *pitch = fm_exp2f((float)(note - 62) * (1.0f / 12.0f)); return BK_PERC;
    default: return -1;
    }
}

static void beat_lp(beat_v *v, float fc)            /* a 2-pole Butterworth (bilinear: nothing left at Nyquist) */
{
    float k = fm_tanf(3.1415927f * fm_minf(fc, 18000.0f) / DK_SR), nrm = 1.0f / (1.0f + 1.4142136f * k + k * k);
    v->b0 = k * k * nrm;
    v->b1 = 2.0f * v->b0;
    v->a1 = 2.0f * (k * k - 1.0f) * nrm;
    v->a2 = (1.0f - 1.4142136f * k + k * k) * nrm;
    v->z1 = v->z2 = 0.0f;
}
static float bk_tmp[2][DK_BLK] DK_POOL;

static void beat_on(float *st, int note, float vel, const float *p, int retrig)
{
    beat_v *v = (beat_v *)st;
    int kit = (int)fm_clampf(p[3], 0.0f, (float)(BK_NKIT - 1)), slot;
    float pitch, tune = fm_exp2f((p[0] - 50.0f) * (1.0f / 50.0f)), pn;
    v->held = 1.0f;
    slot = beat_slot(note, &pitch);
    if (slot < 0) {                                       /* DUSTKIT's voice, to this kit's taste */
        float q[PX_N];
        int i;
        for (i = 0; i < PX_N; i++)
            q[i] = p[i];
        q[0] = fm_clampf(50.0f + (p[0] - 50.0f) * 1.2f + 60.0f * fm_log2f(BK_SYN_TUNE[kit]), 0.0f, 100.0f);
        q[1] = fm_clampf(50.0f + (p[1] - 85.0f) * 0.8f, 0.0f, 100.0f);
        q[2] = 0.0f;
        q[3] = (float)BK_SYN_KIT[kit];
        v->synth = 1.0f;
        drum_on((float *)&v->d, note, vel, q, retrig);
        v->gl = v->d.gl;                                  /* its pan after the filter (the voice renders mono) */
        v->gr = v->d.gr;
        v->d.gl = 1.0f;
        v->d.gr = 0.0f;
        beat_lp(v, 9000.0f);                              /* the squares' ring, its aliasing up top taken off */
        return;
    }
    {
        const bk_smp_t *s = &BK_SMP[kit][slot];
        float dec = 0.025f * fm_exp2f(p[1] * (1.0f / 12.5f));             /* 100: the sample's own length */
        static const float PAN[6] = {0.0f, 0.0f, 0.08f, -0.12f, -0.25f, 0.3f};
        if (slot == BK_KICK)
            tune *= fm_exp2f((p[5] - 50.0f) * (1.0f / 50.0f));
        v->synth = 0.0f;
        v->at = s->at;
        v->n = s->n;
        v->pos = 0.0f;
        v->step = (float)s->rate * (1.0f / DK_SR) * tune * pitch;
        v->env = 1.0f;
        v->envm = p[1] >= 99.5f ? 1.0f : dk_decay(dec * env_dec(p));
        v->amp = vel * (0.35f + 0.65f * vel) * s->gain * 0.62f;
        beat_lp(v, 0.45f * (float)s->rate * tune * pitch);   /* the images of its rate (just under its top) */
        pn = PAN[slot];
        v->gl = 0.5f - 0.5f * pn;
        v->gr = 0.5f + 0.5f * pn;
    }
}
static void beat_off(float *st, const float *p)       /* a hit again, or a hat choked: it dies at once */
{
    beat_v *v = (beat_v *)st;
    if (v->synth != 0.0f) {
        drum_off((float *)&v->d, p);
        return;
    }
    if (v->held == 0.0f)
        return;
    v->held = 0.0f;
    if (v->pos > 0.0f)
        v->envm = fm_minf(v->envm, dk_decay(0.008f));
}
static int beat_render(float *st, float *l, float *r, int n, const float *p)
{
    beat_v *v = (beat_v *)st;
    if (v->synth != 0.0f) {
        float *x = bk_tmp[0], b0 = v->b0, b1 = v->b1, a1 = v->a1, a2 = v->a2, z1 = v->z1, z2 = v->z2;
        int j, on;
        for (j = 0; j < n; j++)
            x[j] = 0.0f;
        on = drum_render((float *)&v->d, x, bk_tmp[1], n, p);
        for (j = 0; j < n; j++) {
            float y = b0 * x[j] + z1;
            z1 = b1 * x[j] - a1 * y + z2;
            z2 = b0 * x[j] - a2 * y;
            l[j] += y * v->gl;
            r[j] += y * v->gr;
        }
        v->z1 = fm_flush(z1);
        v->z2 = fm_flush(z2);
        return on || fm_fabsf(z1) > 1e-5f;
    }
    {
        const uint8_t *b = BK_DATA + v->at;
        float pos = v->pos, step = v->step, env = v->env, envm = v->envm, amp = v->amp, gl = v->gl, gr = v->gr;
        float b0 = v->b0, b1 = v->b1, a1 = v->a1, a2 = v->a2, z1 = v->z1, z2 = v->z2;
        float last = (float)(v->n - 2u);
        int j;
        for (j = 0; j < n; j++) {
            int i;
            float f, xm, x0, x1, x2, c1, c2, c3, x, y;
            if (pos >= last || env < 1e-4f) {
                env = 0.0f;
                break;
            }
            i = (int)pos;
            f = pos - (float)i;
            xm = i > 0 ? bk_mu[b[i - 1]] : 0.0f;          /* four points, a cubic through them (Hermite) */
            x0 = bk_mu[b[i]];
            x1 = bk_mu[b[i + 1]];
            x2 = bk_mu[b[i + 2]];
            c1 = 0.5f * (x1 - xm);
            c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
            x = (((c3 * f + c2) * f + c1) * f + x0) * env * amp;
            y = b0 * x + z1;                               /* (transposed direct form II) */
            z1 = b1 * x - a1 * y + z2;
            z2 = b0 * x - a2 * y;
            pos += step;
            env *= envm;
            l[j] += y * gl;
            r[j] += y * gr;
        }
        v->z1 = fm_flush(z1);
        v->z2 = fm_flush(z2);
        v->pos = pos;
        v->env = env;
        return env > 0.0f || fm_fabsf(z1) > 1e-5f;
    }
}
static float beat_level(const float *st)
{
    const beat_v *v = (const beat_v *)st;
    if (v->synth != 0.0f)
        return drum_level((const float *)&v->d);
    return v->env * v->amp * 2.0f;
}
/* the bus: TONE (under 50 darker, over it thinner) and CRUSH (a lower rate held, fewer bits) on the kit's sum */
static void beat_bus(float *bs, float *l, float *r, int n, const float *p)
{
    float tone = p[2], c = p[4] * 0.01f;
    int j;
    if (tone < 49.5f) {
        float k = dk_lpk(10000.0f * fm_exp2f((tone - 50.0f) * (1.0f / 12.0f)));   /* 0: ~550 Hz */
        for (j = 0; j < n; j++) {
            bs[0] += k * (l[j] - bs[0]);
            bs[1] += k * (r[j] - bs[1]);
            bs[2] += k * (bs[0] - bs[2]);
            bs[3] += k * (bs[1] - bs[3]);
            l[j] = bs[2];
            r[j] = bs[3];
        }
    } else if (tone > 50.5f) {
        float kh = dk_lpk(25.0f * fm_exp2f((tone - 50.0f) * (1.0f / 10.0f)));   /* 100: ~800 Hz up */
        for (j = 0; j < n; j++) {
            bs[7] += kh * (l[j] - bs[7]);
            bs[8] += kh * (r[j] - bs[8]);
            l[j] -= bs[7];
            r[j] -= bs[8];
        }
    }
    if (c > 0.005f) {
        float step = 1.0f / (1.0f + c * c * 9.0f), q = fm_exp2f(15.0f - 11.0f * c), iq = 1.0f / q;
        for (j = 0; j < n; j++) {
            bs[4] += step;
            if (bs[4] >= 1.0f) {
                bs[4] -= 1.0f;
                bs[5] = fm_floorf(l[j] * q + 0.5f) * iq;
                bs[6] = fm_floorf(r[j] * q + 0.5f) * iq;
            }
            l[j] = bs[5];
            r[j] = bs[6];
        }
    }
}
static const uint8_t BEAT_NOTE[8] = {36, 38, 37, 70, 42, 46, 39, 76};
const dk_engine_t ENG_BEATKIT = {"BEATKIT", EF_DRUM | EF_POLY, 8, 4, 6, BEAT_P, beat_on, beat_off, beat_render,
                                 beat_level, beat_bus, 8, BEAT_LANE, BEAT_NOTE, R_DRUM};
