/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The sequencer, part of the core's unit (core.c): the clock (free, and the transport's), the parts and the road,
 * the clips' playback with the road's tricks, recording (LIVE, ENDLESS, the generators', BAKE), the count-in and the
 * metronome, the generators (ARP, HOLD, FINGER, DRIVE, TOMBOLA), the harmony, the modulators, the performance
 * effects, STEMS, MIDI clock. Everything here runs in the render, a tick at a time. */

static uint32_t gt;                            /* the grid's clock: the transport's ticks, or the free clock */
static uint16_t part_tick;
static void rec_on_now(void)           /* the take starts: where in the clip (REPLACE erases from here) */
{
    dk_rec_tick0 = part_tick;
    dk_recording = 1;
}
static uint8_t cur_part, queued = 0xFF, tricks, block_last;
/* a part's track mutes changed while playing (C_PMUTE) wait for the bar's end: dk_pm_x (the tracks that flip) in part dk_pm_part */
static void pm_apply(void)
{
    if (dk_pm_x) {
        dk_song.part[dk_pm_part & 7].mute = (uint8_t)((dk_song.part[dk_pm_part & 7].mute ^ dk_pm_x) & DK_PMUTE_ALL);
        dk_pm_x = 0;
        dk_pm_gen++;
    }
}
static int32_t count_left;
static uint8_t rec_start_after, rec_wait_note, rec_bake;
static uint8_t rec_nocomp;                     /* the note that starts a take is at 0, not made up for the latency */
static uint16_t endless_n;
static uint8_t open_nc[128];                  /* ... and it began at the take's start, not made up for the latency */
static int16_t open_pos[128];                  /* recording: the clip tick a held note began (-1 none) */
static uint8_t open_vel[128], open_trk[128];
static uint8_t arp_step[DK_NTRACK], arp_order[DK_NTRACK][8], arp_n[DK_NTRACK], arp_keys[DK_NTRACK];
volatile uint8_t dk_arp_rpos[DK_NTRACK], dk_arp_hit[DK_NTRACK];   /* for the page: where the rhythm is, a hit just now */
static uint8_t hold_keys[DK_NTRACK];
static uint16_t finger_held[DK_NTRACK];        /* FINGER: the patterns held (a bit each) */
static uint8_t finger_var[DK_NTRACK];          /* its variations: 1 HALF 2 DOUBLE 4 ACCENT 8 GHOST 16 SHIFT- 32 SHIFT+ */
static uint8_t perf_half, perf_rev, perf_stutter, stems_pass, stems_gap;
static uint16_t stutter_at;
static float lp_target = 1.0f;
static dk_harm_t harm;
static int32_t user_hold;                      /* ticks a chord you played holds the harmony */
static uint8_t prog_i, prog_row;
static uint32_t held_pcs[DK_NTRACK], win_pcs, win_at;
static uint32_t part_loops, bar_n;
static struct {
    float x, y, vx, vy;
    uint8_t note, on;
} ball[DK_NBALL];
static float tmb_a;

static int part_len_of(int p)
{
    int b = dk_song.part[p & 7].bars;
    return (b < 1 ? 1 : b > 16 ? 16 : b) * DK_TPB;
}
static int clip_len_of(int p, int t)
{
    const dk_clip_t *c = CLIP(p, t);
    return c->len ? c->len : part_len_of(p);
}
static float tempo_of(void) { return (float)(dk_song.tempo < 30 ? 30 : dk_song.tempo); }
static void tempo_set(void)
{
    static const float ET[4] = {0.75f, 1.0f, 0.5f, 1.5f};   /* the echo: a dotted eighth, a quarter, an eighth, a dotted quarter */
    float d, slp = (float)dk_song.sleep * 0.01f;
    tick_len = DK_SR * 60.0f / (tempo_of() * (float)DK_PPQ);
    dk_ctx.spb = DK_SR * 60.0f / tempo_of();
    d = ET[dk_song.echo_time & 3] * (1.0f + 0.6f * slp) * 60.0f / tempo_of() * (DK_SR * 0.5f);
    while (d > (float)(ECHO_N - 8))
        d *= 0.5f;
    echo_d_t = d;
    echo_fb = fm_minf(0.82f, 0.40f + 0.25f * slp + (perf_echo ? 0.25f : 0.0f));
    /* the plate: ROOM HALL CAVE SHIMMER, SLEEP longer */
    switch (dk_song.space & 3) {
    case 0: pl_decay = 0.48f; pl_damp = 0.55f; pl_pre = 90; pl_bwk = 0.6f; pl_early = 0; shimmer = 0; break;
    case 2: pl_decay = 0.84f; pl_damp = 0.70f; pl_pre = PRE_N; pl_bwk = 0.35f; pl_early = 1; shimmer = 0; break;
    case 3: pl_decay = 0.82f; pl_damp = 0.22f; pl_pre = 350; pl_bwk = 0.7f; pl_early = 0; shimmer = 1; break;
    default: pl_decay = 0.70f; pl_damp = 0.40f; pl_pre = 450; pl_bwk = 0.55f; pl_early = 0; shimmer = 0; break;
    }
    pl_decay = fm_minf(0.9f, pl_decay + 0.08f * slp);
    pl_damp = fm_maxf(0.15f, pl_damp - 0.1f * slp);
    /* the one-poles' coefficients for the half rate (the same corner in Hz) */
    pl_damp = 1.0f - (1.0f - pl_damp) * (1.0f - pl_damp);
    pl_bwk = 1.0f - (1.0f - pl_bwk) * (1.0f - pl_bwk);
}

/* ------------------------------------------------------------- the harmony --- */
static int is_minorish(void)
{
    int m = dk_song.mode;
    return m == 1 || m == 2 || m == 3 || m == 6 || m == 7 || m == 9 || m == 10;
}
static void harm_publish(void)
{
    int i;
    for (i = 0; i < 4; i++)
        dk_harm_pcs[i] = harm.pcs[i < harm.n ? i : 0];
    dk_harm_n = harm.n;
    dk_harm_deg = harm.deg;
}
static void harm_notes(const int *nt, int n, int deg)
{
    int i;
    harm.n = (uint8_t)(n > 4 ? 4 : n);
    for (i = 0; i < harm.n; i++)
        harm.pcs[i] = (uint8_t)(((nt[i] % 12) + 12) % 12);
    harm.deg = (uint8_t)deg;
    harm.fresh = 1;
    harm_publish();
}
static void harm_degree(int code)              /* a progression's step: a degree, or (10..13) a borrowed chord */
{
    int nt[8], n;
    harm.root = dk_song.root % 12;
    harm.mode = dk_song.mode;
    if (code >= 10 && TAPE.borrow_major) {
        const dk_borrow_t *b = &(is_minorish() ? TAPE.borrow_minor : TAPE.borrow_major)[(code - 10) & 3];
        int i;
        for (n = 0, i = 0; i < 7 && b->iv[i] != 99; i++)
            if (b->iv[i] >= 0)
                nt[n++] = 60 + harm.root + b->off + b->iv[i];
        harm_notes(nt, n, 0);
        return;
    }
    n = dk_chord(harm.root, harm.mode, code, TAPE.style_chord ? TAPE.style_chord[dk_song.style % TAPE.nstyle] : CH_7,
                 1, 1, nt, 60);
    harm_notes(nt, n, code);
}
static int chord_bars(void) { return TAPE.style_bars ? TAPE.style_bars[dk_song.style % TAPE.nstyle] : 2; }
static void harm_regrow(void)
{
    prog_i = 0;
    prog_row = (uint8_t)(dk_song.style % (TAPE.nprog ? TAPE.nprog : 1));
    user_hold = 0;
    if (TAPE.prog)
        harm_degree(TAPE.prog[prog_row][0]);
    else
        harm_degree(0);
}
static void harm_bar(void)                     /* a bar begins: the progression moves on (unless you hold it) */
{
    int cb = chord_bars();
    if (user_hold > 0) {
        user_hold -= DK_TPB;
        return;
    }
    harm.user = 0;
    if (bar_n % (uint32_t)cb)
        return;
    if (!TAPE.prog)
        return;
    prog_i = (uint8_t)((prog_i + 1) & 3);
    if (!prog_i && (dk_rnd() & 3u) == 0)       /* now and then another progression */
        prog_row = (uint8_t)irand(TAPE.nprog);
    harm_degree(TAPE.prog[prog_row][prog_i]);
}
static void harm_heard(uint32_t pcs)           /* a chord you played (or the harmony track's clip) */
{
    int n = 0, i, sc, d, nt[6], k = 0;
    for (i = 0; i < 12; i++)
        n += pcs >> i & 1u;
    if (n < 3)
        return;
    d = dk_fit_degree(dk_song.root % 12, dk_song.mode, pcs, &sc);
    if (sc >= 3 && d >= 0) {
        n = dk_chord(dk_song.root % 12, dk_song.mode, d, CH_7, 1, 0, nt, 60);
        harm_notes(nt, n, d);
    } else {
        int lo = -1;
        for (i = 0; i < 12 && lo < 0; i++)
            if (pcs >> i & 1u)
                lo = i;
        for (i = 0; i < 12 && k < 4; i++)
            if (pcs >> ((lo + i) % 12) & 1u)
                nt[k++] = 60 + (lo + i) % 12;
        harm_notes(nt, k, harm.deg);
    }
    harm.user = 1;
    user_hold = chord_bars() * DK_TPB * 2;
}

/* ------------------------------------------------------------- recording --- */
static void rec_push(int clip, int kind, int pos, int dur, int note, int vel, int fl)
{
    uint32_t w = dk_recq_w;
    volatile dk_recev_t *r = &dk_recq[w % DK_RECQ];
    r->clip = (uint8_t)clip;
    r->kind = (uint8_t)kind;
    r->pass = dk_pass;
    r->ev.pos = (uint16_t)pos;
    r->ev.dur = (uint16_t)(dur < 1 ? 1 : dur);
    r->ev.note = (uint8_t)note;
    r->ev.vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    r->ev.fl = (uint8_t)fl;
    r->ev.layer = DK_L_MAKE(dk_pass, 0);
    BARRIER();
    dk_recq_w = w + 1u;
    if (!kind)
        dk_rec_hits++;
}
static int rec_track_of(int t)
{
    if (!dk_recording)
        return 0;
    return (unsigned)t < DK_NTRACK && (dk_rec_mask >> t & 1u);
}
static float clip_now(int t)                   /* the recording clip's time, ticks with the fraction */
{
    int clen = clip_len_of(cur_part, t);
    float f = (float)part_tick - (tick_len > 0.0f ? fm_maxf(tick_left, 0.0f) / tick_len : 0.0f);
    while (f < 0.0f)
        f += (float)clen;
    return f - (float)((int)f / clen * clen);
}
/* where the note you pressed was HEARD to be: the render's block and the note's wait for it put the sound later than the
 * press (dk_rec_lat ms), and you played to what you heard: the clip's time less that, round the clip's end */
static float clip_heard(int t, int nocomp)
{
    int clen = clip_len_of(cur_part, t);
    float f = clip_now(t), comp = 0.0f;
    if (!nocomp && tick_len > 0.0f)
        comp = fm_minf((float)dk_rec_lat * 0.001f * DK_SR / tick_len, (float)clen * 0.5f);
    f -= comp;
    while (f < 0.0f)
        f += (float)clen;
    return f;
}
static int rec_pos(int t)
{
    float f = clip_heard(t, rec_nocomp), g, q = (float)dk_song.trk[t].qnt * 0.01f;
    int clen = clip_len_of(cur_part, t), p;
    g = (float)((int)((f + (float)DK_T16 * 0.5f) / (float)DK_T16) * DK_T16);
    p = (int)(f + (g - f) * q + 0.5f);
    return ((p % clen) + clen) % clen;
}
static void rec_open(int t, int note, int vel)
{
    const dk_track_t *k = &dk_song.trk[t];
    int clip = cur_part * DK_NTRACK + t;
    if (k->seq == SQ_ENDLESS) {                /* ENDLESS: one after another, a division each */
        static const uint8_t DIV[4] = {96, 48, 24, 32};
        int dv = DIV[k->div & 3];
        if ((endless_n + 1) * dv > 60000)
            return;
        rec_push(clip, 0, endless_n * dv, dv, note, vel, 0);
        endless_n++;
        rec_push(clip, 1, endless_n * dv, 0, 0, 0, 0);
        return;
    }
    if (open_pos[note] >= 0)
        rec_push(cur_part * DK_NTRACK + open_trk[note], 0, open_pos[note], DK_T16, note,
                 open_vel[note], 0);
    open_pos[note] = (int16_t)rec_pos(t);
    open_nc[note] = rec_nocomp;
    open_vel[note] = (uint8_t)vel;
    open_trk[note] = (uint8_t)t;
}
static void rec_close(int note, int to_end)
{
    int t = open_trk[note], s = open_pos[note], clen, d, now;
    if (s < 0)
        return;
    clen = clip_len_of(cur_part, t);
    now = to_end ? clen : (int)clip_heard(t, open_nc[note]);
    d = to_end ? clen - s : ((now - s) % clen + clen) % clen;
    if (d <= 0)
        d = DK_T16 / 2;
    rec_push(cur_part * DK_NTRACK + t, 0, s, d, note, open_vel[note], 0);
    open_pos[note] = -1;
}
static void rec_close_all(void)
{
    int i;
    for (i = 0; i < 128; i++)
        if (open_pos[i] >= 0)
            rec_close(i, 1);
}
static void rec_done(int t, int note, int vel, int dur)   /* a generated note: whole at once */
{
    int clen = clip_len_of(cur_part, t), pos = (int)clip_now(t);
    if (dur > clen)
        dur = clen;
    rec_push(cur_part * DK_NTRACK + t, 0, pos % clen, dur, note, vel, 0);
}
static void open_publish(void)
{
    int i, k = 0;
    for (i = 0; i < 128 && k < 8; i++)
        if (open_pos[i] >= 0) {
            dk_open_pos[k] = (uint16_t)open_pos[i];
            dk_open_note[k++] = (uint8_t)i;
        }
    dk_open_n = (uint8_t)k;
}
static void rec_stop(void)
{
    if (dk_recording)
        rec_close_all();
    dk_recording = 0;
    rec_wait_note = 0;
    rec_start_after = 0;
    rec_bake = 0;
    if (count_left > 0) {
        count_left = 0;
        dk_counting = 0;
    }
    open_publish();
}

/* ---------------------------------------------------------------- playing --- */
static uint32_t ch_bits(void) { return 0; }
static uint8_t ev_snd;                   /* the sound of the event being played (fire): 0 the track's own */
static void note_play(int t, int note, float vel, int32_t gate, int32_t delay, int src)
{
    if (!track_audible(t) && src)
        return;
    voice_start(t, note, vel, gate, delay, src, ev_snd);
}
void dk_emit(int t, int note, int vel, int dur, int delay)
{
    if ((unsigned)t >= DK_NTRACK || note < 0 || note > 127)
        return;
    if (dream_off && dk_song.trk[t].seq == SQ_DRIVE)
        return;
    note_play(t, note, (float)vel * (1.0f / 127.0f), (int32_t)((float)dur * tick_len), delay, 2);
    if (rec_track_of(t))
        rec_done(t, note, vel, dur);
}
static int32_t jitter(void)
{
    return (int32_t)(dk_frand() * (float)TAPE.human * 0.01f * 0.018f * DK_SR);
}
static int swing_delay(int t, int pos)
{
    float sw = (float)dk_song.swing * 0.01f + t_swing[t];
    if (pos % (2 * DK_T16) != DK_T16 || sw <= 0.0f)
        return 0;
    return (int)(fm_minf(sw, 0.75f) * (float)DK_T16 * tick_len * 0.66f);
}

static int layer_silent(int t, const dk_ev_t *e)
{
    int c = (cur_part & 7) * DK_NTRACK + t, take = (int)DK_L_TAKE(e->layer);
    if (dk_lsolo_clip == c && dk_lsolo_take != take)
        return 1;
    return (dk_song.lmute[c] >> take & 1u) != 0;
}

static void fire(int t, const dk_ev_t *e, int ct_pos)
{
    static const uint8_t PROB[4] = {100, 75, 50, 25};
    const dk_snd_t *fs = DK_L_SND(e->layer) ? snd_get(t, (int)DK_L_SND(e->layer)) : 0;
    const dk_engine_t *eng = fs ? TAPE.eng[fs->eng] : dk_engine(t);
    int note = e->note, r, i, delay, dur = e->dur;
    float vel = (float)e->vel * (1.0f / 127.0f);
    if (e->fl & EV_MOTION) {                    /* a knob's move, or a p-lock held */
        if (e->note < T_N) {
            mot[t][e->note] = (int16_t)(e->vel > 100 ? 100 : e->vel);
            mot_until[t][e->note] = e->dur ? dk_ticks + e->dur : 0u;
            params_update();                   /* the notes on this tick hear it */
        }
        return;
    }
    if (layer_silent(t, e))                     /* a layer muted (or another one soloed) on the LAYERS list */
        return;
    dk_fired |= 1u << DK_L_TAKE(e->layer);
    if ((e->fl & EV_PROB) && (uint32_t)irand(100) >= PROB[e->fl & EV_PROB])
        return;
    if (!(eng->flags & EF_DRUM)) {
        if (tricks & TR_UP)
            note += 12;
        if (tricks & TR_DOWN)
            note -= 12;
    }
    delay = swing_delay(t, ct_pos);
    if (dk_song.trk[t].seq == SQ_LIVE || dk_song.trk[t].seq == SQ_ENDLESS) {
        delay += jitter() / 2;
        vel = fm_clampf(vel + (dk_frand() - 0.5f) * 0.12f * (float)TAPE.human * 0.01f, 0.05f, 1.0f);
    }
    r = ((e->fl & EV_RATCH) >> 2) + 1;
    if (r > 1)
        dur /= r;
    ev_snd = fs ? (uint8_t)DK_L_SND(e->layer) : 0;   /* (the track's sound as it was when this was recorded) */
    for (i = 0; i < r; i++)
        note_play(t, note, vel * (i ? 0.85f : 1.0f), (int32_t)((float)dur * tick_len * (r > 1 ? 0.8f : 1.0f)),
                  delay + (int32_t)((float)(i * dur) * tick_len), 1);
    ev_snd = 0;
}

static void clip_tick(int t, uint32_t *harm_pcs)
{
    dk_clip_t *c = CLIP(cur_part, t);
    int clen = clip_len_of(cur_part, t), ct, k, i, at = c->at, n = c->n, pt = part_tick, positions = 1;
    int rev = (tricks & TR_REVERSE) || perf_rev;
    if (perf_stutter)
        pt = stutter_at + (pt - stutter_at) % DK_T16;
    if ((tricks & TR_HALF) || perf_half) {
        if (pt & 1)
            return;
        pt >>= 1;
    }
    if (tricks & TR_DOUBLE) {
        pt *= 2;
        positions = 2;
    }
    for (k = 0; k < positions; k++) {
        ct = (pt + k) % clen;
        if (ct == 0 && !perf_stutter)          /* the clip comes round: last pass's fresh notes play from now on */
            for (i = 0; i < n; i++)
                dk_ev[at + i].fl &= (uint8_t)~EV_FRESH;
        for (i = 0; i < n; i++) {
            const dk_ev_t *e = &dk_ev[at + i];
            int p = e->pos;
            if (e->fl & EV_FRESH)
                continue;
            if (rev && !(e->fl & EV_MOTION))
                p = ((clen - e->pos - (e->dur < clen ? e->dur : 1)) % clen + clen) % clen;
            if (p != ct)
                continue;
            fire(t, e, ct);
            if (harm_pcs && !(e->fl & EV_MOTION))
                *harm_pcs |= 1u << (e->note % 12);
        }
    }
}

/* ----------------------------------------------------------------- FINGER --- */
static void finger_tick(int t, uint32_t pats, uint8_t var, int is_fill)
{
    const dk_engine_t *e = dk_engine(t);
    const dk_finger_t *set = (e->flags & EF_DRUM) ? TAPE.finger_drum : TAPE.finger_mel;
    uint32_t g = gt + (var & 16 ? DK_T16 : 0) - (var & 32 ? DK_T16 : 0) + 100000u * DK_T16;
    int unit = var & 1 ? 2 * DK_T16 : var & 2 ? DK_T16 / 2 : DK_T16, step, p, lane;
    if (!set || g % (uint32_t)unit)
        return;
    step = (int)((g / (uint32_t)unit) % 16u);
    for (p = 0; p < 16; p++) {
        const dk_finger_t *f = &set[p];
        if (!(pats >> p & 1u))
            continue;
        if (e->flags & EF_DRUM) {
            for (lane = 0; lane < 8 && lane < e->nlanes; lane++) {
                int v = f->lanes[lane][step];
                float vel;
                if (!v)
                    continue;
                vel = (float)v * (1.0f / 9.0f);
                if ((var & 4) && step % 4 == 0)
                    vel = fm_minf(1.0f, vel * 1.3f);
                if (var & 8)
                    vel *= 0.55f;
                note_play(t, e->lane_note[lane], vel * (0.92f + 0.08f * dk_frand()), (int32_t)((float)DK_T16 * tick_len),
                          jitter() / 3 + swing_delay(t, step * DK_T16), is_fill ? 1 : 2);
                if (rec_track_of(t))
                    rec_done(t, e->lane_note[lane], (int)(vel * 127.0f), DK_T16);
            }
        } else {
            int v = f->lanes[0][step], len, base, tone, note, vel;
            static const uint8_t BASE[R_N] = {36, 36, 60, 64, 55, 72, 79, 60};
            if (!v)
                continue;
            len = f->lanes[1][step] ? f->lanes[1][step] : 1;
            base = BASE[dk_song.trk[t].role % R_N] + 12 * dk_song.trk[t].oct;
            tone = (v - 1) % 4;
            note = dk_nearest(base + 12 * ((v - 1) / 4), harm.pcs[tone < harm.n ? tone : 0], base - 6, base + 30);
            if (f->lanes[2][step])
                note += 12 * (f->lanes[2][step] - 1);
            vel = (var & 8) ? 50 : (var & 4) && step % 4 == 0 ? 110 : 84;
            note_play(t, note, (float)vel * (1.0f / 127.0f), (int32_t)((float)(len * unit) * tick_len * 0.9f),
                      jitter() + (int32_t)(0.012f * DK_SR) + swing_delay(t, step * DK_T16), 2);
            if (rec_track_of(t))
                rec_done(t, note, vel, len * unit);
        }
    }
}

/* -------------------------------------------------------------------- ARP --- */
/* the rates (ticks), 1/4 .. 1/32 in the order the encoder turns */
const uint8_t DK_ARP_DIV[DK_ARP_NDIV] = {96, 48, 32, 24, 16, 12};
/* the rhythms: 16 steps of the rate, 0 a rest, 1 a note, 2 an accent (SKIP: STRAIGHT with a rest at random) */
const uint8_t DK_ARP_RHY[ARPR_N][16] = {
    {2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1},   /* STRAIGHT */
    {2, 0, 1, 0, 1, 0, 1, 0, 2, 0, 1, 0, 1, 0, 1, 0},   /* PULSE: every other */
    {0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1},   /* OFFBEAT: off the beat */
    {2, 0, 0, 1, 0, 0, 1, 0, 2, 0, 0, 1, 0, 0, 1, 0},   /* DOTTED: 3 + 3 + 2 */
    {2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0},   /* GALLOP */
    {2, 0, 0, 0, 1, 0, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1},   /* RISE: sparse to full */
    {2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1},   /* SKIP */
};
static void arp_tick(int t)
{
    static const float GATE[ARPG_N] = {0.55f, 0.25f, 0.85f, 1.02f};   /* NORMAL SHORT LONG TIE */
    const dk_track_t *k = &dk_song.trk[t];
    int n = arp_n[t], div = DK_ARP_DIV[k->arp[1] % DK_ARP_NDIV], octs = k->arp[2] < 1 ? 1 : k->arp[2] > 3 ? 3 : k->arp[2],
        total, idx, s, i, srt[8], hit, rp, src_chord = (k->arp_opt >> 2) & 1;
    float gate = GATE[k->arp_opt & 3];
    if (gt % (uint32_t)div)
        return;
    if (!n && src_chord && dk_playing && dk_harm_n) {    /* no hands: the song's chord (only while the song plays) */
        int base = 60 + 12 * k->oct, prev = -1;
        n = dk_harm_n > 4 ? 4 : dk_harm_n;
        for (i = 0; i < n; i++) {
            int v = base + dk_harm_pcs[i] % 12;
            while (v <= prev)
                v += 12;
            srt[i] = prev = v;
        }
    } else if (!n) {
        return;
    } else {
        for (i = 0; i < n; i++)
            srt[i] = arp_order[t][i];
        for (i = 1; i < n; i++) {
            int v = srt[i], j = i - 1;
            while (j >= 0 && srt[j] > v) {
                srt[j + 1] = srt[j];
                j--;
            }
            srt[j + 1] = v;
        }
    }
    rp = (int)((gt / (uint32_t)div) & 15u);           /* locked to the bar, so the rhythm sits on the beat */
    dk_arp_rpos[t] = (uint8_t)rp;
    hit = DK_ARP_RHY[k->arp_rhy % ARPR_N][rp];
    if (k->arp_rhy == ARPR_SKIP && rp % 4 && irand(4) == 0)
        hit = 0;
    if (!hit)
        return;
    dk_arp_hit[t] = 1;
    total = n * octs;
    s = arp_step[t]++;
    switch (k->arp[0] % 6) {
    case 1: idx = total - 1 - s % total; break;                              /* DOWN */
    case 2: {                                                                /* UPDN */
        int per = total > 1 ? 2 * total - 2 : 1, x = s % per;
        idx = x < total ? x : per - x;
        break;
    }
    case 3: idx = irand(total); break;                                       /* RAND */
    case 4: idx = s % total; break;                                          /* ORDER (as played) */
    case 5:                                                                  /* CHORD */
        for (i = 0; i < n; i++) {
            note_play(t, srt[i], hit == 2 ? 0.7f : 0.58f, (int32_t)((float)div * tick_len * gate), swing_delay(t, (int)(gt % DK_TPB)), 2);
            if (rec_track_of(t))
                rec_done(t, srt[i], hit == 2 ? 89 : 74, (int)((float)div * gate) > 1 ? (int)((float)div * gate) : 1);
        }
        return;
    default: idx = s % total; break;                                         /* UP */
    }
    {
        int note = ((k->arp[0] % 6) == 4 && arp_n[t] ? arp_order[t][idx % n] : srt[idx % n]) + 12 * (idx / n);
        int vel = hit == 2 ? 86 : 68;
        note_play(t, note, (float)vel * (1.0f / 127.0f), (int32_t)((float)div * tick_len * gate),
                  swing_delay(t, (int)(gt % DK_TPB)), 2);
        if (rec_track_of(t))
            rec_done(t, note, vel, (int)((float)div * gate) > 1 ? (int)((float)div * gate) : 1);
    }
}
static void arp_publish(int t)
{
    int i;
    for (i = 0; i < arp_n[t] && i < 8; i++)
        dk_held[t][i] = arp_order[t][i];
    dk_held_n[t] = arp_n[t];
}

/* ---------------------------------------------------------------- TOMBOLA --- */
static void tmb_add(int t, int note)
{
    int i, slot = -1;
    for (i = 0; i < DK_NBALL && slot < 0; i++)
        if (!ball[i].on)
            slot = i;
    if (slot < 0) {                            /* full: the oldest goes */
        for (i = 0; i < DK_NBALL - 1; i++)
            ball[i] = ball[i + 1];
        slot = DK_NBALL - 1;
    }
    ball[slot].on = 1;
    ball[slot].note = (uint8_t)note;
    ball[slot].x = (dk_frand() - 0.5f) * 0.3f;
    ball[slot].y = -0.3f;
    ball[slot].vx = (dk_frand() - 0.5f) * 0.004f;
    ball[slot].vy = 0.0f;
    dk_song.ball_trk = (uint8_t)t;
}
static void tmb_tick(int t)
{
    const dk_track_t *k = &dk_song.trk[t];
    float spin = ((float)k->tmb[0] - 50.0f) * (0.00004f), g = 0.000004f + (float)k->tmb[1] * 0.0000012f,
          e = 0.45f + (float)k->tmb[2] * 0.0053f, w = 6.2831853f * spin, nx[6], ny[6];
    int i, j, n = 0;
    tmb_a += spin;
    if (tmb_a >= 1.0f)
        tmb_a -= 1.0f;
    if (tmb_a < 0.0f)
        tmb_a += 1.0f;
    for (j = 0; j < 6; j++) {
        float a = 6.2831853f * (tmb_a + (float)j / 6.0f) + 0.5235988f;
        nx[j] = fm_cosf(a);
        ny[j] = fm_sinf(a);
    }
    for (i = 0; i < DK_NBALL; i++) {
        if (!ball[i].on)
            continue;
        ball[i].vy += g;
        ball[i].x += ball[i].vx;
        ball[i].y += ball[i].vy;
        for (j = 0; j < 6; j++) {
            float d = ball[i].x * nx[j] + ball[i].y * ny[j], lim = 0.80f;
            if (d > lim) {
                float wx = -w * ball[i].y, wy = w * ball[i].x;
                float vn = (ball[i].vx - wx) * nx[j] + (ball[i].vy - wy) * ny[j];
                ball[i].x -= (d - lim) * nx[j];
                ball[i].y -= (d - lim) * ny[j];
                if (vn > 0.0f) {
                    ball[i].vx -= (1.0f + e) * vn * nx[j];
                    ball[i].vy -= (1.0f + e) * vn * ny[j];
                    if (vn > 0.0012f) {        /* a hit: its note, as hard as it struck (drawn toward the grid) */
                        static const uint8_t QG[3] = {0, DK_T16, 2 * DK_T16};
                        int qg = QG[k->tmb[3] % 3], dl = 0, rem;
                        float vel = fm_clampf(vn * 70.0f, 0.18f, 0.95f);
                        if (qg) {
                            rem = qg - (int)(gt % (uint32_t)qg);
                            if (rem < qg && (float)rem <= 0.3f * (float)qg)
                                dl = (int)((float)rem * tick_len);
                        }
                        note_play(t, ball[i].note, vel, (int32_t)(0.4f * DK_SR), dl + jitter() / 2, 2);
                        if (rec_track_of(t))
                            rec_done(t, ball[i].note, (int)(vel * 127.0f), DK_T16 * 2);
                        dk_tmb_hit[i] = 6;
                    }
                }
            }
        }
        {
            float sp = ball[i].vx * ball[i].vx + ball[i].vy * ball[i].vy;
            if (sp > 0.0004f) {
                ball[i].vx *= 0.98f;
                ball[i].vy *= 0.98f;
            }
        }
    }
    dk_tmb_angle = tmb_a;
    for (i = 0; i < DK_NBALL; i++) {
        if (dk_tmb_hit[i])
            dk_tmb_hit[i]--;
        if (!ball[i].on)
            continue;
        dk_tmb_x[n] = (int16_t)(ball[i].x * 1000.0f);
        dk_tmb_y[n] = (int16_t)(ball[i].y * 1000.0f);
        dk_tmb_note[n++] = ball[i].note;
    }
    dk_tmb_n = (uint8_t)n;
}

/* ------------------------------------------------------------- modulators --- */
static void mods_tick(void)
{
    static const uint16_t PER[8] = {32 * DK_TPB / 4 * 4, 4 * DK_TPB, 2 * DK_TPB, DK_TPB, DK_TPB / 2, DK_TPB / 4,
                                    DK_TPB / 8, DK_TPB / 16};
    int t, i;
    for (t = 0; t < DK_NTRACK; t++)
        for (i = 0; i < 2; i++) {
            const uint8_t *m = dk_song.trk[t].mod[i];
            int per = PER[(m[2] * 7 + 50) / 100 & 7];
            switch (m[0]) {
            case MOD_SWING:
            case MOD_TREMOLO:
                modv[t][i] = dk_sine((float)(gt % (uint32_t)per) / (float)per + 4.0f);
                break;
            case MOD_DICE:
                if (gt % (uint32_t)(m[2] < 50 ? DK_TPB : DK_PPQ) == 0)
                    dice_v[t][i] = dk_frand() * 2.0f - 1.0f;
                modv[t][i] += (dice_v[t][i] - modv[t][i]) * 0.2f;
                break;
            case MOD_PEDAL: {
                float bars = 1.0f + (float)m[2] * 0.15f;
                pedal_v[t][i] = fm_minf(1.0f, pedal_v[t][i] + 1.0f / (bars * (float)DK_TPB));
                modv[t][i] = pedal_v[t][i] * 2.0f - 1.0f;
                break;
            }
            case MOD_FOLLOW:
                modv[t][i] = fm_minf(1.0f, lvl_acc[m[2] % DK_NTRACK] * 4.0f) * 2.0f - 1.0f;
                break;
            default: modv[t][i] = 0.0f; break;
            }
        }
}
static void motion_expire(void)
{
    int t, k;
    for (t = 0; t < DK_NTRACK; t++)
        for (k = 0; k < T_N; k++)
            if (mot_until[t][k] && dk_ticks >= mot_until[t][k]) {
                mot[t][k] = -1;
                mot_until[t][k] = 0;
            }
}
static void motion_reset(void)
{
    int t, k;
    for (t = 0; t < DK_NTRACK; t++)
        for (k = 0; k < T_N; k++) {
            mot[t][k] = -1;
            mot_until[t][k] = 0;
        }
}

/* --------------------------------------------------------------- transport --- */
static void block_enter(void)
{
    const dk_block_t *b;
    if (dk_song.play_mode && dk_song.nroad) {
        if (dk_road_i >= dk_song.nroad)
            dk_road_i = 0;
        b = &dk_song.road[dk_road_i];
        cur_part = b->part & 7;
        tricks = b->tricks;
        drop_mask = b->drop;
    } else {
        tricks = 0;
        drop_mask = 0;
    }
    dk_cur_part = cur_part;
    dk_part_len = (uint16_t)part_len_of(cur_part);
    stop_to = 1.0f;
}
static uint8_t road_from;                  /* SONG: the block PLAY starts from (the one chosen on ROAD) */
static void play_start(void)
{
    int t, i;
    dk_ticks = 0;
    part_tick = 0;
    bar_n = 0;
    part_loops = 0;
    if (dk_song.play_mode) {
        dk_road_i = road_from < dk_song.nroad ? road_from : 0;
        road_from = 0;
        dk_road_rep = 0;
    } else
        cur_part = dk_song.cur_part & 7;
    block_enter();
    tick_left = 0.0f;
    dk_playing = 1;
    harm_regrow();
    for (t = 0; t < DK_NTRACK; t++) {
        arp_step[t] = 0;
        for (i = 0; i < 2; i++)
            pedal_v[t][i] = 0.0f;
    }
    if (dk_midi_clock)
        dk_midi_out(0xFA, 0, 0);
}
static void play_stop(void)
{
    int i;
    rec_stop();
    pm_apply();
    dk_playing = 0;
    dk_stems = 0;
    stems_solo = 0xFF;
    queued = 0xFF;
    dk_next_part = 0xFF;
    drop_mask = 0;
    tricks = 0;
    stop_to = 1.0f;
    for (i = 0; i < DK_NVOICE; i++)
        if (vc[i].on && vc[i].src >= 1 && vc[i].src != 3)
            voice_release(&vc[i]);
    motion_reset();
    if (dk_midi_clock)
        dk_midi_out(0xFC, 0, 0);
}

static void live_note(int t, int note, int vel, int delay)
{
    dk_track_t *k = &dk_song.trk[t];
    const dk_engine_t *e = dk_engine(t);
    if ((unsigned)t >= DK_NTRACK)
        return;
    if (vel) {
        if (rec_wait_note && !dk_playing) {    /* armed: the first note starts the take */
            rec_wait_note = 0;
            play_start();
            rec_on_now();
            rec_nocomp = 1;                    /* its start is the take's start: at 0 */
        }
        if (!(e->flags & EF_DRUM)) {           /* the harmony hears it */
            uint32_t pcs;
            int i, nn = 0;
            held_pcs[t] |= 1u << (note % 12);
            if (gt - win_at < 10u)
                win_pcs |= 1u << (note % 12);
            else
                win_pcs = 1u << (note % 12);
            win_at = gt;
            pcs = held_pcs[t] | win_pcs;
            for (i = 0; i < 12; i++)
                nn += pcs >> i & 1u;
            if (nn >= 3)
                harm_heard(pcs);
            else if (nn == 1) {                /* a line: DRIVE listens, answers later */
                if (harm.nheard >= 8 || (harm.nheard && dk_ticks - harm.heard_at > 2u * DK_TPB))
                    harm.nheard = 0;
                harm.heard[harm.nheard++] = (int8_t)note;
                harm.heard_at = dk_ticks ? dk_ticks : 1u;
            }
        }
        if (dk_arp_on(k) && !(dk_engine(t)->flags & EF_DRUM)) {   /* ARP over the sequencer: the keys feed it */
            if (!arp_keys[t] && !k->arp[3])
                arp_n[t] = 0;
            if (k->arp[3] && !arp_keys[t])     /* latched: a new hand replaces the old */
                arp_n[t] = 0;
            arp_keys[t]++;
            if (arp_n[t] < 8)
                arp_order[t][arp_n[t]++] = (uint8_t)note;
            arp_publish(t);
            return;
        }
        switch (k->seq) {
        case SQ_HOLD:
            if (!hold_keys[t]) {               /* a new hand: the old drone lets go */
                int i;
                for (i = 0; i < DK_NVOICE; i++)
                    if (vc[i].on && vc[i].trk == t && vc[i].src == 3) {
                        if (rec_track_of(t))
                            rec_close(vc[i].note, 0);
                        voice_release(&vc[i]);
                    }
                dk_held_n[t] = 0;
            }
            hold_keys[t]++;
            if (dk_held_n[t] < 8)
                dk_held[t][dk_held_n[t]++] = (uint8_t)note;
            voice_start(t, note, (float)vel * (1.0f / 127.0f), -1, delay, 3, 0);
            if (rec_track_of(t))
                rec_open(t, note, vel);
            return;
        case SQ_TOMBOLA:
            tmb_add(t, note);
            break;
        default: break;
        }
        voice_start(t, note, (float)vel * (1.0f / 127.0f), -1, delay, 0, 0);
        if (rec_track_of(t))
            rec_open(t, note, vel);
    } else {
        held_pcs[t] &= ~(1u << (note % 12));
        if (dk_arp_on(k) && !(dk_engine(t)->flags & EF_DRUM)) {
            if (arp_keys[t])
                arp_keys[t]--;
            if (!k->arp[3]) {
                int i, j = 0;
                for (i = 0; i < arp_n[t]; i++)
                    if (arp_order[t][i] != note)
                        arp_order[t][j++] = arp_order[t][i];
                arp_n[t] = (uint8_t)j;
            }
            arp_publish(t);
            return;
        }
        switch (k->seq) {
        case SQ_HOLD:
            if (hold_keys[t])
                hold_keys[t]--;
            return;
        default: break;
        }
        voices_off_note(t, note);
        if (rec_track_of(t) && k->seq != SQ_ENDLESS)
            rec_close(note, 0);
    }
}

/* -------------------------------------------------------------- the tick --- */
static void metro_tick(void)
{
    static const uint8_t DV[3] = {DK_PPQ, DK_PPQ / 2, DK_PPQ / 4};
    int dv = DV[dk_metro.div % 3], want;
    want = dk_metro.mode == 2 || (dk_metro.mode == 1 && (dk_recording || rec_bake));
    if (!want || dk_metro.mode == 0)
        return;
    if (part_tick % dv == 0) {
        if (part_tick % DK_PPQ == 0)
            metro_click(part_tick % DK_TPB == 0);
        else {
            metro_click(0);
            clk.env *= 0.4f;
        }
    }
}

static void drive_tick(int t)
{
    dk_drive_t d;
    const dk_track_t *k = &dk_song.trk[t];
    int plen = part_len_of(cur_part), bar = (int)(bar_n % 8u);
    d.trk = (uint8_t)t;
    d.role = k->role;
    d.style = k->drv[1];
    d.energy = k->drv[0];
    d.var = k->drv[2];
    d.tick = dk_ticks;
    d.bar_tick = (uint32_t)(part_tick % DK_TPB);
    d.fill = (uint8_t)(dk_fill || (bar % 4 == 3 && (k->drv[2] > 30 || bar == 7)));
    d.breath = (uint8_t)(TAPE.style_breath && TAPE.style_breath[dk_song.style % TAPE.nstyle] && bar == 7);
    d.part_end = (uint8_t)(part_tick >= plen - DK_TPB && (dk_song.play_mode ? block_last : queued != 0xFF));
    if (TAPE.drive)
        TAPE.drive(&d, &harm);
}

static void road_next(void)
{
    const dk_block_t *b = &dk_song.road[dk_road_i < dk_song.nroad ? dk_road_i : 0];
    dk_road_rep++;
    if (dk_road_rep >= (b->reps ? b->reps : 1)) {
        dk_road_rep = 0;
        dk_road_i++;
        if (dk_road_i >= dk_song.nroad) {
            dk_road_i = 0;
            if (dk_stems) {                    /* STEMS: the next pass (after a bar's gap with its click) */
                stems_pass++;
                if (stems_pass > DK_NTRACK) {
                    play_stop();
                    return;
                }
                stems_gap = 1;
                stems_solo = (uint8_t)(stems_pass - 1);
            }
        }
    }
    block_enter();
}

static void seq_tick(void)
{
    int t;
    uint32_t hp = 0;
    dk_clock++;
    if (count_left > 0) {                      /* the count-in: the bar's beats, then the take begins */
        if (count_left % DK_PPQ == 0)
            metro_click(count_left % DK_TPB == 0);
        dk_counting = (uint8_t)((count_left + DK_PPQ - 1) / DK_PPQ);
        if (--count_left == 0) {
            dk_counting = 0;
            if (rec_start_after) {
                rec_start_after = 0;
                play_start();
                rec_on_now();
            }
            if (dk_stems == 1)
                play_start(), dk_stems = 2;
        } else
            return;
    }
    gt = dk_playing ? dk_ticks : dk_clock;
    if (!dk_playing) {                         /* stopped: the hands' generators still run, on the free clock */
        for (t = 0; t < DK_NTRACK; t++) {
            const dk_track_t *k = &dk_song.trk[t];
            if (dk_arp_on(k))
                arp_tick(t);
            else if (k->seq == SQ_FINGER && (finger_held[t] || dk_finger_latch[t]))
                finger_tick(t, finger_held[t] | dk_finger_latch[t], finger_var[t], 0);
        }
        mods_tick();
        return;
    }
    if (stems_gap) {                           /* STEMS: a bar of silence between passes, a click to cut at */
        if (part_tick == 0 && stems_gap == 1) {
            metro_click(1);
            dk_metro_clicks--;
        }
        if (++part_tick >= DK_TPB) {
            part_tick = 0;
            stems_gap = 0;
        }
        dk_part_tick = part_tick;
        return;
    }
    metro_tick();
    if (dk_midi_clock && dk_ticks % (DK_PPQ / 24) == 0)
        dk_midi_out(0xF8, 0, 0);
    mods_tick();
    motion_expire();
    if (part_tick % DK_TPB == 0)
        harm_bar();
    block_last = (uint8_t)(dk_song.play_mode && dk_song.nroad &&
                           dk_road_rep + 1 >= (dk_song.road[dk_road_i].reps ? dk_song.road[dk_road_i].reps : 1));
    if (dk_song.play_mode && (tricks & TR_STOP) && block_last && part_tick >= part_len_of(cur_part) - DK_PPQ)
        stop_to = 0.0f;                        /* TAPESTOP: the block's last beat */
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_track_t *k = &dk_song.trk[t];
        const dk_engine_t *e = dk_engine(t);
        int fill = dk_fill && (e->flags & EF_DRUM);
        if (fill)
            finger_tick(t, 1u << (13 + (bar_n % 3u)), 0, 1);
        else if (k->seq == SQ_DRIVE) {
            if (!(drop_mask >> t & 1u))
                drive_tick(t);
        } else
            clip_tick(t, t == dk_song.harm_track ? &hp : 0);
        if (dk_arp_on(k))
            arp_tick(t);
        if (k->seq == SQ_FINGER && (finger_held[t] || dk_finger_latch[t]))
            finger_tick(t, finger_held[t] | dk_finger_latch[t], finger_var[t], 0);
        else if (k->seq == SQ_TOMBOLA)
            tmb_tick(t);
    }
    {
        int n = 0, i;
        for (i = 0; i < 12; i++)
            n += hp >> i & 1u;
        if (n >= 3)
            harm_heard(hp);
    }
    harm.fresh = 0;
    dk_part_tick = part_tick;
    part_tick++;
    dk_ticks++;
    if (part_tick % DK_TPB == 0) {             /* a bar ends */
        bar_n++;
        pm_apply();                            /* the part's mutes asked for during the bar */
        if (rec_bake && --rec_bake == 0)       /* BAKE done (the UI turns the track to STEP) */
            rec_stop();
        if (!dk_song.play_mode && queued != 0xFF) {   /* PART: the queued part, from the bar's end */
            cur_part = queued;
            queued = 0xFF;
            dk_next_part = 0xFF;
            dk_song.cur_part = cur_part;
            if (dk_recording)
                rec_close_all();
            part_tick = 0;
            for (t = 0; t < DK_NTRACK; t++)
                pedal_v[t][0] = pedal_v[t][1] = 0.0f;
            block_enter();
            return;
        }
    }
    if (dk_recording) {                        /* the recording clip comes round: a new pass (a layer for undo) */
        int clen = clip_len_of(cur_part, dk_rec_trk);
        if (dk_song.trk[dk_rec_trk].seq != SQ_ENDLESS && part_tick % clen == 0) {
            rec_close_all();
            if (dk_pass < 250)
                dk_pass++;
        }
        open_publish();
    }
    if (part_tick >= part_len_of(cur_part)) {
        part_tick = 0;
        part_loops++;
        if (dk_song.play_mode && dk_song.nroad)
            road_next();
        for (t = 0; t < DK_NTRACK; t++)
            pedal_v[t][0] = pedal_v[t][1] = 0.0f;
    }
}

