/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's track effects (AMOUNT and two more knobs each, 0..1) and its master (DUST, SLEEP). Part of tape_dsp.c.
 * The state: f[] floats, buf a 16-bit line (DK_FXBUF). */

static inline void fxbuf_put(dk_fxst_t *s, float x)
{
    s->buf[s->w & (DK_FXBUF - 1)] = (int16_t)(fm_clampf(x, -0.999f, 0.999f) * 32767.0f);
    s->w++;
}
static inline float fxbuf_get(const dk_fxst_t *s, float d)   /* d samples back (fractional) */
{
    /* the write count within the line, not the whole count: past 2^23 samples (3 minutes) a float of the whole count
     * had no fraction left, and the delay stepped by 1, 2, 4... samples (a zipper in the chorus and the wow) */
    float rp = (float)((s->w - 1u) & (DK_FXBUF - 1)) - d, fr;
    int i0;
    uint32_t a, b;
    i0 = (int)fm_floorf(rp);
    fr = rp - (float)i0;
    a = (uint32_t)i0 & (DK_FXBUF - 1);
    b = (a + 1u) & (DK_FXBUF - 1);
    return ((float)s->buf[a] + ((float)s->buf[b] - (float)s->buf[a]) * fr) * (1.0f / 32768.0f);
}

/* VINYL: crackle (a), hiss (b), a softened top; AMOUNT how much of it */
static void fx_vinyl(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, k = dk_lpk(15000.0f * fm_exp2f(-1.6f * amt)), dens = amt * (0.2f + 1.6f * a);
    int j;
    for (j = 0; j < n; j++) {
        float c = 0.0f, h;
        if ((float)(rnd() & 65535u) < dens * 6.0f) {       /* a speck of dust */
            f[4] = (0.2f + 0.8f * frnd()) * (frnd() < 0.5f ? -1.0f : 1.0f);
            f[5] = dk_decay(0.0003f + 0.0012f * frnd());
        }
        if (f[4] != 0.0f) {
            c = f[4] * wnoise();
            f[4] *= f[5];
            if (fm_fabsf(f[4]) < 1e-3f)
                f[4] = 0.0f;
        }
        f[6] += 0.3f * (c - f[6]);
        h = wnoise() * 0.006f * b * amt;
        f[0] += k * (l[j] - f[0]);
        f[1] += k * (r[j] - f[1]);
        l[j] = f[0] + (f[6] * 0.09f + h) * amt;
        r[j] = f[1] + (f[6] * 0.08f + h) * amt;
    }
}

/* a slow LFO's phase n samples on (wrapped) */
static inline float lfo_step(float ph, float inc, int n)
{
    ph += inc * (float)n;
    return ph - (float)(int)ph;
}

/* TAPE: wow and flutter (a), saturation (b), the top worn with AMOUNT */
static void fx_tape(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, k = dk_lpk(16000.0f * fm_exp2f(-1.9f * amt)), drive = 1.0f + 2.5f * b * amt,
          mk = (1.0f + 0.15f * (drive - 1.0f)) / drive, depth = (8.0f + 60.0f * a) * amt;
    float d, dd;
    int j;
    /* the wow and the flutter are slow: their sines once a block, a straight line between */
    d = 80.0f + depth * (0.5f + 0.5f * dk_sinx(f[2])) + depth * 0.12f * dk_sinx(f[3]);
    f[2] = lfo_step(f[2], 0.53f / DK_SR, n);
    f[3] = lfo_step(f[3], 6.1f / DK_SR, n);
    dd = (80.0f + depth * (0.5f + 0.5f * dk_sinx(f[2])) + depth * 0.12f * dk_sinx(f[3]) - d) / (float)n;
    for (j = 0; j < n; j++) {
        float m, x;
        d += dd;
        m = (l[j] + r[j]) * 0.5f;
        fxbuf_put(s, m);
        x = fxbuf_get(s, d);
        f[0] += k * ((x + (l[j] - m)) - f[0]);
        f[1] += k * ((x + (r[j] - m)) - f[1]);
        l[j] = fm_tanhf(f[0] * drive) * mk;
        r[j] = fm_tanhf(f[1] * drive) * mk;
    }
}

/* LOFI: fewer bits (a), a lower rate (b), a low-pass; AMOUNT the mix */
static void fx_lofi(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, q = fm_exp2f(12.0f - 8.0f * a), iq = 1.0f / q, step = 1.0f / (1.0f + 7.0f * b),
          k = dk_lpk(12000.0f * fm_exp2f(-2.0f * b) * fm_exp2f(-1.0f * a));
    int j;
    for (j = 0; j < n; j++) {
        float x, y;
        f[0] += step;
        if (f[0] >= 1.0f) {
            f[0] -= 1.0f;
            f[1] = fm_floorf(l[j] * q + 0.5f) * iq;
            f[2] = fm_floorf(r[j] * q + 0.5f) * iq;
        }
        f[3] += k * (f[1] - f[3]);
        f[4] += k * (f[2] - f[4]);
        x = l[j] + (f[3] - l[j]) * amt;
        y = r[j] + (f[4] - r[j]) * amt;
        l[j] = x;
        r[j] = y;
    }
}

