/* SPDX-License-Identifier: MPL-2.0 */
/* DECK's sound alone (the core, the tape, the song), on the host: WAVs and numbers.
 *   deck_test sounds OUTDIR            every engine's every preset: a note, a chord, a phrase (out/sounds/ENG/NN_NAME.wav)
 *   deck_test demo OUT.wav             the tape's demo song, the road once
 *   deck_test drive STYLE SECS OUT.wav every track in DRIVE, the style (0..3), from its mood
 *   deck_test mood M SECS OUT.wav      a mood (DRIVE everywhere)
 *   deck_test chords OUT.wav           a progression in three modes (the keys' chords)
 *   deck_test phrase OUT.wav           OVERWORLD's piano phrase (ow_test's "phrase") on FELT, to compare
 *   deck_test bench                    the render's cost: the demo's busiest, and all voices busy
 *   deck_test metro                    the metronome's clicks against the clock
 * It prints the peak, the rms and the cost (us a 128-frame block, host). */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L   /* clock_gettime under -std=c99 */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "song.h"
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
static double now_s(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}
#define MKDIR(p) mkdir(p, 0777)
#endif

#ifdef DK_PROF
extern double dk_prof[8];
double dk_prof_now(void) { return now_s(); }
#endif
void dk_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

static int16_t *buf;
static int frames, cap;
static double sum2;
static int peak;
static double cost_s;
static long blocks;
static void wav(const char *fn)
{
    FILE *f = fopen(fn, "wb");
    int32_t v;
    int16_t h;
    if (!f) {
        fprintf(stderr, "cannot write %s\n", fn);
        exit(2);
    }
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f); h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f); fwrite(buf, 4, (size_t)frames, f); fclose(f);
}
static void run(double secs)
{
    int n = (int)(secs * 44100.0), i;
    int32_t blk[512];
    while (n > 0) {
        int k = n > 256 ? 256 : n;
        double c0 = now_s();
        dk_render(blk, (uint32_t)k, 4096u);
        cost_s += now_s() - c0;
        blocks++;
        if (frames + k > cap) {
            cap = cap * 2 + 44100 * 10;
            buf = realloc(buf, (size_t)cap * 4);
        }
        for (i = 0; i < 2 * k; i++) {
            int s = blk[i] >> 8;
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            if (abs(s) > peak) peak = abs(s);
            sum2 += (double)s * s;
            buf[2 * frames + i] = (int16_t)s;
        }
        frames += k;
        n -= k;
    }
}
/* until it rings out (the level below -66 dB for 0.3 s), at most max seconds more */
static void tail(double max)
{
    double t = 0.0;
    int quiet = 0;
    while (t < max) {
        int i, f0 = frames, pk = 0;
        run(0.05);
        for (i = 2 * f0; i < 2 * frames; i++)
            if (abs(buf[i]) > pk) pk = abs(buf[i]);
        quiet = pk < 16 ? quiet + 1 : 0;
        if (quiet >= 6)
            break;
        t += 0.05;
    }
}
static void reset_out(void)
{
    frames = 0;
    sum2 = 0.0;
    peak = 0;
    cost_s = 0.0;
    blocks = 0;
}
static void report(const char *what)
{
    double rms = frames ? sqrt(sum2 / (2.0 * frames)) : 0.0;
    printf("%-34s %6.2f s  peak %6.1f dBFS  rms %6.1f dBFS  cost %5.1f us/128\n", what, frames / 44100.0,
           20.0 * log10((peak + 1) / 32768.0), 20.0 * log10((rms + 1e-9) / 32768.0),
           blocks ? cost_s * 1e6 / (double)frames * 128.0 : 0.0);
}
static void note(int t, int n, int v) { dk_note(t, n, v, 0); }
static void quiet_all(void)
{
    dk_cmd(C_PLAY, 0, 0, 0);
    dk_cmd(C_PANIC, 0, 0, 0);
    run(0.05);
}
static void fresh(void)
{
    TAPE.init();
    dk_lock();
    dk_song_clear();
    dk_song.dust = 25;   /* (the old default: its noise draws on the random numbers in quiet_all, and the calibrated
                          * renders were made with that sequence; each render sets its own dust after) */
    dk_unlock();
    quiet_all();
}

