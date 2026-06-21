#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include <stdint.h>

typedef struct player_s {
    int16_t x_pos_px;
    int16_t y_pos_px;
    int16_t world_x_px;
    int16_t world_y_px;
    uint8_t dir;
    uint8_t sub_x;
    uint8_t sub_y;
} player_t;

extern player_t player;

// Palette extracted from graphics/player.png
static const uint16_t player_4bpp[16] = {
    0x0000,
    0xA820,
    0x0560,
    0xAD60,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF,
    0xFABF,
    0x57FF,
    0xFFFF,
};

void sprite_mode5_players_init(void);

#endif // SPRITE_MODE5_H