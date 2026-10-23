/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK: the shell's contract with a tape (a cassette of sounds) and the song's data, shared by the audio core
 * (core.c, -O2), the tape's sound (tapes/NAME/tape_dsp.c, -O2) and the UI (the unity build).
 *
 * A tape gives engines (voices that know nothing of the sequencer or the screen), track effects, the master effect,
 * presets, the DRIVE styles (a generator per role), FINGER patterns, borrowed chords, performance effects, moods and a
 * demo song. DECK gives the clock, six tracks, the voices' pool, the sequencer types, the keys' harmony, the mixer,
 * the sends (tape echo, plate) and the screen.
 *
 * Threads: dk_render() runs in the audio interrupt. The UI posts commands (dk_cmd); it edits the song's data itself,
 * between dk_lock() and dk_unlock(): the render postpones the sequencer (never the sound) while the song is locked. */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* big zeroed buffers: the device's .pool RAM */
#if defined DK_HOST || defined PLAT_HOST
#define DK_POOL
#else
#define DK_POOL __attribute__((section(".pool")))
#endif

#define DK_SR 44100.0f
#define DK_PPQ 96                       /* sequencer ticks a quarter note */
#define DK_T16 (DK_PPQ / 4)             /* a sixteenth */
#define DK_TPB (4 * DK_PPQ)             /* a bar (4/4) */
#define DK_NTRACK 6
#define DK_NPART 8
#define DK_NCLIP (DK_NPART * DK_NTRACK)
#define DK_NEV 4096                     /* events in a song (notes and motion) */
#define DK_NROAD 64                     /* blocks on the road */
#define DK_NFACT 12                     /* factory presets an engine; + 4 the user's = 16 slots, the white keys */
#define DK_NSLOT 16
#define DK_NP 8                         /* engine parameters (the first three on the ENGINE tab) */
#define DK_NVOICE 24                    /* the voices' pool, shared by the tracks */
#define DK_VSTATE 76                    /* an engine's voice state, floats */
#define DK_BLK 128
#define DK_FXBUF 1024                   /* a track effect's delay line, 16-bit samples */

/* ------------------------------------------------------------------ engines --- */
enum { EF_POLY = 1, EF_MONO = 2, EF_DRUM = 4, EF_CONT = 8 };   /* EF_CONT: a sound that goes on (the world) */
/* the pictures a parameter is drawn as (the tape may draw its own; these are DECK's) */
enum { VZ_BAR, VZ_SUN, VZ_WAVE, VZ_ENV, VZ_TYPE, VZ_DROP, VZ_DUST, VZ_SWELL, VZ_BELL, VZ_GLIDE, VZ_WOBBLE, VZ_SPACE };

typedef struct {
    const char *name;
    int16_t lo, hi, def;
    uint8_t viz;
    const char *const *names;           /* an enum's value names (or 0: a number) */
} dk_param_t;

/* p: the voice's parameters as numbers in their units: p[0..7] the engine's, p[8..10] ENV (ATTACK, DECAY, RELEASE,
 * 0..100, 50 the engine's own), p[11] the track's drift (0..100) */
enum { PX_ATT = 8, PX_DEC, PX_REL, PX_DRIFT, PX_N };
typedef struct {
    const char *name;
    uint8_t flags, max_voices, cost, nparams;
    const dk_param_t *params;
    /* a note: st is the voice's state (zeroed for a new voice; kept, retrig 1, when a MONO voice is reused) */
    void (*on)(float *st, int note, float vel, const float *p, int retrig);   /* retrig 2: legato */
    void (*off)(float *st, const float *p);              /* the key let go */
    int (*render)(float *st, float *l, float *r, int n, const float *p);   /* adds; 0 = finished */
    float (*level)(const float *st);                     /* how loud it is now (stealing, shedding) */
    void (*bus)(float *bs, float *l, float *r, int n, const float *p);   /* the track's sum (or 0); bs: 16 floats */
    uint8_t nlanes;                                      /* DRUM: its lanes (the STEP grid's rows) */
    const char *const *lane_name;
    const uint8_t *lane_note;
    uint8_t role;                                        /* the role it plays by default (R_*) */
} dk_engine_t;

