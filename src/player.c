#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "sprite_mode_5.h"
#include "player.h"

// Reads tile ID from the tilemap stored in XRAM at grid position (x, y)
static uint8_t get_tile(int16_t x, int16_t y)
{
    if (x < 0 || x >= TILEMAP_WIDTH || y < 0 || y >= TILEMAP_HEIGHT) {
        return MAP_TILE_SOLID; // Out of bounds is solid bedrock
    }
    RIA.addr0 = TILEMAP_DATA + y * TILEMAP_WIDTH + x;
    RIA.step0 = 0;
    return RIA.rw0;
}

// Writes a tile ID directly into the XRAM tilemap at grid position (x, y)
static void set_tile(int16_t x, int16_t y, uint8_t tile_id)
{
    if (x < 0 || x >= TILEMAP_WIDTH || y < 0 || y >= TILEMAP_HEIGHT) {
        return;
    }
    RIA.addr0 = TILEMAP_DATA + y * TILEMAP_WIDTH + x;
    RIA.step0 = 0;
    RIA.rw0 = tile_id;
}

// Helper to check if a tile is empty/passable as space
static bool is_empty_tile(uint8_t tile)
{
    return tile == MAP_TILE_EMPTY || tile == MAP_TILE_FALSE || tile == MAP_TILE_GOLD || tile == MAP_TILE_RUNNER || tile == MAP_TILE_GUARD;
}

// Helper to check if the player can move to a tile
static bool can_move_to(int16_t x, int16_t y)
{
    if (x < 0 || x >= TILEMAP_WIDTH || y < 0 || y >= TILEMAP_HEIGHT) {
        return false;
    }
    uint8_t tile = get_tile(x, y);
    return is_empty_tile(tile) || tile == MAP_TILE_LADDER || tile == MAP_TILE_HLADDER || tile == MAP_TILE_ROPE;
}