/* -------------------------------------------------------------- commands --- */
static void drain(void)
{
    while (q_r != q_w) {
        cmd_t c = q[q_r % QN];
        int a = c.a, b = c.b;
        BARRIER();
        q_r++;
        switch (c.c) {
        case C_NOTE:
            live_note(a, b & 0x7F, (b >> 8) & 0x7F, c.d);
            rec_nocomp = 0;
            break;
        case C_PLAY:
            if (a && !dk_playing && !count_left) {
                road_from = (uint8_t)b;
                play_start();
            } else if (!a)
                play_stop();
            break;
        case C_REC:
            if (a) {
                int t = c.d & 7, bake = (c.d >> 8) & 31;
                int t2 = c.d >> 16 & 0xFF, m = c.d >> 24 & 0x3F, i, lay = 0;
                if (t >= DK_NTRACK)
                    t = 0;
                dk_rec_trk = (uint8_t)t;
                m |= 1 << t;                   /* the armed tracks, the one the take began on, the split's harmony track */
                if (t2 < DK_NTRACK)
                    m |= 1 << t2;
                dk_rec_mask = (uint8_t)m;
                for (i = 0; i < DK_NTRACK; i++)   /* one pass number for all: the highest layer + 1 */
                    if ((m >> i & 1) && CLIP(cur_part, i)->layers > lay)
                        lay = CLIP(cur_part, i)->layers;
                dk_pass = (uint8_t)(lay + 1);
                dk_rec_pass0 = dk_pass;
                endless_n = (uint16_t)(dk_song.trk[t].seq == SQ_ENDLESS ? 0 : endless_n);
                if (bake) {
                    rec_bake = (uint8_t)(bake + (dk_playing ? 1 : 0));
                    if (!dk_playing)
                        play_start();
                    rec_on_now();
                } else if (dk_playing)
                    rec_on_now();
                else if (dk_metro.on_note)
                    rec_wait_note = 1;
                else {
                    count_left = (dk_metro.count ? dk_metro.count : 1) * DK_TPB;
                    rec_start_after = 1;
                    dk_counting = 4;
                }
            } else
                rec_stop();
            break;
        case C_RECTRK:
            if (dk_recording)
                rec_close_all();
            dk_rec_trk = (uint8_t)(a % DK_NTRACK);
            dk_rec_mask = (uint8_t)(dk_rec_mask | 1u << dk_rec_trk);
            dk_pass = (uint8_t)(CLIP(cur_part, dk_rec_trk)->layers + 1);
            break;
        case C_RECMASK:                        /* a = the armed tracks, changed while it records: a track let go closes its notes */
            if (dk_recording) {
                int i, m = (a & 0x3F) | 1 << dk_rec_trk;
                for (i = 0; i < 128; i++)
                    if (open_pos[i] >= 0 && !(m >> open_trk[i] & 1))
                        rec_close(i, 1);
                dk_rec_mask = (uint8_t)m;
            }
            break;
        case C_PMUTE:                          /* a = track, b = part: while playing at the bar's end, else now */
            if (a >= DK_NTRACK)
                break;
            if (dk_pm_x && dk_pm_part != (b & 7))
                pm_apply();
            dk_pm_part = (uint8_t)(b & 7);
            dk_pm_x = (uint8_t)(dk_pm_x ^ (1u << a));
            if (!dk_playing)
                pm_apply();
            break;
        case C_PART:
            a &= 7;
            if (!dk_playing || b) {            /* stopped (or at once): now */
                cur_part = (uint8_t)a;
                dk_song.cur_part = (uint8_t)a;
                queued = 0xFF;
                dk_next_part = 0xFF;
                if (dk_playing)
                    part_tick = 0;
                block_enter();
            } else {
                queued = (uint8_t)a;
                dk_next_part = (uint8_t)a;
            }
            break;
        case C_SEEK:
            dk_road_i = (uint8_t)a;
            dk_road_rep = 0;
            part_tick = 0;
            block_enter();
            break;
        case C_PANIC: {
            int i;
            for (i = 0; i < DK_NVOICE; i++)
                vc[i].on = 0;
            break;
        }
        case C_FINGER:                         /* a pattern's key: b 1 down, 0 up, 2 latch toggled */
            if (b == 2)
                dk_finger_latch[a % DK_NTRACK] ^= 1u << (c.d & 15);
            else if (b)
                finger_held[a % DK_NTRACK] |= (uint16_t)(1u << (c.d & 15));
            else
                finger_held[a % DK_NTRACK] &= (uint16_t)~(1u << (c.d & 15));
            dk_finger_on[a % DK_NTRACK] = (uint8_t)(finger_held[a % DK_NTRACK] ? 1 : 0);
            break;
        case C_FVAR:
            if (b)
                finger_var[a % DK_NTRACK] |= (uint8_t)(1u << (c.d & 7));
            else
                finger_var[a % DK_NTRACK] &= (uint8_t)~(1u << (c.d & 7));
            break;
        case C_PERFORM: {
            int on = b != 0;
            dk_perform = (uint8_t)(on ? a : PF_NONE);
            switch (a) {
            case PF_TAPESTOP:
            case PF_VINYLSTOP: stop_to = on ? 0.0f : 1.0f; break;
            case PF_LOWPASS: lp_target = on ? dk_lpk(420.0f) : 1.0f; break;
            case PF_FILTERUP: lp_target = on ? dk_lpk(1500.0f) : 1.0f; break;
            case PF_RAINUP: perf_rain = (uint8_t)on; break;
            case PF_DREAM:
                if (on)
                    dream_off ^= 1u;
                break;
            case PF_HALFTIME: perf_half = (uint8_t)on; break;
            case PF_ECHOTHROW: perf_echo = (uint8_t)on; break;
            case PF_MUTEDRUMS: perf_mute_drums = (uint8_t)on; break;
            case PF_REVERSE: perf_rev = (uint8_t)on; break;
            case PF_STUTTER:
                perf_stutter = (uint8_t)on;
                stutter_at = (uint16_t)(part_tick - part_tick % DK_T16);
                break;
            case PF_BUILD: dk_fill = (uint8_t)on; lp_target = on ? dk_lpk(2500.0f) : 1.0f; break;
            default: break;
            }
            break;
        }
        case C_FILL: dk_fill = (uint8_t)(a != 0); break;
        case C_TOMBOLA:
            if (a == 1)
                tmb_add(b >> 8, b & 0x7F);
            else {
                int i;
                for (i = 0; i < DK_NBALL; i++)
                    ball[i].on = 0;
                dk_tmb_n = 0;
            }
            break;
        case C_SEED:
            rng ^= (uint32_t)c.d * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_PEDAL: dk_ctx.pedal = (uint8_t)(a != 0); break;
        case C_STEMS:
            if (a) {
                play_stop();
                dk_song.play_mode = 1;
                stems_pass = 0;
                stems_solo = 0xFF;
                dk_stems = 1;
                count_left = DK_TPB;
            } else
                play_stop();
            break;
        case C_ARPCLR:
            arp_n[a % DK_NTRACK] = 0;
            arp_keys[a % DK_NTRACK] = 0;
            arp_publish(a % DK_NTRACK);
            break;
        case C_HOLDCLR: voices_release_track(a % DK_NTRACK, 3); dk_held_n[a % DK_NTRACK] = 0; break;
        case C_RETUNE: tempo_set(); harm_regrow(); break;
        default: break;
        }
    }
    /* the knobs the render follows without being told */
    tempo_set();
    master_lp_k += (lp_target - master_lp_k) * 0.08f;
    if (lp_target >= 1.0f && master_lp_k > 0.995f)
        master_lp_k = 1.0f;
    (void)ch_bits;
}

static void seq_init(void)
{
    int i;
    for (i = 0; i < 128; i++)
        open_pos[i] = -1;
    tempo_set();
    harm_regrow();
}