/* roles (for DRIVE and FINGER) */
enum { R_DRUM, R_BASS, R_KEYS, R_ARP, R_PAD, R_LEAD, R_BELL, R_NATURE, R_N };

/* what a preset's MACRO turns (and the modulators' targets): an engine parameter, ENV, the effect, the sends */
enum { T_NONE, T_P0, T_P1, T_P2, T_P3, T_P4, T_P5, T_P6, T_P7, T_ATT, T_DEC, T_REL, T_FXAMT, T_FXA, T_FXB, T_REV, T_ECHO,
       T_VOL, T_PAN, T_SWING, T_N };

typedef struct {
    const char *name;
    uint8_t eng;
    int8_t p[DK_NP];
    int8_t env[3];
    uint8_t fx, fxamt, fxa, fxb;
    uint8_t rev, echo;
    uint8_t mt[3];                      /* MACRO's targets (T_*) and amounts (-100..100 at full) */
    int8_t ma[3];
    int8_t vol;                         /* a level trim, -12..+12 (dB / 2) */
} dk_preset_t;

/* ------------------------------------------------------------------ effects --- */
typedef struct {
    float f[28];
    int16_t *buf;
    uint32_t w;
} dk_fxst_t;
typedef struct {
    const char *name, *pa, *pb;         /* its two knobs' names (with AMOUNT) */
    void (*process)(dk_fxst_t *s, float *l, float *r, int n, float amt, float a, float b);   /* 0..1 each */
} dk_fx_t;

/* ------------------------------------------------------------------ the song --- */
enum { SQ_STEP, SQ_LIVE, SQ_ENDLESS, SQ_ARP, SQ_DRIVE, SQ_FINGER, SQ_HOLD, SQ_TOMBOLA, SQ_N };

/* an event: a note, or (EV_MOTION) a knob's move: note = its target (T_*), vel = the value 0..100, dur 0 a move,
 * dur > 0 a p-lock held that long */
enum { EV_PROB = 3, EV_RATCH = 3 << 2, EV_SLIDE = 1 << 4, EV_MOTION = 1 << 6, EV_FRESH = 1 << 7 };
typedef struct {
    uint16_t pos, dur;                  /* ticks into the clip; length */
    uint8_t note, vel, fl, layer;       /* fl: probability 0..3 (100 75 50 25 %), ratchet (x1..x4), slide; layer: take */
} dk_ev_t;

typedef struct {
    uint16_t at, n;                     /* its events: dk_ev[at .. at + n), sorted by pos */
    uint16_t len;                       /* ticks (a clip shorter than its part goes round) */
    uint8_t layers, rsv;
} dk_clip_t;

typedef struct {
    uint8_t eng, preset, seq, fx;
    int8_t p[DK_NP];
    int8_t env[3];
    int8_t fxamt, fxa, fxb;
    int8_t macro;                       /* 0..100 */
    uint8_t mt[3];
    int8_t ma[3];
    int8_t trim;
    uint8_t vol, rev, echo;
    int8_t pan;
    uint8_t mute, solo, midi_ch;
    int8_t oct;
    uint8_t mod[2][4];                  /* type (MOD_*), depth 0..100, rate 0..100 (FOLLOW: the source track), target T_* */
    uint8_t swing, drift;
    uint8_t drv[3];                     /* DRIVE: ENERGY, STYLE, VARIATION */
    uint8_t arp[4];                     /* ARP: mode, rate (ARPD_*: 1/4 1/8 1/8T 1/16 1/16T 1/32), octaves, latch */
    uint8_t tmb[4];                     /* TOMBOLA: SPIN, GRAVITY, BOUNCE, QUANT */
    uint8_t div;                        /* ENDLESS: the division (ticks / 6) */
    uint8_t qnt;                        /* LIVE: quantize strength 0..100 */
    uint8_t role;
    uint8_t arp_rhy;                    /* ARP: its rhythm (ARPR_*) */
    uint8_t arp_opt;                    /* ARP: the gate (ARPG_*) | the source << 2 (0 the keys, 1 the song's chord) */
    uint8_t prev_seq;                   /* the sequencer before ARP + 1 (the ARP button turns it back), 0 none */
} dk_track_t;

