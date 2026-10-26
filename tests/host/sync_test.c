/* SPDX-License-Identifier: MPL-2.0 */
/* The follower (deck/follow.c, deck/sync.c) against a simulated leader: the real sequencer rendering, a MIDI clock
 * arriving through the real command queue with the delivery the radio gives it (ms stamps, jitter, bursts, silence),
 * and the oscillator's ticks timed against the leader's clocks. Prints each measurement; exits 1 on a failed check.
 *   sync_test            all of it (about a minute)
 *   sync_test quick      without the 10 minutes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "song.h"

uint32_t ble_log[4096], ble_n;
void dk_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }
void dk_midi_ble(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (ble_n < 4096)
        ble_log[ble_n++] = st | d1 << 8 | d2 << 16;
}

static int checks, fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            fails++;                                                                                               \
            printf("  FAIL: ");                                                                                    \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
        }                                                                                                          \
    } while (0)

#define BLK 32
static uint64_t smp;                    /* samples rendered */
static double now_msd(void) { return (double)smp * 1000.0 / 44100.0; }

/* ---- the leader: clock k is made at T[k] ms and delivered at D (the radio's jitter, bursts) */
typedef struct {
    double bpm0, bpm1, secs;            /* tempo ramps from bpm0 to bpm1 over secs */
    double jit;                         /* delivery delay: jit +- jit ms, uniform */
    double burst_p;                     /* chance a burst of 2..3 clocks starts here (they arrive together) */
} lead_t;
static uint32_t rng = 12345;
static double frand(void)
{
    rng = rng * 1664525u + 1013904223u;
    return (double)(rng >> 8) / 16777216.0;
}

#define MAXQ 4096
#define MAXK 300000
static struct { double d; int kind; } pend[MAXQ];       /* kind 0 clock, 1 start, 2 stop */
static int pend_n;
static double *lead_T;                  /* ms, per clock number */
static double lead_next, lead_t0, last_deliver;
static int lead_k, lead_on, burst_left;
static double burst_at;
static lead_t L;

static void push(double d, int kind)
{
    if (d < last_deliver)
        d = last_deliver;               /* a link keeps the order */
    last_deliver = d;
    if (pend_n < MAXQ) {
        pend[pend_n].d = d;
        pend[pend_n++].kind = kind;
    }
}
static void lead_start(void)             /* Start now, the first clock with it */
{
    lead_t0 = lead_next = now_msd();
    lead_k = 0;
    lead_on = 1;
    burst_left = 0;
    last_deliver = 0;
    push(now_msd() + L.jit, 1);
}
static void lead_stop(void)
{
    lead_on = 0;
    push(now_msd() + L.jit, 2);
}
static double lead_bpm(double t)
{
    double f = L.secs > 0 ? (t - lead_t0) / (L.secs * 1000.0) : 0;
    if (f > 1)
        f = 1;
    return L.bpm0 + (L.bpm1 - L.bpm0) * f;
}
static void lead_make(double upto)       /* the clocks made until this time go to the pending list */
{
    while (lead_on && lead_next <= upto && lead_k < MAXK) {
        double d = lead_next + L.jit + (frand() * 2.0 - 1.0) * L.jit;
        if (burst_left > 0) {
            d = burst_at;
            burst_left--;
        } else if (frand() < L.burst_p) {
            burst_left = 1 + (int)(frand() * 2.0);          /* 2 or 3 in all */
            burst_at = lead_next + 2.0 * L.jit + 1.0 + 20.8 * (double)burst_left;
            d = burst_at;
        }
        lead_T[lead_k] = lead_next;
        push(d, 0);
        lead_k++;
        lead_next += 2500.0 / lead_bpm(lead_next);          /* a clock: 1/24 beat: 60000/24 ms at 1 BPM */
    }
}
static void deliver(void)
{
    int i, w = 0;
    double now = now_msd();
    for (i = 0; i < pend_n; i++) {
        if (pend[i].d <= now)
            dk_cmd(pend[i].kind == 0 ? C_XCLK : pend[i].kind == 1 ? C_XSTART : C_XSTOP, 0, 0, (int)pend[i].d);
        else
            pend[w++] = pend[i];
    }
    pend_n = w;
}

