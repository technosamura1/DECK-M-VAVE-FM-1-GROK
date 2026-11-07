/* SPDX-License-Identifier: MPL-2.0 */
#pragma once
#include "plat.h"
#define APP_NAME "TAPE LAB"
#define APP_SPLASH_RGB RGB(164,199,161)
#define APP_BLOCK 256u
enum { OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xDC000u}
void app_init(void);
void app_render(int32_t *,uint32_t,uint32_t);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *,const char *);
#define APP_HOST_EXPECT
int app_expect(const char *,int *);
