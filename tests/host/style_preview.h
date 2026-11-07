/* The actual firmware presets and mixer, not an offline substitute synth. */
static const int STYLE_PATCH[3][6][3] = {
    {{0,0,0},{3,0,0},{2,0,2},{4,0,0},{5,0,2},{7,0,0}},
    {{1,2,0},{9,2,7},{2,2,0},{4,2,0},{5,2,0},{11,2,0}},
    {{9,1,1},{9,1,4},{9,1,2},{4,1,0},{5,1,0},{11,1,1}}
};
static const char *const STYLE_FOLDER[] = {"ambient", "synthwave", "8bit", "acoustic"};
static int preview_expanded_palette;
static const int PALETTE_PATCH[4][6][3]={
    {{0,3,8},{7,3,2},{10,2,9},{4,3,10},{5,0,2},{1,3,7}},
    {{0,2,4},{10,2,7},{2,1,0},{4,3,11},{5,2,0},{11,2,11}},
    {{2,1,0},{8,1,0},{2,1,2},{4,1,0},{5,1,0},{8,1,2}},
    {{0,3,6},{3,3,6},{1,3,4},{4,3,8},{5,0,2},{7,3,1}}
};
static void style_setup(int s)
{
    static const int volumes[3][6] = {{76,66,61,77,54,67},{70,65,60,79,74,74},{62,62,59,78,72,70}};
    int t;
    fresh();
    dk_lock();
    dk_song.dust = dk_song.tape = dk_song.sleep = 0;
    dk_song.space = s == 0 ? 1 : 0;
    dk_song.tempo = s == 0 ? 72 : s == 1 ? 108 : s==3 ? 96 : 132;
    for (t=0;t<6;t++) {
        const int *patch=preview_expanded_palette?PALETTE_PATCH[s][t]:STYLE_PATCH[s][t];
        int e=patch[0];
        dk_track_setup(t,e,preset_id(patch[1],patch[2]),SQ_LIVE,TAPE.eng[e]->role);
        dk_song.trk[t].vol = volumes[s==3?0:s][t];
        dk_song.trk[t].pan = t == 1 ? -12 : t == 5 ? 12 : 0;
    }
    dk_unlock();
    run(0.1);
    reset_out();
}
static void style_previews(const char *dir)
{
    static const int roots[] = {45,41,48,43}; /* Am F C G */
    static const int arp[] = {0,7,12,15,7,12,19,12};
    static const int melody[] = {12,19,22,19,15,12,10,7};
    int s,t,bar,step,j;
    char folder[512],fn[600];
    MKDIR(dir);
    for(s=0;s<(preview_expanded_palette?4:3);s++) {
        sprintf(folder,"%s/%s",dir,STYLE_FOLDER[s]); MKDIR(folder);
        for(t=0;t<6;t++) {
            style_setup(s);
            if(t==4) play_kit(t);
            else if(t==3) play_bass(t);
            else if(t==2) play_pad(t);
            else play_melodic(t,48);
            tail(25);
            sprintf(fn,"%s/solo_%d.wav",folder,t+1); wav(fn); report(fn);
        }
        style_setup(s);
        for(bar=0;bar<8;bar++) {
            int root=roots[bar%4],third=bar%4==0 ? 3 : 4;
            int ch[3]={root+12,root+12+third,root+19};
            double tick=60.0/(s==0?72.0:s==1?108.0:s==3?96.0:132.0)/4.0;
            for(step=0;step<16;step++) {
                int bn=-1,an=-1,ln=-1,pn=-1;
                if(step==0) for(j=0;j<3;j++) note(2,ch[j],s==0?66:77);
                if(step==0 || (s!=0 && step==8)) {
                    pn=root+24; note(0,pn,82);
                }
                if(step % (s==0?8:2)==0) {bn=root-12;note(3,bn,94);}
                if(step % (s==0?4:2)==0) {
                    int interval=arp[(step/2+bar)%8];
                    if(interval==15 && third==4) interval=16;
                    an=root+12+interval; note(1,an,72+(step%4==0?8:0));
                }
                if(bar>=2 && step%4==0) {
                    int interval=melody[(bar*2+step/4)%8];
                    if(interval==15 && third==4) interval=16;
                    ln=root+12+interval; note(5,ln,86);
                }
                if(s==0) {if(step==0)note(4,36,55);if(step==10)note(4,37,42);}
                else {
                    if(step%4==0)note(4,36,100);
                    if(step==4 || step==12)note(4,38,96);
                    if(step%2==0)note(4,42,step%4?73:54);
                }
                run(tick*(s==0?0.92:0.68));
                if(bn>=0)note(3,bn,0);
                if(an>=0)note(1,an,0);
                if(ln>=0)note(5,ln,0);
                if(pn>=0)note(0,pn,0);
                if(step==15)for(j=0;j<3;j++)note(2,ch[j],0);
                run(tick*(s==0?0.08:0.32));
            }
        }
        tail(30);
        sprintf(fn,"%s/demo.wav",folder);wav(fn);report(fn);
    }
}