/* CHORUS: two voices a little late, swaying apart (RATE a, DEPTH b); AMOUNT the mix */
static void fx_chorus(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, rate = (0.15f + 2.2f * a * a) / DK_SR, depth = 30.0f + 220.0f * b;
    float d1 = 300.0f + depth * (0.5f + 0.5f * dk_sinx(f[0])), d2 = 330.0f + depth * (0.5f + 0.5f * dk_sinx(f[0] + 0.5f)), e1, e2;
    int j;
    f[0] = lfo_step(f[0], rate, n);              /* the sway once a block, a straight line between */
    e1 = (300.0f + depth * (0.5f + 0.5f * dk_sinx(f[0])) - d1) / (float)n;
    e2 = (330.0f + depth * (0.5f + 0.5f * dk_sinx(f[0] + 0.5f)) - d2) / (float)n;
    for (j = 0; j < n; j++) {
        float m = (l[j] + r[j]) * 0.5f, y1, y2;
        d1 += e1;
        d2 += e2;
        fxbuf_put(s, m);
        y1 = fxbuf_get(s, d1);
        y2 = fxbuf_get(s, d2);
        l[j] = l[j] * (1.0f - 0.35f * amt) + y1 * 0.8f * amt;
        r[j] = r[j] * (1.0f - 0.35f * amt) + y2 * 0.8f * amt;
    }
}

/* FILTER: a state-variable filter, its cutoff AMOUNT, resonance a, low-pass to high-pass b */
static void fx_filter(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, fc = dk_svf_f(60.0f * fm_exp2f(amt * 8.2f)), q = 1.6f - 1.45f * a;
    int j, mode = b < 0.33f ? 0 : b < 0.66f ? 1 : 2;
    for (j = 0; j < n; j++) {
        float hp;
        f[0] += fc * f[1];
        hp = l[j] - f[0] - q * f[1];
        f[1] = fm_flush(f[1] + fc * hp);
        f[2] += fc * f[3];
        {
            float hp2 = r[j] - f[2] - q * f[3];
            f[3] = fm_flush(f[3] + fc * hp2);
            r[j] = mode == 0 ? f[2] : mode == 1 ? f[3] : hp2;
        }
        l[j] = mode == 0 ? f[0] : mode == 1 ? f[1] : hp;
    }
}

/* TREMOLO: the level rising and falling on the beat (RATE a: half notes to sixteenths), across the stereo (b) */
static void fx_tremolo(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    static const float DIV[5] = {2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f};
    float *f = s->f, beats = DIV[(int)(a * 4.99f)], inc = 1.0f / (beats * (dk_ctx.spb > 1.0f ? dk_ctx.spb : 22050.0f));
    float x = 1.0f - amt * (0.5f + 0.5f * dk_sinx(f[0])), y = 1.0f - amt * (0.5f + 0.5f * dk_sinx(f[0] + 0.5f * b)), dx, dy;
    int j;
    f[0] = lfo_step(f[0], inc, n);               /* the swell once a block (16th notes: still ~40 blocks a cycle) */
    dx = (1.0f - amt * (0.5f + 0.5f * dk_sinx(f[0])) - x) / (float)n;
    dy = (1.0f - amt * (0.5f + 0.5f * dk_sinx(f[0] + 0.5f * b)) - y) / (float)n;
    for (j = 0; j < n; j++) {
        x += dx;
        y += dy;
        l[j] *= x;
        r[j] *= y;
    }
}

/* SPRING: a little tank: a chain of all-passes (the boing), a line fed back (DECAY a), its tone (b); AMOUNT the mix */
static void fx_spring(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, fb = 0.55f + 0.38f * a, k = dk_lpk(1800.0f + 5000.0f * b);
    float sd = 900.0f + 12.0f * dk_sinx(f[11]), dsd;
    int j, i;
    f[11] = lfo_step(f[11], 0.7f / DK_SR, n);
    dsd = (900.0f + 12.0f * dk_sinx(f[11]) - sd) / (float)n;
    for (j = 0; j < n; j++) {
        float x, y;
        sd += dsd;
        f[12] += 0.45f * ((l[j] + r[j]) * 0.5f - f[12]);   /* the tank's input softened: no ping off a sharp attack */
        x = f[12] + f[10] * fb;
        for (i = 0; i < 4; i++) {                          /* dispersion: four short all-passes (one-pole) */
            float g = 0.62f, z = f[i];
            y = -g * x + z;
            f[i] = x + g * y;
            x = y;
        }
        fxbuf_put(s, x);
        y = fxbuf_get(s, sd);
        f[9] += k * (y - f[9]);
        f[10] = fm_flush(f[9]);
        l[j] += (f[9] - l[j] * 0.3f) * amt;
        f[13] += k * (fxbuf_get(s, 730.0f) - f[13]);
        r[j] += (f[13] - r[j] * 0.3f) * amt * 0.9f;
    }
}

/* PUNCH: a drum bus squeezed: a compressor slow enough to let each hit's attack through (SNAP a), its saturation
 * (DRIVE b); AMOUNT how hard (and as much made up) */
