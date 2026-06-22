#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "constants.h"
#include "sprite_mode_5.h"
#include "player.h"

int16_t start_grid_x = 0;
int16_t start_grid_y = 0;

bool level_started = false;
bool game_paused = false;
bool game_over = false;
uint32_t player_score = 0;
uint8_t player_lives = 5;
static bool ignore_fire = false;
static bool ignore_bomb = false;
static bool wait_for_input_release = true;
static bool game_over_waiting_release = true;

void reset_player_input_state(void)
{
    level_started = false;
    ignore_fire = true;
    ignore_bomb = true;
    wait_for_input_release = true;
    game_paused = false;
}

#define MAX_ACTIVE_HOLES 8

typedef enum {
    HOLE_STATE_DIGGING,
    HOLE_STATE_FILLING
} hole_state_t;

typedef struct {
    int16_t hx;
    int16_t hy;
    bool dig_left;
    hole_state_t state;
    int16_t tick;
    uint8_t original_top_tile;
} active_hole_t;

static active_hole_t active_holes[MAX_ACTIVE_HOLES];
static uint8_t active_holes_count = 0;

static void ai_reborn(uint8_t guard_idx);
static bool check_runner_guard_collision(void);
static bool is_guard_trapped_at(int16_t x, int16_t y);

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
    if (tile >= 16 && tile <= 23) return false;
    if (tile >= 34 && tile <= 41) return false;
    return tile == MAP_TILE_EMPTY || tile == MAP_TILE_FALSE || tile == MAP_TILE_GOLD || tile == MAP_TILE_RUNNER || tile == MAP_TILE_GUARD || (tile >= 7 && tile <= 42) || tile == MAP_TILE_HLADDER;
}

static void add_hole(int16_t hx, int16_t hy, bool dig_left)
{
    if (active_holes_count >= MAX_ACTIVE_HOLES) {
        return;
    }
    
    // Check if there is already a hole at this location to prevent duplicate digging
    for (uint8_t i = 0; i < active_holes_count; i++) {
        if (active_holes[i].hx == hx && active_holes[i].hy == hy) {
            return;
        }
    }
    
    active_hole_t *h = &active_holes[active_holes_count++];
    h->hx = hx;
    h->hy = hy;
    h->dig_left = dig_left;
    h->state = HOLE_STATE_DIGGING;
    h->tick = 0;
    h->original_top_tile = get_tile(hx, hy - 1);
    
    // Set initial frame immediately
    if (dig_left) {
        set_tile(hx, hy - 1, 7);
        set_tile(hx, hy, 16);
    } else {
        set_tile(hx, hy - 1, 25);
        set_tile(hx, hy, 34);
    }
}

void clear_all_holes(void)
{
    for (uint8_t i = 0; i < active_holes_count; i++) {
        active_hole_t *h = &active_holes[i];
        if (h->state == HOLE_STATE_DIGGING) {
            set_tile(h->hx, h->hy - 1, h->original_top_tile);
        }
        set_tile(h->hx, h->hy, MAP_TILE_BRICK);
    }
    active_holes_count = 0;
}



void player_die(void)
{
    if (player_lives > 0) {
        player_lives--;
    }
    update_hud(); // Sync final 0 lives display
    if (player_lives == 0) {
        game_over = true;
        game_over_waiting_release = true;
        // Display "GAME OVER" in the middle of text layer
        RIA.addr0 = TEXT_TILES_MAP_DATA + 145;
        RIA.step0 = 1;
        RIA.rw0 = 17; // G
        RIA.rw0 = 11; // A
        RIA.rw0 = 23; // M
        RIA.rw0 = 15; // E
        RIA.rw0 = 0;  // Space
        RIA.rw0 = 25; // O
        RIA.rw0 = 32; // V
        RIA.rw0 = 15; // E
        RIA.rw0 = 28; // R
    } else {
        reload_level();
    }
}

void reveal_hidden_ladders(void)
{
    RIA.addr0 = TILEMAP_DATA;
    RIA.step0 = 1;
    for (int i = 0; i < TILEMAP_WIDTH * TILEMAP_HEIGHT; i++) {
        uint8_t tile = RIA.rw0;
        if (tile == MAP_TILE_HLADDER) {
            RIA.addr0 = TILEMAP_DATA + i;
            RIA.step0 = 0;
            RIA.rw0 = MAP_TILE_LADDER;
            
            RIA.addr0 = TILEMAP_DATA + i + 1;
            RIA.step0 = 1;
        }
    }
}

static void tick_holes(void)
{
    for (int i = 0; i < (int)active_holes_count; ) {
        active_hole_t *h = &active_holes[i];
        
        if (h->state == HOLE_STATE_DIGGING) {
            h->tick++;
            if (h->tick >= 11) {
                // Digging finished, transition to filling
                h->state = HOLE_STATE_FILLING;
                h->tick = 0;
                // Restore top tile to original
                set_tile(h->hx, h->hy - 1, h->original_top_tile);
                // Set bottom tile to initial fill frame
                set_tile(h->hx, h->hy, 33);
                i++;
            } else {
                // Update digging frame in XRAM
                uint8_t dig_frame = 0;
                int16_t t = h->tick;
                if (t < 1) dig_frame = 0;
                else if (t < 2) dig_frame = 1;
                else if (t < 4) dig_frame = 2;
                else if (t < 5) dig_frame = 3;
                else if (t < 7) dig_frame = 4;
                else if (t < 8) dig_frame = 5;
                else if (t < 10) dig_frame = 6;
                else dig_frame = 7;
                
                if (h->dig_left) {
                    set_tile(h->hx, h->hy - 1, 7 + dig_frame);
                    set_tile(h->hx, h->hy, 16 + dig_frame);
                } else {
                    set_tile(h->hx, h->hy - 1, 25 + dig_frame);
                    set_tile(h->hx, h->hy, 34 + dig_frame);
                }
                i++;
            }
        }
        else if (h->state == HOLE_STATE_FILLING) {
            h->tick++;
            if (h->tick >= 186) {
                // Filling finished, restore brick!
                set_tile(h->hx, h->hy, MAP_TILE_BRICK);
                
                // Check if player is crushed inside this hole
                if (player.grid_x == h->hx && player.grid_y == h->hy) {
                    player_die();
                }

                // Check if any guard is crushed inside this hole
                bool guard_crushed = false;
                for (uint8_t g_idx = 0; g_idx < MAX_ENEMIES; g_idx++) {
                    guard_t *g = &guards[g_idx];
                    if (g->active && g->grid_x == h->hx && g->grid_y == h->hy) {
                        ai_reborn(g_idx);
                        player_score += 75;
                        guard_crushed = true;
                    }
                }
                if (guard_crushed) {
                    update_hud();
                }
                
                // Remove hole from list by swapping with last
                if (active_holes_count > 0) {
                    active_holes[i] = active_holes[active_holes_count - 1];
                    active_holes_count--;
                }
            } else {
                // Update refill frame in XRAM
                uint8_t fill_tile = 33;
                int16_t t = h->tick;
                if (t < 166) fill_tile = 33;
                else if (t < 174) fill_tile = 24;
                else if (t < 182) fill_tile = 15;
                else fill_tile = 42;
                
                set_tile(h->hx, h->hy, fill_tile);
                i++;
            }
        }
    }
}

