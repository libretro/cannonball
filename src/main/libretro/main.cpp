/***************************************************************************
    Cannonball Main Entry Point.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include <libretro.h>
#include "libretro_core_options.h"

#include "input.hpp"
#include "video.hpp"

#include "romloader.hpp"
#include "trackloader.hpp"
#include "main.hpp"
#include "lr_setup.hpp"
#include "engine/outrun.hpp"
#include "frontend/config.hpp"
#include "frontend/menu.hpp"

#include "cannonboard/interface.hpp"
#include "engine/oinputs.hpp"
#include "engine/ooutputs.hpp"
#include "engine/omusic.hpp"

#include "lr_options.hpp"

// Haptic Support.
#include "ffeedback.hpp"

// Initialize Shared Variables
using namespace cannonball;

int cannonball::state = STATE_BOOT;
int cannonball::frame = 0;
bool cannonball::tick_frame = true;
int cannonball::fps_counter = 0;

#ifdef COMPILE_SOUND_CODE
Audio cannonball::audio;
#endif

Menu *menu;
Interface cannonboard;

// Pause Engine
bool pause_engine;

static bool libretro_supports_bitmasks = false;

static void config_init(void)
{
   // ------------------------------------------------------------------------
   // Menu Settings
   // ------------------------------------------------------------------------

   config.menu.enabled = 1;
   config.menu.road_scroll_speed = 50;

   // ------------------------------------------------------------------------
   // Video Settings
   // ------------------------------------------------------------------------

   config.video.mode = 0;      // Video Mode: Default is Windowed
   config.video.scale = 1;     // Video Scale: Default is 2x
   config.video.scanlines = 0; // Scanlines
#ifdef LOW_FPS
   config.video.fps = 0; // Default is 30 fps
#else
   config.video.fps = 2; // Default is 60 fps
#endif
   config.video.fps_count = 0; // FPS Counter
#ifdef DINGUX
   config.video.widescreen = 0; // Enable Widescreen Mode
#else
   config.video.widescreen = 1; // Enable Widescreen Mode
#endif
   config.video.hires = 0;     // Hi-Resolution Mode
   config.video.filtering = 0; // Open GL Filtering Mode

   config.set_fps(config.video.fps);

   // ------------------------------------------------------------------------
   // Sound Settings
   // ------------------------------------------------------------------------
#ifdef COMPILE_SOUND_CODE
   config.sound.enabled = 1;
#else
   config.sound.enabled = 0;
#endif
   config.sound.advertise = 1;
   config.sound.preview = 1;
   config.sound.fix_samples = 1;

#if 0
    // Custom Music
    for (int i = 0; i < 4; i++)
    {
        std::string xmltag = "sound.custom_music.track";
        xmltag += Utils::to_string(i+1);  

        config.sound.custom_music[i].enabled = pt_config.get(xmltag + ".<xmlattr>.enabled", 0);
        config.sound.custom_music[i].title   = pt_config.get(xmltag + ".title", "TRACK " +Utils::to_string(i+1));
        config.sound.custom_music[i].filename= pt_config.get(xmltag + ".filename", "track"+Utils::to_string(i+1)+".wav");
    }
#endif

#ifdef CANNONBOARD
   // ------------------------------------------------------------------------
   // CannonBoard Settings
   // ------------------------------------------------------------------------
   cannonboard.enabled = 1;
   cannonboard.port = "COM6";
   cannonboard.baud = 57600;
   cannonboard.debug = 0;
   cannonboard.cabinet = 0;
#endif

   // ------------------------------------------------------------------------
   // Controls
   // ------------------------------------------------------------------------
   config.controls.gear = 0;
   config.controls.steer_speed = 3;
   config.controls.pedal_speed = 4;
   config.controls.keyconfig[0] = 273;  /* up */
   config.controls.keyconfig[1] = 274;  /* down */
   config.controls.keyconfig[2] = 276;  /* left */
   config.controls.keyconfig[3] = 275;  /* right */
   config.controls.keyconfig[4] = 122;  /* accelerate */
   config.controls.keyconfig[5] = 120;  /* brake */
   config.controls.keyconfig[6] = 32;   /* gear1 */
   config.controls.keyconfig[7] = 33;   /* gear2 */
   config.controls.keyconfig[8] = 49;   /* start */
   config.controls.keyconfig[9] = 53;   /* coin */
   config.controls.keyconfig[10] = 286; /* menu */
   config.controls.keyconfig[11] = 304; /* view */
   config.controls.padconfig[0] = 0;    /* accelerate */
   config.controls.padconfig[1] = 1;    /* brake */
   config.controls.padconfig[2] = 2;    /* gear1 */
   config.controls.padconfig[3] = 2;    /* gear2 */
   config.controls.padconfig[4] = 3;    /* start */
   config.controls.padconfig[5] = 4;    /* coin */
   config.controls.padconfig[6] = 5;    /* padconfig menu */
   config.controls.padconfig[7] = 6;    /* padconfig view */
   config.controls.analog = 1;
   config.controls.pad_id = 0;        /* pad_id */
   config.controls.axis[0] = 0;       /* wheel */
   config.controls.axis[1] = 2;       /* accelerate */
   config.controls.axis[2] = 3;       /* brake */
   config.controls.asettings[0] = 75; /* wheel zone */
   config.controls.asettings[1] = 0;  /* wheel dead */
   config.controls.asettings[2] = 0;  /* pedals dead */

   config.controls.haptic = 0;
   config.controls.max_force = 0xFFFF;
   config.controls.min_force = 0x1999;
   config.controls.force_duration = 500;

   // ------------------------------------------------------------------------
   // Engine Settings
   // ------------------------------------------------------------------------

   config.engine.dip_time = 0;
   config.engine.dip_traffic = 1;

   config.engine.freeze_timer = config.engine.dip_time == 4;
   config.engine.disable_traffic = config.engine.dip_traffic == 4;
   config.engine.dip_time &= 3;
   config.engine.dip_traffic &= 3;

   config.engine.freeplay = 0;
   config.engine.jap = 0; /* japanese tracks */
   config.engine.prototype = 0;

   // Additional Level Objects
   config.engine.level_objects = 1;
   config.engine.randomgen = 1;
   config.engine.fix_bugs_backup =
       config.engine.fix_bugs = 1;
   config.engine.fix_timer = 0;
   config.engine.layout_debug = 0;
   config.engine.new_attract = 1;

   // ------------------------------------------------------------------------
   // Time Trial Mode
   // ------------------------------------------------------------------------

   config.ttrial.laps = 3;
   config.ttrial.traffic = 3;

   config.cont_traffic = 3;
}

