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
#define DK_UENG 10                       /* the engines with user presets (a tape has up to ten) */
#define DK_UENG_V1 8                     /* the store's old size: eight */
typedef struct {
    uint32_t magic;
    dk_upreset_t p[DK_UENG][4];
} dk_user_t;
extern dk_user_t dk_user;
extern uint32_t song_edits;

int clip_len(int clip);
int clip_len_raw(int clip);
int clip_find(int clip, int pos, int note, int tol);
void ev_delete(int clip, int k);
void clip_clear(int clip);
void clip_copy(int clip);
int clip_paste(int clip);
void clip_double(int clip);
int layers_list(int clip, uint8_t *take, uint8_t *snd, uint16_t *cnt, int max);
int layer_muted(int clip, int take);
int layer_soloed(int clip, int take);
void layer_mute_toggle(int clip, int take);
void layer_solo_toggle(int clip, int take);
int layer_delete(int clip, int take);
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
int part_has_events(int p);              /* 1: any of the part's six clips has an event */
/* NEW PART = COPY: the part `from` copied into the empty part `to` (all six clips with their events, takes and frozen-sound ids,
 * lengths, the part's bars and track mutes), one undo step. 1 copied; 0 nothing done (same part, `to` not empty, `from` empty);
 * -1 no room for the events (nothing done) */
int part_copy(int from, int to);
int part_copy_x(int from, int to, int wipe);
int part_copy_undo(int to);              /* the copy just made taken back (nothing in `to` was edited since): 1 done */
void track_sound_to(int t);              /* before a track's preset or engine changes: its take being recorded is frozen as it is */
void takes_forget(void);
extern uint8_t song_layers_full;         /* the table of frozen sounds was full (the UI says it once) */
void rec_reset(void);
const dk_preset_t *preset_of(int eng, int slot, dk_preset_t *tmp);
void track_preset(int t, int slot);
void song_template(int tp);
void song_mood(int m);