/* ---- the measurement: when each clock-aligned tick (4j of the follower's run) sounded */
static double *osc_T;                   /* ms, per clock number of the follower's run */
static int osc_n = -1;
static uint32_t last_ticks;
static uint8_t was_playing;
static int mute_clocks;                 /* a test that cuts the clock: nothing more is made */
static void step(int blocks)
{
    int32_t out[2 * BLK];
    int b;
    for (b = 0; b < blocks; b++) {
        uint32_t tk;
        dk_ms_now = (uint32_t)now_msd();
        if (!mute_clocks)
            lead_make(now_msd() + 1.0);
        deliver();
        dk_render(out, BLK, 4096u);
        smp += BLK;
        tk = dk_ticks;
        if (dk_playing && (!was_playing || tk < last_ticks)) {
            osc_n = -1;
            last_ticks = 0;
        }
        was_playing = dk_playing;
        if (dk_playing && tk != last_ticks) {
            uint32_t m;
            for (m = last_ticks; m < tk; m++)
                if (m % 4 == 0 && m / 4 < MAXK) {
                    osc_n = (int)(m / 4);
                    osc_T[osc_n] = now_msd() - 0.5 * BLK * 1000.0 / 44100.0;
                }
            last_ticks = tk;
        }
    }
}
static void run_secs(double s) { step((int)(s * 44100.0 / BLK)); }
static void fresh(void)
{
    TAPE.init();
    dk_lock();
    dk_song_clear();
    dk_unlock();
    dk_song.tempo = 100;
    dk_cmd(C_RETUNE, 0, 0, 0);
    dk_cmd(C_PLAY, 0, 0, 0);
    dk_bt_role = 0;
    dk_bt_notes = 0;
    dk_bt_trim = 0;
    dk_fol_reset();
    pend_n = 0;
    lead_on = 0;
    mute_clocks = 0;
    was_playing = 0;
    osc_n = -1;
    last_ticks = 0;
    step(8);
    ble_n = 0;
}

/* the oscillator against the leader's ideal clocks, clocks lo..hi of the follower's run (k0 the leader's clock it started at) */
typedef struct { double mean, sd, maxdev, min, max; int n; } stat_t;
static stat_t errs(int lo, int hi, int k0)
{
    stat_t s;
    double a = 0, v = 0;
    int j, n = 0;
    s.mean = s.sd = s.maxdev = 0;
    s.min = 1e9;
    s.max = -1e9;
    if (hi > osc_n)
        hi = osc_n;
    if (hi > lead_k - k0 - 1)
        hi = lead_k - k0 - 1;
    for (j = lo; j <= hi; j++) {
        double e = osc_T[j] - lead_T[j + k0];
        a += e;
        n++;
        if (e < s.min) s.min = e;
        if (e > s.max) s.max = e;
    }
    s.n = n;
    if (!n)
        return s;
    s.mean = a / n;
    for (j = lo; j <= hi; j++) {
        double e = osc_T[j] - lead_T[j + k0] - s.mean;
        v += e * e;
        if (fabs(e) > s.maxdev) s.maxdev = fabs(e);
    }
    s.sd = sqrt(v / n);
    return s;
}
static double bpm_now(void) { return dk_fol_bpm10 / 10.0; }

static void follow_case(const char *name, double bpm0, double bpm1, double secs_ramp, double jit, double burst_p,
                        double secs, double sd_max, double dev_max, double bpm_tol)
{
    stat_t s;
    L.bpm0 = bpm0;
    L.bpm1 = bpm1;
    L.secs = secs_ramp;
    L.jit = jit;
    L.burst_p = burst_p;
    fresh();
    lead_start();
    run_secs(0.5);
    CHECK(dk_playing, "%s: the follower did not start on Start and the first clock", name);
    CHECK(dk_fol == 1, "%s: not following (%d)", name, dk_fol);
    run_secs(secs);
    s = errs(96, 1 << 30, 0);                      /* (from the second bar: the lock-in is over) */
    printf("  %-34s %6d clocks: offset %+6.2f ms (= the link's mean delay %.1f), spread sd %.2f ms, worst %.2f ms, tempo %.1f (leader %.1f)\n",
           name, s.n, s.mean, jit, s.sd, s.maxdev, bpm_now(), lead_bpm(lead_next));
    if (getenv("SYNC_DBG")) {
        int j;
        for (j = 0; j + 48 <= osc_n && j < 1200; j += 48) {
            stat_t q = errs(j, j + 47, 0);
            printf("    clocks %4d..%4d: mean %+7.2f sd %5.2f max %5.2f\n", j, j + 47, q.mean, q.sd, q.maxdev);
        }
    }
    CHECK(s.sd <= sd_max, "%s: spread sd %.2f ms > %.2f", name, s.sd, sd_max);
    CHECK(s.maxdev <= dev_max, "%s: worst %.2f ms > %.2f", name, s.maxdev, dev_max);
    CHECK(fabs(bpm_now() - lead_bpm(lead_next)) <= bpm_tol, "%s: tempo %.2f vs %.2f", name, bpm_now(), lead_bpm(lead_next));
    CHECK(fabs(s.mean - jit) < (burst_p > 0 ? 8.0 : 3.5) + 0.02 * jit, "%s: offset %.2f ms is not the link's delay %.1f", name, s.mean, jit);
    {
        stat_t l = errs(0, 95, 0);
        printf("      lock-in (the first bar): worst %.1f ms from its own mean %+.1f\n", l.maxdev, l.mean);
        CHECK(l.maxdev < 60.0 && fabs(l.mean - s.mean) < 15.0, "%s: slow lock-in (%.1f ms, mean %.1f vs %.1f)", name, l.maxdev, l.mean, s.mean);
    }
    CHECK(osc_n + 1 >= lead_k - 12, "%s: the follower lost clocks: its beat %d, the leader's %d", name, osc_n, lead_k);
}

