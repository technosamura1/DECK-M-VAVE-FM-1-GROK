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
#include "eng_wind.c"
#include "eng_synth.c"
#include "eng_lead.c"
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
static const char *const FM_SRC[16][6] = {
    {"ROOT", "1...............", "8"},          {"ROOT 2", "1.......1.......", "6.......6"},
    {"FIFTH", "1.......3.......", "6.......6"}, {"PULSE", "1...1...1...1...", "3...3...3...3"},
    {"BROKEN", "1...3...5...3...", "4...4...4...4"}, {"ARP", "1.2.3.4.3.2.3.4.", "2.2.2.2.2.2.2.2"},
    {"SWAY", "1...2.3.1...2.3.", "3...2.2.3...2.2"}, {"LAZY", "..1...3...2...5.", "..3...3...3...3"},
    {"STEPS", "1.2.3.5.4.3.2.1.", "2.2.2.2.2.2.2.2"}, {"OFFBEAT", "..1...1...1...1.", "..2...2...2...2"},
    {"HIGH", "5...4...3...2...", "4...4...4...4"},  {"SIXTEENTHS", "1323132313231323", ""},
    {"DENSE", "1.3.5.8.5.3.1.3.", "2.2.2.2.2.2.2.2"}, {"FILL UP", "............1235", ""},
    {"FILL DOWN", "............8531", ""},        {"HOLD", "1...............", "g"},
};

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
#define WIND_M {T_P0, T_P1, T_REV}, {40, 35, 35}
#define SYN_M {T_P1, T_P3, T_REV}, {50, 40, 40}
#define PLK_M {T_P1, T_P2, T_REV}, {50, 40, 40}
#define E50 {50, 50, 50}
#define SNAP {5, 24, 18}          /* a hit: short, gone */
#define TONE {14, 42, 36}         /* a note you play */
#define LONG {36, 58, 72}         /* it stays */
#define LD_M {T_P1, T_P3, T_REV}, {50, 40, 40}
static const dk_preset_t PRESETS[12 * DK_NFACT] = {
    /* FELT */
    {"MEADOW", 0, {45, 40, 55, 50, 55}, E50, FX_N_TAPE, 15, 30, 20, 60, 12, FELT_M, 0},
    {"SWEDEN", 0, {40, 50, 60, 55, 60}, E50, FX_N_TAPE, 25, 35, 25, 70, 15, FELT_M, 0},
    {"LULLABY", 0, {30, 75, 60, 40, 50}, E50, FX_N_VINYL, 20, 30, 20, 70, 10, FELT_M, -1},
    {"MORNING", 0, {58, 25, 50, 45, 60}, E50, FX_N_NONE, 0, 0, 0, 50, 10, FELT_M, 2},
    {"SNOWFALL", 0, {48, 40, 65, 40, 65}, E50, FX_N_NONE, 0, 0, 0, 85, 30, FELT_M, -1},
    {"ATTIC", 0, {40, 60, 50, 70, 45}, E50, FX_N_VINYL, 45, 50, 40, 45, 8, FELT_M, 2},
    {"HALL", 0, {55, 20, 70, 30, 70}, E50, FX_N_NONE, 0, 0, 0, 90, 12, FELT_M, -1},
    {"DISTANT", 0, {30, 80, 70, 30, 40}, E50, FX_N_TAPE, 30, 40, 20, 100, 25, FELT_M, -4},
    {"GRAND", 0, {66, 10, 65, 30, 70}, E50, FX_N_NONE, 0, 0, 0, 55, 8, FELT_M, 2},
    {"LOFI KEYS", 0, {42, 55, 45, 60, 45}, E50, FX_N_LOFI, 40, 35, 30, 45, 15, FELT_M, 2},
    {"NIGHT", 0, {32, 70, 50, 50, 50}, E50, FX_N_TAPE, 20, 25, 20, 75, 25, FELT_M, -1},
    {"MEMORY", 0, {38, 65, 60, 55, 55}, E50, FX_N_TAPE, 45, 45, 35, 80, 20, FELT_M, -2},
    /* TINE: twelve electric pianos, not one with the knobs nudged */
    {"RAINY DAY", 1, {48, 28, 22, 42, 58}, TONE, FX_N_TAPE, 18, 25, 20, 42, 14, TINE_M, 0},
    {"SUITCASE", 1, {78, 6, 4, 82, 22}, SNAP, FX_N_NONE, 0, 0, 0, 14, 0, TINE_M, 2},
    {"DUSTY", 1, {36, 70, 80, 28, 46}, {28, 52, 44}, FX_N_VINYL, 60, 70, 45, 32, 6, TINE_M, 0},
    {"VELVET", 1, {88, 42, 18, 58, 78}, LONG, FX_N_CHORUS, 50, 22, 60, 58, 16, TINE_M, 2},
    {"BELLTONE", 1, {98, 10, 6, 92, 16}, {2, 20, 42}, FX_N_SPRING, 32, 60, 50, 64, 30, TINE_M, 3},
    {"STUDY", 1, {32, 14, 10, 22, 34}, {8, 36, 26}, FX_N_NONE, 0, 0, 0, 18, 4, TINE_M, 1},
    {"WURLI", 1, {62, 78, 12, 60, 36}, SNAP, FX_N_TREM, 38, 22, 48, 22, 6, TINE_M, 8},
    {"SHIMMER", 1, {74, 30, 14, 46, 86}, LONG, FX_N_CHORUS, 55, 35, 65, 72, 38, TINE_M, 1},
    {"BROKEN", 1, {50, 90, 95, 34, 24}, SNAP, FX_N_LOFI, 78, 70, 55, 18, 4, TINE_M, 1},
    {"MELLOW", 1, {12, 4, 4, 8, 70}, {42, 60, 68}, FX_N_TAPE, 10, 12, 18, 36, 8, TINE_M, 0},
    {"NIGHT", 1, {40, 58, 48, 24, 74}, LONG, FX_N_SPRING, 26, 48, 32, 55, 24, TINE_M, 1},
    {"CLAV", 1, {92, 2, 2, 100, 8}, {2, 12, 10}, FX_N_FILTER, 78, 40, 8, 8, 10, TINE_M, 3},
    /* AIRPAD */
    {"SWEDEN", 2, {45, 50, 30, 0, 50}, E50, FX_N_NONE, 0, 0, 0, 70, 10, PAD_M, -4},
    {"AIR", 2, {50, 50, 45, 1, 50}, E50, FX_N_NONE, 0, 0, 0, 75, 10, PAD_M, -10},
    {"WARM", 2, {40, 50, 25, 2, 50}, E50, FX_N_TAPE, 15, 20, 20, 65, 8, PAD_M, 1},
    {"SHIMMER", 2, {55, 55, 40, 3, 50}, E50, FX_N_CHORUS, 30, 30, 40, 85, 15, PAD_M, 9},
    {"STRINGS", 2, {45, 45, 25, 4, 45}, E50, FX_N_NONE, 0, 0, 0, 70, 8, PAD_M, -8},
    {"CAVE", 2, {25, 65, 35, 0, 60}, E50, FX_N_NONE, 0, 0, 0, 95, 15, PAD_M, -3},
    {"DAWN", 2, {60, 60, 50, 1, 55}, E50, FX_N_CHORUS, 25, 20, 30, 80, 12, PAD_M, -6},
    {"FOG", 2, {20, 70, 60, 2, 70}, E50, FX_N_TAPE, 30, 30, 20, 90, 10, PAD_M, 4},
    {"GLOW", 2, {55, 40, 30, 3, 40}, E50, FX_N_NONE, 0, 0, 0, 75, 12, PAD_M, 3},
    {"SKY", 2, {65, 60, 55, 1, 50}, E50, FX_N_CHORUS, 35, 25, 35, 95, 20, PAD_M, -7},
    {"DEEP", 2, {30, 50, 20, 4, 55}, E50, FX_N_NONE, 0, 0, 0, 60, 5, PAD_M, -7},
    {"SLOW", 2, {40, 72, 40, 0, 50}, E50, FX_N_TAPE, 20, 25, 20, 85, 12, PAD_M, 1},
    /* GLASS: a mallet, a box, a bell, not the same shine twelve times */
    {"GLASS", 3, {0, 32, 78, 0}, {6, 36, 48}, FX_N_NONE, 0, 0, 0, 68, 18, GLASS_M, -3},
    {"CELESTA", 3, {1, 24, 48, 1}, SNAP, FX_N_SPRING, 22, 42, 35, 48, 16, GLASS_M, 4},
    {"MARIMBA", 3, {2, 14, 72, 0}, {2, 16, 12}, FX_N_NONE, 0, 0, 0, 18, 4, GLASS_M, 14},
    {"MUSIC BOX", 3, {3, 18, 36, 2}, {2, 20, 28}, FX_N_TREM, 28, 18, 72, 36, 20, GLASS_M, 8},
    {"VIBES", 3, {4, 62, 30, 0}, TONE, FX_N_TREM, 62, 12, 40, 34, 10, GLASS_M, 9},
    {"SNOW", 3, {0, 90, 22, 1}, LONG, FX_N_NONE, 0, 0, 0, 96, 48, GLASS_M, -4},
    {"STARS", 3, {3, 36, 96, 2}, {4, 28, 55}, FX_N_CHORUS, 32, 22, 48, 64, 42, GLASS_M, 3},
    {"CHAPEL", 3, {1, 98, 22, 0}, {22, 68, 92}, FX_N_NONE, 0, 0, 0, 100, 12, GLASS_M, -4},
    {"WOOD", 3, {2, 8, 15, 0}, {2, 12, 10}, FX_N_NONE, 0, 0, 0, 12, 2, GLASS_M, 16},
    {"DROPS", 3, {0, 22, 0, 0}, SNAP, FX_N_SPRING, 40, 65, 50, 70, 32, GLASS_M, 0},
    {"GLOCK", 3, {1, 10, 100, 2}, {2, 14, 26}, FX_N_NONE, 0, 0, 0, 30, 16, GLASS_M, 2},
    {"CHIMES", 3, {0, 74, 50, 2}, LONG, FX_N_NONE, 0, 0, 0, 88, 44, GLASS_M, -4},
    /* SOFTBASS: a sub, a thumb, a glide, a reese. not twelve soft subs */
    {"SUB", 4, {18, 4, 0, 0, 78}, TONE, FX_N_NONE, 0, 0, 0, 6, 0, BASS_M, -1},
    {"ROUND", 4, {32, 18, 0, 1, 58}, TONE, FX_N_NONE, 0, 0, 0, 8, 0, BASS_M, 6},
    {"UPRIGHT", 4, {34, 48, 0, 2, 42}, TONE, FX_N_NONE, 0, 0, 0, 24, 0, BASS_M, 6},
    {"THUMB", 4, {96, 100, 0, 2, 6}, {2, 8, 8}, FX_N_NONE, 0, 0, 0, 10, 6, BASS_M, 11},
    {"MOOG", 4, {68, 40, 0, 1, 46}, {8, 34, 28}, FX_N_FILTER, 74, 62, 8, 6, 0, BASS_M, 7},
    {"GLIDE", 4, {55, 34, 82, 4, 72}, LONG, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 4},
    {"DEEP", 4, {4, 0, 0, 6, 88}, {14, 42, 42}, FX_N_NONE, 0, 0, 0, 16, 0, BASS_M, 4},
    {"REESE", 4, {52, 28, 6, 4, 74}, {16, 48, 44}, FX_N_CHORUS, 48, 16, 62, 6, 0, BASS_M, 7},
    {"JAZZ", 4, {78, 100, 0, 2, 6}, SNAP, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 7},
    {"FELT BASS", 4, {16, 22, 0, 0, 48}, {18, 46, 52}, FX_N_TAPE, 28, 22, 24, 12, 0, BASS_M, -1},
    {"NIGHT", 4, {22, 16, 12, 4, 88}, LONG, FX_N_TAPE, 16, 18, 20, 14, 0, BASS_M, 6},
    {"PLUCK", 4, {80, 100, 0, 3, 8}, {2, 10, 8}, FX_N_NONE, 0, 0, 0, 8, 8, BASS_M, 8},
    /* DUSTKIT: a different machine, not the same dust turned a little */
    {"DUSTY", 5, {46, 58, 68, 1}, TONE, FX_N_VINYL, 42, 55, 38, 16, 0, KIT_M, 3},
    {"707", 5, {50, 40, 22, 0}, SNAP, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 0},
    {"BRUSH", 5, {50, 46, 22, 2}, TONE, FX_N_NONE, 0, 0, 0, 34, 0, KIT_M, 0},
    {"BOOM BAP", 5, {40, 66, 74, 1}, {8, 44, 26}, FX_N_TAPE, 48, 42, 55, 12, 0, KIT_M, 4},
    {"808", 5, {34, 92, 4, 6}, {6, 55, 30}, FX_N_NONE, 0, 0, 0, 8, 0, KIT_M, -8},
    {"WOODEN", 5, {60, 32, 16, 2}, SNAP, FX_N_NONE, 0, 0, 0, 26, 0, KIT_M, 6},
    {"909", 5, {50, 38, 4, 7}, SNAP, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 0},
    {"CHIP", 5, {66, 24, 36, 3}, SNAP, FX_N_NONE, 0, 0, 0, 4, 0, KIT_M, 7},
    {"606", 5, {56, 50, 10, 8}, SNAP, FX_N_NONE, 0, 0, 0, 8, 0, KIT_M, 2},
    {"CAMPFIRE", 5, {48, 44, 28, 2}, TONE, FX_N_NONE, 0, 0, 0, 36, 0, KIT_M, 1},
    {"WAR", 5, {38, 74, 12, 5}, {8, 48, 36}, FX_N_NONE, 0, 0, 0, 48, 0, KIT_M, -2},
    {"SYNTH", 5, {58, 36, 8, 4}, SNAP, FX_N_CHORUS, 22, 24, 42, 16, 0, KIT_M, 0},
    /* WORLD */
    {"RAIN", 6, {0, 78, 42}, LONG, FX_N_NONE, 0, 0, 0, 22, 0, WORLD_M, 13},
    {"WIND", 6, {1, 40, 82}, LONG, FX_N_NONE, 0, 0, 0, 28, 0, WORLD_M, 1},
    {"FIRE", 6, {2, 88, 68}, TONE, FX_N_NONE, 0, 0, 0, 8, 0, WORLD_M, 6},
    {"STREAM", 6, {3, 62, 74}, TONE, FX_N_NONE, 0, 0, 0, 24, 0, WORLD_M, 4},
    {"CAVE DROPS", 6, {4, 36, 30}, {12, 50, 70}, FX_N_NONE, 0, 0, 0, 88, 16, WORLD_M, 20},
    {"CRICKETS", 6, {5, 70, 80}, TONE, FX_N_NONE, 0, 0, 0, 16, 0, WORLD_M, -2},
    {"BIRDS", 6, {6, 42, 84}, SNAP, FX_N_NONE, 0, 0, 0, 38, 0, WORLD_M, 4},
    {"OCEAN", 6, {7, 80, 26}, LONG, FX_N_NONE, 0, 0, 0, 30, 0, WORLD_M, 5},
    {"THUNDER", 6, {8, 96, 22}, {6, 36, 48}, FX_N_NONE, 0, 0, 0, 42, 0, WORLD_M, 14},
    {"SNOW STEPS", 6, {9, 32, 64}, SNAP, FX_N_NONE, 0, 0, 0, 32, 0, WORLD_M, 5},
    {"STORM", 6, {8, 100, 58}, {10, 40, 55}, FX_N_NONE, 0, 0, 0, 36, 0, WORLD_M, -1},
    {"DRIZZLE", 6, {0, 22, 66}, TONE, FX_N_NONE, 0, 0, 0, 10, 0, WORLD_M, 20},
    /* KALIMBA */
    {"KALIMBA", 7, {58, 42, 48}, SNAP, FX_N_NONE, 0, 0, 0, 38, 10, KAL_M, -1},
    {"MBIRA", 7, {28, 72, 92}, TONE, FX_N_TAPE, 18, 22, 22, 34, 6, KAL_M, -1},
    {"THUMB", 7, {10, 24, 100}, {2, 16, 12}, FX_N_NONE, 0, 0, 0, 20, 4, KAL_M, 3},
    {"BRIGHT", 7, {98, 18, 6}, SNAP, FX_N_NONE, 0, 0, 0, 28, 14, KAL_M, -1},
    {"CAMPFIRE", 7, {36, 84, 74}, TONE, FX_N_VINYL, 28, 40, 32, 46, 6, KAL_M, -1},
    {"BOX", 7, {18, 96, 98}, {10, 58, 42}, FX_N_NONE, 0, 0, 0, 52, 18, KAL_M, -3},
    {"WOODEN", 7, {48, 30, 82}, SNAP, FX_N_NONE, 0, 0, 0, 16, 2, KAL_M, 3},
    {"DREAM", 7, {64, 100, 16}, LONG, FX_N_CHORUS, 34, 22, 46, 92, 36, KAL_M, -12},
    {"LOFI", 7, {40, 48, 55}, TONE, FX_N_LOFI, 72, 58, 48, 24, 6, KAL_M, 1},
    {"STARLIGHT", 7, {100, 12, 0}, {2, 14, 40}, FX_N_SPRING, 28, 55, 42, 68, 32, KAL_M, 1},
    {"TINY", 7, {90, 4, 2}, {2, 8, 12}, FX_N_NONE, 0, 0, 0, 14, 8, KAL_M, 2},
    {"DEEP", 7, {8, 80, 78}, {14, 52, 48}, FX_N_NONE, 0, 0, 0, 44, 6, KAL_M, 0},
    /* WINDS: the breath, the stab and the swell are different instruments */
    {"FLUTE", 8, {64, 58, 32, 0, 36}, TONE, FX_N_NONE, 0, 0, 0, 46, 8, WIND_M, 1},
    {"PAN FLUTE", 8, {30, 96, 12, 1, 18}, SNAP, FX_N_NONE, 0, 0, 0, 32, 6, WIND_M, 2},
    {"OCARINA", 8, {22, 8, 6, 2, 42}, {20, 50, 48}, FX_N_NONE, 0, 0, 0, 52, 10, WIND_M, 1},
    {"SHAKUHACHI", 8, {26, 100, 58, 1, 88}, {78, 62, 80}, FX_N_NONE, 0, 0, 0, 72, 14, WIND_M, 7},
    {"CLARINET", 8, {18, 6, 14, 3, 28}, TONE, FX_N_NONE, 0, 0, 0, 30, 4, WIND_M, 0},
    {"TRUMPET", 8, {86, 4, 42, 4, 14}, SNAP, FX_N_NONE, 0, 0, 0, 26, 6, WIND_M, 1},
    {"MUTED", 8, {38, 16, 8, 5, 22}, SNAP, FX_N_FILTER, 72, 28, 6, 22, 8, WIND_M, 1},
    {"HORN", 8, {24, 8, 20, 6, 92}, {72, 56, 68}, FX_N_NONE, 0, 0, 0, 82, 8, WIND_M, 7},
    {"BRASS", 8, {74, 2, 10, 7, 10}, SNAP, FX_N_CHORUS, 28, 18, 36, 24, 4, WIND_M, 2},
    {"FLUGEL", 8, {40, 22, 26, 4, 58}, LONG, FX_N_TAPE, 20, 22, 24, 52, 12, WIND_M, 0},
    {"DUDUK", 8, {32, 74, 64, 9, 76}, {52, 58, 74}, FX_N_NONE, 0, 0, 0, 76, 16, WIND_M, -1},
    {"SYNBRASS", 8, {80, 0, 8, 8, 24}, SNAP, FX_N_CHORUS, 42, 28, 52, 28, 10, WIND_M, 5},
    /* SYNTH */
    {"SOFT SAW", 9, {0, 44, 6, 10, 38, 0, 100, 18}, TONE, FX_N_TAPE, 16, 22, 20, 48, 12, SYN_M, 6},
    {"DUST LEAD", 9, {4, 72, 16, 6, 12, 48, 75, 12}, SNAP, FX_N_LOFI, 62, 48, 40, 28, 14, SYN_M, 9},
    {"WARM SUPER", 9, {5, 36, 4, 6, 82, 0, 100, 22}, LONG, FX_N_CHORUS, 42, 24, 55, 58, 10, SYN_M, 10},
    {"SINE", 9, {7, 90, 0, 0, 0, 0, 100, 6}, {12, 48, 68}, FX_N_NONE, 0, 0, 0, 36, 38, SYN_M, -2},
    {"BED", 9, {5, 34, 8, 0, 70, 0, 100, 24}, {48, 62, 82}, FX_N_TAPE, 10, 16, 16, 82, 12, SYN_M, 10},
    {"SQUARE", 9, {3, 62, 42, 48, 6, 6, 55, 8}, SNAP, FX_N_NONE, 0, 0, 0, 18, 14, SYN_M, 3},
    {"HAZE", 9, {0, 40, 10, 8, 55, 0, 100, 18}, {40, 55, 70}, FX_N_VINYL, 18, 22, 20, 80, 12, SYN_M, 8},
    {"BELL", 9, {4, 84, 6, 40, 12, 0, 45, 8}, {2, 18, 42}, FX_N_SPRING, 24, 45, 38, 52, 30, SYN_M, 15},
    {"PLUCK", 9, {0, 76, 42, 70, 10, 0, 30, 4}, {2, 14, 14}, FX_N_NONE, 0, 0, 0, 22, 40, SYN_M, 14},
    {"BREATH", 9, {6, 52, 22, 8, 0, 0, 100, 78}, {68, 55, 66}, FX_N_NONE, 0, 0, 0, 68, 6, SYN_M, 2},
    {"CRUSH", 9, {1, 64, 18, 8, 8, 82, 35, 16}, SNAP, FX_N_LOFI, 80, 72, 55, 12, 8, SYN_M, 17},
    {"ACID", 9, {0, 32, 92, 86, 4, 0, 65, 12}, {6, 26, 24}, FX_N_NONE, 0, 0, 0, 10, 22, SYN_M, 5},
    /* PLUCK */
    {"NYLON", 10, {2, 58, 38, 32, 38, 0}, SNAP, FX_N_NONE, 0, 0, 0, 36, 10, PLK_M, -4},
    {"HARP", 10, {1, 26, 90, 22, 58, 0}, LONG, FX_N_NONE, 0, 0, 0, 82, 20, PLK_M, -16},
    {"LUTE", 10, {0, 52, 42, 62, 68, 0}, SNAP, FX_N_TAPE, 14, 18, 18, 28, 6, PLK_M, -8},
    {"KOTO", 10, {4, 82, 28, 14, 18, 0}, {2, 20, 26}, FX_N_NONE, 0, 0, 0, 40, 16, PLK_M, 6},
    {"DULCIMER", 10, {5, 36, 74, 55, 70, 0}, TONE, FX_N_SPRING, 18, 36, 32, 58, 14, PLK_M, -14},
    {"STEEL", 10, {3, 90, 24, 12, 22, 0}, SNAP, FX_N_NONE, 0, 0, 0, 18, 8, PLK_M, 3},
    {"NIGHT HARP", 10, {1, 16, 98, 40, 74, 0}, {18, 64, 86}, FX_N_NONE, 0, 0, 0, 96, 26, PLK_M, -24},
    {"WORN", 10, {2, 28, 36, 58, 42, 62}, TONE, FX_N_LOFI, 55, 45, 40, 24, 6, PLK_M, -6},
    {"ECHO KOTO", 10, {4, 64, 48, 28, 30, 0}, {4, 32, 55}, FX_N_NONE, 0, 0, 0, 42, 62, PLK_M, -6},
    {"CAMPFIRE", 10, {0, 32, 58, 68, 80, 12}, TONE, FX_N_VINYL, 32, 42, 30, 36, 4, PLK_M, -14},
    {"ANGEL", 10, {1, 48, 92, 16, 24, 0}, LONG, FX_N_CHORUS, 26, 18, 42, 90, 32, PLK_M, -11},
    {"MUTE", 10, {2, 8, 14, 90, 12, 0}, {2, 10, 10}, FX_N_FILTER, 76, 22, 4, 10, 2, PLK_M, 8},
    /* LEAD */
    {"SOFT LEAD", 11, {0, 50, 10, 16, 26, 20, 6, 12}, TONE, FX_N_TAPE, 16, 20, 18, 38, 14, LD_M, 12},
    {"SQUARE", 11, {1, 64, 10, 8, 6, 16, 0, 0}, SNAP, FX_N_LOFI, 32, 28, 22, 18, 8, LD_M, 5},
    {"DUSTY", 11, {0, 34, 20, 28, 8, 26, 16, 28}, TONE, FX_N_VINYL, 45, 50, 38, 26, 10, LD_M, 17},
    {"SUPER", 11, {3, 58, 8, 10, 88, 16, 4, 0}, {16, 46, 48}, FX_N_CHORUS, 48, 22, 58, 36, 16, LD_M, 13},
    {"FM BELL", 11, {5, 82, 0, 70, 42, 6, 0, 0}, {2, 22, 46}, FX_N_SPRING, 20, 42, 32, 52, 26, LD_M, 6},
    {"FLUTE", 11, {4, 66, 2, 0, 0, 58, 10, 0}, {28, 50, 55}, FX_N_NONE, 0, 0, 0, 52, 16, LD_M, 8},
    {"RAIN", 11, {2, 42, 16, 20, 18, 24, 8, 0}, {10, 38, 50}, FX_N_TAPE, 14, 18, 18, 52, 30, LD_M, 8},
    {"GLIDE", 11, {0, 52, 18, 22, 14, 18, 84, 16}, TONE, FX_N_TAPE, 12, 16, 18, 32, 18, LD_M, 12},
    {"REED", 11, {4, 36, 14, 6, 0, 32, 14, 0}, {22, 46, 52}, FX_N_FILTER, 76, 36, 8, 42, 10, LD_M, 12},
    {"ACID", 11, {2, 26, 94, 82, 0, 8, 0, 0}, SNAP, FX_N_NONE, 0, 0, 0, 8, 20, LD_M, 2},
    {"WIDE", 11, {3, 34, 6, 8, 100, 10, 2, 34}, LONG, FX_N_CHORUS, 36, 26, 52, 48, 14, LD_M, 20},
    {"SNOW", 11, {5, 58, 4, 36, 52, 10, 0, 0}, {6, 36, 72}, FX_N_NONE, 0, 0, 0, 78, 32, LD_M, 5},
};

