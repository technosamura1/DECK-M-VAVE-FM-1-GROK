/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The follower's PLL (sync.h). */
#include "sync.h"

void sync_reset(sync_t *s, int keep_tempo)
{
    uint32_t keep = keep_tempo ? s->out_q8 : 0u;
    uint32_t i;
    s->count = 0;
    s->last = 0;
    s->n_err = 0;
    s->fast = 0;
    for (i = 0; i < SY_E; i++)
        s->err[i] = 0;
    s->out_q8 = keep;
    s->held = keep ? 1 : 0;
}

/* the slope of arrival time against clock number over the last m clocks (Q8 samples per clock), twice: a line through
 * all of them, then through the half that lies nearest to or below it. A link only ever delays (jitter, a burst of
 * clocks delivered together), so the early half is the one that is true, and the late outliers are left out. */
static int32_t line(const sync_t *s, uint32_t m, uint32_t first, const int32_t *res, int32_t cut, int32_t *icpt)
{
    /* All in 32 bits (the device has no 64-bit divide, and its 64-bit multiplies are best left out): the sums are taken
     * around a rounded mean, so they stay small; the exact mean is put back as a correction. */
    int32_t n = 0, sx = 0, sy = 0, x0, y0, dx = 0, dy = 0, sxy = 0, sxx = 0, num, den, p, q, r;
    uint32_t i, t0 = s->t[first % SY_N];
    int32_t y[SY_N];
    for (i = 0; i < m; i++) {
        y[i] = (int32_t)(s->t[(first + i) % SY_N] - t0);
        if (res && res[i] > cut)
            continue;
        n++;
        sx += (int32_t)i;
        sy += y[i];
    }
    if (n < 2)
        return 0;
    x0 = (sx + n / 2) / n;
    y0 = (sy + n / 2) / n;
    for (i = 0; i < m; i++) {
        int32_t xc, yc;
        if (res && res[i] > cut)
            continue;
        xc = (int32_t)i - x0;
        yc = y[i] - y0;
        dx += xc;
        dy += yc;
        sxy += xc * yc;
        sxx += xc * xc;
    }
    num = sxy - dx * dy / n;
    den = sxx - dx * dx / n;
    if (den <= 0 || num <= 0)
        return 0;
    q = num / den;
    r = num % den;
    p = q * 256 + (r * 256 + den / 2) / den;                 /* Q8 samples per clock (r*256 fits: den is small) */
    *icpt = y0 * 256 + dy * 256 / n - p * x0 - (int32_t)((int32_t)p * dx / n);
    return p;
}
static uint32_t fit(const sync_t *s, uint32_t m)
{
    uint32_t i, j, first = s->count - m;
    uint32_t t0 = s->t[first % SY_N];
    int32_t res[SY_N], srt[SY_N], cut;
    int32_t b = 0, p = line(s, m, first, 0, 0, &b);
    if (p <= 0)
        return 0;
    for (i = 0; i < m; i++) {
        res[i] = (int32_t)(s->t[(first + i) % SY_N] - t0) * 256 - b - p * (int32_t)i;
        srt[i] = res[i];
    }
    for (i = 1; i < m; i++) {                                /* (insertion sort, 96 at most) */
        int32_t v = srt[i];
        for (j = i; j > 0 && srt[j - 1] > v; j--)
            srt[j] = srt[j - 1];
        srt[j] = v;
    }
    cut = srt[m / 2];
    {
        int32_t q = line(s, m, first, res, cut, &b);
        if (q > 0)
            p = q;
    }
    return (uint32_t)p;
}

void sync_clock(sync_t *s, uint32_t t)
{
    uint32_t m, p;
    if (s->count && (int32_t)(t - s->last) > (int32_t)SY_HOLD)   /* after a silence: a new window, the held tempo kept */
        sync_reset(s, 1);
    s->t[s->count % SY_N] = t;
    s->last = t;
    s->count++;
    m = s->count < SY_N ? s->count : SY_N;
    if (m < 12)
        return;
    if (m < 24 && s->held)                                /* (a held tempo stays until a beat of new clocks) */
        return;
    p = fit(s, m);
    if (p < SY_MIN_PERIOD_Q8 || p > SY_MAX_PERIOD_Q8)
        return;
    if (!s->out_q8 || m < 24 || s->held)
        s->out_q8 = p;
    else
        s->out_q8 = (uint32_t)((int32_t)s->out_q8 + ((int32_t)p - (int32_t)s->out_q8) / 4);
    s->held = 0;
}

int sync_alive(const sync_t *s, uint32_t now) { return s->count && (int32_t)(now - s->last) <= (int32_t)SY_HOLD; }
uint32_t sync_period_q8(const sync_t *s) { return s->out_q8; }

void sync_lock(sync_t *s)
{
    uint32_t i;
    s->n_err = 0;
    s->fast = 24;
    for (i = 0; i < SY_E; i++)
        s->err[i] = 0;
}

static int32_t median7(const int32_t *e, int n)
{
    int32_t a[SY_E], t;
    int i, j;
    for (i = 0; i < n; i++)
        a[i] = e[i];
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && a[j - 1] > a[j]; j--)
            t = a[j], a[j] = a[j - 1], a[j - 1] = t;
    return a[n / 2];
}

int32_t sync_phase(sync_t *s, int32_t err_q8)
{
    int32_t m, c;
    int i;
    for (i = SY_E - 1; i > 0; i--)
        s->err[i] = s->err[i - 1];
    s->err[0] = err_q8;
    if (s->n_err < SY_E)
        s->n_err++;
    m = median7(s->err, s->n_err >= 3 ? s->n_err : 1);
    if (s->fast) {                                          /* (the tempo is still being found: pull hard, then less) */
        c = s->fast > 12 ? m / 2 : m / 4;
        s->fast--;
    } else {
        c = m / 16;
    }
    if (c > 256)
        c = 256;
    if (c < -256)
        c = -256;
    return c;
}
