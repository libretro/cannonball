/***************************************************************************
    OutRun Engine Entry Point.

    This is the hub of the ported OutRun code.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "../setup.hpp"
#include "main.hpp"
#include "trackloader.hpp"
#include "../utils.hpp"
#include "oattractai.hpp"
#include "oanimseq.hpp"
#include "obonus.hpp"
#include "ocrash.hpp"
#include "oferrari.hpp"
#include "ohiscore.hpp"
#include "ohud.hpp"
#include "oinputs.hpp"
#include "olevelobjs.hpp"
#include "ologo.hpp"
#include "omap.hpp"
#include "omusic.hpp"
#include "ooutputs.hpp"
#include "osmoke.hpp"
#include "outrun.hpp"
#include "opalette.hpp"
#include "ostats.hpp"
#include "otiles.hpp"
#include "otraffic.hpp"
#include "outils.hpp"

Outrun outrun;

/*
    Known Core Engine Issues:

    - Road split. Minor bug on positioning traffic on correct side of screen for one frame or so at the point of split.
      Most noticeable in 60fps mode. 
      The Dreamcast version exhibits a bug where the road renders on the wrong side of the screen for one frame at this point.
      The original version (and Cannonball) has a problem where the cars face the wrong direction for one frame. 

    Bugs Present In Original 1986 Release:

    - Millisecond displays incorrectly on Extend Time screen [fixed]
    - Erroneous values in sprite zooming table [fixed]
    - Shadow popping into position randomly. Try setting car x position to 0x1E2. (0x260050) [fixed]
    - Stage 2a: Incomplete arches due to lack of sprite slots [fixed]
    - Best OutRunners screen looks odd after Stage 2 Gateway
    - Stage 3c: Clouds overlapping trees [unable to fix easily]
    - Sometimes the Ferrari stalls on the start-line on game restart. Happens in Attract Mode too.
    - On completion screen, some of the side crowd graphics are misplaced. Japanese version only [fixed]

*/

Outrun::Outrun()
{
    outputs = new OOutputs();
}

Outrun::~Outrun()
{
    delete outputs;
}

void Outrun::init()
{
    freeze_timer = cannonball_mode == MODE_TTRIAL ? true : config.engine.freeze_timer;
    video.enabled = false;
    select_course(config.engine.jap != 0, config.engine.prototype != 0);
    Video_clear_text_ram(&video);

    tick_counter = 0;

    if (config.controls.haptic)
        outputs->set_mode(OOutputs::MODE_FFEEDBACK);
    else
        outputs->set_mode(OOutputs::MODE_DISABLED);

    boot();
}

void Outrun::boot()
{
    game_state = config.engine.layout_debug ? GS_INIT_GAME : GS_INIT;
    /* Initialize default hi-score entries */
    OHiScore_init_def_scores(&ohiscore);
    /* Load saved hi-score entries */
    Config_load_scores(&config, 
        cannonball_mode == Outrun::MODE_ORIGINAL
            ? FILENAME_SCORES
            : FILENAME_CONT);
    OStats_init(&ostats, cannonball_mode == MODE_TTRIAL);
    init_jump_table();
    OInitEngine_init(&oinitengine, cannonball_mode == MODE_TTRIAL ? ttrial.level : 0);
    OSoundInt_init(&osoundint);
    outils::reset_random_seed(); /* Ensure we match the genuine boot up of the original game each time */
}

void Outrun::tick(bool tick_frame)
{
    this->tick_frame = tick_frame;
    
    if (tick_frame)
    {
        tick_counter++;

        if (game_state >= GS_START1 && game_state <= GS_INGAME)
        {
            if (Input_has_pressed(&input, VIEWPOINT))
            {
                int mode = ORoad_get_view_mode(&oroad) + 1;
                if (mode > VIEW_INCAR)
                    mode = VIEW_ORIGINAL;

                ORoad_set_view_mode(&oroad, mode, false);
            }
        }
    }

    /* Only tick the road cpu twice for every time we tick the main cpu */
    /* The timing here isn't perfect, as normally the road CPU would run in parallel with the main CPU. */
    /* We can potentially hack this by calling the road CPU twice. */
    /* Most noticeable with clipping sprites on hills. */
      
    /* 30 FPS  */
    /* Updates Game Logic 1/2 frames */
    /* Updates V-Blank 1/2 frames */
    if (config.fps == 30 && config.tick_fps == 30)
    {
        jump_table();
        ORoad_tick(&oroad);
        vint();
        vint();
    }
    /* 30/60 FPS Hybrid. (This is the same as the original game) */
    /* Updates Game Logic 1/2 frames */
    /* Updates V-Blank 1/1 frames */
    else if (config.fps == 60 && config.tick_fps == 30)
    {
        if (tick_frame)
        {
            jump_table();
            ORoad_tick(&oroad);
        }
        vint();
    }
    /* 60 FPS. Smooth Mode.  */
    /* Updates Game Logic 1/1 frames */
    /* Updates V-Blank 1/1 frames */
    else
    {
        jump_table();
        ORoad_tick(&oroad);
        vint();
    }

    /* Moved out of vertical interrupt */
    if (tick_frame)
    {
        uint8_t coin = OInputs_do_credits(&oinputs);
        outputs->coin_chute_out(&outputs->chute1, coin == 1);
        outputs->coin_chute_out(&outputs->chute2, coin == 2);
    }

    /* Draw FPS */
    if (config.video.fps_count)
        OHud_draw_fps_counter(&ohud, cannonball_fps_counter);
}