/* ------------------------------------------------------------------ sounds --- */
static void play_melodic(int t, int lo)
{
    static const int CH[4] = {0, 7, 16, 23};               /* an open maj7: C G E' B' */
    static const int PH[6] = {12, 16, 19, 23, 21, 19};
    int i;
    note(t, lo + 12, 96);
    run(0.70);
    note(t, lo + 12, 0);
    run(0.15);
    for (i = 0; i < 4; i++)
        dk_note(t, lo + CH[i], 80, i * 900);
    run(1.15);
    for (i = 0; i < 4; i++)
        note(t, lo + CH[i], 0);
    run(0.15);
    for (i = 0; i < 6; i++) {
        note(t, lo + PH[i], 70 + 8 * (i & 1));
        run(i == 5 ? 0.6 : 0.24);
        note(t, lo + PH[i], 0);
    }
}
static void play_pad(int t)
{
    static const int CH[4] = {48, 55, 59, 64};
    int i;
    for (i = 0; i < 4; i++)
        note(t, CH[i], 90);
    run(3.4);
    for (i = 0; i < 4; i++)
        note(t, CH[i], 0);
    note(t, 67, 80);
    run(0.6);
    note(t, 67, 0);
}
static void play_bass(int t)
{
    static const int PH[8] = {36, 36, 43, 45, 41, 41, 43, 31};
    static const float D[8] = {0.4f, 0.2f, 0.4f, 0.4f, 0.4f, 0.2f, 0.4f, 0.9f};
    int i;
    for (i = 0; i < 8; i++) {
        note(t, PH[i], 100 - 10 * (i & 1));
        run((double)D[i] * 0.85);
        note(t, PH[i], 0);
        run((double)D[i] * 0.15);
    }
}
static void play_kit(int t)
{
    /* two bars at 84: kick, snare, the shaker, hats, a rim, a clap, the wood */
    static const char *L[8] = {"9.....6.8.......", "....8.......8...", "....5..5....5..5", "3.4.3.4.3.4.3.4.",
                               "6.6.6.6.6.6.6.6.", "..............5.", "............7...", "...4......4..4.."};
    static const int N[8] = {36, 38, 37, 70, 42, 46, 39, 76};
    double s16 = 60.0 / 84.0 / 4.0;
    int bar, s, l;
    for (bar = 0; bar < 2; bar++)
        for (s = 0; s < 16; s++) {
            for (l = 0; l < 8; l++) {
                char c = L[l][s];
                if (l == 2 && bar == 0)
                    continue;
                if (l == 3 && bar == 1)
                    continue;
                if (c >= '1' && c <= '9')
                    note(t, N[l], (c - '0') * 13 + 6);
            }
            run(s16);
        }
}
static void sounds(const char *dir, const char *only)
{
    int e, p;
    char fn[512], cmd[600];
    for (e = 0; e < TAPE.neng; e++) {
        const dk_engine_t *eng = TAPE.eng[e];
        if (only && !strstr(only, eng->name))
            continue;
        MKDIR(dir);
        sprintf(cmd, "%s/%s", dir, eng->name);
        MKDIR(cmd);
        for (p = 0; p < DK_NBLK * DK_NFACT; p++) {
            const dk_preset_t *pr;
            dk_preset_t tmp;
            int blk = p / DK_NFACT, id = preset_id(blk, p % DK_NFACT);
            char nm[40];
            if (p % DK_NFACT >= preset_count(e, blk))
                continue;
            pr = preset_of(e, id, &tmp);
            int i, t = 2;
            fresh();
            dk_lock();
            dk_song.tempo = 84;
            dk_song.dust = 0;
            dk_track_setup(t, e, id, SQ_LIVE, eng->role);
            dk_song.trk[t].vol = 80;
            dk_song.trk[t].pan = 0;
            dk_unlock();
            run(0.05);
            reset_out();
            if (eng->flags & EF_DRUM)
                play_kit(t);
            else if (eng->flags & EF_CONT) {
                note(t, 60, 100);
                run(3.5);
                note(t, 60, 0);
            } else if (eng->role == R_PAD)
                play_pad(t);
            else if (eng->role == R_BASS)
                play_bass(t);
            else
                play_melodic(t, eng->role == R_BELL ? 60 : 48);
            tail(20.0);
            for (i = 0; pr->name[i] && i < 30; i++)
                nm[i] = pr->name[i] == ' ' ? '_' : pr->name[i];
            nm[i] = 0;
            sprintf(fn, "%s/%s/b%d_%02d_%s.wav", dir, eng->name, blk, p % DK_NFACT + 1, nm);
            wav(fn);
            {   /* what each file is made of, for tools/dup_check.py: the engine's parameters, envelope, effects (not the name, not the level trim) */
                FILE *tf;
                sprintf(cmd, "%s/presets.tsv", dir);
                tf = fopen(cmd, "a");
                if (tf) {
                    fprintf(tf, "%s\t%s\t%d\t%s\t", eng->name, fn + strlen(dir) + 1, id, pr->name);
                    for (i = 0; i < DK_NP; i++)
                        fprintf(tf, "%d,", pr->p[i]);
                    fprintf(tf, "|%d,%d,%d|%d,%d,%d,%d|%d,%d|%d:%d,%d:%d,%d:%d|%d\n", pr->env[0], pr->env[1], pr->env[2], pr->fx, pr->fxamt,
                            pr->fxa, pr->fxb, pr->rev, pr->echo, pr->mt[0], pr->ma[0], pr->mt[1], pr->ma[1], pr->mt[2], pr->ma[2], pr->vol);
                    fclose(tf);
                }
            }
            sprintf(cmd, "%s %s", eng->name, pr->name);
            report(cmd);
        }
    }
}

/* each drum machine's voices, one at a time (out/kits/MACHINE/NN_VOICE.wav): to look at a kit's sounds one by one */
static void kitvoices(const char *dir)
{
    static const struct { const char *n; int blk, i; } M[5] = {{"808", 2, 8}, {"909", 2, 10}, {"606", 1, 8}, {"CR-78", 1, 10}, {"707", 0, 1}};
    static const struct { const char *n; int note; } V[] = {
        {"kick", 36}, {"kick2", 35}, {"snare", 38}, {"snare2", 40}, {"rim", 37}, {"clap", 39}, {"hat", 42}, {"open", 46},
        {"tomlo", 41}, {"tommid", 45}, {"tomhi", 50}, {"cowbell", 56}, {"tamb", 54}, {"crash", 49}, {"ride", 51},
        {"shaker", 70}, {"wood", 76}, {"congahi", 62}, {"congamid", 63}, {"congalo", 64}};
    int m, v;
    char fn[512], cmd[600];
    MKDIR(dir);
    for (m = 0; m < 5; m++) {
        sprintf(cmd, "%s/%s", dir, M[m].n);
        MKDIR(cmd);
        for (v = 0; v < (int)(sizeof V / sizeof V[0]); v++) {
            int id = preset_id(M[m].blk, M[m].i);
            fresh();
            dk_lock();
            dk_song.tempo = 84;
            dk_song.dust = 0;
            dk_track_setup(2, 5, id, SQ_LIVE, R_DRUM);
            dk_song.trk[2].vol = 80;
            dk_song.trk[2].pan = 0;
            dk_unlock();
            run(0.05);
            reset_out();
            note(2, V[v].note, 100);
            run(2.0);
            tail(5.0);
            sprintf(fn, "%s/%s/%02d_%s.wav", dir, M[m].n, v, V[v].n);
            wav(fn);
        }
    }
}

/* -------------------------------------------------------------------- songs --- */
static void demo(const char *out)
{
    int laps = 0, last = 0;
    double t = 0.0;
    fresh();
    dk_lock();
    TAPE.demo();
    dk_unlock();
    dk_cmd(C_RETUNE, 0, 0, 0);
    run(0.1);
    reset_out();
    dk_cmd(C_PLAY, 1, 0, 0);
    while (t < 600.0) {
        run(0.1);
        t += 0.1;
        if (dk_road_i < last)
            laps++;
        last = dk_road_i;
        if (laps)
            break;
    }
    dk_cmd(C_PLAY, 0, 0, 0);
    tail(8.0);
    wav(out);
    report("demo FIRST LIGHT");
}

