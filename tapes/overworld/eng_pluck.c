/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* PLUCK: a plucked string (Karplus-Strong, extended: the pick's brightness and place, a loop filter for the string's
 * tone, an all-pass for its exact pitch). Each voice rings in a line of its own: twelve 16-bit lines in the pool, lent
 * to the voices played last (a voice whose line was lent on ends). Part of tape_dsp.c. */

#define PK_NSLOT 12
#define PK_N 1024                              /* the longest period: 43 Hz (lower notes go an octave up) */
static int16_t pk_buf[PK_NSLOT][PK_N] DK_POOL;
static uint32_t pk_uid[PK_NSLOT], pk_last[PK_NSLOT], pk_clock, pk_next_uid;

typedef struct {
    float pick, place, damp, t60, body, rel;   /* the pick's brightness (0..1), where (0..0.5), the loop's low-pass
                                                * (0.5 dark .. 0.05 bright), the ring at C4 (s), its body's top (Hz),
                                                * the hand damping it (s) */
} pk_type_t;
#define PK_NTYPE 6
static const char *const PK_TYPES[PK_NTYPE] = {"NYLON", "STEEL", "HARP", "KOTO", "BASS", "MUTED"};
static const pk_type_t PK_TYPE[PK_NTYPE] = {
    {0.45f, 0.18f, 0.42f, 3.5f, 4200.0f, 0.12f},  /* NYLON: a classical guitar, warm */
    {0.75f, 0.13f, 0.25f, 5.0f, 9000.0f, 0.12f},  /* STEEL: bright, long */
    {0.55f, 0.30f, 0.30f, 6.0f, 7000.0f, 0.6f},   /* HARP: plucked in the middle, rings on */
    {0.85f, 0.08f, 0.20f, 2.6f, 9000.0f, 0.15f},  /* KOTO: near the bridge, a twang */
    {0.35f, 0.20f, 0.48f, 3.0f, 2200.0f, 0.08f},  /* BASS: a round fretted bass */
    {0.40f, 0.15f, 0.50f, 0.35f, 3000.0f, 0.05f}, /* MUTED: the palm on the strings */
};

typedef struct {
    float slot, uid, w, nint, ap_c, ap_x, ap_y, prev, s, g, g_rel, z1, z2, b0, b1, a1, a2, vel, gl, gr, held, amp;
} pk_v;
CHECK_SIZE(pk_v);
static const dk_param_t PK_P[] = {
    {"PICK", 0, 100, 50, VZ_SUN, 0},  {"DECAY", 0, 100, 50, VZ_ENV, 0}, {"TONE", 0, 100, 50, VZ_BAR, 0},
    {"TYPE", 0, PK_NTYPE - 1, 0, VZ_TYPE, PK_TYPES}, {"PLACE", 0, 100, 50, VZ_BAR, 0},
};

