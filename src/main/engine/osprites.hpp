/***************************************************************************
    Sprite Handling Routines.
    
    - Initializing Sprites from level data.
    - Mapping palettes to sprites.
    - Ordering sprites by priority.
    - Adding shadows to sprites where appropriate.
    - Clipping sprites based on priority in relation to road hardware.
    - Conversion from internal format to output format required by hardware.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include "oentry.hpp"
#include "osprite.hpp"
#include "outrun.hpp"
#include "engine/data/sprite_pals.hpp"

enum {
		HFLIP = 0x1,			/* Bit 0: Horizontally flip sprite */
		WIDE_ROAD = 0x4,		/* Bit 2: Set if road_width >= 0x118,  */
        TRAFFIC_SPRITE = 0x8,   /* Bit 3: Traffic Sprite - Set for traffic */
        SHADOW = 0x10,          /* Bit 4: Sprite has shadows */
		DRAW_SPRITE = 0x20,	    /* Bit 5: Toggle sprite visibility */
        TRAFFIC_RHS = 0x40,	    /* Bit 6: Traffic Sprites - Set to spawn on RHS */
		ENABLE = 0x80	        /* Bit 7: Toggle sprite visibility */
	};

enum { SPRITE_ENTRIES = 0x62 };

enum { JUMP_ENTRIES_TOTAL = SPRITE_ENTRIES + 24 };

enum { PAL_ENTRIES = 0x100 };

static const uint8_t SPRITE_FERRARI = SPRITE_ENTRIES + 1;

static const uint8_t SPRITE_PASS1   = SPRITE_ENTRIES + 2;

static const uint8_t SPRITE_PASS2   = SPRITE_ENTRIES + 3;

static const uint8_t SPRITE_SHADOW  = SPRITE_ENTRIES + 4;

static const uint8_t SPRITE_SMOKE1  = SPRITE_ENTRIES + 5;

static const uint8_t SPRITE_SMOKE2  = SPRITE_ENTRIES + 6;

static const uint8_t SPRITE_TRAFF1  = SPRITE_ENTRIES + 7;

static const uint8_t SPRITE_TRAFF2  = SPRITE_ENTRIES + 8;

static const uint8_t SPRITE_TRAFF3  = SPRITE_ENTRIES + 9;

static const uint8_t SPRITE_TRAFF4  = SPRITE_ENTRIES + 10;

static const uint8_t SPRITE_TRAFF5  = SPRITE_ENTRIES + 11;

static const uint8_t SPRITE_TRAFF6  = SPRITE_ENTRIES + 12;

static const uint8_t SPRITE_TRAFF7  = SPRITE_ENTRIES + 13;

static const uint8_t SPRITE_TRAFF8  = SPRITE_ENTRIES + 14;

static const uint8_t SPRITE_CRASH          = SPRITE_ENTRIES + 15;

static const uint8_t SPRITE_CRASH_SHADOW   = SPRITE_ENTRIES + 16;

static const uint8_t SPRITE_CRASH_PASS1    = SPRITE_ENTRIES + 17;

static const uint8_t SPRITE_CRASH_PASS1_S  = SPRITE_ENTRIES + 18;

static const uint8_t SPRITE_CRASH_PASS2    = SPRITE_ENTRIES + 19;

static const uint8_t SPRITE_CRASH_PASS2_S  = SPRITE_ENTRIES + 20;

static const uint8_t SPRITE_FLAG  = SPRITE_ENTRIES + 21;

static const uint32_t SPRITE_RAM = 0x130000;

static const uint32_t PAL_SPRITES = 0x121000;

typedef struct OSprites
{
    uint8_t no_sprites;
    oentry jump_table[JUMP_ENTRIES_TOTAL];
    osprite sprite_entries[JUMP_ENTRIES_TOTAL];
    uint16_t seg_pos;
    uint8_t seg_total_sprites;
    uint16_t seg_sprite_freq;
    int16_t seg_spr_offset2;
    int16_t seg_spr_offset1;
    uint32_t seg_spr_addr;
    uint16_t sprite_scroll_speed;
    int16_t shadow_offset;
    uint16_t sprite_count;
    uint16_t spr_cnt_main;
    uint16_t spr_cnt_shadow;
    bool do_sprite_swap;
    uint8_t spr_col_pal;
    int16_t pal_copy_count;
    uint16_t pal_addresses[PAL_ENTRIES];
    uint8_t pal_lookup[PAL_LOOKUP_LENGTH];
    uint8_t sprite_order[0x2000];
    uint8_t sprite_order2[0x2000];
} OSprites;

extern OSprites osprites;

void OSprites_init(OSprites* self);

void OSprites_disable_sprites(OSprites* self);

void OSprites_tick(OSprites* self);

void OSprites_update_sprites(OSprites* self);

void OSprites_clear_palette_data(OSprites* self);

void OSprites_copy_palette_data(OSprites* self);

void OSprites_map_palette(OSprites* self, oentry* spr);

void OSprites_sprite_copy(OSprites* self);

void OSprites_blit_sprites(OSprites* self);

void OSprites_do_spr_order_shadows(OSprites* self, oentry*);

void OSprites_do_sprite(OSprites* self, oentry*);

void OSprites_set_sprite_xy(OSprites* self, oentry*, osprite*, uint16_t, uint16_t);

void OSprites_set_hrender(OSprites* self, oentry*, osprite*, uint16_t, uint16_t);

void OSprites_move_sprite(OSprites* self, oentry*, uint8_t);

