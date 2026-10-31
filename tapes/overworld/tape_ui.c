/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* OVERWORLD's face for DECK (the UI's unity build, from DECK's app.c): a manga page in black and white. Ink, paper
 * and screentone greys for the plates, frames and windows; the colours that mark things (knobs, tracks, parts,
 * recording) stay. The PLAY page's art is the girl (girl.c), her hair moving; the splash is her too. */

/* ------------------------------------------------------------------- theme --- */
#define TH_BG RGB(0, 0, 0)
#define TH_TEXT RGB(240, 240, 236)
#define TH_GRAY RGB(132, 132, 132)
#define TH_LINE RGB(54, 54, 54)
#define TH_REC RGB(232, 64, 56)
static const uint16_t TH_ENC[3] = {RGB(124, 204, 96), RGB(110, 186, 244), RGB(246, 214, 92)};
/* the parts' colours (A..H) and the tracks' (kept: colour is for finding things) */
static const uint16_t TH_PART[8] = {RGB(124, 204, 96), RGB(110, 186, 244), RGB(246, 214, 92), RGB(244, 170, 200),
                                    RGB(222, 98, 80),  RGB(170, 140, 240), RGB(150, 214, 170), RGB(250, 160, 90)};
static const uint16_t TH_TRACK[DK_NTRACK] = {RGB(150, 214, 170), RGB(222, 98, 80), RGB(244, 170, 200),
                                             RGB(170, 140, 240), RGB(246, 214, 92), RGB(124, 204, 96)};
/* the cassette (the REC page), and the plate every page is: paper, ink, a black window */
#define CS_BODY RGB(236, 236, 232)
#define CS_BODY_D RGB(150, 150, 148)
#define CS_LABEL RGB(250, 250, 248)
#define CS_INK RGB(16, 16, 16)
#define CS_WINDOW RGB(0, 0, 0)
#define CS_TAPE RGB(70, 70, 70)
static const uint16_t CS_STRIPE[3] = {RGB(124, 204, 96), RGB(246, 214, 92), RGB(244, 170, 200)};
#define TAPE_TITLE "OVERWORLD"

static void garden_overlay(int32_t y0, int32_t h);   /* the shell's (pages.c): what goes over the art */
static int biome(void) { return dk_song.biome % 9; }
#include "girl.c"

/* ------------------------------------------------------------ the shell's joins --- */
static void tape_ui_init(void) {}
static void tape_ui_keep(void) {}
static void tape_ui_step(uint32_t dt) { (void)dt; }
/* the PLAY page's art: the girl, rows y0 .. y0 + 143 */
static void tape_play_draw(int32_t y0) { girl_draw(y0, plat_ms()); }
static void tape_mood(int m) { (void)m; }

/* the splash: the girl, the name over her in a black caption box, the way a manga's first page opens */
void app_splash(void)
{
    lcd_fill(0, 0, 240, 240, TH_BG);
    girl_draw(48, 0);
    cv_begin(240, 40, TH_BG);
    cv_text((240 - text_w(&FONT_L, "DECK")) / 2, 4, &FONT_L, "DECK", TH_TEXT);
    cv_blit(0, 6);
    cv_begin(240, 40, TH_BG);
    cv_text((240 - text_w(&FONT_S, TAPE_TITLE)) / 2, 4, &FONT_S, TAPE_TITLE, TH_GRAY);
    cv_blit(0, 196);
}