/* Vertical Interrupt */
void Outrun::vint()
{
    otiles.write_tilemap_hw();
    OSprites_update_sprites(&osprites);
    otiles.update_tilemaps(cannonball_mode == MODE_ORIGINAL ? ostats.cur_stage : 0);
    OPalette_cycle_sky_palette(&opalette);
    OPalette_fade_palette(&opalette);
    OStats_do_timers(&ostats);
    if (cannonball_mode != MODE_TTRIAL) OHud_draw_timer1(&ohud, ostats.time_counter);
    OInitEngine_set_granular_position(&oinitengine);
}

void Outrun::jump_table()
{
    if (tick_frame && game_state != GS_CALIBRATE_MOTOR)
    {
        main_switch();                  /* Address #1 (0xB128) - Main Switch */
        OInputs_adjust_inputs(&oinputs);        /* Address #2 (0x74D8) - Adjust Analogue Inputs */
    }

    switch (game_state)
    {
        case GS_REINIT:
        case GS_CALIBRATE_MOTOR:
            break;
 
        /* ---------------------------------------------------------------------------------------- */
        /* Couse Map Specific Code */
        /* ---------------------------------------------------------------------------------------- */
        case GS_MAP:
            OMap_tick(&omap);
            break;


        case GS_MUSIC:
            if (tick_frame) omusic.check_start(); /* Check for start button */
            OSprites_tick(&osprites);
            OLevelObjs_do_sprite_routine(&olevelobjs);

            if (!outrun.tick_frame)
            {
                omusic.blit();
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Best OutRunners Entry (EditJumpTable3 Entries) */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_BEST2:
        case GS_BEST2:
            OSprites_tick(&osprites);
            OLevelObjs_do_sprite_routine(&olevelobjs);

            if (!tick_frame)
            {
                /* Check for start button if credits are remaining and set state to Music Selection */
                if (ostats.credits && Input_is_pressed_clear(&input, START))
                    game_state = GS_INIT_MUSIC;
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Core Game Engine Routines */
        /* ---------------------------------------------------------------------------------------- */
        case GS_LOGO:
            if (!tick_frame)
                OLogo_blit(&ologo);

        case GS_ATTRACT:
        case GS_BEST1:
            if (tick_frame) check_freeplay_start();
        
        default:
            if (tick_frame) OSprites_tick(&osprites);                /* Address #3 Jump_SetupSprites */
            OLevelObjs_do_sprite_routine(&olevelobjs);                 /* replaces calling each sprite individually */
            if (!config.engine.disable_traffic)
                OTraffic_tick(&otraffic);                            /* Spawn & Tick Traffic */
            if (tick_frame) OInitEngine_init_crash_bonus(&oinitengine); /* Initalize crash sequence or bonus code */
            OFerrari_tick(&oferrari);
            if (oferrari.state != FERRARI_END_SEQ)
            {
                OAnimSeq_flag_seq(&oanimseq);
                OCrash_tick(&ocrash);
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE1]); /* Do Left Hand Smoke */
                OFerrari_draw_shadow(&oferrari);                                                   /* (0xF1A2) - Draw Ferrari Shadow */
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE2]); /* Do Right Hand Smoke */
            }
            else
            {
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE1]); /* Do Left Hand Smoke */
                OSmoke_draw_ferrari_smoke(&osmoke, &osprites.jump_table[SPRITE_SMOKE2]); /* Do Right Hand Smoke */
            }
            break;
    }

    OSprites_sprite_copy(&osprites);

    /* Libretro force feedback uses the current steering position. */
    if (tick_frame && config.controls.haptic)
        outputs->tick(oinputs.input_steering);
}

