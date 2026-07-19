/***************************************************************************
    Binary File Loader.

    Handles loading an individual binary file to memory.
    Supports reading bytes, words and longs from this area of memory.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include <stdint.h>

class RomLoader
{
public:
    enum {NORMAL = 1, INTERLEAVE2 = 2, INTERLEAVE4 = 4};

    uint8_t* rom;
    uint32_t length;
    bool loaded;

    RomLoader();
    ~RomLoader();

    void init(uint32_t length);

    // Retains the upstream call interface used by Roms. The Libretro
    // implementation identifies ROMs by CRC32 first, then falls back to the
    // canonical filename for compatibility with existing frontend setups.
    int (RomLoader::*load)(const char*, const int, const int,
                           const uint32_t, const uint8_t, const bool);

    int load_auto(const char* filename, const int offset, const int length,
                  const uint32_t expected_crc,
                  const uint8_t mode = NORMAL,
                  const bool verbose = true);

    int load_rom(const char* filename, const int offset, const int length,
                 const uint32_t expected_crc,
                 const uint8_t mode = NORMAL,
                 const bool verbose = true);

    int load_crc32(const char* debug, const int offset, const int length,
                   const uint32_t expected_crc,
                   const uint8_t mode = NORMAL,
                   const bool verbose = true);

    int load_binary(const char* filename);
    void unload(void);

    inline uint32_t read32(uint32_t* addr)
    {
        uint32_t data = (rom[*addr] << 24) | (rom[*addr + 1] << 16) |
                        (rom[*addr + 2] << 8) | rom[*addr + 3];
        *addr += 4;
        return data;
    }

    inline uint16_t read16(uint32_t* addr)
    {
        uint16_t data = (rom[*addr] << 8) | rom[*addr + 1];
        *addr += 2;
        return data;
    }

    inline uint8_t read8(uint32_t* addr)
    {
        return rom[(*addr)++];
    }

    inline uint32_t read32(uint32_t addr)
    {
        return (rom[addr] << 24) | (rom[addr + 1] << 16) |
               (rom[addr + 2] << 8) | rom[addr + 3];
    }

    inline uint16_t read16(uint32_t addr)
    {
        return (rom[addr] << 8) | rom[addr + 1];
    }

    inline uint8_t read8(uint32_t addr)
    {
        return rom[addr];
    }

    inline uint16_t read16(uint16_t* addr)
    {
        uint16_t data = (rom[*addr + 1] << 8) | rom[*addr];
        *addr += 2;
        return data;
    }

    inline uint8_t read8(uint16_t* addr)
    {
        return rom[(*addr)++];
    }

    inline uint16_t read16(uint16_t addr)
    {
        return (rom[addr + 1] << 8) | rom[addr];
    }

    inline uint8_t read8(uint16_t addr)
    {
        return rom[addr];
    }

private:
    int create_map();
};
