/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK on fm1-base: the app's unity build (compiled into fm1-base's, after the platform): the song's edits, the store,
 * the export, the keys, the fields, the panel's map, the tape's face (tapes/NAME/tape_ui.c), the UI and its pages;
 * and the glue to the sound (core.c and the tape's dsp_unit.c, their own -O2 units). */
#include "song.c"
#include "store.c"
#include "export.c"
#include "keys.c"
#include "fields.c"
#include "ui_map.c"
#include "tape_ui.c"
#include "ui.c"

/* the sound's MIDI out: to USB (status, data 1, data 2 -> a USB-MIDI packet) */
void dk_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

void app_init(void)
{
    dk_init();
    settings_load();
    user_load();
    if (project_load(set.proj) != 0) {   /* nothing saved yet: the tape's demo, to play at once */
        dk_lock();
        TAPE.demo();
        dk_unlock();
    }
    dk_cmd(C_SEED, 0, 0, (int)(plat_ms() * 2654435761u + 12345u));
    dk_cmd(C_RETUNE, 0, 0, 0);
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { dk_render(lr, n, gain_q12); }
void app_panic(void)
{
    dk_cmd(C_PLAY, 0, 0, 0);
    dk_cmd(C_PANIC, 0, 0, 0);
}
void app_overload(void) { dk_shed(); }
void app_ui_init(void) { ui_init(); }
void app_frame(void) { ui_frame(); }
void app_input_only(void) { ui_input_only(); }
void app_say(const char *a, const char *b) { ui_say(a, b); }

#ifdef PLAT_HOST
#include "host.c"
#endif
