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

// Called at 60 Hz to update the visual scroll positions
void player_update_motion(void);

// Called at 23 Hz to poll direction and perform logic checks
void player_tick_logic(const input_actions_t *actions);

#endif // PLAYER_H
