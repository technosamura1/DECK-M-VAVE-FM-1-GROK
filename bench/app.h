/* SPDX-License-Identifier: MPL-2.0 */
/* BENCH: a measuring firmware for the FM-1 (JieLi AC791N): what a DX7-style 6-operator FM engine costs on CPU0,
 * integer against float, and whether CPU1 (the second core) runs and how much it adds. It plays no sound.
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "BENCH"
#define APP_SPLASH_RGB RGB(80, 220, 120)
#define APP_BLOCK 256u                   /* frames per audio block (the platform's; the bench renders silence) */
#define APP_SYSEX                        /* the PC tool (tools/deck_bench.py): SysEx F0 7D 44 4B 53 <cmd> F7 */
int app_sysex(const uint8_t *p, uint32_t n);

/* the store: only the panel calibration (the platform's) is kept; the bench saves nothing of its own */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's checks (host/plat_host.c "expect"): st, rows, ok0, ok1, c1st, sysex */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
