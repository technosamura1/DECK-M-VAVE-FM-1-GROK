/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors; the plate and the tape echo after OVERWORLD and AMB-1 (C) 2026 Charles Vestal */
/* DECK's audio core (core.h). Its own -O2 unit; float only, no libm (fastmath.h). The sequencer is seq.c, the
 * harmony's pure parts music.c, both part of this unit. */
#include "core.h"
#include "fastmath.h"
#ifdef DK_PROF
double dk_prof_now(void);
double dk_prof[8];
static double prof_t0;
#define PROF_T(k) do { double t_ = dk_prof_now(); if (k) dk_prof[k] += t_ - prof_t0; prof_t0 = t_; } while (0)
#else
#define PROF_T(k) do { } while (0)
#endif

#define BARRIER() __asm__ volatile("" ::: "memory")

dk_song_t dk_song DK_POOL;   /* (the big state in the pool: plain RAM kept for the rest) */
dk_ev_t dk_ev[DK_NEV] DK_POOL;
dk_ctx_t dk_ctx;
dk_metro_t dk_metro = {0, 1, METRO_WOOD, 0, 40, 0};
uint8_t dk_midi_notes = 1, dk_midi_clock = 1;
uint8_t dk_rec_lat = 9;
uint8_t dk_rec_arm = 0;                 /* REC ARM (optional, default OFF): REC arms the tape, PLAY starts it (two steps); OFF: REC records at once */
uint8_t dk_newpart_copy = 1;             /* NEW PART: COPY (the default) or EMPTY */
volatile uint8_t dk_pm_part, dk_pm_x, dk_pm_gen;
uint8_t dk_lock_take = 1;                /* LOCK TAKE: a take keeps the sound it was recorded with */                  /* 5.8 ms the block + ~2.9 ms the note waits for it (APP_BLOCK 256 at 44.1 kHz) */

#include "music.c"

/* ------------------------------------------------------------------ helpers --- */
static uint32_t rng = 2654435769u;
uint32_t dk_rnd(void)
{
    uint32_t x = rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng = x;
    return x;
}
float dk_frand(void) { return (float)(dk_rnd() >> 8) * (1.0f / 16777216.0f); }
float dk_white(void) { return (float)(int32_t)dk_rnd() * (1.0f / 2147483648.0f); }
static int irand(int n) { return n > 0 ? (int)((dk_rnd() >> 8) % (uint32_t)n) : 0; }