//  libretro.cpp

retro_log_printf_t log_cb;
retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
retro_environment_t environ_cb;
static retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;

static bool libretro_fps_record_inhibit = false;
static int libretro_fps_prev = 0;

char rom_path[1024];

char FILENAME_SCORES[1024];
char FILENAME_TTRIAL[1024];
char FILENAME_CONT[1024];

static bool option_visibility_set = false;
static bool sound_enable_prev = true;
static bool analog_enable_prev = true;

static bool update_option_visibility(void)
{
   struct retro_variable var = {0};
   struct retro_core_option_display option_display = {0};
   bool sound_enable = true;
   bool analog_enable = true;
   bool updated = false;

   /* Check if sound is enabled */
   var.key = "cannonball_sound_enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&
       var.value &&
       (strcmp(var.value, "OFF") == 0))
      sound_enable = false;

   /* Check if analog input is enabled */
   var.key = "cannonball_analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&
       var.value &&
       (strcmp(var.value, "OFF") == 0))
      analog_enable = false;

   /* Hide auxiliary sound options, if required */
   if ((sound_enable != sound_enable_prev) ||
       (!option_visibility_set && !sound_enable))
   {
      option_display.visible = sound_enable;

      option_display.key = "cannonball_sound_advertise";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      option_display.key = "cannonball_sound_preview";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      option_display.key = "cannonball_sound_fix_samples";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      sound_enable_prev = sound_enable;
      updated = true;
   }

   /* Hide auxiliary input options, if required */
   if ((analog_enable != analog_enable_prev) ||
       (!option_visibility_set && analog_enable))
   {
      option_display.visible = !analog_enable;

      option_display.key = "cannonball_steer_speed";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      option_display.key = "cannonball_pedal_speed";
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

      analog_enable_prev = analog_enable;
      updated = true;
   }

   option_visibility_set = true;
   return updated;
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   struct retro_log_callback log;
   struct retro_core_options_update_display_callback update_display_cb;
   bool no_content = true;
   bool option_categories = false;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   libretro_set_core_options(environ_cb, &option_categories);

   update_display_cb.callback = update_option_visibility;
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK,
              &update_display_cb);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }

