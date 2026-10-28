/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The sequencer, part of the core's unit (core.c): the clock (free, and the transport's), the parts and the road,
 * the clips' playback with the road's tricks, recording (LIVE, the generators', BAKE), the count-in and the
 * metronome, the generators (ARP, ENDLESS), the harmony, the modulators, the performance
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
int dk_rec_pending(void) { return rec_wait_note || rec_start_after; }   /* REC pressed: the first note / the count-in awaited */
static uint8_t rec_nocomp;                     /* the note that starts a take is at 0, not made up for the latency */
static uint8_t open_nc[128];                  /* ... and it began at the take's start, not made up for the latency */
static int16_t open_pos[128];                  /* recording: the clip tick a held note began (-1 none) */
static uint8_t open_vel[128], open_trk[128];
/* the count-in's last beat: notes played after its last click belong to the take's first beat (a hand a little
 * early). Held on: they start at 0 when the take does; let go already: a note at 0 as long as it was */
static uint8_t pre_vel[128], pre_trk[128];
static int16_t pre_dur[128];                   /* -1 still held; else its length in ticks */
static int32_t pre_at[128];                    /* count_left when pressed */
#define PRE_WIN DK_PPQ
uint8_t dk_arp_step[DK_NTRACK];            /* (the pattern's step: it starts from 1 again with every new hand) */
#define arp_step dk_arp_step
static uint8_t arp_order[DK_NTRACK][8], arp_cnt[DK_NTRACK][8], arp_n[DK_NTRACK], arp_keys[DK_NTRACK];
volatile uint8_t dk_arp_rpos[DK_NTRACK], dk_arp_hit[DK_NTRACK];   /* for the page: where the rhythm is, a hit just now */
static uint8_t perf_half, perf_rev, perf_stutter, stems_pass, stems_gap;
static uint16_t stutter_at;
static float lp_target = 1.0f;
static dk_harm_t harm;
static int32_t user_hold;                      /* ticks a chord you played holds the harmony */
static uint8_t prog_i, prog_row;
static uint32_t held_pcs[DK_NTRACK], win_pcs, win_at;
static uint32_t part_loops, bar_n;

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
/* the layer a track writes. SELECT can add a track that already has takes: those notes stay their own
 * layer, the new ones take the next free number. A track already in this take is left on its number
 * (switching back must not open a second layer). REPLACE's floor is the number it joined at, so a
 * later loop of the same press does not eat the notes it just recorded. */
static int take_clamp(int v)
{
    if (v < 1)
        return 1;
    return v > 31 ? 31 : v;
}
static void rec_takes_begin(int mask)
{
    int i, p = take_clamp(dk_pass);
    for (i = 0; i < DK_NTRACK; i++) {
        dk_rec_take[i] = 0;
        dk_rec_floor[i] = 0;
        if (mask >> i & 1) {
            dk_rec_take[i] = (uint8_t)p;
            dk_rec_floor[i] = (uint8_t)p;
        }
    }
}
static void rec_take_join(int t)
{
    int lay, take;
    t %= DK_NTRACK;
    if ((dk_rec_mask >> t & 1u) && dk_rec_take[t])
        return;
    lay = CLIP(cur_part, t)->layers;
    take = take_clamp(dk_pass);
    if (take <= lay)
        take = take_clamp(lay + 1);
    dk_rec_take[t] = (uint8_t)take;
    dk_rec_floor[t] = (uint8_t)take;
    dk_rec_mask = (uint8_t)(dk_rec_mask | 1u << t);
}
static void rec_push(int clip, int kind, int pos, int dur, int note, int vel, int fl)
{
    uint32_t w = dk_recq_w;
    volatile dk_recev_t *r = &dk_recq[w % DK_RECQ];
    int tr = clip % DK_NTRACK;
    int take = dk_pass;
    if ((unsigned)tr < DK_NTRACK && dk_rec_take[tr])
        take = dk_rec_take[tr];
    take = take_clamp(take);
    r->clip = (uint8_t)clip;
    r->kind = (uint8_t)kind;
    r->pass = (uint8_t)take;
    r->ev.pos = (uint16_t)pos;
    r->ev.dur = (uint16_t)(dur < 1 ? 1 : dur);
    r->ev.note = (uint8_t)note;
    r->ev.vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    r->ev.fl = (uint8_t)fl;
    r->ev.layer = DK_L_MAKE(take, 0);
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
static int rec_pre_track(int t)               /* in the count-in's last beat, a track the take will record */
{
    return count_left > 0 && count_left <= PRE_WIN && rec_start_after && (unsigned)t < DK_NTRACK &&
           (dk_rec_mask >> t & 1u);
}
static void rec_pre_flush(void)                /* the take begins: the early notes in at its start */
{
    int n;
    for (n = 0; n < 128; n++) {
        if (!pre_vel[n])
            continue;
        if (pre_dur[n] < 0) {                  /* still held: open from 0 */
            open_pos[n] = 0;
            open_vel[n] = pre_vel[n];
            open_trk[n] = pre_trk[n];
            open_nc[n] = 0;
        } else
            rec_push(cur_part * DK_NTRACK + pre_trk[n], 0, 0, pre_dur[n] > DK_T16 / 2 ? pre_dur[n] : DK_T16 / 2, n,
                     pre_vel[n], 0);
        pre_vel[n] = 0;
    }
}
static int strum_ticks(int delay)              /* a strummed note's delay (samples) in ticks: kept in the take */
{
    return tick_len > 0.0f && delay > 0 ? (int)((float)delay / tick_len + 0.5f) : 0;
}
static void rec_open(int t, int note, int vel, int delay)
{
    if (open_pos[note] >= 0)
        rec_push(cur_part * DK_NTRACK + open_trk[note], 0, open_pos[note], DK_T16, note,
                 open_vel[note], 0);
    open_pos[note] = (int16_t)((rec_pos(t) + strum_ticks(delay)) % clip_len_of(cur_part, t));   /* the grid's place,
                                                 * then the strum's offset after it (STRUM survives QUANT) */
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
/* a part branch mid-take: the notes you are holding end HERE in this part (not stretched to the end of the loop)
 * and open again in the part you land on, so the rest of the note records there */
static uint8_t br_note[32], br_vel[32], br_trk[32], br_nc[32], br_n;
static void rec_split_close(void)
{
    int i;
    br_n = 0;
    for (i = 0; i < 128; i++) {
        if (open_pos[i] < 0)
            continue;
        if (br_n < 32) {
            br_note[br_n] = (uint8_t)i;
            br_vel[br_n] = open_vel[i];
            br_trk[br_n] = open_trk[i];
            br_nc[br_n] = open_nc[i];
            br_n++;
        }
        rec_close(i, 0);
    }
}
static void rec_split_open(void)
{
    int i;
    for (i = 0; i < br_n; i++) {
        int t = br_trk[i];
        if (!rec_track_of(t))
            continue;
        open_pos[br_note[i]] = (int16_t)rec_pos(t);
        open_nc[br_note[i]] = br_nc[i];
        open_vel[br_note[i]] = br_vel[i];
        open_trk[br_note[i]] = (uint8_t)t;
    }
    br_n = 0;
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
            dk_open_note[k] = (uint8_t)i;
            dk_open_trk[k] = open_trk[i];
            k++;
        }
    dk_open_n = (uint8_t)k;
}
/* a take asked for: the part it records goes round (its bars), the road (SONG) set aside till the take ends */
static uint8_t rec_loop;
static void rec_stop(void)
{
    if (dk_recording)
        rec_close_all();
    dk_recording = 0;
    rec_wait_note = 0;
    rec_start_after = 0;
    rec_bake = 0;                              /* (rec_loop stays: the part goes on round after the take, till the stop) */
    {                                          /* (stopped in the count-in: the early notes forgotten) */
        int n;
        for (n = 0; n < 128; n++)
            pre_vel[n] = 0;
    }
    if (count_left > 0) {
        count_left = 0;
        dk_counting = 0;
    }
    open_publish();
}

/* ---------------------------------------------------------------- playing --- */
static uint32_t ch_bits(void) { return 0; }
static uint8_t ev_snd;                   /* the sound of the event being played (fire): 0 the track's own */
static uint16_t gr_act;
static void note_play(int t, int note, float vel, int32_t gate, int32_t delay, int src)
{
    if (!track_audible(t) && src)
        return;
    if (!(dk_engine(t)->flags & EF_DRUM))
        gr_act++;                              /* (GROOVE's FOLLOW: how much the others play) */
    voice_start(t, note, vel, gate, delay, src, ev_snd);
}
void dk_emit(int t, int note, int vel, int dur, int delay)
{
    if ((unsigned)t >= DK_NTRACK || note < 0 || note > 127)
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
            mot[t][e->note] = (int16_t)((e->vel > 100 ? 100 : e->vel) * 10);
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
    r = ((e->fl & EV_RATCH) >> 2) + 1;
    if (r > 1)
        dur /= r;
    ev_snd = fs ? (uint8_t)DK_L_SND(e->layer) : 0;   /* (the track's sound as it was when this was recorded) */
    for (i = 0; i < r; i++)
        note_play(t, note, vel * (i ? 0.85f : 1.0f), (int32_t)((float)dur * tick_len * (r > 1 ? 0.8f : 1.0f)),
                  delay + (int32_t)((float)(i * dur) * tick_len), 1);
    ev_snd = 0;
}

/* AUTOMATION: the clip's curves. A point (EV_MOTION|EV_RAMP) holds its value till the next point of its target, between two
 * the value moves in a straight line (tenths of a percent, so a slow turn is smooth). Before the first point the loop's
 * last value goes on. A knob turned by hand wins till the bar's end (auto_hold), then the curve takes over again. A
 * target that has no curve in this clip (another part, erased) is let go. Evaluated every tick, whatever the tricks. */
static uint32_t ramp_own[DK_NTRACK];            /* the targets (bit) whose motion value is a curve's */
static uint32_t auto_hold_t[DK_NTRACK][T_N];    /* hand wins while dk_ticks is below this */
static uint8_t auto_dirty;                      /* a curve moved: the tick's end updates the voices' parameters once */
static void auto_tick(int t)
{
    const dk_clip_t *c = CLIP(cur_part, t);
    int clen = clip_len_of(cur_part, t), ct = part_tick % clen, i, g, n = c->n, at = c->at;
    int16_t last[T_N], next[T_N];
    uint32_t seen = 0;
    if (!n && !ramp_own[t])
        return;
    for (g = 0; g < T_N; g++)
        last[g] = next[g] = -1;
    for (i = 0; i < n; i++) {
        const dk_ev_t *e = &dk_ev[at + i];
        if ((e->fl & (EV_MOTION | EV_RAMP)) != (EV_MOTION | EV_RAMP) || e->note >= T_N || !e->note)
            continue;
        g = e->note;
        seen |= 1u << g;
        if (e->pos <= ct)
            last[g] = (int16_t)i;
        else if (next[g] < 0)
            next[g] = (int16_t)i;
    }
    for (g = 1; g < T_N; g++) {
        const dk_ev_t *a, *b;
        int v;
        if (!(seen >> g & 1u)) {
            if (ramp_own[t] >> g & 1u) {
                ramp_own[t] &= ~(1u << g);
                mot[t][g] = -1;
                mot_until[t][g] = 0;
                auto_dirty = 1;
            }
            continue;
        }
        if (dk_ticks < auto_hold_t[t][g] || last[g] < 0)
            continue;
        a = &dk_ev[at + last[g]];
        v = a->vel > 100 ? 1000 : a->vel * 10;
        if (next[g] >= 0) {
            b = &dk_ev[at + next[g]];
            v += ((b->vel > 100 ? 1000 : b->vel * 10) - v) * (ct - a->pos) / (b->pos - a->pos);
        }
        ramp_own[t] |= 1u << g;
        mot_until[t][g] = 0;
        if (mot[t][g] != v) {
            mot[t][g] = (int16_t)v;
            auto_dirty = 1;
        }
    }
}
/* the UI: a knob turned by hand (target of the track): its own value is heard till the bar's end */
void dk_auto_hold(int t, int target)
{
    if ((unsigned)t >= DK_NTRACK || target <= 0 || target >= T_N)
        return;
    auto_hold_t[t][target] = dk_playing ? dk_ticks + (uint32_t)(DK_TPB - part_tick % DK_TPB) : 0u;
    if (ramp_own[t] >> target & 1u) {           /* (a curve not begun yet has nothing to let go; its first point waits too) */
        mot[t][target] = -1;
        auto_dirty = 1;
    }
}
/* the value a curve gives now (tenths of a percent of the range), -1 none: the knob's display follows it */
int dk_auto_live(int t, int target)
{
    if ((unsigned)t >= DK_NTRACK || target <= 0 || target >= T_N || !dk_playing || !(ramp_own[t] >> target & 1u))
        return -1;
    return mot[t][target];
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
            if ((e->fl & (EV_MOTION | EV_RAMP)) == (EV_MOTION | EV_RAMP))   /* a curve's point: auto_tick */
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

/* ---------------------------------------------------------------- ENDLESS --- */
/* ENDLESS (as OP-1's): its clip a row of steps, a sixteenth each in the clip (a step's notes at step * DK_T16, a tie
 * longer), played at its own rate - not the part's bars: it goes round by itself - FORWARD, BACKWARD, PINGPONG or
 * RANDOM, each step as long as the GATE says. A key played on it (not entering) transposes it: the first step's
 * lowest note goes to the key (dk_endl_tr, till the next key or a stop) */
volatile uint8_t dk_endl_step[DK_NTRACK];
volatile int8_t dk_endl_tr[DK_NTRACK];
static const uint8_t ERATE[6] = {96, 48, 32, 24, 16, 12};
static void endless_tick(int t)
{
    static const float GATE[4] = {0.8f, 0.35f, 1.0f, 1.08f};
    const dk_track_t *k = &dk_song.trk[t];
    const dk_clip_t *c = CLIP(cur_part, t);
    int n = c->len / DK_T16, dv = ERATE[k->drv[2] % 6], dir = k->drv[0] & 3, i, step, kk;
    float g = GATE[k->drv[1] & 3];
    if (n <= 0 || !c->n || dk_ticks % (uint32_t)dv)
        return;
    kk = (int)(dk_ticks / (uint32_t)dv);
    switch (dir) {
    case 1: step = n - 1 - kk % n; break;
    case 2: {
        int per = n > 1 ? 2 * n - 2 : 1, x = kk % per;
        step = x < n ? x : per - x;
        break;
    }
    case 3: step = (int)(((uint32_t)kk * 2654435761u >> 13) % (uint32_t)n); break;
    default: step = kk % n; break;
    }
    dk_endl_step[t] = (uint8_t)step;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        int steps, note;
        if ((e->fl & EV_MOTION) || e->pos != step * DK_T16)
            continue;
        steps = (e->dur + 2 + DK_T16 - 1) / DK_T16;   /* a tie: more steps long */
        note = e->note + ((dk_engine(t)->flags & EF_DRUM) ? 0 : dk_endl_tr[t]);
        if (note < 0 || note > 127)
            continue;
        note_play(t, note, (float)e->vel * (1.0f / 127.0f), (int32_t)((float)(steps * dv) * g * tick_len), 0, 1);
    }
}
/* the lowest note of an ENDLESS clip's first step (the key a transposition is measured from), -1 none */
int dk_endl_root(int t)
{
    const dk_clip_t *c = CLIP(dk_song.cur_part, t);
    int i, lo = -1;
    for (i = 0; i < c->n; i++) {
        const dk_ev_t *e = &dk_ev[c->at + i];
        if (!(e->fl & EV_MOTION) && e->pos == 0 && (lo < 0 || e->note < lo))
            lo = e->note;
    }
    return lo;
}

/* FILL (GLOBAL held) on a drum track: a bar that builds - the snare in sixteenths, the toms coming down, louder to its end */
static void fill_tick(int t)
{
    const dk_engine_t *e = dk_engine(t);
    int step, l, n = e->nlanes;
    float v;
    if (gt % DK_T16 || n < 2)
        return;
    step = (int)((gt / DK_T16) % 16u);
    v = 0.45f + 0.5f * (float)step / 15.0f;
    if (step % 4 == 0)
        note_play(t, e->lane_note[0], v, (int32_t)((float)DK_T16 * tick_len), 0, 1);
    l = step < 8 || n < 5 ? 1 : 4 + (step / 2) % (n - 4 > 0 ? n - 4 : 1);   /* the snare, then what follows it in the kit */
    note_play(t, e->lane_note[l % n], v * (step % 2 ? 0.8f : 1.0f), (int32_t)((float)DK_T16 * tick_len),
              jitter() / 3 + swing_delay(t, step * DK_T16), 1);
}


/* ----------------------------------------------------------------- GROOVE --- */
/* A drum track played as EZdrummer: the tape's grooves (white 1-15), fills (black 1-10), DICE (black 11), the ending
 * (white 16). A new groove comes in on the next bar (from silence at once: stopped, on the free clock from the next 1/16);
 * a fill from the next beat to the bar's end, the groove under it stopped there, then on (a long fill: a crash on the
 * next one). Stopped with the transport. Recording, its notes go into the clip as any generator's.
 * The groove plays from a copy (gp[t]) the drummer can change: DICE (the same drums, other hits), MUTATE (a hit or two
 * moved every two bars), FOLLOW (fewer hits when the others play little, ghosts when they play a lot); its white key
 * again brings the written one back. AUTO FILL: a fill every 4 / 8 / 16 bars, a long one before a new section.
 * The track's drv[]: 0 AUTO FILL (0 off, 1 4 bars, 2 8, 3 16), 1 MUTATE 0..100, 2 FOLLOW 0..100. */
#define GP_ROWS 8
typedef struct {
    uint8_t n, note[GP_ROWS], len[GP_ROWS], v[GP_ROWS][32], mod;   /* v: 0 none, else 1..9 */
} gpat_t;
static gpat_t gp[DK_NTRACK] DK_POOL;
volatile uint8_t dk_gr_cur[DK_NTRACK], dk_gr_next[DK_NTRACK] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, dk_gr_fill[DK_NTRACK];
volatile uint8_t dk_gr_mod[DK_NTRACK], dk_gr_dens[DK_NTRACK];   /* changed from the written one; FOLLOW's level 0..100 */
volatile uint32_t dk_gr_hits;                         /* (the drummer's notes so far: the tests) */
static uint8_t gr_fst[DK_NTRACK], gr_from[DK_NTRACK];   /* the fill: 1 this bar (from gr_from), 2 the next bar */
static uint32_t gr_bar0[DK_NTRACK];                    /* stopped: the free clock's tick its first step was */
static uint16_t gr_bars[DK_NTRACK];                    /* bars since it started (AUTO FILL, MUTATE) */
static uint16_t gr_act_at[DK_NTRACK];          /* FOLLOW: the other tracks' notes, counted; at the bar line */
static float gr_dens[DK_NTRACK];                       /* their density, smoothed, 0..1 */
static int groove_note(char c)
{
    switch (c) {
    case 'K': return 36;
    case 'S': return 38;
    case 'R': return 37;
    case 'P': return 39;
    case 'H': return 42;
    case 'O': return 46;
    case 'L': return 45;
    case 'M': return 47;
    case 'T': return 50;
    case 'X': return 49;
    case 'Y': return 51;
    case 'Z': return 70;
    case 'W': return 76;
    default: return -1;
    }
}
static int grand(int n) { return n > 0 ? (int)(dk_frand() * (float)n) % n : 0; }
static void groove_hit(int t, int note, int d, int step)
{
    int vel = 30 + 10 * d + grand(9) - 4;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    dk_gr_hits++;
    note_play(t, note, (float)vel * (1.0f / 127.0f), (int32_t)((float)DK_T16 * tick_len), jitter() / 3 + swing_delay(t, step * DK_T16), 2);
    if (rec_track_of(t))
        rec_done(t, note, vel, DK_T16);
}
/* the written groove into the copy (its full rows; the short ones are a fill's / the ending's) */
static void gp_load(int t, const dk_groove_t *g)
{
    gpat_t *p = &gp[t];
    const char *r = g->rows;
    p->n = 0;
    p->mod = 0;
    dk_gr_mod[t] = 0;
    while (*r && p->n < GP_ROWS) {
        const char *e = r + 1;
        int n, note = groove_note(*r), i;
        while (*e && *e != '|')
            e++;
        n = (int)(e - r) - 1;
        if (note >= 0 && n >= 16) {
            n = n >= 32 ? 32 : 16;
            p->note[p->n] = (uint8_t)note;
            p->len[p->n] = (uint8_t)n;
            for (i = 0; i < 32; i++)
                p->v[p->n][i] = (uint8_t)(i < n && r[1 + i] >= '1' && r[1 + i] <= '9' ? r[1 + i] - '0' : 0);
            p->n++;
        }
        r = *e ? e + 1 : e;
    }
}
/* a step that holds the groove up: the kick on the one, the snare's backbeats, the crash */
static int gp_strong(int note, int i)
{
    return (note == 36 && i % 16 == 0) || (note == 38 && (i % 16 == 4 || i % 16 == 12)) || note == 49;
}
static int gp_keeper(int note) { return note == 42 || note == 46 || note == 51 || note == 70; }   /* the hats, the ride, the shaker */
/* DICE: the same drums, other hits: each row keeps how many it has (its strong ones where they are), the rest put again
 * where that drum would go (the kick on 8ths, the snare's ghosts between, the hats their grid, an accent moved) */
static void gp_dice(int t)
{
    gpat_t *p = &gp[t];
    int r, i, bar;
    for (r = 0; r < p->n; r++) {
        int note = p->note[r], len = p->len[r];
        for (bar = 0; bar < len; bar += 16) {
            uint8_t *v = &p->v[r][bar], vel[16];
            int nv = 0, free = 0, k;
            if (gp_keeper(note)) {           /* the time keepers: the grid stays, a hit or two in or out, the accents moved */
                int sh = grand(3) * 2;
                uint8_t w[16];
                for (i = 0; i < 16; i++)
                    w[i] = v[(i + 16 - sh) % 16] && v[i] ? v[(i + 16 - sh) % 16] : v[i];
                for (i = 0; i < 16; i++)
                    v[i] = w[i];
                for (k = 0; k < 2; k++) {
                    i = grand(16);
                    if (v[i] && i % 4)
                        v[i] = 0;
                    else if (!v[i] && i % 2)
                        v[i] = 3;
                }
                continue;
            }
            for (i = 0; i < 16; i++)         /* the rest: keep the strong, gather the others' forces */
                if (v[i] && !gp_strong(note, i))
                    vel[nv++] = v[i], v[i] = 0;
            for (k = 0; k < nv; k++) {       /* each again on a free step that suits the drum */
                int tries;
                for (tries = 0; tries < 24; tries++) {
                    int c = note == 36 ? grand(8) * 2 + (grand(4) == 0) : note == 38 ? grand(16) : grand(8) * 2;
                    c &= 15;
                    if (!v[c] && !(note == 36 && (c == 4 || c == 12))) {
                        v[c] = note == 38 && vel[k] > 5 && c % 4 ? 2 : vel[k];   /* (an off snare: a ghost) */
                        break;
                    }
                }
                free += tries == 24;
            }
            (void)free;
        }
    }
    p->mod = 1;
    dk_gr_mod[t] = 1;
}
/* MUTATE: one or two hits moved a step or two, a ghost in, a weak one out */
static void gp_mutate(int t, int amount)
{
    gpat_t *p = &gp[t];
    int k, n = 1 + (amount > 60) + (amount > 90);
    if (!p->n)
        return;
    for (k = 0; k < n; k++) {
        int r = grand(p->n), len = p->len[r], i = grand(len), note = p->note[r], j, what = grand(3);
        uint8_t *v = p->v[r];
        if (note == 49)
            continue;
        if (what == 0 && v[i] && !gp_strong(note, i)) {   /* moved */
            j = (i + (grand(2) ? 1 : -1) * (1 + grand(2)) + len) % len;
            if (!v[j] && !gp_strong(note, j))
                v[j] = v[i], v[i] = 0;
        } else if (what == 1 && !v[i] && i % 2 && (note == 38 || gp_keeper(note)))   /* a ghost */
            v[i] = 2;
        else if (what == 2 && v[i] && v[i] <= 5 && !gp_strong(note, i))   /* a weak one out */
            v[i] = 0;
    }
    p->mod = 1;
    dk_gr_mod[t] = 1;
}
/* the rows of a written groove (`once`: its short rows only, at the one: a fill's after, the ending) */
static void groove_rows(int t, const dk_groove_t *g, uint32_t q, int step, int once)
{
    const char *r = g->rows;
    while (*r) {
        const char *e = r + 1;
        int n, note = groove_note(*r);
        while (*e && *e != '|')
            e++;
        n = (int)(e - r) - 1;
        if (note >= 0 && n > 0) {
            if (once) {
                if (n < 16 && r[1] >= '1' && r[1] <= '9')
                    groove_hit(t, note, r[1] - '0', 0);
            } else if (n >= 16) {
                char c = r[1 + (int)(q % (uint32_t)(n >= 32 ? 32 : 16))];
                if (c >= '1' && c <= '9')
                    groove_hit(t, note, c - '0', step);
            }
        }
        r = *e ? e + 1 : e;
    }
}
/* the copy at 1/16 q, FOLLOW's level on it: below the middle the weak hits thin out, above it ghosts come in */
static void gp_play(int t, uint32_t q, int step)
{
    const gpat_t *p = &gp[t];
    int r, fol = dk_song.trk[t].drv[2];
    float lvl = 0.5f + (gr_dens[t] - 0.5f) * (float)(fol > 100 ? 100 : fol) * 0.01f;   /* 0.5: as written */
    for (r = 0; r < p->n; r++) {
        int note = p->note[r], i = (int)(q % p->len[r]), d = p->v[r][i];
        if (d) {
            if (fol && lvl < 0.5f && !gp_strong(note, i) && (i % 4 || d <= 5) && dk_frand() > lvl * 2.0f)
                continue;                    /* the others quiet: the drummer plays less */
            groove_hit(t, note, d, step);
        } else if (fol && lvl > 0.5f && step % 2 && dk_frand() < (lvl - 0.5f) * 0.7f) {
            if (note == 42 || (note == 38 && dk_frand() < 0.4f))
                groove_hit(t, note, note == 38 ? 2 : 3, step);   /* the others busy: ghosts, the hat in 16ths */
        }
    }
}
static int fill_first(const dk_groove_t *f)   /* the step its notes start (the full rows) */
{
    const char *r = f->rows;
    int first = 15;
    while (*r) {
        const char *e = r + 1;
        int n, i;
        while (*e && *e != '|')
            e++;
        n = (int)(e - r) - 1;
        for (i = 0; n >= 16 && i < 16; i++)
            if (r[1 + i] != '.') {
                first = i < first ? i : first;
                break;
            }
        r = *e ? e + 1 : e;
    }
    return first;
}
static uint32_t groove_pos(int t) { return dk_playing ? part_tick : gt - gr_bar0[t]; }
static void groove_fill_at(int t, int b, uint32_t pos);
static int groove_section(int g) { return g ? (g - 1) / 3 : -1; }
static void groove_tick(int t)
{
    uint32_t pos = groove_pos(t), q;
    int step;
    if (!TAPE.groove || pos % DK_T16)
        return;
    q = pos / DK_T16;
    step = (int)(q % 16u);
    if (step == 0) {                           /* the bar line */
        const dk_track_t *k = &dk_song.trk[t];
        uint16_t act = (uint16_t)(gr_act - gr_act_at[t]);
        gr_act_at[t] = gr_act;                 /* FOLLOW: the others' notes this bar (16 a bar: busy) */
        gr_dens[t] += ((float)(act > 24 ? 24 : act) / 24.0f - gr_dens[t]) * 0.5f;
        dk_gr_dens[t] = (uint8_t)(gr_dens[t] * 100.0f);
        if (gr_fst[t] == 1) {                  /* the fill done (its crash) */
            if (dk_gr_fill[t] && dk_gr_fill[t] <= TAPE.nfill)
                groove_rows(t, &TAPE.fill[dk_gr_fill[t] - 1], q, 0, 1);
            gr_fst[t] = 0;
            dk_gr_fill[t] = 0;
        } else if (gr_fst[t] == 2)
            gr_fst[t] = 1;
        if (dk_gr_next[t] != 0xFF) {           /* the groove queued */
            if (dk_gr_next[t] == 0) {
                if (dk_gr_cur[t] && TAPE.groove_end)
                    groove_rows(t, TAPE.groove_end, q, 0, 1);
                dk_gr_cur[t] = 0;
                gr_fst[t] = 0;
                dk_gr_fill[t] = 0;
            } else if (dk_gr_next[t] <= TAPE.ngroove) {
                dk_gr_cur[t] = dk_gr_next[t];
                gp_load(t, &TAPE.groove[dk_gr_cur[t] - 1]);
                gr_bars[t] = 0;
            }
            dk_gr_next[t] = 0xFF;
        } else if (dk_gr_cur[t]) {
            gr_bars[t]++;
            if (k->drv[1] && gr_bars[t] % 2 == 0 && grand(100) < k->drv[1])
                gp_mutate(t, k->drv[1]);
            if ((k->drv[0] & 3) && !gr_fst[t] && TAPE.nfill) {   /* AUTO FILL: in the last bar of 4 / 8 / 16 */
                int every = 4 << ((k->drv[0] & 3) - 1);
                if ((gr_bars[t] + 1) % every == 0)
                    groove_fill_at(t, 1 + grand(TAPE.nfill > 4 ? 4 : TAPE.nfill), pos);
            }
        }
    }
    if (!dk_gr_cur[t])
        return;
    if (gr_fst[t] == 1 && step >= gr_from[t] && dk_gr_fill[t] && dk_gr_fill[t] <= TAPE.nfill) {
        groove_rows(t, &TAPE.fill[dk_gr_fill[t] - 1], (uint32_t)step, step, 0);
        return;
    }
    gp_play(t, q, step);
}
/* a fill b (1..): from the next beat, or where its notes start; too late in the bar: the whole next bar */
static void groove_fill_at(int t, int b, uint32_t pos)
{
    uint32_t q = (pos + DK_T16 - 1) / DK_T16;
    int step = (int)(q % 16u), nb = (step + 3) / 4 * 4, first = fill_first(&TAPE.fill[b - 1]);
    dk_gr_fill[t] = (uint8_t)b;
    if (q % 16u == 0 && pos % DK_T16 == 0)
        nb = 0;
    if (nb >= 16 || (step > first && nb > 12)) {
        gr_fst[t] = 2;
        gr_from[t] = (uint8_t)first;
    } else {
        gr_fst[t] = 1;
        gr_from[t] = (uint8_t)(nb > first ? nb : first);
    }
}
static void play_start(void);
/* a GROOVE track of the set (bits) whose drummer plays */
static int groove_in(int mask)
{
    int t;
    for (t = 0; t < DK_NTRACK; t++)
        if ((mask >> t & 1) && dk_song.trk[t].seq == SQ_GROOVE && dk_gr_cur[t])
            return 1;
    return 0;
}
/* C_GROOVE: kind 0 a groove (b 1..15; 0 the ending), 1 a fill (b 1..), 2 off at once, 3 DICE */
static void groove_cmd(int t, int kind, int b)
{
    t %= DK_NTRACK;
    if (kind == 2) {
        dk_gr_cur[t] = 0, dk_gr_next[t] = 0xFF, dk_gr_fill[t] = 0, gr_fst[t] = 0;
        return;
    }
    if (kind == 3) {
        if (dk_gr_cur[t])
            gp_dice(t);
        return;
    }
    if (kind == 0) {
        if (b > TAPE.ngroove)
            return;
        if (b && rec_wait_note && !dk_playing) {   /* REC waiting for the first note: a groove is one - the take starts with it */
            rec_wait_note = 0;
            play_start();
            rec_on_now();
            dk_gr_cur[t] = (uint8_t)b;
            dk_gr_next[t] = 0xFF;
            gp_load(t, &TAPE.groove[b - 1]);
            gr_bars[t] = 0;
            return;
        }
        if (!dk_gr_cur[t] && b) {              /* from silence: at once (playing: where the bar is; stopped: its bar from the next 1/16) */
            if (!dk_playing)
                gr_bar0[t] = gt + (DK_T16 - gt % DK_T16) % DK_T16;
            dk_gr_cur[t] = (uint8_t)b;
            dk_gr_next[t] = 0xFF;
            gp_load(t, &TAPE.groove[b - 1]);
            gr_bars[t] = 0;
            return;
        }
        if (b && (dk_song.trk[t].drv[0] & 3) && !gr_fst[t] && groove_section(b) != groove_section(dk_gr_cur[t]) &&
            TAPE.nfill > 4)                    /* AUTO FILL: a new section: a long fill into it */
            groove_fill_at(t, 5 + grand(TAPE.nfill - 4), groove_pos(t));
        dk_gr_next[t] = (uint8_t)b;            /* on the next bar (the same one again: the written one back) */
        return;
    }
    if (!dk_gr_cur[t] || b < 1 || b > TAPE.nfill)
        return;
    groove_fill_at(t, b, groove_pos(t));
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
/* one pitch, however many keys hold it: letting the old chord go must not take a note the new chord still holds
 * (that left only the low one and the high one). a full pattern drops the oldest pitch the new chord does not use */
static void arp_drop_at(int t, int drop)
{
    int i;
    for (i = drop + 1; i < arp_n[t]; i++) {
        arp_order[t][i - 1] = arp_order[t][i];
        arp_cnt[t][i - 1] = arp_cnt[t][i];
    }
    arp_n[t]--;
}
static void arp_add(int t, int note)
{
    int i;
    note &= 127;
    for (i = 0; i < arp_n[t]; i++)
        if (arp_order[t][i] == (uint8_t)note) {
            if (arp_cnt[t][i] < 255)
                arp_cnt[t][i]++;
            return;
        }
    if (arp_n[t] >= 8) {
        int drop = 0;
        for (i = 0; i < arp_n[t]; i++)
            if (arp_cnt[t][i] <= 1) {
                drop = i;
                break;
            }
        arp_drop_at(t, drop);
    }
    arp_order[t][arp_n[t]] = (uint8_t)note;
    arp_cnt[t][arp_n[t]] = 1;
    arp_n[t]++;
}
static void arp_sub(int t, int note)
{
    int i;
    for (i = 0; i < arp_n[t]; i++)
        if (arp_order[t][i] == (uint8_t)note) {
            if (arp_cnt[t][i] > 1) {
                arp_cnt[t][i]--;
                return;
            }
            arp_drop_at(t, i);
            return;
        }
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
            auto_hold_t[t][k] = 0;
        }
    for (t = 0; t < DK_NTRACK; t++)
        ramp_own[t] = 0;
}

/* --------------------------------------------------------------- transport --- */
/* (rec_loop: a take asked for - see rec_stop) */
static int song_on(void) { return dk_song.play_mode && dk_song.nroad && !rec_loop; }
static void block_enter(void)
{
    const dk_block_t *b;
    if (song_on()) {
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
    if (song_on()) {
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
    rec_loop = 0;                              /* (the stop gives the road back) */
    pm_apply();
    dk_playing = 0;
    dk_stems = 0;
    for (i = 0; i < DK_NTRACK; i++)        /* GROOVE: the drummer stops with the song */
        groove_cmd(i, 2, 0);
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

/* a mono lead's keys held (the hand's), the last on top: let the top one go and the voice slides back to the one still
 * held, as a mono synth does (EF_LEGATO) */
static uint8_t mono_stk[DK_NTRACK][8], mono_n[DK_NTRACK], mono_vel[DK_NTRACK];
static void mono_drop(int t, int note)
{
    int i, j = 0;
    for (i = 0; i < mono_n[t]; i++)
        if (mono_stk[t][i] != note)
            mono_stk[t][j++] = mono_stk[t][i];
    mono_n[t] = (uint8_t)j;
}
static void live_note(int t, int note, int vel, int delay)
{
    dk_track_t *k = &dk_song.trk[t];
    const dk_engine_t *e = dk_engine(t);
    if ((unsigned)t >= DK_NTRACK)
        return;
    if (vel && !(e->flags & EF_DRUM))
        gr_act++;                              /* (GROOVE's FOLLOW: the hands count too) */
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
            if (!arp_keys[t]) {                /* a new hand from silence: the pattern starts again from its first step.
                                                * latched notes are replaced (the same key stops them before it gets here) */
                arp_step[t] = 0;
                arp_n[t] = 0;
            }
            arp_keys[t]++;
            arp_add(t, note);
            arp_publish(t);
            return;
        }
        if (e->flags & EF_LEGATO) {
            mono_drop(t, note);
            if (mono_n[t] >= 8)
                mono_drop(t, mono_stk[t][0]);
            mono_stk[t][mono_n[t]++] = (uint8_t)note;
            mono_vel[t] = (uint8_t)vel;
        }
        voice_start(t, note, (float)vel * (1.0f / 127.0f), -1, delay, 0, 0);
        if (rec_track_of(t))
            rec_open(t, note, vel, delay);
        else if (rec_pre_track(t)) {           /* a touch early: kept for the take's first beat */
            pre_vel[note] = (uint8_t)vel;
            pre_trk[note] = (uint8_t)t;
            pre_dur[note] = -1;
            pre_at[note] = count_left;
        }
    } else {
        held_pcs[t] &= ~(1u << (note % 12));
        if (dk_arp_on(k) && !(dk_engine(t)->flags & EF_DRUM)) {
            if (arp_keys[t])
                arp_keys[t]--;
            if (!k->arp[3])                    /* latched: the notes stay until the same key (or a new hand) */
                arp_sub(t, note);
            arp_publish(t);
            return;
        }
        if (e->flags & EF_LEGATO) {
            int top = mono_n[t] ? mono_stk[t][mono_n[t] - 1] : -1, i, back = 0;
            mono_drop(t, note);
            if (top == note && mono_n[t])          /* the top key let go, another still held: back to it, legato */
                for (i = 0; i < DK_NVOICE && !back; i++)
                    if (vc[i].on && vc[i].held && vc[i].trk == t && vc[i].note == note && vc[i].gate < 0 && !vc[i].src)
                        back = 1;
            if (back) {
                voice_start(t, mono_stk[t][mono_n[t] - 1], (float)mono_vel[t] * (1.0f / 127.0f), -1, 0, 0, 0);
                if (rec_track_of(t))
                    rec_close(note, 0);
                return;
            }
        }
        voices_off_note(t, note);
        if (rec_track_of(t))
            rec_close(note, 0);
        else if (pre_vel[note] && pre_dur[note] < 0)   /* an early one let go in the count-in: its length */
            pre_dur[note] = (int16_t)(pre_at[note] - count_left);
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
/* METRO ALWAYS, transport stopped: the same click, on the free clock */
static void metro_free(void)
{
    static const uint8_t DV[3] = {DK_PPQ, DK_PPQ / 2, DK_PPQ / 4};
    int dv = DV[dk_metro.div % 3];
    if (gt % (uint32_t)dv)
        return;
    if (gt % DK_PPQ == 0)
        metro_click(gt % DK_TPB == 0);
    else {
        metro_click(0);
        clk.env *= 0.4f;
    }
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
                rec_pre_flush();
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
            const dk_engine_t *e = dk_engine(t);
            if (dk_arp_on(k))
                arp_tick(t);
            if (dk_fill && (e->flags & EF_DRUM))   /* FILL held: the building bar, instead of the groove */
                fill_tick(t);
            else if (k->seq == SQ_GROOVE)
                groove_tick(t);
        }
        if (dk_metro.mode == 2)            /* METRO ALWAYS clicks with the transport stopped too */
            metro_free();
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
    block_last = (uint8_t)(song_on() &&
                           dk_road_rep + 1 >= (dk_song.road[dk_road_i].reps ? dk_song.road[dk_road_i].reps : 1));
    if (song_on() && (tricks & TR_STOP) && block_last && part_tick >= part_len_of(cur_part) - DK_PPQ)
        stop_to = 0.0f;                        /* TAPESTOP: the block's last beat */
    for (t = 0; t < DK_NTRACK; t++) {
        const dk_track_t *k = &dk_song.trk[t];
        const dk_engine_t *e = dk_engine(t);
        int fill = dk_fill && (e->flags & EF_DRUM);
        if (fill)
            fill_tick(t);
        else if (k->seq == SQ_GROOVE) {      /* the drummer, or (silent) the clip: what was recorded of it */
            if (!(drop_mask >> t & 1u))
                groove_tick(t);
            if (!dk_gr_cur[t])
                clip_tick(t, t == dk_song.harm_track ? &hp : 0);
        } else if (k->seq == SQ_ENDLESS) {
            if (!(drop_mask >> t & 1u))
                endless_tick(t);
        } else
            clip_tick(t, t == dk_song.harm_track ? &hp : 0);
        auto_tick(t);
        if (dk_arp_on(k))
            arp_tick(t);
    }
    if (auto_dirty) {
        auto_dirty = 0;
        params_update();
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
        if (!song_on() && queued != 0xFF) {   /* PART: the queued part, from the bar's end */
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
        if (clen > 0 && part_tick % clen == 0) {
            int i;
            rec_close_all();
            if (dk_pass < 250)
                dk_pass++;
            for (i = 0; i < DK_NTRACK; i++)
                if ((dk_rec_mask >> i & 1u) && dk_rec_take[i] && dk_rec_take[i] < 31)
                    dk_rec_take[i]++;          /* the floor stays: REPLACE still means "what was there before this press" */
        }
        open_publish();
    }
    if (part_tick >= part_len_of(cur_part)) {
        part_tick = 0;
        part_loops++;
        if (song_on())
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
                rec_loop = 1;                  /* (the take's part goes round, SONG or not) */
                drop_mask = 0;                 /* (and plain: a ROAD block's DROP and tricks do not silence what is recorded) */
                tricks = 0;
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
                rec_takes_begin(m);
                if (bake) {
                    rec_bake = (uint8_t)(bake + (dk_playing ? 1 : 0));
                    if (!dk_playing)
                        play_start();
                    rec_on_now();
                } else if (dk_playing)
                    rec_on_now();
                else if (groove_in(m)) {       /* a drummer playing (stopped, its own clock): the take starts now, from the bar */
                    play_start();
                    rec_on_now();
                } else if (dk_metro.on_note)
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
            if (b) {                           /* SELECT while recording: this track records too; its old takes stay theirs */
                rec_take_join(a % DK_NTRACK);
                break;
            }
            if (dk_recording)
                rec_close_all();
            dk_rec_trk = (uint8_t)(a % DK_NTRACK);
            dk_rec_mask = (uint8_t)(dk_rec_mask | 1u << dk_rec_trk);
            dk_pass = (uint8_t)(CLIP(cur_part, dk_rec_trk)->layers + 1);
            {
                int i, p = take_clamp(dk_pass);
                for (i = 0; i < DK_NTRACK; i++)
                    if (dk_rec_mask >> i & 1u)
                        dk_rec_take[i] = (uint8_t)p;
                dk_rec_floor[dk_rec_trk] = (uint8_t)p;
            }
            break;
        case C_RECMASK:                        /* a = the armed tracks, changed while it records: a track let go closes its notes */
            if (dk_recording) {
                int i, m = (a & 0x3F) | 1 << dk_rec_trk;
                for (i = 0; i < 128; i++)
                    if (open_pos[i] >= 0 && !(m >> open_trk[i] & 1))
                        rec_close(i, 1);
                for (i = 0; i < DK_NTRACK; i++) /* armed now, and not already writing: its own layer, not the one it already has */
                    if ((m >> i & 1u) && !dk_rec_take[i])
                        rec_take_join(i);
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
            if (!dk_playing || b) {            /* stopped, or at once (b 2: the loop stays, the take goes on) */
                int keep = b == 2 && dk_playing;
                if (keep)
                    rec_split_close();         /* held notes end here, in this part; they open again after the part changes */
                cur_part = (uint8_t)a;
                dk_song.cur_part = (uint8_t)a;
                queued = 0xFF;
                dk_next_part = 0xFF;
                if (keep) {
                    int len = part_len_of(cur_part);
                    if (len > 0 && part_tick >= (uint16_t)len)
                        part_tick = (uint16_t)(part_tick % (uint16_t)len);
                    dk_cur_part = cur_part;
                    dk_part_len = (uint16_t)len;
                    rec_split_open();
                } else {
                    if (dk_playing)
                        part_tick = 0;
                    block_enter();
                }
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
        case C_GROOVE:                         /* GROOVE: track a, kind b, which d */
            groove_cmd(a, b, c.d);
            break;
        case C_ENDLTR:                         /* ENDLESS: the transposition (semitones) of track a */
            dk_endl_tr[a % DK_NTRACK] = (int8_t)(b < -48 ? -48 : b > 48 ? 48 : b);
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
