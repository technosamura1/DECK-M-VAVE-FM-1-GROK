/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DREAMWALL / DECK contributors */
/* DREAMWALL's WALL: a shoegaze guitar wall without the guitar. Each note is a few saws pulled apart (left and right),
 * an octave up for some, a breath of noise, through a soft low-pass, swelling in like a bowed string and holding for as
 * long as the key does (a held chord is a drone); its pitch wanders slowly, as a guitar's under the tremolo arm.
 * The fuzz is the track's (the bus): the whole chord through it together, so the notes grind into each other the way
 * a fuzz pedal's do, then a speaker's soft top. Part of tape_dsp.c. */

typedef struct {
    float ph[4], oph, inc, det, vel, wl, wr, es, att, att_inc, mr, held;
    float l1, l2, r1, r2, lpk, gph, ginc, gdep, nz, air, oct, kind;
} wall_v;
CHECK_SIZE(wall_v);
static const char *const WALL_TYPES[] = {"WALL", "VEIL", "BOW", "OCTAVE"};
static const dk_param_t WALL_P[] = {
    {"TONE", 0, 100, 55, VZ_SUN, 0},   {"FUZZ", 0, 100, 45, VZ_DUST, 0},  {"DETUNE", 0, 100, 45, VZ_BAR, 0},
    {"TYPE", 0, 3, 0, VZ_TYPE, WALL_TYPES}, {"GLIDE", 0, 100, 30, VZ_WOBBLE, 0}, {"SWELL", 0, 100, 40, VZ_SWELL, 0},
    {"AIR", 0, 100, 20, VZ_WAVE, 0},   {"SPREAD", 0, 100, 70, VZ_BAR, 0},
};
static void wall_on(float *st, int note, float vel, const float *p, int retrig)
{
    wall_v *v = (wall_v *)st;
    int k = (int)p[3], i;
    float sw = p[5] * 0.01f, kt = fm_exp2f((float)(note - 60) * (1.0f / 48.0f)), width = p[7] * 0.01f;
    (void)retrig;
    k = k < 0 ? 0 : k > 3 ? 3 : k;
    v->kind = (float)k;
    v->vel = vel;
    v->inc = dk_mtof((float)note) * drift_of(p) * (1.0f / DK_SR);
    v->det = 1.0f + (0.002f + p[2] * 0.0001f) * (k == 2 ? 0.5f : 1.0f);   /* up to ~20 cents apart */
    for (i = 0; i < 4; i++)
        v->ph[i] = frnd();
    v->oph = frnd();
    v->oct = k == 3 ? 0.55f : k == 1 ? 0.0f : 0.12f;
    /* the saws 0 and 2 lean left, 1 and 3 right, as far as SPREAD; a note's place in the keyboard a little too */
    v->wl = 0.5f + 0.45f * width;
    v->wr = 0.5f - 0.45f * width;
    v->lpk = dk_lpk(fm_minf(500.0f * fm_exp2f(p[0] * 0.042f) * kt, 12000.0f));
    v->att_inc = 1.0f / (env_att(p, 0.03f + sw * sw * (k == 2 ? 4.0f : 2.6f)) * DK_SR);
    v->att = 0.0f;
    v->es = 0.0f;
    v->mr = dk_decay(1.4f * env_rel(p));
    v->ginc = (0.13f + 0.22f * frnd()) / DK_SR;
    v->gph = frnd();
    v->gdep = p[4] * 0.00016f;                    /* up to ~27 cents: the arm pushed, let go */
    v->air = p[6] * 0.01f;
    v->nz = 0.0f;
    v->l1 = v->l2 = v->r1 = v->r2 = 0.0f;
    v->held = 1.0f;
}
static void wall_off(float *st, const float *p)
{
    (void)p;
    ((wall_v *)st)->held = 0.0f;
}
static int wall_render(float *st, float *l, float *r, int n, const float *p)
{
    wall_v *v = (wall_v *)st;
    int k = (int)v->kind, j;
    float g, w = dk_ctx.wow, glide, d0, d1, d2, d3, od, gsaw = k == 1 ? 0.30f : 0.24f, air = v->air * 0.5f, oct = v->oct;
    float lpk = v->lpk, wl = v->wl, wr = v->wr;
    (void)p;
    v->gph += v->ginc * (float)n;                 /* the arm: slow, once a block */
    if (v->gph >= 1.0f)
        v->gph -= 1.0f;
    glide = 1.0f + v->gdep * (dk_sinx(v->gph) + 0.4f * dk_sinx(v->gph * 2.3f + 0.25f));
    d0 = v->inc * w * glide;
    d1 = d0 * v->det;
    d2 = d0 * (2.0f - v->det);
    d3 = d0 * (1.0f + (v->det - 1.0f) * 2.1f);
    od = d0 * 2.0f * (2.0f - v->det) ;
    g = v->vel * 0.17f;
    for (j = 0; j < n; j++) {
        float a, b, s, x;
        if (v->held != 0.0f) {
            if (v->att < 1.0f) {
                v->att = fm_minf(1.0f, v->att + v->att_inc);
                v->es = v->att * v->att;              /* a bow's swell: slow, then all at once */
            } else
                v->es = 1.0f;                         /* held: it does not fade (a drone) */
        } else
            v->es *= v->mr;
        v->ph[0] = dk_wrap1(v->ph[0] + d0);
        v->ph[1] = dk_wrap1(v->ph[1] + d1);
        a = dk_blep(v->ph[0], d0);
        b = dk_blep(v->ph[1], d1);
        if (k != 2 && k != 1) {                       /* WALL, OCTAVE: four saws */
            v->ph[2] = dk_wrap1(v->ph[2] + d2);
            v->ph[3] = dk_wrap1(v->ph[3] + d3);
            a += dk_blep(v->ph[2], d2);
            b += dk_blep(v->ph[3], d3);
        } else if (k == 1) {                          /* VEIL: two saws, a sine under them */
            v->ph[2] = dk_wrap1(v->ph[2] + d0 * 0.5f);
            x = dk_sinx(v->ph[2] + 4.0f) * 0.9f;
            a += x;
            b += x;
        }
        if (oct > 0.0f) {                             /* the octave up (the high strings ringing) */
            v->oph = dk_wrap1(v->oph + od);
            x = dk_blep(v->oph, od) * oct;
            a += x;
            b += x;
        }
        if (air > 0.0f) {                             /* the pick's breath, the amp's hiss in it */
            v->nz += 0.25f * (wnoise() - v->nz);
            a += v->nz * air;
            b -= v->nz * air;
        }
        s = v->es * g * gsaw;
        v->l1 += lpk * (a * s - v->l1);
        v->l2 += lpk * (v->l1 - v->l2);
        v->r1 += lpk * (b * s - v->r1);
        v->r2 += lpk * (v->r1 - v->r2);
        l[j] += v->l2 * wl + v->r2 * (1.0f - wl);
        r[j] += v->r2 * (1.0f - wr) + v->l2 * wr;
    }
    return !(v->held == 0.0f && v->es < 1e-3f);
}
static float wall_level(const float *st)
{
    const wall_v *v = (const wall_v *)st;
    return v->es * v->vel * 0.6f;
}
/* the track's fuzz: drive with FUZZ, leaning one way (the even harmonics), a speaker's top, the DC out.
 * bs: 0..3 the low-passes, 4..7 the DC blockers */
