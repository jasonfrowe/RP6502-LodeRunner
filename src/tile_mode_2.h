#ifndef TILE_MODE2_H
#define TILE_MODE2_H

#include <stdint.h>

// Palette extracted from graphics/maptiles.png
static const uint16_t maptiles_4bpp[16] = {
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

void tile_mode2_init(void);

#endif // TILE_MODE2_H