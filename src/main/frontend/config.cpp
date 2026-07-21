/***************************************************************************
    XML Configuration File Handling.

    Load & Save Hi-Scores.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdio.h> /* remove() */

#include <pugixml/pugixml.hpp>

#include <libretro.h>

#include "../main.hpp"
#include "config.hpp"
#include "../globals.hpp"
#include "../setup.hpp"
#include "../utils.hpp"

#include "../engine/ohiscore.hpp"
#include "../engine/audio/osoundint.hpp"

extern retro_log_printf_t                 log_cb;

#ifdef __PS3__
#define remove std::remove
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

    music_t magical;
    magical.type  = music_t::IS_YM_INT;
    magical.cmd   = SOUND_MUSIC_MAGICAL;
    magical.title = "MAGICAL SOUND SHOWER";

    music_t breeze;
    breeze.type  = music_t::IS_YM_INT;
    breeze.cmd   = SOUND_MUSIC_BREEZE;
    breeze.title = "PASSING BREEZE";

    music_t splash;
    splash.type  = music_t::IS_YM_INT;
    splash.cmd   = SOUND_MUSIC_SPLASH;
    splash.title = "SPLASH WAVE";

    self->sound.music.push_back(magical);
    self->sound.music.push_back(breeze);
    self->sound.music.push_back(splash);
}



void Config_init(Config* self)
{

}

void Config_load_custom_music(Config* self, const std::string& filename)
{
    /* Remove custom tracks added by any previous content load, */
    /* while retaining the three original arcade tracks. */
    const size_t original_track_count = 3;

    if (self->sound.music.size() > original_track_count)
        self->sound.music.resize(original_track_count);

    pugi::xml_document document;

    const pugi::xml_parse_result result =
        document.load_file(
            filename.c_str(),
            pugi::parse_default |
                pugi::parse_trim_pcdata);

    if (!result)
    {
        if (log_cb)
            log_cb(
                RETRO_LOG_WARN,
                "[Cannonball]: Could not read custom music configuration "
                "%s: %s\n",
                filename.c_str(),
                result.description());

        return;
    }

    const pugi::xml_node custom_music =
        document
            .child("sound")
            .child("custom_music");

    { unsigned loaded_tracks = 0;

    /* Scan track1, track2, track3... until the first entry */
    /* without an enabled attribute. */
    { int i; for (i = 0; ; i++)
    {
        const std::string track_number =
            Utils_to_string(i + 1);

        const std::string track_name =
            "track" + track_number;

        const pugi::xml_node track =
            custom_music.child(track_name.c_str());

        const pugi::xml_attribute enabled =
            track.attribute("enabled");

        if (!enabled)
            break;

        if (enabled.as_int() != 1)
            continue;

        music_t music;

        const std::string default_title =
            "TRACK " + track_number;

        const std::string default_filename =
            "track" + track_number + ".wav";

        music.title =
            track
                .child("title")
                .text()
                .as_string(default_title.c_str());

        music.filename =
            track
                .child("filename")
                .text()
                .as_string(default_filename.c_str());

        { const size_t filename_length =
            music.filename.length();

        const bool is_wav =
            filename_length >= 4 &&
            (
                music.filename.compare(
                    filename_length - 4,
                    4,
                    ".wav") == 0 ||
                music.filename.compare(
                    filename_length - 4,
                    4,
                    ".WAV") == 0
            );

        music.type = is_wav
            ? music_t::IS_WAV
            : music_t::IS_YM_EXT;

        music.cmd = SOUND_MUSIC_CUSTOM;

        self->sound.music.push_back(music);
        loaded_tracks++;
     }} }

    if (log_cb)
        log_cb(
            RETRO_LOG_INFO,
            "[Cannonball]: Loaded %u custom music track(s) "
            "from %s\n",
            loaded_tracks,
            filename.c_str());
 }}

static std::string get_xml_filename(
    const std::string& filename,
    const bool japanese)
{
    return japanese
        ? filename + "_jap.xml"
        : filename + ".xml";
}


static void append_xml_declaration(
    pugi::xml_document& document)
{
    pugi::xml_node declaration =
        document.append_child(pugi::node_declaration);

    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "utf-8";
}


void Config_load_scores(Config* self, const std::string &filename)
{
    const std::string xml_filename =
        get_xml_filename(filename, self->engine.jap);

    pugi::xml_document document;

    const pugi::xml_parse_result result =
        document.load_file(
            xml_filename.c_str(),
            pugi::parse_default |
                pugi::parse_trim_pcdata |
                pugi::parse_fragment);

    if (!result)
        return;

    /* Game Scores */
    { int i; for (i = 0; i < NO_SCORES; i++)
    {
        score_entry* e = &ohiscore.scores[i];

        const std::string score_name =
            "score" + Utils_to_string(i);

        const pugi::xml_node score =
            document.child(score_name.c_str());

        e->score =
            Utils_from_hex_string(
                score
                    .child("score")
                    .text()
                    .as_string("0"));

        { const char* initial1 =
            score
                .child("initial1")
                .text()
                .as_string(".");

        const char* initial2 =
            score
                .child("initial2")
                .text()
                .as_string(".");

        const char* initial3 =
            score
                .child("initial3")
                .text()
                .as_string(".");

        e->initial1 = initial1[0] ? initial1[0] : '.';
        e->initial2 = initial2[0] ? initial2[0] : '.';
        e->initial3 = initial3[0] ? initial3[0] : '.';

        e->maptiles =
            Utils_from_hex_string(
                score
                    .child("maptiles")
                    .text()
                    .as_string("20202020"));

        e->time =
            Utils_from_hex_string(
                score
                    .child("time")
                    .text()
                    .as_string("0"));

        if (e->initial1 == '.')
            e->initial1 = 0x20;

        if (e->initial2 == '.')
            e->initial2 = 0x20;

        if (e->initial3 == '.')
            e->initial3 = 0x20;
     }} }
}


