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
static void sounds(const char *dir)
{
    int e, p;
    char fn[512], cmd[600];
    for (e = 0; e < TAPE.neng; e++) {
        const dk_engine_t *eng = TAPE.eng[e];
        MKDIR(dir);
        sprintf(cmd, "%s/%s", dir, eng->name);
        MKDIR(cmd);
        for (p = 0; p < DK_NFACT; p++) {
            const dk_preset_t *pr = &TAPE.presets[e * DK_NFACT + p];
            char nm[40];
            int i, t = 2;
            fresh();
            dk_lock();
            dk_song.tempo = 84;
            dk_song.dust = 0;
            dk_track_setup(t, e, p, SQ_LIVE, eng->role);
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
            sprintf(fn, "%s/%s/%02d_%s.wav", dir, eng->name, p + 1, nm);
            wav(fn);
            sprintf(cmd, "%s %s", eng->name, pr->name);
            report(cmd);
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

static void drive_all(int mood, int style, double secs, const char *out)
{
    int t;
    fresh();
    dk_lock();
    song_mood(mood);
    if (style >= 0)
        dk_song.style = (uint8_t)style;
    for (t = 0; t < DK_NTRACK; t++) {
        dk_song.trk[t].seq = SQ_DRIVE;
        dk_song.trk[t].drv[0] = 55;
        dk_song.trk[t].drv[1] = dk_song.style;
        dk_song.trk[t].drv[2] = 50;
    }
    dk_unlock();
    dk_cmd(C_SEED, 0, 0, 1234 + mood * 7 + style);
    dk_cmd(C_RETUNE, 0, 0, 0);
    run(0.1);
    reset_out();
    dk_cmd(C_PLAY, 1, 0, 0);
    run(secs);
    dk_cmd(C_PLAY, 0, 0, 0);
    tail(12.0);
    wav(out);
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
static int fingerbank(const char *dir)
{
    static const struct { int t; const char *tag; } TR[4] = {{4, "DRUM_DUSTKIT"}, {3, "BASS"}, {2, "PAD"}, {0, "MEL"}};
    int b, k, p, bad = 0, n = 0;
    char cmd[256], fn[320];
    for (b = 0; b < DK_NFBANK; b++)
        for (k = 0; k < 4; k++) {
            int t = TR[k].t;
            snprintf(fn, sizeof fn, "%s/%s_%s", dir, dk_fb_bankname(b), TR[k].tag);
            snprintf(cmd, sizeof cmd, "mkdir -p '%s'", fn);
            if (system(cmd))
                return 2;
            for (p = 0; p < 16; p++) {
                double bar;
                int v, vmax;
                fresh();
                dk_lock();
                dk_song.trk[t].seq = SQ_FINGER;
                dk_song.fbank[t] = (uint8_t)b;
                dk_song.dust = 0;
                dk_unlock();
                bar = 4.0 * 60.0 / (dk_song.tempo > 0 ? dk_song.tempo : 100);
                run(0.05);
                reset_out();
                dk_cmd(C_FINGER, t, 1, p);
                for (vmax = 0, v = 0; v < 20; v++) {       /* two bars, the busiest moment's voices counted */
                    run(0.1 * bar);
                    vmax = (int)dk_voices_on > vmax ? (int)dk_voices_on : vmax;
                }
                dk_cmd(C_FINGER, t, 0, p);
                tail(8.0);
                snprintf(fn, sizeof fn, "%s/%s_%s/p%02d.wav", dir, dk_fb_bankname(b), TR[k].tag, p + 1);
                wav(fn);
                n++;
                if (vmax >= DK_NVOICE) {
                    printf("VOICES FULL %s\n", fn);
                    bad++;
                }
                if (peak < 200) {
                    printf("SILENT %s (peak %d)\n", fn, peak);
                    bad++;
                }
            }
        }
    printf("fingerbank: %d patterns rendered, %d silent\n", n, bad);
    return bad ? 1 : 0;
}

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

int main(int argc, char **argv)
{
    const char *m = argc > 1 ? argv[1] : "sounds";
    dk_init();
    if (!strcmp(m, "sounds"))
        sounds(argc > 2 ? argv[2] : "out/sounds");
    else if (!strcmp(m, "demo"))
        demo(argc > 2 ? argv[2] : "out/demo.wav");
    else if (!strcmp(m, "drive") && argc > 4) {
        int st = atoi(argv[2]), mood = st == 0 ? 0 : st == 1 ? 7 : st == 2 ? 3 : 6;
        drive_all(mood, st, atof(argv[3]), argv[4]);
        report(TAPE.style_name[st]);
    } else if (!strcmp(m, "mood") && argc > 4) {
        drive_all(atoi(argv[2]), -1, atof(argv[3]), argv[4]);
        report(TAPE.moods[atoi(argv[2]) % TAPE.nmood].name);
    } else if (!strcmp(m, "chords"))
        chords(argc > 2 ? argv[2] : "out/chords.wav");
    else if (!strcmp(m, "phrase") && argc > 3)
        phrase(atoi(argv[2]) & 3, argv[3]);
    else if (!strcmp(m, "balance"))
        balance();
    else if (!strcmp(m, "atmo"))
        atmo_demo(argc > 2 ? argv[2] : "out/atmo.wav");
    else if (!strcmp(m, "fingerbank"))
        return fingerbank(argc > 2 ? argv[2] : "out/fingerbanks");
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
    else {
        fprintf(stderr, "deck_test sounds|demo|drive|mood|chords|phrase|fingerbank|bench|metro\n");
        return 2;
    }
    return 0;
}