static void roles(void)
{
    int i, clk = 0, st = 0, sp = 0, spp = 0, ct = 0;
    printf("-- roles and the transport sent\n");
    /* LEAD: PLAY sends SPP 0, Start, 24 clocks a beat; Stop; follows nothing */
    fresh();
    dk_bt_role = 1;
    L.bpm0 = L.bpm1 = 90;
    L.secs = 0;
    L.jit = 5;
    L.burst_p = 0;
    dk_song.tempo = 120;
    dk_cmd(C_RETUNE, 0, 0, 0);
    dk_cmd(C_PLAY, 1, 0, 0);
    run_secs(2.0);
    for (i = 0; i < (int)ble_n; i++) {
        int s = ble_log[i] & 0xFF;
        clk += s == 0xF8, st += s == 0xFA, ct += s == 0xFB, spp += s == 0xF2, sp += s == 0xFC;
    }
    CHECK(st == 1 && spp == 1 && ct == 0, "LEAD: Start %d, SPP %d, Continue %d (1, 1, 0)", st, spp, ct);
    CHECK(ble_log[0] == 0xF2 && ble_log[1] == 0xFA, "LEAD: SPP goes before Start: %02X %02X", ble_log[0] & 0xFF, ble_log[1] & 0xFF);
    CHECK(clk >= 95 && clk <= 97, "LEAD: %d clocks in 2 s at 120 BPM (96)", clk);
    CHECK(ble_n >= 2 && (ble_log[1] >> 8 & 0xFF) == 0 && (ble_log[0] >> 8 & 0xFF) == 0, "LEAD: SPP 0 is 0 0");
    lead_start();                                   /* a leader's clock arrives: LEAD ignores it */
    run_secs(1.0);
    CHECK(dk_fol == 0 && dk_fol_bpm10 == 0, "LEAD follows a clock (fol %d, %d)", dk_fol, dk_fol_bpm10);
    lead_on = 0;
    ble_n = 0;
    dk_cmd(C_PLAY, 0, 0, 0);
    run_secs(0.1);
    CHECK(ble_n == 1 && (ble_log[0] & 0xFF) == 0xFC, "LEAD: Stop sent (%u messages)", ble_n);
    /* FOLLOW: PLAY with no clock plays on its own and sends nothing */
    fresh();
    dk_bt_role = 2;
    dk_cmd(C_PLAY, 1, 0, 0);
    run_secs(1.0);
    CHECK(dk_playing, "FOLLOW: PLAY with no clock did not start");
    CHECK(ble_n == 0, "FOLLOW sent %u messages", ble_n);
    dk_cmd(C_PLAY, 0, 0, 0);
    run_secs(0.1);
    CHECK(ble_n == 0, "FOLLOW sent a Stop");
    /* AUTO: PLAY with no clock leads (sends) */
    fresh();
    dk_bt_role = 0;
    dk_cmd(C_PLAY, 1, 0, 0);
    run_secs(1.0);
    CHECK(ble_n > 20 && (ble_log[1] & 0xFF) == 0xFA, "AUTO: PLAY with no clock does not lead (%u messages)", ble_n);
    /* ... and it ignores a clock that starts while it leads */
    L.bpm0 = L.bpm1 = 133;
    lead_start();
    run_secs(1.5);
    CHECK(dk_fol == 0 && dk_fol_bpm10 == 0, "AUTO leading followed a clock (fol %d bpm %d)", dk_fol, dk_fol_bpm10);
    lead_on = 0;
    dk_cmd(C_PLAY, 0, 0, 0);
    run_secs(0.5);
    /* AUTO as a follower: Start, the next clock; sends nothing */
    fresh();
    dk_bt_role = 0;
    L.bpm0 = L.bpm1 = 110;
    lead_start();
    run_secs(0.5);
    CHECK(dk_playing && dk_fol == 1, "AUTO: Start did not start it (%d, %d)", dk_playing, dk_fol);
    CHECK(ble_n == 0, "AUTO following sent %u messages", ble_n);
    CHECK(osc_n >= 0 && fabs(osc_T[0] - lead_T[0] - L.jit) < 8.0, "AUTO: its first tick %.1f ms after the clock's (delay %.1f)",
          osc_T[0] - lead_T[0], L.jit);
    lead_stop();
    run_secs(0.2);
    CHECK(!dk_playing, "AUTO: Stop did not stop it");
    CHECK(ble_n == 1 && (ble_log[0] & 0xFF) == 0xFC ? 0 : 1, "(a follower's stop is not sent on)");
    CHECK(ble_n == 0, "AUTO: a follower's Stop was sent on (%u)", ble_n);
    /* BT NOTES */
    fresh();
    dk_bt_role = 1;
    dk_bt_notes = 0;
    dk_midi_notes = 1;
    dk_note(0, 60, 100, 0);
    run_secs(0.2);
    for (i = 0, clk = 0; i < (int)ble_n; i++)
        clk += ((ble_log[i] & 0xF0) == 0x90 || (ble_log[i] & 0xF0) == 0x80);
    CHECK(clk == 0, "BT NOTES off: %d note messages to BLE", clk);
    dk_bt_notes = 1;
    dk_note(0, 60, 100, 0);
    run_secs(0.3);
    for (i = 0, clk = 0; i < (int)ble_n; i++)
        clk += ((ble_log[i] & 0xF0) == 0x90);
    CHECK(clk >= 1, "BT NOTES on: %d note-ons to BLE", clk);
    dk_bt_notes = 0;
}

