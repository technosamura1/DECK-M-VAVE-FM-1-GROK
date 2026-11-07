/* SPDX-License-Identifier: MPL-2.0 */
#include "loop.h"
#include "fastmath.h"
#define TL_CAP 65536u
static int16_t tl_a[TL_CAP] PLAT_POOL,tl_b[TL_CAP] PLAT_POOL;
static tape_loop tape;
static volatile uint32_t tl_keys,tl_command_pending;
static volatile int tl_muted;
static float tl_phase[27],tl_env[27],tl_inc[27],tl_gain,tl_prev,tl_next,tl_lp1,tl_lp2;
static uint32_t tl_half,tl_buttons,tl_draw;
static int tl_octave;
void app_init(void){
    int k;tl_init(&tape,tl_a,tl_b,TL_CAP);
    for(k=0;k<27;k++)tl_inc[k]=440.0f*fm_exp2f((float)(k+53-69)/12.0f)/22050.0f;
}
void app_render(int32_t *lr,uint32_t n,uint32_t master){
    uint32_t i,k,keys=tl_keys,cmd=tl_command_pending;
    float octave=fm_exp2f((float)tl_octave),gain=(float)master/4096.0f;
    if(tl_muted){for(i=0;i<n*2;i++)lr[i]=0;return;}
    if(cmd){tl_command_pending=0;tl_command(&tape,(int)cmd);}
    for(i=0;i<n;i++){
        float out;
        if(!tl_half){
            float live=0;
            for(k=0;k<27;k++){
                float target=(keys>>k&1u)?0.07f:0.0f;
                tl_env[k]+=(target-tl_env[k])*0.008f;
                if(tl_env[k]<0.000001f){tl_env[k]=0;continue;}
                tl_phase[k]+=tl_inc[k]*octave;
                if(tl_phase[k]>=1)tl_phase[k]-=1;
                live+=fm_sinf(tl_phase[k]*6.2831853f)*tl_env[k];
            }
            tl_prev=tl_next;
            tl_next=live+tl_sample(&tape,live);
        }
        out=tl_half?tl_next:(tl_prev+tl_next)*0.5f;
        tl_half^=1;
        /* Reconstruction filter; synth and recorder run at 22.05 kHz. */
        tl_lp1+=(out-tl_lp1)*0.6f;tl_lp2+=(tl_lp1-tl_lp2)*0.6f;
        tl_gain+=(gain-tl_gain)*0.004f;
        out=tl_lp2*tl_gain;
        out=fm_clampf(out,-0.98f,0.98f);
        lr[2*i]=lr[2*i+1]=(int32_t)(out*8388607.0f);
    }
}
void app_panic(void){tl_muted=1;tl_keys=0;}
#ifdef PLAT_HOST
int app_expect(const char *what,int *got){
    if(!strcmp(what,"state"))*got=tape.state;
    else if(!strcmp(what,"length"))*got=(int)tape.length;
    else if(!strcmp(what,"undo"))*got=tape.undo_valid;
    else if(!strcmp(what,"wraps"))*got=(int)tape.wraps;
    else return 0;
    return 1;
}
#endif
void app_say(const char *a,const char *b){(void)a;(void)b;}
void app_input_only(void){
    uint32_t buttons=plat_buttons(),down=buttons&~tl_buttons;
    tl_buttons=buttons;tl_keys=plat_keys();
    if(!tl_command_pending){
        if(down&(1u<<B_REC))tl_command_pending=TL_REC;
        else if(down&(1u<<B_PLAY))tl_command_pending=TL_TOGGLE;
        else if(down&(1u<<B_EDIT))tl_command_pending=TL_UNDO;
        else if((down&(1u<<B_SAVE)) && (buttons&(1u<<B_HOME)))tl_command_pending=TL_CLEAR;
    }
    if(down&(1u<<B_OCTUP))tl_octave=clamp(tl_octave+1,-2,2);
    if(down&(1u<<B_OCTDN))tl_octave=clamp(tl_octave-1,-2,2);
}
static void tl_number(uint32_t x,char *s){char b[12];int n=0;do{b[n++]=(char)('0'+x%10);x/=10;}while(x);while(n)*s++=b[--n];*s=0;}
void app_ui_init(void){lcd_fill(0,0,240,240,RGB(20,24,25));}
void app_frame(void){
    static const char *const names[]={"EMPTY","RECORD","PLAY","OVERDUB","STOP"};
    char num[12];uint32_t now=plat_ms();
    app_input_only();if(now-tl_draw<80)return;tl_draw=now;
    cv_begin(240,60,RGB(20,24,25));
    cv_text(12,8,&FONT_B,"TAPE LAB",RGB(164,199,161));
    cv_text(12,34,&FONT_XS,"RAM AUDIO / 22K MONO",RGB(220,220,210));cv_blit(0,0);
    cv_begin(240,60,RGB(20,24,25));
    cv_text(12,5,&FONT_B,tape.armed?"DUB NEXT LOOP":names[tape.state],RGB(235,200,120));
    cv_rect(12,32,216,8,RGB(60,65,65));
    if(tape.length)cv_rect(12,32,(int)(216u*(tape.state==TL_RECORD?tape.length:tape.pos)/(tape.state==TL_RECORD?TL_CAP:tape.length)),8,RGB(164,199,161));
    cv_blit(0,60);
    cv_begin(240,60,RGB(20,24,25));
    cv_text(12,2,&FONT_XS,"CPU",RGB(220,220,210));tl_number(plat_cpu_pct(),num);cv_text(44,2,&FONT_XS,num,RGB(164,199,161));
    cv_text(90,2,&FONT_XS,"XRUN",RGB(220,220,210));tl_number(plat_xruns(),num);cv_text(130,2,&FONT_XS,num,RGB(164,199,161));
    cv_text(12,22,&FONT_XS,"MS",RGB(220,220,210));tl_number(tape.length*1000u/22050u,num);cv_text(44,22,&FONT_XS,num,RGB(164,199,161));
    cv_text(90,22,&FONT_XS,"CLIP",RGB(220,220,210));tl_number(tape.clipped,num);cv_text(130,22,&FONT_XS,num,RGB(164,199,161));
    cv_text(12,42,&FONT_XS,tape.undo_valid?"EDIT: UNDO / REDO":"MAX 2972 MS",RGB(220,220,210));cv_blit(0,120);
    cv_begin(240,60,RGB(20,24,25));
    cv_text(12,2,&FONT_XS,"REC: RECORD / DUB",RGB(220,220,210));
    cv_text(12,20,&FONT_XS,"PLAY: STOP / PLAY",RGB(220,220,210));
    cv_text(12,38,&FONT_XS,"HOME+SAVE: CLEAR",RGB(220,220,210));cv_blit(0,180);
    plat_leds((tape.state==TL_RECORD||tape.state==TL_OVERDUB?1u<<B_REC:0)|(tape.state==TL_PLAY?1u<<B_PLAY:0),tl_keys);
}
