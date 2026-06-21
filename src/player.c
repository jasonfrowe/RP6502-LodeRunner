#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "sprite_mode_5.h"
#include "player.h"

// Called at 60 Hz to update the visual scroll positions smoothly
void player_update_motion(void)
{
    if (player.dir == DIR_NONE) {
        return;
    }

    // 1.227 pixels/frame in 8.8 fixed-point:
    // Integer step base = 1
    // Fractional step = 0.227 * 256 = 58
    int16_t step = 1;
    uint16_t accum;

    // Bounds in tilemap space relative to screen center
    int16_t max_scroll_x = SCREEN_WIDTH_D2;
    int16_t min_scroll_x = SCREEN_WIDTH_D2 - (WORLD_WIDTH_PX - TILE_SIZE);
    int16_t max_scroll_y = SCREEN_HEIGHT_D2;
    int16_t min_scroll_y = SCREEN_HEIGHT_D2 - (WORLD_HEIGHT_PX - TILE_SIZE);

    if (player.dir == DIR_RIGHT) {
        accum = player.sub_x + 58;
        player.sub_x = (uint8_t)accum;
        step += (accum >> 8);

        if (player.world_x_px - step < min_scroll_x) {
            player.world_x_px = min_scroll_x;
            player.dir = DIR_NONE;
        } else {
            player.world_x_px -= step;
        }
    } 
    else if (player.dir == DIR_LEFT) {
        accum = player.sub_x + 58;
        player.sub_x = (uint8_t)accum;
        step += (accum >> 8);

        if (player.world_x_px + step > max_scroll_x) {
            player.world_x_px = max_scroll_x;
            player.dir = DIR_NONE;
        } else {
            player.world_x_px += step;
        }
    } 
    else if (player.dir == DIR_DOWN) {
        accum = player.sub_y + 58;
        player.sub_y = (uint8_t)accum;
        step += (accum >> 8);

        if (player.world_y_px - step < min_scroll_y) {
            player.world_y_px = min_scroll_y;
            player.dir = DIR_NONE;
        } else {
            player.world_y_px -= step;
        }
    } 
    else if (player.dir == DIR_UP) {
        accum = player.sub_y + 58;
        player.sub_y = (uint8_t)accum;
        step += (accum >> 8);

        if (player.world_y_px + step > max_scroll_y) {
            player.world_y_px = max_scroll_y;
            player.dir = DIR_NONE;
        } else {
            player.world_y_px += step;
        }
    }

    // Write updated scroll offset directly to Mode 2 configuration in XRAM
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, player.world_x_px);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_pos_px, player.world_y_px);
}

// Called at 23 Hz to poll directions and perform high-level decisions
void player_tick_logic(const input_actions_t *actions)
{
    const input_actions_t *last = input_last_actions();

    // Detect transition from not-pressed to pressed (new keypress)
    bool new_right = actions->right && !last->right;
    bool new_left = actions->left && !last->left;
    bool new_down = actions->down && !last->down;
    bool new_up = actions->up && !last->up;

    // Prioritize the newest keypress
    if (new_right) {
        player.dir = DIR_RIGHT;
    } else if (new_left) {
        player.dir = DIR_LEFT;
    } else if (new_down) {
        player.dir = DIR_DOWN;
    } else if (new_up) {
        player.dir = DIR_UP;
    } else {
        // No new keypresses. Check if the current direction is still being held.
        bool current_held = false;
        if (player.dir == DIR_RIGHT && actions->right) {
            current_held = true;
        } else if (player.dir == DIR_LEFT && actions->left) {
            current_held = true;
        } else if (player.dir == DIR_DOWN && actions->down) {
            current_held = true;
        } else if (player.dir == DIR_UP && actions->up) {
            current_held = true;
        }

        if (!current_held) {
            // Current key was released, fallback to any other key currently held
            if (actions->right) {
                player.dir = DIR_RIGHT;
            } else if (actions->left) {
                player.dir = DIR_LEFT;
            } else if (actions->down) {
                player.dir = DIR_DOWN;
            } else if (actions->up) {
                player.dir = DIR_UP;
            } else {
                player.dir = DIR_NONE;
            }
        }
    }
}