static void wall_bus(float *bs, float *l, float *r, int n, const float *p)
{
    float f = p[1] * 0.01f, drive, bias, mk, k, tb;
    int j;
    if (f <= 0.005f)
        return;
    drive = 1.0f + f * f * 16.0f;
    bias = 0.18f * f;
    tb = fm_tanhf(bias);
    mk = 1.0f / (0.6f + 0.4f * drive) * (1.0f + 0.9f * f);
    k = dk_lpk(6200.0f - 2600.0f * f);
    for (j = 0; j < n; j++) {
        float a = (fm_tanhf(l[j] * drive + bias) - tb) * mk, b = (fm_tanhf(r[j] * drive + bias) - tb) * mk, y;
        bs[0] += k * (a - bs[0]);
        bs[1] += k * (bs[0] - bs[1]);
        bs[2] += k * (b - bs[2]);
        bs[3] += k * (bs[2] - bs[3]);
        y = bs[1] - bs[4] + 0.9965f * bs[5];          /* ~25 Hz high-pass: the bias's DC gone */
        bs[4] = bs[1];
        bs[5] = fm_flush(y);
        l[j] = y;
        y = bs[3] - bs[6] + 0.9965f * bs[7];
        bs[6] = bs[3];
        bs[7] = fm_flush(y);
        r[j] = y;
    }
}
const dk_engine_t ENG_WALL = {"WALL", EF_POLY, 6, 18, 8, WALL_P, wall_on, wall_off, wall_render, wall_level, wall_bus, 0, 0, 0,
                              R_PAD};
