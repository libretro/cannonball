/***************************************************************************
    Front End Menu System.

    This file is part of Cannonball. 
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

class TTrial;

enum {
        MENU_STATE_MENU,
        MENU_STATE_REDEFINE_KEYS,
        MENU_STATE_REDEFINE_JOY,
        MENU_STATE_TTRIAL
    };

static const int32_t MESSAGE_TIME = 5;

struct Menu
{
    uint8_t state;
    TTrial* ttrial;
    uint8_t redef_state;
    uint32_t frame;
    int32_t message_counter;
    std::string msg;
    int16_t cursor;
    bool is_text_menu;
    std::vector<std::string>* menu_selected;
    std::vector<std::string> menu_main;
    std::vector<std::string> menu_gamemodes;
    std::vector<std::string> menu_cont;
    std::vector<std::string> menu_timetrial;
    std::vector<std::string> menu_about;
    std::vector<std::string> menu_settings;
    std::vector<std::string> menu_video;
    std::vector<std::string> menu_sound;
    std::vector<std::string> menu_controls;
    std::vector<std::string> menu_engine;
    std::vector<std::string> menu_enhancements;
    std::vector<std::string> menu_handling;
    std::vector<std::string> menu_musictest;
    std::vector<std::string> text_redefine;
};

void Menu_ctor(Menu* self);

void Menu_dtor(Menu* self);

void Menu_populate(Menu* self);

void Menu_init(Menu* self);

void Menu_tick(Menu* self);

void Menu_refresh_menu(Menu* self);
