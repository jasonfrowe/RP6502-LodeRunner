#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "tile_mode_2.h"
#include "sprite_mode_5.h"

unsigned TILE_GROUND_CONFIG;

void tile_mode2_init(void)
{
    TILE_GROUND_CONFIG = ENEMY_CONFIG + MAX_ENEMIES * sizeof(vga_mode5_sprite_t); // Just after enemy sprite configs

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, player.world_x_px);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_pos_px, player.world_y_px);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, width_tiles,  TILEMAP_WIDTH);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, height_tiles, TILEMAP_HEIGHT);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_data_ptr, TILEMAP_DATA); // tile ID grid
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_palette_ptr, TILE_PALETTE_ADDR);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_tile_ptr, TILE_DATA);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=1 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b1010 = 0x0A
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x0A, TILE_GROUND_CONFIG, 0, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TILE_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = maptiles_4bpp[i] & 0xFF;
        RIA.rw0 = maptiles_4bpp[i] >> 8;
    }

    // Start at the player's home position so the terrain is correct on the first frame
    // tile_mode2_set_scroll_x(PLAYER_START_WORLD_X_PX);
}