/* SPDX-License-Identifier: MPL-2.0 */
/* BENCH: times a DX7-style 6-operator FM voice (dx.c) on the FM-1, and reports it on the screen and over the diag
 * SysEx (tools/deck_bench.py). Part of the unity build (device/unity.c, host/plat_host.c), after the platform.
 *
 * What it measures (each "row" is one (variant, voices, block) and takes a few ms to a few tens of ms):
 *   I32   CPU0, integer voice (32-bit phase, int16 table, Q15 x Q15 -> int32; no 64-bit products: see docs/CPU.md)
 *   F32   CPU0, the same voice in single-precision float with a float table
 *   FSIN  CPU0, float with fm_sin_turns (no table)
 *   C1-I32 / C1-F32  the same voices run on CPU1 (the second core), CPU0 only waits
 *   PAR   N integer voices on CPU0 AND N on CPU1 at once (the speed-up, and what the two cores cost each other)
 *   C1-NOP  a CPU1 job that does nothing: the hand-off cost
 * 6 operators x N voices x 128 (or 64) samples = one block; the time per block against the block's length (2902 us
 * for 128 samples at 44.1 kHz) says how many voices fit. Rows run one per main-loop pass; CPU0 rows keep interrupts
 * off for ~4 ms at a time (so nothing else takes time from the measurement), CPU1 rows do not.
 *
 * The bench never makes sound (app_render writes silence) and saves nothing. CPU1 is EXPERIMENTAL (fm1_cpu1.h: it
 * has not run on a device): it starts only on request (PC tool, or REC pressed twice), never in the first 31 s after
 * power-on (a crash then would count as a failed boot), waits for it at most 400 ms, and if it hangs says so and
 * leaves the rest alone: power-cycle and the CPU0 numbers are still good. */
#include "dx.h"

#ifdef PLAT_HOST
#include <time.h>
static uint32_t bx_ticks(void)           /* the host's 24 MHz tick, from its clock */
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 24000000u + (uint64_t)ts.tv_nsec * 24u / 1000u);
}
static uint32_t bx_cyc(void) { return 0; }
#define BX_WDT()
#else
#include "fm1_cpu1.h"
static uint8_t bx_perf_ok;               /* the cycle counter (C0_TL_CKCNT) moves: else cycles are reported as 0 */
static uint32_t bx_ticks(void) { return fm1_ticks(); }
#define BX_WDT() fm1_wdt_feed()      /* the main loop's watchdog: fed while the bench waits */
static uint32_t bx_cyc(void) { return bx_perf_ok ? fm1_perf_cycles() : 0u; }
#endif
#define TICKS_US 24u                     /* the tick: 24 MHz */

enum { V_I, V_F, V_S, V_C1I, V_PAR, V_C1F, V_HS, NVAR };
static const char *const VAR_NAME[NVAR] = {"I32", "F32", "FSIN", "C1-I32", "PAR-I32", "C1-F32", "C1-NOP"};
enum { ST_IDLE, ST_RUN0, ST_DONE0, ST_RUN1, ST_DONE1 };
enum { R_PEND, R_OK, R_SLOW, R_TIMEOUT, R_NOC1 };                    /* a row's result */
enum { C1_NEVER, C1_UP, C1_NOSTART, C1_HUNG };                       /* CPU1 */

#define NN 6
static const uint8_t NLIST[NN] = {1, 4, 8, 12, 16, 24};
static const uint8_t BLKS[2] = {128, 64};
#define N0 45u                           /* CPU0 rows: 3 variants x 2 blocks x 6 counts, then 9 for the operator count */
#define NROWS 82u                        /* + CPU1 rows: 1 hand-off, then 3 variants x 2 blocks x 6 counts */
#define OPS_ROWS 3u
#define TRIAL_TICKS (4000u * TICKS_US)   /* one trial: ~4 ms (interrupts off for that long, CPU0 rows) */
#define TRIALS 5u                        /* the best of five */
#define SKIP_TICKS (150000u * TICKS_US)  /* a row predicted to take over 150 ms is skipped */
#define C1_TIMEOUT (400000u * TICKS_US)  /* CPU1 gives no answer for 400 ms: hung */
#define C1_AFTER_BOOT_MS 31000u          /* CPU1 never in the boot guard's 30 s */

