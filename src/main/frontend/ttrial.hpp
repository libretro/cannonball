/***************************************************************************
    Time Trial Mode Front End.

    This file is part of Cannonball. 
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>

enum {
        BACK_TO_MENU = -1,
        CONTINUE     = 0,
        INIT_GAME    = 1
    };

enum {
        INIT_COURSEMAP,
        TICK_COURSEMAP,
        TICK_GAME_ENGINE
    };

static const uint8_t MAX_LAPS = 5;

static const uint8_t MAX_TRAFFIC = 8;

typedef struct TTrial
{
    uint8_t state;
    int8_t level_selected;
    uint16_t* best_times;
    uint8_t best_converted[3];
} TTrial;

void TTrial_ctor(TTrial* self, uint16_t* best_times);

void TTrial_init(TTrial* self);

int TTrial_tick(TTrial* self);

void TTrial_update_best_time(TTrial* self);
