/***************************************************************************
    XML Configuration File Handling.

    Load & Save Hi-Scores.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdio.h>
#include <string.h> /* remove() */

#include <streams/file_stream.h>

#include <libretro.h>

#include "../main.hpp"
#include "config.hpp"
#include "../globals.hpp"
#include "../setup.hpp"
#include "../utils.hpp"

#include "../engine/ohiscore.hpp"

enum { COUNTER_1M_15 = 0x11D0 };
#include "../engine/audio/osoundint.hpp"

extern retro_log_printf_t                 log_cb;

#ifdef __PS3__
#endif

Config config;

void Config_ctor(Config* self)
{
    /* Safe defaults required before Config_set_fps(Config* self) initializes audio. */
    self->sound.rate        = 44100;
    self->sound.music_timer = MUSIC_TIMER;
    self->video.shadow      = 0;
    self->engine.hiscore_delete = true;
    self->engine.hiscore_timer  = HIGHSCORE_TIMER;
    self->engine.grippy_tyres   = false;
    self->engine.offroad        = false;
    self->engine.bumper         = false;
    self->engine.turbo          = false;
    self->engine.car_pal        = 0;

    self->sound.music[0].type = IS_YM_INT;
    self->sound.music[0].cmd  = SOUND_MUSIC_MAGICAL;
    strcpy(self->sound.music[0].title, "MAGICAL SOUND SHOWER");
    self->sound.music[0].filename[0] = 0;

    self->sound.music[1].type = IS_YM_INT;
    self->sound.music[1].cmd  = SOUND_MUSIC_BREEZE;
    strcpy(self->sound.music[1].title, "PASSING BREEZE");
    self->sound.music[1].filename[0] = 0;

    self->sound.music[2].type = IS_YM_INT;
    self->sound.music[2].cmd  = SOUND_MUSIC_SPLASH;
    strcpy(self->sound.music[2].title, "SPLASH WAVE");
    self->sound.music[2].filename[0] = 0;

    self->sound.music_num = 3;
}



static RFILE* config_open_read(const char* filename)
{
    char path[600];
    snprintf(path, sizeof(path), "%s.sav", filename);
    return filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
                           RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static RFILE* config_open_write(const char* filename)
{
    char path[600];
    snprintf(path, sizeof(path), "%s.sav", filename);
    return filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
                           RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

/* High scores are persisted as a small binary blob in the save */
/* directory. Settings themselves come from the libretro core options, */
/* so no XML configuration is loaded or written. */
void Config_load_scores(Config* self, const char* filename)
{
    RFILE* f = config_open_read(filename);
    if (!f)
        return;
    filestream_read(f, ohiscore.scores, sizeof(ohiscore.scores));
    filestream_close(f);
}

void Config_save_scores(Config* self, const char* filename)
{
    RFILE* f = config_open_write(filename);
    if (!f)
        return;
    filestream_write(f, ohiscore.scores, sizeof(ohiscore.scores));
    filestream_close(f);
}

void Config_load_tiletrial_scores(Config* self)
{
    RFILE* f = config_open_read(FILENAME_TTRIAL);
    if (!f)
    {
        int i;
        for (i = 0; i < 15; i++)
            self->ttrial.best_times[i] = COUNTER_1M_15;
        return;
    }
    filestream_read(f, self->ttrial.best_times,
                    sizeof(self->ttrial.best_times));
    filestream_close(f);
}

void Config_save_tiletrial_scores(Config* self)
{
    RFILE* f = config_open_write(FILENAME_TTRIAL);
    if (!f)
        return;
    filestream_write(f, self->ttrial.best_times,
                     sizeof(self->ttrial.best_times));
    filestream_close(f);
}

bool Config_clear_scores(Config* self)
{
    char path[600];
    bool files_removed = false;

    OHiScore_init_def_scores(&ohiscore);

    /* remove() returns 0 on success */
    snprintf(path, sizeof(path), "%s.sav", FILENAME_SCORES);
    if (!remove(path))
        files_removed = true;
    snprintf(path, sizeof(path), "%s.sav", FILENAME_TTRIAL);
    if (!remove(path))
        files_removed = true;
    snprintf(path, sizeof(path), "%s.sav", FILENAME_CONT);
    if (!remove(path))
        files_removed = true;

    return files_removed;
}

void Config_set_fps(Config* self, int fps)
{
    self->video.fps = fps;
    /* Set core FPS to 30fps, 60fps or 120fps */
    if (self->video.fps == 0)
        self->fps = 30;
    else if (self->video.fps == 3)
        self->fps = 120;
    else
        self->fps = 60;
    
    /* Original game ticks sprites at 30fps but background scroll at 60fps */
    if (self->video.fps == 3)
        self->tick_fps = 120;
    else if (self->video.fps < 2)
        self->tick_fps = 30;
    else
        self->tick_fps = 60;

    if (config.sound.enabled)
        Audio_stop_audio(&cannonball_audio);
    OSoundInt_init(&osoundint);
    if (config.sound.enabled)
        Audio_start_audio(&cannonball_audio);
}
