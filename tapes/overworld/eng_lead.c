/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* LEAD: a lead synth for a single line. One voice that glides (a note played while one is held slides to it: GLIDE), a
 * vibrato that comes in after the note has begun (VIBRATO), the waves of a lead: a saw (two, spread), a square, a moving
 * pulse, a supersaw (three), a triangle, a two-operator FM voice; a sub octave under it (SUB); through a resonant low-pass
 * whose cut-off a note's envelope sweeps. Built on SYNTH's band-limited oscillators (syn_osc). Part of tape_dsp.c. */
typedef struct {
    float inc, tinc, glk, ph1, ph2, ph3, phs, pm, vph, vel, gl, gr;
    float es, att, att_inc, mr, held, kind, t;
    float fe, fe_mul, fc0, fe_oct, q, lo, bp, open;
    float det, vdep, sub, fmi, pwm, kt, lp, lpk;
} ld_v;
CHECK_SIZE(ld_v);
static const char *const LD_WAVES[] = {"SAW", "SQUARE", "PULSE", "SUPER", "TRI", "FM"};
static const dk_param_t LD_P[] = {
    {"WAVE", 0, 5, 0, VZ_TYPE, LD_WAVES}, {"CUTOFF", 0, 100, 70, VZ_SUN, 0}, {"RESO", 0, 100, 20, VZ_WAVE, 0},
    {"SWEEP", 0, 100, 30, VZ_DROP, 0},    {"SPREAD", 0, 100, 30, VZ_BAR, 0}, {"VIBRATO", 0, 100, 30, VZ_WOBBLE, 0},
    {"GLIDE", 0, 100, 0, VZ_ENV, 0},      {"SUB", 0, 100, 0, VZ_SWELL, 0},
};
static void ld_pitch(ld_v *v, int note, const float *p)
{
    float g = p[6];
    v->tinc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    v->kt = fm_exp2f((float)(note - 60) * (1.0f / 36.0f));
    v->fc0 = fm_minf(50.0f * fm_exp2f(p[1] * 0.082f) * v->kt, 7400.0f);
    /* GLIDE: 0 jumps; up to about a second (the time to travel most of the way) */
    v->glk = g < 1.0f ? 1.0f : 1.0f - fm_expf(-1.0f / (0.012f * fm_exp2f(g * 0.065f) * DK_SR));
}
static void ld_on(float *st, int note, float vel, const float *p, int retrig)
{
    ld_v *v = (ld_v *)st;
    int k = (int)p[0];
    float spread = p[4] * 0.01f;
    k = k < 0 ? 0 : k > 5 ? 5 : k;
    if (retrig == 2) {                                  /* legato: the same voice goes to the new pitch */
        ld_pitch(v, note, p);
        v->vel = vel;
        v->fe = fm_maxf(v->fe, 0.5f);                   /* (the cut-off opens a little again) */
        return;
    }
    v->kind = (float)k;
    v->vel = vel;
    ld_pitch(v, note, p);
    v->inc = v->tinc;
    pan_of((float)note, k == 3 ? 0.7f : 0.45f, &v->gl, &v->gr);
    v->det = k == 3 ? 1.0f + spread * 0.014f : 1.0f + spread * 0.008f;
    v->open = (p[1] >= 97.0f && p[3] < 1.0f) ? 1.0f : 0.0f;
    v->q = 1.5f - 1.3f * p[2] * 0.01f;
    v->fe_oct = p[3] * 0.05f;
    v->fe = 1.0f;
    v->fe_mul = dk_decay(0.22f * env_dec(p) * (1.0f / 16.0f));
    v->att_inc = 1.0f / (env_att(p, 0.006f) * DK_SR);
    v->att = 0.0f;
    v->mr = dk_decay(0.18f * env_rel(p));
    v->es = 0.0f;
    v->ph1 = k == 3 ? frnd() : 0.0f;
    v->ph2 = frnd();
    v->ph3 = frnd();
    v->phs = 0.0f;
    v->pm = 0.0f;
    v->vph = 0.0f;
    v->vdep = p[5] * 0.01f * 0.035f;                    /* up to ~60 cents */
    v->sub = p[7] * 0.01f;
    v->fmi = 0.15f + spread * 1.1f;                     /* FM: the index (in turns) */
    v->pwm = 0.0f;
    v->lp = 0.0f;
    v->lpk = k == 1 || k == 2 ? 0.42f : 1.0f;         /* the old chip's output: its own low-pass (as SYNTH's pulse) */
    v->t = 0.0f;
    v->lo = v->bp = 0.0f;
    v->held = 1.0f;
}
static void ld_off(float *st, const float *p)
{
    (void)p;
    ((ld_v *)st)->held = 0.0f;
}
static int ld_render(float *st, float *l, float *r, int n, const float *p)
{
    ld_v *v = (ld_v *)st;
    int k = (int)v->kind, j, filt = v->open == 0.0f ? 1 : 0;
    float w = dk_ctx.wow, lvl = 0.2f, fk = 0.0f;
    float two = k == 3 ? 1.0f : (v->det > 1.0005f && k != 4 && k != 5 ? 1.0f : 0.0f);
    float g = k == 3 ? 0.42f : two != 0.0f ? 0.6f : 0.85f;
    static const uint8_t OSC[6] = {0, 3, 1, 5, 4, 7};     /* syn_osc's kinds: saw, square, pulse, super, tri, sine */
    for (j = 0; j < n; j++) {
        float s, a, d1, d2, d3, vib, vin, pw;
        if ((j & 15) == 0 && filt) {
            float hz = fm_minf(6000.0f, v->fc0 * fm_exp2f(v->fe_oct * v->fe));
            fk = fm_minf(0.8f, dk_svf_f(hz));
            v->fe *= v->fe_mul;
        }
        v->inc += (v->tinc - v->inc) * v->glk;
        v->vph = dk_wrap1(v->vph + 5.4f * (1.0f / DK_SR));
        vin = fm_minf(1.0f, fm_maxf(0.0f, (v->t - 0.3f) * 1.6f));   /* the vibrato comes in after the note began */
        vib = 1.0f + v->vdep * vin * dk_sinx(v->vph + 4.0f);
        d1 = v->inc * w * vib;
        d2 = d1 * v->det;
        d3 = d1 * (2.0f - v->det);
        pw = k == 2 ? 0.25f + 0.12f * dk_sinx(v->vph * 0.37f + 4.0f) : 0.5f;
        if (v->held != 0.0f) {
            if (v->att < 1.0f) {
                v->att = fm_minf(1.0f, v->att + v->att_inc);
                v->es = v->att;
            } else
                v->es = 1.0f;
        } else
            v->es *= v->mr;
        v->ph1 = dk_wrap1(v->ph1 + d1);
        if (k == 5) {                                       /* FM: a sine, a sine of the same pitch bends it; the index dies away with the cut-off's envelope */
            v->pm = dk_wrap1(v->pm + d1);
            s = dk_sinx(v->ph1 + 4.0f + v->fmi * (0.3f + 0.7f * v->fe) * dk_sinx(v->pm + 4.0f));
        } else {
            s = syn_osc(OSC[k], v->ph1, d1, pw);
            if (two != 0.0f) {
                v->ph2 = dk_wrap1(v->ph2 + d2);
                if (k == 3) {
                    v->ph3 = dk_wrap1(v->ph3 + d3);
                    s += syn_osc(OSC[k], v->ph2, d2, pw) + syn_osc(OSC[k], v->ph3, d3, pw);
                } else
                    s += syn_osc(OSC[k], v->ph2, d2, pw) * 0.8f;
            }
        }
        s *= g;
        if (v->sub > 0.0f) {                                /* a sine an octave down */
            v->phs = dk_wrap1(v->phs + d1 * 0.5f);
            s += dk_sinx(v->phs + 4.0f) * v->sub * 0.7f;
        }
        if (filt) {
            float hp;
            v->lo += fk * v->bp;
            hp = s - v->lo - v->q * v->bp;
            v->bp = fm_flush(v->bp + fk * hp);
            v->lo = fm_flush(v->lo);
            s = fm_clampf(v->lo, -2.0f, 2.0f);
        }
        v->lp += v->lpk * (s - v->lp);
        s = v->lp;
        a = s * v->es * v->vel * lvl;
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    v->t += (float)n * (1.0f / DK_SR);
    return !((v->held == 0.0f && v->es < 1e-3f) || (v->att >= 1.0f && v->es < 1e-4f));
}
static float ld_level(const float *st)
{
    const ld_v *v = (const ld_v *)st;
    return v->es * v->vel * 0.5f;
}
const dk_engine_t ENG_LEAD = {"LEAD", EF_MONO, 2, 9, 8, LD_P, ld_on, ld_off, ld_render, ld_level, 0, 0, 0, 0, R_LEAD};
