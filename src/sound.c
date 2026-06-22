#include <stdint.h>
#include <stdbool.h>
#include "opl.h"
#include "sound.h"

#define SFX_DIG_CH          5
#define SFX_FALL_CH         6
#define SFX_TRAP_CH         7
#define SFX_PLAYER_CH       8

// ---------------------------------------------------------------------------
// OPL patches
// ---------------------------------------------------------------------------

// Gold pickup: bright sine chime
static const OPL_Patch gold_patch = {
    .m_ave=0x21, .m_ksl=0x00, .m_atdec=0xF8, .m_susrel=0x0A, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF6, .c_susrel=0x08, .c_wave=0x00,
    .feedback=0x02,
};

// Hidden ladder revealed: sparkling metallic FM chime
static const OPL_Patch hladder_patch = {
    .m_ave=0x22, .m_ksl=0x00, .m_atdec=0xF8, .m_susrel=0x0A, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF6, .c_susrel=0x08, .c_wave=0x00,
    .feedback=0x06,
};

// Digging blaster: descending sweep
static const OPL_Patch dig_patch = {
    .m_ave=0x61, .m_ksl=0x11, .m_atdec=0xF9, .m_susrel=0x0F, .m_wave=0x01,
    .c_ave=0x61, .c_ksl=0x00, .c_atdec=0xF7, .c_susrel=0x0F, .c_wave=0x00,
    .feedback=0x04,
};

// Falling ambient drone
static const OPL_Patch fall_patch = {
    .m_ave=0x31, .m_ksl=0x24, .m_atdec=0xF2, .m_susrel=0xF6, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xE2, .c_susrel=0xF4, .c_wave=0x00,
    .feedback=0x0E,
};

// Guard trapped struggle sound
static const OPL_Patch trap_patch = {
    .m_ave=0x21, .m_ksl=0x00, .m_atdec=0xFF, .m_susrel=0x0F, .m_wave=0x02,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xFF, .c_susrel=0x0F, .c_wave=0x02,
    .feedback=0x06,
};

// Player death: descending crash
static const OPL_Patch death_patch = {
    .m_ave=0x11, .m_ksl=0x14, .m_atdec=0xF4, .m_susrel=0xC8, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF3, .c_susrel=0xC6, .c_wave=0x00,
    .feedback=0x0E,
};

// Fanfare level clear
static const OPL_Patch win_patch = {
    .m_ave=0x61, .m_ksl=0x11, .m_atdec=0xF5, .m_susrel=0xF8, .m_wave=0x00,
    .c_ave=0x61, .c_ksl=0x00, .c_atdec=0xF3, .c_susrel=0xF6, .c_wave=0x00,
    .feedback=0x0E,
};

// ---------------------------------------------------------------------------
// Sound Sequencer State
// ---------------------------------------------------------------------------

// Dig (Channel 5)
#define DIG_NOTES 6u
#define DIG_NOTE_TICKS 2u
static const uint8_t dig_notes[DIG_NOTES] = { 68, 64, 60, 56, 52, 48 };
static bool    dig_active;
static uint8_t dig_note_idx;
static uint8_t dig_tick;

// Fall (Channel 6)
#define FALL_NOTES 4u
#define FALL_NOTE_TICKS 4u
static const uint8_t fall_notes[FALL_NOTES] = { 50, 47, 44, 41 };
static bool    fall_active;
static uint8_t fall_note_idx;
static uint8_t fall_tick;

// Trap (Channel 7)
#define TRAP_NOTES 4u
#define TRAP_NOTE_TICKS 4u
static const uint8_t trap_notes[TRAP_NOTES] = { 40, 43, 40, 43 };
static bool    trap_active;
static uint8_t trap_note_idx;
static uint8_t trap_tick;

// Player events (Channel 8)
#define GOLD_NOTES 2u
#define GOLD_NOTE_TICKS 4u
static const uint8_t gold_notes[GOLD_NOTES] = { 72, 79 };

#define HLADDER_NOTES 7u
#define HLADDER_NOTE_TICKS 4u
static const uint8_t hladder_notes[HLADDER_NOTES] = { 72, 74, 76, 78, 80, 82, 84 };

#define DEATH_NOTES 6u
#define DEATH_NOTE_TICKS 6u
static const uint8_t death_notes[DEATH_NOTES] = { 48, 45, 42, 39, 36, 33 };

#define WIN_NOTES 7u
#define WIN_NOTE_TICKS 5u
static const uint8_t win_notes[WIN_NOTES] = { 60, 64, 67, 72, 76, 79, 84 };

static bool            player_sfx_active;
static uint8_t         player_sfx_prio; // 0=none, 1=gold, 2=hladder, 3=death, 4=win
static uint8_t         player_sfx_note_idx;
static uint8_t         player_sfx_tick;
static uint8_t         player_sfx_total_notes;
static uint8_t         player_sfx_note_ticks;
static uint8_t         player_sfx_volume;
static const OPL_Patch *player_sfx_patch;
static const uint8_t   *player_sfx_notes;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void sfx_note_on(uint8_t ch, const OPL_Patch *p, uint8_t note, uint8_t vol)
{
    OPL_NoteOff(ch);
    OPL_SetPatch(ch, p);
    OPL_SetVolume(ch, vol);
    OPL_NoteOn(ch, note);
}

static void stop_channel(uint8_t ch)
{
    OPL_NoteOff(ch);
    OPL_SetVolume(ch, 0);
}

