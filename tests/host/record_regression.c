/* SPDX-License-Identifier: MPL-2.0 */
/* Native host regression; core is included to exercise the sequencer's private state.
 * gcc -O2 -std=c99 -DDK_HOST -Ibase/include -Ideck -Itapes/overworld
 *   tests/host/record_regression.c deck/song.c deck/dsp_unit.c -lm
 *   -o build/test/record_regression.exe
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../deck/core.c"
void dk_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{ (void)st; (void)d1; (void)d2; }

static void reset(void)
{
    int t;
    memset(&dk_song, 0, sizeof(dk_song));
    memset(vc, 0, sizeof(vc));
    memset(pre_vel, 0, sizeof(pre_vel));
    memset(rec_keys.live.flags, 0, sizeof(rec_keys.live.flags));
    memset(arp_n, 0, sizeof(arp_n));
    memset(mono_n, 0, sizeof(mono_n));
    dk_song.tempo = 120;
    dk_song.part[0].bars = dk_song.part[1].bars = 4;
    cur_part = 0; part_tick = 0; queued = 0xFF;
    count_left = rec_start_after = rec_wait_note = rec_bake = 0;
    rec_loop = 1; rec_nocomp = 0;
    dk_playing = dk_recording = 1;
    dk_rec_trk = 0; dk_rec_mask = 3; dk_pass = 1;
    dk_recq_w = dk_rec_r = dk_rec_hits = dk_rec_dropped = 0;
    q_r = q_w = 0; dk_rec_lat = 0;
    for (t = 0; t < DK_NTRACK; t++) {
        dk_song.trk[t].qnt = 0;
        dk_song.clip[t].len = DK_TPB;
    }
    seq_init();
    tick_left = 0;
    rec_takes_begin(3);
}
static void command(int c, int a, int b, int d)
{ dk_cmd(c, a, b, d); drain(); }
static void event(int index, int clip, int pos, int dur, int note, int vel, int pass)
{
    const volatile dk_recev_t *r = &dk_recq[index];
    assert((uint32_t)index < dk_recq_w);
    if (r->clip != clip || r->ev.pos != pos || r->ev.dur != dur)
        fprintf(stderr, "event %d actual clip=%d pos=%d dur=%d expected %d/%d/%d\n", index, r->clip, r->ev.pos, r->ev.dur, clip, pos, dur);
    assert(r->clip == clip && r->ev.pos == pos && r->ev.dur == dur);
    assert(r->ev.note == note && r->ev.vel == vel && r->pass == pass);
}
static void same_pitch(void)
{
    reset();
    part_tick = 10; live_note(0, 60, 80, 0);
    part_tick = 20; live_note(1, 60, 100, 0);
    part_tick = 40; live_note(0, 60, 0, 0);
    assert(open_pos[128 + 60] == 20);
    part_tick = 70; live_note(1, 60, 0, 0);
    assert(dk_recq_w == 2);
    event(0, 0, 10, 30, 60, 80, 1);
    event(1, 1, 20, 50, 60, 100, 1);
}
static void count_in(void)
{
    reset(); dk_recording = 0; dk_playing = 0;
    rec_start_after = 1; count_left = 20;
    live_note(0, 60, 80, 0); live_note(1, 60, 100, 0);
    count_left = 10; live_note(0, 60, 0, 0);
    count_left = 0; dk_recording = dk_playing = 1;
    rec_pre_flush();
    event(0, 0, 0, DK_T16 / 2, 60, 80, 1);
    assert(open_pos[128 + 60] == 0 && open_pos[60] < 0);
    part_tick = 30; live_note(1, 60, 0, 0);
    event(1, 1, 0, 30, 60, 100, 1);
}
static void loops(void)
{
    reset();
    CLIP(0, 0)->len = DK_TPB * 2;
    part_tick = 10; rec_open(0, 60, 80, 0); rec_open(1, 60, 100, 0);
    part_tick = DK_TPB - 1; seq_tick();
    assert(dk_recq_w == 1 && open_pos[60] == 10);
    assert(dk_rec_take[0] == 1 && dk_rec_take[1] == 2 && dk_pass == 1);
    event(0, 1, 10, DK_TPB - 10, 60, 100, 1);
    part_tick = DK_TPB + 20; rec_close(1, 60, 0);
    event(1, 1, 0, 20, 60, 100, 2);
    part_tick = 2 * DK_TPB - 1; seq_tick();
    event(2, 0, 10, 2 * DK_TPB - 10, 60, 80, 1);
    assert(dk_rec_take[0] == 2 && dk_rec_take[1] == 3 && dk_pass == 2);
    part_tick = 2 * DK_TPB + 30; rec_close(0, 60, 0);
    event(3, 0, 0, 30, 60, 80, 2);
    reset();
    part_tick = 10; rec_open(1, 60, 90, 0);
    part_tick = DK_TPB - 1; seq_tick();
    part_tick = 2 * DK_TPB - 1; seq_tick();
    part_tick = 2 * DK_TPB + 5; rec_close(1, 60, 0);
    assert(dk_recq_w == 2);
    event(1, 1, 0, DK_TPB, 60, 90, 3);
    /* A part reset also wraps clips longer than, or not dividing, the part. */
    reset(); dk_song.part[0].bars = 1; CLIP(0, 0)->len = 2 * DK_TPB;
    part_tick = 10; rec_open(0, 60, 80, 0);
    part_tick = DK_TPB - 1; seq_tick();
    event(0, 0, 10, DK_TPB - 10, 60, 80, 1);
    assert(part_tick == 0 && open_pos[60] == 0);
}
static void switches(void)
{
    int i;
    reset(); CLIP(1, 0)->len = DK_TPB / 2;
    part_tick = 10; rec_open(0, 60, 80, 0);
    part_tick = 20; rec_open(1, 60, 100, 0);
    command(C_PART, 1, 0, 0);
    part_tick = DK_TPB - 1; seq_tick();
    event(0, 0, 10, DK_TPB - 10, 60, 80, 1);
    event(1, 1, 20, DK_TPB - 20, 60, 100, 1);
    assert(cur_part == 1 && open_pos[60] == 0 && open_pos[188] == 0);
    part_tick = 30; rec_close(0, 60, 0); rec_close(1, 60, 0);
    event(2, DK_NTRACK, 0, 30, 60, 80, 1);
    event(3, DK_NTRACK + 1, 0, 30, 60, 100, 1);
    /* A queued bar switch inside a longer source clip closes at the bar,
     * without latency compensation, and the destination starts exactly at 0. */
    reset(); CLIP(0, 0)->len = 2 * DK_TPB;
    part_tick = 10; rec_open(0, 60, 80, 0);
    dk_rec_lat = 10; dk_song.trk[0].qnt = 100;
    command(C_PART, 1, 0, 0);
    part_tick = DK_TPB - 1; seq_tick();
    event(0, 0, 10, DK_TPB - 10, 60, 80, 1);
    assert(open_pos[60] == 0);
    part_tick = 30; rec_close(0, 60, 0);
    event(1, DK_NTRACK, 0, 30, 60, 80, 1);
    reset();
    part_tick = 10;
    for (i = 40; i < 80; i++) rec_open(i % 2, i, 90, 0);
    part_tick = 50; command(C_PART, 1, 2, 0);
    assert(dk_recq_w == 40);
    part_tick = 70; rec_close_all();
    assert(dk_recq_w == 80);
    for (i = 40; i < 80; i++) assert(open_pos[(i % 2) * 128 + i] < 0);
    reset(); part_tick = 10; rec_open(0, 60, 80, 0);
    part_tick = 50; command(C_PART, 1, 1, 0);
    event(0, 0, 10, 40, 60, 80, 1);
    assert(open_pos[60] == 0 && cur_part == 1);
}
static void endings(void)
{
    reset(); part_tick = 10; rec_open(0, 60, 80, 0);
    part_tick = 50; command(C_REC, 0, 0, 0);
    event(0, 0, 10, 40, 60, 80, 1);
    assert(!dk_recording && open_pos[60] < 0);
    reset(); part_tick = 10; rec_open(0, 60, 80, 0); rec_open(1, 60, 100, 0);
    part_tick = 50; command(C_RECMASK, 1, 0, 0);
    event(0, 1, 10, 40, 60, 100, 1);
    assert(open_pos[60] == 10 && open_pos[188] < 0);
    part_tick = 70; command(C_PLAY, 0, 0, 0);
    event(1, 0, 10, 60, 60, 80, 1);
    assert(!dk_playing && !dk_recording);
    reset(); part_tick = 10; rec_open(0, 60, 80, 0);
    /* Fractional clock and latency compensation must survive punch-out. */
    dk_rec_lat = 10; part_tick = 50; tick_left = tick_len * 0.5f;
    { int expected = (int)clip_heard(0, 0) - 10;
      rec_stop(); event(0, 0, 10, expected, 60, 80, 1); }
}
int main(void)
{
    dk_init();
    assert(sizeof(rec_keys) <= 6144);
    same_pitch(); count_in(); loops(); switches(); endings();
    puts("record regression: same-pitch tracks, count-in, independent loops, held carries, part switches, punch-out/disarm PASS");
    return 0;
}
