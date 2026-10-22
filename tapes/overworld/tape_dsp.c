/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 OVERWORLD / DECK contributors; the sound after OVERWORLD (its ow.c) and AMB-1 (C) Charles Vestal */
/* OVERWORLD, a tape for DECK: lo-fi ambient after the music of C418. A quiet felt piano, warm pads, glass bells,
 * dusty lo-fi drums, the rain and the vinyl. Much air, few notes, everything drifting a little.
 *
 * Its own -O2 unit (DECK's dsp_unit.c includes it): the engines (eng_*.c), the track effects and the master (fx.c),
 * DREAM (dream.c, DECK's DRIVE), the FINGER patterns, the presets, the moods, the demo song (demo.c). */
#include "deck_api.h"
#include "fastmath.h"

#define CHECK_SIZE(T) typedef char T##_fits[(sizeof(T) <= DK_VSTATE * sizeof(float)) ? 1 : -1]

static uint32_t trng = 0x2545F491u;
static inline uint32_t rnd(void)
{
    uint32_t x = trng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    trng = x;
    return x;
}
static inline float frnd(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static inline float wnoise(void) { return (float)(int32_t)rnd() * (1.0f / 2147483648.0f); }

#include "eng_keys.c"
#include "eng_pad.c"
#include "eng_kit.c"
#include "eng_world.c"
#include "fx.c"

/* ------------------------------------------------------------- FINGER patterns --- */
/* drums: lanes KICK SNARE RIM SHAKER HAT OPEN CLAP WOOD, a sixteenth a character ('.' none, 1..9 how hard); from
 * only a shaker to a full boom-bap, then three breaks (a rim roll, a stop, a record slowing down) */
static const char *const FD_SRC[16][9] = {
    {"SHAKER", "", "", "", "3.2.3.2.3.2.3.2.", "", "", "", ""},
    {"PULSE", "6.......5.......", "", "", "3.2.3.2.3.2.3.2.", "", "", "", ""},
    {"WOOD", "5.......", "", "", "2.3.2.3.2.3.2.3.", "", "", "", "5..3..5...3.5..."},
    {"EMBERS", "5.......5.......", "", "..4.......4.....", "3.2.3.2.3.2.3.2.", "", "", "", "....4.......4..3"},
    {"LAZY", "7......5..5.....", "....5.......5...", "", "3.2.3.2.3.2.3.2.", "", "", "", ""},
    {"RIMSHOT", "7.......6.......", "", "....6.......6...", "", "3.2.3.2.3.2.3.2.", "", "", ""},
    {"BOOM BAP", "8.....6.7.......", "....7.......7...", "", "", "4.3.4.3.4.3.4.3.", "", "", ""},
    {"DUSTY", "8.....6.7.......", "....6.......6...", "", "2.2.2.2.2.2.2.2.", "4...4...4...4...", "", "....4.......4...", ""},
    {"SHUFFLE", "7..5..6.......5.", "....7.......7...", "", "", "4.24.24.24.24.2.", "..............3.", "", ""},
    {"BOSSA", "6..4..6.6..4..6.", "", "..5..5....5..5..", "2.2.2.2.2.2.2.2.", "", "", "", ""},
    {"HALFTIME", "8.......6..4....", "........8.......", "", "3.3.3.3.3.3.3.3.", "4.3.4.3.4.3.4.3.", "", "", ""},
    {"STEADY", "8.....6.7..5....", "....8.......8..3", "", "", "4343434343434343", "..............4.", "", ""},
    {"FULL", "8.....6.7..5....", "....8.......8..3", "", "3.3.3.3.3.3.3.3.", "4.4.4.4.4.4.4.4.", "......3.......3.",
     "....5.......5...", "...3.......3...."},
    {"RIM ROLL", "", "", "..3.4.5.6.7.8899", "", "", "", "", ""},
    {"STOP", "8...............", "", "", "", "", "", "", ""},
    {"SLOWDOWN", "", "9...7.....5.....", "", "", "", "", "", "3..........2...."},
};
/* the melodic ones: chord tones ('1' the root, '2' its third, '3' fifth, '4' seventh, '5'..'8' an octave up), each
 * note's length in sixteenths ('.' one) */
static const char *const FM_SRC[16][3] = {
    {"ROOT", "1...............", "8"},          {"ROOT 2", "1.......1.......", "6.......6"},
    {"FIFTH", "1.......3.......", "6.......6"}, {"PULSE", "1...1...1...1...", "3...3...3...3"},
    {"BROKEN", "1...3...5...3...", "4...4...4...4"}, {"ARP", "1.2.3.4.3.2.3.4.", "2.2.2.2.2.2.2.2"},
    {"SWAY", "1...2.3.1...2.3.", "3...2.2.3...2.2"}, {"LAZY", "..1...3...2...5.", "..3...3...3...3"},
    {"STEPS", "1.2.3.5.4.3.2.1.", "2.2.2.2.2.2.2.2"}, {"OFFBEAT", "..1...1...1...1.", "..2...2...2...2"},
    {"HIGH", "5...4...3...2...", "4...4...4...4"},  {"SIXTEENTHS", "1323132313231323", ""},
    {"DENSE", "1.3.5.8.5.3.1.3.", "2.2.2.2.2.2.2.2"}, {"FILL UP", "............1235", ""},
    {"FILL DOWN", "............8531", ""},        {"HOLD", "1...............", "g"},
};
static dk_finger_t FINGER_DRUM[16], FINGER_MEL[16];
static void finger_build(void)
{
    int p, l, s;
    for (p = 0; p < 16; p++) {
        FINGER_DRUM[p].name = FD_SRC[p][0];
        for (l = 0; l < 8; l++) {
            const char *c = FD_SRC[p][l + 1];
            for (s = 0; s < 16; s++)
                {
                int k = 0;
                while (c[k] && k < 16)
                    k++;
                FINGER_DRUM[p].lanes[l][s] = (uint8_t)(s < k && c[s] >= '1' && c[s] <= '9' ? c[s] - '0' : 0);
            }
        }
        FINGER_MEL[p].name = FM_SRC[p][0];
        for (s = 0; s < 16; s++) {
            const char *ln = FM_SRC[p][2];
            int k = 0;
            char t = FM_SRC[p][1][s], d;
            while (ln[k] && k < 16)
                k++;
            d = s < k ? ln[s] : '.';
            FINGER_MEL[p].lanes[0][s] = (uint8_t)(t >= '1' && t <= '8' ? t - '0' : 0);
            FINGER_MEL[p].lanes[1][s] = (uint8_t)(d >= '1' && d <= '9' ? d - '0' : d == 'g' ? 16 : 1);
            FINGER_MEL[p].lanes[2][s] = 0;
        }
    }
}

#include "dream.c"

/* -------------------------------------------------------------------- presets --- */
/* name, engine, its parameters, ENV, the effect (kind, amount, a, b), reverb, echo, MACRO's targets and amounts, trim */
#define FELT_M {T_P1, T_FXAMT, T_REV}, {50, 60, 35}
#define TINE_M {T_P1, T_FXB, T_P3}, {50, 50, -40}
#define PAD_M {T_P0, T_REV, T_P2}, {45, 45, 40}
#define GLASS_M {T_P1, T_ECHO, T_P3}, {45, 50, 50}
#define BASS_M {T_P0, T_P4, T_NONE}, {-40, 50, 0}
#define KIT_M {T_P2, T_SWING, T_FXAMT}, {50, 40, 40}
#define WORLD_M {T_P1, T_P2, T_REV}, {45, -40, 40}
#define KAL_M {T_P2, T_REV, T_P0}, {50, 40, -25}
#define E50 {50, 50, 50}
static const dk_preset_t PRESETS[8 * DK_NFACT] = {
    /* FELT */
    {"MEADOW", 0, {45, 40, 55, 50, 55}, E50, FX_N_TAPE, 15, 30, 20, 60, 12, FELT_M, 1},
    {"SWEDEN", 0, {40, 50, 60, 55, 60}, E50, FX_N_TAPE, 25, 35, 25, 70, 15, FELT_M, 0},
    {"LULLABY", 0, {30, 75, 60, 40, 50}, E50, FX_N_VINYL, 20, 30, 20, 70, 10, FELT_M, 0},
    {"MORNING", 0, {58, 25, 50, 45, 60}, E50, FX_N_NONE, 0, 0, 0, 50, 10, FELT_M, 3},
    {"SNOWFALL", 0, {48, 40, 65, 40, 65}, E50, FX_N_NONE, 0, 0, 0, 85, 30, FELT_M, -2},
    {"ATTIC", 0, {40, 60, 50, 70, 45}, E50, FX_N_VINYL, 45, 50, 40, 45, 8, FELT_M, 2},
    {"HALL", 0, {55, 20, 70, 30, 70}, E50, FX_N_NONE, 0, 0, 0, 90, 12, FELT_M, 0},
    {"DISTANT", 0, {30, 80, 70, 30, 40}, E50, FX_N_TAPE, 30, 40, 20, 100, 25, FELT_M, -3},
    {"GRAND", 0, {66, 10, 65, 30, 70}, E50, FX_N_NONE, 0, 0, 0, 55, 8, FELT_M, 2},
    {"LOFI KEYS", 0, {42, 55, 45, 60, 45}, E50, FX_N_LOFI, 40, 35, 30, 45, 15, FELT_M, 2},
    {"NIGHT", 0, {32, 70, 50, 50, 50}, E50, FX_N_TAPE, 20, 25, 20, 75, 25, FELT_M, 0},
    {"MEMORY", 0, {38, 65, 60, 55, 55}, E50, FX_N_TAPE, 45, 45, 35, 80, 20, FELT_M, -1},
    /* TINE */
    {"RAINY DAY", 1, {50, 35, 30, 45, 50}, E50, FX_N_TAPE, 20, 30, 25, 40, 15, TINE_M, 0},
    {"SUITCASE", 1, {55, 50, 20, 55, 55}, E50, FX_N_NONE, 0, 0, 0, 35, 10, TINE_M, 1},
    {"DUSTY", 1, {45, 40, 45, 35, 50}, E50, FX_N_TAPE, 40, 40, 45, 40, 15, TINE_M, 0},
    {"VELVET", 1, {35, 25, 20, 30, 60}, E50, FX_N_CHORUS, 35, 30, 40, 50, 10, TINE_M, 1},
    {"BELLTONE", 1, {80, 20, 20, 60, 60}, E50, FX_N_NONE, 0, 0, 0, 50, 20, TINE_M, -1},
    {"STUDY", 1, {48, 30, 35, 40, 45}, E50, FX_N_VINYL, 30, 40, 30, 35, 12, TINE_M, 0},
    {"WARM", 1, {30, 20, 15, 25, 55}, E50, FX_N_TAPE, 15, 20, 30, 40, 8, TINE_M, -1},
    {"SHIMMER", 1, {65, 45, 25, 50, 65}, E50, FX_N_CHORUS, 50, 40, 50, 60, 25, TINE_M, 2},
    {"BROKEN", 1, {50, 60, 70, 40, 45}, E50, FX_N_LOFI, 35, 40, 35, 35, 10, TINE_M, 1},
    {"MELLOW", 1, {25, 15, 10, 20, 50}, E50, FX_N_NONE, 0, 0, 0, 45, 10, TINE_M, -1},
    {"NIGHT", 1, {40, 55, 30, 35, 60}, E50, FX_N_SPRING, 30, 50, 40, 50, 15, TINE_M, 2},
    {"WORN", 1, {45, 45, 60, 45, 50}, E50, FX_N_TAPE, 55, 60, 50, 40, 15, TINE_M, 0},
    /* AIRPAD */
    {"SWEDEN", 2, {45, 50, 30, 0, 50}, E50, FX_N_NONE, 0, 0, 0, 70, 10, PAD_M, -2},
    {"AIR", 2, {50, 50, 45, 1, 50}, E50, FX_N_NONE, 0, 0, 0, 75, 10, PAD_M, -9},
    {"WARM", 2, {40, 50, 25, 2, 50}, E50, FX_N_TAPE, 15, 20, 20, 65, 8, PAD_M, 1},
    {"SHIMMER", 2, {55, 55, 40, 3, 50}, E50, FX_N_CHORUS, 30, 30, 40, 85, 15, PAD_M, 7},
    {"STRINGS", 2, {45, 45, 25, 4, 45}, E50, FX_N_NONE, 0, 0, 0, 70, 8, PAD_M, -8},
    {"CAVE", 2, {25, 65, 35, 0, 60}, E50, FX_N_NONE, 0, 0, 0, 95, 15, PAD_M, -2},
    {"DAWN", 2, {60, 60, 50, 1, 55}, E50, FX_N_CHORUS, 25, 20, 30, 80, 12, PAD_M, -7},
    {"FOG", 2, {20, 70, 60, 2, 70}, E50, FX_N_TAPE, 30, 30, 20, 90, 10, PAD_M, 1},
    {"GLOW", 2, {55, 40, 30, 3, 40}, E50, FX_N_NONE, 0, 0, 0, 75, 12, PAD_M, 3},
    {"SKY", 2, {65, 60, 55, 1, 50}, E50, FX_N_CHORUS, 35, 25, 35, 95, 20, PAD_M, -8},
    {"DEEP", 2, {30, 50, 20, 4, 55}, E50, FX_N_NONE, 0, 0, 0, 60, 5, PAD_M, -6},
    {"SLOW", 2, {40, 72, 40, 0, 50}, E50, FX_N_TAPE, 20, 25, 20, 85, 12, PAD_M, 0},
    /* GLASS */
    {"GLASS", 3, {0, 50, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 75, 25, GLASS_M, -2},
    {"CELESTA", 3, {1, 50, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 70, 20, GLASS_M, 0},
    {"MARIMBA", 3, {2, 50, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 55, 10, GLASS_M, 9},
    {"MUSIC BOX", 3, {3, 55, 55, 0}, E50, FX_N_NONE, 0, 0, 0, 70, 25, GLASS_M, 0},
    {"VIBES", 3, {4, 50, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 60, 15, GLASS_M, 4},
    {"SNOW", 3, {0, 70, 45, 1}, E50, FX_N_NONE, 0, 0, 0, 90, 40, GLASS_M, -5},
    {"STARS", 3, {3, 60, 50, 1}, E50, FX_N_NONE, 0, 0, 0, 85, 35, GLASS_M, -4},
    {"CHAPEL", 3, {1, 75, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 95, 20, GLASS_M, -2},
    {"WOOD", 3, {2, 40, 35, 0}, E50, FX_N_NONE, 0, 0, 0, 45, 8, GLASS_M, 8},
    {"DROPS", 3, {0, 40, 50, 1}, E50, FX_N_NONE, 0, 0, 0, 80, 30, GLASS_M, -4},
    {"NIGHT VIBES", 3, {4, 70, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 75, 25, GLASS_M, 2},
    {"WIND CHIME", 3, {0, 75, 45, 1}, E50, FX_N_NONE, 0, 0, 0, 90, 30, GLASS_M, -5},
    /* SOFTBASS */
    {"SUB", 4, {40, 35, 0, 0, 50}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, -1},
    {"ROUND", 4, {30, 20, 0, 0, 60}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, -1},
    {"UPRIGHT", 4, {45, 50, 0, 2, 50}, E50, FX_N_NONE, 0, 0, 0, 20, 0, BASS_M, 1},
    {"THUMB", 4, {55, 70, 0, 2, 40}, E50, FX_N_NONE, 0, 0, 0, 15, 0, BASS_M, 2},
    {"MOOG", 4, {40, 40, 0, 1, 50}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 6},
    {"GLIDE", 4, {40, 30, 50, 0, 55}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 0},
    {"DEEP", 4, {20, 15, 0, 0, 75}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 0},
    {"WARM MOOG", 4, {30, 25, 20, 1, 55}, E50, FX_N_TAPE, 20, 30, 30, 10, 0, BASS_M, 5},
    {"JAZZ", 4, {50, 60, 0, 2, 45}, E50, FX_N_NONE, 0, 0, 0, 20, 0, BASS_M, 2},
    {"FELT BASS", 4, {35, 45, 0, 0, 40}, E50, FX_N_TAPE, 25, 20, 20, 10, 0, BASS_M, 0},
    {"NIGHT", 4, {25, 25, 30, 0, 65}, E50, FX_N_NONE, 0, 0, 0, 15, 0, BASS_M, 0},
    {"PLUCKY", 4, {60, 80, 0, 1, 30}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 6},
    /* DUSTKIT */
    {"DUSTY", 5, {50, 50, 45, 1}, E50, FX_N_VINYL, 25, 40, 30, 15, 0, KIT_M, 4},
    {"707", 5, {50, 50, 15, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 0, KIT_M, -1},
    {"BRUSH", 5, {50, 55, 35, 2}, E50, FX_N_NONE, 0, 0, 0, 25, 0, KIT_M, -1},
    {"BOOM BAP", 5, {45, 50, 55, 1}, E50, FX_N_TAPE, 30, 30, 40, 12, 0, KIT_M, 5},
    {"SOFT", 5, {55, 40, 40, 2}, E50, FX_N_VINYL, 20, 30, 20, 20, 0, KIT_M, 2},
    {"WOODEN", 5, {60, 45, 30, 2}, E50, FX_N_NONE, 0, 0, 0, 25, 0, KIT_M, 0},
    {"OLD TAPE", 5, {45, 55, 70, 1}, E50, FX_N_TAPE, 40, 40, 50, 12, 0, KIT_M, 4},
    {"CRISP", 5, {55, 45, 25, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 0, KIT_M, 0},
    {"LAZY", 5, {42, 60, 50, 1}, E50, FX_N_VINYL, 35, 40, 30, 15, 0, KIT_M, 4},
    {"CAMPFIRE", 5, {55, 50, 35, 2}, E50, FX_N_NONE, 0, 0, 0, 30, 0, KIT_M, 0},
    {"TINY", 5, {70, 35, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 15, 0, KIT_M, 0},
    {"DEEP", 5, {38, 65, 55, 1}, E50, FX_N_TAPE, 25, 30, 40, 12, 0, KIT_M, 4},
    /* WORLD */
    {"RAIN", 6, {0, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 13},
    {"WIND", 6, {1, 50, 50}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 23},
    {"FIRE", 6, {2, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 10, 0, WORLD_M, 15},
    {"STREAM", 6, {3, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 9},
    {"CAVE DROPS", 6, {4, 50, 50}, E50, FX_N_NONE, 0, 0, 0, 60, 10, WORLD_M, 9},
    {"CRICKETS", 6, {5, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 14},
    {"BIRDS", 6, {6, 50, 60}, E50, FX_N_NONE, 0, 0, 0, 25, 0, WORLD_M, 12},
    {"OCEAN", 6, {7, 50, 50}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 10},
    {"THUNDER", 6, {8, 50, 45}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 14},
    {"SNOW STEPS", 6, {9, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 1},
    {"STORM", 6, {8, 80, 50}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 5},
    {"DRIZZLE", 6, {0, 25, 40}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 24},
    /* KALIMBA */
    {"KALIMBA", 7, {50, 50, 45}, E50, FX_N_NONE, 0, 0, 0, 50, 15, KAL_M, 0},
    {"MBIRA", 7, {60, 45, 35}, E50, FX_N_TAPE, 25, 30, 30, 45, 12, KAL_M, -1},
    {"THUMB", 7, {40, 55, 55}, E50, FX_N_NONE, 0, 0, 0, 45, 10, KAL_M, -1},
    {"BRIGHT", 7, {75, 40, 30}, E50, FX_N_NONE, 0, 0, 0, 50, 15, KAL_M, -1},
    {"CAMPFIRE", 7, {40, 55, 70}, E50, FX_N_NONE, 0, 0, 0, 55, 10, KAL_M, 1},
    {"BOX", 7, {70, 65, 20}, E50, FX_N_NONE, 0, 0, 0, 60, 30, KAL_M, -5},
    {"WOODEN", 7, {35, 35, 80}, E50, FX_N_NONE, 0, 0, 0, 35, 8, KAL_M, 3},
    {"DREAM", 7, {55, 75, 40}, E50, FX_N_NONE, 0, 0, 0, 85, 30, KAL_M, -4},
    {"LOFI", 7, {45, 45, 50}, E50, FX_N_LOFI, 35, 35, 35, 40, 10, KAL_M, 0},
    {"STARLIGHT", 7, {65, 70, 30}, E50, FX_N_NONE, 0, 0, 0, 80, 25, KAL_M, -5},
    {"TINY", 7, {80, 30, 25}, E50, FX_N_NONE, 0, 0, 0, 45, 15, KAL_M, -1},
    {"DEEP", 7, {30, 65, 60}, E50, FX_N_NONE, 0, 0, 0, 55, 10, KAL_M, -1},
};

static const dk_engine_t *const ENGINES[8] = {&ENG_FELT, &ENG_TINE, &ENG_AIRPAD, &ENG_GLASS, &ENG_SOFTBASS, &ENG_DUSTKIT,
                                              &ENG_WORLD, &ENG_KALIMBA};

/* ---------------------------------------------------------------- the harmony --- */
static const char *const STYLE_NAME[NSTYLE] = {"C418", "LOFI", "AMBIENT", "CAMPFIRE"};
static const uint8_t STYLE_CHORD[NSTYLE] = {CH_MAJ7, CH_9, CH_ADD9, CH_TRIAD};
static const uint8_t STYLE_BARS[NSTYLE] = {2, 1, 2, 1};
static const uint8_t STYLE_BREATH[NSTYLE] = {1, 0, 1, 0};
/* I-IVmaj7-vi-IV, I-iii-IV-iv (C418), IVmaj7-I-vi-V, ii9-V7-Imaj9-vi9 (lo-fi), Imaj7-II-IVmaj7-I (lydian) */
static const int8_t PROGS[5][4] = {{0, 3, 5, 3}, {0, 2, 3, 10}, {3, 0, 5, 4}, {1, 4, 0, 5}, {0, 12, 3, 0}};
static const dk_borrow_t BORROW_MAJ[4] = {
    {5, {0, 3, 7, 14, 99}}, {10, {0, 4, 7, 14, 99}}, {2, {0, 4, 7, 14, 99}}, {5, {-5, 0, 4, 7, 11, 14, 99}}};
static const dk_borrow_t BORROW_MIN[4] = {
    {5, {0, 4, 7, 14, 99}}, {8, {0, 4, 7, 14, 99}}, {3, {0, 4, 7, 11, 99}}, {7, {0, 3, 7, 10, 14, 99}}};

static const char *const PERFORM[11] = {"TAPESTOP", "REWIND", "VINYL STOP", "LOW PASS", "RAIN UP", "DREAM",
                                        "HALF TIME", "ECHO THROW", "MUTE DRUMS", "SLEEP", "REVERSE"};
static const uint8_t PERFORM_ACT[11] = {PF_TAPESTOP, PF_REWIND, PF_VINYLSTOP, PF_LOWPASS, PF_RAINUP, PF_DREAM,
                                        PF_HALFTIME, PF_ECHOTHROW, PF_MUTEDRUMS, PF_SLEEP, PF_REVERSE};

/* the biomes (the garden's scenes): DAWN BLOSSOM RAIN EMBER SNOW LAGOON NIGHT AURORA DUSK */
static const char *const BIOME_NAME[9] = {"DAWN", "BLOSSOM", "RAIN", "EMBER", "SNOW", "LAGOON", "NIGHT", "AURORA", "DUSK"};
/* name, tempo, mode, style, biome, world preset, dust, space (ROOM HALL CAVE SHIMMER); the tracks' engines, presets */
static const dk_mood_t MOODS[8] = {
    {"PLAINS", 76, 0, ST_C418, 0, 6, 25, 1, {5, 4, 0, 2, 3, 6}, {4, 1, 0, 0, 0, 6}},
    {"RAIN", 72, 2, ST_LOFI, 2, 0, 45, 0, {5, 4, 0, 1, 3, 6}, {0, 0, 9, 0, 2, 0}},
    {"CAVE", 64, 1, ST_AMBIENT, 7, 4, 20, 2, {5, 4, 0, 2, 3, 6}, {4, 6, 7, 5, 7, 4}},
    {"SNOW", 68, 4, ST_AMBIENT, 4, 9, 20, 3, {5, 4, 0, 2, 3, 6}, {4, 1, 4, 3, 3, 9}},
    {"OCEAN", 70, 5, ST_C418, 5, 7, 25, 1, {5, 4, 0, 7, 3, 6}, {4, 0, 1, 0, 0, 7}},
    {"NIGHT", 60, 0, ST_C418, 6, 5, 30, 1, {5, 4, 0, 2, 3, 6}, {4, 10, 10, 1, 10, 5}},
    {"CAMPFIRE", 82, 8, ST_CAMPFIRE, 3, 2, 30, 0, {5, 4, 7, 7, 3, 6}, {9, 2, 4, 0, 8, 2}},
    {"STUDY", 84, 2, ST_LOFI, 8, 11, 55, 0, {5, 4, 0, 1, 3, 6}, {3, 8, 9, 5, 4, 11}},
};

/* the tracks by default: DRUMS BASS PIANO KEYS BELLS NATURE */
static const uint8_t DEF_ENG[DK_NTRACK] = {5, 4, 0, 1, 3, 6}, DEF_PRESET[DK_NTRACK] = {0, 0, 0, 0, 0, 6},
                     DEF_SEQ[DK_NTRACK] = {SQ_FINGER, SQ_STEP, SQ_LIVE, SQ_ARP, SQ_TOMBOLA, SQ_HOLD},
                     DEF_ROLE[DK_NTRACK] = {R_DRUM, R_BASS, R_KEYS, R_ARP, R_BELL, R_NATURE};

#include "demo.c"

static void tape_init(void)
{
    pad_tables();
    finger_build();
}

const dk_tape_t TAPE = {
    "OVERWORLD", tape_init, ENGINES, 8, PRESETS, TAPE_FX, 8, STYLE_NAME, NSTYLE, tape_drive, PROGS, 5, STYLE_CHORD,
    STYLE_BARS, STYLE_BREATH, BORROW_MAJ, BORROW_MIN, FINGER_DRUM, FINGER_MEL, PERFORM, PERFORM_ACT, MOODS, 8,
    BIOME_NAME, tape_master, tape_demo, METRO_WOOD, 25, DEF_ENG, DEF_PRESET, DEF_SEQ, DEF_ROLE,
};