enum { ARPR_STRAIGHT, ARPR_PULSE, ARPR_OFFBEAT, ARPR_DOTTED, ARPR_GALLOP, ARPR_RISE, ARPR_SKIP, ARPR_N };
enum { ARPG_NORMAL, ARPG_SHORT, ARPG_LONG, ARPG_TIE, ARPG_N };
#define ARPO_ON 8u                      /* arp_opt: the ARP on, over the track's sequencer (keys -> the arpeggio) */
#define dk_arp_on(k) (((k)->arp_opt & ARPO_ON) != 0)
#define DK_ARP_NDIV 6
enum { MOD_OFF, MOD_SWING, MOD_DICE, MOD_PEDAL, MOD_FOLLOW, MOD_TREMOLO, MOD_N };
enum { TR_HALF = 1, TR_DOUBLE = 2, TR_REVERSE = 4, TR_UP = 8, TR_DOWN = 16, TR_STOP = 32 };
typedef struct {
    uint8_t part, reps, tricks, drop;   /* drop: tracks silenced on this block (a bit each) */
} dk_block_t;
typedef struct {                        /* a drum sound's own settings (0 = as the kit) */
    int8_t level, pan, tune, decay;     /* level: half dB (-24..+12); pan -50..50; tune, decay: offsets to p[0], p[1] */
    uint8_t rev, echo, crush, rsv;      /* extra sends 0..100; crush: bits and rate down, 0..100 */
} dk_lane_t;
typedef struct {
    uint8_t label, bars, rsv[2];        /* label: INTRO VERSE PRE CHORUS DROP BRIDGE BREAK OUTRO; bars 1..16 */
} dk_part_t;

#define DK_MAGIC 0x4B434544u            /* "DECK" */
#define DK_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    char name[12];
    uint8_t tempo, swing, root, mode;
    uint8_t layout, white, black, chord, quality, voicing, bass, lead, strum, split, koct, dim;
    uint8_t harm_track;                 /* SPLIT: where the chords go */
    uint8_t mood, style, dust, sleep, sidechain, space, echo_time;
    uint8_t play_mode, cur_part, cur_track, tape;
    uint8_t nroad, biome;
    uint16_t nev;
    dk_track_t trk[DK_NTRACK];
    dk_part_t part[DK_NPART];
    dk_clip_t clip[DK_NCLIP];
    dk_block_t road[DK_NROAD];
    uint8_t ball[12], nball, ball_trk;  /* TOMBOLA's balls (their notes) */
    float garden[4];                    /* the screen's world (the tape's): its day, its flower */
    uint8_t rsv[16];
    /* (added 2026-10: an older project ends here, DK_SONG_V1; the rest loads as zeros, which are the defaults) */
    dk_lane_t lane[DK_NTRACK][8];       /* a drum track's sounds, each its own */
    uint8_t atmo[8];                    /* ATMO: the layers' levels 0..100 (the tape's atmo list) */
    uint8_t tapesim, atmo_lvl, rsv2[6]; /* ATMO: the tape emulation 0..100; the layers' master level (0 = 100) */
} dk_song_t;
#define DK_SONG_V1 ((uint32_t)offsetof(dk_song_t, lane))

extern dk_song_t dk_song;
extern dk_ev_t dk_ev[DK_NEV];
#define CLIP(part, trk) (&dk_song.clip[(part) * DK_NTRACK + (trk)])

/* ------------------------------------------------------------------ the tape --- */
typedef struct {
    const char *name;
    uint8_t tempo, mode, style, biome, world, dust, space;
    int8_t eng[DK_NTRACK];              /* the tracks' engines (-1: as they are) and presets */
    int8_t preset[DK_NTRACK];
} dk_mood_t;