void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }

void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }

void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void update_geometry()
{
   struct retro_system_av_info av_info;
   /* Calling 'RETRO_ENVIRONMENT_SET_GEOMETRY' does not
    * update frontend timing; we must therefore inhibit
    * any record of the last set frame rate value, or
    * the next timing update may be skipped */
   libretro_fps_record_inhibit = true;
   retro_get_system_av_info(&av_info);
   libretro_fps_record_inhibit = false;
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info);
}

static void update_variables(bool startup)
{
   bool geometry_update = false;
   bool timing_update = false;
   struct retro_variable var;

   var.key = "cannonball_menu_enabled";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.menu.enabled = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.menu.enabled = 0;
   }

   var.key = "cannonball_menu_road_scroll_speed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.menu.road_scroll_speed = atoi(var.value);
   }

   var.key = "cannonball_video_fps";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned int newval = 0;

      if (strcmp(var.value, "Ultra Smooth (120)") == 0)
         newval = 3;
      else if (strcmp(var.value, "Original (60/30)") == 0)
         newval = 1;
      else if (strcmp(var.value, "Low (30)") == 0)
         newval = 0;
      else
         newval = 2;

      if (newval != config.video.fps)
      {
         config.video.fps = newval;
         timing_update = true;
      }
   }

   var.key = "cannonball_video_widescreen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int newval = 0;

      if (strcmp(var.value, "ON") == 0)
         newval = 1;
      else if (strcmp(var.value, "OFF") == 0)
         newval = 0;

      if (newval != config.video.widescreen)
      {
         config.video.widescreen = newval;
         geometry_update = true;
      }
   }

   var.key = "cannonball_video_hires";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int newval = 0;

      if (strcmp(var.value, "ON") == 0)
         newval = 1;
      else if (strcmp(var.value, "OFF") == 0)
         newval = 0;

      if (newval != config.video.hires)
      {
         config.video.hires = newval;
         geometry_update = true;
      }
   }

   var.key = "cannonball_sound_enable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned int newval = 0;

      if (strcmp(var.value, "ON") == 0)
         newval = 1;
      else if (strcmp(var.value, "OFF") == 0)
         newval = 0;

      if (newval != config.sound.enabled)
      {
         config.sound.enabled = newval;
#ifdef COMPILE_SOUND_CODE
         if (config.sound.enabled)
            cannonball::audio.start_audio();
         else
            cannonball::audio.stop_audio();
#endif
      }
   }

   var.key = "cannonball_gear";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int gear_mode = 0;

      if (strcmp(var.value, "Manual Cabinet") == 0)
         gear_mode = 1;
      else if (strcmp(var.value, "Manual 2 Buttons") == 0)
         gear_mode = 2;
      else if (strcmp(var.value, "Automatic") == 0)
         gear_mode = 3;

      config.controls.gear = gear_mode;
   }

   var.key = "cannonball_analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
      {
         config.controls.analog = 1;
         input.analog = 1;
      }
      else if (strcmp(var.value, "OFF") == 0)
      {
         config.controls.analog = 0;
         input.analog = 0;
      }
   }

   var.key = "cannonball_haptic_strength";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int min_force_last = config.controls.min_force;
      int max_force_last = config.controls.max_force;
      int haptic_level = atoi(var.value);

      haptic_level = (haptic_level < 0) ? 0 : haptic_level;
      haptic_level = (haptic_level > 10) ? 10 : haptic_level;

      if (haptic_level == 0)
      {
         config.controls.min_force = 0;
         config.controls.max_force = 0;
         forcefeedback::deactivate_rumble();
      }
      else
      {
         config.controls.min_force = 0x3F +
                                     (haptic_level * ((0x1999 - 0x3F) / 10));
         config.controls.max_force = 0x5 +
                                     (haptic_level * (0xFFFF / 10));
      }

      if ((config.controls.min_force != min_force_last) ||
          (config.controls.max_force != max_force_last))
         forcefeedback::update_force_limits(
             config.controls.max_force,
             config.controls.min_force,
             config.controls.force_duration);
   }

   var.key = "cannonball_freeplay";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.freeplay = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.freeplay = 0;
   }

   var.key = "cannonball_force_ai";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "OFF") == 0)
         config.engine.force_ai = false;
      else
         config.engine.force_ai = true;
   }

   /* All of the remaining options require a core
    * restart to apply */
   if (!startup)
      goto end;

   var.key = "cannonball_sound_advertise";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.sound.advertise = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.sound.advertise = 0;
   }

   var.key = "cannonball_sound_preview";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.sound.preview = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.sound.preview = 0;
   }

   var.key = "cannonball_sound_fix_samples";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.sound.fix_samples = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.sound.fix_samples = 0;
   }

   var.key = "cannonball_steer_speed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      config.controls.steer_speed = atoi(var.value);
   }

   var.key = "cannonball_pedal_speed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      config.controls.pedal_speed = atoi(var.value);
   }

   var.key = "cannonball_jap";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.jap = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.jap = 0;
   }

   var.key = "cannonball_dip_time";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int diptime = 0;
      config.engine.freeze_timer = false;

      if (strcmp(var.value, "Normal (75s)") == 0)
         diptime = 1;
      else if (strcmp(var.value, "Hard (72s)") == 0)
         diptime = 2;
      else if (strcmp(var.value, "Very Hard (70s)") == 0)
         diptime = 3;
      else if (strcmp(var.value, "Infinite Time") == 0)
      {
         diptime = 4;
         config.engine.freeze_timer = true;
      }

      config.engine.dip_time = diptime;
   }

   var.key = "cannonball_dip_traffic";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int diptraffic = 0;
      config.engine.disable_traffic = false;

      if (strcmp(var.value, "Normal") == 0)
         diptraffic = 1;
      else if (strcmp(var.value, "Hard") == 0)
         diptraffic = 2;
      else if (strcmp(var.value, "Very Hard") == 0)
         diptraffic = 3;
      else if (strcmp(var.value, "No Traffic") == 0)
      {
         diptraffic = 4;
         config.engine.disable_traffic = true;
      }

      config.engine.dip_traffic = diptraffic;
   }

   var.key = "cannonball_level_objects";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.level_objects = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.level_objects = 0;
   }

   var.key = "cannonball_prototype";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.prototype = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.prototype = 0;
   }

   var.key = "cannonball_new_attract";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.new_attract = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.new_attract = 0;
   }

   var.key = "cannonball_randomgen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "OFF") == 0)
         config.engine.randomgen = 0;
      else
         config.engine.randomgen = 1;
   }

   var.key = "cannonball_fix_bugs";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.fix_bugs = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.fix_bugs = 0;
   }

   var.key = "cannonball_fix_timer";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.fix_timer = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.fix_timer = 0;
   }

   var.key = "cannonball_ttrial_laps";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.ttrial.laps = atoi(var.value);
   }

   var.key = "cannonball_ttrial_traffic";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.ttrial.traffic = atoi(var.value);
   }

   var.key = "cannonball_cont_traffic";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      config.cont_traffic = atoi(var.value);
   }

   var.key = "cannonball_layout_debug";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "ON") == 0)
         config.engine.layout_debug = 1;
      else if (strcmp(var.value, "OFF") == 0)
         config.engine.layout_debug = 0;
   }

