/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The panel's map, in one table (DECK_SPEC 10): each button's tap, its hold (a timed hold, or a modifier held while
 * keys or knobs do something), and HOME + it. The free knobs K1..K4. Change the controls here. Part of the unity
 * build; CLAUDE.md's table is this one. */

enum { A_NONE, A_PAGE_SOUND, A_PAGE_SEQ, A_PAGE_ROAD, A_PAGE_MIX, A_PAGE_KEYS, A_PAGE_PROJECT, A_PAGE_PLAY, A_PLAY,
       A_REC, A_NEXT_TRACK, A_NEXT_PART, A_FILL, A_TAP_TEMPO, A_PLAYMODE, A_CLEAR, A_METRO, A_MUTE, A_BAKE, A_UNDO,
       A_REDO, A_GARAGE, A_OCT_DOWN, A_OCT_UP, A_MOD, A_TRANS_DOWN, A_TRANS_UP, A_ARP, A_ARP_OFF, A_PAGE_GLOBAL, A_N };
/* a hold: 0 none, a time (ms) for a timed one, HOLD_MOD a modifier (its tap on release unless it was used) */
#define HOLD_MOD 0xFFFFu
typedef struct {
    uint8_t btn, tap, hold, home;
    uint16_t hold_ms;
    const char *what;                    /* for CLAUDE.md and the hints */
} ui_map_t;
static const ui_map_t UI_MAP[] = {
    {B_FX, A_PAGE_SOUND, A_NONE, A_NONE, 0, "SOUND; again: ENGINE ENV FX MOD"},
    {B_SEQ, A_PAGE_SEQ, A_NONE, A_BAKE, HOLD_MOD, "SEQ; again: its next row | hold + white 1-5: the sequencer | HOME+: BAKE (DRIVE to STEP)"},
    {B_ENV, A_PAGE_ROAD, A_GARAGE, A_NONE, 600, "ROAD | hold: the garage of parts"},
    {B_LFO, A_PAGE_MIX, A_NONE, A_NONE, HOLD_MOD, "MIX; again: MASTER | hold + turn a SOUND knob: MOD 1 on it"},
    {B_PLAY, A_PLAY, A_PLAYMODE, A_TAP_TEMPO, 800, "play/stop | hold: PART/SONG | HOME+: tap tempo"},
    {B_REC, A_REC, A_CLEAR, A_METRO, 1000, "record (count-in) | hold 1 s: clear the clip | HOME+: metronome"},
    {B_SEL, A_NEXT_TRACK, A_NONE, A_NONE, HOLD_MOD, "next track (the six shown) | hold + white 1-6: that track"},
    {B_ARP, A_ARP, A_NONE, A_ARP_OFF, HOLD_MOD, "ARP on (over the sequencer); again: LATCH on/off | HOME+: ARP off | hold + white A-H: a part"},
    {B_GLO, A_PAGE_GLOBAL, A_FILL, A_NONE, 250, "GLOBAL (tempo, swing, dust, metronome, MIDI); again: its row | hold: FILL"},
    {B_EDIT, A_PAGE_KEYS, A_UNDO, A_REDO, 700, "KEYS; again: its next row | hold: UNDO | HOME+: REDO"},
    {B_SAVE, A_PAGE_PROJECT, A_NONE, A_NONE, HOLD_MOD, "PROJECT; again there: save now | hold + white 13-16: save preset"},
    {B_HOME, A_PAGE_PLAY, A_NONE, A_NONE, HOLD_MOD, "PLAY | HOME + white 1-16: preset | HOME + black 1-6: track | HOME + black 7-11: COPY PASTE DOUBLE VARIATE HUMANIZE the clip"},
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