#define SN 1024
float dk_sint[SN + 1] DK_POOL;
float dk_sine(float ph) { return dk_sinx(ph); }
float dk_mtof(float n) { return 440.0f * dk_ctx.tune * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
float dk_decay(float tau) { return fm_expf(-1.0f / (fm_maxf(tau, 0.0001f) * DK_SR)); }
float dk_lpk(float fc) { return 1.0f - fm_expf(-6.2831853f * fm_clampf(fc, 20.0f, 18000.0f) * (1.0f / DK_SR)); }
/* the biggest coefficient the Chamberlin filter keeps stable at this damping q (measured: 0.96 at q 1.6 .. 1.86 at q 0.15; this
 * is the safe side of it); above it FILTER's cutoff would blow up */
float dk_svf_fmax(float q) { return -0.5f * q + fm_sqrtf(0.25f * q * q + 2.0f); }
float dk_svf_f(float fc) { return 2.0f * fm_sinf(3.1415927f * fm_clampf(fc, 30.0f, 16000.0f) * (1.0f / DK_SR)); }
float dk_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* ----------------------------------------------------------------- commands --- */
typedef struct {
    uint8_t c, a;
    uint16_t b;
    int32_t d;
} cmd_t;
#define QN 256u
static cmd_t q[QN] DK_POOL;
static volatile uint32_t q_w, q_r;
void dk_cmd(int c, int a, int b, int d)
{
    cmd_t *e;
    if (q_w - q_r >= QN)
        return;
    e = &q[q_w % QN];
    e->c = (uint8_t)c;
    e->a = (uint8_t)a;
    e->b = (uint16_t)b;
    e->d = d;
    BARRIER();
    q_w++;
}
static volatile uint8_t locked;
void dk_lock(void)
{
    locked = 1;
    BARRIER();
}
void dk_unlock(void)
{
    BARRIER();
    locked = 0;
}

/* ------------------------------------------------------------------- state --- */
volatile dk_recev_t dk_recq[DK_RECQ] DK_POOL;
volatile uint32_t dk_recq_w;
volatile uint32_t dk_seen_w;
volatile uint32_t dk_seen[DK_SEEN];
volatile uint8_t dk_playing, dk_recording, dk_counting, dk_rec_trk, dk_pass;
volatile uint32_t dk_fired;
volatile uint8_t dk_rec_mask, dk_rec_pass0, dk_lsolo_clip = 0xFF, dk_lsolo_take;
volatile uint8_t dk_rec_take[DK_NTRACK], dk_rec_floor[DK_NTRACK];
volatile uint16_t dk_rec_tick0;
uint8_t dk_rec_mode;
volatile uint8_t dk_cur_part, dk_next_part = 0xFF, dk_road_i, dk_road_rep, dk_stems;
volatile uint16_t dk_part_tick, dk_part_len = DK_TPB * 4;
volatile uint32_t dk_ticks, dk_clock;
volatile uint8_t dk_beat_flash;
volatile float dk_level[DK_NTRACK], dk_master_level;
volatile uint8_t dk_hush, dk_hushed;
static float hush_g = 1.0f;
volatile uint8_t dk_harm_pcs[4], dk_harm_n, dk_harm_deg;
volatile uint8_t dk_perform;
volatile uint8_t dk_open_n;
volatile uint16_t dk_open_pos[8];
volatile uint8_t dk_open_note[8];
volatile uint8_t dk_open_trk[8];
volatile uint8_t dk_voices_on;
volatile uint8_t dk_live_voices;             /* (voices of the hand's notes, still held) */
volatile uint8_t dk_trk_voices[DK_NTRACK];   /* (voices sounding per track) */
volatile uint8_t dk_voices_frozen;       /* (of them, the ones with a frozen sound: layers) */
volatile uint8_t dk_fill;
volatile uint32_t dk_rec_hits;
volatile uint8_t dk_held_n[DK_NTRACK];
volatile uint8_t dk_held[DK_NTRACK][8];
volatile uint32_t dk_metro_clicks, dk_metro_err_max;

static void seen(int note, int trk, int vel, int src)
{
    uint32_t w = dk_seen_w;
    dk_seen[w % DK_SEEN] = (uint32_t)(note & 0x7F) | (uint32_t)trk << 8 | (uint32_t)(vel & 0x7F) << 16 | (uint32_t)src << 24;
    BARRIER();
    dk_seen_w = w + 1u;
}

/* ----------------------------------------------------------- the parameters --- */
/* a track's values as its voices get them: the knobs, the MACRO, what the clip's motion holds, the modulators */
static float tp[DK_NTRACK][PX_N];
static float t_fxamt[DK_NTRACK], t_fxa[DK_NTRACK], t_fxb[DK_NTRACK], t_rev[DK_NTRACK], t_echo[DK_NTRACK];
static float t_vol[DK_NTRACK], t_pan[DK_NTRACK], t_swing[DK_NTRACK];
static int16_t mot[DK_NTRACK][T_N];           /* the clip's motion: the value in tenths of a percent of the range, 0..1000 (-1 none) */
static uint32_t mot_until[DK_NTRACK][T_N];    /* a p-lock's end (dk_ticks), 0: a move (stays) */
static float modv[DK_NTRACK][2];              /* the modulators now, -1..1 (PEDAL, FOLLOW: 0..1) */
static float dice_v[DK_NTRACK][2], pedal_v[DK_NTRACK][2];
static uint8_t perf_rain;

const dk_engine_t *dk_engine(int t)
{
    int e = dk_song.trk[t].eng;
    return TAPE.eng[(unsigned)e < TAPE.neng ? e : 0];
}

static int base_value(int t, int target, int *lo, int *hi)
{
    const dk_track_t *k = &dk_song.trk[t];
    *lo = 0;
    *hi = 100;
    if (target >= T_P0 && target <= T_P7) {
        const dk_engine_t *e = dk_engine(t);
        int i = target - T_P0;
        if (i >= e->nparams)
            return 0;
        *lo = e->params[i].lo;
        *hi = e->params[i].hi;
        return k->p[i];
    }
    switch (target) {
    case T_ATT: return k->env[0];
    case T_DEC: return k->env[1];
    case T_REL: return k->env[2];
    case T_FXAMT: return k->fxamt;
    case T_FXA: return k->fxa;
    case T_FXB: return k->fxb;
    case T_REV: return k->rev;
    case T_ECHO: return k->echo;
    case T_VOL: return k->vol;
    case T_PAN: *lo = -50; *hi = 50; return k->pan;
    case T_SWING: *hi = 60; return k->swing;
    default: return 0;
    }
}

/* live: with the clip's motion and the modulators (what the track's voices hear); else the knobs and the MACRO only (what a
 * take is frozen with) */
static int param_value(int t, int target, int live)
{
    const dk_track_t *k = &dk_song.trk[t];
    int lo, hi, v = base_value(t, target, &lo, &hi), i;
    float f;
    if (live && mot[t][target] >= 0)          /* the clip's motion: 0..1000 of the range */
        v = lo + (mot[t][target] * (hi - lo) + 500) / 1000;
    f = (float)v;
    for (i = 0; i < 3; i++)                    /* MACRO: amount % of the range at full */
        if (k->mt[i] == target && k->ma[i])
            f += (float)k->ma[i] * 0.01f * (float)k->macro * 0.01f * (float)(hi - lo);
    for (i = 0; live && i < 2; i++)
        if (k->mod[i][0] != MOD_OFF && k->mod[i][0] != MOD_TREMOLO && k->mod[i][3] == target)
            f += modv[t][i] * (float)k->mod[i][1] * 0.01f * (float)(hi - lo) * 0.5f;
    if (live && target == T_P1 && perf_rain && (dk_engine(t)->flags & EF_CONT))
        f += 40.0f;
    f = fm_clampf(f, (float)lo, (float)hi);
    return (int)(f + (f >= 0.0f ? 0.5f : -0.5f));
}
int dk_param_value(int t, int target) { return param_value(t, target, 1); }
int dk_param_static(int t, int target) { return param_value(t, target, 0); }

static uint8_t params_half;
static void params_track(int t)
{
    const dk_engine_t *e = dk_engine(t);
    int i;
    for (i = 0; i < DK_NP; i++)
        tp[t][i] = i < e->nparams ? (float)dk_param_value(t, T_P0 + i) : 0.0f;
    tp[t][PX_ATT] = (float)dk_param_value(t, T_ATT);
    tp[t][PX_DEC] = (float)dk_param_value(t, T_DEC);
    tp[t][PX_REL] = (float)dk_param_value(t, T_REL);
    tp[t][PX_DRIFT] = (float)dk_song.trk[t].drift;
    t_fxamt[t] = (float)dk_param_value(t, T_FXAMT) * 0.01f;
    t_fxa[t] = (float)dk_param_value(t, T_FXA) * 0.01f;
    t_fxb[t] = (float)dk_param_value(t, T_FXB) * 0.01f;
    t_rev[t] = (float)dk_param_value(t, T_REV) * 0.01f;
    t_echo[t] = (float)dk_param_value(t, T_ECHO) * 0.01f;
    t_vol[t] = (float)dk_param_value(t, T_VOL) * 0.01f;
    t_pan[t] = (float)dk_param_value(t, T_PAN) * 0.02f;
    t_swing[t] = (float)dk_param_value(t, T_SWING) * 0.01f;
}
static void params_update_some(int all)
{
    int t;
    params_half ^= 1u;
    for (t = 0; t < DK_NTRACK; t++) {
        if (!all && (t & 1) != params_half)    /* half the tracks a render (each one every 11.6 ms) */
            continue;
        params_track(t);
    }
}
static void params_update(void) { params_update_some(1); }

/* ------------------------------------------------------------------- voices --- */
typedef struct {
    uint8_t on, held, note, trk, src, eng, lane, snd;   /* lane: a drum's sound + 1 (its own settings), 0 none; snd: 0 the track's
                                                         * live sound, 1..7 a frozen one (dk_song.snd[trk][snd - 1]: layers) */
    int32_t gate, delay;
    uint32_t age;
    float ch_l, ch_r, ch_c;                            /* a drum sound's CRUSH: the sample held, its count */
    float st[DK_VSTATE];
} voice_t;
static voice_t vc[DK_NVOICE] DK_POOL;
static uint32_t age_ctr;
static uint8_t shed;
/* a voice taken while it still sounds fades out over a few milliseconds here, from a copy (no click) */
#define NFADE 4
static voice_t fv[NFADE] DK_POOL;
static float fv_g[NFADE];
static void voice_fade(const voice_t *v)
{
    int i, k = 0;
    if (!v->on || v->delay > 0 || TAPE.eng[v->eng]->level(v->st) < 0.002f)
        return;
    for (i = 1; i < NFADE; i++)
        if (fv_g[i] < fv_g[k])
            k = i;
    fv[k] = *v;
    fv_g[k] = 1.0f;
}
static float sc_env;                           /* the kick's envelope (the sidechain, FOLLOW) */

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (dk_midi_notes)
        dk_midi_out(st, d1, d2);
}
static uint32_t ch_of(int t) { return (uint32_t)((dk_song.trk[t].midi_ch ? dk_song.trk[t].midi_ch - 1 : t) & 15); }

/* a frozen sound (a take recorded with it, kept as it was): valid when its place is in use and its engine is a melodic one */
static const dk_snd_t *snd_get(int t, int id)
{
    const dk_snd_t *s;
    if (id < 1 || id > DK_NSND)
        return 0;
    s = &dk_song.snd[t][id - 1];
    if (!s->used || s->eng >= TAPE.neng || (TAPE.eng[s->eng]->flags & (EF_DRUM | EF_CONT)))
        return 0;
    return s;
}
/* what a voice hears of its track: the live values, or a frozen sound's own (b: PX_N floats to build them in) */
static const float *voice_params(const voice_t *v, float *b)
{
    const dk_snd_t *s = v->snd ? snd_get(v->trk, v->snd) : 0;
    const dk_engine_t *e;
    int i;
    if (!s)
        return tp[v->trk];
    e = TAPE.eng[s->eng];
    for (i = 0; i < DK_NP; i++)
        b[i] = i < e->nparams ? (float)s->p[i] : 0.0f;
    b[PX_ATT] = (float)s->env[0];
    b[PX_DEC] = (float)s->env[1];
    b[PX_REL] = (float)s->env[2];
    b[PX_DRIFT] = (float)s->drift;
    return b;
}

static void voice_release(voice_t *v)
{
    float pb[PX_N];
    if (!v->held)
        return;
    v->held = 0;
    v->gate = 0;
    if (!(TAPE.eng[v->eng]->flags & EF_DRUM))   /* a drum rings out its own way (off: choked) */
        TAPE.eng[v->eng]->off(v->st, voice_params(v, pb));
    midi(0x80u | ch_of(v->trk), v->note, 0);
}