end:
   if (geometry_update)
   {
      video.disable();
      video.init(&roms, &config.video);
      video.sprite_layer->set_x_clip(false);
      update_geometry();
   }

   if (timing_update)
      config.set_fps(config.video.fps);

   /* Show/hide core options */
   update_option_visibility();

   /* Update menu to reflect any option changes */
   if (!startup && (state == STATE_MENU))
      menu->refresh_menu();
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "Cannonball";
   info->library_version = "git";
   info->need_fullpath = true;
   info->valid_extensions = "game";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   unsigned scale_factor = config.video.hires ? 2 : 1;

   memset(info, 0, sizeof(*info));

   info->timing.fps = config.fps;
   /* Due to integer rounding errors (44100/102 = 367.5),
    * we produce fewer than the expected 44100 samples
    * per second when running at 120 fps... */
   info->timing.sample_rate = (config.fps == 120) ? 44040 : 44100;

   info->geometry.max_width = S16_WIDTH_WIDE << 1;
   info->geometry.max_height = S16_HEIGHT << 1;

   if (config.video.widescreen)
   {
      info->geometry.base_width = S16_WIDTH_WIDE * scale_factor;
      info->geometry.base_height = S16_HEIGHT * scale_factor;
      info->geometry.aspect_ratio = 16.0 / 9.0;
   }
   else
   {
      info->geometry.base_width = S16_WIDTH * scale_factor;
      info->geometry.base_height = S16_HEIGHT * scale_factor;
      info->geometry.aspect_ratio = 4.0 / 3.0;
   }

   if (!libretro_fps_record_inhibit)
      libretro_fps_prev = config.fps;
}

