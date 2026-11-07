/* SPDX-License-Identifier: MPL-2.0 */
#include "loop.h"
void tl_init(tape_loop *t,int16_t *audio,int16_t *undo,uint32_t capacity)
{
    *t=(tape_loop){0}; t->audio=audio;t->undo=undo;t->capacity=capacity;
}
static void tl_finish(tape_loop *t)
{
    if(t->length<128){t->state=TL_EMPTY;t->length=0;return;}
    t->state=TL_PLAY;t->pos=0;t->gain=0;
}
void tl_command(tape_loop *t,int cmd)
{
    if(cmd==TL_REC){
        if(t->state==TL_EMPTY){t->length=t->pos=0;t->state=TL_RECORD;t->undo_valid=0;}
        else if(t->state==TL_RECORD)tl_finish(t);
        else if(t->state==TL_PLAY || t->state==TL_STOP){
            t->state=TL_PLAY;t->armed=!t->armed;
        }
    }else if(cmd==TL_TOGGLE){
        if(t->state==TL_RECORD)tl_finish(t);
        else if(t->state==TL_OVERDUB)t->deferred=TL_TOGGLE; /* complete atomic pass */
        else if(t->state==TL_PLAY){t->state=TL_STOP;t->armed=0;}
        else if(t->state==TL_STOP)t->state=TL_PLAY;
    }else if(cmd==TL_UNDO && t->undo_valid && t->state!=TL_OVERDUB){
        int16_t *p=t->audio;t->audio=t->undo;t->undo=p;
        t->correction=t->last; t->gain=0; /* smooth old output into restored take */
        t->armed=0;
    }else if(cmd==TL_CLEAR && t->state!=TL_RECORD && t->state!=TL_OVERDUB){
        t->state=TL_EMPTY;t->length=t->pos=0;t->undo_valid=t->armed=0;
        t->correction=t->last;t->gain=0;
    }
}
static int16_t tl_pcm(tape_loop *t,float x)
{
    if(x>0.98f){x=0.98f;t->clipped++;}
    if(x< -0.98f){x= -0.98f;t->clipped++;}
    return (int16_t)(x*32767.0f);
}
float tl_sample(tape_loop *t,float input)
{
    float output=0,edge=1;
    if(t->state==TL_RECORD){
        if(t->length<t->capacity)t->audio[t->length++]=tl_pcm(t,input);
        if(t->length==t->capacity)tl_finish(t);
    }else if(t->length && (t->state==TL_PLAY || t->state==TL_OVERDUB || t->state==TL_STOP)){
        if(t->pos==0 && t->armed && t->state==TL_PLAY){
            t->state=TL_OVERDUB;t->armed=0;t->undo_valid=0;t->pass=0;
        }
        if(t->pos<64)edge=(float)t->pos/64.0f;
        if(t->length-1-t->pos<64)edge=(float)(t->length-1-t->pos)/64.0f;
        /* Input only is recorded: the monitored loop never feeds back into itself. */
        output=t->audio[t->pos]*(1.0f/32767.0f);
        if(t->state==TL_OVERDUB){
            t->undo[t->pos]=t->audio[t->pos];
            t->audio[t->pos]=tl_pcm(t,output+input);
            t->pass++;
        }
        t->gain+=((t->state==TL_STOP?0.0f:1.0f)-t->gain)*0.01f;
        output*=edge*t->gain;
        if(t->state!=TL_STOP || t->gain>0.00001f){
            if(++t->pos>=t->length){
                t->pos=0;t->wraps++;
                if(t->state==TL_OVERDUB){
                    t->undo_valid=1;t->state=TL_PLAY;
                    if(t->deferred==TL_TOGGLE)t->state=TL_STOP;
                    t->deferred=0;
                }
            }
        }
    }
    output+=t->correction;t->correction*=0.99f;
    if(t->correction<0.000001f && t->correction> -0.000001f)t->correction=0;
    t->last=output;
    return output;
}