/* the demo's balance: each track alone through the whole song (solo), its loudness against the piano's */
static void balance(void)
{
    static const char *const N[6] = {"DRUMS", "BASS", "PIANO", "PAD", "BELLS", "NATURE"};
    int t;
    for (t = -1; t < DK_NTRACK; t++) {
        char fn[64];
        int laps = 0, last = 0;
        double tt = 0.0;
        fresh();
        dk_lock();
        TAPE.demo();
        if (t >= 0)
            dk_song.trk[t].solo = 1;
        dk_unlock();
        dk_cmd(C_RETUNE, 0, 0, 0);
        run(0.1);
        reset_out();
        dk_cmd(C_PLAY, 1, 0, 0);
        while (tt < 600.0) {
            run(0.1);
            tt += 0.1;
            if (dk_road_i < last)
                laps++;
            last = dk_road_i;
            if (laps)
                break;
        }
        dk_cmd(C_PLAY, 0, 0, 0);
        tail(4.0);
        sprintf(fn, "build/test/stem_%s.wav", t < 0 ? "MIX" : N[t]);
        wav(fn);
        report(t < 0 ? "MIX" : N[t]);
    }
}


/* GROOVE: the drummer through the song: each section's three grooves a bar each, a fill at the end of each section,
 * the ending; on DUSTKIT (track 5), 90 BPM */
static void groove_demo(const char *out, int preset)
{
    double bar;
    int sec, j;
    fresh();
    dk_lock();
    dk_song.tempo = 90;
    dk_song.dust = 0;
    dk_track_setup(4, 5, preset, SQ_GROOVE, R_DRUM);
    dk_song.trk[4].vol = 80;
    dk_unlock();
    bar = 4.0 * 60.0 / 90.0;
    reset_out();
    dk_cmd(C_PLAY, 1, 0, 0);
    dk_cmd(C_GROOVE, 4, 0, 1);
    run(0.02);
    for (sec = 0; sec < 5; sec++)
        for (j = 0; j < 3; j++) {
            int g = sec * 3 + j + 1;
            if (g > 1)
                dk_cmd(C_GROOVE, 4, 0, g);          /* (on the next bar) */
            run(bar * 0.5);
            if (j == 2)                              /* the section's end: a fill, longer each time */
                dk_cmd(C_GROOVE, 4, 1, 2 + sec * 2);
            run(bar * 0.5);
        }
    dk_cmd(C_GROOVE, 4, 0, 0);
    run(bar * 1.2);
    dk_cmd(C_PLAY, 0, 0, 0);
    tail(3.0);
    wav(out);
    report("groove: 5 sections x 3, fills, the end");
}

/* GROOVE by itself: VERSE 2 with AUTO FILL every 4 bars and MUTATE, a DICE, then a new section (a long fill into it) */
static void groove_auto(const char *out)
{
    double bar = 4.0 * 60.0 / 90.0;
    fresh();
    dk_lock();
    dk_song.tempo = 90;
    dk_song.dust = 0;
    dk_track_setup(4, 5, 0, SQ_GROOVE, R_DRUM);
    dk_song.trk[4].vol = 80;
    dk_song.trk[4].drv[0] = 1;               /* a fill every 4 bars */
    dk_song.trk[4].drv[1] = 70;              /* MUTATE */
    dk_song.trk[4].drv[2] = 0;
    dk_unlock();
    reset_out();
    dk_cmd(C_PLAY, 1, 0, 0);
    dk_cmd(C_GROOVE, 4, 0, 5);
    {                                        /* the fills that came by themselves: two in 8 bars */
        int fills = 0, was = 0, i;
        for (i = 0; i < (int)(bar * 8.0 / 0.05); i++) {
            run(0.05);
            if (dk_gr_fill[4] && !was)
                fills++;
            was = dk_gr_fill[4];
        }
        printf("auto fills in 8 bars: %d%s\n", fills, fills == 2 ? "" : "  BAD");
    }
    dk_cmd(C_GROOVE, 4, 3, 0);               /* DICE */
    run(bar * 4.0);
    dk_cmd(C_GROOVE, 4, 0, 8);               /* CHORUS 2: a long fill into it by itself */
    run(bar * 8.0);
    dk_cmd(C_GROOVE, 4, 0, 0);
    run(bar * 1.2);
    dk_cmd(C_PLAY, 0, 0, 0);
    tail(3.0);
    wav(out);
    report("groove auto: fills every 4, mutate 70, dice, a section");
}

static void chords(const char *out)
{
    static const int MODES[3] = {0, 1, 4};               /* MAJOR, MINOR, LYDIAN */
    static const int DEG[4] = {0, 5, 3, 4};
    int m, d, i, prev[8] = {55, 59, 64, 67, 71, 74, 76, 79}, have = 0;
    fresh();
    dk_lock();
    dk_track_setup(2, 0, 1, SQ_LIVE, R_KEYS);
    dk_song.tempo = 76;
    dk_unlock();
    reset_out();
    for (m = 0; m < 3; m++)
        for (d = 0; d < 4; d++) {
            int nt[8], v[10], n, k;
            n = dk_chord(0, MODES[m], DEG[d], CH_MAJ7, 1, 1, nt, 52);
            k = dk_voice(nt, n, VC_OPEN, 1, have ? prev : 0, v);
            for (i = 0; i < k; i++)
                dk_note(2, v[i], 78, i * 1700);            /* strummed, 40 ms */
            run(1.5);
            for (i = 0; i < k; i++)
                note(2, v[i], 0);
            for (i = 0; i < n && i < 8; i++)
                prev[i] = nt[i];
            have = 1;
            run(0.1);
        }
    tail(4.0);
    wav(out);
    report("chords MAJOR MINOR LYDIAN");
}

/* OVERWORLD's ow_test "phrase" (Cmaj7 - Am7 - Fmaj7 - G6, a tune on top): one of its instruments alone on the DECK
 * engine that took it over (0 piano FELT, 1 pad AIRPAD SWEDEN, 2 bell GLASS CELESTA, 3 bass SOFTBASS SUB), the
 * sends as OVERWORLD's DAWN, to compare with ow_test phrase on the same instrument */
