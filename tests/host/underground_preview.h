/* SPDX-License-Identifier: MPL-2.0 */
/* Original short arrangements to audition the chip collection through DECK's mixer. */
static void underground_demo(int scene,const char *path)
{
    static const int engines[]={9,3,9,4,5,11};
    static const int slots[]={0,0,2,0,0,0};
    static const int volumes[]={64,62,57,78,67,72};
    static const int roots[2][4]={{48,44,51,46},{45,41,48,43}};
    static const int melody[2][16]={
        {72,74,75,79,77,75,74,72,67,70,72,74,75,74,70,67},
        {76,76,79,81,79,76,74,72,74,76,79,83,81,79,76,74}};
    int t,bar,step,j;
    double tick=60.0/(scene?148.0:92.0)/4.0;
    fresh();dk_lock();dk_song.dust=dk_song.tape=dk_song.sleep=0;
    dk_song.tempo=scene?148:92;dk_song.space=0;
    for(t=0;t<6;t++){
        dk_track_setup(t,engines[t],preset_id(1,slots[t]),SQ_LIVE,TAPE.eng[engines[t]]->role);
        dk_song.trk[t].vol=volumes[t];dk_song.trk[t].pan=t==1?-14:t==0?10:0;
    }
    dk_unlock();run(0.1);reset_out();
    for(bar=0;bar<8;bar++){
        int root=roots[scene][bar%4],third=bar%4==0?3:4;
        int ch[3]={root,root+third,root+7};
        for(step=0;step<16;step++){
            int bass=-1,arpnote=-1,lead=-1,bell=-1;
            if(step==0)for(j=0;j<3;j++)note(2,ch[j]+12,68);
            if(step%(scene?2:4)==0){bass=root-12+(step==10?7:0);note(3,bass,94);}
            if(step%2==0){arpnote=ch[(step/2+bar)%3]+12;note(0,arpnote,step%4?67:78);}
            if(step%4==0){
                lead=melody[scene][(bar*4+step/4)%16];
                if(bar!=0 || step>=8)note(5,lead,91);else lead=-1;
            }
            if(!scene && step==10){bell=ch[(bar+1)%3]+24;note(1,bell,68);}
            if(step==0 || step==8 || (scene && step==6))note(4,36,96);
            if(step==4 || step==12)note(4,38,86);
            if(step%2==0)note(4,42,step%4?67:44);
            run(tick*0.72);
            if(bass>=0)note(3,bass,0);
            if(arpnote>=0)note(0,arpnote,0);
            if(lead>=0)note(5,lead,0);
            if(bell>=0)note(1,bell,0);
            if(step==15)for(j=0;j<3;j++)note(2,ch[j]+12,0);
            run(tick*0.28);
        }
    }
    tail(25);wav(path);report(path);
}
