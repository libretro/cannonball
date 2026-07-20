#pragma once

#include "globals.hpp"

#include "libretro/audio.hpp"

namespace cannonball
{
    extern Audio audio;

    /* Frame counter */
    extern int frame;

    /* Tick Logic. Used when running at non-standard > 30 fps */
    extern bool tick_frame;

    /* FPS Counter */
    extern int fps_counter;

    /* Engine Master State */
    extern int state;
    
    enum
    {
        STATE_BOOT,
        STATE_INIT_MENU,
        STATE_MENU,
        STATE_INIT_GAME,
        STATE_GAME,
        STATE_QUIT
    };
}
