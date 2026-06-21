#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Frame rate configuration
#define FPS 23
#define FRAME_TIME (1000.0 / FPS)

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U            // Starting address in XRAM for sprite data

#define PLAYER_DATA            (SPRITE_DATA_START) // Address for main tile bitmap data
#define PLAYER_DATA_SIZE        0x1400U            // 5120 bytes (40 frames 16x16 at 4bpp)
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_SIZE       0x0080U            // 128 bytes per 16x16 4bpp frame
#define PLAYER_FRAME_COUNT      40                 // 40 frames
#define MAX_ENEMIES             3                  // Max number of enemies on screen at once

#define TILE_DATA              (PLAYER_DATA + PLAYER_DATA_SIZE) // Address for tile bitmap data
#define TILE_DATA_SIZE          0x1780U            // 6016 bytes 47 x 16x16 tiles at 4bpp)

#define TILEMAP_DATA           (TILE_DATA + TILE_DATA_SIZE) // Address for tilemap data
#define TILEMAP_DATA_SIZE       0x1C0U             // 448 bytes for tilemap data
#define TILEMAP_WIDTH           28                 // Width of terrain tileset in pixels
#define TILEMAP_HEIGHT          16                 // Height of terrain tileset in pixels

#define SPRITE_DATA_END        (TILEMAP_DATA + TILEMAP_DATA_SIZE) // End address for sprite data

// Palette configurations
#define PLAYER_PALETTE_ADDR      0xFC00  // 16-color palette (32 bytes, 0xFC00-0xFC1F)
#define PLAYER_PALETTE_SIZE      0x0020  // 32 bytes for 16 colors at 2 bytes each
#define TILE_PALETTE_ADDR        0xFC20  // 16-color palette for tiles (32 bytes, 0xFC20-0xFC3F)
#define TILE_PALETTE_SIZE        0x0020  // 32 bytes for 16 colors at 2 bytes each


// OPL2 sound chip configuration
#define OPL_XRAM_ADDR   0xFE00  // Native RIA OPL2 register page
#define OPL_SIZE        0x0100

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

// Configs 
extern unsigned PLAYER_CONFIG; // Address in XRAM where player sprite config is stored, for updates
extern unsigned ENEMY_CONFIG;  // Address in XRAM where enemy sprite configs start, for updates
extern unsigned TILE_GROUND_CONFIG; // Address in XRAM where tile ground config is stored, for updates

#endif // CONSTANTS_H