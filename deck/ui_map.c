/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 DECK contributors */
/* The panel's map, in one table (DECK_SPEC 10): each button's tap, its hold (a timed hold, or a modifier held while
 * keys or knobs do something), and HOME + it. The free knobs K1..K4. Change the controls here. Part of the unity
 * build; CLAUDE.md's table is this one. */

enum { A_NONE, A_PAGE_SOUND, A_PAGE_SEQ, A_PAGE_ROAD, A_PAGE_MIX, A_PAGE_KEYS, A_PAGE_PROJECT, A_PAGE_PLAY, A_PLAY,
       A_REC, A_NEXT_TRACK, A_NEXT_PART, A_FILL, A_TAP_TEMPO, A_PLAYMODE, A_CLEAR, A_METRO, A_MUTE, A_BAKE, A_UNDO,
       A_REDO, A_GARAGE, A_OCT_DOWN, A_OCT_UP, A_MOD, A_TRANS_DOWN, A_TRANS_UP, A_ARP, A_ARP_OFF, A_PAGE_GLOBAL, A_PAGE_ATMO, A_BLOCK_NEXT, A_BLOCK_PREV, A_N };
/* a hold: 0 none, a time (ms) for a timed one, HOLD_MOD a modifier (its tap on release unless it was used) */
#define HOLD_MOD 0xFFFFu
typedef struct {
    uint8_t btn, tap, hold, home;
    uint16_t hold_ms;
    const char *what;                    /* for CLAUDE.md and the hints */
} ui_map_t;
static const ui_map_t UI_MAP[] = {
    {B_FX, A_PAGE_SOUND, A_NONE, A_BLOCK_NEXT, HOLD_MOD, "SOUND; again: FAV (the preset's 4 main knobs, K5 MACRO) MORE FX (K1 tone, K2 depth - with FILTER: K2 CUTOFF in Hz, K1 RES, K3 reverb, K4 echo) TYPE (effect and its 2nd knob) MOD | hold + white 1-4: BYPASS the chain's stage at once: 1 effect, 2 echo, 3 reverb, 4 tape (again = back on) | HOME+: the next preset BLOCK (AMBIENT LOFI, 8-16 BIT, SYNTHWAVE, CINEMATIC), the same place in it"},
    {B_SEQ, A_PAGE_SEQ, A_NONE, A_BAKE, HOLD_MOD, "SEQ; again: its next row | hold + black 1-5: COOK, RANDOM, QUANTIZE the clip (by the track QUANT), COPY BAR, PASTE BAR (on TRACKS: black 1-6 = SOLO) | hold + white 1-5: the sequencer (in MIX: white 1-6 = TEMP mute of the track; on TRACKS: white 1-6 = ARM the track for REC, black 1-6 = SOLO) | HOME+: BAKE (DRIVE to STEP) | FINGER: K1 of its row = BANK of the track: LOFI, 8BIT, SYNTH (night drive), CINEMA (the north); white = 16 patterns simple to dense (last 3 fills), black = variations"},
    {B_ENV, A_PAGE_ROAD, A_GARAGE, A_NONE, 600, "ROAD | hold: the garage (white 1-8: copy part to)"},
    {B_LFO, A_PAGE_MIX, A_NONE, A_NONE, HOLD_MOD, "MIX (white 1-6: mute the track IN THIS PART; playing: at the bar end; hold 1 s: clear its clip); again: MASTER | hold + turn a SOUND knob: MOD 1 on it"},
    {B_PLAY, A_PLAY, A_PLAYMODE, A_TAP_TEMPO, 800, "play/stop | hold: PART/SONG | HOME+: tap tempo"},
    {B_REC, A_REC, A_CLEAR, A_METRO, 1000, "record at once (count-in, or ON NOTE; optional REC ARM: arm, then PLAY); while recording: punch-out (the tape plays on) | hold 1 s: clear the notes of the clip (the automation stays), 2.5 s: the track in all parts, automation too (not on SOUND/MIX) | hold + a SOUND/MIX knob: AUTOMATION (a smooth curve of the knob, per track) | hold + FX: erase this track's automation (undo: hold EDIT) | SEQ+: clear the notes | HOME+: metronome"},
    {B_SEL, A_PAGE_ATMO, A_NONE, A_BLOCK_PREV, 0, "ATMO: rain, fire, birds, ocean, wind, crickets, the dust, the tape; again: its row | HOME+: the previous preset BLOCK"},
    {B_ARP, A_ARP, A_NONE, A_ARP_OFF, HOLD_MOD, "ARP on (over the sequencer); again: LATCH on/off | HOME+: ARP off | hold + white A-H: a part (an empty one gets a COPY of this part, NEW PART on GLOBAL)"},
    {B_GLO, A_PAGE_GLOBAL, A_FILL, A_NONE, 250, "GLOBAL (tempo, swing, dust, metronome, MIDI); again: its row | hold: FILL (FINGER: its fill patterns 14-16 of the bank on a drum track)"},
    {B_EDIT, A_PAGE_KEYS, A_UNDO, A_REDO, 700, "KEYS; again: next row; on TRACKS: LAYERS | hold: UNDO | HOME+: REDO"},
    {B_SAVE, A_PAGE_PROJECT, A_NONE, A_NONE, HOLD_MOD, "PROJECT (white 1-4 open, 5 new, 6 demo; black 1 EXPORT, 2 STEMS, 3 SAVE, 4 CLEAR ALL: press twice); again there: save now | hold + white 1: SNAPSHOT of the part (before an experiment), white 2: RECALL it (an undo step), white 13-16: save preset"},
    {B_HOME, A_PAGE_PLAY, A_NONE, A_NONE, HOLD_MOD, "PLAY; on PLAY: TRACKS (six track rows; again: back; while recording: TRACKS <-> REC) | HOME + white 1-16: preset (in MIX: white 1-6 = the part's track mute at once; on TRACKS: white 1-6 = SELECT the track, black 1-6 = mute in the part, SEQ + black 1-6 = solo, SEQ + white 1-6 = ARM; K3 REC MODE, K4 REC ARM) | HOME + black 1-6: track | HOME + black 7-11: COPY PASTE DOUBLE VARIATE HUMANIZE the clip"},
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
