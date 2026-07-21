/***************************************************************************
    Sega 8-Bit PCM Driver
    
    This driver is based upon the MAME source code, with some minor 
    modifications to integrate it into the Cannonball framework.

    Note, that I've altered this driver to output at a fixed 44,100Hz.
    This is to avoid the need for downsampling.  
    
    See http://mamedev.org/source/docs/license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>
#include "romloader.hpp"
#include "hwaudio/soundchip.hpp"

static const uint32_t BANK_256    = (11);

static const uint32_t BANK_512    = (12);

static const uint32_t BANK_12M    = (13);

static const uint32_t BANK_MASK7  = (0x70 << 16);

static const uint32_t BANK_MASKF  = (0xf0 << 16);

static const uint32_t BANK_MASKF8 = (0xf8 << 16);

struct SegaPCM
{
    SoundChip sc;
    uint8_t* ram;
    uint8_t* low;
    uint8_t* pcm_rom;
    int32_t max_addr;
    int32_t bankshift;
    int32_t bankmask;
    int32_t rgnmask;
    double downsample;
};

void SegaPCM_ctor(SegaPCM* self, uint32_t clock, RomLoader* rom, uint8_t* ram, int32_t bank);

void SegaPCM_dtor(SegaPCM* self);

void SegaPCM_init(SegaPCM* self, int32_t rate, int32_t fps);

void SegaPCM_stream_update(SegaPCM* self);
