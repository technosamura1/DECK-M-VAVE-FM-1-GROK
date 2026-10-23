/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors */
/* The tape's demo song, FIRST LIGHT: PLAINS at 76, C major with a lydian turn. INTRO (the piano alone, its phrases,
 * the birds) - PRE (the pad comes in) - VERSE (a quiet beat, the bass) - CHORUS (I-iii-IV-iv, the bells) - BRIDGE
 * (no drums, the snow, a music box) - BREAK (the campfire: wood, a marimba) - CHORUS - OUTRO (the piano alone).
 * Written into the clips as notes (the UI thread, the song locked). Part of tape_dsp.c. */

enum { DT_PIANO, DT_PAD, DT_BELLS, DT_BASS, DT_DRUMS, DT_NATURE };   /* (the KEYS track plays the bells) */
enum { P_INTRO, P_PRE, P_VERSE, P_CHORUS, P_BRIDGE, P_BREAK, P_OUTRO };
#define E8 (DK_PPQ / 2)

static void dn(int part, int trk, int bar, int e8, int note, int len8, int vel)
{
    dk_ev_t e;
    e.pos = (uint16_t)(bar * DK_TPB + e8 * E8);
    e.dur = (uint16_t)(len8 * E8 - 4);
    e.note = (uint8_t)note;
    e.vel = (uint8_t)vel;
    e.fl = 0;
    e.layer = 1;
    dk_song_insert(part * DK_NTRACK + trk, &e);
}
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
/* a chord rolled from the bottom (a few ticks a note) */
static void roll(int part, int trk, int bar, int e8, const int8_t *nt, int len8, int vel)
{
    int i;
    for (i = 0; i < 6 && nt[i]; i++)
        dn_tick(part, trk, bar * DK_TPB + e8 * E8 + i * 7, nt[i], len8 * E8 - 8 - i * 7, vel - i * 2);
}
typedef struct {
    int8_t bar, e8, note, len, vel;
} mel_t;
static void melody_put(int part, int trk, int bar0, const mel_t *m, int n, int dvel)
{
    int i;
    for (i = 0; i < n; i++)
        dn(part, trk, bar0 + m[i].bar, m[i].e8, m[i].note, m[i].len, m[i].vel + dvel);
}
static void drums_put(int part, int bar, const char *kick, const char *snare, const char *shaker, const char *hat,
                      const char *other, int onote, int scale)
{
    const char *lane[5] = {kick, snare, shaker, hat, other};
    static const uint8_t NOTE[4] = {36, 38, 70, 42};
    int l, s;
    for (l = 0; l < 5; l++)
        for (s = 0; s < 16 && lane[l] && lane[l][s]; s++)
            if (lane[l][s] >= '1' && lane[l][s] <= '9')
                dn_tick(part, DT_DRUMS, bar * DK_TPB + s * DK_T16, l < 4 ? NOTE[l] : onote, DK_T16,
                        (lane[l][s] - '0') * 13 * scale / 10 + 4);
}

/* the chords (left hand, low first) */
static const int8_t C_MAJ7[] = {48, 55, 59, 64, 0}, E_M7[] = {52, 59, 62, 67, 0}, F_MAJ7[] = {41, 48, 57, 64, 0},
                    F_M6[] = {41, 48, 56, 62, 0}, A_M7[] = {45, 52, 55, 60, 0}, G_6[] = {43, 50, 52, 59, 0},
                    D_MAJ7[] = {50, 57, 61, 66, 0}, F_C[] = {48, 53, 57, 64, 0}, A_M9[] = {45, 52, 59, 60, 0},
                    G_SUS[] = {43, 50, 55, 60, 0}, C_ADD9[] = {48, 55, 62, 64, 67, 0};
