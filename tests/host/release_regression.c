/* SPDX-License-Identifier: MPL-2.0 */
/* Regression: normal-load releases must survive synthetic budget limits;
 * emergency retirement is smoothed without rendering another oscillator. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../../deck/core.c"
void dk_midi_out(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; }
static int32_t out_[512];
static void blocks(int n) { while (n--) dk_render(out_,256,4096); }
int main(void)
{
    int t, j, n=0;
    float err=0.0f;
    dk_init(); dk_song_clear();
    for(j=-20000;j<=20000;j++) {
        float x=(float)j/1000.0f, e=fm_fabsf(dk_tanh(x)-fm_tanhf(x));
        if(e>err) err=e;
    }
    printf("saturation max error %.8f\n",err);
    assert(err<0.00006f);
    for(t=0;t<6;t++) {
        dk_track_setup(t,2,0,SQ_LIVE,R_PAD);
        dk_song.trk[t].fx=0;
        for(j=0;j<4;j++) dk_note(t,48+4*j,100,0);
    }
    blocks(150);
    for(t=0;t<6;t++) for(j=0;j<4;j++) dk_note(t,48+4*j,0,0);
    dk_budget_force(60); dk_load_force(40);
    blocks(4);
    for(j=0;j<DK_NVOICE;j++) n+=vc[j].on;
    printf("normal-load released pad voices %d (expected 24)\n",n);
    assert(n==24);
    /* A lock must not execute pending REC transitions while song.c takes a snapshot. */
    dk_cmd(C_REC,1,0,0); dk_lock(); blocks(1); assert(q_r!=q_w); dk_unlock();
    /* no oscillator copy needed to suppress an emergency cut's discontinuity */
    vc[0].on=1; vc[0].delay=0; vc[0].trk=0;
    vc[0].last_l=0.1f; vc[0].last_r=-0.1f;
    voice_retire(&vc[0]);
    assert(!vc[0].on && retire_l[0]>=0.1f && retire_r[0]<=-0.1f);
    dk_unlock();
    for(j=0;j<DK_NVOICE;j++) vc[j].on=0;
    dk_track_setup(1,4,0,SQ_LIVE,R_BASS);
    note_take=1; assert(voice_start(1,36,0.7f,44100,0,1,0));
    note_take=2; assert(voice_start(1,43,0.7f,44100,0,1,0));
    n=0; for(j=0;j<DK_NVOICE;j++) n+=vc[j].on && vc[j].trk==1;
    assert(n==2);
    note_take=1; assert(voice_start(1,38,0.7f,44100,0,1,0));
    n=0; for(j=0;j<DK_NVOICE;j++) n+=vc[j].on && vc[j].trk==1;
    assert(n==2); /* legato stays within its take */
    for(j=0;j<DK_NVOICE;j++) vc[j].on=0;
    dk_track_setup(0,2,0,SQ_LIVE,R_PAD);
    note_take=1;
    for(j=0;j<4;j++) assert(voice_start(0,48+4*j,0.6f,44100,0,1,0));
    for(j=0;j<DK_NVOICE;j++) if(vc[j].on) voice_release(&vc[j]);
    for(j=0;j<4;j++) assert(voice_start(0,50+4*j,0.6f,44100,0,1,0));
    n=0;for(j=0;j<DK_NVOICE;j++) n+=vc[j].on;
    assert(n==8); /* a fresh chord must not consume the previous chord's release */
    note_take=2;
    for(j=0;j<4;j++) assert(voice_start(0,52+4*j,0.6f,44100,0,1,0));
    n=0;for(j=0;j<DK_NVOICE;j++) n+=vc[j].on;
    assert(n==12); /* another overdub has its own chord allowance */
    puts("DSP release/lock/declick/mono and poly overdub regression: OK");
    return 0;
}