static void fx_punch(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b)
{
    float *f = s->f, ka = dk_lpk(1.0f / (0.0004f + 0.025f * a * a) * 0.16f), kr = dk_lpk(1.0f / 0.12f * 0.16f);
    float thr = 0.16f, k = amt * 14.0f, mk = 1.0f + amt * 1.6f, drive = 1.0f + 3.0f * b,
          dmk = (1.0f + 0.15f * (drive - 1.0f)) / drive;
    int j;
    for (j = 0; j < n; j++) {
        float x = (fm_fabsf(l[j]) + fm_fabsf(r[j])) * 0.5f, g;
        f[0] += (x > f[0] ? ka : kr) * (x - f[0]);         /* the level, its attack late by SNAP */
        g = f[0] > thr ? mk / (1.0f + k * (f[0] - thr)) : mk;
        f[1] += 0.01f * (g - f[1]);                        /* the gain, smoothed: no zipper */
        l[j] = fm_tanhf(l[j] * f[1] * drive) * dmk;
        r[j] = fm_tanhf(r[j] * f[1] * drive) * dmk;
    }
}

static const dk_fx_t FX_NONE = {"NONE", "", "", 0};
static const dk_fx_t FX_VINYL = {"VINYL", "CRACKLE", "HISS", fx_vinyl};
static const dk_fx_t FX_TAPE = {"TAPE", "WOW", "SATURATE", fx_tape};
static const dk_fx_t FX_LOFI = {"LOFI", "BITS", "RATE", fx_lofi};
static const dk_fx_t FX_CHORUS = {"CHORUS", "RATE", "DEPTH", fx_chorus};
static const dk_fx_t FX_FILTER = {"FILTER", "RES", "MODE", fx_filter};
static const dk_fx_t FX_TREM = {"TREMOLO", "RATE", "STEREO", fx_tremolo};
static const dk_fx_t FX_SPRING = {"SPRING", "DECAY", "TONE", fx_spring};
static const dk_fx_t FX_PUNCH = {"PUNCH", "SNAP", "DRIVE", fx_punch};
static const dk_fx_t *const TAPE_FX[] = {&FX_NONE, &FX_VINYL, &FX_TAPE, &FX_LOFI, &FX_CHORUS, &FX_FILTER, &FX_TREM,
                                         &FX_SPRING, &FX_PUNCH};
enum { FX_N_NONE, FX_N_VINYL, FX_N_TAPE, FX_N_LOFI, FX_N_CHORUS, FX_N_FILTER, FX_N_TREM, FX_N_SPRING, FX_N_PUNCH };

/* the master: DUST (0 clean, 1 a cassette from childhood: a worn top, its saturation, a hiss that follows the music,
 * a crackle), SLEEP (darker, softer) */
static float ms_l1, ms_l2, ms_r1, ms_r2, ms_crk, ms_crkm, ms_cf, ms_lvl;
static void tape_master(float *l, float *r, int n, float dust, float sleep)
{
    float k = dk_lpk(16500.0f * fm_exp2f(-2.0f * dust - 2.2f * sleep)), drive = 1.0f + 1.0f * dust,
          mk = (1.0f + 0.15f * (drive - 1.0f)) / drive * (1.0f - 0.2f * sleep), pk = 0.0f;
    int j;
    if (dust <= 0.0f && sleep <= 0.0f) {
        ms_l1 = ms_l2 = l[n - 1];
        ms_r1 = ms_r2 = r[n - 1];
        return;
    }
    for (j = 0; j < n; j++) {
        float hiss = dust * 0.0015f * (0.25f + 4.0f * ms_lvl), c = 0.0f;
        pk = fm_maxf(pk, fm_fabsf(l[j]));
        if ((float)(rnd() & 65535u) < dust * 2.0f) {
            ms_crk = (0.1f + 0.4f * frnd()) * dust;
            ms_crkm = dk_decay(0.0006f);
        }
        if (ms_crk > 1e-4f) {
            c = ms_crk * wnoise();
            ms_crk *= ms_crkm;
        }
        ms_cf += 0.35f * (c - ms_cf);
        ms_l1 += k * (l[j] - ms_l1);
        ms_l2 += k * (ms_l1 - ms_l2);
        ms_r1 += k * (r[j] - ms_r1);
        ms_r2 += k * (ms_r1 - ms_r2);
        hiss *= wnoise();                                  /* one hiss for both sides: half the dice */
        l[j] = fm_tanhf(ms_l2 * drive) * mk + hiss + ms_cf * 0.05f;
        r[j] = fm_tanhf(ms_r2 * drive) * mk - hiss * 0.8f + ms_cf * 0.045f;
    }
    ms_lvl = fm_flush(fm_maxf(pk, ms_lvl * 0.97f));
    ms_cf = fm_flush(ms_cf);                               /* no denormal kept: the crackle's filter fed silence ends */
    ms_l1 = fm_flush(ms_l1);                               /* on the smallest one and stays (a slow path a sample) */
    ms_l2 = fm_flush(ms_l2);
    ms_r1 = fm_flush(ms_r1);
    ms_r2 = fm_flush(ms_r2);
}
