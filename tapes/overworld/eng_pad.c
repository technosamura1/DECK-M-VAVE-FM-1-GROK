/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's air and depth: AIRPAD (its pads: SWEDEN two detuned saws, AIR a breathy choir, WARM soft strings,
 * SHIMMER glassy, STRINGS bowed), GLASS (its bells: glass, celesta, marimba, music box, vibes; two-operator FM),
 * SOFTBASS (its basses: SUB a round sine, UPRIGHT a thumb on a string, MOOG a saw through a resonant low-pass; MONO,
 * with a glide). Part of tape_dsp.c. */

/* ------------------------------------------------------------------ AIRPAD --- */
#define PSN 1024
static float padtab[4][PSN + 1] DK_POOL;
static void pad_tables(void)
{
    int i;
    float tone_ = 0.45f, ka = 0.06f + 0.05f * tone_, kw = (0.09f + 0.05f * tone_) * 2.2f;
    for (i = 0; i <= PSN; i++) {
        float ph = (float)i * (1.0f / (float)PSN);
        padtab[0][i] = dk_sinx(ph + dk_sinx(2.0f * ph) * ka + 4.0f);
        padtab[1][i] = dk_sinx(ph + dk_sinx(ph) * kw + 4.0f) * 0.85f + dk_sinx(2.0f * ph + 4.0f) * 0.12f;
        padtab[2][i] = (dk_sinx(ph + 4.0f) + dk_sinx(2.0f * ph + 4.0f) * (0.30f + 0.25f * tone_) +
                        dk_sinx(3.0f * ph + 4.0f) * 0.16f + dk_sinx(4.0f * ph + 4.0f) * (0.08f + 0.12f * tone_)) * 0.75f;
        padtab[3][i] = (dk_sinx(ph + 4.0f) + dk_sinx(2.0f * ph + 4.0f) * 0.75f + dk_sinx(3.0f * ph + 4.0f) * 0.5f +
                        dk_sinx(4.0f * ph + 4.0f) * 0.4f + dk_sinx(6.0f * ph + 4.0f) * 0.25f + dk_sinx(8.0f * ph + 4.0f) * 0.2f) *
                       0.4f;                     /* ORGAN: the drawbars 1 2 2 2/3 4 5 1/3 8 pulled out */
    }
}
static inline float ptab(const float *t, float ph)
{
    float x = ph * (float)PSN;
    int i = (int)x;
    return t[i] + (t[i + 1] - t[i]) * (x - (float)i);
}