// Helper to check if the player can move to a tile
static bool can_move_to(int16_t x, int16_t y)
{
    if (x < 0 || x >= TILEMAP_WIDTH || y < 0 || y >= TILEMAP_HEIGHT) {
        return false;
    }
    uint8_t tile = get_tile(x, y);
    return is_empty_tile(tile) || tile == MAP_TILE_LADDER || tile == MAP_TILE_ROPE;
}

// Called at 60 Hz to update coordinates, scroll offsets, and apply physics
void player_update_motion(void)
{
    if (game_paused || game_over) return;
    if (!level_started) return;

    uint8_t current_tile = get_tile(player.grid_x, player.grid_y);
    uint8_t tile_below = get_tile(player.grid_x, player.grid_y + 1);

    bool is_falling_state = (player.state == RSTATE_FALL_LEFT || player.state == RSTATE_FALL_RIGHT);

    bool should_fall = is_falling_state || (
        (is_empty_tile(tile_below) || tile_below == MAP_TILE_ROPE)
        && current_tile != MAP_TILE_ROPE
        && current_tile != MAP_TILE_LADDER
        && !is_guard_trapped_at(player.grid_x, player.grid_y + 1)
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
        else if (player.offset_y >= 0 && ((!is_empty_tile(check_below) && check_below != MAP_TILE_ROPE) || is_guard_trapped_at(player.grid_x, player.grid_y + 1))) {
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
            bool on_ladder = (curr_tile == MAP_TILE_LADDER);

            if (next_ty < 0 && (!on_ladder || !can_move_to(player.grid_x, next_grid_y - 1))) {
                player.offset_y = 0;
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
                player.state = RSTATE_STOP;
            }
            else {
                player.grid_y = next_grid_y;
                player.offset_y = next_ty;
                player.offset_x = 0;

                if (curr_tile == MAP_TILE_ROPE) {
                    uint8_t under_rope = get_tile(player.grid_x, player.grid_y + 1);
                    if (under_rope == MAP_TILE_LADDER) {
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
    int16_t target_scroll_y = 112 - wy;

    // Clamp the camera scroll offset so the tilemap is always fully on-screen
    // The screen size is 320x240, and the tilemap is 448x256 pixels.
    // Scroll range X: [320 - 448, 0] = [-128, 0]
    // Scroll range Y: [224 - 256, 0] = [-32, 0] (leaving bottom 16 pixels for HUD)
    player.world_x_px = target_scroll_x;
    if (player.world_x_px > 0) {
        player.world_x_px = 0;
    } else if (player.world_x_px < -128) {
        player.world_x_px = -128;
    }

    player.world_y_px = target_scroll_y;
    if (player.world_y_px > 0) {
        player.world_y_px = 0;
    } else if (player.world_y_px < -32) {
        player.world_y_px = -32;
    }

    // Calculate player sprite screen coordinates
    player.x_pos_px = wx + player.world_x_px;
    player.y_pos_px = wy + player.world_y_px;

    // Check collision with guards
    if (check_runner_guard_collision()) {
        player_die();
        return;
    }

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
    // 1. Handle Game Over state
    if (game_over) {
        bool button_pressed = (actions->left || actions->right || actions->up || actions->down || 
                               actions->fire || actions->bomb || actions->start);
        if (game_over_waiting_release) {
            if (!button_pressed) {
                game_over_waiting_release = false;
            }
            return;
        }
        
        if (button_pressed) {
            // Clear "GAME OVER" text
            RIA.addr0 = TEXT_TILES_MAP_DATA + 145;
            RIA.step0 = 1;
            for (int i = 0; i < 9; i++) {
                RIA.rw0 = 0;
            }
            
            player_score = 0;
            player_lives = 5;
            current_level = 1;
            game_over = false;
            load_level(1);
        }
        return;
    }

    // 2. Handle Pause input and state
    static bool start_was_pressed = false;
    bool start_pressed = actions->start;
    bool start_triggered = start_pressed && !start_was_pressed;
    start_was_pressed = start_pressed;

    if (start_triggered) {
        if (game_paused) {
            game_paused = false;
            // Clear "PAUSED" text from the screen
            RIA.addr0 = TEXT_TILES_MAP_DATA + 147;
            RIA.step0 = 1;
            for (int i = 0; i < 6; i++) {
                RIA.rw0 = 0;
            }
        } else {
            game_paused = true;
            // Draw "PAUSED"
            RIA.addr0 = TEXT_TILES_MAP_DATA + 147;
            RIA.step0 = 1;
            RIA.rw0 = 26; // P
            RIA.rw0 = 11; // A
            RIA.rw0 = 31; // U
            RIA.rw0 = 29; // S
            RIA.rw0 = 15; // E
            RIA.rw0 = 14; // D
        }
    }

    if (game_paused) {
        return;
    }

    // 3. Handle waiting for input release after respawn/load
    if (wait_for_input_release) {
        bool button_pressed = (actions->left || actions->right || actions->up || actions->down || 
                               actions->fire || actions->bomb);
        if (!button_pressed) {
            wait_for_input_release = false;
        }
        return;
    }

    // 4. Handle starting the level
    if (!level_started) {
        if (actions->left || actions->right || actions->up || actions->down || actions->fire || actions->bomb) {
            level_started = true;
        } else {
            return;
        }
    }

    // 0. Update active holes
    tick_holes();

    // Update guard AI and animations at 23 Hz
    guards_tick_logic();

    // Check if player is crushed by a brick (only when grid-aligned to avoid false triggers during motion transitions)
    if (player.offset_x == 0 && player.offset_y == 0 && get_tile(player.grid_x, player.grid_y) == MAP_TILE_BRICK) {
        player_die();
        return;
    }

    // 1. Gather current center coordinates for item collection
    int16_t px = (player.grid_x << 4) + player.offset_x;
    int16_t py = (player.grid_y << 4) + player.offset_y;
    int16_t center_x = (px + 8) >> 4;
    int16_t center_y = (py + 8) >> 4;

    // 2. If center overlaps with gold, collect it
    if (get_tile(center_x, center_y) == MAP_TILE_GOLD) {
        set_tile(center_x, center_y, MAP_TILE_EMPTY); // Erases gold from screen & logic
        player_score += 250;
        update_hud();
        
        // Check if there is any gold left on the map or held by guards
        uint16_t gold_left = 0;
        RIA.addr0 = TILEMAP_DATA;
        RIA.step0 = 1;
        for (int i = 0; i < TILEMAP_WIDTH * TILEMAP_HEIGHT; i++) {
            if (RIA.rw0 == MAP_TILE_GOLD) {
                gold_left++;
            }
        }
        
        for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
            if (guards[i].active && guards[i].gold) {
                gold_left++;
            }
        }
        
        if (gold_left == 0) {
            reveal_hidden_ladders();
        }
    }

    // Check if player has reached the top row on a ladder (Level Win condition)
    if (player.grid_y == 0 && get_tile(player.grid_x, player.grid_y) == MAP_TILE_LADDER) {
        load_next_level();
        return;
    }

    // 3. Process Digging and Movement Inputs (only if not falling or digging)
    bool is_falling_state = (player.state == RSTATE_FALL_LEFT || player.state == RSTATE_FALL_RIGHT);
    bool is_digging_state = (player.state == RSTATE_DIG_LEFT || player.state == RSTATE_DIG_RIGHT);

    if (!is_falling_state && !is_digging_state) {
        // Check if we can trigger digging
        bool on_ground_or_ladder = (!is_empty_tile(get_tile(player.grid_x, player.grid_y + 1)) 
                                    || get_tile(player.grid_x, player.grid_y) == MAP_TILE_LADDER);

        bool did_dig = false;
        if (on_ground_or_ladder) {
            if (actions->fire) { // Dig Left
                if (!ignore_fire) {
                    if (get_tile(player.grid_x - 1, player.grid_y + 1) == MAP_TILE_BRICK &&
                        is_empty_tile(get_tile(player.grid_x - 1, player.grid_y))) {
                        
                        add_hole(player.grid_x - 1, player.grid_y + 1, true);
                        player.state = RSTATE_DIG_LEFT;
                        player.anim_frame = 0;
                        player.anim_tick = 0;
                        player.dir = DIR_NONE;
                        did_dig = true;
                    }
                }
            } else {
                ignore_fire = false;
            }

            if (!did_dig) {
                if (actions->bomb) { // Dig Right
                    if (!ignore_bomb) {
                        if (get_tile(player.grid_x + 1, player.grid_y + 1) == MAP_TILE_BRICK &&
                            is_empty_tile(get_tile(player.grid_x + 1, player.grid_y))) {
                            
                            add_hole(player.grid_x + 1, player.grid_y + 1, false);
                            player.state = RSTATE_DIG_RIGHT;
                            player.anim_frame = 0;
                            player.anim_tick = 0;
                            player.dir = DIR_NONE;
                            did_dig = true;
                        }
                    }
                } else {
                    ignore_bomb = false;
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

// ==========================================
// Guard AI, Physics, and Animations
// ==========================================

#define DIR_FALL 5

#define RATING_MAX 255
#define RATING_BASE_BELLOW 200
#define RATING_BASE_ABOVE 100

static const uint8_t move_policy[4][6] = {
    {0, 0, 0, 0, 0, 0}, // 0 guards
    {0, 1, 1, 0, 1, 1}, // 1 guard
    {1, 1, 1, 1, 1, 1}, // 2 guards
    {1, 2, 1, 1, 2, 1}  // 3 guards
};

static bool is_active_hole(int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < active_holes_count; i++) {
        if (active_holes[i].hx == x && active_holes[i].hy == y) {
            return true;
        }
    }
    return false;
}

static bool is_hole_fully_formed(int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < active_holes_count; i++) {
        if (active_holes[i].hx == x && active_holes[i].hy == y) {
            return active_holes[i].state == HOLE_STATE_FILLING;
        }
    }
    return false;
}

static bool guard_occupied(uint8_t me_idx, int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (i == me_idx) continue;
        guard_t *g = &guards[i];
        if (g->active && g->grid_x == x && g->grid_y == y) {
            return true;
        }
    }
    return false;
}

static bool tile_t_nh(int16_t x, int16_t y, uint8_t t)
{
    if (x < 0 || x >= TILEMAP_WIDTH || y < 0 || y >= TILEMAP_HEIGHT) {
        return t == MAP_TILE_SOLID;
    }
    uint8_t tile = get_tile(x, y);
    if (t == MAP_TILE_BRICK) {
        return tile == MAP_TILE_BRICK || is_active_hole(x, y);
    }
    return tile == t;
}

static bool ai_falling(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    int16_t x = g->grid_x;
    int16_t y = g->grid_y;
    int16_t ty = g->offset_y;

    uint8_t curr_tile = get_tile(x, y);
    if (curr_tile == MAP_TILE_LADDER || 
        (curr_tile == MAP_TILE_ROPE && (ty > -4 && ty <= 4))) {
        return false;
    }

    uint8_t tile_below = get_tile(x, y + 1);
    bool passable_below = is_empty_tile(tile_below) || tile_below == MAP_TILE_ROPE;

    if (ty < 0 || (y < TILEMAP_HEIGHT - 1 && passable_below && !guard_occupied(guard_idx, x, y + 1))) {
        return true;
    }

    return false;
}

static uint8_t ai_scan_level(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    int16_t rx = player.grid_x;
    int16_t ry = player.grid_y;
    int16_t gx = g->grid_x;
    int16_t gy = g->grid_y;

    if (ry != gy) {
        return DIR_NONE;
    }

    while (gx != rx) {
        uint8_t lvl = get_tile(gx, gy);
        uint8_t nextlvl = (gy < TILEMAP_HEIGHT - 1) ? get_tile(gx, gy + 1) : MAP_TILE_SOLID;
        bool hole = is_active_hole(gx, gy + 1);
        bool gd = guard_occupied(guard_idx, gx, gy + 1);

        if (lvl == MAP_TILE_LADDER || lvl == MAP_TILE_ROPE
            || (nextlvl == MAP_TILE_EMPTY && hole)
            || nextlvl == MAP_TILE_SOLID
            || nextlvl == MAP_TILE_LADDER
            || nextlvl == MAP_TILE_BRICK
            || gd) {
            if (gx < rx) {
                gx++;
            } else {
                gx--;
            }
        } else {
            break;
        }
    }

    if (gx == rx) {
        if (g->grid_x < rx) {
            return DIR_RIGHT;
        } else if (g->grid_x > rx) {
            return DIR_LEFT;
        } else {
            if (g->offset_x < player.offset_x) {
                return DIR_RIGHT;
            } else {
                return DIR_LEFT;
            }
        }
    }

    return DIR_NONE;
}

static uint8_t ai_scan_down(int16_t x, int16_t y, int16_t startx)
{
    if (y == TILEMAP_HEIGHT - 1
        || tile_t_nh(x, y + 1, MAP_TILE_BRICK)
        || tile_t_nh(x, y + 1, MAP_TILE_SOLID)) {
        return RATING_MAX;
    }

    while (y < TILEMAP_HEIGHT - 1 && !tile_t_nh(x, y + 1, MAP_TILE_BRICK)
           && !tile_t_nh(x, y + 1, MAP_TILE_SOLID)) {

        if (!tile_t_nh(x, y, MAP_TILE_EMPTY)) {
            if (x > 0) {
                if (tile_t_nh(x - 1, y + 1, MAP_TILE_BRICK)
                    || tile_t_nh(x - 1, y + 1, MAP_TILE_SOLID)
                    || tile_t_nh(x - 1, y + 1, MAP_TILE_LADDER)
                    || tile_t_nh(x - 1, y, MAP_TILE_ROPE)) {
                    if (y >= player.grid_y) {
                        break;
                    }
                }
            }
            if (x < TILEMAP_WIDTH - 1) {
                if (tile_t_nh(x + 1, y + 1, MAP_TILE_BRICK)
                    || tile_t_nh(x + 1, y + 1, MAP_TILE_SOLID)
                    || tile_t_nh(x + 1, y + 1, MAP_TILE_LADDER)
                    || tile_t_nh(x + 1, y, MAP_TILE_ROPE)) {
                    if (y >= player.grid_y) {
                        break;
                    }
                }
            }
        }
        y++;
    }

    int16_t dist = abs(startx - x);
    if (y == player.grid_y) {
        return dist;
    } else if (y > player.grid_y) {
        return RATING_BASE_BELLOW + (y - player.grid_y);
    } else {
        return RATING_BASE_ABOVE + (player.grid_y - y);
    }
}

static uint8_t ai_scan_up(int16_t x, int16_t y, int16_t startx)
{
    if (!tile_t_nh(x, y, MAP_TILE_LADDER)) {
        return RATING_MAX;
    }

    while (y > 0 && (tile_t_nh(x, y, MAP_TILE_LADDER))) {
        y--;

        if (x > 0) {
            if (tile_t_nh(x - 1, y + 1, MAP_TILE_BRICK)
                || tile_t_nh(x - 1, y + 1, MAP_TILE_SOLID)
                || tile_t_nh(x - 1, y + 1, MAP_TILE_LADDER)
                || tile_t_nh(x - 1, y, MAP_TILE_ROPE)) {
                if (y <= player.grid_y) {
                    break;
                }
            }
        }
        if (x < TILEMAP_WIDTH - 1) {
            if (tile_t_nh(x + 1, y + 1, MAP_TILE_BRICK)
                || tile_t_nh(x + 1, y + 1, MAP_TILE_SOLID)
                || tile_t_nh(x + 1, y + 1, MAP_TILE_LADDER)
                || tile_t_nh(x + 1, y, MAP_TILE_ROPE)) {
                if (y <= player.grid_y) {
                    break;
                }
            }
        }
    }

    int16_t dist = abs(startx - x);
    if (y == player.grid_y) {
        return dist;
    } else if (y > player.grid_y) {
        return RATING_BASE_BELLOW + (y - player.grid_y);
    } else {
        return RATING_BASE_ABOVE + (player.grid_y - y);
    }
}

static uint8_t ai_scan_horizontal(int16_t x, int16_t y, bool left)
{
    uint8_t rating = RATING_MAX;
    int16_t startx = x;
    int16_t dx = left ? -1 : 1;

    for (;;) {
        if (left && x == 0) {
            break;
        } else if (!left && x == TILEMAP_WIDTH - 1) {
            break;
        }

        if (get_tile(x + dx, y) == MAP_TILE_BRICK || get_tile(x + dx, y) == MAP_TILE_SOLID) {
            break;
        }

        bool climb = tile_t_nh(x + dx, y, MAP_TILE_LADDER)
            || tile_t_nh(x + dx, y, MAP_TILE_ROPE);

        bool walk = (y == TILEMAP_HEIGHT - 1)
            || (tile_t_nh(x + dx, y + 1, MAP_TILE_BRICK)
                || tile_t_nh(x + dx, y + 1, MAP_TILE_SOLID)
                || tile_t_nh(x + dx, y + 1, MAP_TILE_LADDER));

        x += dx;

        uint8_t r = ai_scan_down(x, y, startx);
        uint8_t q = ai_scan_up(x, y, startx);
        if (q < r) {
            r = q;
        }
        if (r <= rating) {
            rating = r;
        }

        if (!climb && !walk) {
            break;
        }
    }

    return rating;
}

static uint8_t ai_scan(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    bool no_down = g->hole;

    if (g->state == GSTATE_CLIMB_OUT) {
        return DIR_UP;
    }
    if (!no_down && ai_falling(guard_idx)) {
        return DIR_FALL;
    }

    uint8_t d = ai_scan_level(guard_idx);
    if (d != DIR_NONE) {
        return d;
    }

    uint8_t score = RATING_MAX;
    uint8_t s;
    
    if (!no_down) {
        s = ai_scan_down(g->grid_x, g->grid_y, g->grid_x);
        if (s < score) {
            score = s;
            d = DIR_DOWN;
        }
    }
    
    s = ai_scan_up(g->grid_x, g->grid_y, g->grid_x);
    if (s < score) {
        score = s;
        d = DIR_UP;
    }
    
    s = ai_scan_horizontal(g->grid_x, g->grid_y, true); // left
    if (s < score) {
        score = s;
        d = DIR_LEFT;
    }
    
    s = ai_scan_horizontal(g->grid_x, g->grid_y, false); // right
    if (s < score) {
        score = s;
        d = DIR_RIGHT;
    }

    return d;
}

static void ai_reborn(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    
    int16_t x = rand() % TILEMAP_WIDTH;
    int16_t y = 1;
    int16_t start_x = x;
    
    while (get_tile(x, y) != MAP_TILE_EMPTY || get_tile(x, y + 1) == MAP_TILE_EMPTY) {
        x = (x + 1) % TILEMAP_WIDTH;
        if (x == start_x) {
            y++;
            if (y >= TILEMAP_HEIGHT) {
                x = g->start_grid_x;
                y = g->start_grid_y;
                break;
            }
        }
    }
    
    g->grid_x = x;
    g->grid_y = y;
    g->offset_x = 0;
    g->offset_y = 0;
    g->hole = false;
    g->holey = -1;
    g->state = GSTATE_REBORN;
    g->anim_frame = 0;
    g->anim_tick = 0;
    g->dir = DIR_NONE;
    
    if (g->gold) {
        g->gold = false;
        if (get_tile(x, y - 1) == MAP_TILE_EMPTY) {
            set_tile(x, y - 1, MAP_TILE_GOLD);
        }
    }
    g->goldholds = 0;
}

static void ai_drop_gold(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    int16_t x = g->grid_x;
    int16_t y = g->grid_y;

    if (g->goldholds < 0) {
        g->goldholds++;
    } else if (g->goldholds == 0
               && g->gold
               && get_tile(x, y) == MAP_TILE_EMPTY
               && (y == TILEMAP_HEIGHT - 1
                    || get_tile(x, y + 1) == MAP_TILE_BRICK
                    || get_tile(x, y + 1) == MAP_TILE_SOLID
                    || get_tile(x, y + 1) == MAP_TILE_LADDER)) {
        set_tile(x, y, MAP_TILE_GOLD);
        g->gold = false;
        g->goldholds = -1;
    } else if (g->goldholds > 0) {
        g->goldholds--;
    }
}

static void ai_drop_gold_trapped(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    if (!g->gold) return;

    int16_t x = g->grid_x;
    int16_t y = g->grid_y;
    if (get_tile(x, y - 1) == MAP_TILE_EMPTY) {
        set_tile(x, y - 1, MAP_TILE_GOLD);
    }
    g->gold = false;
    g->goldholds = -2;
}

static void update_guard_animation(uint8_t guard_idx)
{
    guard_t *g = &guards[guard_idx];
    if (!g->active) return;

    uint8_t count = 0;
    uint8_t frame_val = 21;
    uint8_t duration_val = 2;

    switch (g->state) {
        case GSTATE_LEFT:
            count = 3;
            frame_val = 21 + g->anim_frame;
            duration_val = 2;
            break;
        case GSTATE_RIGHT:
            count = 3;
            frame_val = 18 + g->anim_frame;
            duration_val = 2;
            break;
        case GSTATE_UPDOWN:
        case GSTATE_CLIMB_OUT:
            count = 2;
            frame_val = 24 + g->anim_frame;
            duration_val = 1;
            break;
        case GSTATE_CLIMB_LEFT:
            count = 3;
            if (g->anim_frame == 0) { frame_val = 32; duration_val = 1; }
            else if (g->anim_frame == 1) { frame_val = 33; duration_val = 2; }
            else { frame_val = 34; duration_val = 2; }
            break;
        case GSTATE_CLIMB_RIGHT:
            count = 3;
            if (g->anim_frame == 0) { frame_val = 29; duration_val = 1; }
            else if (g->anim_frame == 1) { frame_val = 30; duration_val = 2; }
            else { frame_val = 31; duration_val = 2; }
            break;
        case GSTATE_FALL_LEFT:
            count = 1;
            frame_val = 37;
            duration_val = 1;
            break;
        case GSTATE_FALL_RIGHT:
            count = 1;
            frame_val = 26;
            duration_val = 1;
            break;
        case GSTATE_REBORN:
            count = 2;
            if (g->anim_frame == 0) { frame_val = 35; duration_val = 6; }
            else { frame_val = 36; duration_val = 2; }
            break;
        case GSTATE_TRAP_LEFT:
            count = 6;
            if (g->anim_frame == 0) { frame_val = 37; duration_val = 37; }
            else if (g->anim_frame == 1) { frame_val = 38; duration_val = 3; }
            else if (g->anim_frame == 2) { frame_val = 39; duration_val = 3; }
            else if (g->anim_frame == 3) { frame_val = 38; duration_val = 3; }
            else if (g->anim_frame == 4) { frame_val = 39; duration_val = 3; }
            else { frame_val = 37; duration_val = 3; }
            break;
        case GSTATE_TRAP_RIGHT:
            count = 6;
            if (g->anim_frame == 0) { frame_val = 26; duration_val = 37; }
            else if (g->anim_frame == 1) { frame_val = 27; duration_val = 3; }
            else if (g->anim_frame == 2) { frame_val = 28; duration_val = 3; }
            else if (g->anim_frame == 3) { frame_val = 27; duration_val = 3; }
            else if (g->anim_frame == 4) { frame_val = 28; duration_val = 3; }
            else { frame_val = 26; duration_val = 3; }
            break;
        case GSTATE_STOP:
            count = 0;
            break;
    }

    if (count > 0) {
        g->anim_tick++;
        if (g->anim_tick >= duration_val) {
            g->anim_tick = 0;
            g->anim_frame++;
            if (g->anim_frame >= count) {
                if (g->state == GSTATE_TRAP_LEFT || g->state == GSTATE_TRAP_RIGHT) {
                    g->state = GSTATE_CLIMB_OUT;
                    g->anim_frame = 0;
                    g->anim_tick = 0;
                    g->hole = true;
                    g->offset_y = 8;
                    g->grid_y -= 1;
                } else if (g->state == GSTATE_REBORN) {
                    g->state = GSTATE_FALL_RIGHT;
                    g->anim_frame = 0;
                    g->anim_tick = 0;
                } else {
                    g->anim_frame = 0;
                }
            }
        }
        
        if (g->state == GSTATE_CLIMB_LEFT) {
            if (g->anim_frame == 0) frame_val = 32;
            else if (g->anim_frame == 1) frame_val = 33;
            else frame_val = 34;
        } else if (g->state == GSTATE_CLIMB_RIGHT) {
            if (g->anim_frame == 0) frame_val = 29;
            else if (g->anim_frame == 1) frame_val = 30;
            else frame_val = 31;
        } else if (g->state == GSTATE_LEFT) {
            frame_val = 21 + g->anim_frame;
        } else if (g->state == GSTATE_RIGHT) {
            frame_val = 18 + g->anim_frame;
        } else if (g->state == GSTATE_UPDOWN || g->state == GSTATE_CLIMB_OUT) {
            frame_val = 24 + g->anim_frame;
        } else if (g->state == GSTATE_REBORN) {
            if (g->anim_frame == 0) frame_val = 35;
            else frame_val = 36;
        } else if (g->state == GSTATE_TRAP_LEFT) {
            if (g->anim_frame == 0) frame_val = 37;
            else if (g->anim_frame == 1) frame_val = 38;
            else if (g->anim_frame == 2) frame_val = 39;
            else if (g->anim_frame == 3) frame_val = 38;
            else if (g->anim_frame == 4) frame_val = 39;
            else frame_val = 37;
        } else if (g->state == GSTATE_TRAP_RIGHT) {
            if (g->anim_frame == 0) frame_val = 26;
            else if (g->anim_frame == 1) frame_val = 27;
            else if (g->anim_frame == 2) frame_val = 28;
            else if (g->anim_frame == 3) frame_val = 27;
            else if (g->anim_frame == 4) frame_val = 28;
            else frame_val = 26;
        }
    }

    unsigned ptr = ENEMY_CONFIG + (guard_idx * sizeof(vga_mode5_sprite_t));
    xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, (PLAYER_DATA + frame_val * PLAYER_FRAME_SIZE));
}

static bool check_runner_guard_collision(void)
{
    int16_t px = (player.grid_x << 4) + player.offset_x;
    int16_t py = (player.grid_y << 4) + player.offset_y;

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        guard_t *g = &guards[i];
        if (g->active && g->state != GSTATE_REBORN) {
            // Ignore collision if player is standing on the head of a trapped or climbing-out guard
            if (g->state == GSTATE_TRAP_LEFT || g->state == GSTATE_TRAP_RIGHT) {
                if (player.grid_y != g->grid_y) {
                    continue;
                }
            }
            if (g->state == GSTATE_CLIMB_OUT) {
                if (player.grid_y != g->grid_y + 1) {
                    continue;
                }
            }

            int16_t gx = (g->grid_x << 4) + g->offset_x;
            int16_t gy = (g->grid_y << 4) + g->offset_y;

            if (abs(px - gx) < 10 && abs(py - gy) < 10) {
                return true;
            }
        }
    }
    return false;
}

static bool is_guard_trapped_at(int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        guard_t *g = &guards[i];
        if (g->active && g->grid_x == x) {
            if (g->grid_y == y && (g->state == GSTATE_TRAP_LEFT || g->state == GSTATE_TRAP_RIGHT)) {
                return true;
            }
            if (g->grid_y == y - 1 && g->state == GSTATE_CLIMB_OUT) {
                return true;
            }
        }
    }
    return false;
}

void guards_update_motion(void)
{
    if (game_paused || game_over) return;
    if (!level_started) {
        for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
            guard_t *g = &guards[i];
            unsigned ptr = ENEMY_CONFIG + (i * sizeof(vga_mode5_sprite_t));
            if (!g->active) {
                xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32);
                xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
            } else {
                xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, g->x_pos_px);
                xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, g->y_pos_px);
            }
        }
        return;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        guard_t *g = &guards[i];
        if (!g->active) {
            unsigned ptr = ENEMY_CONFIG + (i * sizeof(vga_mode5_sprite_t));
            xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32);
            xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
            continue;
        }

        if (g->state == GSTATE_REBORN || g->state == GSTATE_TRAP_LEFT || g->state == GSTATE_TRAP_RIGHT) {
            g->x_pos_px = (g->grid_x << 4) + player.world_x_px;
            g->y_pos_px = (g->grid_y << 4) + player.world_y_px;
            unsigned ptr = ENEMY_CONFIG + (i * sizeof(vga_mode5_sprite_t));
            xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, g->x_pos_px);
            xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, g->y_pos_px);
            continue;
        }

        uint8_t current_tile = get_tile(g->grid_x, g->grid_y);
        uint8_t tile_below = get_tile(g->grid_x, g->grid_y + 1);

        bool is_falling_state = (g->state == GSTATE_FALL_LEFT || g->state == GSTATE_FALL_RIGHT);
        bool should_fall = is_falling_state || (
            (is_empty_tile(tile_below) || tile_below == MAP_TILE_ROPE)
            && current_tile != MAP_TILE_ROPE
            && current_tile != MAP_TILE_LADDER
            && !g->hole
            && !guard_occupied(i, g->grid_x, g->grid_y + 1)
            && !is_guard_trapped_at(g->grid_x, g->grid_y + 1)
        );

        if (should_fall) {
            if (!is_falling_state) {
                if (g->state == GSTATE_LEFT || g->state == GSTATE_CLIMB_LEFT) {
                    g->state = GSTATE_FALL_LEFT;
                } else {
                    g->state = GSTATE_FALL_RIGHT;
                }
                g->dir = DIR_FALL;
            }

            int16_t step = 1;
            uint16_t accum = g->sub_y + 26;
            g->sub_y = (uint8_t)accum;
            step += (accum >> 8);

            g->offset_x = 0;
            g->offset_y += step;

            if (g->offset_y > 8) {
                g->grid_y += 1;
                g->offset_y -= 16;
            }

            if (g->offset_y <= 0) {
                if (is_hole_fully_formed(g->grid_x, g->grid_y)) {
                    g->state = (g->state == GSTATE_FALL_LEFT) ? GSTATE_TRAP_LEFT : GSTATE_TRAP_RIGHT;
                    g->offset_x = 0;
                    g->offset_y = 0;
                    g->hole = true;
                    g->holey = g->grid_y;
                    ai_drop_gold_trapped(i);
                    g->anim_frame = 0;
                    g->anim_tick = 0;
                    g->dir = DIR_NONE;
                    player_score += 75;
                    update_hud();
                }
            }

            bool blocked_below = (!is_empty_tile(tile_below) && tile_below != MAP_TILE_ROPE) || guard_occupied(i, g->grid_x, g->grid_y + 1);
            if (g->offset_y >= 0 && blocked_below) {
                g->offset_y = 0;
                g->state = GSTATE_STOP;
                g->dir = DIR_NONE;
            }
        }
        else if (g->state == GSTATE_CLIMB_OUT) {
            int16_t step = 1;
            uint16_t accum = g->sub_y + 26;
            g->sub_y = (uint8_t)accum;
            step += (accum >> 8);

            g->offset_x = 0;
            g->offset_y -= step;

            if (g->offset_y < -8) {
                g->grid_y -= 1;
                g->offset_y += 16;
            }

            if (g->offset_y <= 0) {
                g->offset_y = 0;
                g->state = GSTATE_STOP;
                g->dir = DIR_NONE;
            }
        }
        else if (g->dir != DIR_NONE) {
            int16_t step = 1;
            uint16_t accum;

            if (g->dir == DIR_LEFT || g->dir == DIR_RIGHT) {
                accum = g->sub_x + 26;
                g->sub_x = (uint8_t)accum;
                step += (accum >> 8);
            } else {
                accum = g->sub_y + 26;
                g->sub_y = (uint8_t)accum;
                step += (accum >> 8);
            }

            if (g->dir == DIR_LEFT) {
                int16_t next_tx = g->offset_x - step;
                int16_t next_grid_x = g->grid_x;
                if (next_tx < -8) {
                    next_grid_x -= 1;
                    next_tx += 16;
                    g->hole = false;
                    g->holey = -1;
                    ai_drop_gold(i);
                }

                if (next_tx < 0 && (!can_move_to(next_grid_x - 1, g->grid_y) || guard_occupied(i, next_grid_x - 1, g->grid_y))) {
                    g->offset_x = 0;
                    g->state = GSTATE_STOP;
                    g->dir = DIR_NONE;
                } else {
                    g->grid_x = next_grid_x;
                    g->offset_x = next_tx;
                    g->offset_y = 0;

                    uint8_t new_tile = get_tile(g->grid_x, g->grid_y);
                    if (new_tile == MAP_TILE_ROPE) {
                        g->state = GSTATE_CLIMB_LEFT;
                    } else {
                        g->state = GSTATE_LEFT;
                    }
                }
            }
            else if (g->dir == DIR_RIGHT) {
                int16_t next_tx = g->offset_x + step;
                int16_t next_grid_x = g->grid_x;
                if (next_tx > 8) {
                    next_grid_x += 1;
                    next_tx -= 16;
                    g->hole = false;
                    g->holey = -1;
                    ai_drop_gold(i);
                }

                if (next_tx > 0 && (!can_move_to(next_grid_x + 1, g->grid_y) || guard_occupied(i, next_grid_x + 1, g->grid_y))) {
                    g->offset_x = 0;
                    g->state = GSTATE_STOP;
                    g->dir = DIR_NONE;
                } else {
                    g->grid_x = next_grid_x;
                    g->offset_x = next_tx;
                    g->offset_y = 0;

                    uint8_t new_tile = get_tile(g->grid_x, g->grid_y);
                    if (new_tile == MAP_TILE_ROPE) {
                        g->state = GSTATE_CLIMB_RIGHT;
                    } else {
                        g->state = GSTATE_RIGHT;
                    }
                }
            }
            else if (g->dir == DIR_UP) {
                int16_t next_ty = g->offset_y - step;
                int16_t next_grid_y = g->grid_y;
                if (next_ty < -8) {
                    next_grid_y -= 1;
                    next_ty += 16;
                    ai_drop_gold(i);
                }

                uint8_t curr_tile = get_tile(g->grid_x, next_grid_y);
                bool on_ladder = (curr_tile == MAP_TILE_LADDER);

                if (next_ty < 0 && (!on_ladder || !can_move_to(g->grid_x, next_grid_y - 1) || guard_occupied(i, g->grid_x, next_grid_y - 1))) {
                    g->offset_y = 0;
                    g->state = GSTATE_STOP;
                    g->dir = DIR_NONE;
                } else {
                    g->grid_y = next_grid_y;
                    g->offset_y = next_ty;
                    g->offset_x = 0;
                    g->state = GSTATE_UPDOWN;
                }
            }
            else if (g->dir == DIR_DOWN) {
                int16_t next_ty = g->offset_y + step;
                int16_t next_grid_y = g->grid_y;
                if (next_ty > 8) {
                    next_grid_y += 1;
                    next_ty -= 16;
                    ai_drop_gold(i);
                }

                uint8_t curr_tile = get_tile(g->grid_x, next_grid_y);

                if (g->state == GSTATE_UPDOWN && next_ty < 0 && is_empty_tile(curr_tile)) {
                    g->state = GSTATE_FALL_RIGHT;
                    g->grid_y = next_grid_y;
                    g->offset_y = next_ty;
                    g->offset_x = 0;
                    g->dir = DIR_FALL;
                }
                else if (next_ty > 0 && (!can_move_to(g->grid_x, next_grid_y + 1) || guard_occupied(i, g->grid_x, next_grid_y + 1))) {
                    g->offset_y = 0;
                    g->state = GSTATE_STOP;
                    g->dir = DIR_NONE;
                }
                else {
                    g->grid_y = next_grid_y;
                    g->offset_y = next_ty;
                    g->offset_x = 0;

                    if (curr_tile == MAP_TILE_ROPE) {
                        uint8_t under_rope = get_tile(g->grid_x, g->grid_y + 1);
                        if (under_rope == MAP_TILE_LADDER) {
                            g->state = GSTATE_UPDOWN;
                        } else {
                            g->state = GSTATE_FALL_RIGHT;
                            g->dir = DIR_FALL;
                        }
                    } else {
                        g->state = GSTATE_UPDOWN;
                    }
                }
            }
        }

        g->x_pos_px = (g->grid_x << 4) + g->offset_x + player.world_x_px;
        g->y_pos_px = (g->grid_y << 4) + g->offset_y + player.world_y_px;

        unsigned ptr = ENEMY_CONFIG + (i * sizeof(vga_mode5_sprite_t));
        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, g->x_pos_px);
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, g->y_pos_px);
    }
}

