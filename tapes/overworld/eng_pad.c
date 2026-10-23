/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's air and depth: AIRPAD (its pads: SWEDEN two detuned saws, AIR a breathy choir, WARM soft strings,
 * SHIMMER glassy, STRINGS bowed), GLASS (its bells: glass, celesta, marimba, music box, vibes; two-operator FM),
 * SOFTBASS (its basses: SUB a round sine, UPRIGHT a thumb on a string, MOOG a saw through a resonant low-pass; MONO,
 * with a glide). Part of tape_dsp.c. */

/* ------------------------------------------------------------------ AIRPAD --- */
#define PSN 1024
static float padtab[3][PSN + 1] DK_POOL;
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
} pad_v;
CHECK_SIZE(pad_v);
static const char *const PAD_TYPES[] = {"SWEDEN", "AIR", "WARM", "SHIMMER", "STRINGS"};
static const dk_param_t PAD_P[] = {
    {"BRIGHT", 0, 100, 45, VZ_SUN, 0}, {"SWELL", 0, 100, 50, VZ_SWELL, 0}, {"AIR", 0, 100, 40, VZ_WAVE, 0},
    {"TYPE", 0, 4, 0, VZ_TYPE, PAD_TYPES}, {"DETUNE", 0, 100, 50, VZ_BAR, 0},
};
static void pad_on(float *st, int note, float vel, const float *p, int retrig)
{
    pad_v *v = (pad_v *)st;
    static const float ATT[5] = {1.6f, 2.2f, 1.6f, 2.6f, 1.1f}, REL[5] = {2.8f, 3.4f, 2.8f, 3.4f, 2.8f};
    static const float FC0[5] = {500.0f, 1600.0f, 900.0f, 2400.0f, 1300.0f}, FC1[5] = {1100.0f, 2200.0f, 1500.0f,
                                                                                      3000.0f, 2600.0f};
    int k = (int)p[3];
    float tone = p[0] * 0.01f, kt = fm_clampf((float)(note - 48) * (1.0f / 48.0f), -0.5f, 1.0f), sw, dt;
    k = k < 0 ? 0 : k > 4 ? 4 : k;
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
    if (k == 4) {                                         /* STRINGS: a bowed vibrato */
        float vb = 1.0f + 0.0022f * dk_sinx(dk_ctx.lfo_fast + v->vib);
        d1 *= vb;
        d2 *= vb;
    }
    kk = fm_minf(k == 4 ? 0.7f : 0.6f, v->lpk * (1.0f + (k == 4 ? 0.15f : 0.35f + 0.3f * air) * drift));
    {   /* the state in registers for the loop (the outputs might alias it otherwise) */
        float es = v->es, att_inc = v->att_inc, mr = v->mr, ph1 = v->ph1, ph2 = v->ph2, ph3 = v->ph3, lp = v->lp;
        float lp2 = v->lp2, vel = v->vel, gl = v->gl, gr = v->gr;
        for (j = 0; j < n; j++) {
            float s, a;
            if (es < t)
                es = fm_minf(t, es + att_inc);
            else
                es = fm_maxf(0.0f, es - mr);
            ph2 += d1;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            ph3 += d2;
            if (ph3 >= 1.0f)
                ph3 -= 1.0f;
            if (k == 1)
                s = ptab(padtab[0], ph2) + ptab(padtab[0], ph3) * 0.7f + wnoise() * breath;
            else if (k == 2)
                s = (ptab(padtab[1], ph2) + ptab(padtab[1], ph3)) * 0.8f + wnoise() * breath * 0.25f;
            else if (k == 3) {
                ph1 += d1 * 2.0f;
                if (ph1 >= 1.0f)
                    ph1 -= 1.0f;
                s = (ptab(padtab[2], ph2) + ptab(padtab[2], ph3) * 0.8f + ptab(padtab[2], ph1) * shim) * 0.6f;
            } else
                s = dk_blep(ph2, d1) + dk_blep(ph3, d2) + wnoise() * breath * 0.3f;
            lp += kk * (s - lp);
            lp2 += kk * (lp - lp2);
            a = lp2 * es * es * vel * lvl;
            l[j] += a * gl;
            r[j] += a * gr;
        }
        v->es = es;
        v->ph1 = ph1;
        v->ph2 = ph2;
        v->ph3 = ph3;
        v->lp = lp;
        v->lp2 = lp2;
    }
    return !(v->held == 0.0f && v->es <= 0.0f);
}
static float pad_level(const float *st)
{
    const pad_v *v = (const pad_v *)st;
    return v->es * v->es * v->vel * 0.5f;
}
const dk_engine_t ENG_AIRPAD = {"AIRPAD", EF_POLY, 4, 7, 5, PAD_P, pad_on, pad_off, pad_render, pad_level, 0, 0, 0, 0,
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
    note += 12 * (int)p[3];
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
    /* the state in registers for the loop (the outputs might alias it otherwise) */
    float att = v->att, att_inc = v->att_inc, es = v->es, idx = v->idx, idx_mul = v->idx_mul, ph1 = v->ph1, ph2 = v->ph2;
    float vel = v->vel, gl = v->gl, gr = v->gr;
    int j;
    (void)p;
    for (j = 0; j < n; j++) {
        float a;
        if (att < 1.0f)
            att = fm_minf(1.0f, att + att_inc);
        es *= m;
        idx *= idx_mul;
        ph1 = dk_wrap1(ph1 + incm);
        ph2 = dk_wrap1(ph2 + inc);
        a = dk_sinx(ph2 + dk_sinx(ph1) * idx + 4.0f) * es * att * vel * lvl * trem;
        l[j] += a * gl;
        r[j] += a * gr;
    }
    v->att = att;
    v->es = es;
    v->idx = idx;
    v->ph1 = ph1;
    v->ph2 = ph2;
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
static const char *const BASS_TYPES[] = {"SUB", "MOOG", "UPRIGHT"};
static const dk_param_t BASS_P[] = {
    {"TONE", 0, 100, 40, VZ_SUN, 0},  {"PLUCK", 0, 100, 35, VZ_ENV, 0}, {"GLIDE", 0, 100, 0, VZ_GLIDE, 0},
    {"TYPE", 0, 2, 0, VZ_TYPE, BASS_TYPES}, {"LENGTH", 0, 100, 50, VZ_ENV, 0},
};
static void bass_on(float *st, int note, float vel, const float *p, int retrig)
{
    bass_v *v = (bass_v *)st;
    int k = (int)p[3];
    float tone = p[0] * 0.01f, pluck = p[1] * 0.01f, len = fm_exp2f((p[4] - 50.0f) * (1.0f / 35.0f)) * env_dec(p);
    float inc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    k = k < 0 ? 0 : k > 2 ? 2 : k;
    v->kind = (float)k;
    v->vel = vel;
    v->inc_to = inc;
    v->glide = p[2] > 0.5f && retrig == 2 ? 1.0f - fm_expf(-1.0f / (p[2] * 0.004f * DK_SR)) : 1.0f;
    if (retrig != 2 || p[2] <= 0.5f)
        v->inc = inc;
    pan_of((float)note, 0.15f, &v->gl, &v->gr);
    if (retrig != 2)                                      /* legato: no new attack (and none from zero again) */
        v->att = 0.0f;
    v->att_inc = 1.0f / (env_att(p, k == 1 ? 0.006f : k == 2 ? 0.004f : 0.018f - 0.012f * pluck) * DK_SR);
    v->ms = dk_decay((k == 1 ? 0.9f : k == 2 ? 1.1f : 1.2f) * len);
    v->mr = dk_decay((k == 1 ? 0.20f : k == 2 ? 0.16f : 0.35f) * env_rel(p));
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
    if (k == 1) {
        v->lpk = 6.2831853f * fm_clampf((260.0f + 1500.0f * vel) * (0.5f + tone) * (0.6f + 0.8f * pluck), 80.0f, 3000.0f) *
                 (1.0f / DK_SR);
        v->lpk_floor = 6.2831853f * (90.0f + 160.0f * tone) * (1.0f / DK_SR);
        v->lpk_mul = dk_decay(0.16f);
    }
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
    int k = (int)v->kind, j, held = v->held != 0.0f;
    float lvl = 0.26f, w = dk_ctx.wow;
    /* the state in registers for the loop (the outputs might alias it otherwise) */
    float vinc = v->inc, inc_to = v->inc_to, glide = v->glide, att = v->att, att_inc = v->att_inc, es = v->es;
    float ms = v->ms, mr = v->mr, ph1 = v->ph1, ph2 = v->ph2, idx = v->idx, idx_floor = v->idx_floor;
    float idx_mul = v->idx_mul, lpk = v->lpk, lpk_floor = v->lpk_floor, lpk_mul = v->lpk_mul, lp = v->lp, lp2 = v->lp2;
    float dc = v->dc, vel = v->vel, gl = v->gl, gr = v->gr;
    (void)p;
    for (j = 0; j < n; j++) {
        float a, s, inc;
        vinc += (inc_to - vinc) * glide;
        inc = vinc * w;
        if (att < 1.0f) {
            att = fm_minf(1.0f, att + att_inc);
            es = fm_maxf(es, att);
        } else if (k == 2)
            es *= held ? ms : mr;
        else if (held)
            es = 0.7f + (es - 0.7f) * ms;
        else
            es *= mr;
        ph2 = dk_wrap1(ph2 + inc);
        if (k == 2) {
            idx = idx_floor + (idx - idx_floor) * idx_mul;
            s = dk_sinx(ph2 + dk_sinx(ph2) * idx + 4.0f) * 1.1f;
        } else if (k == 1) {
            float hp;
            lpk = lpk_floor + (lpk - lpk_floor) * lpk_mul;
            s = dk_blep(ph2, inc) * 0.8f;
            lp += lpk * lp2;
            hp = s - lp - 0.55f * lp2;
            lp2 = fm_flush(lp2 + lpk * hp);
            s = fm_tanhf(lp * 1.4f) * 0.9f;
            dc += 0.0006f * (s - dc);                  /* no offset left by the filter */
            s -= dc;
        } else {
            idx = idx_floor + (idx - idx_floor) * idx_mul;
            ph1 = dk_wrap1(ph1 + inc);
            s = dk_sinx(ph2 + dk_sinx(ph1) * idx + 4.0f);
        }
        a = s * es * vel * lvl;
        l[j] += a * gl;
        r[j] += a * gr;
    }
    v->inc = vinc;
    v->att = att;
    v->es = es;
    v->ph1 = ph1;
    v->ph2 = ph2;
    v->idx = idx;
    v->lpk = lpk;
    v->lp = lp;
    v->lp2 = lp2;
    v->dc = dc;
    return !(v->held == 0.0f && v->att >= 1.0f && v->es < 1e-3f);
}
static float bass_level(const float *st)
{
    const bass_v *v = (const bass_v *)st;
    return v->es * v->vel;
}
const dk_engine_t ENG_SOFTBASS = {"SOFTBASS", EF_MONO, 2, 10, 5, BASS_P, bass_on, bass_off, bass_render, bass_level, 0,
                                  0, 0, 0, R_BASS};