// Called at 60 Hz to update coordinates, scroll offsets, and apply physics
void player_update_motion(void)
{

    uint8_t current_tile = get_tile(player.grid_x, player.grid_y);
    uint8_t tile_below = get_tile(player.grid_x, player.grid_y + 1);

    bool is_falling_state = (player.state == RSTATE_FALL_LEFT || player.state == RSTATE_FALL_RIGHT);

    bool should_fall = is_falling_state || (
        (is_empty_tile(tile_below) || tile_below == MAP_TILE_ROPE)
        && current_tile != MAP_TILE_ROPE
        && current_tile != MAP_TILE_LADDER
        && current_tile != MAP_TILE_HLADDER
    );

    if (should_fall) {
        if (!is_falling_state) {
            if (player.state == RSTATE_LEFT || player.state == RSTATE_CLIMB_LEFT) {
                player.state = RSTATE_FALL_LEFT;
            } else {
                player.state = RSTATE_FALL_RIGHT;
            }
        }

        int16_t step = 1;
        uint16_t accum = player.sub_y + 58;
        player.sub_y = (uint8_t)accum;
        step += (accum >> 8);

        player.offset_x = 0; // Centered horizontally
        player.offset_y += step;

        if (player.offset_y > 8) {
            player.grid_y += 1;
            player.offset_y -= 16;
        }

        uint8_t check_tile = get_tile(player.grid_x, player.grid_y);
        uint8_t check_below = get_tile(player.grid_x, player.grid_y + 1);

        if (check_tile == MAP_TILE_ROPE && player.offset_y >= 0 && player.offset_y < step) {
            player.offset_y = 0;
            if (player.state == RSTATE_FALL_LEFT) {
                player.state = RSTATE_CLIMB_LEFT;
            } else {
                player.state = RSTATE_CLIMB_RIGHT;
            }
        }
        else if (player.offset_y >= 0 && (!is_empty_tile(check_below) && check_below != MAP_TILE_ROPE)) {
            player.offset_y = 0;
            player.state = RSTATE_STOP;
        }
    }
    else if (player.dir != DIR_NONE) {
        int16_t step = 1;
        uint16_t accum;

        if (player.dir == DIR_LEFT || player.dir == DIR_RIGHT) {
            accum = player.sub_x + 58;
            player.sub_x = (uint8_t)accum;
            step += (accum >> 8);
        } else {
            accum = player.sub_y + 58;
            player.sub_y = (uint8_t)accum;
            step += (accum >> 8);
        }

        if (player.dir == DIR_LEFT) {
            int16_t next_tx = player.offset_x - step;
            int16_t next_grid_x = player.grid_x;
            if (next_tx < -8) {
                next_grid_x -= 1;
                next_tx += 16;
            }

            if (next_tx < 0 && !can_move_to(next_grid_x - 1, player.grid_y)) {
                player.offset_x = 0;
                player.offset_y = 0;
                player.state = RSTATE_STOP;
            } else {
                player.grid_x = next_grid_x;
                player.offset_x = next_tx;
                player.offset_y = 0;

                uint8_t new_tile = get_tile(player.grid_x, player.grid_y);
                if (new_tile == MAP_TILE_ROPE) {
                    player.state = RSTATE_CLIMB_LEFT;
                } else {
                    player.state = RSTATE_LEFT;
                }
            }
        }
        else if (player.dir == DIR_RIGHT) {
            int16_t next_tx = player.offset_x + step;
            int16_t next_grid_x = player.grid_x;
            if (next_tx > 8) {
                next_grid_x += 1;
                next_tx -= 16;
            }

            if (next_tx > 0 && !can_move_to(next_grid_x + 1, player.grid_y)) {
                player.offset_x = 0;
                player.offset_y = 0;
                player.state = RSTATE_STOP;
            } else {
                player.grid_x = next_grid_x;
                player.offset_x = next_tx;
                player.offset_y = 0;

                uint8_t new_tile = get_tile(player.grid_x, player.grid_y);
                if (new_tile == MAP_TILE_ROPE) {
                    player.state = RSTATE_CLIMB_RIGHT;
                } else {
                    player.state = RSTATE_RIGHT;
                }
            }
        }
        else if (player.dir == DIR_UP) {
            int16_t next_ty = player.offset_y - step;
            int16_t next_grid_y = player.grid_y;
            if (next_ty < -8) {
                next_grid_y -= 1;
                next_ty += 16;
            }

            uint8_t curr_tile = get_tile(player.grid_x, next_grid_y);
            bool on_ladder = (curr_tile == MAP_TILE_LADDER || curr_tile == MAP_TILE_HLADDER);

            if (next_ty < 0 && (!on_ladder || !can_move_to(player.grid_x, next_grid_y - 1))) {
                player.offset_y = 0;
                player.offset_x = 0;
                player.state = RSTATE_STOP;
            } else {
                player.grid_y = next_grid_y;
                player.offset_y = next_ty;
                player.offset_x = 0;
                player.state = RSTATE_UPDOWN;
            }
        }
        else if (player.dir == DIR_DOWN) {
            int16_t next_ty = player.offset_y + step;
            int16_t next_grid_y = player.grid_y;
            if (next_ty > 8) {
                next_grid_y += 1;
                next_ty -= 16;
            }

            uint8_t curr_tile = get_tile(player.grid_x, next_grid_y);

            if (player.state == RSTATE_UPDOWN && next_ty < 0 && is_empty_tile(curr_tile)) {
                player.state = RSTATE_FALL_RIGHT;
                player.grid_y = next_grid_y;
                player.offset_y = next_ty;
                player.offset_x = 0;
            }
            else if (next_ty > 0 && !can_move_to(player.grid_x, next_grid_y + 1)) {
                player.offset_y = 0;
                player.offset_x = 0;
                player.state = RSTATE_STOP;
            }
            else {
                player.grid_y = next_grid_y;
                player.offset_y = next_ty;
                player.offset_x = 0;

                if (curr_tile == MAP_TILE_ROPE) {
                    uint8_t under_rope = get_tile(player.grid_x, player.grid_y + 1);
                    if (under_rope == MAP_TILE_LADDER || under_rope == MAP_TILE_HLADDER) {
                        player.state = RSTATE_UPDOWN;
                    } else {
                        if (player.state == RSTATE_CLIMB_RIGHT) {
                            player.state = RSTATE_FALL_RIGHT;
                        } else {
                            player.state = RSTATE_FALL_LEFT;
                        }
                    }
                } else {
                    player.state = RSTATE_UPDOWN;
                }
            }
        }
    }

    // Calculate world pixels from the grid and offsets
    int16_t wx = (player.grid_x << 4) + player.offset_x;
    int16_t wy = (player.grid_y << 4) + player.offset_y;

    // Convert to target camera scroll positions to center the player
    int16_t target_scroll_x = SCREEN_WIDTH_D2 - wx;
    int16_t target_scroll_y = SCREEN_HEIGHT_D2 - wy;

    // Clamp the camera scroll offset so the tilemap is always fully on-screen
    // The screen size is 320x240, and the tilemap is 448x256 pixels.
    // Scroll range X: [320 - 448, 0] = [-128, 0]
    // Scroll range Y: [240 - 256, 0] = [-16, 0]
    player.world_x_px = target_scroll_x;
    if (player.world_x_px > 0) {
        player.world_x_px = 0;
    } else if (player.world_x_px < -128) {
        player.world_x_px = -128;
    }

    player.world_y_px = target_scroll_y;
    if (player.world_y_px > 0) {
        player.world_y_px = 0;
    } else if (player.world_y_px < -16) {
        player.world_y_px = -16;
    }

    // Calculate player sprite screen coordinates
    player.x_pos_px = wx + player.world_x_px;
    player.y_pos_px = wy + player.world_y_px;

    // Synchronize is_falling boolean for backward compatibility
    player.is_falling = (player.state == RSTATE_FALL_LEFT || player.state == RSTATE_FALL_RIGHT);

    // Update XRAM configurations at 60 Hz
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, player.x_pos_px);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, player.y_pos_px);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, player.world_x_px);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_pos_px, player.world_y_px);
}