void guards_tick_logic(void)
{
    static uint8_t imoves = 5;
    static uint8_t iguard = 0;
    
    uint8_t num_active_guards = 0;
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (guards[i].active) {
            num_active_guards++;
        }
    }
    
    if (num_active_guards == 0) return;
    
    imoves++;
    if (imoves >= 6) {
        imoves = 0;
    }
    
    uint8_t moves = move_policy[num_active_guards][imoves];
    
    while (moves > 0) {
        iguard++;
        if (iguard >= MAX_ENEMIES) {
            iguard = 0;
        }
        
        guard_t *g = &guards[iguard];
        if (!g->active) continue;
        
        if (g->state == GSTATE_TRAP_LEFT || g->state == GSTATE_TRAP_RIGHT || g->state == GSTATE_REBORN) {
            moves--;
            continue;
        }
        
        if (g->offset_x == 0 && g->offset_y == 0) {
            uint8_t d = ai_scan(iguard);
            g->dir = d;
            
            if (d == DIR_LEFT) {
                g->state = (get_tile(g->grid_x, g->grid_y) == MAP_TILE_ROPE) ? GSTATE_CLIMB_LEFT : GSTATE_LEFT;
            } else if (d == DIR_RIGHT) {
                g->state = (get_tile(g->grid_x, g->grid_y) == MAP_TILE_ROPE) ? GSTATE_CLIMB_RIGHT : GSTATE_RIGHT;
            } else if (d == DIR_UP || d == DIR_DOWN || d == GSTATE_CLIMB_OUT) {
                g->state = GSTATE_UPDOWN;
            } else if (d == DIR_FALL) {
                g->state = (g->state == GSTATE_LEFT || g->state == GSTATE_CLIMB_LEFT || g->state == GSTATE_FALL_LEFT) ? GSTATE_FALL_LEFT : GSTATE_FALL_RIGHT;
            } else {
                if (g->grid_y < TILEMAP_HEIGHT - 1 && is_hole_fully_formed(g->grid_x, g->grid_y + 1) && !guard_occupied(iguard, g->grid_x, g->grid_y + 1)) {
                    g->state = GSTATE_FALL_RIGHT;
                    g->dir = DIR_FALL;
                    g->hole = false;
                    g->holey = -1;
                } else {
                    g->state = GSTATE_STOP;
                }
            }
        }
        moves--;
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        guard_t *g = &guards[i];
        if (!g->active) continue;

        if (!g->gold && g->goldholds == 0) {
            if (get_tile(g->grid_x, g->grid_y) == MAP_TILE_GOLD) {
                g->gold = true;
                g->goldholds = (rand() % 26) + 11;
                set_tile(g->grid_x, g->grid_y, MAP_TILE_EMPTY);
            }
        }

        if (g->state != GSTATE_REBORN && get_tile(g->grid_x, g->grid_y) == MAP_TILE_BRICK) {
            ai_reborn(i);
        }

        update_guard_animation(i);
    }
}