void update_timing(void)
{
   struct retro_system_av_info system_av_info;
   retro_get_system_av_info(&system_av_info);
   environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   return false;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

static void retro_osd_error_msg(const char *str)
{
   unsigned msg_interface_version = 0;

   if (string_is_empty(str))
      return;

   environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION,
              &msg_interface_version);

   if (msg_interface_version >= 1)
   {
      struct retro_message_ext msg;
      msg.msg = str;
      msg.duration = 3000;
      msg.priority = 3;
      msg.level = RETRO_LOG_ERROR;
      msg.target = RETRO_MESSAGE_TARGET_ALL;
      msg.type = RETRO_MESSAGE_TYPE_NOTIFICATION;
      msg.progress = -1;
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
   }
   else
   {
      struct retro_message msg;
      msg.msg = str;
      msg.frames = 180;
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
   }
}

static void retro_build_save_paths(void)
{
   const char *save_dir = NULL;

   FILENAME_SCORES[0] = '\0';
   FILENAME_TTRIAL[0] = '\0';
   FILENAME_CONT[0] = '\0';

   /* Get frontend save directory
    * > Use game data directory as a fallback if
    *   no save directory is defined */
   if (!environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) ||
       !save_dir)
      save_dir = rom_path;

   /* Build high score save paths
    * > Note: These are not 'full' paths;
    *   suffix + extension are added elsewhere */
   fill_pathname_join(FILENAME_SCORES, save_dir,
                      "hiscores", sizeof(FILENAME_SCORES));

   fill_pathname_join(FILENAME_TTRIAL, save_dir,
                      "hiscores_timetrial", sizeof(FILENAME_TTRIAL));

   fill_pathname_join(FILENAME_CONT, save_dir,
                      "hiscores_continuous", sizeof(FILENAME_CONT));
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   struct retro_input_descriptor desc[] = {
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Gear (Low, 2 Buttons Mode)"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Accelerate"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Gear (High, 2 Buttons Mode)"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Brake"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Adjust View"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Go Back To Menu"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Analog Brake"},
       {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Analog Accelerate"},
       {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X"},
       {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y"},

       {0},
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[Cannonball]: RGB565 is not supported.\n");
      return false;
   }

   if (info && !string_is_empty(info->path))
      fill_pathname_basedir(rom_path, info->path, sizeof(rom_path));
   else
   {
      const char *system_dir = NULL;
      bool path_valid = false;

      if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
      {
         fill_pathname_join(rom_path, system_dir,
                            "cannonball", sizeof(rom_path));
         fill_pathname_slash(rom_path, sizeof(rom_path));

         path_valid = path_is_directory(rom_path);
      }

      if (!path_valid)
      {
         retro_osd_error_msg("Cannonball game files missing from frontend system directory");
         return false;
      }
   }

   log_cb(RETRO_LOG_INFO, "Rom directory: %s\n", rom_path);
   retro_build_save_paths();

   bool loaded = roms.load_revb_roms();

   if (!loaded)
   {
      retro_osd_error_msg("Cannonball ROM files missing from game directory");
      return false;
   }

   config_init();

   update_variables(true);

   // Load fixed PCM ROM based on config
   if (config.sound.fix_samples)
      roms.load_pcm_rom(true);

   // Load patched widescreen tilemaps
   if (!omusic.load_widescreen_map())
   {
      if (log_cb)
         log_cb(RETRO_LOG_WARN, "[Cannonball]: Unable to load widescreen tilemaps.\n");
   }

   if (!video.init(&roms, &config.video))
      return false;

   menu = new Menu(&cannonboard);

#ifdef COMPILE_SOUND_CODE
   audio.init();
#endif
   state = config.menu.enabled ? STATE_INIT_MENU : STATE_INIT_GAME;

   // Initialize controls
   input.init(config.controls.pad_id,
              config.controls.keyconfig, config.controls.padconfig,
              config.controls.analog, config.controls.axis, config.controls.asettings);

   config.controls.haptic = forcefeedback::init_rumble_interface(environ_cb);
   if (config.controls.haptic)
      config.controls.haptic = forcefeedback::init(
          config.controls.max_force,
          config.controls.min_force,
          config.controls.force_duration);

#ifdef CANNONBOARD
   // Initialize CannonBoard (For use in original cabinets)
   if (config.cannonboard.enabled)
   {
      cannonboard.init(config.cannonboard.port, config.cannonboard.baud);
      cannonboard.start();
   }
#endif

   // Populate menus
   menu->populate();

   return true;
}

bool retro_load_game_special(unsigned game_type,
                             const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void)
{
#ifdef COMPILE_SOUND_CODE
   audio.stop_audio();
#endif
   input.close();
   forcefeedback::close();
   delete menu;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void *retro_get_memory_data(unsigned id)
{
   switch (id)
   {
   case RETRO_MEMORY_SYSTEM_RAM:
   default:
      break;
   }

   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   return 0;
}

void retro_init(void)
{
   unsigned level = 2;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

   lr_options::init();

   option_visibility_set = false;
   sound_enable_prev = true;
   analog_enable_prev = true;
}

void retro_deinit(void)
{
   lr_options::close();

   libretro_fps_record_inhibit = false;
   libretro_fps_prev = 0;

   libretro_supports_bitmasks = false;
}

void retro_reset(void)
{
}

struct button_bind
{
   unsigned id;
   unsigned joy_id;
};

static struct button_bind binds[] = {
    {273, RETRO_DEVICE_ID_JOYPAD_UP},
    {274, RETRO_DEVICE_ID_JOYPAD_DOWN},
    {276, RETRO_DEVICE_ID_JOYPAD_LEFT},
    {275, RETRO_DEVICE_ID_JOYPAD_RIGHT},
    {122, RETRO_DEVICE_ID_JOYPAD_B},     /* Accelerate */
    {120, RETRO_DEVICE_ID_JOYPAD_Y},     /* Brake */
    {32, RETRO_DEVICE_ID_JOYPAD_X},      /* Gear 1 */
    {33, RETRO_DEVICE_ID_JOYPAD_A},      /* Gear 2 */
    {49, RETRO_DEVICE_ID_JOYPAD_START},  /* Start  */
    {53, RETRO_DEVICE_ID_JOYPAD_SELECT}, /* Coin  */
    {304, RETRO_DEVICE_ID_JOYPAD_L},     /* View  */
    {286, RETRO_DEVICE_ID_JOYPAD_R},     /* Menu  */
};

static void process_events(void)
{
   unsigned i;
   int analog_left_x, analog_r2, analog_l2;

   input_poll_cb();

   for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R + 1); i++)
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, binds[i].joy_id))
         input.handle_key(binds[i].id, true);
      else
         input.handle_key(binds[i].id, false);
   }

   analog_left_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
                                  RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
   analog_r2 = input_state_cb(0, RETRO_DEVICE_ANALOG,
                              RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
   analog_l2 = input_state_cb(0, RETRO_DEVICE_ANALOG,
                              RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2);

   /* Fallback to digital */
   if (config.controls.analog == 1)
   {
      if (analog_r2 == 0)
         analog_r2 = (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B)) ? 0x7FFF : 0;
      if (analog_l2 == 0)
         analog_l2 = (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y)) ? 0x7FFF : 0;
      if (analog_left_x == 0)
      {
         analog_left_x += (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) ? -0x7FFF : 0;
         analog_left_x += (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) ? 0x7FFF : 0;
      }
   }

   input.handle_joy_axis(analog_left_x, analog_r2, analog_l2);
}

