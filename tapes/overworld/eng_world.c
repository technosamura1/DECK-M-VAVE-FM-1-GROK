/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* WORLD: the place's own sound, made of noise and a few sines (OVERWORLD's TEXTURE, and more): rain, wind, a fire,
 * a stream, drops in a cave, crickets, birds, the ocean, a storm far off, steps in the snow. A note starts it (it
 * fades in), letting go fades it out; HOLD keeps it. TYPE, AMOUNT (how much of it), TONE (near and bright, or far and
 * dark). Part of tape_dsp.c. */

enum { W_RAIN, W_WIND, W_FIRE, W_STREAM, W_DROPS, W_CRICKETS, W_BIRDS, W_OCEAN, W_THUNDER, W_SNOW, NW };
static const char *const WORLD_TYPES[NW] = {"RAIN", "WIND", "FIRE", "STREAM", "DROPS", "CRICKETS", "BIRDS",
                                            "OCEAN", "THUNDER", "SNOW"};
#define NPING 4
typedef struct {
    float a1l, a1r, a2l, a2r, hl, hr, br_l, br_r;
    float gust, gust_to, swell, gust_t;
    float pa[NPING], pm[NPING], pph[NPING], pinc[NPING], pdi[NPING], pgl[NPING], pnz[NPING];
    float ct[2], clen[2], cgap[2], cn[2], crest[2], cph[2], cinc[2], cdinc[2], camp[2], cgl[2];
    float fade, held, vel, kind, olp_l, olp_r, olp_l2, olp_r2, rumble, rumble_to, step_t;
} world_v;
CHECK_SIZE(world_v);
static const dk_param_t WORLD_P[] = {
    {"TYPE", 0, NW - 1, 0, VZ_TYPE, WORLD_TYPES}, {"AMOUNT", 0, 100, 50, VZ_DROP, 0}, {"TONE", 0, 100, 55, VZ_SUN, 0},
};

static void w_ping(world_v *w, float f, float amp, float tau, int noise, float fall)
{
    int i, k = 0;
    for (i = 1; i < NPING; i++)
        if (w->pa[i] < w->pa[k])
            k = i;
    w->pa[k] = amp;
    w->pm[k] = dk_decay(tau);
    w->pph[k] = 0.0f;
    w->pinc[k] = f * (1.0f / DK_SR);
    w->pdi[k] = fall;
    w->pnz[k] = (float)noise;
    w->pgl[k] = frnd();
}
static void w_chirp(world_v *w, int i, int kind)
{
    if (w->crest[i] > 0.0f) {
        w->crest[i] -= 1.0f;
        return;
    }
    if (w->cn[i] <= 0.0f) {
        float p = frnd();
        if (kind == W_CRICKETS) {
            w->cn[i] = 3.0f;
            w->clen[i] = 0.022f * DK_SR;
            w->cgap[i] = 0.014f * DK_SR;
            w->cinc[i] = (4300.0f + 600.0f * frnd()) * (1.0f / DK_SR);
            w->cdinc[i] = 0.0f;
            w->camp[i] = 0.04f + 0.03f * frnd();
        } else {
            w->cn[i] = (float)(2 + (int)(frnd() * 5.0f));
            w->clen[i] = (0.04f + 0.06f * p) * DK_SR;
            w->cgap[i] = (0.05f + 0.08f * frnd()) * DK_SR;
            w->cinc[i] = (2400.0f + 1800.0f * frnd()) * (1.0f / DK_SR);
            w->cdinc[i] = w->cinc[i] * (frnd() - 0.4f) * 0.9f / w->clen[i];
            w->camp[i] = 0.035f + 0.03f * frnd();
        }
        w->cgl[i] = 0.2f + 0.6f * frnd();
        w->ct[i] = 0.0f;
    }
}