typedef struct {
    float inc, ph1, ph2, ph3, vel, gl, gr;
    float es, att_inc, mr, lp, lp2, lpk, det1, det2, held, kind, vib;
    float f1l, f1b, f2l, f2b, f3l, f3b;      /* CHOIR: its three formants */
} pad_v;
CHECK_SIZE(pad_v);
static const char *const PAD_TYPES[] = {"SWEDEN", "AIR", "WARM", "SHIMMER", "STRINGS", "SUPER", "CHOIR", "DRONE", "ORGAN"};
static const dk_param_t PAD_P[] = {
    {"BRIGHT", 0, 100, 45, VZ_SUN, 0}, {"SWELL", 0, 100, 50, VZ_SWELL, 0}, {"AIR", 0, 100, 40, VZ_WAVE, 0},
    {"TYPE", 0, 8, 0, VZ_TYPE, PAD_TYPES}, {"DETUNE", 0, 100, 50, VZ_BAR, 0},
};
static void pad_on(float *st, int note, float vel, const float *p, int retrig)
{
    pad_v *v = (pad_v *)st;
    static const float ATT[9] = {1.6f, 2.2f, 1.6f, 2.6f, 1.1f, 0.9f, 1.2f, 2.0f, 0.06f},
                       REL[9] = {2.8f, 3.4f, 2.8f, 3.4f, 2.8f, 1.8f, 2.4f, 6.0f, 0.3f};
    static const float FC0[9] = {500.0f, 1600.0f, 900.0f, 2400.0f, 1300.0f, 1500.0f, 4000.0f, 220.0f, 700.0f},
                       FC1[9] = {1100.0f, 2200.0f, 1500.0f, 3000.0f, 2600.0f, 5200.0f, 1000.0f, 1100.0f, 3600.0f};
    int k = (int)p[3];
    float tone = p[0] * 0.01f, kt = fm_clampf((float)(note - 48) * (1.0f / 48.0f), -0.5f, 1.0f), sw, dt;
    k = k < 0 ? 0 : k > 8 ? 8 : k;
    if (retrig && v->held != 0.0f)
        (void)0;
    v->kind = (float)k;
    v->vel = vel;
    v->inc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    pan_of((float)note, 0.55f, &v->gl, &v->gr);
    sw = fm_exp2f((p[1] - 50.0f) * (1.0f / 22.0f));       /* SWELL: the rise, a fifth to five times its own */
    v->att_inc = 1.0f / (env_att(p, ATT[k] * sw) * DK_SR);
    v->mr = 1.0f / (REL[k] * env_rel(p) * DK_SR);
    v->lpk = dk_lpk(FC0[k] + FC1[k] * tone + 200.0f * kt);
    dt = 0.00347f * p[4] * 0.02f;                         /* six cents either way at 50 */
    v->det1 = 1.0f - dt;
    v->det2 = 1.0f + dt;
    if (!retrig) {
        v->ph1 = frnd();
        v->ph2 = frnd();
        v->ph3 = frnd();
        v->es = 0.0f;
        v->f1l = v->f1b = v->f2l = v->f2b = v->f3l = v->f3b = 0.0f;
    }
    v->vib = frnd();
    v->held = 1.0f;
}
static void pad_off(float *st, const float *p)
{
    (void)p;
    ((pad_v *)st)->held = 0.0f;
}
static int pad_render(float *st, float *l, float *r, int n, const float *p)
{
    pad_v *v = (pad_v *)st;
    int k = (int)v->kind, j;
    float air = p[2] * 0.01f, drift = dk_sinx(dk_ctx.lfo_slow + v->vib), w = dk_ctx.wow;
    float d1 = v->inc * w * v->det1, d2 = v->inc * w * v->det2, t = v->held != 0.0f ? 1.0f : 0.0f, kk, lvl = 0.17f;
    float shim = 0.25f + 0.2f * dk_sinx(dk_ctx.lfo_mid + 4.0f), breath = 0.18f * (0.4f + 1.5f * air);
    float c1 = 0.0f, c2 = 0.0f, c3 = 0.0f, vow = 0.0f, d0 = v->inc * w;
    if (k == 4 || k == 6) {                               /* STRINGS, CHOIR: a bowed / sung vibrato */
        float vb = 1.0f + (k == 6 ? 0.0032f : 0.0022f) * dk_sinx(dk_ctx.lfo_fast + v->vib);
        d1 *= vb;
        d2 *= vb;
        d0 *= vb;
    }
    if (k == 6) {                                         /* CHOIR: the vowel by BRIGHT, AH to EE; its formants' coefficients */
        vow = fm_clampf(p[0] * 0.01f + 0.08f * drift, 0.0f, 1.0f);
        c1 = dk_svf_f(720.0f - 430.0f * vow);
        c2 = dk_svf_f(1100.0f + 1350.0f * vow);
        c3 = dk_svf_f(2650.0f);
        lvl = 0.30f;
    } else if (k == 5 || k == 8)
        lvl = k == 5 ? 0.15f : 0.20f;
    else if (k == 7)
        lvl = 0.20f;
    kk = fm_minf(k == 4 ? 0.7f : 0.6f, v->lpk * (1.0f + (k == 4 ? 0.15f : k == 7 ? 0.6f : 0.35f + 0.3f * air) * drift));
    for (j = 0; j < n; j++) {
        float s, a;
        if (v->es < t)
            v->es = fm_minf(t, v->es + v->att_inc);
        else
            v->es = fm_maxf(0.0f, v->es - v->mr);
        v->ph2 += d1;
        if (v->ph2 >= 1.0f)
            v->ph2 -= 1.0f;
        v->ph3 += d2;
        if (v->ph3 >= 1.0f)
            v->ph3 -= 1.0f;
        if (k == 1)
            s = ptab(padtab[0], v->ph2) + ptab(padtab[0], v->ph3) * 0.7f + wnoise() * breath;
        else if (k == 2)
            s = (ptab(padtab[1], v->ph2) + ptab(padtab[1], v->ph3)) * 0.8f + wnoise() * breath * 0.25f;
        else if (k == 3) {
            v->ph1 += d1 * 2.0f;
            if (v->ph1 >= 1.0f)
                v->ph1 -= 1.0f;
            s = (ptab(padtab[2], v->ph2) + ptab(padtab[2], v->ph3) * 0.8f + ptab(padtab[2], v->ph1) * shim) * 0.6f;
        } else if (k == 5 || k == 7) {                    /* SUPER: three saws apart; DRONE: two, and the octave under */
            v->ph1 = dk_wrap1(v->ph1 + (k == 5 ? d0 : d0 * 0.5f));
            s = dk_blep(v->ph2, d1) + dk_blep(v->ph3, d2) + dk_blep(v->ph1, k == 5 ? d0 : d0 * 0.5f) * (k == 5 ? 1.0f : 1.2f) +
                wnoise() * breath * (k == 7 ? 0.5f : 0.1f);
            s *= k == 5 ? 0.62f : 0.55f;
        } else if (k == 6) {                              /* CHOIR: a glottal buzz and breath through three formants */
            float x, hp, buzz;
            v->ph1 = dk_wrap1(v->ph1 + d0);
            buzz = dk_blep(v->ph1, d0) + dk_blep(v->ph2, d1) + dk_blep(v->ph3, d2);
            x = buzz * 0.5f + wnoise() * breath * 0.45f;
            v->f1l += c1 * v->f1b;
            hp = x - v->f1l - 0.16f * v->f1b;
            v->f1b = fm_flush(v->f1b + c1 * hp);
            v->f1l = fm_flush(v->f1l);
            v->f2l += c2 * v->f2b;
            hp = x - v->f2l - 0.14f * v->f2b;
            v->f2b = fm_flush(v->f2b + c2 * hp);
            v->f2l = fm_flush(v->f2l);
            v->f3l += c3 * v->f3b;
            hp = x - v->f3l - 0.12f * v->f3b;
            v->f3b = fm_flush(v->f3b + c3 * hp);
            v->f3l = fm_flush(v->f3l);
            s = (v->f1b * 0.55f + v->f2b * 0.38f + v->f3b * 0.16f) * 1.0f;
        } else if (k == 8)                                /* ORGAN: drawbars, two a little apart (the rotor) */
            s = (ptab(padtab[3], v->ph2) + ptab(padtab[3], v->ph3)) * 0.8f;
        else
            s = dk_blep(v->ph2, d1) + dk_blep(v->ph3, d2) + wnoise() * breath * 0.3f;
        if (k == 6)
            a = s * v->es * v->es * v->vel * lvl;         /* (the formants are the filter) */
        else {
            v->lp += kk * (s - v->lp);
            v->lp2 += kk * (v->lp - v->lp2);
            a = v->lp2 * v->es * v->es * v->vel * lvl;
        }
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    return !(v->held == 0.0f && v->es <= 0.0f);
}
static float pad_level(const float *st)
{
    const pad_v *v = (const pad_v *)st;
    return v->es * v->es * v->vel * 0.5f;
}
const dk_engine_t ENG_AIRPAD = {"PAD", EF_POLY, 4, 7, 5, PAD_P, pad_on, pad_off, pad_render, pad_level, 0, 0, 0, 0,
                                R_PAD};

/* ------------------------------------------------------------------- GLASS --- */
typedef struct {
    float inc, ph1, ph2, vel, gl, gr;
    float es, ms, mr, att, att_inc, idx, idx_mul, ratio, held, kind;
} bell_v;
CHECK_SIZE(bell_v);
static const char *const BELL_TYPES[] = {"GLASS", "CELESTA", "MARIMBA", "MUSIC BOX", "VIBES"};
static const dk_param_t BELL_P[] = {
    {"TYPE", 0, 4, 0, VZ_TYPE, BELL_TYPES}, {"DECAY", 0, 100, 50, VZ_ENV, 0}, {"SHINE", 0, 100, 50, VZ_SUN, 0},
    {"OCTAVE", 0, 2, 0, VZ_BAR, 0},
};
static void bell_on(float *st, int note, float vel, const float *p, int retrig)
{
    static const float RATIO[5] = {3.5f, 4.0f, 4.0f, 5.95f, 4.0f}, I0[5] = {0.28f, 0.22f, 0.30f, 0.24f, 0.10f},
                       TI[5] = {1.4f, 0.30f, 0.05f, 0.09f, 0.5f}, TA[5] = {3.6f, 2.4f, 0.55f, 1.7f, 3.2f};
    bell_v *v = (bell_v *)st;
    int t = (int)p[0];
    float tone = p[2] * 0.01f, f, kt, dsc = fm_exp2f((p[1] - 50.0f) * (1.0f / 40.0f)) * env_dec(p);
    (void)retrig;
    t = t < 0 ? 0 : t > 4 ? 4 : t;
    {                                          /* OCTAVE is 0..2. A KEYS preset change can still hand this a TINE BARK
                                                * (0..100) for one note: +840 semitones, the phase runs away, the plate farts */
        int oct = (int)p[3];
        if (oct < 0)
            oct = 0;
        else if (oct > 2)
            oct = 2;
        note += 12 * oct;
        if (note < 0)
            note = 0;
        else if (note > 127)
            note = 127;
    }
    f = dk_mtof((float)note) * drift_of(p);
    kt = fm_clampf((float)(note - 48) * (1.0f / 48.0f), -0.5f, 1.0f);
    v->kind = (float)t;
    v->vel = vel;
    v->inc = f * (1.0f / DK_SR);
    pan_of((float)note, 0.55f, &v->gl, &v->gr);
    v->ratio = RATIO[t];
    if (f * v->ratio > 17000.0f)
        v->ratio = 17000.0f / f;
    v->idx = I0[t] * (0.5f + 0.7f * vel) * (0.6f + 0.8f * tone);
    {                                                     /* the sidebands kept under 17 kHz: no fold-over up high */
        float fm = f * v->ratio, beta = (13000.0f - f) / fm - 1.0f;
        if (beta < 0.0f)
            beta = 0.0f;
        if (v->idx * 6.2831853f > beta)
            v->idx = beta * (1.0f / 6.2831853f);
    }
    v->idx_mul = dk_decay(TI[t]);
    v->es = 1.0f;
    v->ms = dk_decay(TA[t] * fm_clampf(1.3f - 0.6f * kt, 0.4f, 1.4f) * dsc);
    v->mr = dk_decay(fm_minf(2.2f, TA[t] * 0.6f * env_rel(p) * fm_minf(dsc, 1.5f)));
    v->att = 0.0f;
    v->att_inc = 1.0f / (env_att(p, 0.002f) * DK_SR);
    v->held = 1.0f;
    v->ph1 = v->ph2 = 0.0f;
}
static void bell_off(float *st, const float *p)
{
    (void)p;
    ((bell_v *)st)->held = 0.0f;
}
static int bell_render(float *st, float *l, float *r, int n, const float *p)
{
    bell_v *v = (bell_v *)st;
    float inc = v->inc * dk_ctx.wow, incm = inc * v->ratio, lvl = 0.20f;
    float trem = v->kind == 4.0f ? 0.72f + 0.28f * dk_sinx(dk_ctx.lfo_fast * 1.06f + 4.0f) : 1.0f;
    float m = v->held != 0.0f ? v->ms : v->mr;
    int j;
    (void)p;
    for (j = 0; j < n; j++) {
        float a;
        if (v->att < 1.0f)
            v->att = fm_minf(1.0f, v->att + v->att_inc);
        v->es *= m;
        v->idx *= v->idx_mul;
        v->ph1 = dk_wrap1(v->ph1 + incm);
        v->ph2 = dk_wrap1(v->ph2 + inc);
        a = dk_sinx(v->ph2 + dk_sinx(v->ph1) * v->idx + 4.0f) * v->es * v->att * v->vel * lvl * trem;
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    return v->es >= 8e-4f;
}
static float bell_level(const float *st)
{
    const bell_v *v = (const bell_v *)st;
    return v->es * v->vel;
}
const dk_engine_t ENG_GLASS = {"GLASS", EF_POLY, 5, 8, 4, BELL_P, bell_on, bell_off, bell_render, bell_level, 0, 0, 0,
                               0, R_BELL};

/* ---------------------------------------------------------------- SOFTBASS --- */
typedef struct {
    float inc, inc_to, ph1, ph2, vel, gl, gr;
    float es, ms, mr, att, att_inc, idx, idx_floor, idx_mul, lp, lp2, lpk, lpk_floor, lpk_mul, glide, held, kind;
    float dc;
} bass_v;
CHECK_SIZE(bass_v);
static const char *const BASS_TYPES[] = {"SUB", "MOOG", "UPRIGHT", "SQUARE", "SAW", "TRI", "SUBDRONE"};
static const dk_param_t BASS_P[] = {
    {"TONE", 0, 100, 40, VZ_SUN, 0},  {"PLUCK", 0, 100, 35, VZ_ENV, 0}, {"GLIDE", 0, 100, 0, VZ_GLIDE, 0},
    {"TYPE", 0, 6, 0, VZ_TYPE, BASS_TYPES}, {"LENGTH", 0, 100, 50, VZ_ENV, 0},
};
static void bass_on(float *st, int note, float vel, const float *p, int retrig)
{
    bass_v *v = (bass_v *)st;
    int k = (int)p[3], mg;                                /* mg: a filtered saw (MOOG, SAW) */
    float tone = p[0] * 0.01f, pluck = p[1] * 0.01f, len = fm_exp2f((p[4] - 50.0f) * (1.0f / 35.0f)) * env_dec(p);
    float inc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    k = k < 0 ? 0 : k > 6 ? 6 : k;
    mg = k == 1 || k == 4;
    v->kind = (float)k;
    v->vel = vel;
    v->inc_to = inc;
    v->glide = p[2] > 0.5f && retrig == 2 ? 1.0f - fm_expf(-1.0f / (p[2] * 0.004f * DK_SR)) : 1.0f;
    if (retrig != 2 || p[2] <= 0.5f)
        v->inc = inc;
    pan_of((float)note, 0.15f, &v->gl, &v->gr);
    if (retrig != 2)                                      /* legato: no new attack (and none from zero again) */
        v->att = 0.0f;
    v->att_inc = 1.0f / (env_att(p, k == 6 ? 0.45f : k >= 3 ? 0.004f : mg ? 0.006f : k == 2 ? 0.004f : 0.018f - 0.012f * pluck) *
                         DK_SR);
    v->ms = dk_decay((k == 6 ? 60.0f : k == 3 || k == 5 ? 3.0f * len : mg ? 0.9f : k == 2 ? 1.1f : 1.2f) *
                     (k == 3 || k == 5 || k == 6 ? 1.0f : len));
    v->mr = dk_decay((k == 6 ? 1.6f : k == 3 || k == 5 ? 0.04f : mg ? 0.20f : k == 2 ? 0.16f : 0.35f) * env_rel(p));
    v->idx = 0.035f * (0.6f + tone);
    v->idx_floor = v->idx;
    v->idx_mul = 1.0f;
    if (k == 0 && pluck > 0.0f && retrig != 2) {                         /* SUB: a bloom of harmonics at the start */
        v->idx = v->idx_floor * (1.0f + 5.0f * pluck * vel);
        v->idx_mul = dk_decay(0.05f);
    }
    if (k == 2 && retrig != 2) {
        v->idx = 0.09f * (0.5f + vel) * (0.6f + tone) * (0.6f + 0.8f * pluck);
        v->idx_floor = 0.02f;
        v->idx_mul = dk_decay(0.08f);
    }
    if (mg) {
        v->lpk = 6.2831853f * fm_clampf((260.0f + 1500.0f * vel) * (0.5f + tone) * (0.6f + 0.8f * pluck) * (k == 4 ? 1.7f : 1.0f),
                                        80.0f, 3000.0f) * (1.0f / DK_SR);
        v->lpk_floor = 6.2831853f * (90.0f + 160.0f * tone) * (k == 4 ? 1.5f : 1.0f) * (1.0f / DK_SR);
        v->lpk_mul = dk_decay(k == 4 ? 0.26f : 0.16f);
    }
    if (retrig == 0 && k == 4)
        v->ph1 = frnd();
    v->held = 1.0f;
}
static void bass_off(float *st, const float *p)
{
    (void)p;
    ((bass_v *)st)->held = 0.0f;
}
static int bass_render(float *st, float *l, float *r, int n, const float *p)
{
    bass_v *v = (bass_v *)st;
    int k = (int)v->kind, j;
    float lvl = 0.26f, w = dk_ctx.wow;
    (void)p;
    for (j = 0; j < n; j++) {
        float a, s, inc;
        v->inc += (v->inc_to - v->inc) * v->glide;
        inc = v->inc * w;
        if (v->att < 1.0f) {
            v->att = fm_minf(1.0f, v->att + v->att_inc);
            v->es = fm_maxf(v->es, v->att);
        } else if (k == 2)
            v->es *= v->held != 0.0f ? v->ms : v->mr;
        else if (v->held != 0.0f)
            v->es = 0.7f + (v->es - 0.7f) * v->ms;
        else
            v->es *= v->mr;
        v->ph2 = dk_wrap1(v->ph2 + inc);
        if (k == 2) {
            v->idx = v->idx_floor + (v->idx - v->idx_floor) * v->idx_mul;
            s = dk_sinx(v->ph2 + dk_sinx(v->ph2) * v->idx + 4.0f) * 1.1f;
        } else if (k == 1 || k == 4) {
            float hp;
            v->lpk = v->lpk_floor + (v->lpk - v->lpk_floor) * v->lpk_mul;
            s = dk_blep(v->ph2, inc) * 0.8f;
            if (k == 4) {                                  /* SAW: a second one a little apart (a hair sharp) */
                v->ph1 = dk_wrap1(v->ph1 + inc * 1.0065f);
                s = (s + dk_blep(v->ph1, inc * 1.0065f) * 0.8f) * 0.62f;
            }
            v->lp += v->lpk * v->lp2;
            hp = s - v->lp - (k == 4 ? 0.42f : 0.55f) * v->lp2;
            v->lp2 = fm_flush(v->lp2 + v->lpk * hp);
            s = fm_tanhf(v->lp * 1.4f) * 0.9f;
            v->dc += 0.0006f * (s - v->dc);            /* no offset left by the filter */
            s -= v->dc;
        } else if (k == 3) {                               /* SQUARE: the pulse of a chip, its width by TONE */
            float duty = 0.12f + 0.38f * fm_clampf((v->idx_floor - 0.021f) * (1.0f / 0.035f), 0.0f, 1.0f), q;
            q = dk_blep(v->ph2, inc) - dk_blep(dk_wrap1(v->ph2 + duty), inc);
            v->lp += 0.3f * (q * 0.38f - v->lp);           /* the chip's own output low-pass: an edge, not a click */
            s = v->lp;
        } else if (k == 5) {                               /* TRI: a triangle in sixteen steps (the NES's) */
            float tri = (v->ph2 < 0.5f ? 4.0f * v->ph2 - 1.0f : 3.0f - 4.0f * v->ph2);
            s = (fm_floorf(tri * 7.5f) + 0.5f) * (1.0f / 7.5f) * 0.95f;
        } else if (k == 6) {                               /* SUBDRONE: the note, its octave and the one under, breathing */
            v->ph1 = dk_wrap1(v->ph1 + inc * 0.5f);
            s = dk_sinx(v->ph2) + 0.45f * dk_sinx(v->ph1) + 0.30f * dk_sinx(2.0f * v->ph2 + 0.3f * dk_sinx(v->ph1) + 4.0f) *
                                                             (0.5f + 0.5f * dk_sinx(dk_ctx.lfo_slow + 4.0f));
            s *= 0.8f;
        } else {
            v->idx = v->idx_floor + (v->idx - v->idx_floor) * v->idx_mul;
            v->ph1 = dk_wrap1(v->ph1 + inc);
            s = dk_sinx(v->ph2 + dk_sinx(v->ph1) * v->idx + 4.0f);
        }
        a = s * v->es * v->vel * lvl;
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    return !(v->held == 0.0f && v->att >= 1.0f && v->es < 1e-3f);
}
static float bass_level(const float *st)
{
    const bass_v *v = (const bass_v *)st;
    return v->es * v->vel;
}
const dk_engine_t ENG_SOFTBASS = {"BASS", EF_MONO, 2, 10, 5, BASS_P, bass_on, bass_off, bass_render, bass_level, 0,
                                  0, 0, 0, R_BASS};