static voice_t *voice_get(int t, const dk_engine_t *e, int eng, int snd, int *retrig, int note, int src)
{
    int i, cnt = 0;
    voice_t *best = 0;
    float lo = 1e9f;
    *retrig = 0;
    if (e->flags & EF_MONO) {                  /* one voice: played again while held (legato glides); a sound that
                                                * goes on (the world) whenever it still sounds */
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on && vc[i].trk == t && vc[i].eng == eng && vc[i].snd == snd &&
                (vc[i].held || (e->flags & EF_CONT) || ((e->flags & EF_LEGATO) && vc[i].delay <= 0))) {
                if (vc[i].held)
                    midi(0x80u | ch_of(t), vc[i].note, 0);
                *retrig = vc[i].held ? 2 : (e->flags & EF_CONT) ? 1 : 3;   /* (3: a lead's tail taken on, no restart) */
                return &vc[i];
            }
    }
    if (e->flags & EF_DRUM)                    /* a drum hit again: its own voice cut short */
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on && vc[i].trk == t && vc[i].note == note && vc[i].delay <= 0) {
                float pb[PX_N];
                TAPE.eng[vc[i].eng]->off(vc[i].st, voice_params(&vc[i], pb));
            }
    for (i = 0; i < DK_NVOICE; i++)
        cnt += vc[i].on && vc[i].trk == t;
    if (cnt >= e->max_voices) {                /* the track's own: its quietest let go, else its oldest */
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on && vc[i].trk == t) {
                float l = TAPE.eng[vc[i].eng]->level(vc[i].st) + (vc[i].held ? 10.0f : 0.0f) + (src && !vc[i].src && vc[i].held ? 100.0f : 0.0f);
                if ((int32_t)(age_ctr - vc[i].age) < (int32_t)(DK_SR / 10))
                    l += 40.0f;                /* a note just begun is quiet: it is not the one given up */
                if (l < lo) {
                    lo = l;
                    best = &vc[i];
                }
            }
    } else {
        for (i = 0; i < DK_NVOICE; i++)
            if (!vc[i].on)
                return &vc[i];
        for (i = 0; i < DK_NVOICE; i++) {      /* the pool full: the quietest let go anywhere, else the oldest */
            float l = TAPE.eng[vc[i].eng]->level(vc[i].st);
            if (!vc[i].held && l < lo) {
                lo = l;
                best = &vc[i];
            }
        }
        if (!best) {                           /* the sequencer's oldest first: the hand's voices are the last to go */
            uint32_t top = 0;
            for (i = 0; i < DK_NVOICE; i++) {
                uint32_t score = (age_ctr - vc[i].age) + (vc[i].src ? 0x40000000u : 0u);
                if (!best || score >= top) {
                    top = score;
                    best = &vc[i];
                }
            }
        }
    }
    if (best && best->held)
        midi(0x80u | ch_of(best->trk), best->note, 0);
    if (best)
        voice_fade(best);
    return best;
}

/* a note starts: gate samples held (-1: until let go), delay samples late */
/* the CPU's budget: the voices' cost (the engines' cost, tenths of a microsecond a block on the host) kept under what
 * OVERWORLD's busiest moment cost: over it, the quietest tails go first (faded), then a quiet held voice; never a
 * loud note */
/* the budget moves with the device's measured load (dk_set_load, from the UI): lower when the render runs long */
/* (11 voices plus the effects were 2.8 of 5.8 ms, ~48 %. The cost sum does not count the reverb and the inserts, so a
 * full song can sit "under budget" while the device is already at 90 %: the lead's first notes then crackle. The
 * measured % is what cuts the tails. It starts at 140 so a piano chord and a keys chord still fit, and falls fast
 * once the render is past 64 %.) */
#define DK_BUDGET_MAX 200
#define DK_BUDGET_MIN 50
static volatile int dk_budget = 140;
static volatile int dk_budget_hold;      /* (the host's scenarios: a device that holds its budget low, as a CPU load of 45-62% does) */
static volatile int dk_load_pct;         /* the device's last measure, 0..100 (the host can hold one) */
static volatile int dk_load_hold;
void dk_budget_force(int b)
{
    dk_budget_hold = b;
    if (b)
        dk_budget = b;
}
void dk_load_force(int pct)              /* -1: the device's measure again; else hold this % (a test of the 90 % crackle) */
{
    if (pct < 0) {
        dk_load_hold = 0;
        return;
    }
    dk_load_pct = pct > 100 ? 100 : pct;
    dk_load_hold = 1;
}
void dk_set_load(int pct)
{
    int b = dk_budget;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    if (!dk_load_hold)
        dk_load_pct = pct;
    if (dk_budget_hold)
        return;
    if (pct >= 85)
        b -= 30;                         /* already crackling: down at once, not 14 a frame */
    else if (pct >= 74)
        b -= 16;
    else if (pct >= 64)
        b -= 8;
    else if (pct < 48)
        b += 2;
    dk_budget = b < DK_BUDGET_MIN ? DK_BUDGET_MIN : b > DK_BUDGET_MAX ? DK_BUDGET_MAX : b;
}
int dk_get_budget(void) { return dk_budget; }
/* a voice younger than this (samples) is still in its attack: it is never the one given up (a chord's own first notes, the
 * note the hand has just played: the live note that "did not sound" was the quietest voice of all, at its start) */
#define BUDGET_YOUNG (DK_SR / 2)
/* over the budget, voices go: the tails first (the quietest), then a quiet held voice of the sequencer past its attack, from the
 * track that has the most; for a LIVE note (live: the hand's) the sequencer gives up a loud held voice too: what the hand plays
 * always has a voice. A voice the hand holds is never taken by the sequencer. */
static void budget_make_room(int want, int live)
{
    int hot = shed || dk_load_pct >= 72;     /* the cost sum misses the reverb: a hot device sheds before the new note */
    int melt = shed || dk_load_pct >= 84;    /* already crackling: a fade copy still renders, so the voice just goes */
    int guard = DK_NVOICE;
    while (guard--) {
        int i, total = want, best = -1, most = 0, cnt[DK_NTRACK], nv = 0;
        float lo = 1e9f;
        for (i = 0; i < DK_NTRACK; i++)
            cnt[i] = 0;
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on) {
                total += TAPE.eng[vc[i].eng]->cost;
                nv++;
                if (vc[i].trk < DK_NTRACK)
                    cnt[vc[i].trk]++;
            }
        if (!hot && total <= dk_budget)
            return;
        if (hot && !melt && total <= dk_budget && nv <= 14)
            return;
        if (melt && total <= dk_budget && nv <= 12)
            return;
        for (i = 0; i < DK_NVOICE; i++)          /* 1: a tail (let go) - the quietest */
            if (vc[i].on && !vc[i].held) {
                float l = TAPE.eng[vc[i].eng]->level(vc[i].st);
                if (l < lo) {
                    lo = l;
                    best = i;
                }
            }
        if (best < 0) {                          /* 2: a held voice of the sequencer past its attack, from the track with the most */
            int pass;
            /* (the quiet ones; for a live note any; any too when the device runs over (its budget lowered by the load):
             * the sequencer's oldest loud notes give way rather than the render running past its block - a crackle, or
             * the 5th track's notes lost in it) */
            int any = live || melt || (!dk_budget_hold && dk_budget < 100) || (dk_budget_hold && dk_budget < 60);
            for (pass = 0; pass < 1 + any && best < 0; pass++) {
                most = 0;
                lo = 1e9f;
                for (i = 0; i < DK_NVOICE; i++)
                    if (vc[i].on && vc[i].src && vc[i].trk < DK_NTRACK &&
                        (int32_t)(age_ctr - vc[i].age) >= (int32_t)BUDGET_YOUNG) {
                        float l = TAPE.eng[vc[i].eng]->level(vc[i].st);
                        if ((pass || l <= 0.15f) && (cnt[vc[i].trk] > most || (cnt[vc[i].trk] == most && l < lo))) {
                            most = cnt[vc[i].trk];
                            lo = l;
                            best = i;
                        }
                    }
            }
        }
        if (best < 0)                            /* only loud, young or the hand's notes left: they stay */
            return;
        if (vc[best].held)
            midi(0x80u | ch_of(vc[best].trk), vc[best].note, 0);
        if (melt)
            vc[best].on = 0;
        else {
            voice_fade(&vc[best]);
            vc[best].on = 0;
        }
    }
}

