/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's made voices: SYNTH (one oscillator or two or three: the NES's pulses, its triangle and its noise, the
 * saw of the eighties, a supersaw, a sine; through a resonant low-pass whose cut-off a note's envelope sweeps; an
 * amplitude that dies away or holds; the bits and the rate of an old chip; a vibrato, or on a pulse the width moving),
 * and PLUCK (a string struck or plucked: eight modes ringing down, each its own way; the place it is plucked at, the
 * wooden box under it: a lute, a harp, a nylon guitar, a steel one, a koto, a dulcimer). Part of tape_dsp.c. */

/* a sample's bits and rate cut, in place: the old chip's grain (crush 0..1) */
static inline float chip_crush(float s, float q, float step, float *hc, float *hs)
{
    *hc += step;
    if (*hc >= 1.0f) {
        *hc -= 1.0f;
        *hs = fm_floorf(s * q + 0.5f) * (1.0f / q);
    }
    return *hs;
}

/* ------------------------------------------------------------------ SYNTH --- */
typedef struct {
    float inc, ph1, ph2, ph3, vel, gl, gr;
    float es, att, att_inc, ms, sus, mr, held, kind;
    float fe, fe_mul, fc0, fe_oct, q, lo, bp, open;
    float hc, hs, cq, cstep, nz, t, vib, det, pw, lp, lpk;
} syn_v;
CHECK_SIZE(syn_v);
static const char *const SYN_WAVES[] = {"SAW", "PULSE", "PULSE 12", "SQUARE", "TRI", "SUPER", "NOISE", "SINE"};
static const dk_param_t SYN_P[] = {
    {"WAVE", 0, 7, 0, VZ_TYPE, SYN_WAVES}, {"CUTOFF", 0, 100, 70, VZ_SUN, 0}, {"RESO", 0, 100, 20, VZ_WAVE, 0},
    {"SWEEP", 0, 100, 0, VZ_DROP, 0},     {"DETUNE", 0, 100, 30, VZ_BAR, 0}, {"CRUSH", 0, 100, 0, VZ_DUST, 0},
    {"SUSTAIN", 0, 100, 100, VZ_ENV, 0},  {"MOTION", 0, 100, 20, VZ_WOBBLE, 0},
};
static void syn_on(float *st, int note, float vel, const float *p, int retrig)
{
    syn_v *v = (syn_v *)st;
    int k = (int)p[0];
    float det = p[4] * 0.01f, crush = p[5] * 0.01f, kt = fm_exp2f((float)(note - 60) * (1.0f / 36.0f));
    (void)retrig;
    k = k < 0 ? 0 : k > 7 ? 7 : k;
    v->kind = (float)k;
    v->vel = vel;
    v->inc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    pan_of((float)note, k == 5 ? 0.7f : 0.45f, &v->gl, &v->gr);
    v->det = k == 5 ? 1.0f + det * 0.014f : 1.0f + det * 0.008f;
    v->fc0 = fm_minf(50.0f * fm_exp2f(p[1] * 0.082f) * kt, 7400.0f);
    v->open = (p[1] >= 97.0f && p[3] < 1.0f) ? 1.0f : 0.0f;       /* the filter out of the way */
    v->q = 1.5f - 1.3f * p[2] * 0.01f;
    v->fe_oct = p[3] * 0.05f;
    v->fe = 1.0f;
    v->fe_mul = dk_decay(0.22f * env_dec(p) * (1.0f / 16.0f));   /* per 16 samples */
    v->att_inc = 1.0f / (env_att(p, 0.006f) * DK_SR);
    v->att = 0.0f;
    v->sus = p[6] * 0.01f;
    v->ms = dk_decay(0.5f * env_dec(p));
    v->mr = dk_decay(0.18f * env_rel(p));
    v->es = 0.0f;
    v->cq = crush > 0.005f ? fm_exp2f(11.0f - crush * 9.0f) : 0.0f;   /* 12 bits down to 3 */
    v->cstep = 1.0f / (1.0f + crush * 5.0f);                          /* the rate: down to a sixth */
    v->ph1 = k == 5 ? frnd() : 0.0f;
    v->ph2 = frnd();
    v->ph3 = frnd();
    v->vib = frnd();
    v->t = 0.0f;
    v->lo = v->bp = v->hc = v->hs = v->lp = 0.0f;
    v->lpk = (k >= 1 && k <= 3) || k == 6 || crush > 0.005f ? 0.42f : 1.0f;   /* the old chip's output, its own low-pass */
    v->held = 1.0f;
}
static void syn_off(float *st, const float *p)
{
    (void)p;
    ((syn_v *)st)->held = 0.0f;
}
/* one oscillator at a phase: the wave of kind k */
static inline float syn_osc(int k, float ph, float inc, float pw)
{
    switch (k) {
    case 0:
    case 5:
        return dk_blep(ph, inc);
    case 1:
    case 2:
    case 3: {
        float ph2 = ph + pw;
        ph2 = ph2 >= 1.0f ? ph2 - 1.0f : ph2;
        return (dk_blep(ph, inc) - dk_blep(ph2, inc)) * (0.5f / fm_maxf(pw, 1.0f - pw));
    }
    case 4:
        return 4.0f * fm_fabsf(ph - 0.5f) - 1.0f;
    default:
        return dk_sinx(ph + 4.0f);
    }
}
static int syn_render(float *st, float *l, float *r, int n, const float *p)
{
    syn_v *v = (syn_v *)st;
    int k = (int)v->kind, j, filt = v->open == 0.0f && k != 6 ? 1 : 0, pulse = k >= 1 && k <= 3;
    float motion = p[7] * 0.01f, w = dk_ctx.wow, lvl = 0.2f, fcur = 0.0f, pw0 = k == 1 ? 0.25f : k == 2 ? 0.125f : 0.5f;
    float vin = fm_minf(1.0f, fm_maxf(0.0f, (v->t - 0.25f) * 2.0f));
    float vb = pulse ? 1.0f : 1.0f + motion * vin * 0.006f * dk_sinx(dk_ctx.lfo_fast + v->vib);
    float pw = fm_clampf(pw0 + (pulse && k != 3 ? 0.18f : 0.28f) * motion * dk_sinx(dk_ctx.lfo_mid + v->vib), 0.06f, 0.5f);
    float two = (k == 5) ? 1.0f : (v->det > 1.0005f ? 1.0f : 0.0f), g = k == 5 ? 0.42f : two != 0.0f ? 0.6f : 0.85f;
    float d1 = v->inc * w * vb, d2 = d1 * v->det, d3 = d1 * (2.0f - v->det), fk = 0.0f;
    for (j = 0; j < n; j++) {
        float s, a;
        if ((j & 15) == 0 && filt) {                       /* the cut-off, its envelope, every 16 samples */
            float hz = fm_minf(6000.0f, v->fc0 * fm_exp2f(v->fe_oct * v->fe));
            fcur = hz;
            fk = fm_minf(0.8f, dk_svf_f(fcur));
            v->fe *= v->fe_mul;
        }
        if (v->held != 0.0f) {
            if (v->att < 1.0f) {
                v->att = fm_minf(1.0f, v->att + v->att_inc);
                v->es = v->att;
            } else
                v->es = v->sus + (v->es - v->sus) * v->ms;
        } else
            v->es *= v->mr;
        v->ph1 = dk_wrap1(v->ph1 + d1);
        if (k == 6) {                                      /* NOISE: held, new at the note's rate (the NES's) */
            v->ph2 += d1 * 6.0f;
            if (v->ph2 >= 1.0f) {
                v->ph2 -= 1.0f;
                v->nz = wnoise();
            }
            s = v->nz;
        } else {
            s = syn_osc(k, v->ph1, d1, pw);
            if (two != 0.0f) {
                v->ph2 = dk_wrap1(v->ph2 + d2);
                if (k == 5) {
                    v->ph3 = dk_wrap1(v->ph3 + d3);
                    s += syn_osc(k, v->ph2, d2, pw) + syn_osc(k, v->ph3, d3, pw);
                } else
                    s += syn_osc(k, v->ph2, d2, pw) * 0.8f;
            }
        }
        s *= g;
        if (filt) {
            float hp;
            v->lo += fk * v->bp;
            hp = s - v->lo - v->q * v->bp;
            v->bp = fm_flush(v->bp + fk * hp);
            v->lo = fm_flush(v->lo);
            s = fm_clampf(v->lo, -2.0f, 2.0f);
        }
        if (v->cq > 0.0f)
            s = chip_crush(s, v->cq, v->cstep, &v->hc, &v->hs);
        v->lp += v->lpk * (s - v->lp);
        s = v->lp;
        a = s * v->es * v->vel * lvl;
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    v->t += (float)n * (1.0f / DK_SR);
    return !((v->held == 0.0f && v->es < 1e-3f) || (v->att >= 1.0f && v->es < 1e-4f));
}
static float syn_level(const float *st)
{
    const syn_v *v = (const syn_v *)st;
    return v->es * v->vel * 0.5f;
}
const dk_engine_t ENG_SYNTH = {"SYNTH", EF_POLY, 6, 8, 8, SYN_P, syn_on, syn_off, syn_render, syn_level, 0, 0, 0, 0,
                               R_LEAD};

/* ------------------------------------------------------------------ PLUCK --- */
#define PL_N 8
typedef struct {
    float x[PL_N], y[PL_N], c[PL_N], s[PL_N], r[PL_N];
    float vel, gl, gr, es, mr, held, lv, lvm, lo, bp, bq, bg, click, clm, cl_lp, cq, cstep, hc, hs, lpk, lp, lq;
} pl_v;
CHECK_SIZE(pl_v);
static const char *const PL_TYPES[] = {"LUTE", "HARP", "NYLON", "STEEL", "KOTO", "DULCIMER"};
static const dk_param_t PL_P[] = {
    {"TYPE", 0, 5, 0, VZ_TYPE, PL_TYPES}, {"BRIGHT", 0, 100, 50, VZ_SUN, 0}, {"DECAY", 0, 100, 50, VZ_ENV, 0},
    {"POSITION", 0, 100, 40, VZ_BAR, 0},  {"BODY", 0, 100, 40, VZ_SWELL, 0}, {"CRUSH", 0, 100, 0, VZ_DUST, 0},
};
/* by type: the ring (s, the fundamental), the stiffness (the modes' stretch), its release (s), the pick's click */
static const float PL_TAU[6] = {1.5f, 3.4f, 2.2f, 2.8f, 1.7f, 3.8f};
static const float PL_B[6] = {0.0004f, 0.00004f, 0.0002f, 0.0006f, 0.0009f, 0.0013f};
static const float PL_REL[6] = {0.30f, 1.40f, 0.28f, 0.45f, 0.80f, 2.20f};
static const float PL_CLK[6] = {0.10f, 0.05f, 0.14f, 0.16f, 0.22f, 0.10f};
static void pl_on(float *st, int note, float vel, const float *p, int retrig)
{
    pl_v *v = (pl_v *)st;
    int k = (int)p[0], i;
    float f0 = dk_mtof((float)note) * drift_of(p), bright = p[1] * 0.01f, pos = 0.05f + 0.45f * p[3] * 0.01f, tilt, damp, tau0,
          crush = p[5] * 0.01f, body = p[4] * 0.01f, sum = 0.0f;
    (void)retrig;
    k = k < 0 ? 0 : k > 5 ? 5 : k;
    v->vel = vel;
    pan_of((float)note, 0.45f, &v->gl, &v->gr);
    tilt = 0.7f + (1.0f - bright) * 1.7f + (1.0f - vel) * 0.7f;
    damp = 0.05f + (1.0f - bright) * 0.30f;
    tau0 = PL_TAU[k] * fm_exp2f((p[2] - 50.0f) * (1.0f / 22.0f)) * env_dec(p) * fm_exp2f(-(float)(note - 60) * (1.0f / 40.0f));
    for (i = 0; i < PL_N; i++) {
        float n = (float)(i + 1), f = n * f0 * fm_sqrtf(1.0f + PL_B[k] * n * n), w, a, tn;
        a = fm_maxf(0.12f, fm_fabsf(dk_sinx(n * pos * 0.5f)));   /* the place plucked: |sin(pi n pos)| */
        a = a * fm_exp2f(-tilt * fm_log2f(n));
        if (f > 17000.0f)
            a = 0.0f;
        w = f * (1.0f / DK_SR);
        tn = tau0 / (1.0f + n * n * damp);
        v->x[i] = 0.0f;
        v->y[i] = a;
        v->c[i] = dk_sinx(w + 4.25f);                  /* cos */
        v->s[i] = dk_sinx(w + 4.0f);                   /* sin */
        v->r[i] = fm_expf(-1.0f / (fm_maxf(tn, 0.02f) * DK_SR));
        sum += a * a;
    }
    sum = 0.55f / fm_sqrtf(sum + 0.05f);
    for (i = 0; i < PL_N; i++)
        v->y[i] *= sum * vel;
    v->es = 1.0f;
    v->mr = dk_decay(PL_REL[k] * env_rel(p));
    v->lv = 1.0f;
    v->lvm = v->r[0];
    v->lo = v->bp = 0.0f;
    v->bg = body * 2.2f;                               /* the box: a resonance low (around 200 Hz) */
    v->bq = 0.7f;
    v->click = (0.3f + 0.7f * bright) * PL_CLK[k] * (0.4f + vel);
    v->clm = dk_decay(0.0025f + 0.004f * (1.0f - bright));
    v->cl_lp = 0.0f;
    v->lpk = dk_lpk(1500.0f + 9000.0f * bright * bright);
    v->lp = 0.0f;
    v->cq = crush > 0.005f ? fm_exp2f(11.0f - crush * 9.0f) : 0.0f;
    v->cstep = 1.0f / (1.0f + crush * 5.0f);
    v->hc = v->hs = v->lq = 0.0f;
    v->held = 1.0f;
}
static void pl_off(float *st, const float *p)
{
    (void)p;
    ((pl_v *)st)->held = 0.0f;
}
static int pl_render(float *st, float *l, float *r, int n, const float *p)
{
    pl_v *v = (pl_v *)st;
    int j, i;
    float bf = dk_svf_f(210.0f), lvl = 0.5f;
    (void)p;
    for (j = 0; j < n; j++) {
        float s = 0.0f, a, hp;
        for (i = 0; i < PL_N; i++) {
            float x = v->x[i], y = v->y[i];
            v->x[i] = v->r[i] * (v->c[i] * x - v->s[i] * y);
            v->y[i] = v->r[i] * (v->s[i] * x + v->c[i] * y);
            s += x;
        }
        s += wnoise() * v->click;                       /* the pick's tick */
        v->click *= v->clm;
        v->lp += v->lpk * (s - v->lp);
        s = v->lp;
        v->lo += bf * v->bp;                            /* the wooden box */
        hp = s - v->lo - v->bq * v->bp;
        v->bp = fm_flush(v->bp + bf * hp);
        v->lo = fm_flush(v->lo);
        s += v->bp * v->bg;
        if (v->cq > 0.0f) {
            s = chip_crush(s, v->cq, v->cstep, &v->hc, &v->hs);
            v->lq += 0.45f * (s - v->lq);              /* the chip's output low-pass */
            s = v->lq;
        }
        if (v->held == 0.0f)
            v->es *= v->mr;
        v->lv *= v->lvm;
        a = s * v->es * lvl;
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    return !(v->lv * v->vel < 1e-4f || v->es < 1e-4f);
}
static float pl_level(const float *st)
{
    const pl_v *v = (const pl_v *)st;
    return v->es * v->lv * v->vel * 0.5f;
}
const dk_engine_t ENG_PLUCK = {"PLUCK", EF_POLY, 6, 8, 6, PL_P, pl_on, pl_off, pl_render, pl_level, 0, 0, 0, 0, R_LEAD};