/* the pad's (voice-led) */
static const int8_t PD_C[] = {55, 59, 64, 0}, PD_EM[] = {55, 59, 62, 0}, PD_F[] = {57, 60, 64, 0},
                    PD_FM[] = {56, 60, 62, 65, 0}, PD_AM[] = {57, 60, 64, 0}, PD_FV[] = {57, 60, 65, 0},
                    PD_CV[] = {55, 60, 64, 0}, PD_G[] = {55, 59, 62, 0}, PD_D[] = {57, 61, 66, 0},
                    PD_A9[] = {59, 60, 64, 0}, PD_GS[] = {55, 60, 62, 0};

static const mel_t M_INTRO[] = {
    {0, 2, 67, 2, 62}, {0, 4, 76, 6, 68}, {1, 4, 74, 2, 60}, {1, 6, 72, 6, 62},
    {2, 2, 71, 2, 58}, {2, 4, 74, 4, 64}, {3, 0, 79, 8, 66},
    {4, 2, 72, 2, 58}, {4, 4, 76, 4, 64}, {5, 0, 77, 3, 62}, {5, 3, 76, 1, 56}, {5, 4, 72, 4, 60},
    {6, 4, 68, 4, 60}, {7, 0, 72, 8, 62}};
static const mel_t M_PRE[] = {
    {0, 4, 76, 4, 64}, {1, 0, 79, 3, 66}, {1, 3, 76, 1, 58}, {1, 4, 74, 4, 60},
    {2, 4, 71, 2, 58}, {2, 6, 74, 2, 60}, {3, 0, 76, 8, 64},
    {4, 2, 77, 2, 60}, {4, 4, 81, 4, 68}, {5, 0, 79, 4, 62}, {5, 4, 77, 4, 58},
    {6, 0, 75, 4, 60}, {6, 4, 74, 4, 58}, {7, 0, 72, 8, 62}};
static const mel_t M_VERSE[] = {
    {0, 2, 72, 2, 60}, {0, 4, 76, 4, 64}, {1, 0, 79, 6, 64},
    {2, 2, 77, 2, 58}, {2, 4, 76, 2, 60}, {2, 6, 72, 6, 62},
    {4, 0, 67, 2, 58}, {4, 2, 72, 2, 60}, {4, 4, 74, 4, 64}, {5, 0, 76, 8, 64},
    {6, 2, 74, 2, 58}, {6, 4, 71, 4, 60}, {7, 0, 74, 6, 62}};
static const mel_t M_VERSE2[] = {
    {0, 2, 72, 2, 60}, {0, 4, 76, 4, 64}, {1, 0, 81, 6, 66},
    {2, 2, 79, 2, 58}, {2, 4, 77, 2, 60}, {2, 6, 76, 6, 62},
    {4, 0, 72, 2, 58}, {4, 2, 76, 2, 60}, {4, 4, 79, 4, 64}, {5, 0, 84, 6, 66},
    {6, 2, 83, 2, 60}, {6, 4, 79, 4, 60}, {7, 0, 74, 8, 62}};
static const mel_t M_CHORUS[] = {
    {0, 0, 76, 3, 68}, {0, 3, 79, 1, 62}, {0, 4, 83, 4, 70}, {1, 0, 79, 4, 64}, {1, 4, 76, 4, 62},
    {2, 0, 74, 3, 64}, {2, 3, 76, 1, 58}, {2, 4, 79, 4, 66}, {3, 0, 83, 6, 70}, {3, 6, 81, 2, 60},
    {4, 0, 81, 3, 66}, {4, 3, 79, 1, 58}, {4, 4, 77, 4, 64}, {5, 0, 76, 4, 62}, {5, 4, 72, 4, 60},
    {6, 0, 80, 4, 66}, {6, 4, 77, 4, 62}, {7, 0, 72, 8, 64}};
static const mel_t M_CHORUS2[] = {
    {0, 0, 76, 3, 70}, {0, 3, 79, 1, 62}, {0, 4, 83, 4, 72}, {1, 0, 84, 4, 68}, {1, 4, 83, 4, 64},
    {2, 0, 79, 3, 66}, {2, 3, 81, 1, 60}, {2, 4, 83, 4, 68}, {3, 0, 86, 6, 72}, {3, 6, 84, 2, 62},
    {4, 0, 84, 3, 68}, {4, 3, 81, 1, 60}, {4, 4, 77, 4, 64}, {5, 0, 79, 4, 64}, {5, 4, 76, 4, 62},
    {6, 0, 80, 4, 66}, {6, 4, 77, 4, 62}, {7, 0, 76, 8, 66}};