/* DRIVE: the tape's generator for a role, a tick at a time. h: the harmony; emit notes with dk_emit */
typedef struct {
    uint8_t pcs[4], n;                  /* the chord now: pitch classes, its root first */
    uint8_t deg;                        /* its scale degree */
    uint8_t root, mode;                 /* the key */
    uint8_t fresh;                      /* the chord changed this tick */
    uint8_t user;                       /* you played it */
    uint32_t heard_at;                  /* tick of the last note you played (call and response) */
    int8_t heard[8];
    uint8_t nheard;
} dk_harm_t;
typedef struct {
    uint8_t trk, role, style, energy, var;
    uint32_t tick;                      /* ticks since the transport started */
    uint32_t bar_tick;                  /* in the bar */
    uint8_t fill, breath, part_end;     /* a fill bar; DRIVE breathes (silent bar); the part's last bar */
} dk_drive_t;

/* a borrowed chord (the last four black keys, DRIVE's colour): semitones from the key's root to its root, then its
 * notes from there (99 ends) */
typedef struct {
    int8_t off;
    int8_t iv[7];
} dk_borrow_t;

#define DK_NFINGER 16
typedef struct {
    const char *name;
    uint8_t lanes[8][16];               /* DRUM: velocity 0..9 a sixteenth per lane; melodic: lanes[0] the degree+1
                                         * (0 rest), lanes[1] its length in sixteenths, lanes[2] the octave (+1) */
} dk_finger_t;

/* an ATMO layer: an engine's voice that keeps going (WORLD's rain, its fire...) at a level of its own */
typedef struct dk_atmo_s {
    const char *name;
    uint8_t eng;
    int8_t p[3];                        /* its engine's first three parameters */
    float gain;
} dk_atmo_t;

typedef struct {
    const char *name;
    void (*init)(void);                 /* once at boot (its tables) */
    const dk_engine_t *const *eng;
    uint8_t neng;
    const dk_preset_t *presets;         /* DK_NFACT an engine */
    const dk_fx_t *const *fx;
    uint8_t nfx;
    const char *const *style_name;
    uint8_t nstyle;
    void (*drive)(const dk_drive_t *d, const dk_harm_t *h);
    const int8_t (*prog)[4];            /* chord progressions as degrees, a row each (10..13: a borrowed chord) */
    uint8_t nprog;
    const uint8_t *style_chord;         /* a style's chords (CH_*) and bars a chord */
    const uint8_t *style_bars;
    const uint8_t *style_breath;        /* a style rests a bar in eight */
    const dk_borrow_t *borrow_major, *borrow_minor;   /* 4 each */
    const dk_finger_t *finger_drum, *finger_mel;   /* 16 each */
    const char *const *perform;         /* the 11 black keys in PERFORM (PF_*) */
    const uint8_t *perform_act;
    const dk_mood_t *moods;
    uint8_t nmood;
    const char *const *biome_name;
    void (*master)(float *l, float *r, int n, float dust, float sleep);
    void (*demo)(void);                 /* the demo song into dk_song (UI thread, the song locked) */
    uint8_t metro_sound;                /* its click's default (METRO_*) */
    uint8_t human;                      /* timing and velocity looseness, % */
    const uint8_t *def_eng;             /* the tracks' engines, presets, sequencers, effects, roles by default */
    const uint8_t *def_preset, *def_seq, *def_role;
    const uint8_t (*keys_set)[2];       /* the KEYS track's 12 presets across engines {engine, preset}, or 0 */
    uint8_t keys_role;                  /* the role whose track browses keys_set (R_ARP: "KEYS") */
    const struct dk_atmo_s *atmo;       /* ATMO: up to six layers of the place's sound, under the music, or 0 */
    uint8_t natmo;
} dk_tape_t;
extern const dk_tape_t TAPE;

/* performance effects (generic, done by DECK) */
enum { PF_NONE, PF_TAPESTOP, PF_REWIND, PF_VINYLSTOP, PF_LOWPASS, PF_RAINUP, PF_DREAM, PF_HALFTIME, PF_ECHOTHROW,
       PF_MUTEDRUMS, PF_SLEEP, PF_REVERSE, PF_STUTTER, PF_FILTERUP, PF_BUILD, PF_N };
enum { METRO_CLICK, METRO_WOOD, METRO_HAT, METRO_COWBELL, METRO_RIM, METRO_N };

