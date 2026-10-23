/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's keys: FELT (its felt piano, OVERWORLD's FM piano with the felt: the top softened, the attack eased, the
 * key's own knock), TINE (OVERWORLD's VENUS: a worn FM electric piano, with its tremolo and a drift), KALIMBA (a
 * plucked tine: its modes, and the wooden box it sits on). Part of tape_dsp.c. */

/* the ENV tab: 50 is the engine's own; ATTACK slower, DECAY and RELEASE longer or shorter */
static float env_att(const float *p, float att_s)
{
    float a = (p[PX_ATT] - 50.0f) * 0.02f;
    return a <= 0.0f ? att_s * (1.0f + a * 0.8f) : att_s + a * a * 1.6f;
}
static float env_dec(const float *p) { return fm_exp2f((p[PX_DEC] - 50.0f) * (1.0f / 35.0f)); }
static float env_rel(const float *p) { return fm_exp2f((p[PX_REL] - 50.0f) * (1.0f / 25.0f)); }
static float drift_of(const float *p)          /* a voice's own detune: a few cents at most (the analogue in it) */
{
    return 1.0f + (frnd() - 0.5f) * 0.0030f * p[PX_DRIFT] * 0.01f;
}
static void pan_of(float note, float width, float *gl, float *gr)
{
    float p = fm_clampf((note - 64.0f) * (1.0f / 30.0f), -1.0f, 1.0f) * width;
    *gl = 0.5f - 0.5f * p;
    *gr = 0.5f + 0.5f * p;
}

/* ------------------------------------------------------------------- FELT --- */
typedef struct {
    float inc, ph1, ph2, ph3, vel, gl, gr;
    float ef, es, mf, ms, mr, att, att_inc;
    float idx, idx_floor, idx_mul, lp, lpk, lp2, lpk2, noise, noise_mul;
    float th, th_mul, th_ph, th_inc, th_lp;    /* the key's knock (and the damper's, let go) */
    float held, det, done, noise_mul2;
} felt_v;
CHECK_SIZE(felt_v);

