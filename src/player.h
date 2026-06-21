#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>
#include "input.h"

// Movement Directions
#define DIR_NONE  0
#define DIR_LEFT  1
#define DIR_RIGHT 2
#define DIR_UP    3
#define DIR_DOWN  4

// Runner states matching loderunner-ng
#define RSTATE_CLIMB_LEFT  0
#define RSTATE_CLIMB_RIGHT 1
#define RSTATE_DIG_LEFT    2
#define RSTATE_DIG_RIGHT   3
#define RSTATE_FALL_LEFT   4
#define RSTATE_FALL_RIGHT  5
#define RSTATE_LEFT        6
#define RSTATE_RIGHT       7
#define RSTATE_STOP        8
#define RSTATE_UPDOWN      9

extern int16_t start_grid_x;
extern int16_t start_grid_y;

// Called at 60 Hz to update the visual scroll positions
void player_update_motion(void);

// Called at 23 Hz to poll direction and perform logic checks
void player_tick_logic(const input_actions_t *actions);
void player_die(void);
void clear_all_holes(void);
void save_original_map(void);
void reload_level(void);

extern uint8_t current_level;
void load_level(uint8_t lvl);
void load_next_level(void);

#endif // PLAYER_H
