/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* The PLAY picture: a girl in four greys (the user's own drawing, tools/make_girl.py -> girl_img.h, ~4.7 KB of runs).
 * Her face stays; the hair round it moves sideways, a wave running down it, more at its ends. Each row is decoded
 * from its runs and drawn shifted, so nothing but one row lives in RAM. Part of tape_ui.c. */
#include "girl_img.h"

#define GIRL_MS 5200u                           /* one sway, there and back */
static uint8_t girl_line[240];
void ui_mid_poll(void);                         /* (the keys read between the halves: ui.c) */
/* the hair's weight by the squared distance from the face's ellipse, d2 in 1 .. 2.1025 (past it: 1): t^1.5,
 * t = (sqrt(d2) - 1) / 0.45 -- a table of 64, so no root a pixel */
#define GW_N 64
static float girl_w[GW_N + 1];
static void girl_wtab(void)
{
    int i;
    for (i = 0; i <= GW_N; i++) {
        float d2 = 1.0f + 1.1025f * (float)i / (float)GW_N, t = fm_clampf((fm_sqrtf(d2) - 1.0f) * (1.0f / 0.45f), 0.0f, 1.0f);
        girl_w[i] = t * fm_sqrtf(t);
    }
}

static void girl_row(int y)
{
    const uint8_t *p = &GIRL_RLE[GIRL_ROW[y]];
    int x = 0;
    while (x < 240) {
        int v = *p >> 6, n = (*p & 63) + 1;
        p++;
        while (n-- && x < 240)
            girl_line[x++] = (uint8_t)v;
    }
}

/* rows y0 .. y0 + 143: two bands of 72 */
static void girl_draw(int32_t y0, uint32_t now)
{
    static const uint16_t PAL4[4] = {RGB(0, 0, 0), RGB(86, 86, 92), RGB(172, 172, 178), RGB(244, 244, 240)};
    /* the face in the source picture (crop 60 130 620): its centre and half-axes, scaled to the screen's pixels */
    const float k = 620.0f / 240.0f, fcx = (372.0f - 60.0f) / k, fcy = (352.0f - 130.0f) / k, frx = 150.0f / k, fry = 178.0f / k;
    float ph = (float)(now % GIRL_MS) * (6.2831853f / (float)GIRL_MS);
    uint16_t pal[4];
    int half, x, y, i;
    for (i = 0; i < 4; i++)
        pal[i] = swap16(PAL4[i]);
    if (girl_w[GW_N] == 0.0f)
        girl_wtab();
    for (half = 0; half < 2; half++) {
        cv_begin(240, 72, 0);
        for (y = 0; y < 72; y++) {
            int Y = y + half * 72;
            float sy = 130.0f + (float)Y * k, dy = ((float)Y - fcy) / fry;
            float wave = (7.0f * fm_sinf(ph + sy / 70.0f) + 2.0f * fm_sinf(2.0f * ph + sy / 31.0f)) / k;   /* screen pixels */
            float down = fm_clampf(sy / 300.0f, 0.15f, 1.0f);
            uint16_t *row = &cv_px[(uint32_t)y * 240u];
            girl_row(Y);
            for (x = 0; x < 240; x++) {
                float dx = ((float)x - fcx) / frx, d2 = dx * dx + dy * dy, w = 0.0f;
                int sx = x;
                if (d2 > 1.0f) {                /* outside the face: the hair, moving */
                    float f = (d2 - 1.0f) * ((float)GW_N / 1.1025f);
                    w = (f >= (float)GW_N ? 1.0f : girl_w[(int)f]) * down;
                    sx = x - (int)(w * wave + (w * wave >= 0.0f ? 0.5f : -0.5f));
                    sx = sx < 0 ? 0 : sx > 239 ? 239 : sx;
                }
                row[x] = pal[girl_line[sx]];
            }
        }
        garden_overlay(y0 + half * 72, 72);
        cv_blit(0, (uint32_t)(y0 + half * 72));
        ui_mid_poll();
    }
}
