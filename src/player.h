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

// Called at 60 Hz to update the visual scroll positions
void player_update_motion(void);

// Called at 23 Hz to poll direction and perform logic checks
void player_tick_logic(const input_actions_t *actions);

#endif // PLAYER_H
