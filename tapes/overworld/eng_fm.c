/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* FM: four operators (phase modulation, as the DX7 and the FM-1's own name), five algorithms, a TYPE each a patch: its
 * algorithm, the operators' ratios, levels and envelopes. TONE scales the modulators (the brightness), DECAY the
 * envelopes' times, GRIT the feedback, DETUNE spreads the carriers. Operators above 16 kHz are let go (no aliasing);
 * the modulation thins out up the keyboard. Part of tape_dsp.c. */

enum { FA_STACK, FA_PAIRS, FA_Y, FA_ORGAN, FA_TWIN };   /* 3>2>1>0 | 1>0 + 3>2 | 3>1, 1+2>0 | 0+1+2+3 | 3>2, 2>0 + 2>1 */
typedef struct {
    uint8_t alg;
    float ratio[4], level[4];                  /* a modulator's level: its index (turns); a carrier's: its amplitude */
    float att[4], dec[4], sus[4], rel[4];      /* seconds; sustain 0..1 */
    float fb;                                  /* operator 3's feedback (turns) */
} fm_patch_t;
#define FM_NTYPE 10
static const char *const FM_TYPES[FM_NTYPE] = {"E.PIANO", "BELL", "BASS", "BRASS", "ORGAN", "CLAV", "MALLET", "PLUCK",
                                                "PAD", "LEAD"};
static const fm_patch_t FM_PATCH[FM_NTYPE] = {
    /* E.PIANO: the body (1:1) and the tine (14:1), two pairs */
    {FA_PAIRS, {1, 1, 1, 14}, {1.0f, 0.30f, 0.55f, 0.20f}, {0.001f, 0.001f, 0.001f, 0.001f},
     {2.6f, 1.1f, 1.6f, 0.22f}, {0, 0.05f, 0, 0}, {0.35f, 0.35f, 0.3f, 0.3f}, 0.0f},
    /* BELL: inharmonic pairs, long */
    {FA_PAIRS, {1, 3.5f, 2.0f, 5.19f}, {1.0f, 0.42f, 0.45f, 0.28f}, {0.001f, 0.001f, 0.001f, 0.001f},
     {4.5f, 2.5f, 2.8f, 1.2f}, {0, 0, 0, 0}, {1.2f, 1.2f, 1.0f, 1.0f}, 0.0f},
    /* BASS: a round body, a pluck of harmonics at the start */
    {FA_Y, {1, 1, 2, 1}, {1.0f, 0.45f, 0.14f, 0.08f}, {0.002f, 0.001f, 0.001f, 0.001f},
     {1.4f, 0.32f, 0.12f, 0.4f}, {0.55f, 0.18f, 0, 0.2f}, {0.12f, 0.12f, 0.1f, 0.1f}, 0.08f},
    /* BRASS: a swell of harmonics, two carriers */
    {FA_TWIN, {1, 1, 1, 1}, {0.7f, 0.5f, 0.55f, 0.18f}, {0.045f, 0.05f, 0.07f, 0.05f},
     {0.6f, 0.6f, 0.45f, 0.5f}, {0.85f, 0.85f, 0.45f, 0.5f}, {0.22f, 0.22f, 0.2f, 0.2f}, 0.12f},
    /* ORGAN: four drawbars (16', 8', 4', 2 2/3') */
    {FA_ORGAN, {0.5f, 1, 2, 3}, {0.55f, 0.7f, 0.42f, 0.25f}, {0.004f, 0.004f, 0.004f, 0.004f},
     {1.0f, 1.0f, 1.0f, 1.0f}, {1, 1, 1, 1}, {0.07f, 0.07f, 0.07f, 0.07f}, 0.04f},
    /* CLAV: a bright stack, short */
    {FA_STACK, {1, 3, 1, 1}, {1.0f, 0.55f, 0.25f, 0.12f}, {0.001f, 0.001f, 0.001f, 0.001f},
     {0.7f, 0.22f, 0.3f, 0.3f}, {0.15f, 0.08f, 0, 0}, {0.06f, 0.06f, 0.06f, 0.06f}, 0.05f},
    /* MALLET: a struck bar (4:1 its overtone) */
    {FA_PAIRS, {1, 4, 4, 1}, {1.0f, 0.3f, 0.18f, 0.12f}, {0.001f, 0.001f, 0.001f, 0.001f},
     {0.9f, 0.07f, 0.25f, 0.1f}, {0, 0, 0, 0}, {0.3f, 0.3f, 0.2f, 0.2f}, 0.0f},
    /* PLUCK: a koto, a harp */
    {FA_Y, {1, 2, 5, 1}, {1.0f, 0.38f, 0.1f, 0.06f}, {0.001f, 0.001f, 0.001f, 0.001f},
     {1.3f, 0.16f, 0.05f, 0.2f}, {0, 0, 0, 0}, {0.4f, 0.4f, 0.4f, 0.4f}, 0.0f},
    /* PAD: a slow, warm swell */
    {FA_TWIN, {1, 1.002f, 2, 1}, {0.6f, 0.55f, 0.22f, 0.08f}, {0.9f, 1.0f, 1.2f, 1.0f},
     {2.0f, 2.0f, 2.0f, 2.0f}, {0.9f, 0.9f, 0.6f, 0.5f}, {1.6f, 1.6f, 1.4f, 1.4f}, 0.06f},
    /* LEAD: a sung stack, the feedback for its edge */
    {FA_STACK, {1, 2, 1, 1}, {0.9f, 0.35f, 0.3f, 0.25f}, {0.008f, 0.01f, 0.01f, 0.01f},
     {0.8f, 0.8f, 0.8f, 0.8f}, {0.9f, 0.7f, 0.7f, 0.7f}, {0.25f, 0.25f, 0.25f, 0.25f}, 0.18f},
};

typedef struct {
    float inc, ph[4], r[4], lv[4], e[4], ain[4], dm[4], su[4], rm[4], up[4];   /* up: 1 while rising */
    float fb, fb1, fb2, vel, gl, gr, held, det, alg, dc;   /* dc: what the 1:1 sidebands leave at 0 Hz */
} fm_v;
CHECK_SIZE(fm_v);
static const dk_param_t FM_P[] = {
    {"TONE", 0, 100, 50, VZ_SUN, 0},   {"DECAY", 0, 100, 50, VZ_ENV, 0},  {"GRIT", 0, 100, 0, VZ_DUST, 0},
    {"TYPE", 0, FM_NTYPE - 1, 0, VZ_TYPE, FM_TYPES}, {"DETUNE", 0, 100, 25, VZ_WOBBLE, 0},
    {"VIBRATO", 0, 100, 0, VZ_WAVE, 0},
};

static void fm_on(float *st, int note, float vel, const float *p, int retrig)
{
    fm_v *v = (fm_v *)st;
    int k, ty = (int)fm_clampf(p[3], 0.0f, (float)(FM_NTYPE - 1));
    const fm_patch_t *pt = &FM_PATCH[ty];
    float f = dk_mtof((float)note), bright = fm_exp2f((p[0] - 50.0f) * (1.0f / 35.0f)),
          dsc = fm_exp2f((p[1] - 50.0f) * (1.0f / 30.0f)) * env_dec(p), rsc = env_rel(p),
          ks = fm_clampf(1.0f - (float)(note - 60) * (1.0f / 40.0f), 0.3f, 1.25f);
    v->alg = (float)pt->alg;
    v->vel = vel;
    v->inc = f * drift_of(p) * (1.0f / DK_SR);
    v->det = 1.0f + p[4] * 0.0002f;                       /* 100: 34 cents apart */
    v->fb = fm_minf(0.25f, pt->fb + p[2] * 0.002f);
    pan_of((float)note, 0.4f, &v->gl, &v->gr);
    for (k = 0; k < 4; k++) {
        int car = k == 0 || (pt->alg == FA_PAIRS && k == 2) || pt->alg == FA_ORGAN || (pt->alg == FA_TWIN && k == 1);
        float lv = pt->level[k], att = car ? env_att(p, pt->att[k]) : pt->att[k];
        v->r[k] = pt->ratio[k];
        if (!car)                                         /* a modulator: brighter played harder, darker up high */
            lv = fm_minf(2.5f, lv * bright * (0.45f + 0.75f * vel) * ks);
        if (f * pt->ratio[k] > 16000.0f)
            lv = 0.0f;
        v->lv[k] = lv;
        v->ain[k] = 1.0f / (fm_maxf(att, 0.0005f) * DK_SR);
        v->dm[k] = dk_decay(pt->dec[k] * dsc);
        v->su[k] = pt->sus[k];
        v->rm[k] = dk_decay(pt->rel[k] * rsc);
        if (!retrig || v->e[k] <= 0.0f)
            v->e[k] = 0.0f;
        v->up[k] = 1.0f;
        if (!retrig)
            v->ph[k] = 0.0f;
    }
    if (!retrig)
        v->dc = 0.0f;
    v->held = 1.0f;
}
static void fm_off(float *st, const float *p)
{
    (void)p;
    ((fm_v *)st)->held = 0.0f;
}
static int fm_render(float *st, float *l, float *r, int n, const float *p)
{
    fm_v *v = (fm_v *)st;
    int j, k, alg = (int)v->alg, held = v->held != 0.0f;
    float vib = p[5] > 0.5f ? 1.0f + p[5] * 0.00006f * dk_sinx(dk_ctx.lfo_fast + 4.0f) : 1.0f;
    float inc = v->inc * dk_ctx.wow * vib, g = v->vel * 0.26f, gl = v->gl, gr = v->gr, det = v->det, fb = v->fb;
    float ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], ph3 = v->ph[3], fb1 = v->fb1, fb2 = v->fb2;
    float i0 = inc * v->r[0] / det, i1 = inc * v->r[1] * (alg == FA_TWIN ? det : 1.0f), i2 = inc * v->r[2] * det,
          i3 = inc * v->r[3];
    float e[4], lv0 = v->lv[0], lv1 = v->lv[1], lv2 = v->lv[2], lv3 = v->lv[3], peak = 0.0f, dc = v->dc;
    for (k = 0; k < 4; k++)
        e[k] = v->e[k];
    for (j = 0; j < n; j++) {
        float o3, o2, o1, s;
        for (k = 0; k < 4; k++) {                          /* the envelopes: up, down to the sustain, let go */
            if (v->up[k] != 0.0f) {
                e[k] += v->ain[k];
                if (e[k] >= 1.0f) {
                    e[k] = 1.0f;
                    v->up[k] = 0.0f;
                }
            } else if (held)
                e[k] = v->su[k] + (e[k] - v->su[k]) * v->dm[k];
            else
                e[k] = fm_flush(e[k] * v->rm[k]);
        }
        ph0 = dk_wrap1(ph0 + i0);
        ph1 = dk_wrap1(ph1 + i1);
        ph2 = dk_wrap1(ph2 + i2);
        ph3 = dk_wrap1(ph3 + i3);
        o3 = dk_sinx(ph3 + (fb1 + fb2) * 0.5f * fb + 4.0f) * lv3 * e[3];   /* (fed back averaged: no chaos) */
        fb2 = fb1;
        fb1 = o3;
        switch (alg) {
        case FA_STACK:
            o2 = dk_sinx(ph2 + o3 + 4.0f) * lv2 * e[2];
            o1 = dk_sinx(ph1 + o2 + 4.0f) * lv1 * e[1];
            s = dk_sinx(ph0 + o1 + 4.0f) * lv0 * e[0];
            break;
        case FA_PAIRS:
            o1 = dk_sinx(ph1 + 4.0f) * lv1 * e[1];
            s = dk_sinx(ph0 + o1 + 4.0f) * lv0 * e[0] + dk_sinx(ph2 + o3 + 4.0f) * lv2 * e[2];
            break;
        case FA_Y:
            o1 = dk_sinx(ph1 + o3 + 4.0f) * lv1 * e[1];
            o2 = dk_sinx(ph2 + 4.0f) * lv2 * e[2];
            s = dk_sinx(ph0 + o1 + o2 + 4.0f) * lv0 * e[0];
            break;
        case FA_ORGAN:
            s = dk_sinx(ph0) * lv0 * e[0] + dk_sinx(ph1) * lv1 * e[1] + dk_sinx(ph2) * lv2 * e[2] + o3;
            break;
        default:                                          /* FA_TWIN */
            o2 = dk_sinx(ph2 + o3 + 4.0f) * lv2 * e[2];
            s = dk_sinx(ph0 + o2 + 4.0f) * lv0 * e[0] + dk_sinx(ph1 + o2 + 4.0f) * lv1 * e[1];
            break;
        }
        dc += 0.0012f * (s - dc);                          /* a DC blocker (~8 Hz) */
        s = (s - dc) * g;
        l[j] += s * gl;
        r[j] += s * gr;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->ph[3] = ph3;
    v->dc = fm_flush(dc);
    v->fb1 = fm_flush(fb1);
    v->fb2 = fm_flush(fb2);
    for (k = 0; k < 4; k++) {
        v->e[k] = e[k];
        if (k == 0 || alg == FA_ORGAN || (alg == FA_PAIRS && k == 2) || (alg == FA_TWIN && k == 1))
            peak = fm_maxf(peak, e[k] * (alg == FA_ORGAN ? 1.0f : v->lv[k]));
    }
    return (held && peak > 1e-4f) || peak > 1e-3f || v->up[0] != 0.0f;
}
static float fm_level(const float *st)
{
    const fm_v *v = (const fm_v *)st;
    return fm_maxf(v->e[0], v->alg == (float)FA_PAIRS ? v->e[2] : v->alg == (float)FA_TWIN ? v->e[1] : 0.0f) * v->vel;
}
const dk_engine_t ENG_FM = {"FM", EF_POLY, 6, 15, 6, FM_P, fm_on, fm_off, fm_render, fm_level, 0, 0, 0, 0, R_KEYS};