typedef struct {
    uint8_t var, nv, blk, ops, st, ok;
    uint32_t tk, cy, rp, ck;             /* ticks and cycles per block (best trial), reps per trial, checksum */
} row_t;
static row_t rows[NROWS];
static dx_voice_t bv[DX_MAXV];           /* CPU0's voices */
static int32_t oi[128];
static float of[128];
static uint32_t pv_ref[3][2];            /* ticks one voice's block takes on CPU0, per variant and block (rows with 1 voice) */

static struct {
    uint8_t st, c1st, phase_end;
    uint32_t cur;
    uint32_t armed_at;                   /* REC pressed once: asks for CPU1 */
    uint32_t seq;
} bx;

/* ------------------------------------------------------------- CPU1's job --- */
static dx_voice_t c1v[DX_MAXV];
static int32_t c1oi[128];
static float c1of[128];
static volatile struct {                 /* single-writer words: CPU0 writes go, the job; CPU1 writes done, hb */
    uint32_t go, done, hb;
    uint32_t var, nv, blk, reps;
    uint32_t chk;
} c1;

static uint32_t chk_i(const int32_t *o, uint32_t n)       /* 32-bit adds and rotates only */
{
    uint32_t i, c = 0;
    for (i = 0; i < n; i++)
        c = ((c << 3) | (c >> 29)) + (uint32_t)o[i];
    return c;
}
static uint32_t chk_f(const float *o, uint32_t n)
{
    uint32_t i, c = 0;
    for (i = 0; i < n; i++)
        c = ((c << 3) | (c >> 29)) + (uint32_t)(int32_t)(o[i] * 8192.0f);
    return c;
}

static void render_c0(uint32_t var, dx_voice_t *v, uint32_t nv, uint32_t blk, uint32_t reps)
{
    uint32_t r, i;
    for (r = 0; r < reps; r++) {
        if (var == V_I) {
            for (i = 0; i < blk; i++)
                oi[i] = 0;
            dx_render_i(v, nv, oi, blk);
        } else {
            for (i = 0; i < blk; i++)
                of[i] = 0.0f;
            if (var == V_F)
                dx_render_f(v, nv, of, blk);
            else
                dx_render_s(v, nv, of, blk);
        }
    }
}

static void c1_job(void)                 /* on CPU1 (the host runs it inline) */
{
    uint32_t r, i, nv = c1.nv, blk = c1.blk, reps = c1.reps, var = c1.var;
    for (r = 0; r < reps; r++) {
        if (var == V_C1F) {
            for (i = 0; i < blk; i++)
                c1of[i] = 0.0f;
            dx_render_f(c1v, nv, c1of, blk);
            if (r == 0)
                c1.chk = chk_f(c1of, blk);
        } else if (var != V_HS) {        /* V_C1I and V_PAR: the integer voice */
            for (i = 0; i < blk; i++)
                c1oi[i] = 0;
            dx_render_i(c1v, nv, c1oi, blk);
            if (r == 0)
                c1.chk = chk_i(c1oi, blk);
        }
    }
}

#ifndef PLAT_HOST
void fm1_cpu1_main(void)                 /* CPU1: polls for a job, no interrupts (fm1_cpu1.h) */
{
    uint32_t seen = 0;
    fm1_cpu1_ready();
    for (;;) {
        c1.hb++;
        if (c1.go != seen) {
            seen = c1.go;
            c1_job();
            c1.done = seen;
        }
    }
}
#endif

static int c1_start(void)                /* 0 = CPU1 reported in */
{
#ifdef PLAT_HOST
    return 0;
#else
    int rc;
    fm1_guard_unlock_top();              /* the entry word is in the guarded top of RAM */
    rc = fm1_cpu1_start(300);
    fm1_guard_lock_top();
    return rc;
#endif
}