/* between notes the tails stay on and over the budget, so the next instrument has no room. once a chunk, if the
 * mix is heavy, the quietest tails (already let go) fade out: at most NFADE of them. a sparse song is left alone
 * (six felt voices sit under the line). a loud tail stays unless the mix is actually over the budget, or the pool
 * is nearly full.
 * when the DEVICE is hot (the measure, or a block that already ran past 90 %), the cost sum is not trusted: tails
 * go now, and if it is already crackling a few of the sequencer's older notes go too. the hand's notes stay. */
static void tails_trim(void)
{
    int hot = shed || dk_load_pct >= 72, melt = shed || dk_load_pct >= 84;
    int guard = melt ? 8 : hot ? 6 : NFADE;
    while (guard--) {
        int i, total = 0, nv = 0, best = -1;
        float lo = 1e9f;
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on) {
                total += TAPE.eng[vc[i].eng]->cost;
                nv++;
            }
        if (!hot && total <= dk_budget - 14 && nv <= 18)
            return;
        if (hot && !melt && total <= dk_budget - 28 && nv <= 12)
            return;
        if (melt && nv <= 12)
            break;
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on && !vc[i].held) {
                float l = TAPE.eng[vc[i].eng]->level(vc[i].st);
                if (!hot && l > 0.5f && total <= dk_budget && nv <= 20)
                    continue;
                if (l < lo) {
                    lo = l;
                    best = i;
                }
            }
        if (best < 0)
            break;
        if (melt || lo < 0.12f)              /* a tail, and the render is already late: do not keep a copy that still renders */
            vc[best].on = 0;
        else {
            voice_fade(&vc[best]);
            vc[best].on = 0;
        }
    }
    if (!melt)
        return;
    guard = 4;                               /* still packed: the sequencer's older notes, never the hand's */
    while (guard--) {
        int i, best = -1, nv = 0;
        uint32_t top = 0;
        for (i = 0; i < DK_NVOICE; i++)
            nv += vc[i].on;
        if (nv <= 12)
            return;
        for (i = 0; i < DK_NVOICE; i++) {
            uint32_t age, score;
            float l;
            if (!vc[i].on || !vc[i].held || !vc[i].src)
                continue;
            age = age_ctr - vc[i].age;
            if ((int32_t)age < (int32_t)(DK_SR / 10))
                continue;                    /* a note just begun, left alone (its attack is the quietest) */
            l = TAPE.eng[vc[i].eng]->level(vc[i].st);
            score = age + (l < 0.25f ? 0x20000000u : 0u);
            if (best < 0 || score >= top) {
                top = score;
                best = i;
            }
        }
        if (best < 0)
            return;
        midi(0x80u | ch_of(vc[best].trk), vc[best].note, 0);
        vc[best].on = 0;
    }
}

static voice_t *voice_start(int t, int note, float vel, int32_t gate, int32_t delay, int src, int snd)
{
    const dk_snd_t *fs = snd ? snd_get(t, snd) : 0;   /* a frozen sound that is gone plays as the track's own */
    int eng = fs ? fs->eng : dk_song.trk[t].eng;
    const dk_engine_t *e = fs ? TAPE.eng[eng] : dk_engine(t);
    voice_t *v;
    float pb[PX_N];
    const float *pp;
    int retrig, i;
    if (!fs)
        snd = 0;
    if (note < 0 || note > 127 || vel <= 0.0f)
        return 0;
    if (!fs)
        params_track(t);                       /* the note hears this preset. tp[] is refreshed later, and only half
                                                * the tracks a block: a KEYS engine swap otherwise plays one note on
                                                * the previous engine's numbers (GLASS's OCTAVE was TINE's BARK) */
    if ((e->flags & EF_DRUM) && (note == 42 || note == 44 || note == 46))   /* the hats choke each other */
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on && vc[i].trk == t && (vc[i].note == 42 || vc[i].note == 44 || vc[i].note == 46))
                TAPE.eng[vc[i].eng]->off(vc[i].st, tp[t]);   /* (drums are never frozen) */
    budget_make_room(e->cost, !src);
    v = voice_get(t, e, eng, snd, &retrig, note, src);
    if (!v)
        return 0;
    if (!retrig)
        for (i = 0; i < DK_VSTATE; i++)
            v->st[i] = 0.0f;
    v->on = 1;
    v->held = 1;
    v->note = (uint8_t)note;
    v->trk = (uint8_t)t;
    v->src = (uint8_t)src;
    v->eng = (uint8_t)eng;
    v->snd = (uint8_t)snd;
    v->gate = gate;
    if (retrig && v->delay <= 0)              /* a voice already sounding (legato, a tail taken on): never paused - a strum's delay
                                             * on it cut the sound out and back (two clicks) */
        delay = 0;
    v->delay = delay > 0 ? delay : 0;
    v->age = age_ctr + (uint32_t)(delay > 0 ? delay : 0);
    v->lane = 0;
    v->ch_c = 0.0f;
    if ((e->flags & EF_DRUM) && e->nlanes) {          /* a drum sound: its own tune and decay (the kit's p[0], p[1]) */
        int l;
        for (l = 0; l < e->nlanes && l < 8; l++)
            if (e->lane_note[l] == note)
                v->lane = (uint8_t)(l + 1);
        if (v->lane) {
            const dk_lane_t *ln = &dk_song.lane[t][v->lane - 1];
            float pv[PX_N];
            for (i = 0; i < PX_N; i++)
                pv[i] = tp[t][i];
            if (e->nparams > 0)
                pv[0] = fm_minf(100.0f, fm_maxf(0.0f, pv[0] + (float)ln->tune));
            if (e->nparams > 1)
                pv[1] = fm_minf(100.0f, fm_maxf(0.0f, pv[1] + (float)ln->decay));
            e->on(v->st, note, vel, pv, retrig);
            goto started;
        }
    }
    pp = voice_params(v, pb);
    e->on(v->st, note, vel, pp, retrig);
started:
    if ((e->flags & EF_DRUM) && (note == 35 || note == 36))
        sc_env = 1.0f;
    seen(note, t, (int)(vel * 127.0f), src);
    midi(0x90u | ch_of(t), (uint32_t)note, (uint32_t)(vel * 127.0f) < 1u ? 1u : (uint32_t)(vel * 127.0f));
    return v;
}

static void voices_off_note(int t, int note)
{
    int i;
    for (i = 0; i < DK_NVOICE; i++)
        if (vc[i].on && vc[i].held && vc[i].trk == t && vc[i].note == note && vc[i].gate < 0)
            voice_release(&vc[i]);
}
static void voices_release_track(int t, int src_min)
{
    int i;
    for (i = 0; i < DK_NVOICE; i++)
        if (vc[i].on && vc[i].trk == t && vc[i].src >= src_min)
            voice_release(&vc[i]);
}

/* ------------------------------------------------------- reverb: a plate --- */
/* (as OVERWORLD's and AMB-1's, from FoMni) J. Dattorro, "Effect Design, Part 1", JAES 1997; its decay, damping and
 * pre-delay from the song's SPACE (ROOM HALL CAVE SHIMMER) and SLEEP */