static void phrase(int inst, const char *out)
{
    static const float PH[][5] = {
        {0.0f, 3, 36, 70, 3.8f}, {0.0f, 1, 52, 60, 3.8f}, {0.0f, 1, 55, 60, 3.8f}, {0.0f, 1, 59, 60, 3.8f},
        {0.0f, 0, 48, 64, 3.0f}, {0.3f, 0, 55, 52, 2.6f}, {0.6f, 0, 64, 58, 2.0f}, {1.2f, 0, 71, 66, 1.2f},
        {2.0f, 0, 67, 58, 1.8f}, {2.9f, 2, 79, 50, 1.0f},
        {4.0f, 3, 33, 70, 3.8f}, {4.0f, 1, 52, 60, 3.8f}, {4.0f, 1, 55, 60, 3.8f}, {4.0f, 1, 60, 60, 3.8f},
        {4.0f, 0, 45, 64, 3.0f}, {4.3f, 0, 52, 52, 2.6f}, {4.6f, 0, 60, 58, 2.0f}, {5.2f, 0, 72, 66, 1.2f},
        {6.0f, 0, 71, 58, 0.9f}, {6.6f, 0, 67, 60, 1.4f},
        {8.0f, 3, 29, 70, 3.8f}, {8.0f, 1, 53, 60, 3.8f}, {8.0f, 1, 57, 60, 3.8f}, {8.0f, 1, 64, 60, 3.8f},
        {8.0f, 0, 41, 64, 3.0f}, {8.3f, 0, 48, 52, 2.6f}, {8.6f, 0, 57, 58, 2.0f}, {9.2f, 0, 69, 66, 1.4f},
        {10.2f, 0, 72, 60, 1.6f}, {10.9f, 2, 84, 46, 1.2f},
        {12.0f, 3, 31, 70, 4.5f}, {12.0f, 1, 52, 60, 4.5f}, {12.0f, 1, 55, 60, 4.5f}, {12.0f, 1, 62, 60, 4.5f},
        {12.0f, 0, 43, 64, 3.5f}, {12.3f, 0, 50, 52, 3.0f}, {12.6f, 0, 59, 58, 2.5f}, {13.2f, 0, 71, 62, 1.0f},
        {14.0f, 0, 74, 60, 1.0f}, {14.8f, 0, 76, 66, 3.0f},
    };
    /* the sends as OVERWORLD's (its reverb and echo for each instrument, at SPACE 50, ECHO 25) */
    static const int ENG[4] = {0, 2, 3, 4}, PRE[4] = {0, 0, 1, 0}, REV[4] = {82, 100, 97, 20}, ECH[4] = {12, 3, 21, 0};
    int n = (int)(sizeof PH / sizeof PH[0]), i, on[64] = {0};
    double t = 0.0;
    fresh();
    dk_lock();
    dk_track_setup(2, ENG[inst], PRE[inst], SQ_LIVE, R_KEYS);
    dk_song.trk[2].fx = 0;
    dk_song.trk[2].rev = (uint8_t)REV[inst];
    dk_song.trk[2].echo = (uint8_t)ECH[inst];
    dk_song.trk[2].vol = 80;
    dk_song.trk[2].trim = 0;
    dk_song.dust = 0;
    dk_unlock();
    run(0.05);
    reset_out();
    while (t < 20.0) {
        for (i = 0; i < n; i++) {
            if ((int)PH[i][1] != inst)
                continue;
            if (!on[i] && (double)PH[i][0] <= t) {
                note(2, (int)PH[i][2], (int)PH[i][3]);
                on[i] = 1;
            }
            if (on[i] == 1 && (double)(PH[i][0] + PH[i][4]) <= t) {
                note(2, (int)PH[i][2], 0);
                on[i] = 2;
            }
        }
        run(0.01);
        t += 0.01;
    }
    wav(out);
    report("phrase (OVERWORLD's, one instrument)");
#ifdef DK_PROF
    {
        int k;
        static const char *const W[7] = {"", "voices", "bus", "fx", "mix", "sends", "master"};
        for (k = 1; k < 7; k++)
            printf("   %-8s %6.1f us/128\n", W[k], dk_prof[k] * 1e6 / (double)frames * 128.0);
    }
#endif
}


/* FINGER's banks: every pattern of every bank (a drum, a bass, a pad, a melody track) held for two bars on the default
 * engines, one WAV each in <dir>/<BANK>_<ROLE>/pNN.wav; sound_check.py --nolevel checks them. Silence fails the run. */

/* ATMO to listen to: the demo's chorus three times: dry; with RAIN and the FIRE under it; with the TAPE as well */
static void atmo_demo(const char *out)
{
    int pass;
    fresh();
    dk_lock();
    TAPE.demo();
    dk_song.play_mode = 0;
    dk_song.cur_part = 3;
    dk_unlock();
    dk_cmd(C_RETUNE, 0, 0, 0);
    run(0.1);
    reset_out();
    dk_cmd(C_PLAY, 1, 0, 0);
    for (pass = 0; pass < 3; pass++) {
        dk_lock();
        dk_song.atmo[0] = pass ? 45 : 0;     /* RAIN */
        dk_song.atmo[1] = pass ? 35 : 0;     /* FIRE */
        dk_song.tapesim = pass == 2 ? 70 : 0;
        dk_unlock();
        run(10.0);
    }
    wav(out);
}

static void bench(void)
{
    int i, t;
    double worst = 0.0;
    /* the demo's chorus, twice through */
    fresh();
    dk_lock();
    TAPE.demo();
    dk_song.play_mode = 0;
    dk_song.cur_part = 3;
    dk_unlock();
    dk_cmd(C_RETUNE, 0, 0, 0);
    run(0.1);
    reset_out();
    dk_cmd(C_PLAY, 1, 0, 0);
    run(20.0);
    report("bench: the demo's CHORUS");
    /* the same with ATMO's six layers and the tape all the way up (the worst it can be asked) */
    dk_lock();
    for (i = 0; i < 6; i++)
        dk_song.atmo[i] = 100;
    dk_song.tapesim = 100;
    dk_unlock();
    reset_out();
    run(5.0);
    report("bench: + ATMO all six, TAPE 100");
    dk_lock();
    dk_song.tapesim = 0;
    dk_unlock();
    reset_out();
    run(3.0);
    report("bench: + ATMO all six alone");
    dk_lock();
    for (i = 0; i < 6; i++)
        dk_song.atmo[i] = 0;
    dk_song.tapesim = 100;
    dk_unlock();
    run(3.0);
    reset_out();
    run(3.0);
    report("bench: + TAPE 100 alone");
    dk_lock();
    for (i = 0; i < 6; i++)
        dk_song.atmo[i] = 0;
    dk_song.tapesim = 0;
    dk_unlock();
    run(2.0);
#ifdef DK_PROF
    {
        int k;
        static const char *const W[7] = {"", "voices", "bus", "fx", "mix", "sends", "master"};
        for (k = 1; k < 7; k++)
            printf("   %-8s %6.1f us/128\n", W[k], dk_prof[k] * 1e6 / (double)frames * 128.0);
    }
#endif
    /* every voice busy: six tracks, the heaviest presets, chords held, the effects on */
    fresh();
    dk_lock();
    dk_track_setup(0, 5, 3, SQ_LIVE, R_DRUM);
    dk_track_setup(1, 4, 4, SQ_LIVE, R_BASS);
    dk_track_setup(2, 0, 11, SQ_LIVE, R_KEYS);
    dk_track_setup(3, 2, 3, SQ_LIVE, R_PAD);
    dk_track_setup(4, 1, 7, SQ_LIVE, R_KEYS);
    dk_track_setup(5, 6, 8, SQ_LIVE, R_NATURE);
    dk_song.space = 3;
    dk_song.dust = 80;
    dk_unlock();
    run(0.1);
    reset_out();
    note(5, 60, 100);
    for (i = 0; i < 40; i++) {
        for (t = 0; t < 8; t++)
            note(2, 48 + (i * 5 + t * 7) % 36, 90);
        for (t = 0; t < 5; t++)
            note(3, 48 + t * 4 + (i & 3), 80);
        for (t = 0; t < 6; t++)
            note(4, 55 + t * 3 + (i & 7), 80);
        note(1, 36 + (i % 12), 100);
        note(0, 36, 110);
        note(0, 42, 90);
        note(0, 38, 100);
        reset_out();
        run(0.25);
        {
            double us = cost_s * 1e6 / (double)frames * 128.0;
            if (us > worst)
                worst = us;
        }
        for (t = 0; t < 8; t++)
            note(2, 48 + (i * 5 + t * 7) % 36, 0);
        for (t = 0; t < 5; t++)
            note(3, 48 + t * 4 + (i & 3), 0);
        for (t = 0; t < 6; t++)
            note(4, 55 + t * 3 + (i & 7), 0);
    }
    printf("bench: every voice busy: worst %.1f us/128 (voices %d)\n", worst, dk_voices_on);
}