/* the blocks 1..3 (8-16 BIT, SYNTHWAVE, CINEMATIC) of each engine, one after the other, as many as PCOUNT says */
static const dk_preset_t PRESETS2[] = {
    /* FELT, block 1 */
    {"8-BIT PIANO", 0, {60, 10, 35, 60, 40}, E50, FX_N_LOFI, 95, 85, 55, 15, 0, FELT_M, 5},
    {"GAME KEYS", 0, {55, 20, 30, 55, 45}, E50, FX_N_LOFI, 85, 70, 40, 20, 10, FELT_M, 5},
    {"PIXEL", 0, {70, 5, 25, 70, 35}, E50, FX_N_LOFI, 100, 90, 65, 10, 0, FELT_M, 6},
    {"16-BIT KEYS", 0, {50, 25, 45, 50, 50}, E50, FX_N_LOFI, 70, 55, 30, 25, 15, FELT_M, 4},
    /* FELT, block 2 */
    {"NEON PIANO", 0, {68, 8, 45, 70, 70}, E50, FX_N_CHORUS, 35, 30, 50, 55, 40, FELT_M, 3},
    {"OUTRUN KEYS", 0, {72, 5, 35, 75, 75}, E50, FX_N_CHORUS, 40, 35, 55, 45, 45, FELT_M, 4},
    {"DRIVE PIANO", 0, {60, 12, 55, 55, 70}, E50, FX_N_TAPE, 30, 30, 35, 60, 35, FELT_M, 1},
    {"RETRO GRAND", 0, {55, 15, 60, 45, 65}, E50, FX_N_CHORUS, 25, 25, 40, 65, 30, FELT_M, 2},
    /* FELT, block 3 */
    {"TAVERN", 0, {48, 30, 40, 60, 60}, E50, FX_N_VINYL, 20, 30, 30, 40, 5, FELT_M, 3},
    {"HEARTH", 0, {32, 45, 70, 35, 60}, E50, FX_N_TAPE, 20, 25, 25, 85, 25, FELT_M, -3},
    {"SOLITUDE", 0, {25, 40, 85, 25, 70}, E50, FX_N_NONE, 0, 0, 0, 100, 30, FELT_M, -4},
    {"SNOWHOLD", 0, {38, 25, 75, 30, 75}, E50, FX_N_NONE, 0, 0, 0, 95, 35, FELT_M, -3},
    {"ELDER KEYS", 0, {45, 35, 65, 40, 70}, E50, FX_N_TAPE, 20, 25, 25, 90, 30, FELT_M, -2},
    {"LAMENT", 0, {30, 60, 80, 20, 65}, E50, FX_N_TAPE, 25, 30, 30, 100, 20, FELT_M, -3},
    /* TINE, block 1 */
    {"FM CHIP", 1, {80, 10, 10, 70, 30}, E50, FX_N_LOFI, 90, 80, 50, 15, 0, TINE_M, 2},
    {"COIN", 1, {90, 0, 0, 50, 20}, E50, FX_N_LOFI, 80, 70, 45, 10, 15, TINE_M, 2},
    {"ARCADE EP", 1, {65, 30, 20, 60, 35}, E50, FX_N_LOFI, 85, 60, 40, 20, 10, TINE_M, 2},
    {"SNES EP", 1, {55, 40, 25, 45, 45}, E50, FX_N_LOFI, 65, 45, 30, 30, 15, TINE_M, 1},
    /* TINE, block 2 */
    {"DX KEYS", 1, {75, 15, 10, 60, 45}, E50, FX_N_CHORUS, 45, 30, 50, 40, 25, TINE_M, 1},
    {"NEON EP", 1, {60, 35, 20, 55, 50}, E50, FX_N_CHORUS, 50, 35, 55, 50, 35, TINE_M, 2},
    {"ELECTRIC DREAM", 1, {50, 45, 25, 40, 60}, E50, FX_N_CHORUS, 40, 25, 45, 65, 40, TINE_M, 0},
    {"SUNSET EP", 1, {45, 30, 20, 35, 65}, E50, FX_N_TAPE, 30, 30, 35, 60, 35, TINE_M, -2},
    {"TURBO", 1, {85, 10, 5, 75, 35}, E50, FX_N_CHORUS, 35, 35, 45, 35, 30, TINE_M, 2},
    {"SPARKLE", 1, {90, 20, 15, 65, 45}, E50, FX_N_CHORUS, 40, 30, 50, 55, 40, TINE_M, 1},
    {"RETRO TINE", 1, {65, 50, 30, 50, 50}, E50, FX_N_TREM, 35, 35, 60, 45, 30, TINE_M, 2},
    {"MIAMI", 1, {55, 25, 15, 45, 55}, E50, FX_N_CHORUS, 45, 25, 55, 50, 40, TINE_M, 0},
    /* TINE, block 3 */
    {"CRYSTAL", 1, {90, 10, 10, 20, 80}, E50, FX_N_NONE, 0, 0, 0, 100, 40, TINE_M, -6},
    {"DISTANT BELLS", 1, {85, 5, 10, 15, 90}, E50, FX_N_NONE, 0, 0, 0, 100, 45, TINE_M, -6},
    {"MOONLIT", 1, {60, 20, 20, 25, 85}, E50, FX_N_TAPE, 20, 25, 25, 95, 40, TINE_M, -5},
    {"ARCANE", 1, {95, 30, 25, 30, 75}, E50, FX_N_CHORUS, 25, 20, 45, 100, 50, TINE_M, -5},
    /* AIRPAD, block 2 */
    {"SUPER PAD", 2, {55, 40, 25, 5, 50}, E50, FX_N_CHORUS, 40, 30, 50, 65, 15, PAD_M, 0},
    {"NIGHT DRIVE PAD", 2, {45, 45, 20, 5, 60}, E50, FX_N_CHORUS, 35, 25, 45, 70, 25, PAD_M, -1},
    {"NEON STRINGS", 2, {60, 35, 30, 4, 55}, E50, FX_N_CHORUS, 45, 35, 55, 60, 20, PAD_M, -6},
    {"SUNSET", 2, {70, 55, 30, 5, 40}, E50, FX_N_TAPE, 25, 25, 30, 75, 25, PAD_M, -4},
    {"ORGAN GLOW", 2, {50, 30, 15, 8, 50}, E50, FX_N_CHORUS, 50, 30, 50, 55, 15, PAD_M, 0},
    {"RETRO WARM", 2, {55, 40, 20, 2, 60}, E50, FX_N_CHORUS, 40, 25, 45, 60, 20, PAD_M, 1},
    {"DARK WAVE", 2, {30, 50, 25, 5, 70}, E50, FX_N_NONE, 0, 0, 0, 85, 40, PAD_M, -2},
    {"ARP BED", 2, {65, 25, 35, 3, 45}, E50, FX_N_CHORUS, 35, 30, 50, 50, 35, PAD_M, 4},
    /* AIRPAD, block 3 */
    {"CHOIR AH", 2, {10, 60, 35, 6, 50}, E50, FX_N_NONE, 0, 0, 0, 90, 10, PAD_M, -12},
    {"CHOIR OH", 2, {40, 65, 30, 6, 45}, E50, FX_N_NONE, 0, 0, 0, 90, 10, PAD_M, -10},
    {"CHOIR EE", 2, {85, 60, 30, 6, 55}, E50, FX_N_NONE, 0, 0, 0, 90, 10, PAD_M, -17},
    {"SOVNGARDE", 2, {20, 85, 40, 6, 65}, E50, FX_N_NONE, 0, 0, 0, 100, 15, PAD_M, -5},
    {"DRAGON", 2, {35, 55, 20, 7, 60}, E50, FX_N_NONE, 0, 0, 0, 95, 10, PAD_M, 0},
    {"DEEP DRONE", 2, {15, 60, 10, 7, 40}, E50, FX_N_NONE, 0, 0, 0, 100, 10, PAD_M, 4},
    {"WINTERHOLD", 2, {35, 60, 20, 4, 60}, E50, FX_N_NONE, 0, 0, 0, 95, 15, PAD_M, -8},
    {"FALKREATH", 2, {40, 55, 15, 5, 40}, E50, FX_N_NONE, 0, 0, 0, 90, 10, PAD_M, -4},
    {"IVORY CHAPEL", 2, {35, 40, 10, 8, 35}, E50, FX_N_NONE, 0, 0, 0, 100, 10, PAD_M, -4},
    {"DAWNSTAR", 2, {45, 70, 35, 2, 55}, E50, FX_N_TAPE, 15, 20, 20, 95, 15, PAD_M, 0},
    {"BLACKREACH", 2, {20, 75, 25, 0, 60}, E50, FX_N_NONE, 0, 0, 0, 100, 20, PAD_M, 2},
    {"HIGH HROTHGAR", 2, {55, 70, 40, 6, 60}, E50, FX_N_NONE, 0, 0, 0, 100, 15, PAD_M, -8},
    /* GLASS, block 1 */
    {"CHIP BELL", 3, {0, 30, 50, 0}, E50, FX_N_LOFI, 60, 55, 65, 10, 10, GLASS_M, 5},
    {"8-BIT MALLET", 3, {2, 35, 50, 0}, E50, FX_N_LOFI, 90, 85, 60, 10, 0, GLASS_M, 12},
    {"GAME BOX", 3, {3, 45, 45, 0}, E50, FX_N_LOFI, 80, 60, 40, 15, 15, GLASS_M, 5},
    {"PIXEL VIBES", 3, {4, 40, 50, 0}, E50, FX_N_LOFI, 85, 70, 45, 15, 10, GLASS_M, 7},
    /* GLASS, block 2 */
    {"NEON BELLS", 3, {0, 45, 55, 0}, E50, FX_N_CHORUS, 35, 30, 50, 55, 40, GLASS_M, 1},
    {"SYNTH VIBES", 3, {4, 55, 60, 0}, E50, FX_N_CHORUS, 40, 30, 55, 50, 35, GLASS_M, 5},
    {"NIGHT CELESTA", 3, {1, 50, 70, 1}, E50, FX_N_TAPE, 25, 25, 30, 65, 45, GLASS_M, -2},
    {"DREAM BOX", 3, {3, 60, 55, 1}, E50, FX_N_CHORUS, 30, 25, 45, 70, 45, GLASS_M, -1},
    {"RETRO MALLET", 3, {2, 45, 65, 0}, E50, FX_N_CHORUS, 35, 30, 45, 45, 35, GLASS_M, 10},
    {"CITY LIGHTS", 3, {0, 60, 40, 0}, E50, FX_N_CHORUS, 30, 30, 50, 60, 50, GLASS_M, 0},
    /* GLASS, block 3 */
    {"CHAPEL BELL", 3, {0, 85, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 100, 30, GLASS_M, -5},
    {"TEMPLE", 3, {2, 80, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 95, 35, GLASS_M, 1},
    {"FROST BELLS", 3, {1, 75, 70, 2}, E50, FX_N_NONE, 0, 0, 0, 100, 40, GLASS_M, -3},
    {"SHRINE", 3, {3, 85, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 100, 35, GLASS_M, -3},
    {"TOLL", 3, {0, 95, 25, 0}, E50, FX_N_NONE, 0, 0, 0, 100, 20, GLASS_M, -6},
    {"DAWN CHIME", 3, {4, 80, 60, 2}, E50, FX_N_NONE, 0, 0, 0, 100, 40, GLASS_M, 0},
    /* SOFTBASS, block 1 */
    {"NES BASS", 4, {50, 0, 0, 5, 50}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 5},
    {"SQUARE", 4, {70, 0, 0, 3, 60}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 11},
    {"PULSE 25", 4, {30, 0, 0, 3, 40}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 13},
    {"GB BASS", 4, {0, 0, 0, 3, 30}, E50, FX_N_LOFI, 50, 60, 40, 5, 0, BASS_M, 17},
    {"ARCADE", 4, {50, 0, 25, 3, 70}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 11},
    {"TRI SLIDE", 4, {50, 0, 80, 5, 18}, E50, FX_N_LOFI, 45, 40, 40, 5, 0, BASS_M, 5},
    {"BOSS SAW", 4, {40, 45, 0, 4, 50}, E50, FX_N_LOFI, 60, 45, 45, 5, 0, BASS_M, 7},
    {"CHIP SUB", 4, {40, 0, 0, 0, 50}, E50, FX_N_LOFI, 70, 65, 45, 5, 0, BASS_M, 0},
    /* SOFTBASS, block 2 */
    {"NIGHT DRIVE", 4, {50, 50, 0, 4, 60}, E50, FX_N_NONE, 0, 0, 0, 8, 0, BASS_M, 7},
    {"OUTRUN", 4, {60, 60, 0, 4, 50}, E50, FX_N_NONE, 0, 0, 0, 8, 0, BASS_M, 7},
    {"ARP BASS", 4, {45, 70, 0, 4, 25}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 8},
    {"ANALOG", 4, {55, 50, 10, 1, 50}, E50, FX_N_NONE, 0, 0, 0, 8, 0, BASS_M, 6},
    {"RESO BASS", 4, {70, 70, 0, 1, 40}, E50, FX_N_NONE, 0, 0, 0, 8, 0, BASS_M, 6},
    {"DETUNED", 4, {40, 30, 0, 4, 80}, E50, FX_N_CHORUS, 25, 25, 40, 10, 0, BASS_M, 7},
    {"GLIDER", 4, {50, 40, 60, 4, 70}, E50, FX_N_NONE, 0, 0, 0, 8, 0, BASS_M, 7},
    {"PULSE DRIVE", 4, {15, 0, 0, 3, 35}, E50, FX_N_CHORUS, 25, 30, 40, 8, 0, BASS_M, 15},
    {"SUB WAVE", 4, {25, 10, 0, 1, 70}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 5},
    {"ACID", 4, {80, 90, 40, 1, 30}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 6},
    {"MIDNIGHT", 4, {35, 40, 0, 4, 90}, E50, FX_N_NONE, 0, 0, 0, 12, 0, BASS_M, 6},
    {"LASER", 4, {75, 80, 70, 1, 35}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 6},
    /* SOFTBASS, block 3 */
    {"SUB DRONE", 4, {40, 0, 0, 6, 80}, E50, FX_N_NONE, 0, 0, 0, 25, 0, BASS_M, 7},
    {"RUMBLE", 4, {10, 0, 0, 1, 100}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 4},
    {"WAR HORN LOW", 4, {25, 10, 0, 4, 90}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 5},
    {"BOWED BASS", 4, {35, 15, 0, 2, 85}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 0},
    {"ANCIENT", 4, {5, 10, 0, 4, 90}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 5},
    {"DRAGON BREATH", 4, {60, 0, 20, 6, 90}, E50, FX_N_NONE, 0, 0, 0, 35, 0, BASS_M, 5},
    {"CELLO LOW", 4, {35, 20, 0, 4, 100}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 5},
    {"BRONZE", 4, {30, 20, 0, 1, 80}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 5},
    /* DUSTKIT, block 1 */
    {"NES KIT", 5, {50, 50, 45, 3}, E50, FX_N_NONE, 0, 0, 0, 8, 0, KIT_M, 2},
    {"GAME BOY", 5, {60, 40, 60, 3}, E50, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 2},
    {"ARCADE", 5, {55, 45, 30, 3}, E50, FX_N_NONE, 0, 0, 0, 10, 0, KIT_M, 2},
    {"C64", 5, {40, 60, 55, 3}, E50, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 0},
    {"16-BIT", 5, {50, 40, 20, 4}, E50, FX_N_NONE, 0, 0, 0, 10, 0, KIT_M, -1},
    {"CRUNCH", 5, {50, 55, 80, 3}, E50, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 0},
    {"SNES KIT", 5, {50, 50, 70, 1}, E50, FX_N_LOFI, 50, 55, 40, 12, 0, KIT_M, 4},
    {"PIXEL", 5, {70, 35, 40, 3}, E50, FX_N_NONE, 0, 0, 0, 8, 0, KIT_M, 3},
    {"606 TIGHT", 5, {50, 45, 15, 8}, E50, FX_N_NONE, 0, 0, 0, 8, 0, KIT_M, 3},
    {"606 FUNK", 5, {58, 40, 30, 8}, E50, FX_N_NONE, 0, 0, 0, 10, 0, KIT_M, 4},
    {"CR-78 SOFT", 5, {50, 55, 20, 9}, E50, FX_N_NONE, 0, 0, 0, 12, 0, KIT_M, 5},
    {"CR-78 DISCO", 5, {55, 50, 35, 9}, E50, FX_N_TAPE, 20, 25, 30, 15, 0, KIT_M, 6},
    /* DUSTKIT, block 2 */
    {"LINN", 5, {50, 50, 30, 4}, E50, FX_N_NONE, 0, 0, 0, 20, 0, KIT_M, -2},
    {"OUTRUN", 5, {55, 55, 20, 4}, E50, FX_N_NONE, 0, 0, 0, 25, 0, KIT_M, -3},
    {"DRIVE", 5, {45, 60, 15, 4}, E50, FX_N_NONE, 0, 0, 0, 20, 0, KIT_M, -3},
    {"NEON", 5, {60, 45, 25, 4}, E50, FX_N_CHORUS, 25, 30, 40, 25, 0, KIT_M, 0},
    {"SYNTH POP", 5, {50, 40, 35, 4}, E50, FX_N_NONE, 0, 0, 0, 15, 0, KIT_M, -1},
    {"GATED", 5, {45, 70, 20, 4}, E50, FX_N_NONE, 0, 0, 0, 35, 0, KIT_M, -4},
    {"ELECTRO", 5, {55, 50, 10, 4}, E50, FX_N_NONE, 0, 0, 0, 15, 0, KIT_M, -2},
    {"SUNSET", 5, {40, 65, 30, 4}, E50, FX_N_TAPE, 25, 25, 30, 30, 0, KIT_M, -3},
    {"808 CLASSIC", 5, {50, 50, 8, 6}, E50, FX_N_NONE, 0, 0, 0, 12, 0, KIT_M, -3},
    {"808 LONG", 5, {42, 70, 10, 6}, E50, FX_N_NONE, 0, 0, 0, 15, 0, KIT_M, -5},
    {"909 TECHNO", 5, {50, 50, 8, 7}, E50, FX_N_NONE, 0, 0, 0, 10, 0, KIT_M, -3},
    {"909 HOUSE", 5, {52, 55, 15, 7}, E50, FX_N_CHORUS, 15, 25, 40, 20, 0, KIT_M, -2},
    /* DUSTKIT, block 3 */
    {"WAR DRUMS", 5, {50, 60, 25, 5}, E50, FX_N_NONE, 0, 0, 0, 50, 0, KIT_M, -3},
    {"TAIKO", 5, {40, 70, 20, 5}, E50, FX_N_NONE, 0, 0, 0, 55, 0, KIT_M, -3},
    {"DEEP SKINS", 5, {35, 80, 30, 5}, E50, FX_N_NONE, 0, 0, 0, 60, 0, KIT_M, -5},
    {"TRIBAL", 5, {55, 50, 35, 5}, E50, FX_N_NONE, 0, 0, 0, 45, 0, KIT_M, -3},
    {"BATTLE", 5, {50, 55, 15, 5}, E50, FX_N_NONE, 0, 0, 0, 50, 0, KIT_M, -3},
    {"HEARTBEAT", 5, {30, 45, 40, 5}, E50, FX_N_NONE, 0, 0, 0, 40, 0, KIT_M, 0},
    {"MARCH", 5, {55, 40, 25, 5}, E50, FX_N_NONE, 0, 0, 0, 40, 0, KIT_M, -3},
    {"THUNDER", 5, {25, 90, 35, 5}, E50, FX_N_NONE, 0, 0, 0, 65, 0, KIT_M, -3},
    {"808 SUB BOOM", 5, {30, 85, 10, 6}, E50, FX_N_NONE, 0, 0, 0, 35, 0, KIT_M, -5},
    {"808 DIRTY", 5, {45, 60, 60, 6}, E50, FX_N_TAPE, 35, 35, 45, 15, 0, KIT_M, -2},
    {"909 HARD", 5, {45, 45, 25, 7}, E50, FX_N_NONE, 0, 0, 0, 25, 0, KIT_M, -1},
    {"707 PUNCH", 5, {35, 25, 20, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 0, KIT_M, 3},
    /* WORLD, block 3 */
    {"BLIZZARD", 6, {1, 90, 40}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, -10},
    {"TAVERN FIRE", 6, {2, 70, 40}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 11},
    {"DRAGON STORM", 6, {8, 90, 35}, E50, FX_N_NONE, 0, 0, 0, 25, 0, WORLD_M, 10},
    {"MOUNTAIN", 6, {1, 65, 30}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, -5},
    /* WINDS, block 2 */
    {"SYNTH BRASS", 8, {55, 5, 15, 8, 55}, E50, FX_N_CHORUS, 35, 30, 45, 40, 15, WIND_M, 7},
    {"BRASS STAB", 8, {65, 5, 10, 8, 25}, E50, FX_N_CHORUS, 30, 30, 40, 30, 15, WIND_M, 6},
    {"LEAD HORN", 8, {45, 10, 40, 8, 50}, E50, FX_N_CHORUS, 25, 25, 35, 45, 25, WIND_M, 7},
    {"NEON REED", 8, {45, 15, 40, 3, 40}, E50, FX_N_CHORUS, 30, 25, 35, 40, 25, WIND_M, 2},
    {"OBERHEIM", 8, {70, 0, 20, 8, 60}, E50, FX_N_CHORUS, 40, 30, 50, 40, 20, WIND_M, 7},
    {"NEON FLUTE", 8, {60, 20, 50, 0, 45}, E50, FX_N_CHORUS, 30, 25, 40, 50, 35, WIND_M, 2},
    /* WINDS, block 3 */
    {"NORDIC HORN", 8, {30, 10, 20, 6, 70}, E50, FX_N_NONE, 0, 0, 0, 85, 20, WIND_M, 2},
    {"WAR HORN", 8, {25, 5, 15, 6, 85}, E50, FX_N_NONE, 0, 0, 0, 95, 25, WIND_M, 4},
    {"BATTLE BRASS", 8, {50, 10, 20, 7, 50}, E50, FX_N_NONE, 0, 0, 0, 80, 20, WIND_M, -4},
    {"CALL", 8, {55, 10, 25, 4, 45}, E50, FX_N_NONE, 0, 0, 0, 80, 25, WIND_M, -2},
    {"DUDUK", 8, {45, 55, 60, 9, 55}, E50, FX_N_NONE, 0, 0, 0, 80, 25, WIND_M, -4},
    {"BARD FLUTE", 8, {30, 70, 70, 0, 75}, E50, FX_N_NONE, 0, 0, 0, 70, 25, WIND_M, 0},
    {"PIPES", 8, {30, 40, 50, 3, 50}, E50, FX_N_NONE, 0, 0, 0, 75, 25, WIND_M, -2},
    {"DISTANT HORN", 8, {20, 20, 20, 6, 95}, E50, FX_N_NONE, 0, 0, 0, 100, 45, WIND_M, 3},
    {"LAMENT", 8, {30, 70, 80, 9, 70}, E50, FX_N_NONE, 0, 0, 0, 95, 30, WIND_M, -4},
    {"PAN WOODS", 8, {45, 85, 30, 0, 50}, E50, FX_N_NONE, 0, 0, 0, 85, 25, WIND_M, -1},
    {"CARNYX", 8, {35, 10, 20, 8, 60}, E50, FX_N_NONE, 0, 0, 0, 85, 20, WIND_M, 3},
    {"HUNTER", 8, {45, 20, 30, 5, 50}, E50, FX_N_NONE, 0, 0, 0, 80, 20, WIND_M, -2},
    /* SYNTH, block 1 */
    {"PULSE LEAD", 9, {1, 80, 10, 0, 10, 25, 100, 25}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, 6},
    {"SQUARE LEAD", 9, {3, 75, 10, 0, 0, 30, 100, 30}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, -3},
    {"TRIANGLE", 9, {4, 90, 0, 0, 0, 20, 100, 15}, E50, FX_N_NONE, 0, 0, 0, 10, 15, SYN_M, 6},
    {"12.5% PULSE", 9, {2, 85, 10, 0, 0, 24, 100, 20}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, 13},
    {"CHIP ARP", 9, {1, 70, 10, 0, 10, 35, 30, 10}, E50, FX_N_NONE, 0, 0, 0, 10, 30, SYN_M, 16},
    {"NOISE HAT", 9, {6, 70, 20, 40, 0, 50, 15, 0}, E50, FX_N_NONE, 0, 0, 0, 8, 0, SYN_M, 15},
    {"C64 SAW", 9, {0, 60, 40, 25, 15, 50, 100, 40}, E50, FX_N_NONE, 0, 0, 0, 10, 20, SYN_M, 6},
    {"GAME OVER", 9, {3, 50, 20, 45, 10, 45, 70, 60}, E50, FX_N_NONE, 0, 0, 0, 15, 25, SYN_M, 2},
    {"POWER-UP", 9, {1, 85, 15, 60, 20, 25, 60, 20}, E50, FX_N_NONE, 0, 0, 0, 12, 30, SYN_M, 11},
    {"BOSS ORGAN", 9, {3, 55, 30, 0, 40, 40, 100, 70}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, 0},
    {"DUTY SWEEP", 9, {1, 65, 20, 20, 30, 35, 100, 80}, E50, FX_N_NONE, 0, 0, 0, 12, 25, SYN_M, 6},
    {"SNES STRINGS", 9, {5, 40, 10, 10, 50, 35, 100, 30}, E50, FX_N_CHORUS, 25, 25, 35, 40, 20, SYN_M, 13},
    /* SYNTH, block 2 */
    {"NIGHT DRIVE", 9, {0, 52, 25, 15, 35, 0, 100, 35}, E50, FX_N_CHORUS, 35, 30, 50, 55, 40, SYN_M, 6},
    {"OUTRUN LEAD", 9, {5, 70, 15, 10, 45, 0, 100, 40}, E50, FX_N_CHORUS, 40, 35, 55, 45, 40, SYN_M, 9},
    {"SUNSET SAW", 9, {0, 45, 20, 25, 30, 0, 100, 30}, E50, FX_N_CHORUS, 35, 25, 45, 60, 35, SYN_M, 6},
    {"NEON PLUCK", 9, {0, 60, 35, 45, 25, 0, 25, 15}, E50, FX_N_CHORUS, 25, 30, 40, 40, 45, SYN_M, 14},
    {"MIAMI BRASS", 9, {5, 55, 20, 35, 50, 0, 70, 25}, E50, FX_N_CHORUS, 40, 30, 50, 40, 30, SYN_M, 13},
    {"RETRO SQUARE", 9, {3, 50, 25, 15, 10, 0, 80, 40}, E50, FX_N_CHORUS, 30, 25, 40, 40, 35, SYN_M, 0},
    {"LASER LEAD", 9, {0, 75, 35, 40, 25, 0, 100, 25}, E50, FX_N_NONE, 0, 0, 0, 35, 45, SYN_M, 5},
    {"ARP SYNTH", 9, {0, 50, 40, 30, 20, 0, 20, 10}, E50, FX_N_CHORUS, 25, 30, 40, 35, 50, SYN_M, 13},
    {"DREAM PAD", 9, {5, 35, 15, 10, 60, 0, 100, 35}, E50, FX_N_CHORUS, 45, 30, 55, 80, 30, SYN_M, 11},
    {"DARK DRIVE", 9, {0, 32, 45, 30, 35, 0, 100, 45}, E50, FX_N_NONE, 0, 0, 0, 50, 35, SYN_M, 5},
    {"VAPOR", 9, {4, 45, 10, 0, 30, 15, 100, 60}, E50, FX_N_CHORUS, 50, 40, 60, 70, 40, SYN_M, 6},
    {"MIDNIGHT", 9, {5, 42, 25, 20, 55, 0, 100, 50}, E50, FX_N_TAPE, 25, 25, 30, 75, 40, SYN_M, 6},
    /* SYNTH, block 3 */
    {"DRONE SAW", 9, {0, 22, 30, 10, 50, 0, 100, 35}, E50, FX_N_NONE, 0, 0, 0, 90, 20, SYN_M, 14},
    {"DEEP SUPER", 9, {5, 25, 20, 0, 60, 0, 100, 30}, E50, FX_N_NONE, 0, 0, 0, 95, 20, SYN_M, 17},
    {"RUMBLE", 9, {0, 15, 45, 20, 40, 0, 100, 60}, E50, FX_N_NONE, 0, 0, 0, 90, 10, SYN_M, 19},
    {"ANCIENT", 9, {3, 30, 35, 0, 25, 0, 100, 50}, E50, FX_N_NONE, 0, 0, 0, 100, 25, SYN_M, -4},
    {"HORN SWELL", 9, {5, 45, 25, 45, 55, 0, 100, 35}, E50, FX_N_NONE, 0, 0, 0, 90, 25, SYN_M, 5},
    {"SINE SUB", 9, {7, 18, 0, 0, 10, 0, 100, 20}, E50, FX_N_NONE, 0, 0, 0, 70, 0, SYN_M, 19},
    {"DARK PAD", 9, {4, 30, 25, 0, 45, 0, 100, 40}, E50, FX_N_NONE, 0, 0, 0, 100, 30, SYN_M, 4},
    {"WIND SYNTH", 9, {6, 55, 60, 40, 0, 0, 100, 90}, E50, FX_N_NONE, 0, 0, 0, 100, 30, SYN_M, 2},
    {"ELDER", 9, {5, 38, 20, 20, 65, 0, 100, 25}, E50, FX_N_NONE, 0, 0, 0, 95, 20, SYN_M, 6},
    {"TENSION", 9, {0, 28, 50, 30, 70, 0, 100, 80}, E50, FX_N_NONE, 0, 0, 0, 85, 25, SYN_M, 5},
    {"VOID", 9, {4, 20, 35, 0, 40, 0, 100, 55}, E50, FX_N_NONE, 0, 0, 0, 100, 35, SYN_M, 13},
    {"SOLEMN", 9, {5, 33, 15, 5, 50, 0, 100, 25}, E50, FX_N_NONE, 0, 0, 0, 100, 25, SYN_M, 10},
    /* PLUCK, block 1 */
    {"CHIP HARP", 10, {1, 70, 30, 35, 30, 70}, E50, FX_N_NONE, 0, 0, 0, 15, 20, PLK_M, -5},
    {"8-BIT LUTE", 10, {0, 65, 25, 40, 30, 75}, E50, FX_N_NONE, 0, 0, 0, 10, 15, PLK_M, 0},
    {"PIXEL KOTO", 10, {4, 70, 30, 40, 25, 80}, E50, FX_N_NONE, 0, 0, 0, 10, 20, PLK_M, -1},
    {"GB NYLON", 10, {2, 60, 30, 45, 25, 65}, E50, FX_N_NONE, 0, 0, 0, 10, 15, PLK_M, -3},
    {"PLUCK BIT", 10, {3, 80, 20, 50, 20, 85}, E50, FX_N_NONE, 0, 0, 0, 8, 25, PLK_M, -2},
    {"ARCADE HARP", 10, {5, 75, 30, 45, 25, 60}, E50, FX_N_NONE, 0, 0, 0, 12, 20, PLK_M, -5},
    /* PLUCK, block 2 */
    {"NEON HARP", 10, {1, 70, 50, 35, 30, 0}, E50, FX_N_CHORUS, 35, 30, 50, 55, 40, PLK_M, -10},
    {"SYNTH LUTE", 10, {0, 75, 40, 45, 30, 0}, E50, FX_N_CHORUS, 30, 30, 45, 45, 40, PLK_M, -5},
    {"DX PLUCK", 10, {3, 85, 35, 40, 25, 0}, E50, FX_N_CHORUS, 35, 30, 45, 40, 35, PLK_M, -5},
    {"KOTO WAVE", 10, {4, 65, 55, 35, 30, 0}, E50, FX_N_CHORUS, 30, 25, 40, 60, 45, PLK_M, -7},
    {"GLASS HARP", 10, {5, 75, 60, 45, 30, 0}, E50, FX_N_CHORUS, 35, 30, 50, 65, 45, PLK_M, -10},
    {"MIAMI NYLON", 10, {2, 70, 45, 40, 35, 0}, E50, FX_N_CHORUS, 35, 30, 45, 50, 40, PLK_M, -7},
    /* PLUCK, block 3 */
    {"LUTE", 10, {0, 45, 55, 40, 55, 0}, E50, FX_N_NONE, 0, 0, 0, 65, 15, PLK_M, -12},
    {"BARD'S LUTE", 10, {0, 55, 45, 45, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 55, 10, PLK_M, -9},
    {"HARP", 10, {1, 40, 70, 35, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 85, 25, PLK_M, -16},
    {"ANGEL HARP", 10, {1, 55, 85, 30, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 100, 35, PLK_M, -17},
    {"NORDIC LYRE", 10, {3, 45, 60, 40, 55, 0}, E50, FX_N_NONE, 0, 0, 0, 75, 20, PLK_M, -15},
    {"DULCIMER", 10, {5, 45, 70, 45, 55, 0}, E50, FX_N_NONE, 0, 0, 0, 85, 25, PLK_M, -17},
    {"TAVERN STEEL", 10, {3, 60, 45, 45, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 50, 10, PLK_M, -12},
    {"KOTO", 10, {4, 50, 60, 35, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 80, 25, PLK_M, -13},
    {"HARP GLISS", 10, {1, 65, 60, 30, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 90, 30, PLK_M, -16},
    {"FOLK NYLON", 10, {2, 45, 55, 45, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 60, 10, PLK_M, -13},
    {"DARK LUTE", 10, {0, 30, 60, 40, 60, 0}, E50, FX_N_NONE, 0, 0, 0, 80, 20, PLK_M, -15},
    {"MOURNING", 10, {1, 55, 90, 70, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 100, 40, PLK_M, -20},
    /* LEAD, block 1 */
    {"NES LEAD", 11, {1, 100, 0, 0, 0, 0, 0, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 15, LD_M, 2},
    {"PULSE 25 LEAD", 11, {2, 100, 0, 0, 0, 20, 0, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 15, LD_M, 8},
    {"CHIP VIBRATO", 11, {1, 100, 0, 0, 0, 60, 0, 40}, E50, FX_N_NONE, 0, 0, 0, 12, 15, LD_M, 2},
    {"TRI LEAD", 11, {4, 100, 0, 0, 0, 25, 0, 0}, E50, FX_N_NONE, 0, 0, 0, 10, 15, LD_M, 12},
    {"GB LEAD", 11, {1, 92, 0, 0, 35, 0, 0, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 20, LD_M, 4},
    {"SNES LEAD", 11, {0, 80, 10, 20, 20, 30, 0, 0}, E50, FX_N_NONE, 0, 0, 0, 15, 20, LD_M, 13},
    {"ARCADE SLIDE", 11, {1, 100, 0, 0, 0, 20, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 20, LD_M, 2},
    {"BOSS THEME", 11, {2, 90, 10, 30, 30, 35, 0, 40}, E50, FX_N_NONE, 0, 0, 0, 15, 20, LD_M, 9},
    /* LEAD, block 2 */
    {"OUTRUN SAW", 11, {0, 62, 25, 30, 35, 35, 10, 25}, E50, FX_N_CHORUS, 35, 30, 45, 45, 35, LD_M, 13},
    {"NEON SUPERLEAD", 11, {3, 70, 15, 25, 60, 30, 0, 0}, E50, FX_N_CHORUS, 35, 30, 45, 45, 35, LD_M, 14},
    {"SUNSET LEAD", 11, {0, 50, 20, 35, 25, 40, 25, 0}, E50, FX_N_CHORUS, 35, 30, 45, 55, 35, LD_M, 14},
    {"SYNTH MONO", 11, {0, 55, 35, 45, 0, 25, 35, 40}, E50, FX_N_NONE, 0, 0, 0, 35, 30, LD_M, 9},
    {"LASER SOLO", 11, {1, 75, 35, 40, 25, 20, 30, 0}, E50, FX_N_NONE, 0, 0, 0, 35, 40, LD_M, 2},
    {"DX LEAD", 11, {5, 80, 0, 60, 50, 25, 0, 0}, E50, FX_N_CHORUS, 35, 30, 45, 40, 35, LD_M, 9},
    {"VAPOR SOLO", 11, {3, 40, 10, 20, 50, 45, 15, 20}, E50, FX_N_CHORUS, 35, 30, 45, 60, 35, LD_M, 16},
    {"CHROME", 11, {2, 68, 30, 35, 40, 30, 0, 20}, E50, FX_N_CHORUS, 35, 30, 45, 40, 35, LD_M, 9},
    /* LEAD, block 3 */
    {"HORN LEAD", 11, {0, 40, 10, 50, 20, 35, 0, 30}, {65, 50, 60}, FX_N_NONE, 0, 0, 0, 85, 25, LD_M, 13},
    {"EPIC SUPERLEAD", 11, {3, 55, 15, 40, 70, 30, 0, 25}, {60, 50, 60}, FX_N_NONE, 0, 0, 0, 85, 25, LD_M, 11},
    {"LONELY TRUMPET", 11, {0, 52, 20, 45, 10, 55, 10, 0}, {55, 50, 60}, FX_N_NONE, 0, 0, 0, 80, 30, LD_M, 12},
    {"DUDUK", 11, {4, 48, 0, 0, 0, 65, 25, 0}, {60, 50, 65}, FX_N_NONE, 0, 0, 0, 85, 25, LD_M, 8},
    {"ELEGY", 11, {1, 35, 15, 20, 20, 50, 30, 0}, {60, 50, 65}, FX_N_NONE, 0, 0, 0, 90, 30, LD_M, 2},
    {"TENSE FM", 11, {5, 58, 0, 35, 70, 40, 0, 0}, E50, FX_N_NONE, 0, 0, 0, 80, 25, LD_M, 5},
    {"ARCTIC BELL LEAD", 11, {5, 75, 0, 55, 35, 25, 0, 0}, {45, 70, 70}, FX_N_NONE, 0, 0, 0, 95, 35, LD_M, 4},
    {"WAR HORN", 11, {0, 32, 25, 55, 30, 25, 20, 60}, {60, 50, 65}, FX_N_NONE, 0, 0, 0, 90, 20, LD_M, 10},
};
/* presets an engine has in each block (AMBIENT LOFI is the first twelve: PRESETS) */
static const uint8_t PCOUNT[12 * DK_NBLK] = {
    12, 4, 4, 6,
    12, 4, 8, 4,
    12, 0, 8, 12,
    12, 4, 6, 6,
    12, 8, 12, 8,
    12, 12, 12, 12,
    12, 0, 0, 4,
    12, 0, 0, 0,
    12, 0, 6, 12,
    12, 12, 12, 12,
    12, 6, 6, 12,
    12, 8, 8, 8,
};
static const char *const BLOCK_NAME[DK_NBLK] = {"AMBIENT LOFI", "8-16 BIT", "SYNTHWAVE", "CINEMATIC"};

static const dk_engine_t *const ENGINES[12] = {&ENG_FELT, &ENG_TINE, &ENG_AIRPAD, &ENG_GLASS, &ENG_SOFTBASS, &ENG_DUSTKIT,
                                               &ENG_WORLD, &ENG_KALIMBA, &ENG_WINDS, &ENG_SYNTH, &ENG_PLUCK, &ENG_LEAD};

/* ---------------------------------------------------------------- the harmony --- */
/* the song's styles: its chords and how long each (once also DREAM's, the generator, now gone) */
enum { ST_C418, ST_LOFI, ST_AMBIENT, ST_CAMPFIRE, NSTYLE };
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

static const char *const PERFORM[11] = {"TAPESTOP", "REWIND", "VINYL STOP", "LOW PASS", "RAIN UP", "STUTTER",
                                        "HALF TIME", "ECHO THROW", "MUTE DRUMS", "SLEEP", "REVERSE"};
static const uint8_t PERFORM_ACT[11] = {PF_TAPESTOP, PF_REWIND, PF_VINYLSTOP, PF_LOWPASS, PF_RAINUP, PF_STUTTER,
                                        PF_HALFTIME, PF_ECHOTHROW, PF_MUTEDRUMS, PF_SLEEP, PF_REVERSE};

/* the biomes (the garden's scenes): DAWN BLOSSOM RAIN EMBER SNOW LAGOON NIGHT AURORA DUSK */
static const char *const BIOME_NAME[9] = {"DAWN", "BLOSSOM", "RAIN", "EMBER", "SNOW", "LAGOON", "NIGHT", "AURORA", "DUSK"};
/* name, tempo, mode, style, biome, world preset, dust, space (ROOM HALL CAVE SHIMMER); the tracks' engines, presets */
/* the tracks: PIANO (FELT) KEYS (TINE / GLASS / KALIMBA) PAD (AIRPAD) BASS DRUMS LEAD */
static const dk_mood_t MOODS[8] = {
    {"PLAINS", 76, 0, ST_C418, 0, 6, 25, 1, {0, 3, 2, 4, 5, 11}, {0, 0, 0, 1, 4, 5}},
    {"RAIN", 72, 2, ST_LOFI, 2, 0, 45, 0, {0, 1, 2, 4, 5, 11}, {9, 0, 7, 0, 0, 6}},
    {"CAVE", 64, 1, ST_AMBIENT, 7, 4, 20, 2, {0, 3, 2, 4, 5, 11}, {7, 7, 5, 6, 4, 8}},
    {"SNOW", 68, 4, ST_AMBIENT, 4, 9, 20, 3, {0, 3, 2, 4, 5, 11}, {4, 3, 3, 1, 4, 11}},
    {"OCEAN", 70, 5, ST_C418, 5, 7, 25, 1, {0, 7, 2, 4, 5, 11}, {1, 0, 9, 0, 4, 7}},
    {"NIGHT", 60, 0, ST_C418, 6, 5, 30, 1, {0, 3, 2, 4, 5, 11}, {10, 10, 1, 10, 4, 0}},
    {"CAMPFIRE", 82, 8, ST_CAMPFIRE, 3, 2, 30, 0, {0, 7, 2, 4, 5, 11}, {2, 4, 2, 2, 9, 1}},
    {"STUDY", 84, 2, ST_LOFI, 8, 11, 55, 0, {0, 1, 2, 4, 5, 11}, {9, 5, 8, 8, 3, 2}},
};

/* the tracks by default: PIANO KEYS PAD BASS DRUMS NATURE (the piano first: it is played the most) */
static const uint8_t DEF_ENG[DK_NTRACK] = {0, 1, 2, 4, 11, 5}, DEF_PRESET[DK_NTRACK] = {0, 0, 0, 0, 0, 0},
                     DEF_SEQ[DK_NTRACK] = {SQ_STEP, SQ_STEP, SQ_STEP, SQ_STEP, SQ_STEP, SQ_STEP},
                     DEF_ROLE[DK_NTRACK] = {R_KEYS, R_ARP, R_PAD, R_BASS, R_LEAD, R_DRUM};
/* the KEYS track's twelve: electric pianos, bells, kalimbas (HOME + white 1-12, the PRESET knob) */
static const uint8_t KEYS_SET[DK_NFACT][2] = {{1, 0}, {1, 1}, {1, 3}, {1, 9},  {3, 0}, {3, 1},
                                              {3, 3}, {3, 4}, {7, 0}, {7, 1}, {7, 7}, {7, 9}};

/* ATMO (the SEL page): the place under the music, each layer a WORLD voice of its own (TYPE, AMOUNT, TONE) */
static const dk_atmo_t ATMO[6] = {
    {"RAIN", 6, {W_RAIN, 55, 50}, 0.9f},     {"FIRE", 6, {W_FIRE, 60, 55}, 1.0f},
    {"BIRDS", 6, {W_BIRDS, 50, 60}, 0.8f},   {"OCEAN", 6, {W_OCEAN, 55, 45}, 0.9f},
    {"WIND", 6, {W_WIND, 50, 45}, 0.8f},     {"CRICKETS", 6, {W_CRICKETS, 50, 55}, 0.7f},
};

#include "grooves.c"
#include "demo.c"

static void tape_init(void)
{
    trng = 0x2545F491u;                         /* the same noise each time (the tests' WAVs repeat) */
    pad_tables();
}

const dk_tape_t TAPE = {
    "OVERWORLD", tape_init, ENGINES, 12, PRESETS, TAPE_FX, 8, STYLE_NAME, NSTYLE, 0, PROGS, 5, STYLE_CHORD,
    STYLE_BARS, STYLE_BREATH, BORROW_MAJ, BORROW_MIN, 0, PERFORM, PERFORM_ACT, MOODS, 8,
    BIOME_NAME, tape_master, tape_demo, METRO_WOOD, 25, DEF_ENG, DEF_PRESET, DEF_SEQ, DEF_ROLE, KEYS_SET, R_ARP,
    ATMO, 6, PRESETS2, PCOUNT, BLOCK_NAME, GROOVES, 15, 10, FILLS, &GROOVE_END,
};