void update_hud(void)
{
    uint32_t score = player_score;
    uint8_t d5, d4, d3, d2, d1, d0;
    
    if (score > 999999) {
        score = 999999;
    }
    
    d5 = score / 100000;
    score %= 100000;
    d4 = score / 10000;
    score %= 10000;
    d3 = score / 1000;
    score %= 1000;
    d2 = score / 100;
    score %= 100;
    d1 = score / 10;
    d0 = score % 10;
    
    RIA.addr0 = TEXT_TILES_MAP_DATA + 280;
    RIA.step0 = 1;
    RIA.rw0 = d5 + 1;
    RIA.rw0 = d4 + 1;
    RIA.rw0 = d3 + 1;
    RIA.rw0 = d2 + 1;
    RIA.rw0 = d1 + 1;
    RIA.rw0 = d0 + 1;
    
    {
        uint8_t lives = player_lives;
        uint8_t l2, l1, l0;
        l2 = lives / 100;
        l1 = (lives % 100) / 10;
        l0 = lives % 10;
        
        RIA.addr0 = TEXT_TILES_MAP_DATA + 290;
        RIA.step0 = 1;
        RIA.rw0 = l2 + 1;
        RIA.rw0 = l1 + 1;
        RIA.rw0 = l0 + 1;
    }
    
    {
        uint8_t lvl = current_level;
        uint8_t lv2, lv1, lv0;
        lv2 = lvl / 100;
        lv1 = (lvl % 100) / 10;
        lv0 = lvl % 10;
        
        RIA.addr0 = TEXT_TILES_MAP_DATA + 297;
        RIA.step0 = 1;
        RIA.rw0 = lv2 + 1;
        RIA.rw0 = lv1 + 1;
        RIA.rw0 = lv0 + 1;
    }
}