/* 24 voices at once: all six tracks on the heaviest presets, the FX chain on every track, held chords with long releases.
   Checks the pool really fills (24), nothing goes NaN/inf (the output would clip hard or sit on a rail), the peak stays
   under full scale, no DC, and prints the worst block cost. Returns 1 on a failure. */
static int stress24(void)
{
    static const int ENG[6] = {10, 10, 10, 0, 2, 1};            /* MOURNING plucks on three tracks, the rest heavy pads */
    int bad = 0, pass, i, t, fxn = TAPE.nfx;
    for (pass = 0; pass < 3; pass++) {
        double worst = 0.0;
        int maxv = 0, hold, dc_bad = 0;
        long dc = 0;
        fresh();
        dk_lock();
        for (t = 0; t < 6; t++) {
            int eng = pass == 0 ? 10 : ENG[t];
            int pre = eng == 10 ? DK_PBASE + 2 * 12 + 11 : DK_PBASE + ((t + pass) % 3) * 12 + (t * 5 + pass) % 4;
            dk_track_setup(t, eng, pre, SQ_LIVE, t == 0 ? R_DRUM : R_KEYS);
            dk_song.trk[t].fx = (uint8_t)(fxn ? 1 + (t + pass) % (fxn > 1 ? fxn - 1 : 1) : 0);
            dk_song.trk[t].fxamt = 80;
            dk_song.trk[t].rev = 100;
            dk_song.trk[t].echo = 60;
        }
        dk_song.space = 3;
        dk_song.dust = 80;
        dk_song.tapesim = 100;
        dk_unlock();
        run(0.1);
        reset_out();
        for (hold = 0; hold < 30; hold++) {
            for (t = 0; t < 6; t++)
                for (i = 0; i < 5; i++)
                    note(t, 36 + (hold * 3 + t * 7 + i * 4) % 48, 110);
            run(0.12);
            {
                double us = cost_s * 1e6 / (double)frames * 128.0;
                if (us > worst)
                    worst = us;
            }
            if (dk_voices_on > maxv)
                maxv = dk_voices_on;
            for (t = 0; t < 6; t++)
                for (i = 0; i < 5; i++)
                    note(t, 36 + (hold * 3 + t * 7 + i * 4) % 48, 0);
        }
        for (i = 0; i < 2 * frames; i++)
            dc += buf[i];
        dc /= frames ? frames : 1;
        dc_bad = labs(dc) > 600;
        report(pass == 0 ? "stress24: 6 x MOURNING + fx" : pass == 1 ? "stress24: mixed heavy + fx" : "stress24: mixed 2 + fx");
        printf("   voices max %d, worst %.1f us/128, dc %ld\n", maxv, worst, dc);
        if (maxv < 24 || peak >= 32767 || dc_bad) {
            printf("   FAIL stress24 pass %d (voices %d, peak %d, dc %ld)\n", pass, maxv, peak, dc);
            bad = 1;
        }
        quiet_all();
        tail(8.0);
    }
    return bad;
}

#ifdef DK_PROF
static void engbench(void)
{
    int e, i;
    for (e = 0; e < TAPE.neng; e++) {
        const dk_engine_t *eng = TAPE.eng[e];
        int nv = eng->max_voices;
        fresh();
        dk_lock();
        dk_track_setup(2, e, 0, SQ_LIVE, eng->role);
        dk_song.trk[2].fx = 0;
        dk_unlock();
        run(0.05);
        for (i = 0; i < nv; i++)
            note(2, eng->flags & EF_DRUM ? (i & 1 ? 42 : 36) : 48 + i * 5, 100);
        run(0.02);
        reset_out();
        for (i = 0; i < 8; i++)
            dk_prof[i] = 0.0;
        run(1.0);
        printf("%-9s %d voices: %6.1f us/128 (%5.1f a voice)\n", eng->name, dk_voices_on, dk_prof[1] * 1e6 / frames * 128.0,
               dk_prof[1] * 1e6 / frames * 128.0 / (dk_voices_on ? dk_voices_on : 1));
    }
}
#endif
static void metro(void)
{
    uint32_t c0;
    fresh();
    dk_metro.mode = 2;
    dk_cmd(C_PLAY, 1, 0, 0);
    c0 = dk_metro_clicks;
    run(10.0);
    printf("metro: %u clicks in 10 s at %d BPM (expect ~%d), worst offset %u samples\n", dk_metro_clicks - c0,
           dk_song.tempo, (int)(10.0 * dk_song.tempo / 60.0), dk_metro_err_max);
}

/* SOUND's BYPASS: every stage of the chain switched off and on again under a held chord must not click (the biggest step between
 * two samples around the switch against the same sound's own steps just before; the tail's peak is the end of the reverb) */
