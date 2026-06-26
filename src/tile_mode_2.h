#ifndef TILE_MODE2_H
#define TILE_MODE2_H

#include <stdint.h>

// Palette extracted from graphics/maptiles.png
static const uint16_t maptiles_4bpp[16] = {
    0x0000,
    0xA820,
    0x0560,
    0xFD21,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x02FE,
    0xFABF,
    0x57FF,
    0xFFFF,
};

// Palette extracted from graphics/texttiles.png
static const uint16_t texttiles_4bpp[16] = {
    0x0000,
    0x1020,
    0x0560,
    0xFD21,
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

void tile_mode2_init(void);
void update_title_screen_aperture(void);
void reset_title_aperture(void);

#endif // TILE_MODE2_H