/* at half rate (22.05 kHz, as the tape echo): Dattorro's lengths (for 29761 Hz) scaled by 22050/29761 */
#define DS(n) (((n) * 7409 + 5000) / 10000)
#define PRE_N 550
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] DK_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_bwk = 0.55f, pl_damp_l, pl_damp_r, pl_decay = 0.6f, pl_damp = 0.4f, lfo_s, lfo_c = 1.0f;
static int pl_pre = PRE_N, pl_early;
static float pl_rin, pl_wl0, pl_wl1, pl_wr0, pl_wr1, pl_ol, pl_ol2, pl_ol3, pl_or, pl_or2, pl_or3;
static uint8_t pl_odd;
static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720)), er = 0.0f;
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], pl_pre);
    if (pl_early)                               /* CAVE: the walls' first answers */
        er = tap(&dl[L_PRE], 105) * 0.5f + tap(&dl[L_PRE], 243) * 0.35f + tap(&dl[L_PRE], 366) * 0.25f;
    pl_bw += pl_bwk * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0002f * lfo_c, c = lfo_c - 0.0002f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += pl_damp * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, 0.5f);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += pl_damp * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, 0.5f);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066))) + er;
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121))) + er * 0.9f;
}
static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
}
/* SHIMMER: an octave up fed into the plate (two heads gliding through a short line, crossfaded) */
#define SH_N 1024
static float sh_buf[SH_N] DK_POOL;
static int sh_w;
static float sh_ph;
static uint8_t shimmer;
static inline float shimmer_step(float x)
{
    float y, d1, d2, w1, r1, r2;
    int i1, i2;
    sh_buf[sh_w] = x;
    sh_ph += 1.0f / 700.0f;                   /* the heads move at twice the speed: an octave up */
    if (sh_ph >= 1.0f)
        sh_ph -= 1.0f;
    d1 = sh_ph * 700.0f;
    d2 = (sh_ph < 0.5f ? sh_ph + 0.5f : sh_ph - 0.5f) * 700.0f;
    w1 = 1.0f - fm_fabsf(2.0f * sh_ph - 1.0f);
    r1 = (float)sh_w - 700.0f + d1;
    r2 = (float)sh_w - 700.0f + d2;
    if (r1 < 0.0f)
        r1 += (float)SH_N;
    if (r2 < 0.0f)
        r2 += (float)SH_N;
    i1 = (int)r1 & (SH_N - 1);
    i2 = (int)r2 & (SH_N - 1);
    y = sh_buf[i1] * w1 + sh_buf[i2] * (1.0f - w1);
    if (++sh_w >= SH_N)
        sh_w = 0;
    return y;
}

/* ------------------------------------------------------------- tape echo --- */
/* (as OVERWORLD's) a mono line at half rate, 16-bit, up to 1.3 s; each repeat darker and a little worn */
#define ECHO_N 28700
static int16_t echo_buf[ECHO_N] DK_POOL;
static int echo_w;
static float echo_d = 10000.0f, echo_d_t = 10000.0f, echo_fb = 0.4f, echo_lp, echo_half, echo_out;
static uint8_t echo_odd;
static inline float echo_step(float in)
{
    if (echo_odd) {
        float x, rp, fr, a, b;
        int i0, i1;
        echo_d += (echo_d_t - echo_d) * 0.0002f;
        rp = (float)echo_w - echo_d;
        if (rp < 0.0f)
            rp += (float)ECHO_N;
        if (rp >= (float)ECHO_N)         /* (a hair under zero rounds up to exactly ECHO_N: one past the buffer's end) */
            rp = 0.0f;
        i0 = (int)rp;
        fr = rp - (float)i0;
        i1 = i0 + 1 >= ECHO_N ? 0 : i0 + 1;
        a = (float)echo_buf[i0] * (1.0f / 32768.0f);
        b = (float)echo_buf[i1] * (1.0f / 32768.0f);
        x = a + (b - a) * fr;
        echo_lp += 0.28f * (x - echo_lp);
        echo_out = echo_lp;
        x = (in + echo_half) * 0.5f + fm_tanhf(echo_lp * echo_fb * 1.2f) * 0.833f;
        echo_buf[echo_w] = (int16_t)(fm_clampf(x, -0.999f, 0.999f) * 32767.0f);
        if (++echo_w >= ECHO_N)
            echo_w = 0;
    } else
        echo_half = in;
    echo_odd ^= 1u;
    return echo_out;
}

/* ------------------------------------------------------------ the metronome --- */
static struct {
    float env, ph, inc, inc2, ton, nz, lp, mul;
    uint8_t kind;
} clk;
static void metro_click(int accent)
{
    static const float F[METRO_N] = {2100.0f, 880.0f, 7000.0f, 560.0f, 1700.0f};
    float f = F[dk_metro.sound % METRO_N] * (accent ? 1.5f : 1.0f);
    clk.kind = dk_metro.sound % METRO_N;
    clk.env = (accent ? 1.0f : 0.6f) * (float)dk_metro.vol * 0.01f;
    clk.ph = 0.0f;
    clk.inc = f * (1.0f / DK_SR);
    clk.inc2 = f * 1.48f * (1.0f / DK_SR);
    clk.mul = dk_decay(clk.kind == METRO_WOOD ? 0.035f : clk.kind == METRO_COWBELL ? 0.09f : clk.kind == METRO_HAT ? 0.02f : 0.012f);
    clk.nz = clk.kind == METRO_HAT || clk.kind == METRO_RIM ? 1.0f : 0.0f;
    dk_metro_clicks++;
    dk_beat_flash = (uint8_t)(accent ? 2 : 1);
}
static inline float metro_step(void)
{
    float s = 0.0f;
    if (clk.env < 1e-4f)
        return 0.0f;
    clk.ph = clk.ph + clk.inc;
    if (clk.ph >= 1.0f)
        clk.ph -= 1.0f;
    switch (clk.kind) {
    case METRO_WOOD: s = dk_sine(clk.ph) * 0.8f + dk_sine(clk.ph * 2.76f) * 0.2f; break;   /* a soft wood block */
    case METRO_COWBELL: s = (clk.ph < 0.5f ? 0.3f : -0.3f) + dk_sine(clk.ph * 1.48f) * 0.4f; break;
    case METRO_HAT: clk.lp += 0.6f * (dk_white() - clk.lp); s = (dk_white() - clk.lp) * 0.6f; break;
    case METRO_RIM: s = dk_sine(clk.ph) * 0.6f + dk_white() * 0.25f; break;
    default: s = dk_sine(clk.ph) * 0.9f; break;
    }
    s *= clk.env * 0.35f;
    clk.env *= clk.mul;
    return s;
}

/* -------------------------------------------------------------- the tracks --- */
static dk_fxst_t fxs[DK_NTRACK] DK_POOL;
static uint8_t fx_prev[DK_NTRACK];
static float fx_byp[DK_NTRACK];            /* the insert's bypass, faded: 0 in, 1 out */
static int16_t fxbuf[DK_NTRACK][DK_FXBUF] DK_POOL;
static float bus_st[DK_NTRACK][16];
static float g_cur[DK_NTRACK][2];             /* the gain now (left, right): glides to the knob */
static int32_t tail[DK_NTRACK];               /* samples the effect still rings after the last voice */
static uint8_t t_soloed, drop_mask, stems_solo = 0xFF, perf_mute_drums, perf_echo;
static float master_lp_k = 1.0f, master_lp_l, master_lp_r, master_lp_l2, master_lp_r2;
static float mixl[DK_BLK], mixr[DK_BLK], rsend[DK_BLK], esend[DK_BLK], tl[DK_BLK], tr[DK_BLK];
static float lvl_acc[DK_NTRACK];