/* a job to CPU1 and the wait for it: 0 = done, -1 = no answer in 400 ms (CPU1 hung) */
static int c1_run(uint32_t var, uint32_t nv, uint32_t blk, uint32_t reps, int own_too)
{
    uint32_t t0, seq = ++bx.seq;
    c1.var = var;
    c1.nv = nv;
    c1.blk = blk;
    c1.reps = reps;
    c1.done = 0;
    c1.go = seq;                         /* the last write: CPU1 starts on it */
#ifdef PLAT_HOST
    c1_job();
    c1.done = seq;
#endif
    if (own_too)                         /* PAR: CPU0 works too, while CPU1 does */
        render_c0(V_I, bv, nv, blk, reps);
    t0 = bx_ticks();
    while (c1.done != seq) {
        BX_WDT();
        if ((uint32_t)(bx_ticks() - t0) > C1_TIMEOUT)
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------- rows --- */
static void voices_set(dx_voice_t *v, uint32_t nv, uint32_t ops)
{
    uint32_t i;
    for (i = 0; i < nv; i++)
        dx_voice_set(&v[i], i, ops, 36u + (i * 7u) % 36u);
}

static uint32_t reps_for(uint32_t tcal)
{
    uint32_t r = tcal ? TRIAL_TICKS / tcal : 64u;
    return r < 1u ? 1u : r > 64u ? 64u : r;
}

static void plan(void)
{
    uint32_t i = 0, v, b, k, o;
    for (v = 0; v < 3u; v++)
        for (b = 0; b < 2u; b++)
            for (k = 0; k < NN; k++, i++)
                rows[i] = (row_t){(uint8_t)v, NLIST[k], BLKS[b], DX_OPS, 0, 0, 0, 0, 0, 0};
    for (v = 0; v < 3u; v++)
        for (o = 0; o < OPS_ROWS; o++, i++)
            rows[i] = (row_t){(uint8_t)v, 1, 128, (uint8_t)(o == 0 ? 1 : o == 1 ? 2 : 4), 0, 0, 0, 0, 0, 0};
    rows[i++] = (row_t){V_HS, 0, 128, DX_OPS, 0, 0, 0, 0, 0, 0};
    {
        static const uint8_t C1V[3] = {V_C1I, V_PAR, V_C1F};
        for (v = 0; v < 3u; v++)
            for (b = 0; b < 2u; b++)
                for (k = 0; k < NN; k++, i++)
                    rows[i] = (row_t){C1V[v], NLIST[k], BLKS[b], DX_OPS, 0, 0, 0, 0, 0, 0};
    }
}

static void row_core0(row_t *r)
{
    uint32_t bi = r->blk == 128 ? 0u : 1u, tcal, t0, reps, best = 0xFFFFFFFFu, bestc = 0, tr;
    if (pv_ref[r->var][bi] && pv_ref[r->var][bi] * r->nv > SKIP_TICKS) {
        r->st = R_SLOW;
        return;
    }
    voices_set(bv, r->nv, r->ops);
    t0 = bx_ticks();
    render_c0(r->var, bv, r->nv, r->blk, 1);
    tcal = bx_ticks() - t0;
    r->ck = r->var == V_I ? chk_i(oi, r->blk) : chk_f(of, r->blk);
    if (r->nv == 1 && r->ops == DX_OPS)
        pv_ref[r->var][bi] = tcal;
    reps = reps_for(tcal);
    for (tr = 0; tr < TRIALS; tr++) {
        uint32_t f, c0, c1c, t1, per, perc;
        voices_set(bv, r->nv, r->ops);
        f = tcal * reps <= 10000u * TICKS_US ? plat_lock() : 0xFFu;   /* nothing else runs (~4 ms); a long row (rare: >10 ms) is not locked */
        c0 = bx_cyc();
        t0 = bx_ticks();
        render_c0(r->var, bv, r->nv, r->blk, reps);
        t1 = bx_ticks();
        c1c = bx_cyc();
        if (f != 0xFFu)
            plat_unlock(f);
        per = (t1 - t0) / reps;
        perc = (c1c - c0) / reps;
        if (per < best) {
            best = per;
            bestc = perc;
        }
    }
    r->tk = best;
    r->cy = bestc;
    r->rp = reps;
    r->ok = 1;
    r->st = R_OK;
}

static void row_core1(row_t *r)
{
    uint32_t ref = (r->var == V_C1F) ? V_F : V_I, bi = r->blk == 128 ? 0u : 1u, tcal, t0, reps, best = 0xFFFFFFFFu, tr;
    uint32_t exp_chk = 0;
    if (bx.c1st != C1_UP) {
        r->st = bx.c1st == C1_HUNG ? R_TIMEOUT : R_NOC1;
        return;
    }
    if (r->var != V_HS && pv_ref[ref][bi] && pv_ref[ref][bi] * r->nv > SKIP_TICKS) {
        r->st = R_SLOW;
        return;
    }
    if (r->var != V_HS) {                /* what CPU0 gets for the same voices: the answer CPU1 must give */
        voices_set(bv, r->nv, r->ops);
        render_c0(ref, bv, r->nv, r->blk, 1);
        exp_chk = ref == V_I ? chk_i(oi, r->blk) : chk_f(of, r->blk);
    }
    voices_set(c1v, r->nv, r->ops);
    t0 = bx_ticks();
    if (c1_run(r->var, r->nv, r->blk, 1, 0) < 0)
        goto hung;
    tcal = bx_ticks() - t0;
    r->ck = c1.chk;
    r->ok = r->var == V_HS || r->ck == exp_chk;
    reps = reps_for(tcal);
    if (r->var == V_HS)
        reps = 1;
    for (tr = 0; tr < TRIALS; tr++) {
        uint32_t t1, per;
        voices_set(c1v, r->nv, r->ops);
        if (r->var == V_PAR)
            voices_set(bv, r->nv, r->ops);
        t0 = bx_ticks();
        if (c1_run(r->var, r->nv, r->blk, reps, r->var == V_PAR) < 0)
            goto hung;
        t1 = bx_ticks();
        per = (t1 - t0) / reps;
        if (per < best)
            best = per;
    }
    r->tk = best;
    r->cy = 0;
    r->rp = reps;
    r->st = R_OK;
    return;
hung:
    bx.c1st = C1_HUNG;
    r->st = R_TIMEOUT;
}

/* ------------------------------------------------------------- the run --- */
static uint8_t redraw;
static void clear_all(void)
{
    uint32_t i;
    plan();
    for (i = 0; i < 6u; i++)
        pv_ref[i / 2u][i % 2u] = 0;
    bx.st = ST_IDLE;
    redraw = 1;
}

static void start_phase(uint32_t p)      /* 0: CPU0 rows, 1: CPU1 rows. 0 = started, else seconds to wait */
{
    if (bx.st == ST_RUN0 || bx.st == ST_RUN1)
        return;
    if (p == 0) {
        clear_all();
        bx.st = ST_RUN0;
        bx.cur = 0;
        bx.phase_end = N0;
    } else {
        uint32_t i;
        for (i = N0; i < NROWS; i++)
            if (bx.c1st == C1_UP || rows[i].st != R_OK)
                rows[i].st = R_PEND;
        bx.st = ST_RUN1;
        bx.cur = N0;
        bx.phase_end = NROWS;
        if (bx.c1st == C1_NEVER)
            bx.c1st = c1_start() == 0 ? C1_UP : C1_NOSTART;
    }
    redraw = 1;
}

static uint32_t c1_wait_s(void)          /* seconds until CPU1 may start */
{
    uint32_t t = plat_ms();
    return t >= C1_AFTER_BOOT_MS ? 0u : (C1_AFTER_BOOT_MS - t + 999u) / 1000u;
}

static void step(void)
{
    row_t *r;
    if (bx.st != ST_RUN0 && bx.st != ST_RUN1)
        return;
    r = &rows[bx.cur];
    if (bx.cur < N0)
        row_core0(r);
    else
        row_core1(r);
    bx.cur++;
    redraw = 1;
    if (bx.cur >= bx.phase_end)
        bx.st = bx.st == ST_RUN0 ? ST_DONE0 : ST_DONE1;
}

/* -------------------------------------------------------------- the PC tool --- */
static char *kv(char *q, const char *k, uint32_t v)   /* " key=number" */
{
    char t[11];
    int n = 0;
    *q++ = ' ';
    while (*k)
        *q++ = *k++;
    do
        t[n++] = (char)('0' + v % 10u);
    while ((v /= 10u) != 0);
    while (n)
        *q++ = t[--n];
    return q;
}

static uint32_t reply(uint8_t cmd, uint32_t arg, uint8_t *f)   /* the frame for a command, its length */
{
    char *q;
    uint32_t k = 0, i;
    f[k++] = 0xF0, f[k++] = 0x7D, f[k++] = 0x44, f[k++] = 0x4B, f[k++] = 0x53;
    q = (char *)&f[k];
    if (cmd == 'R' && arg < NROWS) {
        const row_t *r = &rows[arg];
        *q++ = 'B', *q++ = 'X', *q++ = ' ', *q++ = 'r';
        q = kv(q, "i=", arg);
        q = kv(q, "v=", r->var);
        q = kv(q, "n=", r->nv);
        q = kv(q, "b=", r->blk);
        q = kv(q, "o=", r->ops);
        q = kv(q, "st=", r->st);
        q = kv(q, "tk=", r->tk);
        q = kv(q, "cy=", r->cy);
        q = kv(q, "rp=", r->rp);
        q = kv(q, "ck=", r->ck);
        q = kv(q, "ok=", r->ok);
    } else {
        *q++ = 'B', *q++ = 'X', *q++ = ' ', *q++ = 'm';
        q = kv(q, "ver=", 1);
        q = kv(q, "st=", bx.st);
        q = kv(q, "cur=", bx.cur);
        q = kv(q, "n0=", N0);
        q = kv(q, "n=", NROWS);
        q = kv(q, "c1=", bx.c1st);
        q = kv(q, "hb=", c1.hb);
        q = kv(q, "wait=", c1_wait_s());
#ifdef PLAT_HOST
        q = kv(q, "perf=", 0);
#else
        q = kv(q, "perf=", bx_perf_ok);
        q = kv(q, "irqoff_max_us=", lock_max);
        q = kv(q, "xrun=", plat_xruns());
#endif
        q = kv(q, "t=", plat_ms());
    }
    *q = 0;
    for (i = k; f[i]; i++)
        f[i] &= 0x7Fu;
    k = i;
    f[k++] = 0xF7;
    return k;
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    static uint8_t f[160];
    uint8_t cmd = n > 4 ? p[4] : '?';
    if (n < 4 || p[0] != 0x7D || p[1] != 0x44 || p[2] != 0x4B || p[3] != 0x53)
        return 0;
    if (cmd == 'S')
        start_phase(0);
    else if (cmd == 'X' && bx.st != ST_RUN0 && bx.st != ST_RUN1)
        clear_all();
    else if (cmd == 'C' && !c1_wait_s())
        start_phase(1);
    plat_sysex_send(f, reply(cmd, n > 5 ? p[5] : 0u, f));
    return 1;
}

/* ------------------------------------------------------------- the platform --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    (void)master_q12;
    for (i = 0; i < 2u * n; i++)
        lr[i] = 0;
}
void app_panic(void) {}

void app_init(void)
{
    dx_init();
    clear_all();
    bx.c1st = C1_NEVER;
#ifndef PLAT_HOST
    bx_perf_ok = (uint8_t)fm1_perf_cycles_run();
#endif
}

static struct {
    char msg[2][24];
    uint32_t msg_until, btn;
} ui;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
static void u_text(uint32_t v, char *b)
{
    char t[11];
    int n = 0;
    do
        t[n++] = (char)('0' + v % 10u);
    while ((v /= 10u) != 0);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 2500u;
    redraw = 1;
}

static void input(void)
{
    uint32_t btn = plat_buttons(), down = btn & ~ui.btn, now = plat_ms();
    int32_t k;
    for (k = 0; k < NE; k++)
        (void)plat_enc(k);               /* (the encoders' counts are not used) */
    if (down >> B_PLAY & 1u) {
        start_phase(0);
        app_say("RUN", "CPU0");
    }
    if (down >> B_REC & 1u) {
        if (bx.st == ST_RUN0 || bx.st == ST_RUN1) {
            app_say("BUSY", "");
        } else if (c1_wait_s()) {
            char t[12];
            u_text(c1_wait_s(), t);
            text_cat(t, " S");
            app_say("CPU1 WAIT", t);
        } else if (bx.armed_at && now - bx.armed_at < 5000u) {
            bx.armed_at = 0;
            start_phase(1);
            app_say("RUN", "CPU1");
        } else {
            bx.armed_at = now ? now : 1u;
            app_say("CPU1 EXPERIMENTAL", "REC AGAIN");
        }
    }
    if (down >> B_HOME & 1u) {
        if (bx.st != ST_RUN0 && bx.st != ST_RUN1) {
            clear_all();
            app_say("CLEARED", "");
        }
    }
    if (btn != ui.btn)
        redraw = 1;
    ui.btn = btn;
    plat_glow(1);
    plat_leds(btn, 0);
}
void app_input_only(void) { input(); }

static uint32_t row_index(uint32_t var, uint32_t bi, uint32_t k)   /* the table's row: variant 0..5, block 0/1, count k */
{
    if (var < 3u)
        return (var * 2u + bi) * NN + k;
    return N0 + 1u + ((var == 3u ? 0u : var == 4u ? 1u : 2u) * 2u + bi) * NN + k;
}

static void cell_text(const row_t *r, char *t)
{
    if (r->st == R_OK)
        u_text((r->tk + TICKS_US / 2u) / TICKS_US, t);
    else {
        t[0] = r->st == R_SLOW ? 'S' : r->st == R_TIMEOUT ? 'T' : r->st == R_NOC1 ? 'X' : '-';
        t[1] = 0;
    }
}

static void draw(void)
{
    static const char *const LAB[6] = {"I32", "F32", "FSIN", "C1-I", "C1-F", "PAR"};
    static const uint8_t VAR_OF[6] = {V_I, V_F, V_S, V_C1I, V_C1F, V_PAR};
    char t[32], u[12];
    uint32_t i, k, now = plat_ms();
    const row_t *r;
    cv_begin(240, 30, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(6, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        static const char *const SN[5] = {"IDLE", "RUNNING", "CPU0 DONE", "CPU1 RUN", "ALL DONE"};
        int32_t x = cv_text(6, 6, &FONT_B, APP_NAME, C_WHITE);
        t[0] = 0;
        text_cat(t, SN[bx.st]);
        if (bx.st == ST_RUN0 || bx.st == ST_RUN1) {
            text_cat(t, " ");
            u_text(bx.cur, u);
            text_cat(t, u);
        }
        cv_text(x + 10, 8, &FONT_XS, t, C_HI);
    }
    cv_blit(0, 0);
    cv_begin(240, 144, C_BLACK);
    cv_text(4, 0, &FONT_XS, "US PER 128-SAMPLE BLOCK (2902 = FULL)", C_GRAY);
    for (k = 0; k < NN; k++) {
        u_text(NLIST[k], t);
        cv_text(46 + (int32_t)k * 32, 13, &FONT_XS, t, C_GRAY);
    }
    for (i = 0; i < 6u; i++) {
        int32_t y = 27 + (int32_t)i * 17;
        cv_text(4, y, &FONT_XS, LAB[i], C_GRAY);
        for (k = 0; k < NN; k++) {
            uint32_t ri = row_index(VAR_OF[i], 0, k);
            r = &rows[ri];
            cell_text(r, t);
            cv_text(46 + (int32_t)k * 32, y, &FONT_XS, t, r->st != R_OK ? C_DIM : (r->tk / TICKS_US > 2902u) ? C_AMB : C_HI);
        }
    }
    cv_blit(0, 32);
    cv_begin(240, 64, C_BLACK);
    t[0] = 0;
    r = &rows[row_index(V_I, 0, NN - 1u)];
    if (r->st == R_OK) {                 /* ns per operator-sample: tk / (24 ticks per us) / (24 voices x 6 ops x 128) */
        u_text(r->tk * 5u / 2212u, u);
        text_cat(t, "OP I32 ");
        text_cat(t, u);
        text_cat(t, "NS");
    }
    {
        const row_t *rf = &rows[row_index(V_F, 0, NN - 1u)];
        if (rf->st == R_OK) {
            u_text(rf->tk * 5u / 2212u, u);
            text_cat(t, "  F32 ");
            text_cat(t, u);
            text_cat(t, "NS");
        }
    }
    if (r->st == R_OK && r->cy) {
        u_text(r->cy * 24u / r->tk, u);
        text_cat(t, "  CLK ");
        text_cat(t, u);
        text_cat(t, "MHZ");
    }
    cv_text(4, 0, &FONT_XS, t, C_HI);
    {
        static const char *const CN[4] = {"CPU1 NOT STARTED", "CPU1 RUNNING", "CPU1 DID NOT START", "CPU1 HUNG: POWER CYCLE"};
        cv_text(4, 15, &FONT_XS, CN[bx.c1st], bx.c1st >= C1_NOSTART ? C_AMB : C_GRAY);
    }
    cv_text(4, 30, &FONT_XS, "PLAY RUN  REC 2X CPU1  HOME CLEAR", C_GRAY);
    cv_text(4, 45, &FONT_XS, "S SLOW  T TIMEOUT  X NO CPU1", C_DIM);
    cv_blit(0, 176);
    (void)VAR_NAME;
}

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    redraw = 1;
}

void app_frame(void)
{
    input();
    if (bx.armed_at && plat_ms() - bx.armed_at >= 5000u)
        bx.armed_at = 0;
    step();                              /* one row a pass: the screen, USB and the watchdog get their turns between */
    if (ui.msg_until && plat_ms() >= ui.msg_until) {
        ui.msg_until = 0;
        redraw = 1;
    }
    if (redraw) {
        redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    static uint8_t f[160];
    uint32_t i, ok = 1;
    if (!strcmp(what, "st"))
        *got = bx.st;
    else if (!strcmp(what, "c1st"))
        *got = bx.c1st;
    else if (!strcmp(what, "rows")) {    /* rows measured */
        *got = 0;
        for (i = 0; i < NROWS; i++)
            *got += rows[i].st == R_OK;
    } else if (!strcmp(what, "ok0") || !strcmp(what, "ok1")) {   /* every row of the phase measured, and CPU1 right */
        for (i = what[2] == '0' ? 0u : N0; i < (what[2] == '0' ? N0 : NROWS); i++)
            ok = ok && rows[i].st == R_OK && rows[i].ok && (i >= N0 || rows[i].tk);
        *got = (int)ok;
    } else if (!strcmp(what, "sysex")) {  /* the replies are 7-bit, framed, and not empty */
        uint32_t n = reply('?', 0, f), m;
        ok = n > 20 && f[0] == 0xF0 && f[n - 1] == 0xF7;
        for (i = 1; i + 1 < n; i++)
            ok = ok && f[i] < 0x80;
        m = reply('R', 12, f);
        ok = ok && m > 20 && f[m - 1] == 0xF7 && f[5] == 'B' && f[8] == 'r';
        *got = (int)ok;
    } else
        return 0;
    return 1;
}
#endif
