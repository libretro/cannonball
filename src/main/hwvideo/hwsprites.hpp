#pragma once

#include <stdint.h>

class video;

enum { SPRITE_RAM_SIZE = 128 * 8 };

enum { SPRITES_LENGTH = 0x100000 >> 2 };

static const uint16_t COLOR_BASE = 0x800;

struct hwsprites
{
    uint16_t x1, x2;
    uint32_t sprites[SPRITES_LENGTH];
    uint16_t ram[SPRITE_RAM_SIZE];
    uint16_t ramBuff[SPRITE_RAM_SIZE];
};

void hwsprites_init(hwsprites* self, const uint8_t*);

void hwsprites_reset(hwsprites* self);

void hwsprites_set_x_clip(hwsprites* self, bool);

void hwsprites_swap(hwsprites* self);

uint8_t hwsprites_read(hwsprites* self, const uint16_t adr);

void hwsprites_write(hwsprites* self, const uint16_t adr, const uint16_t data);

void hwsprites_render(hwsprites* self, const uint8_t);