static const mel_t M_BRIDGE[] = {
    {0, 4, 78, 4, 58}, {1, 0, 81, 8, 62}, {2, 4, 79, 4, 58}, {3, 0, 76, 8, 60},
    {4, 2, 83, 2, 58}, {4, 4, 84, 8, 64}, {6, 4, 79, 4, 58}, {7, 0, 74, 8, 60}};
static const mel_t M_OUTRO[] = {
    {0, 4, 76, 4, 58}, {1, 0, 74, 4, 54}, {1, 4, 72, 8, 56},
    {2, 4, 72, 2, 52}, {2, 6, 69, 2, 50}, {3, 0, 72, 8, 54},
    {4, 4, 67, 4, 50}, {5, 0, 74, 8, 52}, {6, 0, 76, 16, 50}};

static void chords4(int part, int trk, int bar0, const int8_t *a, const int8_t *b, const int8_t *c, const int8_t *d,
                    int vel)
{
    roll(part, trk, bar0 + 0, 0, a, 16, vel);
    roll(part, trk, bar0 + 2, 0, b, 16, vel);
    roll(part, trk, bar0 + 4, 0, c, 16, vel);
    roll(part, trk, bar0 + 6, 0, d, 16, vel);
}
static void pad4(int part, int bar0, const int8_t *a, const int8_t *b, const int8_t *c, const int8_t *d, int vel)
{
    const int8_t *ch[4] = {a, b, c, d};
    int k, i;
    for (k = 0; k < 4; k++)
        for (i = 0; ch[k][i]; i++)
            dn_tick(part, DT_PAD, (bar0 + 2 * k) * DK_TPB + i * 3, ch[k][i], 2 * DK_TPB - 30, vel);
}
static void bass4(int part, int bar0, const int8_t *roots, int busy)
{
    int k;
    for (k = 0; k < 4; k++) {
        int r = roots[k], b = bar0 + 2 * k;
        dn(part, DT_BASS, b, 0, r, 6, 92);
        if (busy)
            dn(part, DT_BASS, b, 6, r + 7 > 47 ? r - 5 : r + 7, 2, 74);
        dn(part, DT_BASS, b + 1, 0, r, 3, 86);
        if (busy)
            dn(part, DT_BASS, b + 1, 4, r, 3, 78);
    }
}

