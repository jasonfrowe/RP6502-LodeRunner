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

static uint16_t gplay_tick = 0u;

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
static uint8_t s_music_vsync_last = 0;

static bool wait_for_vsync(void)
{
    uint16_t spin_guard = 0xFFFFu;
    while (RIA.vsync == s_vsync_last) {
        if (--spin_guard == 0u) {
            return false;
        }
    }
    s_vsync_last = RIA.vsync;
    return true;
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

    s_vsync_last = RIA.vsync;
    s_music_vsync_last = s_vsync_last;

    while (true) {
        if (!wait_for_vsync()) {
            s_vsync_last = RIA.vsync;
        }

        uint8_t now_vsync = RIA.vsync;
        uint8_t music_ticks = (uint8_t)(now_vsync - s_music_vsync_last);
        if (music_ticks == 0u) {
            music_ticks = 1u;
        }
        s_music_vsync_last = now_vsync;

        update_music_advance(music_ticks);
        input_poll(&actions);

        // Smoothly update movement/camera offsets at 60 Hz VSync
        player_update_motion();
        guards_update_motion();

        sound_update();

        gplay_tick += FPS;

        while (gplay_tick >= 60u) {
            // Run original 23 Hz logic engine (AI, animations, input actions)
            player_tick_logic(&actions);

            gplay_tick -= 60u;
        }

    }

}
