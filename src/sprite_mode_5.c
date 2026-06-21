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

uint8_t current_level = 1;
static volatile uint8_t original_map[TILEMAP_WIDTH * TILEMAP_HEIGHT];

void reload_level(void)
{
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
    current_level++;
    if (current_level > 150) {
        current_level = 1;
    }
    load_level(current_level);
}

void sprite_mode5_players_init(void){

    // Find the player start position from the tilemap data
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
    bool found = false;

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

                int16_t wx = col << 4;
                int16_t wy = row << 4;

                player.world_x_px = SCREEN_WIDTH_D2 - wx;
                if (player.world_x_px > 0) {
                    player.world_x_px = 0;
                } else if (player.world_x_px < -128) {
                    player.world_x_px = -128;
                }

                player.world_y_px = SCREEN_HEIGHT_D2 - wy;
                if (player.world_y_px > 0) {
                    player.world_y_px = 0;
                } else if (player.world_y_px < -16) {
                    player.world_y_px = -16;
                }

                player.x_pos_px = wx + player.world_x_px;
                player.y_pos_px = wy + player.world_y_px;
                found = true;
                break;
            }
        }
        if (found) break;
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
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1 + MAX_ENEMIES, 2, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_4bpp[i] & 0xFF;
        RIA.rw0 = player_4bpp[i] >> 8;
    } 

}   