static void tape_demo(void)
{
    static const char *const NAME = "FIRST LIGHT";
    static const uint8_t BARS[7] = {8, 8, 16, 16, 8, 8, 8}, LABEL[7] = {0, 2, 1, 3, 5, 6, 7};
    static const uint8_t ROAD[8] = {P_INTRO, P_PRE, P_VERSE, P_CHORUS, P_BRIDGE, P_BREAK, P_CHORUS, P_OUTRO};
    static const int8_t R_CH[4] = {36, 40, 41, 41}, R_VS[4] = {45, 41, 36, 43}, R_BR[4] = {38, 36, 33, 31};
    int i, k;
    dk_song_clear();
    for (i = 0; i < 11 && NAME[i]; i++)
        dk_song.name[i] = NAME[i];
    dk_song.name[i] = 0;
    dk_song.tempo = 76;
    dk_song.root = 0;
    dk_song.mode = 0;
    dk_song.style = ST_C418;
    dk_song.mood = 0;
    dk_song.biome = 0;
    dk_song.dust = 25;
    dk_song.sleep = 0;
    dk_song.sidechain = 15;
    dk_song.space = 1;
    dk_song.play_mode = 1;
    dk_track_setup(DT_DRUMS, 5, 4, SQ_STEP, R_DRUM);
    dk_track_setup(DT_BASS, 4, 1, SQ_STEP, R_BASS);
    dk_track_setup(DT_PIANO, 0, 1, SQ_LIVE, R_KEYS);
    dk_track_setup(DT_PAD, 2, 0, SQ_LIVE, R_PAD);
    dk_track_setup(DT_BELLS, 3, 0, SQ_LIVE, R_ARP);    /* the KEYS track: GLASS (bells) */
    dk_track_setup(DT_NATURE, 6, 6, SQ_LIVE, R_NATURE);
    dk_song.trk[DT_DRUMS].vol = 62;
    dk_song.trk[DT_BASS].vol = 70;
    dk_song.trk[DT_PIANO].vol = 82;
    dk_song.trk[DT_PAD].vol = 64;
    dk_song.trk[DT_BELLS].vol = 58;
    dk_song.trk[DT_NATURE].vol = 44;
    dk_song.harm_track = DT_PIANO;
    for (k = 0; k < 7; k++) {
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
    /* the world under it all: the birds, the snow, the fire */
    for (k = 0; k < 7; k++) {
        motion(k, DT_NATURE, T_P0, k == P_BRIDGE ? 100 : k == P_BREAK ? 22 : 67);
        dn_tick(k, DT_NATURE, 0, 60, BARS[k] * DK_TPB - 1, 96);
    }
    /* INTRO: the piano alone */
    chords4(P_INTRO, DT_PIANO, 0, C_MAJ7, E_M7, F_MAJ7, F_M6, 52);
    melody_put(P_INTRO, DT_PIANO, 0, M_INTRO, (int)(sizeof M_INTRO / sizeof M_INTRO[0]), 0);
    /* PRE: the pad comes in */
    chords4(P_PRE, DT_PIANO, 0, C_MAJ7, E_M7, F_MAJ7, F_M6, 50);
    melody_put(P_PRE, DT_PIANO, 0, M_PRE, (int)(sizeof M_PRE / sizeof M_PRE[0]), 0);
    pad4(P_PRE, 0, PD_C, PD_EM, PD_F, PD_FM, 62);
    /* VERSE: vi IV I V, a quiet beat, the bass */
    for (k = 0; k < 2; k++) {
        int b0 = 8 * k;
        static const int8_t *const VS[4] = {A_M7, F_MAJ7, C_MAJ7, G_6};
        for (i = 0; i < 4; i++) {
            roll(P_VERSE, DT_PIANO, b0 + 2 * i, 0, VS[i], 6, 48);
            roll(P_VERSE, DT_PIANO, b0 + 2 * i + 1, 3, VS[i], 5, 44);
        }
        melody_put(P_VERSE, DT_PIANO, b0, k ? M_VERSE2 : M_VERSE, 13, 0);
        pad4(P_VERSE, b0, PD_AM, PD_FV, PD_CV, PD_G, 50);
        bass4(P_VERSE, b0, R_VS, 1);
        for (i = 0; i < 8; i++)
            drums_put(P_VERSE, b0 + i, "7.....5...5.....", "....5.......5...", "2.3.2.3.2.3.2.3.", 0,
                      i == 7 ? "..........3.4.5." : 0, 37, i < 1 && !k ? 6 : 8);
    }
    /* CHORUS: I iii IV iv, everything, the bells */
    for (k = 0; k < 2; k++) {
        int b0 = 8 * k;
        chords4(P_CHORUS, DT_PIANO, b0, C_MAJ7, E_M7, F_MAJ7, F_M6, 50);
        melody_put(P_CHORUS, DT_PIANO, b0, k ? M_CHORUS2 : M_CHORUS, 18, 2);
        pad4(P_CHORUS, b0, PD_C, PD_EM, PD_F, PD_FM, 60);
        bass4(P_CHORUS, b0, R_CH, 1);
        for (i = 0; i < 8; i++)
            drums_put(P_CHORUS, b0 + i, "8.....6.7.......", "....7.......7..2", "2222222222222222", "3.2.3.2.3.2.3.2.",
                      i == 7 && k ? "............3456" : "..............3.", i == 7 && k ? 38 : 46, 8);
        dn(P_CHORUS, DT_BELLS, b0 + 0, 4, 88, 6, 70);
        dn(P_CHORUS, DT_BELLS, b0 + 2, 4, 91, 6, 64);
        dn(P_CHORUS, DT_BELLS, b0 + 4, 4, 84, 6, 68);
        dn(P_CHORUS, DT_BELLS, b0 + 6, 2, 80, 4, 60);
        dn(P_CHORUS, DT_BELLS, b0 + 6, 6, 84, 8, 64);
    }
    motion(P_CHORUS, DT_BELLS, T_P0, 0);
    /* BRIDGE: no drums, the snow, a music box */
    chords4(P_BRIDGE, DT_PIANO, 0, D_MAJ7, F_C, A_M9, G_SUS, 46);
    melody_put(P_BRIDGE, DT_PIANO, 0, M_BRIDGE, 8, 0);
    pad4(P_BRIDGE, 0, PD_D, PD_F, PD_A9, PD_GS, 58);
    for (k = 0; k < 4; k++)
        dn(P_BRIDGE, DT_BASS, 2 * k, 0, R_BR[k], 15, 74);
    motion(P_BRIDGE, DT_BELLS, T_P0, 75);
    {
        static const mel_t MB[] = {{0, 6, 90, 4, 60}, {1, 4, 85, 4, 54}, {2, 6, 88, 4, 58}, {3, 4, 84, 4, 54},
                                   {4, 6, 91, 4, 60}, {5, 4, 88, 4, 54}, {6, 6, 86, 4, 58}, {7, 2, 79, 6, 54}};
        melody_put(P_BRIDGE, DT_BELLS, 0, MB, 8, 0);
    }
    /* BREAK: the campfire: wood and a shaker, a marimba's broken chords, a plucked bass */
    motion(P_BREAK, DT_BELLS, T_P0, 50);
    {
        static const int8_t ARP[4][8] = {{60, 64, 67, 72, 67, 64, 67, 72}, {60, 65, 69, 72, 69, 65, 69, 74},
                                         {57, 60, 64, 69, 64, 60, 64, 67}, {55, 59, 62, 67, 62, 59, 62, 67}};
        static const int8_t RB[4] = {36, 41, 45, 43};
        for (k = 0; k < 4; k++)
            for (i = 0; i < 16; i++)
                dn(P_BREAK, DT_BELLS, 2 * k + i / 8, i % 8, ARP[k][i % 8] + (i >= 8 && i % 8 == 3 ? 2 : 0), 1,
                   i % 4 == 0 ? 90 : 72);
        for (k = 0; k < 4; k++) {
            dn(P_BREAK, DT_BASS, 2 * k, 0, RB[k], 2, 50);
            dn(P_BREAK, DT_BASS, 2 * k, 4, RB[k] + 7 > 47 ? RB[k] - 5 : RB[k] + 7, 2, 44);
            dn(P_BREAK, DT_BASS, 2 * k + 1, 0, RB[k], 2, 48);
            dn(P_BREAK, DT_BASS, 2 * k + 1, 4, RB[k] + 7 > 47 ? RB[k] - 5 : RB[k] + 7, 2, 42);
        }
        for (i = 0; i < 8; i++)
            drums_put(P_BREAK, i, "5.......4.......", 0, "2.3.2.3.2.3.2.3.", 0, "5..3..5...3.5...", 76, 8);
    }
    /* OUTRO: the piano alone, slower to fade */
    roll(P_OUTRO, DT_PIANO, 0, 0, C_MAJ7, 16, 46);
    roll(P_OUTRO, DT_PIANO, 2, 0, F_MAJ7, 16, 44);
    roll(P_OUTRO, DT_PIANO, 4, 0, A_M7, 16, 42);
    roll(P_OUTRO, DT_PIANO, 6, 0, C_ADD9, 16, 40);
    melody_put(P_OUTRO, DT_PIANO, 0, M_OUTRO, 9, 0);
    dn(P_OUTRO, DT_BASS, 0, 0, 36, 15, 60);
    dn(P_OUTRO, DT_BASS, 6, 0, 36, 15, 50);
}