static int stepmax(int f0, int f1)
{
    int i, m = 0;
    for (i = 2 * f0 + 2; i < 2 * f1; i++) {
        int d = abs((int)buf[i] - (int)buf[i - 2]);
        m = d > m ? d : m;
    }
    return m;
}
static int bypass_test(void)
{
    int bad = 0, st, fx, n = 0;
    for (st = 0; st < 4; st++)
        for (fx = st ? 0 : 1; fx < (st ? 1 : 8); fx++) {
            int t = 2, k, sw[2] = {44100 * 2, 44100 * 4}, ref, jmp[2];
            fresh();
            dk_lock();
            dk_song.trk[t].fx = (uint8_t)fx;
            dk_song.trk[t].fxamt = 80;
            dk_song.trk[t].fxa = 60;
            dk_song.trk[t].fxb = 50;
            dk_song.trk[t].echo = 70;
            dk_song.trk[t].rev = 70;
            dk_song.tapesim = 70;
            dk_unlock();
            reset_out();
            for (k = 0; k < 4; k++)
                note(t, 48 + 4 * k, 96);
            run(1.9);
            ref = stepmax(44100 / 2, 44100 * 19 / 10);
            run(0.1);
            dk_lock();
            if (st == 3) dk_song.rsv[DK_NTRACK] |= 1; else dk_song.rsv[t] |= (uint8_t)(1u << st);
            dk_unlock();
            run(1.9);
            jmp[0] = stepmax(sw[0] - 64, sw[0] + 2048);
            run(0.1);
            dk_lock();
            if (st == 3) dk_song.rsv[DK_NTRACK] = 0; else dk_song.rsv[t] = 0;
            dk_unlock();
            run(1.9);
            jmp[1] = stepmax(sw[1] - 64, sw[1] + 2048);
            n++;
            if (jmp[0] > 2 * ref + 300 || jmp[1] > 2 * ref + 300 || peak > 29200) {
                printf("CLICK stage %d fx %d: step %d / %d (own %d) peak %d\n", st, fx, jmp[0], jmp[1], ref, peak);
                bad++;
            }
        }
    printf("bypass: %d stage/effect pairs switched off and on, %d clicks\n", n, bad);
    return bad ? 1 : 0;
}

/* AUTOMATION: a curve on every target of a track, a sawtooth with a very steep edge (the worst a hand can do), over a held
 * chord and a note pattern: no click (the biggest step between two samples against the same sound with no automation), the
 * peak under -1 dBFS, no DC, not silent. The curve is made as the UI makes it (a ramp event per point) */
static int auto_test(void)
{
    static const int TG[] = {T_P0, T_P1, T_P2, T_P3, T_P4, T_P5, T_ATT, T_DEC, T_REL, T_FXAMT, T_FXA, T_FXB, T_REV, T_ECHO, T_VOL, T_PAN};
    int bad = 0, n = 0, e, i, k;
    for (e = 0; e < 3; e++)
        for (k = 0; k < (int)(sizeof TG / sizeof TG[0]); k++) {
            int t = 2, pass, st[2], pk[2], dc[2];
            for (pass = 0; pass < 2; pass++) {
                int c = CLIP(0, t) - dk_song.clip;
                double sum = 0.0;
                fresh();
                dk_lock();
                dk_track_setup(t, e * 3 % TAPE.neng, 0, SQ_LIVE, dk_engine(t) ? dk_engine(t)->role : 0);
                dk_song.trk[t].fx = (uint8_t)(1 + e);
                dk_song.trk[t].fxamt = 60;
                dk_song.trk[t].echo = 40;
                dk_song.trk[t].rev = 40;
                dk_song.clip[c].len = DK_TPB * 2;
                for (i = 0; i < 16; i++) {
                    dk_ev_t ev;
                    ev.pos = (uint16_t)(i * 48);
                    ev.dur = 40;
                    ev.note = (uint8_t)(48 + (i * 5) % 17);
                    ev.vel = 90;
                    ev.fl = 0;
                    ev.layer = 0;
                    dk_song_insert(c, &ev);
                }
                if (pass) {
                    for (i = 0; i < 9; i++) {           /* up in a bar, a steep fall, up again, down */
                        static const int V[9] = {0, 40, 100, 100, 5, 5, 60, 100, 0};
                        dk_ev_t ev;
                        ev.pos = (uint16_t)(i < 4 ? i * 96 : i == 4 ? 4 * 96 + 24 : 4 * 96 + 24 + (i - 4) * 90);
                        ev.dur = 0;
                        ev.note = (uint8_t)TG[k];
                        ev.vel = (uint8_t)V[i];
                        ev.fl = EV_MOTION | EV_RAMP;
                        ev.layer = 0;
                        dk_song_insert(c, &ev);
                    }
                }
                dk_unlock();
                dk_cmd(C_PLAY, 1, 0, 0);
                run(2.0);
                reset_out();
                run(10.0);
                st[pass] = stepmax(0, frames);
                pk[pass] = peak;
                for (i = 0; i < 2 * frames; i++)
                    sum += buf[i];
                dc[pass] = (int)(sum / (2.0 * frames));
                if (pass && peak < 200) {
                    printf("SILENT engine %d target %d\n", e, TG[k]);
                    bad++;
                }
            }
            n++;
            if (getenv("AUTO_VERBOSE"))
                printf("engine %d target %2d: step %5d (plain %5d) peak %5d (plain %5d) dc %d\n", e, TG[k], st[1], st[0], pk[1], pk[0], dc[1]);
            if (st[1] > 2 * st[0] + 600 || pk[1] > 29200 || abs(dc[1]) > 300) {
                printf("BAD engine %d target %d: step %d (plain %d) peak %d dc %d\n", e, TG[k], st[1], st[0], pk[1], dc[1]);
                bad++;
            }
        }
    printf("automation: %d tracks x curves rendered, %d bad\n", n, bad);
    return bad ? 1 : 0;
}


/* ------------------------------------------------------------------ pitch --- */
/* the fundamental of the note just played (frames from f0 on, 0.7 s of it): the autocorrelation's first strong peak between
 * 25 Hz and 1 kHz; conf (0..1) how periodic it is (a noise or a bell is not) */