static void start_player_sfx(uint8_t prio, const OPL_Patch *patch,
                             const uint8_t *notes, uint8_t num,
                             uint8_t ticks, uint8_t vol)
{
    player_sfx_active      = true;
    player_sfx_prio        = prio;
    player_sfx_note_idx    = 0;
    player_sfx_tick        = 0;
    player_sfx_total_notes = num;
    player_sfx_note_ticks  = ticks;
    player_sfx_volume      = vol;
    player_sfx_patch       = patch;
    player_sfx_notes       = notes;
    sfx_note_on(SFX_PLAYER_CH, patch, notes[0], vol);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void sound_init(void)
{
    dig_active = false;
    dig_note_idx = 0;
    dig_tick = 0;

    fall_active = false;
    fall_note_idx = 0;
    fall_tick = 0;

    trap_active = false;
    trap_note_idx = 0;
    trap_tick = 0;

    player_sfx_active = false;
    player_sfx_prio = 0;
    player_sfx_note_idx = 0;
    player_sfx_tick = 0;
    player_sfx_total_notes = 0;
    player_sfx_note_ticks = 0;
    player_sfx_volume = 0;
    player_sfx_patch = 0;
    player_sfx_notes = 0;

    stop_channel(SFX_DIG_CH);
    stop_channel(SFX_FALL_CH);
    stop_channel(SFX_TRAP_CH);
    stop_channel(SFX_PLAYER_CH);
}

void sound_play_dig(void)
{
    dig_active = true;
    dig_note_idx = 0;
    dig_tick = 0;
    sfx_note_on(SFX_DIG_CH, &dig_patch, dig_notes[0], 127);
}

void sound_play_fall(bool enable)
{
    if (enable) {
        if (!fall_active) {
            fall_active = true;
            fall_note_idx = 0;
            fall_tick = 0;
            sfx_note_on(SFX_FALL_CH, &fall_patch, fall_notes[0], 127);
        }
    } else {
        if (fall_active) {
            fall_active = false;
            stop_channel(SFX_FALL_CH);
        }
    }
}

void sound_play_trap(void)
{
    trap_active = true;
    trap_note_idx = 0;
    trap_tick = 0;
    sfx_note_on(SFX_TRAP_CH, &trap_patch, trap_notes[0], 127);
}

void sound_play_gold(void)
{
    if (player_sfx_active && player_sfx_prio >= 1u) return;
    start_player_sfx(1, &gold_patch, gold_notes, GOLD_NOTES, GOLD_NOTE_TICKS, 127);
}

void sound_play_hladder(void)
{
    if (player_sfx_active && player_sfx_prio >= 2u) return;
    start_player_sfx(2, &hladder_patch, hladder_notes, HLADDER_NOTES, HLADDER_NOTE_TICKS, 127);
}

void sound_play_death(void)
{
    if (player_sfx_active && player_sfx_prio >= 3u) return;
    start_player_sfx(3, &death_patch, death_notes, DEATH_NOTES, DEATH_NOTE_TICKS, 127);
}

void sound_play_win(void)
{
    if (player_sfx_active && player_sfx_prio >= 4u) return;
    start_player_sfx(4, &win_patch, win_notes, WIN_NOTES, WIN_NOTE_TICKS, 127);
}

void sound_update(void)
{
    // --- Dig sequencer (ch 5) ---
    if (dig_active) {
        dig_tick++;
        if (dig_tick >= DIG_NOTE_TICKS) {
            dig_tick = 0;
            dig_note_idx++;
            if (dig_note_idx >= DIG_NOTES) {
                dig_active = false;
                stop_channel(SFX_DIG_CH);
            } else {
                sfx_note_on(SFX_DIG_CH, &dig_patch, dig_notes[dig_note_idx], 127);
            }
        }
    }

    // --- Fall sequencer (ch 6) ---
    if (fall_active) {
        fall_tick++;
        if (fall_tick >= FALL_NOTE_TICKS) {
            fall_tick = 0;
            fall_note_idx++;
            if (fall_note_idx >= FALL_NOTES) {
                fall_note_idx = 0;
            }
            sfx_note_on(SFX_FALL_CH, &fall_patch, fall_notes[fall_note_idx], 127);
        }
    }

    // --- Trap sequencer (ch 7) ---
    if (trap_active) {
        trap_tick++;
        if (trap_tick >= TRAP_NOTE_TICKS) {
            trap_tick = 0;
            trap_note_idx++;
            if (trap_note_idx >= TRAP_NOTES) {
                trap_active = false;
                stop_channel(SFX_TRAP_CH);
            } else {
                sfx_note_on(SFX_TRAP_CH, &trap_patch, trap_notes[trap_note_idx], 127);
            }
        }
    }

    // --- Player Event sequencer (ch 8) ---
    if (player_sfx_active) {
        player_sfx_tick++;
        if (player_sfx_tick >= player_sfx_note_ticks) {
            player_sfx_tick = 0;
            player_sfx_note_idx++;
            if (player_sfx_note_idx >= player_sfx_total_notes) {
                player_sfx_active = false;
                player_sfx_prio   = 0;
                stop_channel(SFX_PLAYER_CH);
            } else {
                sfx_note_on(SFX_PLAYER_CH, player_sfx_patch,
                            player_sfx_notes[player_sfx_note_idx], player_sfx_volume);
            }
        }
    }
}
