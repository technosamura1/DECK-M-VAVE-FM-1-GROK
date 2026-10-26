/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The follower's PLL, pure integer code with no dependency on the sequencer (so a test can feed it jittered
 * arrivals directly). Times are in samples (44100/s), a clock period in samples per MIDI clock (24 per quarter),
 * Q8. What it does:
 *   - tempo: a least-squares line through the arrival times of the last SY_N (96, one bar) clocks. That slope is a
 *     weighted moving average of the clock intervals, and with few clocks (a start) it is the plain average of
 *     them; the jitter of a link (the arrivals are only ever late, by up to tens of ms) hardly moves it;
 *   - phase: the caller measures how far its oscillator is from the clock that just arrived (in ticks Q8), and
 *     sync_phase() gives back the part of it to take: the median of the last seven errors (a burst of two or three
 *     clocks delivered at once is outvoted), times 1/16 (1/2 then 1/4 for the first 24 clocks of a lock, while the tempo is still being found);
 *   - silence: after SY_HOLD samples (250 ms) without a clock the clock is "not alive": the caller keeps the last
 *     tempo and stops pulling the phase. A clock after a gap starts a new window but keeps the held tempo until it
 *     has a beat's worth of clocks again. */
#pragma once
#include <stdint.h>

#define SY_N 96
#define SY_E 7                         /* phase errors kept: the median of them is taken (a burst of three is outvoted) */
#define SY_HOLD 11025u                 /* 250 ms at 44100 */
#define SY_MIN_PERIOD_Q8 (275u * 256u)   /* 400 BPM */
#define SY_MAX_PERIOD_Q8 (5512u * 256u)  /* 20 BPM */

typedef struct {
    uint32_t t[SY_N];                  /* arrival times of the last clocks (a ring) */
    uint32_t last;                     /* the last clock's arrival */
    uint32_t count;                    /* clocks since the window was started */
    uint32_t out_q8;                   /* the tempo given out: samples per clock, Q8 (0: none yet) */
    int32_t err[SY_E];                 /* the last phase errors, Q8 ticks */
    uint8_t n_err, fast;               /* how many are valid; clocks left of the fast gain */
    uint8_t held;                      /* the tempo is a held one (after a silence), a new one not yet fitted */
} sync_t;

void sync_reset(sync_t *s, int keep_tempo);
void sync_clock(sync_t *s, uint32_t t);                 /* a clock arrived at t */
int sync_alive(const sync_t *s, uint32_t now);          /* 1: a clock within SY_HOLD of now (and one at all) */
uint32_t sync_period_q8(const sync_t *s);               /* samples per clock, Q8 (0: not known yet) */
void sync_lock(sync_t *s);                              /* a new lock: the fast gain for a few clocks, errors forgotten */
int32_t sync_phase(sync_t *s, int32_t err_q8);          /* the correction to take now, Q8 ticks */
