/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The panel's map, in one table (DECK_SPEC 10): each button's tap, its hold (a timed hold, or a modifier held while
 * keys or knobs do something), and HOME + it. The free knobs K1..K4. Change the controls here. Part of the unity
 * build; CLAUDE.md's table is this one. */

enum { A_NONE, A_PAGE_SOUND, A_PAGE_SEQ, A_PAGE_ROAD, A_PAGE_MIX, A_PAGE_KEYS, A_PAGE_PROJECT, A_PAGE_PLAY, A_PLAY,
       A_REC, A_NEXT_TRACK, A_NEXT_PART, A_FILL, A_TAP_TEMPO, A_TAP_ARM, A_PLAYMODE, A_METRO, A_MUTE, A_BAKE, A_UNDO,
       A_REDO, A_GARAGE, A_OCT_DOWN, A_OCT_UP, A_MOD, A_TRANS_DOWN, A_TRANS_UP, A_ARP, A_ARP_OFF, A_PAGE_GLOBAL, A_PAGE_ATMO, A_BLOCK_NEXT, A_BLOCK_PREV, A_N };
/* a hold: 0 none, a time (ms) for a timed one, HOLD_MOD a modifier (its tap on release unless it was used) */
#define HOLD_MOD 0xFFFFu
typedef struct {
    uint8_t btn, tap, hold, home;
    uint16_t hold_ms;
    const char *what;                    /* for CLAUDE.md and the hints */
} ui_map_t;
static const ui_map_t UI_MAP[] = {
    {B_FX, A_PAGE_SOUND, A_NONE, A_BLOCK_NEXT, HOLD_MOD, "SOUND; again: FAV (the preset's 4 main knobs, K5 MACRO) MORE FX (K1 tone, K2 depth - with FILTER: K2 CUTOFF in Hz, K1 RES; with GLIDE: K2 TIME, the slide; K3 reverb, K4 echo) TYPE (effect and its 2nd knob) MOD | hold + white 1-4: BYPASS the chain's stage at once: 1 effect, 2 echo, 3 reverb, 4 tape (again = back on) | HOME+: the next preset BLOCK (AMBIENT LOFI, 8-16 BIT, SYNTHWAVE, CINEMATIC), the same place in it"},
    {B_SEQ, A_PAGE_SEQ, A_NONE, A_BAKE, 0, "SEQ; again: its next row (SEQUENCER: SEQ or ENDLESS) | hold 0.25 s, drums in GROOVE: FILL (the bar that builds) while held; white and black keys still pick the groove and its fills | ENDLESS: REC = play the steps in (OCT- rest, OCT+ tie), REC = done; a key then transposes it"},
    {B_ENV, A_PAGE_ROAD, A_GARAGE, A_NONE, 600, "ROAD; while recording: on into the next empty part (a copy of this one, the take goes on) | hold: the garage (white 1-8: copy part to) | while REC: hold + SELECT picks a part (an empty one gets a copy of this one now, the loop stays, the take goes on)"},
    {B_LFO, A_PAGE_MIX, A_UNDO, A_REDO, 700, "(the panel's EDIT button: btn_swap) MIX; hold 0.7 s: UNDO, HOME+: REDO; (white 1-6: select the track; black 1-6: its mute IN THIS PART, playing: at the bar end; black 7: solo; black 8: arm); again: MASTER | hold + turn a SOUND knob: MOD 1 on it"},
    {B_PLAY, A_PLAY, A_PLAYMODE, A_TAP_TEMPO, 800, "play/stop | hold: PART/SONG | HOME+: tap tempo"},
    {B_REC, A_REC, A_NONE, A_METRO, HOLD_MOD, "record at once (count-in, or ON NOTE; optional REC ARM: arm, then PLAY); while recording: punch-out (the tape plays on) | hold + a SOUND/MIX knob: AUTOMATION (a smooth curve of the knob, per track); hold (0.6 s) and nothing else: ERASES THE TAKE you just recorded (its notes on all tracks, every pass; older notes and automation stay; one undo step; while recording it goes on) | HOME+: metronome"},
    {B_SEL, A_PAGE_ATMO, A_NONE, A_BLOCK_PREV, 0, "REC SETUP: K1 METRO, K2 REC MODE, K3 NEW PART (COPY/EMPTY), K4 START, ALGO click volume; again: row 2 (COUNT IN, CLICK, LOCK TAKE, REC LAT) | HOME+: the previous preset BLOCK (ATMO is GLOBAL's rows 6-7)"},
    {B_ARP, A_ARP, A_NONE, A_ARP_OFF, HOLD_MOD, "the ARP page (K4 = ARP ON/OFF over the sequencer; row 2: K2 LATCH); again: its next row | HOME+: ARP off | hold + white A-H: a part (an empty one gets a COPY of this part, NEW PART on GLOBAL)"},
    {B_GLO, A_PAGE_GLOBAL, A_TAP_ARM, A_NONE, 250, "GLOBAL (tempo, swing, dust, metronome, MIDI); again: its row | hold + white 1 (W1): tap the tempo you hear; after three taps the BPM is set, the click does not start"},
    {B_EDIT, A_PAGE_KEYS, A_UNDO, A_REDO, 700, "(the panel's LFO button: btn_swap) KEYS; again: next row; on TRACKS: the EDIT LIST (again: back) | hold: UNDO | HOME+: REDO"},
    {B_SAVE, A_PAGE_PROJECT, A_NONE, A_NONE, HOLD_MOD, "PROJECT (white 1-4 open, 5 new, 6 demo; black 1 EXPORT, 2 STEMS, 3 SAVE, 4 CLEAR ALL: press twice); again there: save now | hold + white 1: SNAPSHOT of the part (before an experiment), white 2: RECALL it (an undo step), white 13-16: save preset"},
    {B_HOME, A_PAGE_PLAY, A_NONE, A_NONE, HOLD_MOD, "PLAY; on PLAY: TRACKS (six track rows; again: back; while recording: TRACKS <-> REC) | HOME + white 1-16: preset | HOME + black 1-6: track | TRACKS and MIX: white 1-6 = SELECT the track, black 1-6 = its mute in the part, black 7 = solo, black 8 = arm (K3 REC MODE, K4 REC ARM on TRACKS) | TRACKS + EDIT: the EDIT LIST, white 1-14: QUANTIZE, COPY BAR, PASTE BAR, DOUBLE, VARIATE, HUMANIZE, COOK, RANDOM, COPY CLIP, PASTE CLIP, LAYERS, ERASE NOTES, ERASE AUTO, ERASE ALL (this track in this part: its notes and its instrument; other tracks and parts stay; the erases: press twice, one undo step) | hold EDIT + white 14: the whole road, every part, every track | K2 = the part"},
    {B_OCTDN, A_OCT_DOWN, A_NONE, A_TRANS_DOWN, 0, "the track's octave down | HOME+: the clip a scale step down"},
    {B_OCTUP, A_OCT_UP, A_NONE, A_TRANS_UP, 0, "the track's octave up | HOME+: the clip a scale step up"},
};
#define UI_NMAP (sizeof UI_MAP / sizeof UI_MAP[0])

static const ui_map_t *map_of(int b)
{
    unsigned i;
    for (i = 0; i < UI_NMAP; i++)
        if (UI_MAP[i].btn == b)
            return &UI_MAP[i];
    return 0;
}
