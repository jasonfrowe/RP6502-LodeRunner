#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

typedef struct player_s {
    int16_t x_pos_px;
    int16_t y_pos_px;
    int16_t world_x_px;
    int16_t world_y_px;
    uint8_t dir;
    uint8_t sub_x;
    uint8_t sub_y;
    bool is_falling;

    // Grid states and offsets matching loderunner-ng
    int16_t grid_x;
    int16_t grid_y;
    int16_t offset_x;
    int16_t offset_y;
    uint8_t state;
    uint8_t anim_frame;
    uint8_t anim_tick;
} player_t;

extern player_t player;

// Guard states
typedef enum {
    GSTATE_CLIMB_LEFT,
    GSTATE_CLIMB_OUT,
    GSTATE_CLIMB_RIGHT,
    GSTATE_FALL_LEFT,
    GSTATE_FALL_RIGHT,
    GSTATE_LEFT,
    GSTATE_REBORN,
    GSTATE_RIGHT,
    GSTATE_STOP,
    GSTATE_TRAP_LEFT,
    GSTATE_TRAP_RIGHT,
    GSTATE_UPDOWN,
    GSTATE_DEAD,
} guard_state_t;

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    int16_t grid_x;
    int16_t grid_y;
    int16_t offset_x;
    int16_t offset_y;
    uint8_t sub_x;
    uint8_t sub_y;
    int16_t start_grid_x;
    int16_t start_grid_y;
    uint8_t state;
    uint8_t anim_frame;
    uint8_t anim_tick;
    uint8_t dir;
    bool hole;
    int16_t holey;
    bool gold;
    int16_t goldholds;
    bool active;
    int16_t stuck_x;
    int16_t stuck_y;
    uint16_t stuck_ticks;
} guard_t;

extern guard_t guards[MAX_ENEMIES];

// Palette extracted from graphics/player.png
static const uint16_t player_4bpp[16] = {
    0x0000,
    0xA820,
    0x0560,
    0xFD21,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF,
    0xFABF,
    0x57FF,
    0xFFFF,
};

void sprite_mode5_players_init(void);
void guards_update_motion(void);
void guards_tick_logic(void);

#endif // SPRITE_MODE5_H