static void world_on(float *st, int note, float vel, const float *p, int retrig)
{
    world_v *w = (world_v *)st;
    int k = (int)p[0];
    (void)note;
    if (retrig && w->kind == (float)k) {
        w->held = 1.0f;
        w->vel = vel;
        return;
    }
    w->kind = (float)(k < 0 ? 0 : k >= NW ? NW - 1 : k);
    w->held = 1.0f;
    w->vel = vel;
    w->fade = 0.0f;
    w->crest[0] = frnd() * 0.5f * DK_SR;
    w->crest[1] = frnd() * 1.5f * DK_SR;
    w->gust_to = 0.5f;
    w->rumble_to = 0.2f;
}
static void world_off(float *st, const float *p)
{
    (void)p;
    ((world_v *)st)->held = 0.0f;
}
static int world_render(float *st, float *l, float *r, int n, const float *p)
{
    world_v *w = (world_v *)st;
    int kind = (int)w->kind, j, i;
    /* each kind's own level (they are made at very different ones) */
    static const float GAIN[NW] = {1.6f, 0.85f, 1.4f, 1.1f, 0.85f, 5.0f, 7.0f, 1.0f, 4.5f, 8.0f};
    float amt = p[1] * 0.01f, lvl = (0.25f + 1.5f * amt) * w->vel * 0.8f * GAIN[kind < NW ? kind : 0],
          dens = 0.3f + 1.4f * amt, ok = dk_lpk(500.0f * fm_exp2f(p[2] * 0.042f)), fin = 1.0f / (1.4f * DK_SR),
          fout = 1.0f / (2.2f * DK_SR);
    if ((int)p[0] != kind && w->held != 0.0f)            /* TYPE turned while it sounds: the new one (a fade) */
        w->kind = fm_clampf(p[0], 0.0f, (float)(NW - 1)), kind = (int)w->kind;
    w->gust_t += (float)n;
    if (w->gust_t > 1.6f * DK_SR) {
        w->gust_t = 0.0f;
        w->gust_to = 0.1f + 0.9f * frnd();
        if ((rnd() & 7u) == 0)
            w->rumble_to = 0.2f + 0.8f * frnd();
        else
            w->rumble_to *= 0.7f;
    }
    w->gust += (w->gust_to - w->gust) * (float)n * (0.6f / DK_SR);
    w->rumble += (w->rumble_to - w->rumble) * (float)n * (0.25f / DK_SR);
    w->swell += (float)n * (1.0f / ((kind == W_OCEAN ? 10.5f : 8.5f) * DK_SR));
    if (w->swell >= 1.0f)
        w->swell -= 1.0f;
    for (j = 0; j < n; j++) {
        float L = 0.0f, R = 0.0f, nl = wnoise(), nr = wnoise();
        if (w->held != 0.0f)
            w->fade = fm_minf(1.0f, w->fade + fin);
        else
            w->fade = fm_maxf(0.0f, w->fade - fout);
        switch (kind) {
        case W_RAIN:
            w->hl += 0.08f * (nl - w->hl);
            w->hr += 0.08f * (nr - w->hr);
            w->a1l += 0.5f * (nl - w->hl - w->a1l);
            w->a1r += 0.5f * (nr - w->hr - w->a1r);
            L = w->a1l * 0.06f;
            R = w->a1r * 0.06f;
            if ((rnd() & 4095u) < (uint32_t)(2.0f * dens))
                w_ping(w, 1600.0f + 2600.0f * frnd(), 0.04f + 0.08f * frnd(), 0.004f + 0.006f * frnd(), 0, 0.0f);
            break;
        case W_WIND:
        case W_BIRDS:
        case W_SNOW: {
            float k = 0.004f + 0.02f * w->gust, g = (kind == W_WIND ? 0.13f : 0.03f) * (0.3f + 0.7f * w->gust);
            w->a1l += k * (nl - w->a1l);
            w->a2l += k * (w->a1l - w->a2l);
            w->a1r += k * (nr - w->a1r);
            w->a2r += k * (w->a1r - w->a2r);
            L = w->a2l * g * 9.0f;
            R = w->a2r * g * 9.0f;
            if (kind == W_SNOW) {                          /* steps: a crunch every half second or so */
                w->step_t += 1.0f;
                if (w->step_t > (0.62f - 0.12f * amt) * DK_SR) {
                    w->step_t = -(frnd() * 0.08f * DK_SR);
                    w->hl = 1.0f;
                }
                if (w->hl > 1e-3f) {
                    float c = (rnd() & 7u) == 0 ? wnoise() * 1.8f : 0.0f;
                    w->hr += 0.5f * (c - w->hr);
                    L += (c - w->hr) * w->hl * 0.10f;
                    R += (c - w->hr) * w->hl * 0.09f;
                    w->hl *= 0.99965f;
                }
            }
            break;
        }
        case W_OCEAN: {
            float e = 0.5f - 0.5f * dk_sinx(w->swell + 0.25f), k;
            e *= e;
            k = 0.02f + 0.15f * e;
            w->a1l += k * (nl - w->a1l);
            w->a2l += k * (w->a1l - w->a2l);
            w->a1r += k * (nr - w->a1r);
            w->a2r += k * (w->a1r - w->a2r);
            L = w->a2l * (0.08f + 0.36f * e) + (nl - w->a1l) * 0.02f * e * e;
            R = w->a2r * (0.08f + 0.36f * e) + (nr - w->a1r) * 0.02f * e * e;
            break;
        }
        case W_FIRE:
            w->br_l = 0.996f * w->br_l + 0.03f * nl;
            w->br_r = 0.996f * w->br_r + 0.03f * nr;
            L = w->br_l * 0.10f;
            R = w->br_r * 0.10f;
            if ((rnd() & 8191u) < (uint32_t)(3.0f * dens)) {
                float f = frnd();
                w_ping(w, 0.0f, 0.10f + 0.25f * f * f, 0.0012f + 0.002f * frnd(), 1, 0.0f);
            }
            break;
        case W_STREAM: {                                    /* a babble: bubbles over a bright wash */
            w->hl += 0.25f * (nl - w->hl);
            w->hr += 0.25f * (nr - w->hr);
            w->a1l += 0.6f * (nl - w->hl - w->a1l);
            w->a1r += 0.6f * (nr - w->hr - w->a1r);
            L = w->a1l * 0.035f * (0.7f + 0.3f * w->gust);
            R = w->a1r * 0.035f * (0.7f + 0.3f * w->gust);
            if ((rnd() & 1023u) < (uint32_t)(3.0f * dens))
                w_ping(w, 500.0f + 1300.0f * frnd(), 0.03f + 0.04f * frnd(), 0.010f + 0.012f * frnd(), 0,
                       1.0f + 0.0004f * frnd());
            break;
        }
        case W_DROPS:                                       /* a cave: its hum, a drop now and then, falling */
            w->br_l = 0.9985f * w->br_l + 0.02f * nl;
            w->br_r = 0.9985f * w->br_r + 0.02f * nr;
            L = w->br_l * 0.05f;
            R = w->br_r * 0.05f;
            if ((rnd() & 32767u) < (uint32_t)(6.0f * dens))
                w_ping(w, 900.0f + 1500.0f * frnd(), 0.06f + 0.08f * frnd(), 0.05f + 0.10f * frnd(), 0,
                       1.0f - 0.00006f);
            break;
        case W_THUNDER: {                                   /* far off: a slow roll under a light rain */
            float k = 0.0015f + 0.004f * w->rumble;
            w->br_l += k * (nl - w->br_l);
            w->br_r += k * (nr - w->br_r);
            w->a2l += k * (w->br_l - w->a2l);
            w->a2r += k * (w->br_r - w->a2r);
            w->hl += 0.08f * (nl - w->hl);
            w->hr += 0.08f * (nr - w->hr);
            w->a1l += 0.5f * (nl - w->hl - w->a1l);
            w->a1r += 0.5f * (nr - w->hr - w->a1r);
            L = w->a2l * 2.6f * w->rumble * w->rumble + w->a1l * 0.03f;
            R = w->a2r * 2.6f * w->rumble * w->rumble + w->a1r * 0.03f;
            break;
        }
        case W_CRICKETS:
            w->a1l += 0.02f * (nl - w->a1l);
            w->a1r += 0.02f * (nr - w->a1r);
            L = w->a1l * 0.06f;
            R = w->a1r * 0.06f;
            break;
        default: break;
        }
        for (i = 0; i < NPING; i++) {
            float x;
            if (w->pa[i] < 1e-4f)
                continue;
            if (w->pnz[i] != 0.0f)
                x = wnoise() * w->pa[i];
            else {
                if (w->pdi[i] != 0.0f)
                    w->pinc[i] *= w->pdi[i];
                w->pph[i] = dk_wrap1(w->pph[i] + w->pinc[i]);
                x = dk_sinx(w->pph[i]) * w->pa[i];
            }
            w->pa[i] *= w->pm[i];
            L += x * w->pgl[i];
            R += x * (1.0f - w->pgl[i]);
        }
        if (kind == W_CRICKETS || kind == W_BIRDS)
            for (i = 0; i < (kind == W_CRICKETS ? 2 : 1); i++) {
                w_chirp(w, i, kind);
                if (w->crest[i] > 0.0f || w->cn[i] <= 0.0f)
                    continue;
                if (w->ct[i] < w->clen[i]) {
                    float e = dk_sinx(w->ct[i] / w->clen[i] * 0.5f), x;
                    w->cph[i] = dk_wrap1(w->cph[i] + w->cinc[i] + w->cdinc[i] * w->ct[i]);
                    x = dk_sinx(w->cph[i]) * e * w->camp[i] * (0.6f + 0.6f * amt);
                    L += x * w->cgl[i];
                    R += x * (1.0f - w->cgl[i]);
                }
                w->ct[i] += 1.0f;
                if (w->ct[i] >= w->clen[i] + w->cgap[i]) {
                    w->ct[i] = 0.0f;
                    w->cn[i] -= 1.0f;
                    if (w->cn[i] <= 0.0f)
                        w->crest[i] = kind == W_CRICKETS ? (0.55f + 0.5f * frnd()) * DK_SR / dens
                                                         : (2.0f + 5.0f * frnd()) * DK_SR / dens;
                }
            }
        w->olp_l += ok * (L - w->olp_l);
        w->olp_r += ok * (R - w->olp_r);
        w->olp_l2 += ok * (w->olp_l - w->olp_l2);
        w->olp_r2 += ok * (w->olp_r - w->olp_r2);
        l[j] += w->olp_l2 * lvl * w->fade;
        r[j] += w->olp_r2 * lvl * w->fade;
    }
    return !(w->held == 0.0f && w->fade <= 0.0f);
}
static float world_level(const float *st)
{
    const world_v *w = (const world_v *)st;
    return w->fade * w->vel * 0.3f;
}
const dk_engine_t ENG_WORLD = {"WORLD", EF_CONT | EF_MONO, 1, 17, 3, WORLD_P, world_on, world_off, world_render,
                               world_level, 0, 0, 0, 0, R_NATURE};
