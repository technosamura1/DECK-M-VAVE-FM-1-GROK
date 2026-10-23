/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's breath: WINDS, the flutes and the brass of a small band far off. FLUTE (a sine with a little of its
 * octave and twelfth, the breath through it, a chiff at the start), PAN (more air, a hollow tone), OCARINA (nearly a
 * pure sine), REED (a clarinet: the odd harmonics, a square through a soft low-pass), TRUMPET (a saw through a
 * low-pass that opens with the breath, the note scooped up into), MUTED (the same through a narrow band: the cup
 * mute's nose), HORN (dark, slow), BRASS (two saws apart, a section). The vibrato comes in late, as a player's does.
 * Part of tape_dsp.c. */

typedef struct {
    float inc, ph1, ph2, vel, gl, gr;
    float es, att_inc, mr, held, kind, t, t_inc;
    float lp, lp2, lp3, nz, chiff, scoop, vib, det, hp;
} wind_v;
CHECK_SIZE(wind_v);
static const char *const WIND_TYPES[] = {"FLUTE", "PAN", "OCARINA", "REED", "TRUMPET", "MUTED", "HORN", "BRASS"};
static const dk_param_t WIND_P[] = {
    {"BRIGHT", 0, 100, 50, VZ_SUN, 0}, {"BREATH", 0, 100, 40, VZ_DUST, 0}, {"VIBRATO", 0, 100, 35, VZ_WOBBLE, 0},
    {"TYPE", 0, 7, 0, VZ_TYPE, WIND_TYPES}, {"SWELL", 0, 100, 50, VZ_SWELL, 0},
};
/* by type: the attack (s), the release (s), the low-pass (Hz at BRIGHT 0 and the more at 100), the breath's part,
 * the scoop (semitones under), the level */
static const float W_ATT[8] = {0.07f, 0.035f, 0.05f, 0.04f, 0.05f, 0.04f, 0.13f, 0.16f};
static const float W_REL[8] = {0.18f, 0.16f, 0.2f, 0.12f, 0.16f, 0.14f, 0.3f, 0.32f};
static const float W_FC0[8] = {900.0f, 700.0f, 600.0f, 700.0f, 500.0f, 700.0f, 300.0f, 400.0f};
static const float W_FC1[8] = {3000.0f, 2400.0f, 1500.0f, 2600.0f, 4200.0f, 2200.0f, 1500.0f, 3000.0f};
static const float W_AIR[8] = {0.22f, 0.45f, 0.14f, 0.06f, 0.04f, 0.04f, 0.03f, 0.03f};
static const float W_SCOOP[8] = {0.12f, 0.2f, 0.0f, 0.1f, 0.6f, 0.4f, 0.35f, 0.45f};
static const float W_LVL[8] = {0.21f, 0.19f, 0.21f, 0.15f, 0.24f, 0.25f, 0.27f, 0.27f};

static void wind_on(float *st, int note, float vel, const float *p, int retrig)
{
    wind_v *v = (wind_v *)st;
    int k = (int)p[3];
    float tone = p[0] * 0.01f, kt = fm_clampf((float)(note - 60) * (1.0f / 36.0f), -0.6f, 1.0f), sw;
    k = k < 0 ? 0 : k > 7 ? 7 : k;
    v->kind = (float)k;
    v->vel = vel;
    v->inc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    pan_of((float)note, 0.4f, &v->gl, &v->gr);
    sw = fm_exp2f((p[4] - 50.0f) * (1.0f / 20.0f));      /* SWELL: the breath's rise, a sixth to six times its own */
    v->att_inc = 1.0f / (env_att(p, W_ATT[k] * sw) * DK_SR);
    v->mr = 1.0f / (W_REL[k] * env_rel(p) * DK_SR);
    v->lp3 = dk_lpk(fm_minf(W_FC0[k] + W_FC1[k] * tone * (0.7f + 0.3f * vel) + 600.0f * kt, 9000.0f));
    v->det = k == 7 ? 1.0f + 0.0035f * (0.5f + 0.5f * tone) : 1.0f;
    if (retrig != 2) {                                     /* a new breath (legato keeps the one going) */
        v->scoop = -W_SCOOP[k] * 0.0578f * (1.1f - 0.4f * vel);   /* a fraction of the pitch: semitones under */
        v->chiff = k <= 2 ? 0.5f + 0.5f * vel : k == 3 ? 0.25f : 0.0f;
        v->t = 0.0f;
    }
    v->t_inc = 1.0f / DK_SR;
    if (!retrig) {
        v->ph1 = frnd();
        v->ph2 = frnd();
        v->es = 0.0f;
        v->lp = v->lp2 = v->nz = v->hp = 0.0f;
        v->vib = frnd();
    }
    v->held = 1.0f;
}
static void wind_off(float *st, const float *p)
{
    (void)p;
    ((wind_v *)st)->held = 0.0f;
}
static int wind_render(float *st, float *l, float *r, int n, const float *p)
{
    wind_v *v = (wind_v *)st;
    int k = (int)v->kind, j, brass = k >= 4, flute = k <= 2;
    float tone = p[0] * 0.01f, breath = p[1] * 0.01f, vibd = p[2] * 0.01f, w = dk_ctx.wow, t = v->held != 0.0f ? 1.0f : 0.0f;
    float vin = fm_minf(1.0f, fm_maxf(0.0f, (v->t - 0.3f) * 2.2f));   /* the vibrato: none at first, in by ~0.75 s */
    float vb = 1.0f + vibd * vin * (k == 2 ? 0.0028f : 0.0045f) * dk_sinx(dk_ctx.lfo_fast + v->vib);
    float h2 = k == 0 ? 0.10f + 0.16f * tone : k == 1 ? 0.06f + 0.08f * tone : 0.02f, h3 = k == 0 ? 0.04f + 0.08f * tone : 0.03f;
    float air = W_AIR[k] * (0.3f + 1.7f * breath), lvl = W_LVL[k], nk = flute ? 0.16f : 0.12f;
    float lpk = v->lp3, d1, d2;
    for (j = 0; j < n; j++) {
        float s, a, e, nzw;
        if (v->es < t)
            v->es = fm_minf(t, v->es + v->att_inc);
        else
            v->es = fm_maxf(0.0f, v->es - v->mr);
        v->scoop *= 0.9985f;                               /* up into the note, ~15 ms */
        d1 = v->inc * w * vb * (1.0f + v->scoop);
        d2 = d1 * v->det;
        v->ph1 = dk_wrap1(v->ph1 + d1);
        e = v->es;
        nzw = wnoise();
        v->nz += nk * (nzw - v->nz);                       /* the breath: noise, a little darker */
        if (flute) {
            s = dk_sinx(v->ph1 + 4.0f) + h2 * dk_sinx(2.0f * v->ph1 + 4.0f) + h3 * dk_sinx(3.0f * v->ph1 + 4.0f);
            s = s * (0.85f + 0.15f * dk_sinx(v->ph1 * 1.0f + 4.25f) * v->nz) + v->nz * air * (1.3f - 0.5f * e);
            if (v->chiff > 0.0005f) {                      /* the chiff: a puff at the edge, the octave over it */
                s += (nzw * 0.5f + dk_sinx(2.0f * v->ph1 + 4.0f) * 0.4f) * v->chiff * 0.6f;
                v->chiff *= 0.9965f;
            }
        } else if (k == 3) {                               /* REED: the odd ones (a square), softened */
            float q = dk_blep(v->ph1, d1) - dk_blep(dk_wrap1(v->ph1 + 0.5f), d1);
            s = q + v->nz * air * 2.0f;
            if (v->chiff > 0.0005f) {
                s += nzw * v->chiff * 0.3f;
                v->chiff *= 0.996f;
            }
        } else {                                           /* the brass: a saw (BRASS two), the breath opening it */
            s = dk_blep(v->ph1, d1);
            if (k == 7) {
                v->ph2 = dk_wrap1(v->ph2 + d2);
                s = (s + dk_blep(v->ph2, d2)) * 0.7f;
            }
            s += v->nz * air * 2.0f;
        }
        if (brass) {                                       /* the cut-off rises with the breath (the brass's bloom) */
            float kk = fm_minf(0.85f, lpk * (0.25f + 0.95f * e * e));
            v->lp += kk * (s - v->lp);
            v->lp2 += kk * (v->lp - v->lp2);
            if (k == 5) {                                  /* MUTED: the lows taken out too (the cup's nose) */
                v->hp += 0.05f * (v->lp2 - v->hp);
                s = (v->lp2 - v->hp) * 1.5f;
            } else
                s = v->lp2;
        } else {
            v->lp += lpk * (s - v->lp);
            v->lp2 += lpk * (v->lp - v->lp2);
            s = k == 3 ? v->lp2 : v->lp * 0.7f + v->lp2 * 0.3f;
        }
        v->t += v->t_inc;
        a = s * e * v->vel * lvl * (0.75f + 0.25f * e);
        l[j] += a * v->gl;
        r[j] += a * v->gr;
    }
    return !(v->held == 0.0f && v->es <= 0.0f);
}
static float wind_level(const float *st)
{
    const wind_v *v = (const wind_v *)st;
    return v->es * v->vel * 0.5f;
}
const dk_engine_t ENG_WINDS = {"WINDS", EF_POLY, 4, 8, 5, WIND_P, wind_on, wind_off, wind_render, wind_level, 0, 0, 0, 0,
                               R_LEAD};
