#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode_5.h"
#include "input.h"
#include "tile_mode_2.h"
#include "player.h"
#include "opl.h"
#include "sound.h"

static int8_t gplay_tick = 0u;

static bool init_video(void)
{
    int rc;

    rc = xreg_vga_canvas(1); // 320x200 canvas
    if (rc < 0) {
        return false;
    }

    // Order matters here.  
    load_level(current_level);
    
    // tile_hud_init();
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

    OPL_Config(1, OPL_ADDR);
    opl_init();
    sound_init();
    music_init("ROM:loderun2");

    if (!init_video()) {
        return 1;
    }

    while (true) {
        wait_for_vsync();

        input_poll(&actions);

        // Smoothly update movement/camera offsets at 60 Hz VSync
        player_update_motion();
        guards_update_motion();

        update_music();
        sound_update();

        gplay_tick += FPS;

        if (gplay_tick >= 60) {
            // Run original 23 Hz logic engine (AI, animations, input actions)
            player_tick_logic(&actions);

            gplay_tick -= 60;
        }

    }

}