void Config_save_scores(Config* self, const std::string &filename)
{
    const std::string xml_filename =
        get_xml_filename(filename, self->engine.jap);

    pugi::xml_document document;

    append_xml_declaration(document);

    { int i; for (i = 0; i < NO_SCORES; i++)
    {
        score_entry* e = &ohiscore.scores[i];

        const std::string score_name =
            "score" + Utils_to_string(i);

        pugi::xml_node score =
            document.append_child(score_name.c_str());

        score
            .append_child("score")
            .text()
            .set(Utils_to_hex_string(e->score).c_str());

        const std::string initial1 =
            e->initial1 == 0x20
                ? "."
                : Utils_to_string(
                    (char)(e->initial1));

        const std::string initial2 =
            e->initial2 == 0x20
                ? "."
                : Utils_to_string(
                    (char)(e->initial2));

        const std::string initial3 =
            e->initial3 == 0x20
                ? "."
                : Utils_to_string(
                    (char)(e->initial3));

        score
            .append_child("initial1")
            .text()
            .set(initial1.c_str());

        score
            .append_child("initial2")
            .text()
            .set(initial2.c_str());

        score
            .append_child("initial3")
            .text()
            .set(initial3.c_str());

        score
            .append_child("maptiles")
            .text()
            .set(Utils_to_hex_string(e->maptiles).c_str());

        score
            .append_child("time")
            .text()
            .set(Utils_to_hex_string(e->time).c_str());
    } }

    { const bool saved =
        document.save_file(
            xml_filename.c_str(),
            "\t",
            pugi::format_default,
            pugi::encoding_utf8);

    if (!saved && log_cb)
        log_cb(
            RETRO_LOG_ERROR,
            "Error saving hiscores: %s\n",
            xml_filename.c_str());
 }}


void Config_load_tiletrial_scores(Config* self)
{
    const std::string xml_filename =
        get_xml_filename(FILENAME_TTRIAL, self->engine.jap);

    /* Counter value representing 1m 15s 0ms. */
    static const uint16_t COUNTER_1M_15 = 0x11D0;

    { int i; for (i = 0; i < 15; i++)
        self->ttrial.best_times[i] = COUNTER_1M_15; }

    pugi::xml_document document;

    const pugi::xml_parse_result result =
        document.load_file(
            xml_filename.c_str(),
            pugi::parse_default |
                pugi::parse_trim_pcdata |
                pugi::parse_fragment);

    if (!result)
        return;

    const pugi::xml_node time_trial =
        document.child("time_trial");

    { int i; for (i = 0; i < 15; i++)
    {
        const std::string score_name =
            "score" + Utils_to_string(i);

        self->ttrial.best_times[i] =
            time_trial
                .child(score_name.c_str())
                .text()
                .as_uint(COUNTER_1M_15);
    } }
}


void Config_save_tiletrial_scores(Config* self)
{
    const std::string xml_filename =
        get_xml_filename(FILENAME_TTRIAL, self->engine.jap);

    pugi::xml_document document;

    append_xml_declaration(document);

    pugi::xml_node time_trial =
        document.append_child("time_trial");

    { int i; for (i = 0; i < 15; i++)
    {
        const std::string score_name =
            "score" + Utils_to_string(i);

        time_trial
            .append_child(score_name.c_str())
            .text()
            .set(self->ttrial.best_times[i]);
    } }

    { const bool saved =
        document.save_file(
            xml_filename.c_str(),
            "\t",
            pugi::format_default,
            pugi::encoding_utf8);

    if (!saved && log_cb)
        log_cb(
            RETRO_LOG_ERROR,
            "Error saving time-trial scores: %s\n",
            xml_filename.c_str());
 }}

bool Config_clear_scores(Config* self)
{
    /* Init Default Hiscores */
    OHiScore_init_def_scores(&ohiscore);

    bool files_removed = false;

    /* Remove XML files if they exist */
    /* remove() returns 0 on success */
    if (!remove(std::string(FILENAME_SCORES).append(".xml").c_str()))
        files_removed = true;
    if (!remove(std::string(FILENAME_SCORES).append("_jap.xml").c_str()))
        files_removed = true;
    if (!remove(std::string(FILENAME_TTRIAL).append(".xml").c_str()))
        files_removed = true;
    if (!remove(std::string(FILENAME_TTRIAL).append("_jap.xml").c_str()))
        files_removed = true;
    if (!remove(std::string(FILENAME_CONT).append(".xml").c_str()))
        files_removed = true;
    if (!remove(std::string(FILENAME_CONT).append("_jap.xml").c_str()))
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
