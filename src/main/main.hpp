#pragma once

#include "globals.hpp"

#include "libretro/audio.hpp"

extern Audio cannonball_audio;

/* Frame counter */
extern int cannonball_frame;

/* Tick Logic. Used when running at non-standard > 30 fps */
extern bool cannonball_tick_frame;

/* FPS Counter */
extern int cannonball_fps_counter;

/* Engine Master State */
extern int cannonball_state;

enum
{
    STATE_BOOT,
    STATE_INIT_MENU,
    STATE_MENU,
    STATE_INIT_GAME,
    STATE_GAME,
    STATE_QUIT
};
