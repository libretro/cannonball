/***************************************************************************
    Video Rendering. 
    
    - Renders the System 16 Video Layers
    - Handles Reads and Writes to these layers from the main game code
    - Interfaces with platform specific rendering code

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>
#include "globals.hpp"
#include "roms.hpp"
#include "hwvideo/hwtiles.hpp"
#include "hwvideo/hwsprites.hpp"
#include "hwvideo/hwroad.hpp"

const static float SHADOW_ORIGINAL = 0.63f; /* Hardware Intensity (63%) */
const static float SHADOW_MAME = 0.78f;     /* Mame Intensity (78%) */

class hwsprites;
class RenderBase;
struct video_settings_t;

struct Video
{
    hwsprites* sprite_layer;
    hwtiles* tile_layer;
    uint16_t *pixels;
    bool enabled;
    uint32_t rgb[S16_PALETTE_ENTRIES * 3];
    uint32_t shadow_multi;
    uint8_t palette[S16_PALETTE_ENTRIES * 2];
};

extern Video video;

void Video_ctor(Video* self);

void Video_dtor(Video* self);

int Video_init(Video* self, Roms* roms, video_settings_t* settings);

void Video_disable(Video* self);

int Video_set_video_mode(Video* self, video_settings_t* settings);

void Video_set_shadow_intensity(Video* self, float);

void Video_prepare_frame(Video* self);

void Video_render_frame(Video* self);

void Video_clear_text_ram(Video* self);

void Video_write_text8(Video* self, uint32_t, const uint8_t);

void Video_write_text16(Video* self, uint32_t*, const uint16_t);

void Video_write_text16(Video* self, uint32_t, const uint16_t);

void Video_write_text32(Video* self, uint32_t*, const uint32_t);

void Video_write_text32(Video* self, uint32_t, const uint32_t);

uint8_t Video_read_text8(Video* self, uint32_t);

void Video_clear_tile_ram(Video* self);

void Video_write_tile8(Video* self, uint32_t, const uint8_t);

void Video_write_tile16(Video* self, uint32_t*, const uint16_t);

void Video_write_tile16(Video* self, uint32_t, const uint16_t);

void Video_write_tile32(Video* self, uint32_t*, const uint32_t);

void Video_write_tile32(Video* self, uint32_t, const uint32_t);

uint8_t Video_read_tile8(Video* self, uint32_t);

void Video_write_sprite16(Video* self, uint32_t*, const uint16_t);

void Video_write_pal8(Video* self, uint32_t*, const uint8_t);

void Video_write_pal16(Video* self, uint32_t*, const uint16_t);

void Video_write_pal32(Video* self, uint32_t*, const uint32_t);

void Video_write_pal32(Video* self, uint32_t, const uint32_t);

uint8_t Video_read_pal8(Video* self, uint32_t);

uint16_t Video_read_pal16(Video* self, uint32_t*);

uint16_t Video_read_pal16(Video* self, uint32_t);

uint32_t Video_read_pal32(Video* self, uint32_t*);

