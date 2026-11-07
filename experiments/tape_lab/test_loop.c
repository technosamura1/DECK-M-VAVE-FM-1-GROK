/* SPDX-License-Identifier: MPL-2.0 */
#include "loop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define N 44100u
static int16_t a[N+2],b[N+2],original[N],mixed[N];
static tape_loop t;
static FILE *wav;
static uint32_t frames;
static void u32(uint32_t x){fwrite(&x,4,1,wav);}
static void u16(uint16_t x){fwrite(&x,2,1,wav);}
static float tone(uint32_t i,float hz){return 0.16f*sinf(6.2831853f*hz*(float)i/22050.0f);}
static void step(float input){float y=input+tl_sample(&t,input);int16_t pcm=(int16_t)(fmaxf(-0.98f,fminf(0.98f,y))*32767);fwrite(&pcm,2,1,wav);frames++;}
int main(int argc,char **argv){
    uint32_t i;float previous=0,maxjump=0;
    wav=fopen(argc>1?argv[1]:"tape-lab.wav","wb");assert(wav);
    fwrite("RIFF",1,4,wav);u32(0);fwrite("WAVEfmt ",1,8,wav);u32(16);u16(1);u16(1);u32(22050);u32(44100);u16(2);u16(16);fwrite("data",1,4,wav);u32(0);
    a[0]=b[0]=12345;a[N+1]=b[N+1]=-12345;tl_init(&t,a+1,b+1,N);
    /* First recording stops exactly at capacity; no garbage read or wrap overwrite. */
    tl_command(&t,TL_REC);for(i=0;i<N;i++)step(tone(i,220));
    assert(t.state==TL_PLAY && t.length==N && t.pos==0);
    memcpy(original,t.audio,sizeof original);
    for(i=0;i<N;i++)step(0);
    tl_command(&t,TL_REC);for(i=0;i<N;i++)step(tone(i,330));
    assert(t.state==TL_PLAY && t.undo_valid && t.pass==N);
    assert(!memcmp(t.undo,original,sizeof original));memcpy(mixed,t.audio,sizeof mixed);
    for(i=0;i<N*2;i++)step(0);
    tl_command(&t,TL_UNDO);assert(!memcmp(t.audio,original,sizeof original));for(i=0;i<N;i++)step(0);
    tl_command(&t,TL_UNDO);assert(!memcmp(t.audio,mixed,sizeof mixed));for(i=0;i<N;i++)step(0);
    /* Silence overdub is bit-stable (no recursive recording of monitored playback). */
    tl_command(&t,TL_REC);for(i=0;i<N;i++)step(0);
    for(i=0;i<N;i++)assert(abs(t.audio[i]-mixed[i])<=1);
    tl_command(&t,TL_TOGGLE);for(i=0;i<22050;i++)step(0);
    assert(t.state==TL_STOP);
    /* Stop during a dub is deferred to a complete pass, preserving a complete undo. */
    tl_command(&t,TL_TOGGLE);tl_command(&t,TL_REC);
    for(i=0;i<N+1 && t.state!=TL_OVERDUB;i++)tl_sample(&t,0.1f);
    assert(t.state==TL_OVERDUB);
    tl_command(&t,TL_TOGGLE);for(i=0;i<N;i++)tl_sample(&t,0);assert(t.state==TL_STOP);
    tl_command(&t,TL_CLEAR);for(i=0;i<22050;i++)step(0);assert(t.state==TL_EMPTY);
    /* Arbitrary non-zero endpoints are tapered at the seam. */
    tl_command(&t,TL_REC);for(i=0;i<1000;i++)tl_sample(&t,0.4f);tl_command(&t,TL_REC);
    for(i=0;i<5000;i++){float x=tl_sample(&t,0);float d=fabsf(x-previous);if(d>maxjump)maxjump=d;previous=x;}
    assert(maxjump<0.01f);
    tl_command(&t,TL_CLEAR);for(i=0;i<22050;i++)tl_sample(&t,0);
    tl_command(&t,TL_REC);for(i=0;i<N;i++)tl_sample(&t,2);assert(t.clipped==N);
    assert(a[0]==12345 && b[0]==12345 && a[N+1]==-12345 && b[N+1]==-12345);
    fseek(wav,4,SEEK_SET);u32(36+frames*2);fseek(wav,40,SEEK_SET);u32(frames*2);fclose(wav);
    printf("PASS: record/capacity, overdub, exact undo/redo, no feedback, stop, seam %.6f, clipping, buffer guards\n",maxjump);
    return 0;
}
