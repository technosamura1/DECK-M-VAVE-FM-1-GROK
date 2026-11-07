/* SPDX-License-Identifier: MPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include "song.h"
#include "../../deck/core.c"
void dk_midi_out(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;}
static void queue_take(int take,int pos){
    dk_recev_t r={0};r.pass=(uint8_t)take;r.ev.pos=(uint16_t)pos;
    r.ev.dur=96;r.ev.note=60;r.ev.vel=96;r.ev.layer=DK_L_MAKE(take,0);
    dk_recq[dk_recq_w++%DK_RECQ]=r;
}
int main(int argc,char **argv){
    int id1,id2,i,j;int32_t out[256];float send=0;
    dk_init();dk_song_clear();rec_reset();dk_lock_take=1;
    assert(dk_song.trk[5].seq==SQ_GROOVE);
    dk_track_setup(0,0,0,SQ_LIVE,R_KEYS);
    dk_song.trk[0].fx=1;dk_song.trk[0].fxamt=37;dk_song.trk[0].fxa=23;dk_song.trk[0].fxb=61;
    dk_song.trk[0].rev=62;dk_song.trk[0].echo=18;dk_song.trk[0].trim=-6;
    dk_recording=0;queue_take(1,0);rec_flush();
    id1=DK_L_SND(dk_ev[dk_song.clip[0].at].layer);
    assert(id1);
    assert(dk_song.snd[0][id1-1].rsv==1);
    assert(dk_song.snd[0][id1-1].rsv2[0]==37);
    assert(dk_song.snd[0][id1-1].rsv2[3]==62);
    assert((dk_song.snd[0][id1-1].used&63)-32==-6);
    track_sound_to(0);track_preset(0,1);
    dk_song.trk[0].fx=2;dk_song.trk[0].fxamt=70;
    queue_take(2,96);rec_flush();
    id2=DK_L_SND(dk_ev[dk_song.clip[0].at+1].layer);
    assert(id2 && id1!=id2);
    assert(dk_song.snd[0][id1-1].rsv==1 && dk_song.snd[0][id2-1].rsv==2);
    if(argc>1){
        uint32_t hash=2166136261u;
        if(argv[1][0]=='b'){
            dk_song.trk[0].eng=1;dk_song.trk[0].fx=4;
            dk_song.trk[0].fxamt=99;dk_song.trk[0].rev=0;dk_song.trk[0].echo=0;
            dk_song.trk[0].trim=20;
        }
        assert(voice_start(0,60,0.7f,44100,0,1,id1));
        for(i=0;i<400;i++){
            dk_render(out,128,4096);
            for(j=0;j<256;j++)hash=(hash^(uint32_t)out[j])*16777619u;
        }
        printf("%08x\n",hash);return 0;
    }
    dk_song.trk[0].fx=0;dk_song.trk[0].rev=dk_song.trk[0].echo=0;
    assert(voice_start(0,60,0.7f,44100,0,1,id1));
    assert(voice_start(0,67,0.7f,44100,0,1,id2));
    assert(voice_start(0,72,0.7f,44100,0,0,0));
    for(i=0;i<100;i++){
        dk_render(out,128,4096);
        for(j=0;j<128;j++)send+=rsend[j]*rsend[j];
    }
    assert(fx_prev[id1]==1 && fx_prev[id2]==2);
    assert(fxs[id1].buf && fxs[id2].buf && fxs[id1].buf!=fxs[id2].buf);
    assert(send>0.001f);
    assert(sizeof(dk_snd_t)==20); /* keep existing project layout */
    puts("PASS: GROOVE default, two piano presets with independent FX/trim/sends, third live sound, compatible snapshot size");
    return 0;
}
