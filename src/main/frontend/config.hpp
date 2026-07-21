/***************************************************************************
    XML Configuration File Handling.

    Load Settings.
    Load & Save Hi-Scores.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>
#include <set>
#include <string>
#include <vector>

struct data_settings_t
{
    std::string res_path;
};

struct music_t
{
    const static int IS_YM_INT = 0;
    const static int IS_YM_EXT = 1;
    const static int IS_WAV    = 2;

    int type;
    int cmd;
    std::string title;
    std::string filename;
};

struct custom_music_t
{
    int enabled;
    std::string title;
    std::string filename;
};

struct ttrial_settings_t
{
    int laps;
    int traffic;
    uint16_t best_times[15];
};

struct menu_settings_t
{
    int enabled;
    int road_scroll_speed;
};

struct video_settings_t
{
    const static int MODE_WINDOW  = 0;
    const static int MODE_FULL    = 1;
    const static int MODE_STRETCH = 2;

    int mode;
    int scale;
    int scanlines;
    int widescreen;
    int fps;
    int fps_count;
    int hires;
    int filtering;
    int shadow;
};

struct sound_settings_t
{
    int enabled;
    int rate;
    int advertise;
    int preview;
    int fix_samples;
    int music_timer;
    std::vector<music_t> music;
    custom_music_t custom_music[4];
};

struct controls_settings_t
{
    const static int GEAR_BUTTON   = 0;
    const static int GEAR_PRESS    = 1; /* For cabinets */
    const static int GEAR_SEPARATE = 2; /* Separate button presses */
    const static int GEAR_AUTO     = 3;

    int gear;
    int steer_speed;   /* Steering Digital Speed */
    int pedal_speed;   /* Pedal Digital Speed */
    int padconfig[8];  /* Joypad Button Config */
    int keyconfig[12]; /* Keyboard Button Config */
    int pad_id;        /* Use the N'th joystick on the system. */
    int analog;        /* Use analog controls */
    int axis[3];       /* Analog Axis */
    int asettings[3];  /* Analog Settings */

    int haptic;        /* Force Feedback Enabled */
    int max_force;
    int min_force;
    int force_duration;
};

struct engine_settings_t
{
    int dip_time;
    int dip_traffic;
    bool freeplay;
    bool freeze_timer;
    bool disable_traffic;
    int jap;
    int prototype;
    int randomgen;
    int level_objects;
    bool fix_bugs;
    bool fix_bugs_backup;
    bool fix_timer;
    bool layout_debug;
    bool force_ai;
    bool hiscore_delete;
    int hiscore_timer;
    int new_attract;
    bool grippy_tyres;
    bool offroad;
    bool bumper;
    bool turbo;
    int car_pal;
    
};

struct Config
{
    data_settings_t        data;
    menu_settings_t        menu;
    video_settings_t       video;
    sound_settings_t       sound;
    controls_settings_t    controls;
    engine_settings_t      engine;
    ttrial_settings_t      ttrial;
    uint16_t s16_width, s16_height;
    uint16_t s16_x_off;
    int fps;
    int tick_fps;
    int cont_traffic;
};

extern Config config;

void Config_ctor(Config* self);

void Config_init(Config* self);

void Config_load_custom_music(Config* self, const std::string& filename);

void Config_load_scores(Config* self, const std::string &filename);

void Config_save_scores(Config* self, const std::string &filename);

void Config_load_tiletrial_scores(Config* self);

void Config_save_tiletrial_scores(Config* self);

bool Config_clear_scores(Config* self);

void Config_set_fps(Config* self, int fps);

