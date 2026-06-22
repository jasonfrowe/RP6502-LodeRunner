#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "tile_mode_2.h"
#include "sprite_mode_5.h"
#include "player.h"

unsigned TILE_GROUND_CONFIG;
unsigned TEXT_CONFIG;

static const uint8_t title_screen_map[216] = {
    1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 1, 1,
    1, 1, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 1, 1,
    1, 1, 1, 30, 31, 32, 1, 1, 33, 34, 35, 36, 37, 38, 39, 40, 1, 1,
    1, 1, 1, 1, 1, 1, 41, 1, 42, 43, 44, 45, 46, 47, 48, 49, 1, 1,
    1, 1, 1, 1, 1, 1, 50, 51, 52, 53, 54, 55, 1, 1, 1, 1, 1, 1,
    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
    74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
    92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
    1, 1, 1, 1, 1, 1, 1, 1, 110, 111, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 112, 113, 114, 115, 116, 117, 118, 119, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 120, 121, 122, 123, 124, 125, 126, 127, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 128, 129, 130, 131, 132, 133, 134, 135, 1, 1, 1, 1, 1
};

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
    if (xreg_vga_mode(2, 0x0A, TILE_GROUND_CONFIG, 0, 0, 224) < 0) {
        return;
    }

    RIA.addr0 = TILE_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = maptiles_4bpp[i] & 0xFF;
        RIA.rw0 = maptiles_4bpp[i] >> 8;
    }

    TEXT_CONFIG = TILE_GROUND_CONFIG + sizeof(vga_mode2_config_t); // Just after tile ground config

    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, width_tiles,  TEXT_TILES_WIDTH);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, height_tiles, TEXT_TILES_HEIGHT);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, xram_data_ptr, TEXT_TILES_MAP_DATA); // tile ID grid
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, xram_palette_ptr, TEXT_PALETTE_ADDR);
    xram0_struct_set(TEXT_CONFIG, vga_mode2_config_t, xram_tile_ptr, TEXT_TILES_DATA);        // tile bitmaps

    RIA.addr0 = TEXT_TILES_MAP_DATA;
    RIA.step0 = 1;
    for (int i = 0; i < TEXT_TILES_WIDTH * (TEXT_TILES_HEIGHT -1); i++) {
        RIA.rw0 = 0; // Start with blank text tilemap
    }
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 0;  // blank
    RIA.rw0 = 23; // M
    RIA.rw0 = 15; // E
    RIA.rw0 = 24; // N
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 6;  // 5
    RIA.rw0 = 0;  // blank
    RIA.rw0 = 22; // L
    RIA.rw0 = 15; // E
    RIA.rw0 = 32; // V
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 1;  // 0
    RIA.rw0 = 2;  // 1


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=1 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b1010 = 0x0A
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x0A, TEXT_CONFIG, 1, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TEXT_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = texttiles_4bpp[i] & 0xFF;
        RIA.rw0 = texttiles_4bpp[i] >> 8;
    }

    if (title_screen_active) {
        for (int row = 0; row < 12; row++) {
            RIA.addr0 = TEXT_TILES_MAP_DATA + (row + 1) * TEXT_TILES_WIDTH + 1;
            RIA.step0 = 1;
            for (int col = 0; col < 18; col++) {
                RIA.rw0 = title_screen_map[row * 18 + col] + 55;
            }
        }
    }

    update_hud();
}