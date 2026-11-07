/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DREAMWALL / DECK contributors */
/* The tape's demo, FIRST FOG: a bed to play a guitar over. E major (the guitar's own key), 80, the plate SHIMMER.
 * INTRO (the wall's drone on E, the keys alone) - VERSE (I iii IV iv, the drums drifting, the bass) - CHORUS (I V vi IV,
 * the fuzz up, half time, the crash) - BRIDGE (Kid A: the keys broken through LOFI, a tambourine, the wall far away) -
 * OUTRO (the drone, fading). The road: INTRO VERSE CHORUS VERSE CHORUS BRIDGE CHORUS OUTRO. The wall's fuzz and its
 * reverb move part by part (a knob's move at the part's start). Written into the clips as notes. Part of tape_dsp.c. */

enum { DT_PIANO, DT_KEYS, DT_WALL, DT_BASS, DT_LEAD, DT_DRUMS };
enum { P_INTRO, P_VERSE, P_CHORUS, P_BRIDGE, P_OUTRO };
#define E8 (DK_PPQ / 2)

static void dn_tick(int part, int trk, int pos, int note, int dur, int vel)
{
    dk_ev_t e;
    e.pos = (uint16_t)pos;
    e.dur = (uint16_t)dur;
    e.note = (uint8_t)note;
    e.vel = (uint8_t)vel;
    e.fl = 0;
    e.layer = 1;
    dk_song_insert(part * DK_NTRACK + trk, &e);
}
static void dn(int part, int trk, int bar, int e8, int note, int len8, int vel)
{
    dn_tick(part, trk, bar * DK_TPB + e8 * E8, note, len8 * E8 - 4, vel);
}
static void motion(int part, int trk, int target, int value)
{
    dk_ev_t e;
    e.pos = 0;
    e.dur = 0;
    e.note = (uint8_t)target;
    e.vel = (uint8_t)value;
    e.fl = EV_MOTION;
    e.layer = 1;
    dk_song_insert(part * DK_NTRACK + trk, &e);
}
/* drums from strings, a sixteenth a character ('1'..'9' how hard): kick, snare, then a third lane of any drum */
static void drums_put(int part, int bar, const char *kick, const char *snare, const char *other, int onote, const char *other2,
                      int onote2)
{
    const char *lane[4] = {kick, snare, other, other2};
    const int note[4] = {36, 38, onote, onote2};
    int l, s;
    for (l = 0; l < 4; l++)
        for (s = 0; s < 16 && lane[l] && lane[l][s]; s++)
            if (lane[l][s] >= '1' && lane[l][s] <= '9')
                dn_tick(part, DT_DRUMS, bar * DK_TPB + s * DK_T16, note[l], DK_T16, (lane[l][s] - '0') * 12 + 10);
}
/* the wall: a chord held two bars, its notes a breath apart (a strum's slowness) */
static void wall2(int part, int bar, const int8_t *ch, int vel)
{
    int i;
    for (i = 0; ch[i]; i++)
        dn_tick(part, DT_WALL, bar * DK_TPB + i * 9, ch[i], 2 * DK_TPB - 24 - i * 9, vel - i * 2);
}

/* the wall's chords (E major, voiced for the middle) */
static const int8_t W_E[] = {52, 59, 64, 68, 71, 0}, W_GM[] = {56, 59, 63, 68, 71, 0}, W_A[] = {57, 59, 64, 69, 71, 0},
                    W_AM[] = {57, 60, 64, 69, 72, 0}, W_B[] = {59, 63, 66, 71, 0}, W_CSM[] = {56, 61, 64, 68, 73, 0},
                    W_AMAJ7[] = {57, 61, 64, 68, 0}, W_GSM7[] = {56, 59, 63, 66, 0}, W_FSM7[] = {54, 57, 61, 64, 0},
                    W_BSUS[] = {59, 64, 66, 71, 0}, W_DRONE[] = {40, 47, 52, 0};