static int track_audible(int t)
{
    const dk_track_t *k = &dk_song.trk[t];
    int pm = dk_song.part[(dk_playing ? dk_cur_part : dk_song.cur_part) & 7].mute >> t & 1u;   /* muted in the part playing */
    if (stems_solo != 0xFF)                    /* STEMS: the one track alone, silent where the song has it muted in the part */
        return t == stems_solo && !pm;
    if ((drop_mask >> t & 1u) || pm)
        return 0;
    if (perf_mute_drums && (dk_engine(t)->flags & EF_DRUM))
        return 0;
    if (t_soloed && !k->solo)
        return 0;
    return 1;
}

/* why a track the hand plays is not heard (the "KEYS silent" the device showed and the host did not): 0 nothing found,
 * 1 muted in the part playing, 2 another track soloed, 3 dropped (a ROAD block), 4 its VOL low, 5 its VOL low by a curve
 * (automation), 6 a tremolo, 7 the drums muted by a perform key */
int dk_silent_why(int t)
{
    const dk_track_t *k = &dk_song.trk[t & 7];
    int i;
    if (t < 0 || t >= DK_NTRACK)
        return 0;
    if (dk_song.part[(dk_playing ? dk_cur_part : dk_song.cur_part) & 7].mute >> t & 1u)
        return 1;
    if (t_soloed && !k->solo)
        return 2;
    if (drop_mask >> t & 1u)
        return 3;
    if (perf_mute_drums && (dk_engine(t)->flags & EF_DRUM))
        return 7;
    if (t_vol[t] < 0.12f)
        return mot[t][T_VOL] >= 0 ? 5 : 4;
    for (i = 0; i < 2; i++)
        if (k->mod[i][0] == MOD_TREMOLO && k->mod[i][1] > 80)
            return 6;
    return 0;
}

/* a drum sound's own settings beyond its level, tune and decay (those at its start): any to apply while it rings */
static int lane_fx(const dk_lane_t *ln) { return ln->level || ln->pan || ln->rev || ln->echo || ln->crush; }
static float xrev[DK_BLK] DK_POOL, xecho[DK_BLK] DK_POOL, atmo_vl[DK_BLK] DK_POOL, atmo_vr[DK_BLK] DK_POOL;
static int any_x = 1;
static void lane_crush(voice_t *v, float *l, float *r, int n, int amt)
{
    float hold = 1.0f + (float)amt * 0.11f, q = fm_exp2f(12.0f - (float)amt * 0.08f), iq = 1.0f / q;   /* 1..12x, 12..4 bits */
    int j;
    for (j = 0; j < n; j++) {
        v->ch_c -= 1.0f;
        if (v->ch_c <= 0.0f) {
            v->ch_c += hold;
            v->ch_l = (float)(int)(l[j] * q) * iq;
            v->ch_r = (float)(int)(r[j] * q) * iq;
        }
        l[j] = v->ch_l;
        r[j] = v->ch_r;
    }
}

static void render_tracks(uint32_t n)
{
    int t, i;
    uint32_t j;
    t_soloed = 0;
    for (t = 0; t < DK_NTRACK; t++)
        t_soloed |= dk_song.trk[t].solo;
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_engine_t *e = dk_engine(t);
        const dk_track_t *k = &dk_song.trk[t];
        int any = 0, fx = k->fx;
        unsigned byp = DK_BYP(t);              /* SOUND's chain: the stages switched off (FX held + WHITE 1-4) */
        float gt, gl, gr, dl_, dr_, pk = 0.0f, rv = (byp & DK_BYP_REV) ? 0.0f : t_rev[t],
              ec = ((byp & DK_BYP_ECHO) ? 0.0f : t_echo[t]) + (perf_echo ? 0.9f : 0.0f);
        for (i = 0; i < DK_NVOICE; i++)
            if (vc[i].on && vc[i].trk == t) {
                any = 1;
                break;
            }
        for (i = 0; i < NFADE; i++)
            any |= fv_g[i] > 0.0f && fv[i].trk == t;
        if (any)                               /* an effect (or the kit's grain) rings on a while; else nothing to do */
            tail[t] = fx > 0 ? (int32_t)(1.5f * DK_SR) : e->bus ? (int32_t)(0.3f * DK_SR) : 0;
        if (any)
            ;
        else if (tail[t] > 0)
            tail[t] -= (int32_t)n;
        else {
            lvl_acc[t] *= 0.9f;
            dk_level[t] = lvl_acc[t];
            continue;
        }
        for (j = 0; j < n; j++)
            tl[j] = tr[j] = 0.0f;
        if (any_x)
            for (j = 0; j < n; j++)
                xrev[j] = xecho[j] = 0.0f;
        any_x = 0;
        for (i = 0; i < DK_NVOICE; i++) {
            voice_t *v = &vc[i];
            uint32_t j0 = 0;
            const dk_engine_t *ve;
            float vpb[PX_N];
            if (!v->on || v->trk != t)
                continue;
            if (!v->snd && v->eng != k->eng) { /* its track took another engine: let it go (a frozen sound's voice stays) */
                if (v->held)
                    midi(0x80u | ch_of(t), v->note, 0);
                voice_fade(v);
                v->on = 0;
                continue;
            }
            ve = TAPE.eng[v->eng];
            if (v->delay > 0) {
                if ((uint32_t)v->delay >= n) {
                    v->delay -= (int32_t)n;
                    continue;
                }
                j0 = (uint32_t)v->delay;
                v->delay = 0;
            }
            if (v->held && v->gate > 0 && (v->gate -= (int32_t)n) <= 0)
                voice_release(v);
            if (v->lane && lane_fx(&dk_song.lane[t][v->lane - 1])) {   /* a drum sound with its own pan, sends, crush */
                const dk_lane_t *ln = &dk_song.lane[t][v->lane - 1];
                static float vl[DK_BLK] DK_POOL, vr[DK_BLK] DK_POOL;
                float lg = fm_db2lin((float)ln->level * 0.5f);
                float pl = lg * fm_minf(1.0f, 1.0f - (float)ln->pan * 0.02f), pr = lg * fm_minf(1.0f, 1.0f + (float)ln->pan * 0.02f);
                float rs = (float)ln->rev * 0.01f, es = (float)ln->echo * 0.01f;
                int alive;
                for (j = j0; j < n; j++)
                    vl[j] = vr[j] = 0.0f;
                alive = ve->render(v->st, vl + j0, vr + j0, (int)(n - j0), tp[t]);
                if (ln->crush)                 /* CRUSH: the rate held down, the bits fewer */
                    lane_crush(v, vl + j0, vr + j0, (int)(n - j0), ln->crush);
                for (j = j0; j < n; j++) {
                    tl[j] += vl[j] * pl;
                    tr[j] += vr[j] * pr;
                    xrev[j] += (vl[j] + vr[j]) * rs * lg;
                    xecho[j] += (vl[j] + vr[j]) * es * lg;
                }
                any_x = 1;
                if (!alive) {
                    if (v->held)
                        midi(0x80u | ch_of(t), v->note, 0);
                    v->on = 0;
                } else if (shed && !v->held && ve->level(v->st) < 0.05f)
                    v->on = 0;
                continue;
            }
            if (!ve->render(v->st, tl + j0, tr + j0, (int)(n - j0), voice_params(v, vpb))) {
                if (v->held)
                    midi(0x80u | ch_of(t), v->note, 0);
                v->on = 0;
            } else if (shed && !v->held && ve->level(v->st) < 0.05f)
                v->on = 0;
        }
        for (i = 0; i < NFADE; i++) {          /* the voices fading out */
            static float fl_[DK_BLK], fr_[DK_BLK];
            float fpb[PX_N];
            float g = fv_g[i], dg = 1.0f / (0.006f * DK_SR);
            if (g <= 0.0f || fv[i].trk != t)
                continue;
            for (j = 0; j < n; j++)
                fl_[j] = fr_[j] = 0.0f;
            TAPE.eng[fv[i].eng]->render(fv[i].st, fl_, fr_, (int)n, voice_params(&fv[i], fpb));
            for (j = 0; j < n && g > 0.0f; j++, g -= dg) {
                tl[j] += fl_[j] * g;
                tr[j] += fr_[j] * g;
            }
            fv_g[i] = g;
        }
        PROF_T(1);
        if (e->bus)
            e->bus(bus_st[t], tl, tr, (int)n, tp[t]);
        PROF_T(2);
        if (fx > 0 && fx < TAPE.nfx && TAPE.fx[fx]->process) {
            /* the insert, with its bypass: a fade between the dry and the wet over the block (no click), nothing
             * processed once fully bypassed */
            float bt = (byp & DK_BYP_FX) ? 1.0f : 0.0f;
            if (bt < 1.0f || fx_byp[t] < 1.0f) {
                static float dl2[DK_BLK], dr2[DK_BLK];
                int fade = fx_byp[t] != bt;
                if (fade)
                    for (j = 0; j < n; j++) {
                        dl2[j] = tl[j];
                        dr2[j] = tr[j];
                    }
                if (fx_prev[t] != (uint8_t)fx) {          /* another effect than last time: its state is not this one's (the
                                                          * LOFI's held samples are not the TAPE's wow phases: they made the
                                                          * delay jump about) */
                    for (j = 0; j < 28; j++)
                        fxs[t].f[j] = 0.0f;
                    fx_prev[t] = (uint8_t)fx;
                }
                TAPE.fx[fx]->process(&fxs[t], tl, tr, (int)n, t_fxamt[t], t_fxa[t], t_fxb[t]);
                if (fade) {
                    float g = fx_byp[t], dg = (bt - g) / (float)n;
                    for (j = 0; j < n; j++, g += dg) {
                        tl[j] += (dl2[j] - tl[j]) * g;
                        tr[j] += (dr2[j] - tr[j]) * g;
                    }
                    fx_byp[t] = bt;
                }
            }
        }
        PROF_T(3);
        /* the gain: VOL's square, its trim, the modulators' tremolo, the sidechain on the pads */
        gt = track_audible(t) ? t_vol[t] * t_vol[t] * 2.0f * fm_db2lin((float)k->trim * 0.5f) : 0.0f;
        for (i = 0; i < 2; i++)
            if (k->mod[i][0] == MOD_TREMOLO)
                gt *= 1.0f - (float)k->mod[i][1] * 0.01f * (0.5f + 0.5f * modv[t][i]);
        if (k->role == R_PAD || e->role == R_PAD)
            gt *= 1.0f - (float)dk_song.sidechain * 0.01f * sc_env * 0.8f;
        gl = gt * fm_minf(1.0f, 1.0f - t_pan[t]);
        gr = gt * fm_minf(1.0f, 1.0f + t_pan[t]);
        dl_ = (gl - g_cur[t][0]) / (float)n;
        dr_ = (gr - g_cur[t][1]) / (float)n;
        for (j = 0; j < n; j++) {
            float l, r;
            g_cur[t][0] += dl_;
            g_cur[t][1] += dr_;
            l = tl[j] * g_cur[t][0];
            r = tr[j] * g_cur[t][1];
            mixl[j] += l;
            mixr[j] += r;
            rsend[j] += (l + r) * rv;
            esend[j] += (l + r) * ec;
            if (any_x) {                       /* the drum sounds' own sends, at the track's level */
                float g = (g_cur[t][0] + g_cur[t][1]) * 0.5f;
                rsend[j] += (byp & DK_BYP_REV) ? 0.0f : xrev[j] * g;
                esend[j] += (byp & DK_BYP_ECHO) ? 0.0f : xecho[j] * g;
            }
            pk = fm_maxf(pk, fm_fabsf(l) + fm_fabsf(r));
        }
        lvl_acc[t] = fm_maxf(pk * 0.5f, lvl_acc[t] * 0.94f);
        dk_level[t] = lvl_acc[t];
    }
}