static double fundamental(int f0, double *conf)
{
    int n = 30870, lo = 44, hi = 1764, lag, best = 0, i;     /* 0.7 s; the lags of 1 kHz .. 25 Hz */
    double e0 = 0.0, bestv = -1.0, v[1800];
    static double x[30870 + 1800];
    if (f0 + n + hi > frames)
        n = frames - f0 - hi;
    if (n < 4000) {
        *conf = 0.0;
        return 0.0;
    }
    for (i = 0; i < n + hi; i++)
        x[i] = 0.5 * ((double)buf[2 * (f0 + i)] + (double)buf[2 * (f0 + i) + 1]);
    for (i = 0; i < n; i++)
        e0 += x[i] * x[i];
    if (e0 < 1.0) {
        *conf = 0.0;
        return 0.0;
    }
    for (lag = lo; lag <= hi; lag++) {
        double c = 0.0, e1 = 0.0;
        for (i = 0; i < n; i++) {
            c += x[i] * x[i + lag];
            e1 += x[i + lag] * x[i + lag];
        }
        v[lag - lo] = c / sqrt(e0 * e1 + 1e-9);
        if (v[lag - lo] > bestv)
            bestv = v[lag - lo];
    }
    for (lag = lo + 1; lag < hi; lag++)                        /* the first peak within 5 % of the best (not its multiples) */
        if (v[lag - lo] >= 0.95 * bestv && v[lag - lo] >= v[lag - 1 - lo] && v[lag - lo] >= v[lag + 1 - lo]) {
            best = lag;
            break;
        }
    *conf = bestv;
    if (best <= lo || best >= hi)
        return 0.0;
    {   /* the parabola through the peak: the lag between the samples */
        double a = v[best - 1 - lo], b = v[best - lo], c = v[best + 1 - lo], d = a - 2.0 * b + c;
        return 44100.0 / ((double)best + (d != 0.0 ? 0.5 * (a - c) / d : 0.0));
    }
}
/* every BASS-role preset (all blocks): the notes C2, G2, C3 sound at their pitch (a quarter tone); every other melodic engine's
 * presets: a report line (the octave off by more than one in a periodic sound is an error too). Written to the output. */

/* the time from a key to its sound: each preset, one note from silence, rendered alone. `first`: the first sample over
 * -60 dBFS; `-20`: the level reaches a tenth of the note's own peak (what the ear calls "it started"). A preset whose
 * -20 point is later than LAT_SLOW ms is listed as SLOW (by design only for pads, swells, bowed sounds). */
#define LAT_SLOW 25.0
static int latency_test(int verbose)
{
    int e, p, slow = 0, n = 0;
    double worst = 0.0;
    for (e = 0; e < TAPE.neng; e++) {
        const dk_engine_t *eng = TAPE.eng[e];
        if (eng->flags & EF_CONT)
            continue;
        for (p = 0; p < DK_NBLK * DK_NFACT; p++) {
            int blk = p / DK_NFACT, id = preset_id(blk, p % DK_NFACT), i, pk = 0, pk0, i0 = -1, i20 = -1;
            int nn = eng->flags & EF_DRUM ? 36 : eng->role == R_BASS ? 36 : 60;
            dk_preset_t tmp;
            const dk_preset_t *pr;
            double t0, t20;
            if (p % DK_NFACT >= preset_count(e, blk))
                continue;
            pr = preset_of(e, id, &tmp);
            fresh();
            dk_lock();
            dk_song.dust = 0;
            dk_track_setup(2, e, id, SQ_STEP, eng->role);
            dk_song.trk[2].vol = 80;
            dk_unlock();
            run(2.0);
            reset_out();
            run(0.05);
            for (i = 0; i < 2 * frames; i++)
                if (abs(buf[i]) > pk) pk = abs(buf[i]);
            pk0 = pk;                                  /* (a preset's own crackle, VINYL, or hiss under the note) */
            pk = 0;
            reset_out();
            note(2, nn, 100);
            run(0.6);
            note(2, nn, 0);
            for (i = 0; i < 2 * frames; i++)
                if (abs(buf[i]) > pk) pk = abs(buf[i]);
            for (i = 0; i < 2 * frames; i++) {
                int a = abs(buf[i]);
                if (i0 < 0 && a > 33 && a > 2 * pk0)
                    i0 = i / 2;
                if (a * 10 >= pk && a > 3 * pk0) {
                    i20 = i / 2;
                    break;
                }
            }
            t0 = i0 < 0 ? -1.0 : i0 / 44.1;
            t20 = i20 < 0 ? -1.0 : i20 / 44.1;
            n++;
            if (t20 > worst && eng->role != R_PAD)
                worst = t20;
            if (verbose || t20 > LAT_SLOW || t20 < 0.0)
                printf("%-5s %-9s %-22s first %5.1f ms  -20dB %6.1f ms  peak %5d\n", t20 > LAT_SLOW || t20 < 0 ? "SLOW" : "",
                       eng->name, pr->name, t0, t20, pk);
            if ((t20 > LAT_SLOW || t20 < 0.0) && eng->role != R_PAD)
                slow++;
        }
    }
    printf("%d presets, %d slow (pads not counted), worst non-pad %.1f ms\n", n, slow, worst);
    return 0;
}
static int pitch_test(void)
{
    static const int N[3] = {36, 43, 48};
    int e, p, bad = 0, checked = 0;
    for (e = 0; e < TAPE.neng; e++) {
        const dk_engine_t *eng = TAPE.eng[e];
        if ((eng->flags & (EF_DRUM | EF_CONT)))
            continue;
        for (p = 0; p < DK_NBLK * DK_NFACT; p++) {
            int blk = p / DK_NFACT, id = preset_id(blk, p % DK_NFACT), j;
            dk_preset_t tmp;
            const dk_preset_t *pr;
            if (p % DK_NFACT >= preset_count(e, blk))
                continue;
            pr = preset_of(e, id, &tmp);
            for (j = 0; j < (eng->role == R_BASS ? 3 : 1); j++) {
                double want, got, conf, cents;
                int nn = eng->role == R_BASS ? N[j] : eng->role == R_BELL ? 60 : 48;
                fresh();
                dk_lock();
                dk_song.tempo = 84;
                dk_song.dust = 0;
                dk_track_setup(2, e, id, SQ_LIVE, eng->role);
                dk_song.trk[2].vol = 80;
                dk_song.trk[2].pan = 0;
                dk_unlock();
                run(0.05);
                reset_out();
                want = 440.0 * pow(2.0, (nn - 69) / 12.0);
                note(2, nn, 100);
                run(1.2);
                got = fundamental(2000, &conf);
                note(2, nn, 0);
                run(0.3);
                cents = got > 0.0 ? 1200.0 * log2(got / want) : 0.0;
                if (eng->role == R_BASS) {                     /* at its pitch, or a sub-octave (SUB DRONE, RUMBLE) by design; never above */
                    int ok = got > 0.0 && (fabs(cents) <= 50.0 || fabs(cents + 1200.0) <= 50.0 || fabs(cents + 2400.0) <= 50.0);
                    if (!ok) {
                        printf("BAD   %s %s note %d: %.1f Hz, wanted %.1f (%+.0f cents, periodicity %.2f)\n", eng->name, pr->name,
                               nn, got, want, cents, conf);
                        bad++;
                    } else if (fabs(cents) > 50.0)
                        printf("sub   %s %s note %d: %.1f Hz (%+.0f cents, by design)\n", eng->name, pr->name, nn, got, cents);
                    checked++;
                } else if (conf > 0.9 && got > 0.0 && fabs(cents) > 50.0)   /* the others: a list to look at, not an error */
                    printf("info  %s %s note %d: %.1f Hz, wanted %.1f (%+.0f cents)\n", eng->name, pr->name, nn, got, want, cents);
            }
        }
    }
    printf("%d bass notes checked, %d bad\n", checked, bad);
    return bad != 0 || checked == 0;
}

