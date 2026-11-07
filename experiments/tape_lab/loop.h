/* SPDX-License-Identifier: MPL-2.0 */
#pragma once
#include <stdint.h>
enum { TL_EMPTY, TL_RECORD, TL_PLAY, TL_OVERDUB, TL_STOP };
enum { TL_REC=1, TL_TOGGLE, TL_UNDO, TL_CLEAR };
typedef struct {
    int16_t *audio, *undo;
    uint32_t capacity, length, pos, pass, wraps, clipped;
    int state, armed, undo_valid, deferred;
    float gain, correction, last;
} tape_loop;
void tl_init(tape_loop *t, int16_t *audio, int16_t *undo, uint32_t capacity);
void tl_command(tape_loop *t, int command);
float tl_sample(tape_loop *t, float input);
