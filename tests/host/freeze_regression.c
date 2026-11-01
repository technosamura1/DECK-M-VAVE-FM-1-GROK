#include <assert.h>
#include <stdio.h>
#include "../../deck/song.c"
void dk_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static void queue(int take, int pos)
{
    dk_recev_t r = {0};
    r.pass = (uint8_t)take;
    r.ev.pos = (uint16_t)pos;
    r.ev.dur = 12;
    r.ev.note = 60;
    r.ev.vel = 100;
    r.ev.layer = DK_L_MAKE(take, 0);
    dk_recq[dk_recq_w++ % DK_RECQ] = r;
}
int main(void)
{
    int i;
    dk_init();
    dk_song_clear();
    rec_reset();
    dk_lock_take = 1;
    dk_recording = 1;
    dk_pass = dk_rec_take[0] = 3;
    queue(1, 0); queue(2, 24); queue(3, 48);
    rec_flush();
    assert(dk_song.clip[0].n == 3);
    for (i = 0; i < 3; i++)
        assert(!!DK_L_SND(dk_ev[i].layer) == (i < 2));
    assert(take_pend[0] == (1u << 3));
    dk_recording = 0;
    rec_flush();
    assert(DK_L_SND(dk_ev[2].layer));
    assert(!take_pend[0]);
    puts("freeze regression: ok");
    return 0;
}
