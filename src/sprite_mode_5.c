#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "sprite_mode_5.h"
#include "constants.h"
#include "player.h"
#include "tile_mode_2.h"

// Store the player config address for updates
unsigned PLAYER_CONFIG;
unsigned ENEMY_CONFIG;

player_t player;
guard_t guards[MAX_ENEMIES];

uint8_t current_level = 1;
static volatile uint8_t original_map[TILEMAP_WIDTH * TILEMAP_HEIGHT];

void reload_level(void)
{
    reset_player_input_state();
    clear_all_holes();
    
    RIA.addr0 = TILEMAP_DATA;
    RIA.step0 = 1;
    for (int i = 0; i < TILEMAP_WIDTH * TILEMAP_HEIGHT; i++) {
        RIA.rw0 = original_map[i];
    }
    
    sprite_mode5_players_init();
    tile_mode2_init();
}

void load_level(uint8_t lvl)
{
    reset_player_input_state();
    clear_all_holes();
    
    FILE *fp = fopen("ROM:maps", "rb");
    if (fp) {
        long offset = (long)(lvl - 1) * (TILEMAP_WIDTH * TILEMAP_HEIGHT);
        if (fseek(fp, offset, SEEK_SET) == 0) {
            uint8_t temp_buf[TILEMAP_WIDTH * TILEMAP_HEIGHT];
            if (fread(temp_buf, 1, TILEMAP_WIDTH * TILEMAP_HEIGHT, fp) == TILEMAP_WIDTH * TILEMAP_HEIGHT) {
                for (int i = 0; i < TILEMAP_WIDTH * TILEMAP_HEIGHT; i++) {
                    original_map[i] = temp_buf[i];
                }
            }
        }
        fclose(fp);
    }
    
    // Copy original_map back to XRAM TILEMAP_DATA
    RIA.addr0 = TILEMAP_DATA;
    RIA.step0 = 1;
    for (int i = 0; i < TILEMAP_WIDTH * TILEMAP_HEIGHT; i++) {
        RIA.rw0 = original_map[i];
    }
    
    sprite_mode5_players_init();
    tile_mode2_init();
}

void load_next_level(void)
{
    if (player_lives < 99) {
        player_lives++;
    }
    player_score += 1500;

    current_level++;
    if (current_level > 150) {
        current_level = 1;
    }
    load_level(current_level);
}

void sprite_mode5_players_init(void){

    // Find the player and guard start positions from the tilemap data
    player.x_pos_px = 0;
    player.y_pos_px = 0;
    player.world_x_px = 0;
    player.world_y_px = 0;
    player.dir = 0; // DIR_NONE
    player.sub_x = 0;
    player.sub_y = 0;
    player.is_falling = false;
    player.grid_x = 0;
    player.grid_y = 0;
    player.offset_x = 0;
    player.offset_y = 0;
    player.state = RSTATE_RIGHT;
    player.anim_frame = 0;
    player.anim_tick = 0;

    uint8_t guard_count = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        guards[i].active = false;
    }

    RIA.addr0 = TILEMAP_DATA;
    RIA.step0 = 1;

    for (int row = 0; row < TILEMAP_HEIGHT; row++) {
        for (int col = 0; col < TILEMAP_WIDTH; col++) {
            uint8_t tile_id_1 = RIA.rw0;
            if (tile_id_1 == MAP_TILE_RUNNER) {
                // Seek back to this tile and write empty
                RIA.addr0 = TILEMAP_DATA + row * TILEMAP_WIDTH + col;
                RIA.step0 = 0;
                RIA.rw0 = MAP_TILE_EMPTY;

                player.grid_x = col;
                player.grid_y = row;
                start_grid_x = col;
                start_grid_y = row;
                player.offset_x = 0;
                player.offset_y = 0;
                player.state = RSTATE_RIGHT;
                player.anim_frame = 0;
                player.anim_tick = 0;

                int16_t wx = col * 10;
                int16_t wy = row * 11;

                player.world_x_px = 20;
                player.world_y_px = 30;

                player.x_pos_px = wx + player.world_x_px;
                player.y_pos_px = wy + player.world_y_px;

                // Seek step back to continue scanning
                RIA.addr0 = TILEMAP_DATA + row * TILEMAP_WIDTH + col + 1;
                RIA.step0 = 1;
            }
            else if (tile_id_1 == MAP_TILE_GUARD) {
                // Seek back to this tile and write empty
                RIA.addr0 = TILEMAP_DATA + row * TILEMAP_WIDTH + col;
                RIA.step0 = 0;
                RIA.rw0 = MAP_TILE_EMPTY;

                if (guard_count < MAX_ENEMIES) {
                    guard_t *g = &guards[guard_count];
                    g->start_grid_x = col;
                    g->start_grid_y = row;
                    g->grid_x = col;
                    g->grid_y = row;
                    g->offset_x = 0;
                    g->offset_y = 0;
                    g->sub_x = 0;
                    g->sub_y = 0;
                    g->state = GSTATE_LEFT;
                    g->anim_frame = 0;
                    g->anim_tick = 0;
                    g->dir = 0; // DIR_NONE
                    g->hole = false;
                    g->holey = -1;
                    g->gold = false;
                    g->goldholds = 0;
                    g->active = true;
                    guard_count++;
                }

                // Seek step back to continue scanning
                RIA.addr0 = TILEMAP_DATA + row * TILEMAP_WIDTH + col + 1;
                RIA.step0 = 1;
            }
        }
    }

    // Align guard positions based on player world scroll offset
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        guard_t *g = &guards[i];
        if (g->active) {
            g->x_pos_px = (g->grid_x * 10) + player.world_x_px;
            g->y_pos_px = (g->grid_y * 11) + player.world_y_px;
        }
    }

    // Set the player config address for updates
    PLAYER_CONFIG = SPRITE_DATA_END; // Just after the player sprite data

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, player.x_pos_px);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, player.y_pos_px);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);

    ENEMY_CONFIG = PLAYER_CONFIG + sizeof(vga_mode5_sprite_t); // Just after player config
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {

        unsigned ptr = ENEMY_CONFIG + (i * sizeof(vga_mode5_sprite_t));

        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, (PLAYER_DATA + 21 * PLAYER_FRAME_SIZE));
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1 + MAX_ENEMIES, 1, 0, 224) < 0) {
        return;
    }

    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_4bpp[i] & 0xFF;
        RIA.rw0 = player_4bpp[i] >> 8;
    } 

    // Check if there is any gold on the map initially
    uint16_t gold_left = 0;
    RIA.addr0 = TILEMAP_DATA;
    RIA.step0 = 1;
    for (int i = 0; i < TILEMAP_WIDTH * TILEMAP_HEIGHT; i++) {
        if (RIA.rw0 == MAP_TILE_GOLD) {
            gold_left++;
        }
    }
    if (gold_left == 0) {
        reveal_hidden_ladders();
    }
}   