/* the keys' figures: a chord's tones, an eighth each, high and sparse */
static void keys_fig(int part, int bar, const int8_t *t, int vel)
{
    static const int8_t STEP[8] = {0, -1, 1, -1, 2, -1, 1, 3};
    int i;
    for (i = 0; i < 8; i++)
        if (STEP[i] >= 0)
            dn(part, DT_KEYS, bar, i, t[STEP[i]] + 12, 2, vel - (i % 4 ? 8 : 0));
}

static void tape_demo(void)
{
    static const char *const NAME = "FIRST FOG";
    static const uint8_t BARS[5] = {8, 8, 8, 8, 8}, LABEL[5] = {0, 1, 3, 5, 7};
    static const uint8_t ROAD[8] = {P_INTRO, P_VERSE, P_CHORUS, P_VERSE, P_CHORUS, P_BRIDGE, P_CHORUS, P_OUTRO};
    static const int8_t *const VS[4] = {W_E, W_GM, W_A, W_AM}, *const CH[4] = {W_E, W_B, W_CSM, W_A},
                        *const BR[4] = {W_AMAJ7, W_GSM7, W_FSM7, W_BSUS};
    static const int8_t R_VS[4] = {40, 44, 45, 45}, R_CH[4] = {40, 47, 49, 45}, R_BR[4] = {45, 44, 42, 47};
    static const int8_t K_E[4] = {64, 68, 71, 75}, K_GM[4] = {63, 68, 71, 75}, K_A[4] = {64, 69, 71, 73},
                        K_AM[4] = {64, 69, 72, 71}, K_AMAJ7[4] = {64, 68, 69, 73}, K_GSM7[4] = {63, 66, 68, 71},
                        K_FSM7[4] = {61, 64, 66, 69}, K_BSUS[4] = {64, 66, 71, 73};
    static const int8_t *const KV[4] = {K_E, K_GM, K_A, K_AM}, *const KB[4] = {K_AMAJ7, K_GSM7, K_FSM7, K_BSUS};
    int i, k;
    dk_song_clear();
    for (i = 0; i < 11 && NAME[i]; i++)
        dk_song.name[i] = NAME[i];
    dk_song.name[i] = 0;
    dk_song.tempo = 80;
    dk_song.root = 4;                                  /* E */
    dk_song.mode = 0;
    dk_song.style = ST_AMBIENT;
    dk_song.dust = 0;
    dk_song.sleep = 0;
    dk_song.sidechain = 0;
    dk_song.space = 3;                                 /* SHIMMER */
    dk_song.play_mode = 1;
    dk_track_setup(DT_DRUMS, 5, 1, SQ_STEP, R_DRUM);   /* DUSTY: soft, far */
    dk_track_setup(DT_BASS, 4, 0, SQ_STEP, R_BASS);
    dk_track_setup(DT_PIANO, 0, 0, SQ_STEP, R_KEYS);
    dk_track_setup(DT_WALL, 7, 0, SQ_STEP, R_PAD);     /* SLOW CRUSH */
    dk_track_setup(DT_KEYS, 1, 1, SQ_STEP, R_ARP);     /* the KEYS track: an electric piano */
    dk_track_setup(DT_LEAD, 11, 0, SQ_STEP, R_LEAD);   /* (the guitar is the lead: this one waits) */
    dk_song.trk[DT_DRUMS].vol = 60;
    dk_song.trk[DT_BASS].vol = 66;
    dk_song.trk[DT_WALL].vol = 74;
    dk_song.trk[DT_KEYS].vol = 56;
    dk_song.trk[DT_PIANO].vol = 60;
    dk_song.trk[DT_DRUMS].rev = 40;
    dk_song.harm_track = DT_WALL;
    for (k = 0; k < 5; k++) {
        dk_song.part[k].bars = BARS[k];
        dk_song.part[k].label = LABEL[k];
    }
    dk_song.nroad = 8;
    for (k = 0; k < 8; k++) {
        dk_song.road[k].part = ROAD[k];
        dk_song.road[k].reps = 1;
        dk_song.road[k].tricks = 0;
        dk_song.road[k].drop = 0;
    }
    /* INTRO: the drone on E swelling in, the keys alone over it */
    motion(P_INTRO, DT_WALL, T_P1, 20);
    motion(P_INTRO, DT_WALL, T_P5, 90);
    wall2(P_INTRO, 0, W_DRONE, 70);
    wall2(P_INTRO, 2, W_DRONE, 70);
    wall2(P_INTRO, 4, W_DRONE, 72);
    wall2(P_INTRO, 6, W_DRONE, 74);
    for (i = 0; i < 8; i++)
        keys_fig(P_INTRO, i, i < 4 ? K_E : i < 6 ? K_A : K_AM, 60);
    /* VERSE: I iii IV iv, drifting drums, the bass on the ones */
    motion(P_VERSE, DT_WALL, T_P1, 40);
    motion(P_VERSE, DT_WALL, T_P5, 40);
    for (k = 0; k < 4; k++) {
        wall2(P_VERSE, 2 * k, VS[k], 78);
        keys_fig(P_VERSE, 2 * k, KV[k], 54);
        dn(P_VERSE, DT_BASS, 2 * k, 0, R_VS[k], 7, 88);
        dn(P_VERSE, DT_BASS, 2 * k + 1, 0, R_VS[k], 3, 76);
        dn(P_VERSE, DT_BASS, 2 * k + 1, 4, R_VS[k], 4, 70);
    }
    for (i = 0; i < 8; i++)
        drums_put(P_VERSE, i, "8.....5.7.......", "....6.......6...", "4.3.4.3.4.3.4.3.", 51,
                  i == 7 ? "............4.56" : 0, 38);
    /* CHORUS: I V vi IV, the fuzz up, half time, the crash on every chord */
    motion(P_CHORUS, DT_WALL, T_P1, 85);
    motion(P_CHORUS, DT_WALL, T_P5, 25);
    for (k = 0; k < 4; k++) {
        wall2(P_CHORUS, 2 * k, CH[k], 92);
        dn(P_CHORUS, DT_BASS, 2 * k, 0, R_CH[k], 8, 100);
        dn(P_CHORUS, DT_BASS, 2 * k + 1, 0, R_CH[k], 6, 90);
        dn(P_CHORUS, DT_BASS, 2 * k + 1, 6, R_CH[k] + 7, 2, 80);
    }
    for (i = 0; i < 8; i++)
        drums_put(P_CHORUS, i, "8.6.....8.6.....", "........9.....4.", i % 2 ? "6...6...6...6..." : "9...6...6...6...", 49,
                  i == 7 ? "..........6.7.8." : 0, 45);
    /* BRIDGE: Kid A: the wall far away, the keys broken, a tambourine and a soft kick */
    motion(P_BRIDGE, DT_WALL, T_P1, 10);
    motion(P_BRIDGE, DT_WALL, T_P0, 30);
    motion(P_BRIDGE, DT_KEYS, T_FXAMT, 60);
    for (k = 0; k < 4; k++) {
        wall2(P_BRIDGE, 2 * k, BR[k], 62);
        keys_fig(P_BRIDGE, 2 * k, KB[k], 60);
        keys_fig(P_BRIDGE, 2 * k + 1, KB[k], 52);
        dn(P_BRIDGE, DT_BASS, 2 * k, 0, R_BR[k], 14, 74);
    }
    for (i = 0; i < 8; i++)
        drums_put(P_BRIDGE, i, "8.........6.....", "....4.......4...", "5353535353535353", 70, "4...4...4...4...", 51);
    /* OUTRO: the drone, the keys' last E, fading */
    motion(P_OUTRO, DT_WALL, T_P1, 30);
    motion(P_OUTRO, DT_WALL, T_P5, 90);
    wall2(P_OUTRO, 0, W_E, 70);
    wall2(P_OUTRO, 2, W_A, 62);
    wall2(P_OUTRO, 4, W_DRONE, 56);
    wall2(P_OUTRO, 6, W_DRONE, 46);
    keys_fig(P_OUTRO, 0, K_E, 50);
    keys_fig(P_OUTRO, 2, K_A, 44);
    dn(P_OUTRO, DT_BASS, 0, 0, 40, 15, 70);
    dn(P_OUTRO, DT_BASS, 4, 0, 40, 15, 54);
    {                                        /* every part starts with these instruments; a later edit stays in its part */
        extern void part_inst_capture(int p);
        part_inst_capture(0);
    }
}
