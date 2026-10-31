/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The song's edits (song.c), the user's presets. */
#pragma once
#include "core.h"

typedef struct {
    char name[12];
    uint8_t used, fx, fxamt, fxa, fxb, rev, echo;
    int8_t p[DK_NP], env[3], trim;
    uint8_t mt[3];
    int8_t ma[3];
} dk_upreset_t;
#define DK_UENG 12                       /* the engines with user presets (a tape has up to twelve) */
#define DK_UENG_V1 8                     /* the store's old sizes: eight, then ten engines */
#define DK_UENG_V2 10
typedef struct {
    uint32_t magic;
    dk_upreset_t p[DK_UENG][4];
} dk_user_t;
extern dk_user_t dk_user;
extern uint32_t song_edits;
extern uint32_t song_notes_lost;         /* events refused because the pool was full (the UI says the count) */
extern uint32_t song_notes_lost_at;      /* dk_ticks of the last refused event */

int clip_len(int clip);
int clip_len_raw(int clip);
int clip_find(int clip, int pos, int note, int tol);
void ev_delete(int clip, int k);
void clip_clear(int clip);                 /* the notes out; the automation stays */
void clip_clear_all(int clip);             /* every event */
int auto_count(int clip, int target);      /* AUTOMATION points of a target (0: all) */
int auto_clear(int clip, int target);      /* ... removed */
int auto_point(int clip, int target, int pos, int val, int from, int len);
void auto_thin(int clip, int target, int from, int to, int tol);
void clip_copy(int clip);
int clip_paste(int clip);
void clip_double(int clip);
int layers_list(int clip, uint8_t *take, uint8_t *snd, uint16_t *cnt, int max);
int layer_muted(int clip, int take);
int layer_soloed(int clip, int take);
void layer_mute_toggle(int clip, int take);
void layer_solo_toggle(int clip, int take);
int layer_delete(int clip, int take);
int take_erase_last(void);              /* hold REC: the notes of the latest REC press (all its passes, all its tracks) erased: one undo step; 0 none */
int layers_merge(int clip);
int clip_quantize(int clip, int strength);
int clip_bars(int clip);
int bar_copy(int clip, int bar);
int bar_paste(int clip, int bar);
void clip_transpose(int clip, int steps);
void clip_variate(int clip, uint32_t seed);
void clip_humanize(int clip, uint32_t seed);
void clip_cook(int clip, uint32_t seed, const dk_engine_t *e);     /* a little different, in the key (undoable) */
void clip_random(int clip, uint32_t seed, const dk_engine_t *e);   /* the same rhythm (or as many hits), new notes */
void undo_push(int clip);
void undo_group(int on);                 /* undo_push-es until undo_group(0): one undo step */
void undo_push_road(void);
void undo_push_part(int p);
int undo_count(void);
int song_undo(int *what);
int song_redo(int *what);
int rec_flush(void);
void song_clear_all(void);               /* CLEAR ALL: every clip, the road, the frozen sounds, the parts' bars; undo forgotten; the sounds kept */
void song_wipe(void);                    /* every part, the road, and the instruments back to the tape; UNDO brings them back */
void part_inst_sync(int p);             /* this part's instruments into the tracks (the one we leave is kept) */
void part_inst_capture(int p);          /* the tracks' instruments, copied into every part (a new song, a demo, an old project) */
void part_inst_flush(void);             /* the live part's instruments back into its slot (before a save) */
void part_inst_boot(void);              /* after a project loads */
void part_inst_reset(int p);            /* this part's instruments back to the tape's; the others stay */
void part_inst_reset_track(int p, int t); /* one track of this part; the other tracks and parts stay */
int part_has_events(int p);              /* 1: any of the part's six clips has an event */
/* NEW PART = COPY: the part `from` copied into the empty part `to` (all six clips with their events, takes and frozen-sound ids,
 * lengths, the part's bars and track mutes), one undo step. 1 copied; 0 nothing done (same part, `to` not empty, `from` empty);
 * -1 no room for the events (nothing done) */
int part_copy(int from, int to);
int part_copy_x(int from, int to, int wipe);
int snapshot_take(int part);             /* SNAPSHOT: the part's clips, bars and mutes kept (1; 0 too many events) */
int snapshot_part(void);                 /* the part of the snapshot (-1 none) */
int snapshot_recall(void);               /* the part as taken (one undo step): 1 done, 0 none, -1 no room */
int part_copy_undo(int to);              /* the copy just made taken back (nothing in `to` was edited since): 1 done */
void track_sound_to(int t);              /* before a track's preset or engine changes: its take being recorded is frozen as it is */
void takes_forget(void);
extern uint8_t song_mem_dropped, song_mem_full;   /* the pool was full while recording: the oldest take went / nothing to drop (the UI says it) */
extern uint8_t song_layers_full;         /* the table of frozen sounds was full (the UI says it once) */
void rec_reset(void);
const dk_preset_t *preset_of(int eng, int slot, dk_preset_t *tmp);
int preset_block(int id);                   /* 0..DK_NBLK-1; the user's: -1 */
int preset_id(int blk, int i);
int preset_count(int eng, int blk);
void track_preset(int t, int slot);
void track_preset_sound(int t, int slot);   /* a preset or an instrument by hand: the FX is not touched */
void song_template(int tp);
void song_mood(int m);