// Called at 23 Hz to process inputs and tick core logic
void player_tick_logic(const input_actions_t *actions)
{
    // 1. Gather current center coordinates for item collection
    int16_t px = (player.grid_x << 4) + player.offset_x;
    int16_t py = (player.grid_y << 4) + player.offset_y;
    int16_t center_x = (px + 8) >> 4;
    int16_t center_y = (py + 8) >> 4;

    // 2. If center overlaps with gold, collect it
    if (get_tile(center_x, center_y) == MAP_TILE_GOLD) {
        set_tile(center_x, center_y, MAP_TILE_EMPTY); // Erases gold from screen & logic
    }

    // 3. Process Digging and Movement Inputs (only if not falling or digging)
    bool is_falling_state = (player.state == RSTATE_FALL_LEFT || player.state == RSTATE_FALL_RIGHT);
    bool is_digging_state = (player.state == RSTATE_DIG_LEFT || player.state == RSTATE_DIG_RIGHT);

    if (!is_falling_state && !is_digging_state) {
        // Check if we can trigger digging
        bool on_ground_or_ladder = (!is_empty_tile(get_tile(player.grid_x, player.grid_y + 1)) 
                                    || get_tile(player.grid_x, player.grid_y) == MAP_TILE_LADDER 
                                    || get_tile(player.grid_x, player.grid_y) == MAP_TILE_HLADDER);

        bool did_dig = false;
        if (on_ground_or_ladder) {
            if (actions->fire) { // Dig Left
                if (get_tile(player.grid_x - 1, player.grid_y + 1) == MAP_TILE_BRICK &&
                    is_empty_tile(get_tile(player.grid_x - 1, player.grid_y))) {
                    
                    set_tile(player.grid_x - 1, player.grid_y + 1, MAP_TILE_EMPTY);
                    player.state = RSTATE_DIG_LEFT;
                    player.anim_frame = 0;
                    player.anim_tick = 0;
                    player.dir = DIR_NONE;
                    did_dig = true;
                }
            }
            else if (actions->bomb) { // Dig Right
                if (get_tile(player.grid_x + 1, player.grid_y + 1) == MAP_TILE_BRICK &&
                    is_empty_tile(get_tile(player.grid_x + 1, player.grid_y))) {
                    
                    set_tile(player.grid_x + 1, player.grid_y + 1, MAP_TILE_EMPTY);
                    player.state = RSTATE_DIG_RIGHT;
                    player.anim_frame = 0;
                    player.anim_tick = 0;
                    player.dir = DIR_NONE;
                    did_dig = true;
                }
            }
        }

        if (!did_dig) {
            // Process movement inputs
            const input_actions_t *last = input_last_actions();

            bool new_right = actions->right && !last->right;
            bool new_left = actions->left && !last->left;
            bool new_down = actions->down && !last->down;
            bool new_up = actions->up && !last->up;

            if (new_right) {
                player.dir = DIR_RIGHT;
            } else if (new_left) {
                player.dir = DIR_LEFT;
            } else if (new_down) {
                player.dir = DIR_DOWN;
            } else if (new_up) {
                player.dir = DIR_UP;
            } else {
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
    }

    // 4. Update Animations (for all states, including falling and digging)
    static uint8_t last_state = 0xFF;
    static uint8_t current_frame_idx = 0;

    // Reset animation if state changed (excluding STOP state which preserves last state's frame)
    if (player.state != last_state) {
        if (player.state != RSTATE_STOP) {
            player.anim_frame = 0;
            player.anim_tick = 0;
        }
        last_state = player.state;
    }

    uint8_t state = player.state;
    bool is_animating = (player.dir != DIR_NONE) 
                        || (player.state == RSTATE_FALL_LEFT || player.state == RSTATE_FALL_RIGHT) 
                        || (player.state == RSTATE_DIG_LEFT || player.state == RSTATE_DIG_RIGHT);

    if (state != RSTATE_STOP && is_animating) {
        uint8_t count = 0;
        uint8_t frame_val = 0;
        uint8_t duration_val = 0;

        switch (state) {
            case RSTATE_CLIMB_LEFT:
                count = 3;
                if (player.anim_frame == 0) { frame_val = 12; duration_val = 1; }
                else if (player.anim_frame == 1) { frame_val = 13; duration_val = 2; }
                else { frame_val = 14; duration_val = 2; }
                break;
            case RSTATE_CLIMB_RIGHT:
                count = 3;
                if (player.anim_frame == 0) { frame_val = 9; duration_val = 1; }
                else if (player.anim_frame == 1) { frame_val = 10; duration_val = 2; }
                else { frame_val = 11; duration_val = 2; }
                break;
            case RSTATE_DIG_LEFT:
                count = 1;
                frame_val = 16;
                duration_val = 11;
                break;
            case RSTATE_DIG_RIGHT:
                count = 1;
                frame_val = 15;
                duration_val = 11;
                break;
            case RSTATE_FALL_LEFT:
                count = 1;
                frame_val = 17;
                duration_val = 1;
                break;
            case RSTATE_FALL_RIGHT:
                count = 1;
                frame_val = 8;
                duration_val = 1;
                break;
            case RSTATE_LEFT:
                count = 3;
                frame_val = 3 + player.anim_frame;
                duration_val = 1;
                break;
            case RSTATE_RIGHT:
                count = 3;
                frame_val = 0 + player.anim_frame;
                duration_val = 1;
                break;
            case RSTATE_UPDOWN:
                count = 2;
                frame_val = 6 + player.anim_frame;
                duration_val = 1;
                break;
            default:
                break;
        }

        if (count > 0) {
            player.anim_tick++;
            if (player.anim_tick >= duration_val) {
                player.anim_tick = 0;
                player.anim_frame++;
                if (player.anim_frame >= count) {
                    if (state == RSTATE_DIG_LEFT) {
                        player.state = RSTATE_LEFT;
                        player.anim_frame = 0;
                        player.anim_tick = 0;
                        current_frame_idx = 3; // start of left walk
                    } else if (state == RSTATE_DIG_RIGHT) {
                        player.state = RSTATE_RIGHT;
                        player.anim_frame = 0;
                        player.anim_tick = 0;
                        current_frame_idx = 0; // start of right walk
                    } else {
                        player.anim_frame = 0;
                    }
                }
            }
            if (player.state == state) {
                switch (state) {
                    case RSTATE_CLIMB_LEFT:
                        if (player.anim_frame == 0) { frame_val = 12; }
                        else if (player.anim_frame == 1) { frame_val = 13; }
                        else { frame_val = 14; }
                        break;
                    case RSTATE_CLIMB_RIGHT:
                        if (player.anim_frame == 0) { frame_val = 9; }
                        else if (player.anim_frame == 1) { frame_val = 10; }
                        else { frame_val = 11; }
                        break;
                    case RSTATE_LEFT:
                        frame_val = 3 + player.anim_frame;
                        break;
                    case RSTATE_RIGHT:
                        frame_val = 0 + player.anim_frame;
                        break;
                    case RSTATE_UPDOWN:
                        frame_val = 6 + player.anim_frame;
                        break;
                    default:
                        break;
                }
                current_frame_idx = frame_val;
            }
        }
    }

    // 5. Update sprite pointer in XRAM
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, (PLAYER_DATA + current_frame_idx * PLAYER_FRAME_SIZE));
}