/* Source: 0xB15E */
void Outrun::main_switch()
{
    switch (game_state)
    {
        case GS_INIT:  
            init_attract();
            /* fall through */
            
        /* ---------------------------------------------------------------------------------------- */
        /* Attract Mode */
        /* ---------------------------------------------------------------------------------------- */
        case GS_ATTRACT:
            tick_attract();
            break;

        case GS_INIT_BEST1:
            oferrari.car_ctrl_active = false;
            oinitengine.car_increment = 0;
            oferrari.car_inc_old = 0;
            ostats.time_counter = 5;
            ostats.frame_counter = frame_reset;
            OHiScore_init(&ohiscore);
            OSoundInt_queue_sound(&osoundint, SOUND_FM_RESET);
            cannonball_audio.clear_wav();
            game_state = GS_BEST1;

        case GS_BEST1:
            OHud_draw_copyright_text(&ohud);
            OHiScore_display_scores(&ohiscore);
            OHud_draw_credits(&ohud);
            OHud_draw_insert_coin(&ohud);
            if (ostats.credits)
                game_state = GS_INIT_MUSIC;
            else if (decrement_timers())
                game_state = GS_INIT_LOGO;
            break;

        case GS_INIT_LOGO:
            Video_clear_text_ram(&video);
            oferrari.car_ctrl_active = false;
            oinitengine.car_increment = 0;
            oferrari.car_inc_old = 0;
            ostats.time_counter = 5;
            ostats.frame_counter = frame_reset;
            OSoundInt_queue_sound(&osoundint, 0);
            OLogo_enable(&ologo, SOUND_FM_RESET);
            game_state = GS_LOGO;

        case GS_LOGO:
            OHud_draw_credits(&ohud);
            OHud_draw_copyright_text(&ohud);
            OHud_draw_insert_coin(&ohud);
            OLogo_tick(&ologo);

            if (ostats.credits)
                game_state = GS_INIT_MUSIC;
            else if (decrement_timers())
            {
                OLogo_disable(&ologo);
                game_state = GS_INIT; /* Resume attract mode */
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Music Select Screen */
        /* ---------------------------------------------------------------------------------------- */

        case GS_INIT_MUSIC:
            omusic.enable();
            game_state = GS_MUSIC;

        case GS_MUSIC:
            OHud_draw_credits(&ohud);
            OHud_draw_insert_coin(&ohud);
            omusic.tick();
            if (decrement_timers())
            {
                omusic.disable();
                game_state = GS_INIT_GAME;
            }
            break;
        /* ---------------------------------------------------------------------------------------- */
        /* In-Game */
        /* ---------------------------------------------------------------------------------------- */

        case GS_INIT_GAME:
            /*ROM:0000B3E8                 move.w  #-1,(ingame_active1).l              ; Denote in-game engine is active */
            /*ROM:0000B3F0                 clr.l   (prev_game_time).l                  ; Reset overall game time */
            /*ROM:0000B3F6                 move.w  #-1,(ingame_active2).l */
            Video_clear_text_ram(&video);
            oferrari.car_ctrl_active = true;
            init_jump_table();
            OInitEngine_init(&oinitengine, cannonball_mode == MODE_TTRIAL ? ttrial.level : 0);
            /* Timing Hack to ensure horizon is correct */
            /* Note that the original code disables the screen, and waits for the second CPU's interrupt instead */
            ORoad_tick(&oroad);
            ORoad_tick(&oroad);
            ORoad_tick(&oroad);
            OSoundInt_queue_sound(&osoundint, SOUND_STOP_CHEERS);
            OSoundInt_queue_sound(&osoundint, SOUND_VOICE_GETREADY);
            OSoundInt_queue_sound(&osoundint, SOUND_REVS);             /* Moved from Z80 Code for extra flexibility */
            omusic.play_music(-1);
            
            if (!freeze_timer)
                ostats.time_counter = TIME[config.engine.dip_time * 40]; /* Set time to begin level with */
            else
                ostats.time_counter = 0x30;

            ostats.frame_counter = frame_reset + 50;
            ostats.credits--;                                   /* Update Credits */
            OHud_blit_text1(&ohud, TEXT1_CLEAR_START);
            OHud_blit_text1(&ohud, TEXT1_CLEAR_CREDITS);
            OSoundInt_queue_sound(&osoundint, SOUND_INIT_CHEERS);
            video.enabled = true;
            game_state = GS_START1;
            OHud_draw_main_hud(&ohud);
            /* fall through */

        /*  Start Game - Car Driving In */
        case GS_START1:
        case GS_START2:
            if (--ostats.frame_counter < 0)
            {
                OSoundInt_queue_sound(&osoundint, SOUND_SIGNAL1);
                ostats.frame_counter = frame_reset;
                game_state++;
            }
            break;

        case GS_START3:
            if (--ostats.frame_counter < 0)
            {
                if (cannonball_mode == MODE_TTRIAL)
                {
                    OHud_clear_timetrial_text(&ohud);
                }

                OSoundInt_queue_sound(&osoundint, SOUND_SIGNAL2);
                OSoundInt_queue_sound(&osoundint, SOUND_STOP_CHEERS);
                ostats.frame_counter = frame_reset;
                game_state++;
            }
            break;

        case GS_INGAME:
            if (decrement_timers())
                game_state = GS_INIT_GAMEOVER;
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Bonus Mode */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_BONUS:
            ostats.frame_counter = frame_reset;
            obonus.bonus_control = BONUS_INIT;  /* Initialize Bonus Mode Logic */
            oroad.road_load_end   |= BIT_0;             /* Instruct CPU 1 to load end road section */
            ostats.game_completed |= BIT_0;             /* Denote game completed */
            obonus.bonus_timer = 3600;                  /* Safety Timer Added in Rev. A Roms */
            game_state = GS_BONUS;

        case GS_BONUS:
            if (--obonus.bonus_timer < 0)
            {
                obonus.bonus_control = BONUS_DISABLE;
                game_state = GS_INIT_GAMEOVER;
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Display Game Over Text */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_GAMEOVER:
            if (cannonball_mode != MODE_TTRIAL)
            {
                oferrari.car_ctrl_active = false; /* -1 */
                oinitengine.car_increment = 0;
                oferrari.car_inc_old = 0;
                ostats.time_counter = 3;
                ostats.frame_counter = frame_reset;
                OHud_blit_text2(&ohud, TEXT2_GAMEOVER);
            }
            else
            {
                OHud_blit_text_big(&ohud, 7, ttrial.new_high_score ? "NEW RECORD" : "BAD LUCK", false);

                OHud_blit_text1(&ohud, TEXT1_LAPTIME1);
                OHud_blit_text1(&ohud, TEXT1_LAPTIME2);
                OHud_draw_lap_timer(&ohud, 0x110554, ttrial.best_lap, ttrial.best_lap[2]);

                OHud_blit_text_new(&ohud, 9,  14, "OVERTAKES          - ", GREY);
                OHud_blit_text_new(&ohud, 31, 14, Utils::to_string((int) ttrial.overtakes).c_str(), GREEN);
                OHud_blit_text_new(&ohud, 9,  16, "VEHICLE COLLISIONS - ", GREY);
                OHud_blit_text_new(&ohud, 31, 16, Utils::to_string((int) ttrial.vehicle_cols).c_str(), GREEN);
                OHud_blit_text_new(&ohud, 9,  18, "CRASHES            - ", GREY);
                OHud_blit_text_new(&ohud, 31, 18, Utils::to_string((int) ttrial.crashes).c_str(), GREEN);
            }
            OSoundInt_queue_sound(&osoundint, SOUND_NEW_COMMAND);
            game_state = GS_GAMEOVER;

        case GS_GAMEOVER:
            if (cannonball_mode == MODE_ORIGINAL)
            {
                if (decrement_timers())
                    game_state = GS_INIT_MAP;
            }
            else if (cannonball_mode == MODE_CONT)
            {
                if (decrement_timers())
                    init_best_outrunners();
            }
            else if (cannonball_mode == MODE_TTRIAL)
            {
                if (outrun.tick_counter & BIT_4)
                    OHud_blit_text1(&ohud, 10, 20, TEXT1_PRESS_START);
                else
                    OHud_blit_text1(&ohud, 10, 20, TEXT1_CLEAR_START);

                if (Input_is_pressed(&input, START))
                    cannonball_state = STATE_INIT_MENU;
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Display Course Map */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_MAP:
            OMap_init(&omap);
            OHud_blit_text2(&ohud, TEXT2_COURSEMAP);
            game_state = GS_MAP;
            /* fall through */

        case GS_MAP:
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Best OutRunners / Score Entry */
        /* ---------------------------------------------------------------------------------------- */
        case GS_INIT_BEST2:
            ORoad_set_view_mode(&oroad, VIEW_ORIGINAL, true);
            /* bsr.w   EndGame */
            OSprites_disable_sprites(&osprites);
            OTraffic_disable_traffic(&otraffic);
            /* bsr.w   EditJumpTable3 */
            OSprites_clear_palette_data(&osprites);
            OLevelObjs_init_hiscore_sprites(&olevelobjs);
            ocrash.coll_count1   = 0;
            ocrash.coll_count2   = 0;
            ocrash.crash_counter = 0;
            ocrash.skid_counter  = 0;
            ocrash.spin_control1 = 0;
            oferrari.car_ctrl_active = false; /* -1 */
            oinitengine.car_increment = 0;
            oferrari.car_inc_old = 0;
            ostats.time_counter = config.engine.hiscore_timer;
            ostats.frame_counter = frame_reset;
            OHiScore_init(&ohiscore);
            OSoundInt_queue_sound(&osoundint, SOUND_NEW_COMMAND);
            OSoundInt_queue_sound(&osoundint, SOUND_FM_RESET);
            cannonball_audio.clear_wav();
            game_state = GS_BEST2;
            /* fall through */

        case GS_BEST2:
            OHiScore_tick(&ohiscore); /* Do High Score Logic */
            OHud_draw_credits(&ohud);

            /* If countdown has expired */
            if (decrement_timers())
            {
                /*ROM:0000B700                 bclr    #5,(ppi1_value).l                   ; Turn screen off (not activated until PPI written to) */
                oferrari.car_ctrl_active = true; /* 0 : Allow road updates */
                init_jump_table();
                OInitEngine_init(&oinitengine, cannonball_mode == MODE_TTRIAL ? ttrial.level : 0);
                /*ROM:0000B716                 bclr    #0,(byte_260550).l */
                game_state = GS_REINIT;          /* Reinit game to attract mode */
            }
            break;

        /* ---------------------------------------------------------------------------------------- */
        /* Reinitialize Game After High Score Entry */
        /* ---------------------------------------------------------------------------------------- */
        case GS_REINIT:
            Video_clear_text_ram(&video);
            game_state = GS_INIT;
            break;
    }

    OInitEngine_update_road(&oinitengine);
    OInitEngine_update_engine(&oinitengine);

    /* -------------------------------------------------------------------------------------------- */
    /* Debugging Only */
    /* -------------------------------------------------------------------------------------------- */
#ifndef NDEBUG
    {
        if (oinitengine.rd_split_state != 0)
        {
            if (!fork_chosen)
            {
                if (oinitengine.camera_x_off < 0)
                    fork_chosen = -1;
                else
                    fork_chosen = 1;        
            }
        }
        else if (fork_chosen)
            fork_chosen = 0;

        /* Hack to allow user to choose road fork with left/right */
        if (fork_chosen == -1)
        {
            oroad.road_width_bak = oroad.road_width >> 16; 
            oroad.car_x_bak = -oroad.road_width_bak; 
            oinitengine.car_x_pos = oroad.car_x_bak;
        }
        else
        {
            oroad.road_width_bak = oroad.road_width >> 16; 
            oroad.car_x_bak = oroad.road_width_bak; 
            oinitengine.car_x_pos = oroad.car_x_bak;
        } 
    }
#endif
}

/* Setup Jump Table. Move from ROM to RAM. */
/* */
/* Source Address: 0x7E1C */
/* Input:          Sprite To Copy */
/* Output:         None */
/* */
/* ROM Format [0xF000 - 0xF1F5] */
/* */
/* Word 1: Number of entries [7D] */
/* Long 1: Address 1 (address of jump information) */
/* ... */
/* Long x: Address x */
/* */
/* Each address in the jump table is a pointer into ROM containing 0x1F words  */
/* of info (so info is at 0x40 boundary in bytes) */
/* */
/* RAM Format[0x61800] */
/* */
/* 0x00 byte: If high byte set, take jump */
/* 0x01 byte: Index number */
/* 0x02 long: Address to jump to */
void Outrun::init_jump_table()
{
    /* Reset value to restore car increment to during attract mode */
    car_inc_bak = 0;

    OSprites_init(&osprites);
    if (cannonball_mode != MODE_TTRIAL) 
    {
        OTraffic_init_stage1_traffic(&otraffic);      /* Hard coded traffic in right hand lane */
        if (trackloader.display_start_line)
            OLevelObjs_init_startline_sprites(&olevelobjs); /* Hard coded start line sprites (not part of level data) */
    }
    else if (trackloader.display_start_line)
        OLevelObjs_init_timetrial_sprites(&olevelobjs);

    OTraffic_init(&otraffic);
    OSmoke_init(&osmoke);
    ORoad_init(&oroad);
    otiles.init();
    OPalette_init(&opalette);
    OInputs_init(&oinputs);
    OBonus_init(&obonus);
    outputs->init();

    video.tile_layer->set_x_clamp(video.tile_layer->RIGHT);
    video.sprite_layer->set_x_clip(false);
}

/* ------------------------------------------------------------------------------- */
/* Decrement Game Time */
/*  */
/* Decrements Frame Count, and Overall Time Counter */
/* */
/* Returns true if timer expired. */
/* Source: 0xB736 */
/* ------------------------------------------------------------------------------- */
bool Outrun::decrement_timers()
{
    /* Cheat */
    if (freeze_timer && game_state == GS_INGAME)
        return false;

    /* Correct count-down timer running fast at 1/29th (3%) */
    /* Fix timer counting extra second */
    if (config.engine.fix_timer)
    {
        if (--ostats.frame_counter > 0)
            return false;

        ostats.frame_counter = frame_reset;
        ostats.time_counter  = outils::bcd_sub(1, ostats.time_counter);

        /* We need to manually refresh the HUD here to display '0' seconds */
        if (ostats.time_counter == 0)
            OHud_draw_timer1(&ohud, 0);

        return (ostats.time_counter == 0);
    }
    else
    {
        if (--ostats.frame_counter >= 0)
            return false;

        ostats.frame_counter = frame_reset;
        ostats.time_counter  = outils::bcd_sub(1, ostats.time_counter);
        return (ostats.time_counter < 0);
    }
}

/* ------------------------------------------------------------------------------- */
/* Motor calibration */
/* ------------------------------------------------------------------------------- */

void Outrun::init_motor_calibration()
{
    otiles.init();
    OPalette_init(&opalette);
    OInputs_init(&oinputs);
    outputs->init();

    video.tile_layer->set_x_clamp(video.tile_layer->RIGHT);
    video.sprite_layer->set_x_clip(false);

    otiles.fill_tilemap_color(0x4F60); /* Fill Tilemap Light Blue */

    video.enabled        = true;
    osoundint.has_booted = true;

    ORoad_init(&oroad);
    oroad.horizon_set    = 1;
    oroad.horizon_base   = HORIZON_OFF;
    game_state           = GS_CALIBRATE_MOTOR;


    /* Write Palette To RAM */
    uint32_t dst = 0x120000;
    const static uint32_t PAL_SERVICE[] = {0xFF, 0xFF00FF, 0xFF00FF, 0xFF0000};
    Video_write_pal32(&video, &dst, PAL_SERVICE[0]);
    Video_write_pal32(&video, &dst, PAL_SERVICE[1]);
    Video_write_pal32(&video, &dst, PAL_SERVICE[2]);
    Video_write_pal32(&video, &dst, PAL_SERVICE[3]);
}

/* ------------------------------------------------------------------------------- */
/* Attract Mode Control */
/* ------------------------------------------------------------------------------- */

void Outrun::init_attract()
{
    video.enabled             = true;
    osoundint.has_booted      = true;
    oferrari.car_ctrl_active  = true;
    oferrari.car_inc_old      = car_inc_bak >> 16;
    oinitengine.car_increment = car_inc_bak;
    ostats.time_counter       = config.engine.new_attract ? 0x80 : 0x15;
    ostats.frame_counter      = frame_reset;
    attract_counter           = 0;
    attract_view              = 0;
    OAttractAI_init(&oattractai);
    game_state = cannonball_mode == MODE_TTRIAL ? GS_INIT_MUSIC : GS_ATTRACT;
}

void Outrun::tick_attract()
{
    OHud_draw_credits(&ohud);
    OHud_draw_copyright_text(&ohud);
    OHud_draw_insert_coin(&ohud);

    /* Enhanced Attract Mode (Switch Between Views) */
    if (config.engine.new_attract)
    {
        if (++attract_counter > 240)
        {
            const static uint8_t VIEWS[] = {VIEW_ORIGINAL, VIEW_ELEVATED, VIEW_INCAR};

            attract_counter = 0;
            if (++attract_view > 2)
                attract_view = 0;
            { bool snap = VIEWS[attract_view] == VIEW_INCAR;
            ORoad_set_view_mode(&oroad, VIEWS[attract_view], snap);
         }}
    }

    if (ostats.credits)
        game_state = GS_INIT_MUSIC;

    else if (decrement_timers())
    {
        car_inc_bak = oinitengine.car_increment;
        game_state = GS_INIT_BEST1;
    }
}

void Outrun::check_freeplay_start()
{
    if (config.engine.freeplay)
    {
        if (!ostats.credits && Input_has_pressed(&input, START))
        {
            ostats.credits = 1;
        }
    }
}

/* ------------------------------------------------------------------------------- */
/* Best OutRunners Initialization */
/* ------------------------------------------------------------------------------- */

void Outrun::init_best_outrunners()
{
    video.enabled = false;
    video.sprite_layer->set_x_clip(false); /* Stop clipping in wide-screen mode. */
    otiles.fill_tilemap_color(0); /* Fill Tilemap Black */
    OSprites_disable_sprites(&osprites);
    oroad.horizon_base = 0x154;
    OHiScore_setup_pal_best(&ohiscore);    /* Setup Palettes */
    OHiScore_setup_road_best(&ohiscore);
    game_state = GS_INIT_BEST2;
}

/* ------------------------------------------------------------------------------- */
/* Remap ROM addresses and select course. */
/* ------------------------------------------------------------------------------- */

void Outrun::select_course(bool jap, bool prototype)
{
    if (jap)
    {
        roms.rom0p = &roms.j_rom0;
        roms.rom1p = &roms.j_rom1;

        /* Main CPU */
        adr.tiles_def_lookup      = TILES_DEF_LOOKUP_J;
        adr.tiles_table           = TILES_TABLE_J;
        adr.sprite_master_table   = SPRITE_MASTER_TABLE_J;
        adr.sprite_type_table     = SPRITE_TYPE_TABLE_J;
        adr.sprite_def_props1     = SPRITE_DEF_PROPS1_J;
        adr.sprite_def_props2     = SPRITE_DEF_PROPS2_J;
        adr.sprite_cloud          = SPRITE_CLOUD_FRAMES_J;
        adr.sprite_minitree       = SPRITE_MINITREE_FRAMES_J;
        adr.sprite_grass          = SPRITE_GRASS_FRAMES_J;
        adr.sprite_sand           = SPRITE_SAND_FRAMES_J;
        adr.sprite_stone          = SPRITE_STONE_FRAMES_J;
        adr.sprite_water          = SPRITE_WATER_FRAMES_J;
        adr.sprite_ferrari_frames = SPRITE_FERRARI_FRAMES_J;
        adr.sprite_skid_frames    = SPRITE_SKID_FRAMES_J;
        adr.sprite_pass_frames    = SPRITE_PASS_FRAMES_J;
        adr.sprite_pass1_skidl    = SPRITE_PASS1_SKIDL_J;
        adr.sprite_pass1_skidr    = SPRITE_PASS1_SKIDR_J;
        adr.sprite_pass2_skidl    = SPRITE_PASS2_SKIDL_J;
        adr.sprite_pass2_skidr    = SPRITE_PASS2_SKIDR_J;
        adr.sprite_crash_spin1    = SPRITE_CRASH_SPIN1_J;
        adr.sprite_crash_spin2    = SPRITE_CRASH_SPIN2_J;
        adr.sprite_bump_data1     = SPRITE_BUMP_DATA1_J;
        adr.sprite_bump_data2     = SPRITE_BUMP_DATA2_J;
        adr.sprite_crash_man1     = SPRITE_CRASH_MAN1_J;
        adr.sprite_crash_girl1    = SPRITE_CRASH_GIRL1_J;
        adr.sprite_crash_flip     = SPRITE_CRASH_FLIP_J;
        adr.sprite_crash_flip_m1  = SPRITE_CRASH_FLIP_MAN1_J;
        adr.sprite_crash_flip_g1  = SPRITE_CRASH_FLIP_GIRL1_J;
        adr.sprite_crash_flip_m2  = SPRITE_CRASH_FLIP_MAN2_J;
        adr.sprite_crash_flip_g2  = SPRITE_CRASH_FLIP_GIRL2_J;
        adr.sprite_crash_man2     = SPRITE_CRASH_MAN2_J;
        adr.sprite_crash_girl2    = SPRITE_CRASH_GIRL2_J;
        adr.smoke_data            = SMOKE_DATA_J;
        adr.spray_data            = SPRAY_DATA_J;
        adr.anim_ferrari_frames   = ANIM_FERRARI_FRAMES_J;
        adr.anim_endseq_obj1      = ANIM_ENDSEQ_OBJ1_J;
        adr.anim_endseq_obj2      = ANIM_ENDSEQ_OBJ2_J;
        adr.anim_endseq_obj3      = ANIM_ENDSEQ_OBJ3_J;
        adr.anim_endseq_obj4      = ANIM_ENDSEQ_OBJ4_J;
        adr.anim_endseq_obj5      = ANIM_ENDSEQ_OBJ5_J;
        adr.anim_endseq_obj6      = ANIM_ENDSEQ_OBJ6_J;
        adr.anim_endseq_obj7      = ANIM_ENDSEQ_OBJ7_J;
        adr.anim_endseq_obj8      = ANIM_ENDSEQ_OBJ8_J;
        adr.anim_endseq_objA      = ANIM_ENDSEQ_OBJA_J;
        adr.anim_endseq_objB      = ANIM_ENDSEQ_OBJB_J;
        adr.anim_end_table        = ANIM_END_TABLE_J;
        adr.shadow_data           = SPRITE_SHADOW_DATA_J;
        adr.shadow_frames         = SPRITE_SHDW_FRAMES_J;
        adr.sprite_shadow_small   = SPRITE_SHDW_SMALL_J;
        adr.sprite_logo_bg        = SPRITE_LOGO_BG_J;
        adr.sprite_logo_car       = SPRITE_LOGO_CAR_J;
        adr.sprite_logo_bird1     = SPRITE_LOGO_BIRD1_J;
        adr.sprite_logo_bird2     = SPRITE_LOGO_BIRD2_J;
        adr.sprite_logo_base      = SPRITE_LOGO_BASE_J;
        adr.sprite_logo_text      = SPRITE_LOGO_TEXT_J;
        adr.sprite_logo_palm1     = SPRITE_LOGO_PALM1_J;
        adr.sprite_logo_palm2     = SPRITE_LOGO_PALM2_J;
        adr.sprite_logo_palm3     = SPRITE_LOGO_PALM3_J;
        adr.sprite_fm_left        = SPRITE_FM_LEFT_J;
        adr.sprite_fm_centre      = SPRITE_FM_CENTRE_J;
        adr.sprite_fm_right       = SPRITE_FM_RIGHT_J;
        adr.sprite_dial_left      = SPRITE_DIAL_LEFT_J;
        adr.sprite_dial_centre    = SPRITE_DIAL_CENTRE_J;
        adr.sprite_dial_right     = SPRITE_DIAL_RIGHT_J;
        adr.sprite_eq             = SPRITE_EQ_J;
        adr.sprite_radio          = SPRITE_RADIO_J;
        adr.sprite_hand_left      = SPRITE_HAND_LEFT_J;
        adr.sprite_hand_centre    = SPRITE_HAND_CENTRE_J;
        adr.sprite_hand_right     = SPRITE_HAND_RIGHT_J;
        adr.sprite_coursemap_top  = SPRITE_COURSEMAP_TOP_J;
        adr.sprite_coursemap_bot  = SPRITE_COURSEMAP_BOT_J;
        adr.sprite_coursemap_end  = SPRITE_COURSEMAP_END_J;
        adr.sprite_minicar_right  = SPRITE_MINICAR_RIGHT_J;
        adr.sprite_minicar_up     = SPRITE_MINICAR_UP_J;
        adr.sprite_minicar_down   = SPRITE_MINICAR_DOWN_J;
        adr.anim_seq_flag         = ANIM_SEQ_FLAG_J;
        adr.anim_ferrari_curr     = ANIM_FERRARI_CURR_J;
        adr.anim_ferrari_next     = ANIM_FERRARI_NEXT_J;
        adr.anim_pass1_curr       = ANIM_PASS1_CURR_J;
        adr.anim_pass1_next       = ANIM_PASS1_NEXT_J;
        adr.anim_pass2_curr       = ANIM_PASS2_CURR_J;
        adr.anim_pass2_next       = ANIM_PASS2_NEXT_J;
        adr.traffic_props         = TRAFFIC_PROPS_J;
        adr.traffic_data          = TRAFFIC_DATA_J;
        adr.sprite_porsche        = SPRITE_PORSCHE_J;
        adr.sprite_coursemap      = SPRITE_COURSEMAP_J;
        adr.road_seg_table        = ROAD_SEG_TABLE_J;
        adr.road_seg_end          = ROAD_SEG_TABLE_END_J;
        adr.road_seg_split        = ROAD_SEG_TABLE_SPLIT_J;

        /* Sub CPU */
        adr.road_height_lookup    = ROAD_HEIGHT_LOOKUP_J;
    }
    else
    {
        roms.rom0p = &roms.rom0;
        roms.rom1p = &roms.rom1;

        /* Main CPU */
        adr.tiles_def_lookup      = TILES_DEF_LOOKUP;
        adr.tiles_table           = TILES_TABLE;
        adr.sprite_master_table   = SPRITE_MASTER_TABLE;
        adr.sprite_type_table     = SPRITE_TYPE_TABLE;
        adr.sprite_def_props1     = SPRITE_DEF_PROPS1;
        adr.sprite_def_props2     = SPRITE_DEF_PROPS2;
        adr.sprite_cloud          = SPRITE_CLOUD_FRAMES;
        adr.sprite_minitree       = SPRITE_MINITREE_FRAMES;
        adr.sprite_grass          = SPRITE_GRASS_FRAMES;
        adr.sprite_sand           = SPRITE_SAND_FRAMES;
        adr.sprite_stone          = SPRITE_STONE_FRAMES;
        adr.sprite_water          = SPRITE_WATER_FRAMES;
        adr.sprite_ferrari_frames = SPRITE_FERRARI_FRAMES;
        adr.sprite_skid_frames    = SPRITE_SKID_FRAMES;
        adr.sprite_pass_frames    = SPRITE_PASS_FRAMES;
        adr.sprite_pass1_skidl    = SPRITE_PASS1_SKIDL;
        adr.sprite_pass1_skidr    = SPRITE_PASS1_SKIDR;
        adr.sprite_pass2_skidl    = SPRITE_PASS2_SKIDL;
        adr.sprite_pass2_skidr    = SPRITE_PASS2_SKIDR;
        adr.sprite_crash_spin1    = SPRITE_CRASH_SPIN1;
        adr.sprite_crash_spin2    = SPRITE_CRASH_SPIN2;
        adr.sprite_bump_data1     = SPRITE_BUMP_DATA1;
        adr.sprite_bump_data2     = SPRITE_BUMP_DATA2;
        adr.sprite_crash_man1     = SPRITE_CRASH_MAN1;
        adr.sprite_crash_girl1    = SPRITE_CRASH_GIRL1;
        adr.sprite_crash_flip     = SPRITE_CRASH_FLIP;
        adr.sprite_crash_flip_m1  = SPRITE_CRASH_FLIP_MAN1;
        adr.sprite_crash_flip_g1  = SPRITE_CRASH_FLIP_GIRL1;
        adr.sprite_crash_flip_m2  = SPRITE_CRASH_FLIP_MAN2;
        adr.sprite_crash_flip_g2  = SPRITE_CRASH_FLIP_GIRL2;
        adr.sprite_crash_man2     = SPRITE_CRASH_MAN2;
        adr.sprite_crash_girl2    = SPRITE_CRASH_GIRL2;
        adr.smoke_data            = SMOKE_DATA;
        adr.spray_data            = SPRAY_DATA;
        adr.shadow_data           = SPRITE_SHADOW_DATA;
        adr.shadow_frames         = SPRITE_SHDW_FRAMES;
        adr.sprite_shadow_small   = SPRITE_SHDW_SMALL;
        adr.sprite_logo_bg        = SPRITE_LOGO_BG;
        adr.sprite_logo_car       = SPRITE_LOGO_CAR;
        adr.sprite_logo_bird1     = SPRITE_LOGO_BIRD1;
        adr.sprite_logo_bird2     = SPRITE_LOGO_BIRD2;
        adr.sprite_logo_base      = SPRITE_LOGO_BASE;
        adr.sprite_logo_text      = SPRITE_LOGO_TEXT;
        adr.sprite_logo_palm1     = SPRITE_LOGO_PALM1;
        adr.sprite_logo_palm2     = SPRITE_LOGO_PALM2;
        adr.sprite_logo_palm3     = SPRITE_LOGO_PALM3;
        adr.sprite_fm_left        = SPRITE_FM_LEFT;
        adr.sprite_fm_centre      = SPRITE_FM_CENTRE;
        adr.sprite_fm_right       = SPRITE_FM_RIGHT;
        adr.sprite_dial_left      = SPRITE_DIAL_LEFT;
        adr.sprite_dial_centre    = SPRITE_DIAL_CENTRE;
        adr.sprite_dial_right     = SPRITE_DIAL_RIGHT;
        adr.sprite_eq             = SPRITE_EQ;
        adr.sprite_radio          = SPRITE_RADIO;
        adr.sprite_hand_left      = SPRITE_HAND_LEFT;
        adr.sprite_hand_centre    = SPRITE_HAND_CENTRE;
        adr.sprite_hand_right     = SPRITE_HAND_RIGHT;
        adr.sprite_coursemap_top  = SPRITE_COURSEMAP_TOP;
        adr.sprite_coursemap_bot  = SPRITE_COURSEMAP_BOT;
        adr.sprite_coursemap_end  = SPRITE_COURSEMAP_END;
        adr.sprite_minicar_right  = SPRITE_MINICAR_RIGHT;
        adr.sprite_minicar_up     = SPRITE_MINICAR_UP;
        adr.sprite_minicar_down   = SPRITE_MINICAR_DOWN;
        adr.anim_seq_flag         = ANIM_SEQ_FLAG;
        adr.anim_ferrari_curr     = ANIM_FERRARI_CURR;
        adr.anim_ferrari_next     = ANIM_FERRARI_NEXT;
        adr.anim_pass1_curr       = ANIM_PASS1_CURR;
        adr.anim_pass1_next       = ANIM_PASS1_NEXT;
        adr.anim_pass2_curr       = ANIM_PASS2_CURR;
        adr.anim_pass2_next       = ANIM_PASS2_NEXT;
        adr.anim_ferrari_frames   = ANIM_FERRARI_FRAMES;
        adr.anim_endseq_obj1      = ANIM_ENDSEQ_OBJ1;
        adr.anim_endseq_obj2      = ANIM_ENDSEQ_OBJ2;
        adr.anim_endseq_obj3      = ANIM_ENDSEQ_OBJ3;
        adr.anim_endseq_obj4      = ANIM_ENDSEQ_OBJ4;
        adr.anim_endseq_obj5      = ANIM_ENDSEQ_OBJ5;
        adr.anim_endseq_obj6      = ANIM_ENDSEQ_OBJ6;
        adr.anim_endseq_obj7      = ANIM_ENDSEQ_OBJ7;
        adr.anim_endseq_obj8      = ANIM_ENDSEQ_OBJ8;
        adr.anim_endseq_objA      = ANIM_ENDSEQ_OBJA;
        adr.anim_endseq_objB      = ANIM_ENDSEQ_OBJB;
        adr.anim_end_table        = ANIM_END_TABLE;
        adr.shadow_data           = SPRITE_SHADOW_DATA;
        adr.shadow_frames         = SPRITE_SHDW_FRAMES;
        adr.sprite_shadow_small   = SPRITE_SHDW_SMALL;
        adr.traffic_props         = TRAFFIC_PROPS;
        adr.traffic_data          = TRAFFIC_DATA;
        adr.sprite_porsche        = SPRITE_PORSCHE;
        adr.sprite_coursemap      = SPRITE_COURSEMAP;
        adr.road_seg_table        = ROAD_SEG_TABLE;
        adr.road_seg_end          = ROAD_SEG_TABLE_END;
        adr.road_seg_split        = ROAD_SEG_TABLE_SPLIT;

        /* Sub CPU */
        adr.road_height_lookup    = ROAD_HEIGHT_LOOKUP;
    }

    trackloader.init(jap);

    /* Use Prototype Coconut Beach Track */
    trackloader.stage_data[0] = prototype ? 0x3A : 0x3C;
}
