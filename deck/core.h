/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* DECK's audio core (core.c, its own -O2 unit): the clock, the tracks' playback and generators, the voices, the
 * mixer, the sends, the metronome. The UI talks to it with commands; what it shows comes from the dk_* state below,
 * written by the render. */
#pragma once
#include "deck_api.h"

void dk_init(void);
void dk_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
void dk_shed(void);
void dk_midi_out(uint32_t st, uint32_t d1, uint32_t d2);   /* the app sends it (from the render) */

/* commands (main loop -> render) */
enum { C_NOTE = 1, C_PLAY, C_REC, C_PART, C_PANIC, C_FINGER, C_FVAR, C_PERFORM, C_FILL, C_TOMBOLA, C_SEED, C_PEDAL,
       C_STEMS, C_ARPCLR, C_SEEK, C_REWIND, C_RECTRK, C_HOLDCLR, C_RETUNE, C_PMUTE };
void dk_cmd(int c, int a, int b, int d);
#define dk_note(trk, note, vel, delay) dk_cmd(C_NOTE, (trk), (note) | (vel) << 8, (delay))

/* the song locked: the render's sequencer waits (the sound goes on) */
void dk_lock(void);
void dk_unlock(void);

/* the recording's events, from the render to the UI (which puts them in the song, dk_rec_flush) */
typedef struct {
    uint8_t clip, kind;                 /* kind 0 an event, 1 the clip's length (ev.pos) */
    uint8_t pass, rsv;
    dk_ev_t ev;
} dk_recev_t;
#define DK_RECQ 128
extern volatile dk_recev_t dk_recq[DK_RECQ];
extern volatile uint32_t dk_recq_w;

/* the garden's ring: every note that sounds (note | track << 8 | vel << 16 | src << 24; src 0 you, 1 the clip,
 * 2 a generator) */
#define DK_SEEN 64
extern volatile uint32_t dk_seen_w;
extern volatile uint32_t dk_seen[DK_SEEN];

/* state for the screen */
extern volatile uint8_t dk_playing, dk_recording, dk_counting, dk_rec_trk, dk_pass;
extern volatile uint8_t dk_cur_part, dk_next_part, dk_road_i, dk_road_rep, dk_stems;
extern volatile uint16_t dk_part_tick, dk_part_len;
extern volatile uint32_t dk_ticks;                      /* since the transport started */
extern volatile uint32_t dk_clock;                      /* ticks since boot (the free clock) */
extern volatile uint8_t dk_beat_flash;                  /* the metronome's beat: 2 the bar's first, 1 the others */
extern volatile float dk_level[DK_NTRACK], dk_master_level;
/* the flash's erase stops the sound ~17 ms: before one the UI asks for a hush (the output faded to nothing in ~6 ms),
 * waits for silent blocks, erases, lets go; the cut is then silence, not a click (dust, tape hiss, a tail) */
extern volatile uint8_t dk_hush, dk_hushed;
int dk_dream_off(void);
extern volatile uint8_t dk_finger_on[DK_NTRACK];        /* FINGER: the pattern held (+1), 0 none */
extern volatile uint32_t dk_finger_latch[DK_NTRACK];
extern volatile uint8_t dk_harm_pcs[4], dk_harm_n, dk_harm_deg;
extern volatile uint8_t dk_perform;                     /* the performance effect held (PF_*) */
extern volatile uint8_t dk_open_n;                      /* notes held while recording (the tape's red) */
extern volatile uint16_t dk_open_pos[8];
extern volatile uint8_t dk_open_note[8];
extern volatile uint8_t dk_voices_on;
extern volatile uint8_t dk_voices_frozen;   /* (of them, with a frozen sound) */
extern volatile uint8_t dk_fill;
extern volatile uint32_t dk_rec_hits;                   /* notes recorded (the head blinks) */
/* TOMBOLA for the screen: the drum's turn (turns) and its balls (x, y in -1..1, 0 none) */
#define DK_NBALL 12
extern volatile float dk_tmb_angle;
extern volatile int16_t dk_tmb_x[DK_NBALL], dk_tmb_y[DK_NBALL];
extern volatile uint8_t dk_tmb_note[DK_NBALL], dk_tmb_n, dk_tmb_hit[DK_NBALL];
/* ARP / HOLD: the notes held for them */
extern volatile uint8_t dk_held_n[DK_NTRACK];
extern volatile uint8_t dk_arp_rpos[DK_NTRACK], dk_arp_hit[DK_NTRACK];   /* ARP: its rhythm's step, a note just now */
extern const uint8_t DK_ARP_DIV[DK_ARP_NDIV], DK_ARP_RHY[ARPR_N][16];
extern volatile uint8_t dk_held[DK_NTRACK][8];
/* the metronome (global settings, not the song's) */
typedef struct {
    uint8_t mode;                       /* 0 OFF, 1 REC (while recording and counting), 2 ALWAYS */
    uint8_t count;                      /* bars of count-in: 1 or 2 */
    uint8_t sound;                      /* METRO_* */
    uint8_t div;                        /* 0 quarters, 1 eighths, 2 sixteenths */
    uint8_t vol;                        /* 0..100 */
    uint8_t on_note;                    /* recording starts on the first note instead */
} dk_metro_t;
extern dk_metro_t dk_metro;
/* MIDI out settings */
extern uint8_t dk_midi_notes, dk_midi_clock;
/* recording: the audio's latency made up, ms (GLOBAL: REC LAT; 0..30): a played note is put that much earlier */
extern uint8_t dk_rec_lat;
extern uint8_t dk_rec_arm;              /* REC ARM: two steps (REC arms, PLAY records) when stopped (1) */
extern uint8_t dk_lock_take;             /* LOCK TAKE: a take keeps the sound it was recorded with (1) */
extern uint8_t dk_newpart_copy;          /* NEW PART (GLOBAL): 1 COPY (switching to an empty part while playing copies the part you leave), 0 EMPTY */
/* a part's track mutes (dk_song.part[p].mute): a change made while playing waits for the bar's end (C_PMUTE a = track, b = part);
 * the pending one (dk_pm_part's tracks in dk_pm_x flip at the bar's end) is shown on MIX; dk_pm_gen counts the changes made */
extern volatile uint8_t dk_pm_part, dk_pm_x, dk_pm_gen;
extern volatile uint32_t dk_metro_clicks;               /* clicks made (tests) */
extern volatile uint32_t dk_metro_err_max;              /* the worst offset of a click from its tick, samples */

/* the engine of a track and the values its voices get */
const dk_engine_t *dk_engine(int trk);
int dk_param_static(int trk, int target);               /* the knobs and the MACRO only (what a take is frozen with) */
int dk_param_value(int trk, int target);                /* the value now (MACRO, motion and modulation applied) */
uint32_t dk_cpu_cost(void);
/* the CPU's load (the render's %, the device's measure) to the voices' budget: it sheds sooner when the render runs
 * long, and gives back slowly */
void dk_set_load(int pct);
int dk_get_budget(void);                             /* the render's cost, microseconds a block (host) */