/* ---------------------------------------------------------- what engines share --- */
typedef struct {
    float wow;                          /* the tape's slow pitch sway (and a tape stop's fall), x */
    float tune;                         /* 440 Hz x */
    float lfo_slow, lfo_mid, lfo_fast;  /* phases 0..1 at ~0.07, ~0.9, ~5.2 Hz */
    float trem;                         /* tremolo phase, 4.6 Hz */
    float spb;                          /* samples a beat */
    uint8_t pedal;                      /* the sustain pedal */
    float tape;                         /* ATMO's TAPE: the master's tape emulation 0..1 (the tape's master reads it) */
} dk_ctx_t;
extern dk_ctx_t dk_ctx;
uint32_t dk_rnd(void);
float dk_frand(void);                   /* 0..1 */
float dk_white(void);                   /* -1..1 */
float dk_mtof(float note);
float dk_decay(float tau_s);            /* a one-pole decay's multiplier for tau seconds */
float dk_lpk(float fc);                 /* a one-pole low-pass coefficient */
float dk_sine(float ph);                /* a sine from the table, ph in turns >= 0 */
float dk_saw(float t, float dt);        /* a band-limited saw (polyBLEP) */
float dk_svf_f(float fc);

/* DRIVE's output: a note on the track (dur ticks; delay samples, loose) */
void dk_emit(int trk, int note, int vel, int dur, int delay);

/* -------------------------------------------------------------- the harmony --- */
#define DK_NMODES 11
extern const char *const DK_MODE_NAME[DK_NMODES];
extern const uint16_t DK_MODE_POOL[DK_NMODES];
extern const char *const DK_NOTE_NAME[12];
#define DK_NCHORD 23
extern const char *const DK_CHORD_NAME[DK_NCHORD];
enum { CH_TRIAD, CH_5, CH_SUS2, CH_SUS4, CH_6, CH_M6, CH_7, CH_MAJ7, CH_MIN7, CH_DOM7, CH_M7B5, CH_ADD9, CH_MADD9, CH_9,
       CH_MAJ9, CH_MIN9, CH_11, CH_MIN11, CH_13, CH_7SUS4, CH_QUARTAL, CH_OCT, CH_CLUSTER };
enum { VC_CLOSE, VC_OPEN, VC_SPREAD, VC_DROP2, VC_POWER, VC_N };
int dk_scale(int mode, int *steps);                         /* the scale's steps from its root; how many */
int dk_scale_note(int root, int mode, int idx);             /* scale index (0 = the root at C-1) to MIDI */
int dk_note_index(int root, int mode, int note);            /* the index at or below a note */
/* a chord on a scale degree (diatonic: the type stacked in the scale) or on a root (fixed): intervals from its root,
 * the notes; returns how many */
int dk_chord(int root, int mode, int deg, int type, int diatonic, int dimskip, int *notes, int base);
int dk_voice(const int *chord, int n, int voicing, int bass, const int *prev, int *out);   /* the voicing, led */
const char *dk_chord_name(const int *notes, int n, char *buf);   /* "AM7", "FMAJ9" (capitals) */
/* the song's events (the UI's song.c): an event into a clip, in its place; -1 full */
int dk_song_insert(int clip, const dk_ev_t *e);
void dk_song_clear(void);                 /* an empty song, the tape's tracks */
void dk_track_setup(int trk, int eng, int preset, int seq, int role);   /* an engine, a preset, a sequencer */
int dk_fit_degree(int root, int mode, uint32_t pcs, int *score);
int dk_nearest(int from, int pc, int lo, int hi);

/* ----------------------------------------------------- inline, for the engines --- */
extern float dk_sint[1025];
static inline float dk_sinx(float ph)          /* ph in turns, >= 0: one conversion, the table wraps */
{
    float x = ph * 1024.0f;
    int i = (int)x;
    float f = x - (float)i;
    i &= 1023;
    return dk_sint[i] + (dk_sint[i + 1] - dk_sint[i]) * f;
}
static inline float dk_wrap1(float p) { return p >= 1.0f ? p - 1.0f : p; }   /* a phase stepped by less than a turn */
static inline float dk_blep(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}
