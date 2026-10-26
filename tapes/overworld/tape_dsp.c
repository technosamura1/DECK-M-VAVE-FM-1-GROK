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
#include "finger_banks.c"

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
#define WIND_M {T_P0, T_P1, T_REV}, {40, 35, 35}
#define SYN_M {T_P1, T_P3, T_REV}, {50, 40, 40}
#define PLK_M {T_P1, T_P2, T_REV}, {50, 40, 40}
#define E50 {50, 50, 50}
static const dk_preset_t PRESETS[11 * DK_NFACT] = {
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
    /* TINE */
    {"RAINY DAY", 1, {50, 35, 30, 45, 50}, E50, FX_N_TAPE, 20, 30, 25, 40, 15, TINE_M, 0},
    {"SUITCASE", 1, {55, 50, 20, 55, 55}, E50, FX_N_NONE, 0, 0, 0, 35, 10, TINE_M, 1},
    {"DUSTY", 1, {45, 40, 45, 35, 50}, E50, FX_N_TAPE, 40, 40, 45, 40, 15, TINE_M, 0},
    {"VELVET", 1, {35, 25, 20, 30, 60}, E50, FX_N_CHORUS, 35, 30, 40, 50, 10, TINE_M, 0},
    {"BELLTONE", 1, {80, 20, 20, 60, 60}, E50, FX_N_NONE, 0, 0, 0, 50, 20, TINE_M, -1},
    {"STUDY", 1, {48, 30, 35, 40, 45}, E50, FX_N_VINYL, 30, 40, 30, 35, 12, TINE_M, 1},
    {"WARM", 1, {30, 20, 15, 25, 55}, E50, FX_N_TAPE, 15, 20, 30, 40, 8, TINE_M, 0},
    {"SHIMMER", 1, {65, 45, 25, 50, 65}, E50, FX_N_CHORUS, 50, 40, 50, 60, 25, TINE_M, 2},
    {"BROKEN", 1, {50, 60, 70, 40, 45}, E50, FX_N_LOFI, 35, 40, 35, 35, 10, TINE_M, 1},
    {"MELLOW", 1, {25, 15, 10, 20, 50}, E50, FX_N_NONE, 0, 0, 0, 45, 10, TINE_M, -1},
    {"NIGHT", 1, {40, 55, 30, 35, 60}, E50, FX_N_SPRING, 30, 50, 40, 50, 15, TINE_M, 2},
    {"WORN", 1, {45, 45, 60, 45, 50}, E50, FX_N_TAPE, 55, 60, 50, 40, 15, TINE_M, 0},
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
    /* GLASS */
    {"GLASS", 3, {0, 50, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 75, 25, GLASS_M, -3},
    {"CELESTA", 3, {1, 50, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 70, 20, GLASS_M, -1},
    {"MARIMBA", 3, {2, 50, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 55, 10, GLASS_M, 7},
    {"MUSIC BOX", 3, {3, 55, 55, 0}, E50, FX_N_NONE, 0, 0, 0, 70, 25, GLASS_M, 0},
    {"VIBES", 3, {4, 50, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 60, 15, GLASS_M, 4},
    {"SNOW", 3, {0, 70, 45, 1}, E50, FX_N_NONE, 0, 0, 0, 90, 40, GLASS_M, -5},
    {"STARS", 3, {3, 60, 50, 1}, E50, FX_N_NONE, 0, 0, 0, 85, 35, GLASS_M, -2},
    {"CHAPEL", 3, {1, 75, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 95, 20, GLASS_M, -3},
    {"WOOD", 3, {2, 40, 35, 0}, E50, FX_N_NONE, 0, 0, 0, 45, 8, GLASS_M, 10},
    {"DROPS", 3, {0, 40, 50, 1}, E50, FX_N_NONE, 0, 0, 0, 80, 30, GLASS_M, -2},
    {"NIGHT VIBES", 3, {4, 70, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 75, 25, GLASS_M, 4},
    {"WIND CHIME", 3, {0, 75, 45, 1}, E50, FX_N_NONE, 0, 0, 0, 90, 30, GLASS_M, -4},
    /* SOFTBASS */
    {"SUB", 4, {40, 35, 0, 0, 50}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 0},
    {"ROUND", 4, {30, 20, 0, 0, 60}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 0},
    {"UPRIGHT", 4, {45, 50, 0, 2, 50}, E50, FX_N_NONE, 0, 0, 0, 20, 0, BASS_M, 1},
    {"THUMB", 4, {55, 70, 0, 2, 40}, E50, FX_N_NONE, 0, 0, 0, 15, 0, BASS_M, 2},
    {"MOOG", 4, {40, 40, 0, 1, 50}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 6},
    {"GLIDE", 4, {40, 30, 50, 0, 55}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 0},
    {"DEEP", 4, {20, 15, 0, 0, 75}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 0},
    {"WARM MOOG", 4, {30, 25, 20, 1, 55}, E50, FX_N_TAPE, 20, 30, 30, 10, 0, BASS_M, 5},
    {"JAZZ", 4, {50, 60, 0, 2, 45}, E50, FX_N_NONE, 0, 0, 0, 20, 0, BASS_M, 1},
    {"FELT BASS", 4, {35, 45, 0, 0, 40}, E50, FX_N_TAPE, 25, 20, 20, 10, 0, BASS_M, 0},
    {"NIGHT", 4, {25, 25, 30, 0, 65}, E50, FX_N_NONE, 0, 0, 0, 15, 0, BASS_M, 0},
    {"PLUCKY", 4, {50, 70, 0, 1, 30}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, 7},
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
    {"WIND", 6, {1, 50, 62}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, -2},
    {"FIRE", 6, {2, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 10, 0, WORLD_M, 15},
    {"STREAM", 6, {3, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 10},
    {"CAVE DROPS", 6, {4, 50, 50}, E50, FX_N_NONE, 0, 0, 0, 60, 10, WORLD_M, 8},
    {"CRICKETS", 6, {5, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 14},
    {"BIRDS", 6, {6, 50, 60}, E50, FX_N_NONE, 0, 0, 0, 25, 0, WORLD_M, 2},
    {"OCEAN", 6, {7, 50, 50}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 9},
    {"THUNDER", 6, {8, 50, 45}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 14},
    {"SNOW STEPS", 6, {9, 50, 55}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 0},
    {"STORM", 6, {8, 80, 50}, E50, FX_N_NONE, 0, 0, 0, 20, 0, WORLD_M, 5},
    {"DRIZZLE", 6, {0, 40, 40}, E50, FX_N_NONE, 0, 0, 0, 15, 0, WORLD_M, 24},
    /* KALIMBA */
    {"KALIMBA", 7, {50, 50, 45}, E50, FX_N_NONE, 0, 0, 0, 50, 15, KAL_M, -2},
    {"MBIRA", 7, {60, 45, 35}, E50, FX_N_TAPE, 25, 30, 30, 45, 12, KAL_M, -2},
    {"THUMB", 7, {40, 55, 55}, E50, FX_N_NONE, 0, 0, 0, 45, 10, KAL_M, 0},
    {"BRIGHT", 7, {75, 40, 30}, E50, FX_N_NONE, 0, 0, 0, 50, 15, KAL_M, -2},
    {"CAMPFIRE", 7, {40, 55, 70}, E50, FX_N_NONE, 0, 0, 0, 55, 10, KAL_M, 0},
    {"BOX", 7, {70, 65, 20}, E50, FX_N_NONE, 0, 0, 0, 60, 30, KAL_M, -4},
    {"WOODEN", 7, {35, 35, 80}, E50, FX_N_NONE, 0, 0, 0, 35, 8, KAL_M, 3},
    {"DREAM", 7, {55, 75, 40}, E50, FX_N_NONE, 0, 0, 0, 85, 30, KAL_M, -5},
    {"LOFI", 7, {45, 45, 50}, E50, FX_N_LOFI, 35, 35, 35, 40, 10, KAL_M, 0},
    {"STARLIGHT", 7, {65, 70, 30}, E50, FX_N_NONE, 0, 0, 0, 80, 25, KAL_M, -6},
    {"TINY", 7, {80, 30, 25}, E50, FX_N_NONE, 0, 0, 0, 45, 15, KAL_M, -1},
    {"DEEP", 7, {30, 65, 60}, E50, FX_N_NONE, 0, 0, 0, 55, 10, KAL_M, -1},
    /* WINDS */
    {"FLUTE", 8, {50, 40, 40, 0, 50}, E50, FX_N_NONE, 0, 0, 0, 55, 12, WIND_M, 1},
    {"PAN FLUTE", 8, {40, 70, 25, 1, 40}, E50, FX_N_NONE, 0, 0, 0, 65, 15, WIND_M, 1},
    {"OCARINA", 8, {30, 30, 35, 2, 50}, E50, FX_N_NONE, 0, 0, 0, 60, 15, WIND_M, 1},
    {"SHAKUHACHI", 8, {35, 90, 50, 1, 60}, E50, FX_N_NONE, 0, 0, 0, 80, 20, WIND_M, 1},
    {"CLARINET", 8, {45, 20, 30, 3, 50}, E50, FX_N_NONE, 0, 0, 0, 50, 10, WIND_M, -1},
    {"TRUMPET", 8, {60, 15, 35, 4, 45}, E50, FX_N_NONE, 0, 0, 0, 50, 10, WIND_M, 0},
    {"MUTED", 8, {50, 15, 30, 5, 45}, E50, FX_N_TAPE, 20, 25, 20, 55, 15, WIND_M, 0},
    {"HORN", 8, {35, 10, 25, 6, 55}, E50, FX_N_NONE, 0, 0, 0, 75, 8, WIND_M, 1},
    {"BRASS", 8, {55, 10, 20, 7, 55}, E50, FX_N_NONE, 0, 0, 0, 60, 8, WIND_M, 0},
    {"FLUGEL", 8, {40, 20, 40, 4, 58}, E50, FX_N_TAPE, 20, 25, 25, 65, 12, WIND_M, 0},
    {"MORNING", 8, {60, 50, 50, 0, 40}, E50, FX_N_CHORUS, 25, 25, 30, 85, 25, WIND_M, -1},
    {"OLD BAND", 8, {50, 20, 30, 7, 50}, E50, FX_N_VINYL, 35, 40, 30, 45, 10, WIND_M, 1},
    /* SYNTH */
    {"SOFT SAW", 9, {0, 38, 10, 15, 30, 0, 100, 25}, E50, FX_N_TAPE, 20, 30, 25, 60, 15, SYN_M, 7},
    {"DUST LEAD", 9, {4, 55, 10, 0, 20, 15, 100, 30}, E50, FX_N_LOFI, 35, 25, 25, 50, 20, SYN_M, 5},
    {"WARM SUPER", 9, {5, 35, 10, 10, 40, 0, 100, 20}, E50, FX_N_CHORUS, 35, 25, 40, 70, 15, SYN_M, 12},
    {"SINE LEAD", 9, {7, 70, 0, 0, 10, 0, 100, 15}, E50, FX_N_TAPE, 20, 25, 20, 55, 40, SYN_M, -3},
    {"MUSIC BED", 9, {5, 28, 20, 0, 55, 0, 100, 40}, E50, FX_N_TAPE, 15, 20, 20, 85, 20, SYN_M, 14},
    {"TAPE SQUARE", 9, {3, 30, 15, 10, 15, 10, 70, 20}, E50, FX_N_TAPE, 35, 30, 35, 45, 15, SYN_M, 4},
    {"HAZE", 9, {0, 25, 25, 20, 45, 0, 100, 60}, E50, FX_N_VINYL, 30, 30, 30, 90, 20, SYN_M, 12},
    {"GLASS SYNTH", 9, {4, 60, 5, 0, 35, 0, 100, 30}, E50, FX_N_CHORUS, 30, 20, 35, 70, 25, SYN_M, 4},
    {"SOFT PLUCK", 9, {0, 45, 30, 35, 20, 0, 25, 10}, E50, FX_N_TAPE, 20, 25, 25, 50, 40, SYN_M, 12},
    {"BREATH", 9, {6, 30, 40, 0, 0, 0, 100, 50}, E50, FX_N_NONE, 0, 0, 0, 80, 10, SYN_M, 3},
    {"LOFI PULSE", 9, {1, 40, 20, 20, 20, 30, 40, 10}, E50, FX_N_LOFI, 40, 40, 35, 45, 25, SYN_M, 14},
    {"SLOW SWEEP", 9, {0, 20, 35, 55, 25, 0, 100, 35}, E50, FX_N_TAPE, 20, 20, 20, 75, 25, SYN_M, 9},
    /* PLUCK */
    {"NYLON", 10, {2, 45, 50, 40, 45, 0}, E50, FX_N_TAPE, 15, 25, 20, 55, 15, PLK_M, -10},
    {"SOFT HARP", 10, {1, 40, 60, 35, 40, 0}, E50, FX_N_NONE, 0, 0, 0, 75, 20, PLK_M, -16},
    {"LAZY LUTE", 10, {0, 40, 50, 40, 50, 0}, E50, FX_N_TAPE, 20, 25, 25, 50, 15, PLK_M, -10},
    {"DUSTY KOTO", 10, {4, 45, 55, 40, 40, 15}, E50, FX_N_LOFI, 30, 25, 30, 55, 15, PLK_M, -10},
    {"DULCIMER", 10, {5, 50, 60, 45, 50, 0}, E50, FX_N_VINYL, 25, 30, 25, 70, 20, PLK_M, -15},
    {"WARM STEEL", 10, {3, 35, 50, 40, 50, 0}, E50, FX_N_TAPE, 20, 20, 20, 50, 15, PLK_M, -12},
    {"NIGHT HARP", 10, {1, 30, 80, 30, 50, 0}, E50, FX_N_NONE, 0, 0, 0, 90, 30, PLK_M, -17},
    {"WORN NYLON", 10, {2, 35, 45, 40, 45, 25}, E50, FX_N_LOFI, 35, 30, 30, 45, 15, PLK_M, -9},
    {"RAIN KOTO", 10, {4, 55, 70, 30, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 80, 40, PLK_M, -14},
    {"CAMPFIRE", 10, {3, 50, 40, 45, 55, 0}, E50, FX_N_NONE, 0, 0, 0, 45, 10, PLK_M, -12},
    {"FELT HARP", 10, {1, 25, 50, 30, 40, 0}, E50, FX_N_TAPE, 25, 30, 25, 60, 20, PLK_M, -13},
    {"TINY LUTE", 10, {0, 60, 35, 50, 35, 0}, E50, FX_N_NONE, 0, 0, 0, 40, 10, PLK_M, -4},
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
    {"TRI SLIDE", 4, {50, 0, 55, 5, 60}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 5},
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
    {"PULSE DRIVE", 4, {50, 0, 0, 3, 70}, E50, FX_N_CHORUS, 25, 30, 40, 8, 0, BASS_M, 12},
    {"SUB WAVE", 4, {30, 20, 0, 0, 70}, E50, FX_N_NONE, 0, 0, 0, 10, 0, BASS_M, -1},
    {"ACID", 4, {80, 90, 40, 1, 30}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 6},
    {"MIDNIGHT", 4, {35, 40, 0, 4, 90}, E50, FX_N_NONE, 0, 0, 0, 12, 0, BASS_M, 6},
    {"LASER", 4, {75, 80, 70, 1, 35}, E50, FX_N_NONE, 0, 0, 0, 5, 0, BASS_M, 6},
    /* SOFTBASS, block 3 */
    {"SUB DRONE", 4, {40, 0, 0, 6, 80}, E50, FX_N_NONE, 0, 0, 0, 25, 0, BASS_M, 7},
    {"RUMBLE", 4, {20, 0, 0, 6, 100}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 7},
    {"WAR HORN LOW", 4, {25, 10, 0, 4, 90}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 5},
    {"BOWED BASS", 4, {35, 15, 0, 2, 85}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 0},
    {"ANCIENT", 4, {20, 10, 0, 0, 90}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, -2},
    {"DRAGON BREATH", 4, {60, 0, 20, 6, 90}, E50, FX_N_NONE, 0, 0, 0, 35, 0, BASS_M, 5},
    {"CELLO LOW", 4, {40, 25, 0, 2, 100}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, -1},
    {"BRONZE", 4, {30, 20, 0, 1, 80}, E50, FX_N_NONE, 0, 0, 0, 30, 0, BASS_M, 5},
    /* DUSTKIT, block 1 */
    {"NES KIT", 5, {50, 50, 45, 3}, E50, FX_N_NONE, 0, 0, 0, 8, 0, KIT_M, 2},
    {"GAME BOY", 5, {60, 40, 60, 3}, E50, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 2},
    {"ARCADE", 5, {55, 45, 30, 3}, E50, FX_N_NONE, 0, 0, 0, 10, 0, KIT_M, 2},
    {"C64", 5, {40, 60, 55, 3}, E50, FX_N_NONE, 0, 0, 0, 6, 0, KIT_M, 0},
    {"16-BIT", 5, {50, 50, 20, 3}, E50, FX_N_NONE, 0, 0, 0, 10, 0, KIT_M, 2},
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
    {"707 PUNCH", 5, {50, 40, 20, 0}, E50, FX_N_NONE, 0, 0, 0, 12, 0, KIT_M, 0},
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
    {"BARD FLUTE", 8, {50, 45, 45, 0, 45}, E50, FX_N_NONE, 0, 0, 0, 70, 25, WIND_M, 0},
    {"PIPES", 8, {30, 40, 50, 2, 50}, E50, FX_N_NONE, 0, 0, 0, 75, 25, WIND_M, 1},
    {"DISTANT HORN", 8, {20, 20, 20, 6, 95}, E50, FX_N_NONE, 0, 0, 0, 100, 45, WIND_M, 3},
    {"LAMENT", 8, {30, 70, 80, 9, 70}, E50, FX_N_NONE, 0, 0, 0, 95, 30, WIND_M, -4},
    {"PAN WOODS", 8, {45, 75, 30, 1, 50}, E50, FX_N_NONE, 0, 0, 0, 85, 25, WIND_M, 1},
    {"CARNYX", 8, {35, 10, 20, 8, 60}, E50, FX_N_NONE, 0, 0, 0, 85, 20, WIND_M, 3},
    {"HUNTER", 8, {45, 20, 30, 5, 50}, E50, FX_N_NONE, 0, 0, 0, 80, 20, WIND_M, -2},
    /* SYNTH, block 1 */
    {"PULSE LEAD", 9, {1, 80, 10, 0, 10, 25, 100, 25}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, 6},
    {"SQUARE LEAD", 9, {3, 75, 10, 0, 0, 30, 100, 30}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, -3},
    {"TRIANGLE", 9, {4, 90, 0, 0, 0, 20, 100, 15}, E50, FX_N_NONE, 0, 0, 0, 10, 15, SYN_M, 6},
    {"12.5% PULSE", 9, {2, 85, 10, 0, 0, 30, 100, 20}, E50, FX_N_NONE, 0, 0, 0, 15, 20, SYN_M, 13},
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
    {"WIND SYNTH", 9, {6, 35, 50, 0, 0, 0, 100, 70}, E50, FX_N_NONE, 0, 0, 0, 100, 30, SYN_M, 1},
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
    {"MOURNING", 10, {1, 30, 90, 30, 45, 0}, E50, FX_N_NONE, 0, 0, 0, 100, 40, PLK_M, -18},
};
/* presets an engine has in each block (AMBIENT LOFI is the first twelve: PRESETS) */
static const uint8_t PCOUNT[11 * DK_NBLK] = {
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
};
static const char *const BLOCK_NAME[DK_NBLK] = {"AMBIENT LOFI", "8-16 BIT", "SYNTHWAVE", "CINEMATIC"};

static const dk_engine_t *const ENGINES[11] = {&ENG_FELT, &ENG_TINE, &ENG_AIRPAD, &ENG_GLASS, &ENG_SOFTBASS, &ENG_DUSTKIT,
                                               &ENG_WORLD, &ENG_KALIMBA, &ENG_WINDS, &ENG_SYNTH, &ENG_PLUCK};

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
/* the tracks: PIANO (FELT) KEYS (TINE / GLASS / KALIMBA) PAD (AIRPAD) BASS DRUMS NATURE */
static const dk_mood_t MOODS[8] = {
    {"PLAINS", 76, 0, ST_C418, 0, 6, 25, 1, {0, 3, 2, 4, 5, 6}, {0, 0, 0, 1, 4, 6}},
    {"RAIN", 72, 2, ST_LOFI, 2, 0, 45, 0, {0, 1, 2, 4, 5, 6}, {9, 0, 7, 0, 0, 0}},
    {"CAVE", 64, 1, ST_AMBIENT, 7, 4, 20, 2, {0, 3, 2, 4, 5, 6}, {7, 7, 5, 6, 4, 4}},
    {"SNOW", 68, 4, ST_AMBIENT, 4, 9, 20, 3, {0, 3, 2, 4, 5, 6}, {4, 3, 3, 1, 4, 9}},
    {"OCEAN", 70, 5, ST_C418, 5, 7, 25, 1, {0, 7, 2, 4, 5, 6}, {1, 0, 9, 0, 4, 7}},
    {"NIGHT", 60, 0, ST_C418, 6, 5, 30, 1, {0, 3, 2, 4, 5, 6}, {10, 10, 1, 10, 4, 5}},
    {"CAMPFIRE", 82, 8, ST_CAMPFIRE, 3, 2, 30, 0, {0, 7, 2, 4, 5, 6}, {2, 4, 2, 2, 9, 2}},
    {"STUDY", 84, 2, ST_LOFI, 8, 11, 55, 0, {0, 1, 2, 4, 5, 6}, {9, 5, 8, 8, 3, 11}},
};

/* the tracks by default: PIANO KEYS PAD BASS DRUMS NATURE (the piano first: it is played the most) */
static const uint8_t DEF_ENG[DK_NTRACK] = {0, 1, 2, 4, 5, 6}, DEF_PRESET[DK_NTRACK] = {0, 0, 0, 0, 0, 6},
                     DEF_SEQ[DK_NTRACK] = {SQ_LIVE, SQ_LIVE, SQ_LIVE, SQ_STEP, SQ_FINGER, SQ_LIVE},
                     DEF_ROLE[DK_NTRACK] = {R_KEYS, R_ARP, R_PAD, R_BASS, R_DRUM, R_NATURE};
/* the KEYS track's twelve: electric pianos, bells, kalimbas (HOME + white 1-12, the PRESET knob) */
static const uint8_t KEYS_SET[DK_NFACT][2] = {{1, 0}, {1, 1}, {1, 3}, {1, 9},  {3, 0}, {3, 1},
                                              {3, 3}, {3, 4}, {7, 0}, {7, 1}, {7, 7}, {7, 9}};

/* ATMO (the SEL page): the place under the music, each layer a WORLD voice of its own (TYPE, AMOUNT, TONE) */
static const dk_atmo_t ATMO[6] = {
    {"RAIN", 6, {W_RAIN, 55, 50}, 0.9f},     {"FIRE", 6, {W_FIRE, 60, 55}, 1.0f},
    {"BIRDS", 6, {W_BIRDS, 50, 60}, 0.8f},   {"OCEAN", 6, {W_OCEAN, 55, 45}, 0.9f},
    {"WIND", 6, {W_WIND, 50, 45}, 0.8f},     {"CRICKETS", 6, {W_CRICKETS, 50, 55}, 0.7f},
};

#include "demo.c"

static void tape_init(void)
{
    trng = 0x2545F491u;                         /* the same noise each time (the tests' WAVs repeat) */
    pad_tables();
}

const dk_tape_t TAPE = {
    "OVERWORLD", tape_init, ENGINES, 11, PRESETS, TAPE_FX, 8, STYLE_NAME, NSTYLE, tape_drive, PROGS, 5, STYLE_CHORD,
    STYLE_BARS, STYLE_BREATH, BORROW_MAJ, BORROW_MIN, FBANKS, PERFORM, PERFORM_ACT, MOODS, 8,
    BIOME_NAME, tape_master, tape_demo, METRO_WOOD, 25, DEF_ENG, DEF_PRESET, DEF_SEQ, DEF_ROLE, KEYS_SET, R_ARP,
    ATMO, 6, PRESETS2, PCOUNT, BLOCK_NAME,
};
