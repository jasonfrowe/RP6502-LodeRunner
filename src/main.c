#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode_5.h"
#include "input.h"

static bool init_video(void)
{
    int rc;

    rc = xreg_vga_canvas(1); // 320x200 canvas
    if (rc < 0) {
        return false;
    }

    // tile_mode2_init();
    // tile_hud_init();
    sprite_mode5_players_init();
    // sprite_mode5_init_targets();
    // sprite_mode5_init_projectiles();
    // text_mode1_init();
    // minimap_init();

    return true;
}

static uint8_t s_vsync_last = 0;

static void wait_for_vsync(void)
{
    while (RIA.vsync == s_vsync_last) {
    }
    s_vsync_last = RIA.vsync;
}

int main(void)
{
    input_actions_t actions;

    input_init();

    if (!init_video()) {
        return 1;
    }

    while (true) {
        wait_for_vsync();

        input_poll(&actions);

    }

}
