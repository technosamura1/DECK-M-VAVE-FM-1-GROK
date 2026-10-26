/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The transport's follower and leader, part of the core's unit (core.c, after seq.c): a MIDI clock arriving (from a
 * partner over BLE, or any port when the role is FOLLOW) sets the tempo and the phase of the sequencer's own clock
 * (sync.c, the PLL), and Start and Stop move the transport. Runs in the render, from the commands the UI queues
 * (C_XCLK, C_XSTART, C_XSTOP, each with the time it arrived, in ms).
 *
 *   LEAD    sends clock and transport to BLE, follows nothing
 *   FOLLOW  follows (the clock of any port), sends nothing
 *   AUTO    a local PLAY with no clock alive leads (sends); a clock alive (the partner's, over BLE): follows
 *
 * Following: Start (0xFA/0xFB) arms, the sequencer starts at the next 0xF8 (a restart from 0 when it is playing);
 * a local PLAY while a clock is alive waits for the next bar of the leader's (96 clocks from its Start); each
 * clock after pulls the sequencer's phase to the nearest clock (a median of seven errors, a graded part of it, at most 1 tick); 250 ms without a
 * clock: HOLD, the last tempo goes on and the phase is left alone, the next clock pulls it back. */
#include "sync.c"

volatile uint8_t dk_bt_role;           /* 0 AUTO 1 LEAD 2 FOLLOW (the UI sets it) */
uint8_t dk_bt_notes;                   /* the sequence's notes to BLE as well (default off: the partner would play them) */
volatile uint8_t dk_bt_trim;           /* ms: the link's delay made up (the follower plays this much earlier), 0..40 */
volatile uint8_t dk_fol;               /* 0 not following, 1 following, 2 HOLD (no clock for 250 ms), 3 waiting to start */
volatile uint16_t dk_fol_bpm10;        /* the clock's tempo, tenths of a BPM (0: none yet) */
volatile int16_t dk_fol_err;           /* the last phase error, in 1/256 tick (tests, the page's status) */
volatile uint32_t dk_smp;              /* samples rendered (the clock arrivals are put on this) */
volatile uint32_t dk_ms_now;           /* the app's ms clock as this render began (the app sets it before dk_render) */
volatile uint8_t dk_clk_alive;         /* a clock is arriving (within 250 ms), whoever it is from */

static sync_t sy;
static uint8_t fol_src = 0xFF;         /* the port the clock is followed from */
static uint8_t fol_on;                 /* the transport follows (Start seen, locked on) */
static uint8_t fol_arm;                /* 1 Start seen: the next clock starts; 2 a local PLAY waits for the bar */
static uint8_t fol_lead;               /* this run began here: it leads (and sends, in AUTO) */
static uint8_t fol_start_next;         /* the next clock is the first of a run (after Start) */
static uint32_t lead_n;                /* clocks since the run's first (the leader's count: its bars) */
static int32_t fol_n;                  /* the clock the oscillator's grid has reached: tick 4*fol_n is due at it (rebased when slipped) */
static float xh[5];                    /* the last five raw phase errors, ticks */
static uint8_t xn, xbad;               /* (xbad: how many clocks in a row their median was out of range) */
static uint8_t fol_role = 0xFF;        /* the role last seen (a change forgets what was followed) */

static float fol_tempo(void)           /* the clock's tempo in BPM, 0 when the sequencer is not following one */
{
    uint32_t p = sy.out_q8;
    float b;
    if (!fol_on || !p)
        return 0.0f;
    b = (60.0f * DK_SR * 256.0f / 24.0f) / (float)p;
    return (float)(int)(b * 10.0f + 0.5f) * 0.1f;           /* (tenths: the echo's and the plate's constants hardly move) */
}
static float fol_tick_len(void) { return (float)sy.out_q8 * (1.0f / (256.0f * 4.0f)); }
static int sync_tx(void) { return dk_bt_role == 1 || (dk_bt_role == 0 && fol_lead); }

/* what the song's road adds up to before block i, in sixteenths (SPP's unit), for a run that starts mid-song */
static uint32_t spp_of_start(void)
{
    uint32_t n = 0;
    int i;
    if (!dk_song.play_mode)
        return 0;
    for (i = 0; i < dk_road_i && i < dk_song.nroad; i++) {
        int r = dk_song.road[i].reps ? dk_song.road[i].reps : 1;
        n += (uint32_t)(part_len_of(dk_song.road[i].part) / DK_TPB * r * 16);
    }
    return n > 16383u ? 16383u : n;
}
static void sync_tx_start(void)        /* a run starts here: SPP and Start, or SPP and Continue from mid-song */
{
    uint32_t spp;
    if (!sync_tx())
        return;
    spp = spp_of_start();
    dk_midi_ble(0xF2, spp & 0x7F, (spp >> 7) & 0x7F);
    dk_midi_ble(spp ? 0xFB : 0xFA, 0, 0);
}
static void sync_tx_stop(void)
{
    if (sync_tx())
        dk_midi_ble(0xFC, 0, 0);
}

static uint32_t fol_stamp(int ms)      /* the sample time of an arrival, ms (the app's clock) */
{
    int32_t age = (int32_t)(dk_ms_now - (uint32_t)ms);
    if (age < 0)
        age = 0;
    if (age > 250)
        age = 250;
    return dk_smp - (uint32_t)((float)age * (DK_SR / 1000.0f)) - (uint32_t)dk_bt_trim * (uint32_t)(DK_SR / 1000.0f);
}
static int fol_accept(int port, uint32_t t)
{
    if (fol_lead || dk_bt_role == 1)
        return 0;
    if (fol_src != (uint8_t)port) {
        if (fol_src != 0xFF && sync_alive(&sy, t))
            return 0;                          /* another port's clock is alive: it stays the one */
        fol_src = (uint8_t)port;
        sync_reset(&sy, 1);
        lead_n = 0;
    }
    return 1;
}
static void fol_begin(uint32_t t)      /* the run starts, on the clock that arrived at t */
{
    float late = (float)(uint32_t)(dk_smp - t);
    fol_arm = 0;
    if (count_left)
        return;
    fol_n = 0;
    xn = xbad = 0;
    fol_on = 1;
    play_start();                              /* (fol_on: it leads nothing) */
    tick_left = -late;                         /* tick 0 was due when the clock arrived */
    sync_lock(&sy);
    dk_fol = 1;
}
static void fol_pull(uint32_t t, int rebase)       /* the phase to the clock that arrived at t */
{
    float x, err;
    int32_t c, k;
    if (tick_len <= 0.0f)
        return;
    /* where the oscillator stood at the arrival, in ticks, against where this clock's tick is due (4 per clock, counted
     * from the run's start, so a link that is a clock late stays a clock late and does not slip into the neighbour's) */
    x = (float)((int32_t)dk_ticks - 4 * fol_n) - (tick_left + (float)(uint32_t)(dk_smp - t)) / tick_len;
    {
        float h[5], m;
        int i, j;
        for (i = 4; i > 0; i--)
            xh[i] = xh[i - 1];
        xh[0] = x;
        if (xn < 5)
            xn++;
        for (i = 0; i < xn; i++)
            h[i] = xh[i];
        for (i = 1; i < xn; i++)               /* the median of the last five */
            for (j = i; j > 0 && h[j - 1] > h[j]; j--)
                m = h[j], h[j] = h[j - 1], h[j - 1] = m;
        m = h[xn / 2];
        xbad = (xn >= 3 && (m > 6.0f || m < -6.0f)) ? (uint8_t)(xbad < 255 ? xbad + 1 : 255) : 0;
        if (rebase || (xbad >= 12 && fol_n >= 48)) {      /* a gap, or the count is off by a clock or more: onto the nearest */
            k = (int32_t)(((rebase ? x : m) + 4096.0f * 4.0f + 2.0f) * 0.25f) - 4096;
            fol_n += k;
            x -= (float)k * 4.0f;
            for (i = 0; i < 5; i++)
                xh[i] -= (float)k * 4.0f;
            sync_lock(&sy);
            xbad = 0;
        }
    }
    err = -x;
    dk_fol_err = (int16_t)(err * 256.0f);
    c = sync_phase(&sy, (int32_t)(err * 256.0f));
    tick_left -= (float)c * tick_len * (1.0f / 256.0f);
}

static void fol_clock(int port, int ms)
{
    uint32_t t = fol_stamp(ms);
    int gap;
    if (!fol_accept(port, t))
        return;
    gap = fol_on && sy.count && (int32_t)(t - sy.last) > (int32_t)SY_HOLD;
    sync_clock(&sy, t);
    if (fol_start_next) {
        fol_start_next = 0;
        lead_n = 0;
    } else {
        lead_n++;
    }
    fol_n++;
    if (fol_arm == 1 || (fol_arm == 2 && lead_n % (DK_TPB / 4) == 0)) {
        fol_begin(t);
        return;
    }
    if (fol_on) {
        if (gap)
            sync_lock(&sy);                    /* a clock after a silence: pulled in fast */
        fol_pull(t, gap);
    }
}
static void fol_start(int port, int ms)
{
    uint32_t t = fol_stamp(ms);
    if (!fol_accept(port, t))
        return;
    fol_start_next = 1;
    fol_arm = 1;
    dk_fol = 3;
}
static void fol_stop(int port, int ms)
{
    uint32_t t = fol_stamp(ms);
    if (!fol_accept(port, t))
        return;
    fol_arm = 0;
    if (fol_on && dk_playing)
        play_stop();
    fol_on = 0;
    dk_fol = 0;
}
/* a local PLAY: 1 when it waits for the leader's next bar instead of starting now */
static int fol_local_play(void)
{
    if (fol_arm == 2) {                        /* pressed again while waiting: not wanted after all */
        fol_arm = 0;
        dk_fol = 0;
        return 1;
    }
    if (dk_bt_role != 1 && sync_alive(&sy, dk_smp) && fol_src != 0xFF && !fol_lead) {
        fol_arm = 2;
        dk_fol = 3;
        return 1;
    }
    return 0;
}
static void fol_stopped(void)          /* the transport stopped (any way) */
{
    sync_tx_stop();
    fol_on = 0;
    fol_lead = 0;
    fol_arm = 0;
    dk_fol = 0;
}
static void fol_started(void)          /* the transport started (every way): a run that is not a follower's leads */
{
    fol_lead = !fol_on;
    if (fol_lead)
        dk_fol = 0;
    sync_tx_start();
}
static void fol_cmd(int c, int a, int d)
{
    switch (c) {
    case C_XCLK: fol_clock(a, d); break;
    case C_XSTART: fol_start(a, d); break;
    case C_XSTOP: fol_stop(a, d); break;
    default: break;
    }
}
void dk_fol_reset(void)                /* forget the clock followed (a role change, a test) */
{
    sync_reset(&sy, 0);
    fol_src = 0xFF;
    fol_arm = 0;
    fol_on = 0;
    fol_lead = 0;
    fol_start_next = 0;
    lead_n = 0;
    fol_n = 0;
    dk_fol = 0;
}
static void fol_block(void)            /* once a render, after its commands */
{
    int alive;
    if (fol_role != dk_bt_role) {              /* the role changed: what was followed is forgotten */
        if (fol_role != 0xFF)
            dk_fol_reset();
        fol_role = dk_bt_role;
    }
    alive = sync_alive(&sy, dk_smp);
    dk_clk_alive = (uint8_t)alive;
    if (fol_on)
        dk_fol = alive ? 1 : 2;
    else if (!fol_arm)
        dk_fol = 0;
    dk_fol_bpm10 = sy.out_q8 ? (uint16_t)((60.0f * DK_SR * 256.0f / 24.0f) * 10.0f / (float)sy.out_q8 + 0.5f) : 0u;
}