static void joining(void)
{
    int k0, i;
    printf("-- joining, silence, sources\n");
    /* a local PLAY while a clock is alive waits for the leader's next bar (96 clocks from its Start) */
    L.bpm0 = L.bpm1 = 120;
    L.secs = 0;
    L.jit = 8;
    L.burst_p = 0;
    fresh();
    dk_bt_role = 0;
    lead_start();
    run_secs(0.3);
    CHECK(dk_playing, "joining: the first follower did not start");
    dk_cmd(C_PLAY, 0, 0, 0);                        /* stopped here: the leader goes on */
    run_secs(0.3);
    CHECK(!dk_playing, "joining: STOP did not stop it");
    {
        int k_press = lead_k;
        dk_cmd(C_PLAY, 1, 0, 0);
        run_secs(0.1);
        CHECK(!dk_playing && dk_fol == 3, "joining: PLAY started at once instead of waiting for the bar (play %d, fol %d)", dk_playing, dk_fol);
        run_secs(0.5);
        CHECK(!dk_playing, "joining: started before the bar (clock %d of 96)", lead_k);
        run_secs((lead_t0 + 2100.0 + L.jit * 2 - now_msd()) / 1000.0);   /* the bar's start is at clock 96 = 2000 ms from Start at 120 BPM */
        (void)k_press;
        CHECK(dk_playing, "joining: not playing at the bar");
        k0 = 96;
        run_secs(1.5);
        {
            stat_t s = errs(24, 1 << 30, k0);
            printf("  joined at the bar (clock %d): offset %+.2f ms, sd %.2f, worst %.2f\n", k0, s.mean, s.sd, s.maxdev);
            CHECK(fabs(s.mean - L.jit) < 6.0 && s.maxdev < 8.0, "joining: not on the leader's clock: offset %.2f dev %.2f", s.mean, s.maxdev);
            CHECK(s.n >= 20, "joining: only %d clocks measured", s.n);
        }
    }
    /* PLAY pressed twice while waiting cancels */
    dk_cmd(C_PLAY, 0, 0, 0);
    run_secs(0.2);
    dk_cmd(C_PLAY, 1, 0, 0);
    run_secs(0.1);
    dk_cmd(C_PLAY, 1, 0, 0);
    run_secs(3.0);
    CHECK(!dk_playing && dk_fol == 0, "joining: a second PLAY while waiting did not cancel it (play %d, fol %d)", dk_playing, dk_fol);

    /* silence: 250 ms with no clock = HOLD, the tempo kept; a clock again: back */
    fresh();
    dk_bt_role = 0;
    L.bpm0 = L.bpm1 = 97.5;
    lead_start();
    run_secs(8.0);
    CHECK(dk_fol == 1, "silence: not following before it (%d)", dk_fol);
    mute_clocks = 1;
    run_secs(0.2);
    CHECK(dk_fol == 1, "silence: HOLD already at 200 ms (%d)", dk_fol);
    run_secs(0.2);
    CHECK(dk_fol == 2, "silence: no HOLD after 400 ms (%d)", dk_fol);
    run_secs(3.0);
    CHECK(dk_playing && dk_fol == 2 && fabs(bpm_now() - 97.5) < 0.3, "silence: the tempo not held after 3.4 s: %.2f fol %d", bpm_now(), dk_fol);
    {                                               /* the leader kept going meanwhile: clocks on its schedule from where they stopped */
        double gap;
        mute_clocks = 0;
        gap = now_msd() - lead_next;
        for (i = 0; i < 0; i++) ;
        lead_next += gap - fmod(gap, 2500.0 / 97.5);         /* it was running all along: the grid kept, the clocks of the silence lost */
        lead_k += (int)(gap / (2500.0 / 97.5));
    }
    run_secs(1.0);
    CHECK(dk_fol == 1, "silence: not following again after the clock returned (%d)", dk_fol);
    {
        stat_t s = errs(osc_n - 20, osc_n, 0);
        printf("  after a 3.4 s silence: offset %+.2f ms, sd %.2f ms, tempo %.1f\n", s.mean, s.sd, bpm_now());
        CHECK(s.maxdev < 8.0, "silence: not back on the grid (worst %.2f ms)", s.maxdev);
    }
    /* Stop and Start again restarts from 0 on the next clock (the leader restarts) */
    lead_stop();
    run_secs(0.3);
    CHECK(!dk_playing, "restart: Stop did not stop");
    lead_start();
    run_secs(0.3);
    CHECK(dk_playing && dk_ticks < 200, "restart: not started again (%u ticks)", dk_ticks);
    /* a Start while playing (the leader restarted) restarts at its first clock */
    run_secs(3.0);
    CHECK(dk_ticks > 400, "restart: ticks %u", dk_ticks);
    lead_start();
    run_secs(0.3);
    CHECK(dk_ticks < 200, "restart: a Start while playing did not restart it (%u ticks)", dk_ticks);
    /* another source: while one clock is alive a second port's is ignored: (the sim has one port) */
}