void retro_run(void)
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables(false);

   if (config.fps != libretro_fps_prev)
      update_timing();

   frame++;

   // Get CannonBoard Packet Data
   Packet *packet = NULL;

#ifdef CANNONBOARD
   if (config.cannonboard.enabled)
      packet = cannonboard.get_packet();
#endif

   switch (config.fps)
   {
   case 60:
      /* 60 fps
       * Non-standard: tick every second frame */
      tick_frame = frame & 1;
      break;
   case 120:
      /* 120 fps
       * Non-standard: tick every fourth frame */
      tick_frame = (frame & 3) == 1;
      break;
   case 30:
   default:
      /* 30 fps
       * Standard rate: tick every frame */
      tick_frame = true;
      break;
   }

   process_events();

   if (tick_frame)
   {
      oinputs.tick(packet); // Do Controls
      oinputs.do_gear();    // Digital Gear
   }

   switch (state)
   {
   case STATE_GAME:
   {
      if (tick_frame)
      {
         if (input.has_pressed(Input::TIMER))
            outrun.freeze_timer = !outrun.freeze_timer;
         if (input.has_pressed(Input::PAUSE))
            pause_engine = !pause_engine;
         if (input.has_pressed(Input::MENU))
            state = STATE_INIT_MENU;
      }

      if (!pause_engine || input.has_pressed(Input::STEP))
      {
         outrun.tick(packet, tick_frame);
         if (tick_frame)
            input.frame_done();

#ifdef COMPILE_SOUND_CODE
         // Tick audio program code
         osoundint.tick();
         // Tick Audio
         audio.tick();
#endif
      }
      else
      {
         if (tick_frame)
            input.frame_done();
      }
   }
   break;

   case STATE_INIT_GAME:
      if (config.engine.jap && !roms.load_japanese_roms())
      {
         state = STATE_QUIT;
      }
      else
      {
         pause_engine = false;
         outrun.init();
         state = STATE_GAME;
      }
      break;

   case STATE_MENU:
   {
      menu->tick(packet);
      input.frame_done();
#ifdef COMPILE_SOUND_CODE
      // Tick audio program code
      osoundint.tick();
      // Tick Audio
      audio.tick();
#endif
   }
   break;

   case STATE_INIT_MENU:
      oinputs.init();
      outrun.outputs->init();
      menu->init();
      state = STATE_MENU;
      break;

   case STATE_QUIT:
      environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }

#ifdef CANNONBOARD
   // Write CannonBoard Outputs
   if (config.cannonboard.enabled)
      cannonboard.write(outrun.outputs->dig_out, outrun.outputs->hw_motor_control);
#endif

   // Draw Video
   video.draw_frame();

   // Stop any haptic feedback effects if
   // duration timer has elapsed
   forcefeedback::update_rumble_interface();
}