/* ----------------------------------------------------------------- the sequencer --- */
static float tick_len = 1000.0f, tick_left;
static float wow_ph, flutter_ph, stop_f = 1.0f, stop_to = 1.0f;
static float master_acc;
#include "seq.c"

/* ------------------------------------------------------------------- render --- */
/* ATMO: its layers, each a voice kept going while its level is up (faded out when it goes down) */
#define NATMO 6
static float atmo_st[NATMO][DK_VSTATE] DK_POOL, atmo_p[NATMO][PX_N], atmo_g[NATMO];
static uint8_t atmo_on[NATMO];
static void atmo_render(uint32_t n)
{
    static float al[DK_BLK] DK_POOL, ar[DK_BLK] DK_POOL;
    int i, any = 0;
    uint32_t j;
    float master = dk_song.atmo_lvl ? (float)(dk_song.atmo_lvl - 1) * 0.01f : 1.0f;
    for (i = 0; i < NATMO && i < TAPE.natmo; i++) {
        const dk_atmo_t *a = &TAPE.atmo[i];
        const dk_engine_t *e = TAPE.eng[a->eng % TAPE.neng];
        float want = (float)dk_song.atmo[i] * 0.01f, g0 = atmo_g[i], dg;
        int k;
        if (want > 0.0f && !atmo_on[i]) {     /* up from nothing: the voice starts (it fades in itself) */
            for (k = 0; k < DK_VSTATE; k++)
                atmo_st[i][k] = 0.0f;
            for (k = 0; k < PX_N; k++)
                atmo_p[i][k] = k < 3 ? (float)a->p[k] : k == PX_DRIFT ? 0.0f : 50.0f;
            e->on(atmo_st[i], 60, 1.0f, atmo_p[i], 0);
            atmo_on[i] = 1;
        } else if (want <= 0.0f && atmo_on[i] == 1) {
            e->off(atmo_st[i], atmo_p[i]);
            atmo_on[i] = 2;                    /* letting go: until it has faded */
        }
        if (!atmo_on[i])
            continue;
        if (!any)
            for (j = 0; j < n; j++)
                al[j] = ar[j] = 0.0f;
        any = 1;
        for (j = 0; j < n; j++)
            atmo_vl[j] = atmo_vr[j] = 0.0f;
        if ((!e->render(atmo_st[i], atmo_vl, atmo_vr, (int)n, atmo_p[i]) || e->level(atmo_st[i]) < 0.0005f) &&
            atmo_on[i] == 2)
            atmo_on[i] = 0;                    /* let go and faded: no more of it */
        atmo_g[i] = want > 0.0f ? want * want * a->gain * master : g0;
        dg = (atmo_g[i] - g0) / (float)n;
        for (j = 0; j < n; j++, g0 += dg) {
            al[j] += atmo_vl[j] * g0;
            ar[j] += atmo_vr[j] * g0;
        }
    }
    if (any)
        for (j = 0; j < n; j++) {
            mixl[j] += al[j];
            mixr[j] += ar[j];
            rsend[j] += (al[j] + ar[j]) * 0.12f;   /* a touch of the room on it */
        }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    float pk = 0.0f, dust = (float)dk_song.dust * 0.01f, slp = (float)dk_song.sleep * 0.01f;
    age_ctr += n;                               /* (the voices' age: samples) */
    tails_trim();
    if (dk_perform == PF_SLEEP)
        slp = 1.0f;
    if (dk_perform == PF_VINYLSTOP || dk_perform == PF_REWIND)
        dust = fm_maxf(dust, 0.7f);
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    PROF_T(0);
    render_tracks(n);
    atmo_render(n);
    dk_ctx.tape = (float)dk_song.tapesim * 0.01f;
    dk_ctx.tape_off = (uint8_t)DK_BYP_TAPE();
    PROF_T(4);
    for (j = 0; j < n; j++) {
        float wl, wr, e, l, r, rin;
        e = echo_step(esend[j] * 0.55f);
        rin = (rsend[j] * 0.5f + e * 0.3f) * 0.55f;
        if (pl_odd) {                           /* the plate at half rate: two samples in, one step, eased out */
            rin = (rin + pl_rin) * 0.5f;
            if (shimmer)
                rin += shimmer_step(rin) * 0.6f;
            pl_wl0 = pl_wl1;
            pl_wr0 = pl_wr1;
            reverb(rin, &pl_wl1, &pl_wr1);
            wl = pl_wl1;
            wr = pl_wr1;
        } else {
            pl_rin = rin;
            wl = (pl_wl0 + pl_wl1) * 0.5f;
            wr = (pl_wr0 + pl_wr1) * 0.5f;
        }
        pl_odd ^= 1u;
        pl_ol += 0.44f * (wl - pl_ol);          /* the half rate's images gone: three poles near 4 kHz */
        pl_ol2 += 0.44f * (pl_ol - pl_ol2);
        pl_ol3 += 0.44f * (pl_ol2 - pl_ol3);
        pl_or += 0.44f * (wr - pl_or);
        pl_or2 += 0.44f * (pl_or - pl_or2);
        pl_or3 += 0.44f * (pl_or2 - pl_or3);
        wl = pl_ol3 * 1.12f;
        wr = pl_or3 * 1.12f;
        l = (mixl[j] + e * 0.8f + wl * 1.1f) * gain;
        r = (mixr[j] + e * 0.8f + wr * 1.1f) * gain;
        if (master_lp_k < 0.999f) {            /* the performance's low-pass (the music behind a wall) */
            master_lp_l += master_lp_k * (l - master_lp_l);
            master_lp_l2 += master_lp_k * (master_lp_l - master_lp_l2);
            master_lp_r += master_lp_k * (r - master_lp_r);
            master_lp_r2 += master_lp_k * (master_lp_r - master_lp_r2);
            l = master_lp_l2;
            r = master_lp_r2;
        } else {
            master_lp_l = master_lp_l2 = l;
            master_lp_r = master_lp_r2 = r;
        }
        l *= stop_f > 0.3f ? 1.0f : stop_f * 3.3f;
        r *= stop_f > 0.3f ? 1.0f : stop_f * 3.3f;
        mixl[j] = l;
        mixr[j] = r;
        pk = fm_maxf(pk, fm_fabsf(l));
    }
    PROF_T(5);
    if (TAPE.master)
        TAPE.master(mixl, mixr, (int)n, dust, slp);
    PROF_T(6);
    for (j = 0; j < n; j++) {
        float m = metro_step() * gain * 1.6f, l = fm_tanhf(mixl[j] + m), r = fm_tanhf(mixr[j] + m);
        if (dk_hush)                           /* a save coming: down to nothing in ~6 ms, back the same */
            hush_g = fm_maxf(0.0f, hush_g - (1.0f / 256.0f));
        else if (hush_g < 1.0f)
            hush_g = fm_minf(1.0f, hush_g + (1.0f / 256.0f));
        l *= hush_g;
        r *= hush_g;
        out[2 * j] = (int32_t)(l * 8300000.0f);
        out[2 * j + 1] = (int32_t)(r * 8300000.0f);
    }
    if (dk_hush && hush_g <= 0.0f) {
        if (dk_hushed < 255)
            dk_hushed++;
    } else
        dk_hushed = 0;
    sc_env *= fm_expf(-(float)n / (0.16f * DK_SR));
    master_acc = fm_maxf(pk, master_acc * 0.97f);
    dk_master_level = master_acc;
}