/* the effects' delay lines past 2^24 samples: a float write position lost its fraction (steps of 2, then 4 samples) and a
 * swaying delay (TAPE, CHORUS, ...) turned to a staircase after about six minutes of sound. A held LEAD note through each
 * insert effect: its top (the second difference against the signal) a second in, and again after ~7 minutes. */
static double hf_ratio(void)
{
    double a = 0.0, b = 0.0;
    int i;
    for (i = 2; i < frames; i++) {
        double x = buf[2 * i], d = x - 2.0 * buf[2 * (i - 1)] + buf[2 * (i - 2)];
        a += d * d;
        b += x * x;
    }
    return a / (b + 1.0);
}
static int fxlong_test(void)
{
    int e, fx, bad = 0;
    for (e = 0; e < TAPE.neng && strcmp(TAPE.eng[e]->name, "LEAD"); e++)
        ;
    if (e == TAPE.neng)
        e = 0;
    for (fx = 1; fx < TAPE.nfx; fx++) {
        double early, late;
        int i;
        fresh();
        dk_lock();
        dk_song.tempo = 84;
        dk_song.dust = 0;
        dk_track_setup(2, e, 0, SQ_LIVE, TAPE.eng[e]->role);
        dk_song.trk[2].vol = 80;
        dk_song.trk[2].pan = 0;
        dk_song.trk[2].fx = (uint8_t)fx;
        dk_song.trk[2].fxamt = 40;
        dk_song.trk[2].fxa = 30;
        dk_song.trk[2].fxb = 30;
        dk_unlock();
        note(2, 48, 100);
        run(1.0);
        reset_out();
        run(1.0);
        early = hf_ratio();
        for (i = 0; i < 70; i++) {                              /* 70 x 6 s: past 2^24 samples (380 s) */
            reset_out();
            run(6.0);
        }
        reset_out();
        run(1.0);
        late = hf_ratio();
        if (late > 2.5 * early + 1e-6) {
            printf("BAD   fx %s: the top after 7 minutes is %.1fx the first second's\n", TAPE.fx[fx]->name, late / (early + 1e-12));
            bad++;
        }
        printf("fx %-8s top %.2e -> %.2e\n", TAPE.fx[fx]->name, early, late);
    }
    return bad;
}

int main(int argc, char **argv)
{
    const char *m = argc > 1 ? argv[1] : "sounds";
    dk_init();
    if (!strcmp(m, "sounds"))
        sounds(argc > 2 ? argv[2] : "out/sounds", argc > 3 ? argv[3] : 0);
    else if (!strcmp(m, "kitvoices"))
        kitvoices(argc > 2 ? argv[2] : "out/kits");
    else if (!strcmp(m, "demo"))
        demo(argc > 2 ? argv[2] : "out/demo.wav");
    else if (0) {
    } else if (!strcmp(m, "gmlev")) {                 /* the kit's GM notes, each alone: how loud (the grooves' balance) */
        static const int N[13] = {36, 38, 37, 39, 42, 46, 45, 47, 50, 49, 51, 70, 76};
        int i, j;
        for (i = 0; i < 13; i++) {
            fresh();
            dk_lock();
            dk_song.dust = 0;
            dk_track_setup(4, 5, argc > 2 ? atoi(argv[2]) : 0, SQ_STEP, R_DRUM);
            dk_unlock();
            run(0.1);
            reset_out();
            note(4, N[i], 100);
            run(0.05);
            note(4, N[i], 0);
            run(0.6);
            j = peak;
            printf("%d:%.1f ", N[i], 20.0 * log10((j + 1) / 32768.0));
        }
        printf("\n");
    } else if (!strcmp(m, "groove"))
        groove_demo(argc > 2 ? argv[2] : "out/groove.wav", argc > 3 ? atoi(argv[3]) : 0);
    else if (!strcmp(m, "grooveauto"))
        groove_auto(argc > 2 ? argv[2] : "out/groove_auto.wav");
    else if (!strcmp(m, "chords"))
        chords(argc > 2 ? argv[2] : "out/chords.wav");
    else if (!strcmp(m, "phrase") && argc > 3)
        phrase(atoi(argv[2]) & 3, argv[3]);
    else if (!strcmp(m, "balance"))
        balance();
    else if (!strcmp(m, "atmo"))
        atmo_demo(argc > 2 ? argv[2] : "out/atmo.wav");
    else if (!strcmp(m, "bypass"))
        return bypass_test();
    else if (!strcmp(m, "automation"))
        return auto_test();
    else if (!strcmp(m, "bench"))
        bench();
#ifdef DK_PROF
    else if (!strcmp(m, "engbench"))
        engbench();
#endif
    else if (!strcmp(m, "metro"))
        metro();
    else if (!strcmp(m, "latency"))
        return latency_test(argc > 2);
    else if (!strcmp(m, "pitch"))
        return pitch_test();
    else if (!strcmp(m, "fxlong"))
        return fxlong_test();
    else if (!strcmp(m, "stress24"))
        return stress24();
    else {
        fprintf(stderr, "deck_test sounds|demo|chords|phrase|bench|metro\n");
        return 2;
    }
    return 0;
}