int main(int argc, char **argv)
{
    int quick = argc > 1 && !strcmp(argv[1], "quick");
    lead_T = calloc(MAXK, sizeof(double));
    osc_T = calloc(MAXK, sizeof(double));
    dk_init();
    printf("-- the clock through the real sequencer (jitter = the link's delay jit +- jit, a clock 1/24 beat)\n");
    follow_case("steady 120, no jitter", 120, 120, 0, 0, 0, 60, 0.5, 1.0, 0.15);
    follow_case("120, jitter +-15 ms", 120, 120, 0, 15, 0, 120, 3.2, 10.0, 0.5);
    follow_case("120, +-15 ms, bursts of 2-3", 120, 120, 0, 15, 0.06, 120, 4.2, 14.0, 1.0);
    follow_case("73.3 BPM, +-15 ms, bursts", 73.3, 73.3, 0, 15, 0.06, 120, 3.2, 11.0, 0.6);
    follow_case("200 BPM, +-10 ms, bursts", 200, 200, 0, 10, 0.06, 90, 4.8, 21.0, 1.0);
    follow_case("tempo ramp 100->140 in 60 s, +-15", 100, 140, 60, 15, 0.04, 70, 3.6, 12.0, 0.8);
    if (!quick)
        follow_case("10 min drift 90->110, +-15, bursts", 90, 110, 600, 15, 0.05, 600, 3.6, 14.0, 0.8);
    roles();
    joining();
    printf("sync_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