void dk_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f);
    float drift = 0.0012f;
    int vo = 0, i;
    drain();
    params_update_some(0);                     /* the knobs: half the tracks a render (all when the clip moves one) */
    /* the tape's slow wow and its flutter (DUST deepens them); a tape stop's fall */
    wow_ph += (float)n * (0.37f / DK_SR);
    if (wow_ph >= 1.0f)
        wow_ph -= 1.0f;
    flutter_ph += (float)n * (5.3f / DK_SR);
    if (flutter_ph >= 1.0f)
        flutter_ph -= 1.0f;
    dk_ctx.lfo_slow += (float)n * (0.07f / DK_SR);
    if (dk_ctx.lfo_slow >= 1.0f)
        dk_ctx.lfo_slow -= 1.0f;
    dk_ctx.lfo_mid += (float)n * (0.9f / DK_SR);
    if (dk_ctx.lfo_mid >= 1.0f)
        dk_ctx.lfo_mid -= 1.0f;
    dk_ctx.lfo_fast += (float)n * (5.2f / DK_SR);
    if (dk_ctx.lfo_fast >= 1.0f)
        dk_ctx.lfo_fast -= 1.0f;
    dk_ctx.trem += (float)n * (4.6f / DK_SR);
    if (dk_ctx.trem >= 1.0f)
        dk_ctx.trem -= 1.0f;
    stop_f += (stop_to - stop_f) * fm_minf(1.0f, (float)n * (stop_to < stop_f ? 2.6f : 6.0f) / DK_SR);
    {
        float d = (float)dk_song.dust * 0.01f;
        dk_ctx.wow = (1.0f + (drift + 0.0024f * d) * dk_sine(wow_ph) + 0.0005f * d * dk_sine(flutter_ph)) * stop_f;
        if (dk_perform == PF_REWIND)
            dk_ctx.wow *= 1.6f + 0.5f * dk_sine(flutter_ph * 3.0f);
    }
    while (n) {
        uint32_t k = n > DK_BLK ? DK_BLK : n;
        if (tick_left <= 0.0f && !locked) {
            while (tick_left <= 0.0f) {        /* on time (or catching up after a lock) */
                seq_tick();
                tick_left += tick_len;
            }
        }
        if (!locked && (float)k > tick_left)
            k = (uint32_t)tick_left + 1u;
        if (k > n)
            k = n;
        if (locked && k > 32u)
            k = 32u;
        tick_left -= (float)k;
        render_chunk(out, k, gain);
        out += 2 * k;
        n -= k;
    }
    {
        int fz = 0, lv = 0;
        for (i = 0; i < DK_NTRACK; i++)
            dk_trk_voices[i] = 0;
        for (i = 0; i < DK_NVOICE; i++) {
            if (vc[i].on && vc[i].trk < DK_NTRACK)
                dk_trk_voices[vc[i].trk]++;
            lv += vc[i].on && vc[i].held && !vc[i].src;
            vo += vc[i].on;
            fz += vc[i].on && vc[i].snd;
        }
        dk_voices_frozen = (uint8_t)fz;
        dk_live_voices = (uint8_t)lv;
    }
    dk_voices_on = (uint8_t)vo;
    shed = 0;
}

void dk_shed(void)
{
    shed = 1;                                /* the block just rendered past 90 %: the next one drops tails first */
    if (!dk_budget_hold && dk_budget > 120)
        dk_budget = dk_budget - 40 < 120 ? 120 : dk_budget - 40;
}

void dk_init(void)
{
    int i, t;
    for (i = 0; i <= SN; i++)
        dk_sint[i] = fm_sinf(6.2831853f * (float)i / (float)SN);
    dk_ctx.tune = 1.0f;
    dk_ctx.wow = 1.0f;
    plate_init();
    for (i = 0; i < ECHO_N; i++)
        echo_buf[i] = 0;
    for (t = 0; t < DK_NTRACK; t++) {
        fxs[t].buf = fxbuf[t];
        fxs[t].w = 0;
        for (i = 0; i < 28; i++)
            fxs[t].f[i] = 0.0f;
        fx_prev[t] = 0;
        for (i = 0; i < T_N; i++)
            mot[t][i] = -1;
    }
    dk_metro.sound = TAPE.metro_sound;
    if (TAPE.init)
        TAPE.init();
    seq_init();
}