static const dk_param_t FELT_P[] = {
    {"TONE", 0, 100, 45, VZ_SUN, 0},  {"FELT", 0, 100, 45, VZ_DUST, 0},  {"DECAY", 0, 100, 50, VZ_ENV, 0},
    {"KNOCK", 0, 100, 50, VZ_BAR, 0}, {"WIDTH", 0, 100, 55, VZ_BAR, 0},
};
static void felt_on(float *st, int note, float vel, const float *p, int retrig)
{
    felt_v *v = (felt_v *)st;
    float tone = p[0] * 0.01f, felt = p[1] * 0.01f, dsc = fm_exp2f((p[2] - 50.0f) * (1.0f / 50.0f)) * env_dec(p);
    float f = dk_mtof((float)note), kt = fm_clampf((float)(note - 48) * (1.0f / 48.0f), -0.5f, 1.0f), tau, fc;
    (void)retrig;
    v->vel = vel;
    v->det = drift_of(p);
    v->inc = f * v->det * (1.0f / DK_SR);
    pan_of((float)note, 0.55f * p[4] * 0.018f, &v->gl, &v->gr);
    tau = fm_clampf(8.0f * fm_exp2f(-(float)(note - 48) * (1.0f / 20.0f)), 1.2f, 11.0f) * dsc;
    v->ef = 0.45f;
    v->es = 0.55f;
    v->mf = dk_decay((0.55f + 0.25f * (1.0f - vel)) * fm_minf(dsc, 1.5f));
    v->ms = dk_decay(tau);
    v->mr = dk_decay((note < 48 ? 0.28f : 0.14f) * env_rel(p) * (1.0f + 0.6f * felt));
    v->att_inc = 1.0f / (env_att(p, 0.003f + 0.010f * (1.0f - vel) + 0.014f * felt) * DK_SR);
    v->att = 0.0f;
    v->idx = (0.05f + 0.20f * vel) * (0.45f + 1.1f * tone) * fm_clampf(1.5f - kt, 0.35f, 1.6f) * (1.0f - 0.25f * felt);
    v->idx_floor = v->idx * 0.12f;
    v->idx_mul = dk_decay(0.32f);
    fc = (700.0f + 5200.0f * tone) * (0.55f + 0.7f * vel) * (1.0f + kt);
    v->lpk = dk_lpk(fc * (1.0f - 0.30f * felt));
    v->lpk2 = dk_lpk(fm_maxf(900.0f, fc * (2.8f - 1.2f * felt)));   /* the felt: a second, gentler slope */
    v->noise = 0.05f * vel * (1.0f - 0.7f * felt);
    v->noise_mul = dk_decay(0.007f);
    v->th = (0.04f + 0.16f * felt) * vel * p[3] * 0.02f;           /* KNOCK: the key bed, felt on wood */
    v->th_mul = dk_decay(0.016f);
    v->th_inc = (150.0f + 0.9f * (float)note) * (1.0f / DK_SR);
    v->th_ph = 0.0f;
    v->held = 1.0f;
    v->ph1 = v->ph2 = v->ph3 = 0.0f;
    v->lp = v->lp2 = 0.0f;
}
static void felt_off(float *st, const float *p)
{
    felt_v *v = (felt_v *)st;
    v->held = 0.0f;
    v->th = fm_maxf(v->th, 0.05f * v->vel * p[3] * 0.02f * (0.4f + p[1] * 0.012f) * fm_minf(1.0f, v->es * 3.0f));
    v->th_mul = dk_decay(0.024f);
    v->th_inc *= 0.8f;
    v->th_ph = 0.0f;                             /* from a zero crossing: a thud, not a click */
}
static int felt_render(float *st, float *l, float *r, int n, const float *p)
{
    felt_v *v = (felt_v *)st;
    float inc = v->inc * dk_ctx.wow, inc2 = inc * 1.00128f, damp = v->held == 0.0f && !dk_ctx.pedal ? v->mr : 1.0f;
    /* the state in registers for the loop (the outputs might alias it otherwise) */
    float att = v->att, att_inc = v->att_inc, ef = v->ef, es = v->es, mf = v->mf * damp, ms = v->ms * damp;
    float idx = v->idx, idx_floor = v->idx_floor, idx_mul = v->idx_mul, ph2 = v->ph2, ph3 = v->ph3;
    float lp = v->lp, lp2 = v->lp2, lpk = v->lpk, lpk2 = v->lpk2, noise = v->noise, noise_mul = v->noise_mul;
    float g = v->vel * 0.30f, gl = v->gl, gr = v->gr;
    int j;
    for (j = 0; j < n; j++) {
        float s, m, a;
        if (att < 1.0f)
            att = fm_minf(1.0f, att + att_inc);
        ef *= mf;
        es *= ms;
        idx = idx_floor + (idx - idx_floor) * idx_mul;
        ph2 = dk_wrap1(ph2 + inc);              /* the modulator rides the carrier's phase (1:1) */
        ph3 = dk_wrap1(ph3 + inc2);
        m = dk_sinx(ph2) * idx;
        s = dk_sinx(ph2 + m + 4.0f) * 0.74f + dk_sinx(ph3) * 0.30f;
        if (noise > 1e-4f) {
            s += noise * wnoise();
            noise *= noise_mul;
        }
        lp += lpk * (s - lp);
        lp2 += lpk2 * (lp - lp2);
        a = lp2 * (ef + es) * att * g;
        l[j] += a * gl;
        r[j] += a * gr;
    }
    v->att = att;
    v->ef = ef;
    v->es = es;
    v->idx = idx;
    v->ph2 = ph2;
    v->ph3 = ph3;
    v->lp = lp;
    v->lp2 = lp2;
    v->noise = noise;
    if (v->th > 1e-4f) {                       /* the knock: a short low tock, a breath of felt (no hiss) */
        float th = v->th, th_mul = v->th_mul, th_ph = v->th_ph, th_inc = v->th_inc, t1 = v->th_lp, t2 = v->noise_mul2;
        for (j = 0; j < n && th > 1e-4f; j++) {
            float k;
            th_ph = dk_wrap1(th_ph + th_inc);
            t1 += 0.06f * (wnoise() - t1);
            t2 += 0.06f * (t1 - t2);
            k = (dk_sinx(th_ph) * 0.8f + t2 * 1.6f) * th;
            th *= th_mul;
            l[j] += k * gl;
            r[j] += k * gr;
        }
        v->th = j < n ? 0.0f : th;
        v->th_ph = th_ph;
        v->th_lp = t1;
        v->noise_mul2 = t2;
    }
    return !(v->ef + v->es < 1e-3f && v->att >= 1.0f && v->th <= 1e-4f);   /* -60 dB: under the reverb, unheard */
}
static float felt_level(const float *st)
{
    const felt_v *v = (const felt_v *)st;
    return (v->ef + v->es) * v->vel;
}
const dk_engine_t ENG_FELT = {"FELT", EF_POLY, 8, 13, 5, FELT_P, felt_on, felt_off, felt_render, felt_level, 0, 0, 0, 0,
                              R_KEYS};

