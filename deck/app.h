/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK on fm1-base: the app's configuration and its half of the contract with fm1-base (device/unity.c,
 * host/plat_host.c). The sound is the core (core.c) and the tape (tapes/NAME, dsp_unit.c), both -O2 units; the UI,
 * the song's edits and the store are this unity build (app.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "song.h"

#define APP_NAME "DECK"
#define APP_TAGLINE "a deck for tapes"
#define APP_SPLASH_RGB RGB(232, 232, 232)
#define APP_PALETTE 4u
#define APP_BLOCK 256u
#define APP_TOUCH                        /* the keys' velocity */
#define APP_SPLASH
void app_splash(void);
#define APP_OVERLOAD
void app_overload(void);
#define APP_SYSEX
int app_sysex(const uint8_t *p, uint32_t n);
/* the canvas: a band of 240 x 80 (pages are drawn in three bands; the garden in two) */
#define APP_CANVAS_PX (240u * 80u)
#ifdef APP_VERSION
#define DK_VERSION APP_VERSION
#else
#define DK_VERSION "DEV"
#endif

/* the store: the settings (metronome, MIDI, the project open) at 0xFC000 (Felucca's settings pair), the user's
 * presets at 0xD0000, the panel calibration at 0xDC000; the projects in the data area, DK_PROJ_SPAN each from
 * DK_PROJ_AT */
enum { OBJ_SET, OBJ_USER, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xD0000u, 0xDC000u}
#define DK_PROJ_AT 0xA0000u
#define DK_PROJ_SPAN 0x9000u              /* 36 KiB: the song and 4096 events */
#define DK_NPROJ 4
#define SET_MAGIC 0x54455344u            /* "DSET" */
#define USER_MAGIC 0x52535544u           /* "DUSR" */

typedef struct {
    uint32_t magic;
    uint8_t proj;                        /* the project open */
    uint8_t metro[6];                    /* dk_metro_t */
    uint8_t midi_notes, midi_clock, midi_in;
    uint8_t lat;                         /* REC LAT: the audio latency made up while recording, ms + 1 (0: the default, 9) */
    uint8_t noarm;                       /* REC ARM off (inverted: 0 is on) */
    uint8_t nolock;                      /* LOCK TAKE off (inverted: 0, as in an older store, is on) */
    uint8_t rsv[17];
} settings_t;
extern settings_t set;

int project_load(int slot);              /* 0 = loaded; else an empty song */
int project_save(int slot);              /* 0 = saved and verified */
int project_used(int slot);              /* it holds a song (its name in name, 12 bytes) */
const char *project_name(int slot, char *name);
void settings_save(void);
void user_save(void);

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
void ui_say(const char *a, const char *b);
void ui_arm_reset(void);              /* REC ARM changed: the tape is not armed any more */

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's part (host.c) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_REBOOT
void app_host_reboot(void);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