static void pk_on(float *st, int note, float vel, const float *p, int retrig)
{
    pk_v *v = (pk_v *)st;
    const pk_type_t *ty = &PK_TYPE[(int)fm_clampf(p[3], 0.0f, (float)(PK_NTYPE - 1))];
    int i, slot = 0, n, pp;
    float f = dk_mtof((float)note) * drift_of(p), per, d, bright, place, k, lp = 0.0f, lp2 = 0.0f, t60, amp = 0.0f;
    int16_t *b;
    (void)retrig;
    while (f < DK_SR / (float)(PK_N - 2))
        f *= 2.0f;
    for (i = 1; i < PK_NSLOT; i++)                        /* the line rung longest ago */
        if (pk_clock - pk_last[i] > pk_clock - pk_last[slot])
            slot = i;
    pk_uid[slot] = ++pk_next_uid;
    pk_last[slot] = ++pk_clock;                         /* (lent now: the next note takes another) */
    v->slot = (float)slot;
    v->uid = (float)(pk_uid[slot] & 0xFFFFFFu);
    pk_uid[slot] &= 0xFFFFFFu;
    /* the loop: its low-pass (s: a delay of s samples), the all-pass for the fraction left */
    v->s = fm_clampf(ty->damp * fm_exp2f((50.0f - p[2]) * (1.0f / 60.0f)), 0.04f, 0.5f);
    per = DK_SR / f;
    v->nint = (float)(int)(per - v->s - 0.1f);
    d = per - v->s - v->nint;                             /* 0.1 .. 1.1 */
    v->ap_c = (1.0f - d) / (1.0f + d);
    v->ap_x = v->ap_y = v->prev = 0.0f;
    t60 = ty->t60 * fm_exp2f((p[1] - 50.0f) * (1.0f / 30.0f)) * env_dec(p) *
          fm_clampf(fm_exp2f(-(float)(note - 60) * (1.0f / 24.0f)), 0.35f, 2.5f);
    v->g = fm_exp2f(-9.9658f * per / (t60 * DK_SR));      /* 10^(-3 per / (t60 sr)): -60 dB in t60 */
    v->g_rel = fm_exp2f(-9.9658f * per / (ty->rel * env_rel(p) * DK_SR));
    {   /* the body: its top, a 2-pole Butterworth (bilinear: nothing left at Nyquist) */
        float kk = fm_tanf(3.1415927f * fm_minf(ty->body * (0.6f + 0.6f * vel), 12000.0f) / DK_SR),
              nrm = 1.0f / (1.0f + 1.4142136f * kk + kk * kk);
        v->b0 = kk * kk * nrm;
        v->b1 = 2.0f * v->b0;
        v->a1 = 2.0f * (kk * kk - 1.0f) * nrm;
        v->a2 = (1.0f - 1.4142136f * kk + kk * kk) * nrm;
        v->z1 = v->z2 = 0.0f;
    }
    v->vel = vel;
    pan_of((float)note, 0.5f, &v->gl, &v->gr);
    v->held = 1.0f;
    /* the pluck: noise, as bright as the pick and the hand's strength, from its place on the string (a comb) */
    n = (int)v->nint;
    b = pk_buf[slot];
    bright = fm_clampf(ty->pick * fm_exp2f((p[0] - 50.0f) * (1.0f / 40.0f)) * (0.55f + 0.6f * vel), 0.05f, 1.0f);
    k = bright * bright;
    place = fm_clampf(ty->place * fm_exp2f((p[4] - 50.0f) * (1.0f / 50.0f)), 0.04f, 0.5f);
    pp = (int)(place * (float)n);
    for (i = 0; i < n; i++) {
        lp += k * (wnoise() - lp);
        lp2 += k * (lp - lp2);
        b[i] = (int16_t)(fm_clampf(lp2, -1.0f, 1.0f) * 16000.0f);
    }
    for (i = n - 1; i >= pp && pp > 0; i--)               /* the place: the wave less its own reflection */
        b[i] = (int16_t)(((int32_t)b[i] - (int32_t)b[i - pp]) / 2);
    {   /* no DC in it: the loop keeps 0 Hz forever */
        int32_t sum = 0, mean;
        for (i = 0; i < n; i++)
            sum += b[i];
        mean = sum / n;
        for (i = 0; i < n; i++) {
            b[i] = (int16_t)(b[i] - mean);
            amp = fm_maxf(amp, fm_fabsf((float)b[i]));
        }
    }
    v->amp = vel * 0.5f * 16000.0f / fm_maxf(amp, 2000.0f) * (1.0f / 32768.0f) * 1.6f;
    v->w = (float)n;                                      /* the line full: its oldest sample the first written */
    for (i = n; i < PK_N; i++)
        b[i] = 0;
}
static void pk_off(float *st, const float *p)
{
    (void)p;
    ((pk_v *)st)->held = 0.0f;
}
static int pk_render(float *st, float *l, float *r, int n, const float *p)
{
    pk_v *v = (pk_v *)st;
    int slot = (int)v->slot, j;
    int16_t *b = pk_buf[slot];
    uint32_t w = (uint32_t)v->w, nint = (uint32_t)v->nint;
    float s = v->s, g = v->held != 0.0f || dk_ctx.pedal ? v->g : fm_minf(v->g, v->g_rel), c = v->ap_c;
    float apx = v->ap_x, apy = v->ap_y, prev = v->prev, z1 = v->z1, z2 = v->z2, b0 = v->b0, b1 = v->b1, a1 = v->a1;
    float a2 = v->a2, amp = v->amp;
    float gl = v->gl, gr = v->gr, pk = 0.0f;
    (void)p;
    if ((pk_uid[slot] & 0xFFFFFFu) != (uint32_t)v->uid)  /* its line lent to a newer voice */
        return 0;
    pk_last[slot] = ++pk_clock;
    for (j = 0; j < n; j++) {
        float a = (float)b[(w - nint) & (PK_N - 1)], x, y;
        x = (1.0f - s) * a + s * prev;                     /* the string's loss: the top first */
        prev = a;
        y = c * (x - apy) + apx;                           /* the fraction of a sample (its exact pitch) */
        apx = x;
        apy = y;
        y *= g;
        b[w & (PK_N - 1)] = (int16_t)fm_clampf(y, -32767.0f, 32767.0f);
        w++;
        x = b0 * a + z1;                                   /* the body: its top softened */
        z1 = b1 * a - a1 * x + z2;
        z2 = b0 * a - a2 * x;
        x *= amp;
        l[j] += x * gl;
        r[j] += x * gr;
        pk = fm_maxf(pk, fm_fabsf(a));
    }
    v->w = (float)(w & 0xFFFFu);
    v->ap_x = apx;
    v->ap_y = fm_flush(apy);
    v->prev = prev;
    v->z1 = fm_flush(z1);
    v->z2 = fm_flush(z2);
    v->vel = pk * amp * 2.0f;                              /* (its level now: stealing, shedding) */
    return pk * amp > 2e-4f;
}
static float pk_level(const float *st) { return ((const pk_v *)st)->vel; }
const dk_engine_t ENG_PLUCK = {"PLUCK", EF_POLY, 6, 8, 5, PK_P, pk_on, pk_off, pk_render, pk_level, 0, 0, 0, 0, R_ARP};