/* ------------------------------------------------------------------- TINE --- */
typedef struct {
    float inc, ph1, ph2, vel, gl, gr;
    float ef, es, mf, ms, mr, att, att_inc;
    float idx, idx_floor, idx_mul, tidx, lp, lpk, ratio;
    float held, det, wob, wob_ph;
} tine_v;
CHECK_SIZE(tine_v);
static const dk_param_t TINE_P[] = {
    {"TINE", 0, 100, 50, VZ_BELL, 0}, {"TREMOLO", 0, 100, 35, VZ_WAVE, 0}, {"DRIFT", 0, 100, 30, VZ_WOBBLE, 0},
    {"BARK", 0, 100, 45, VZ_SUN, 0},  {"DECAY", 0, 100, 50, VZ_ENV, 0},
};
static void tine_on(float *st, int note, float vel, const float *p, int retrig)
{
    tine_v *v = (tine_v *)st;
    float tone = p[3] * 0.01f, f = dk_mtof((float)note), kt = fm_clampf((float)(note - 48) * (1.0f / 48.0f), -0.5f, 1.0f);
    float tau = fm_clampf(6.0f * fm_exp2f(-(float)(note - 48) * (1.0f / 24.0f)), 1.0f, 8.0f) *
                fm_exp2f((p[4] - 50.0f) * (1.0f / 50.0f)) * env_dec(p);
    (void)retrig;
    v->vel = vel;
    v->det = drift_of(p) * (1.0f + (frnd() - 0.5f) * 0.0016f * p[2] * 0.01f);
    v->inc = f * (1.0f / DK_SR);
    pan_of((float)note, 0.45f, &v->gl, &v->gr);
    v->es = 1.0f;
    v->ms = dk_decay(tau);
    v->ef = 0.20f + 0.50f * vel;
    v->mf = dk_decay(0.16f + 0.10f * (1.0f - vel));
    v->mr = dk_decay((note < 48 ? 0.30f : 0.20f) * env_rel(p));
    v->att_inc = 1.0f / (env_att(p, 0.0015f) * DK_SR);
    v->att = 0.0f;
    v->idx = (0.035f + 0.20f * vel * vel) * (0.55f + 0.9f * tone) * fm_clampf(1.4f - kt, 0.4f, 1.5f);
    v->idx_floor = v->idx * 0.22f;
    v->idx_mul = dk_decay(0.55f);
    v->ratio = fm_minf(14.0f, 16000.0f / f);
    v->tidx = (0.07f + 0.08f * vel) * (0.3f + 1.4f * p[0] * 0.01f);   /* the tine's ping */
    {                                                     /* its sidebands under 17 kHz */
        float fm = f * v->ratio, beta = (17000.0f - f) / fm - 1.0f;
        if (beta < 0.0f)
            beta = 0.0f;
        if (v->tidx * 6.2831853f > beta)
            v->tidx = beta * (1.0f / 6.2831853f);
    }
    v->lpk = dk_lpk((2600.0f + 6000.0f * tone) * (1.0f + 0.6f * kt));
    v->held = 1.0f;
    v->wob_ph = frnd();
    v->ph1 = v->ph2 = v->lp = 0.0f;
}
static void tine_off(float *st, const float *p)
{
    (void)p;
    ((tine_v *)st)->held = 0.0f;
}
static int tine_render(float *st, float *l, float *r, int n, const float *p)
{
    tine_v *v = (tine_v *)st;
    float trem = p[1] * 0.01f, drift = p[2] * 0.01f, tw = dk_sinx(dk_ctx.trem + 4.0f);
    float pan = 0.12f * dk_sinx(dk_ctx.lfo_mid + 4.0f) + 0.30f * trem * tw;   /* the suitcase's sway, its tremolo */
    float am = 1.0f - 0.45f * trem * (0.5f + 0.5f * tw), gl = (v->gl - pan) * am, gr = (v->gr + pan) * am;
    float inc, incm, damp = v->held == 0.0f && !dk_ctx.pedal ? v->mr : 1.0f, g = v->vel * 0.30f * 0.72f;
    float att = v->att, att_inc = v->att_inc, es = v->es, ef = v->ef, ms = v->ms * damp, mf = v->mf * damp;
    float idx = v->idx, idx_floor = v->idx_floor, idx_mul = v->idx_mul, ph1 = v->ph1, ph2 = v->ph2, tidx = v->tidx;
    float lp = v->lp, lpk = v->lpk;
    int j, tine = ef >= 2e-4f;
    v->wob_ph = dk_wrap1(v->wob_ph + (float)n * (0.45f / DK_SR));           /* the drift: a slow wobble, its own */
    inc = v->inc * v->det * dk_ctx.wow * (1.0f + 0.0016f * drift * dk_sinx(v->wob_ph + 4.0f));
    incm = inc * v->ratio;
    if (!tine)
        ef = 0.0f;
    for (j = 0; j < n; j++) {
        float s, a;
        if (att < 1.0f)
            att = fm_minf(1.0f, att + att_inc);
        es *= ms;
        idx = idx_floor + (idx - idx_floor) * idx_mul;
        ph2 = dk_wrap1(ph2 + inc);
        s = dk_sinx(ph2 + dk_sinx(ph2) * idx + 4.0f) * es;
        if (tine) {
            ef *= mf;
            ph1 = dk_wrap1(ph1 + incm);
            s += dk_sinx(ph2 + dk_sinx(ph1) * tidx + 4.0f) * ef;
        }
        lp += lpk * (s - lp);
        a = lp * att * g;
        l[j] += a * gl;
        r[j] += a * gr;
    }
    v->att = att;
    v->es = es;
    v->ef = ef;
    v->idx = idx;
    v->ph1 = ph1;
    v->ph2 = ph2;
    v->lp = lp;
    return !(v->es + v->ef < 1e-3f && v->att >= 1.0f);
}
static float tine_level(const float *st)
{
    const tine_v *v = (const tine_v *)st;
    return (v->es + v->ef) * v->vel;
}
const dk_engine_t ENG_TINE = {"TINE", EF_POLY, 5, 13, 5, TINE_P, tine_on, tine_off, tine_render, tine_level, 0, 0, 0, 0,
                              R_KEYS};

/* ---------------------------------------------------------------- KALIMBA --- */
/* a tine plucked: its modes (the fundamental, two inharmonic ones above that die fast), the thumb's click, and the box
 * of wood under it ringing at its own pitch */
typedef struct {
    float inc, ph1, ph2, ph3, vel, gl, gr;
    float a1, a2, a3, m1, m2, m3, mr;
    float click, click_mul, clp;
    float wlo, wbp, wf, wex, wex_mul, wood;
    float held, att, r2, r3;
} kal_v;
CHECK_SIZE(kal_v);
static const dk_param_t KAL_P[] = {
    {"TONE", 0, 100, 50, VZ_SUN, 0}, {"DECAY", 0, 100, 50, VZ_ENV, 0}, {"WOOD", 0, 100, 45, VZ_DUST, 0},
};
static void kal_on(float *st, int note, float vel, const float *p, int retrig)
{
    kal_v *v = (kal_v *)st;
    float tone = p[0] * 0.01f, dsc = fm_exp2f((p[1] - 50.0f) * (1.0f / 45.0f)) * env_dec(p), wood = p[2] * 0.01f;
    float f = dk_mtof((float)note) * drift_of(p), kt = fm_clampf((float)(note - 60) * (1.0f / 36.0f), -0.5f, 1.0f);
    float r2 = 5.93f, r3 = 11.4f;
    (void)retrig;
    v->vel = vel;
    v->inc = f * (1.0f / DK_SR);
    if (f * r3 > 17000.0f)
        r3 = 17000.0f / f;
    if (f * r2 > 17000.0f)
        r2 = 17000.0f / f;
    v->ph1 = v->ph2 = v->ph3 = 0.0f;
    v->r2 = r2;
    v->r3 = r3;
    pan_of((float)note, 0.5f, &v->gl, &v->gr);
    v->a1 = 1.0f;
    v->a2 = (0.18f + 0.30f * tone) * vel;
    v->a3 = (0.05f + 0.16f * tone) * vel;
    v->m1 = dk_decay(fm_clampf(1.9f - 0.9f * kt, 0.5f, 2.6f) * dsc);
    v->m2 = dk_decay(0.11f * dsc);
    v->m3 = dk_decay(0.035f);
    v->mr = dk_decay(0.18f * env_rel(p));
    v->click = (0.14f + 0.22f * tone) * vel;
    v->click_mul = dk_decay(0.0018f);
    v->wood = wood;
    v->wf = dk_svf_f(fm_clampf(f * 0.5f, 180.0f, 700.0f));
    v->wex = vel;
    v->wex_mul = dk_decay(0.006f);
    v->held = 1.0f;
    v->att = 0.0f;
    v->wlo = v->wbp = 0.0f;
}
static void kal_off(float *st, const float *p)
{
    (void)p;
    ((kal_v *)st)->held = 0.0f;
}
static int kal_render(float *st, float *l, float *r, int n, const float *p)
{
    kal_v *v = (kal_v *)st;
    float inc = v->inc * dk_ctx.wow, i2 = inc * v->r2, i3 = inc * v->r3, damp = v->held == 0.0f ? v->mr : 1.0f;
    float lvl = 0.30f, wood = v->wood;
    /* the state in registers for the loop (the outputs might alias it otherwise) */
    float ph1 = v->ph1, ph2 = v->ph2, ph3 = v->ph3, a1 = v->a1, a2 = v->a2, a3 = v->a3, m1 = v->m1, m2 = v->m2, m3 = v->m3;
    float att = v->att, click = v->click, click_mul = v->click_mul, wex = v->wex, wex_mul = v->wex_mul, wlo = v->wlo;
    float wbp = v->wbp, wf = v->wf, vel = v->vel, gl = v->gl, gr = v->gr;
    int j;
    (void)p;
    for (j = 0; j < n; j++) {
        float s, x, hp;
        ph1 = dk_wrap1(ph1 + inc);
        ph2 = dk_wrap1(ph2 + i2);
        ph3 = dk_wrap1(ph3 + i3);
        a1 *= m1 * damp;
        a2 = fm_flush(a2 * m2);                /* the upper modes die fast: no denormals (a slow path on an FPU) */
        a3 = fm_flush(a3 * m3);
        s = dk_sinx(ph1) * a1 + dk_sinx(ph2) * a2 + dk_sinx(ph3) * a3;
        if (att < 1.0f) {                      /* a millisecond's rise: no click but the thumb's */
            att = fm_minf(1.0f, att + 1.0f / (0.0008f * DK_SR));
            s *= att;
        }
        if (click > 1e-4f) {
            s += wnoise() * click * 0.5f;
            click *= click_mul;
        }
        /* the box: a resonance at its pitch, struck by the pluck and fed a little by the tine */
        x = s * 0.05f;
        if (wex > 1e-4f) {                     /* (only while it is heard: no denormals) */
            x = wnoise() * wex + x;
            wex *= wex_mul;
        }
        wlo += wf * wbp;
        hp = x - wlo - 0.12f * wbp;
        wbp = fm_flush(wbp + wf * hp);
        wlo = fm_flush(wlo);
        s = s * (1.0f - 0.35f * wood) + wbp * wood * 0.9f;
        s *= vel * lvl;
        l[j] += s * gl;
        r[j] += s * gr;
    }
    v->ph1 = ph1;
    v->ph2 = ph2;
    v->ph3 = ph3;
    v->a1 = a1;
    v->a2 = a2;
    v->a3 = a3;
    v->att = att;
    v->click = click;
    v->wex = wex;
    v->wlo = wlo;
    v->wbp = wbp;
    return v->a1 > 1e-3f || v->wex > 1e-4f || fm_fabsf(v->wbp) > 3e-4f;
}
static float kal_level(const float *st)
{
    const kal_v *v = (const kal_v *)st;
    return v->a1 * v->vel;
}
const dk_engine_t ENG_KALIMBA = {"KALIMBA", EF_POLY, 5, 22, 3, KAL_P, kal_on, kal_off, kal_render, kal_level, 0, 0, 0,
                                 0, R_ARP};
