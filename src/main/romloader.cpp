/***************************************************************************
    Binary File Loader.

    Handles loading an individual binary file to memory.
    Supports reading bytes, words and longs from this area of memory.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stddef.h>
#include <string>
#include <map>
#include <vector>

#include <encodings/crc32.h>
#include <libretro.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "romloader.hpp"
#include <string.h>

static int RomLoader_create_map(RomLoader* self);

extern retro_log_printf_t log_cb;
extern char rom_path[1024];

static std::map<uint32_t, std::string> crc_map;
static bool map_created = false;
static std::string mapped_path;

static std::string join_path(const std::string& directory,
                             const std::string& filename)
{
    if (directory.empty())
        return filename;

    const char last = directory[directory.size() - 1];
    if (last == '/' || last == '\\')
        return directory + filename;

    return directory + "/" + filename;
}

static bool read_exact(RFILE* file, void* data, size_t size)
{
    size_t total = 0;
    uint8_t* output = (uint8_t*)(data);

    while (total < size)
    {
        const int64_t count = filestream_read(
            file,
            output + total,
            (int64_t)(size - total));

        if (count <= 0)
            return false;

        total += (size_t)(count);
    }

    return true;
}

static bool calculate_crc32(const std::string& path, uint32_t& checksum)
{
    RFILE* file = filestream_open(
        path.c_str(),
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!file)
        return false;

    uint32_t crc = 0;
    uint8_t buffer[64 * 1024];

    for (;;)
    {
        const int64_t count = filestream_read(
            file,
            buffer,
            (int64_t)(sizeof(buffer)));

        if (count < 0)
        {
            filestream_close(file);
            return false;
        }

        if (count == 0)
            break;

        crc = encoding_crc32(crc, buffer, (size_t)(count));
    }

    filestream_close(file);
    checksum = crc;
    return true;
}



void RomLoader_dtor(RomLoader* self)
{
    RomLoader_unload(self);
}

void RomLoader_init(RomLoader* self, const uint32_t new_length)
{
    RomLoader_unload(self);

    self->length = new_length;
    self->rom = new uint8_t[self->length];
    self->loaded = false;
}

void RomLoader_unload(RomLoader* self)
{
    delete[] self->rom;
    self->rom = NULL;
    self->length = 0;
    self->loaded = false;
}

int RomLoader_load_auto(RomLoader* self, const char* filename,
                         const int offset,
                         const int file_length,
                         const uint32_t expected_crc,
                         const uint8_t interleave,
                         const bool verbose)
{
    /* Prefer content identification, matching the modern upstream loader. */
    /* The canonical filename remains a compatibility fallback for frontends */
    /* where directory enumeration is unavailable or for legacy ROM sets. */
    if (RomLoader_load_crc32(self, filename, offset, file_length, expected_crc,
                   interleave, false) == 0)
    {
        return 0;
    }

    if (RomLoader_load_rom(self, filename, offset, file_length, expected_crc,
                 interleave, verbose) == 0)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_WARN,
                   "Loaded ROM by canonical filename fallback: %s\n",
                   filename);

        return 0;
    }

    if (verbose && log_cb)
        log_cb(RETRO_LOG_ERROR,
               "Unable to locate ROM by CRC32 or canonical name: %s "
               "(CRC32: 0x%08x)\n",
               filename,
               (unsigned)(expected_crc));

    self->loaded = false;
    return 1;
}

int RomLoader_load_rom(RomLoader* self, const char* filename,
                        const int offset,
                        const int file_length,
                        const uint32_t expected_crc,
                        const uint8_t interleave,
                        const bool verbose)
{
    const std::string path = join_path(rom_path, filename);

    RFILE* source = filestream_open(
        path.c_str(),
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        self->loaded = false;
        return 1;
    }

    std::vector<uint8_t> buffer((size_t)(file_length));
    const bool read_ok = read_exact(source, &buffer[0], buffer.size());
    filestream_close(source);

    if (!read_ok)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "ROM has an unexpected size: %s\n",
                   path.c_str());

        self->loaded = false;
        return 1;
    }

    const uint32_t checksum = encoding_crc32(0, &buffer[0], buffer.size());

    /* Match the existing Libretro core: report checksum mismatches, but keep */
    /* loading the named ROM instead of rejecting a set that previously worked. */
    if (expected_crc != checksum && verbose && log_cb)
    {
        log_cb(RETRO_LOG_WARN,
               "%s has incorrect checksum. Expected: 0x%08x, Found: 0x%08x\n",
               filename,
               (unsigned)(expected_crc),
               (unsigned)(checksum));
    }

    { int i; for (i = 0; i < file_length; i++)
        self->rom[(i * interleave) + offset] = buffer[(size_t)(i)]; }

    self->loaded = true;
    return 0;
}static 

int RomLoader_create_map(RomLoader* self)
{
    const std::string path = rom_path;

    crc_map.clear();
    mapped_path = path;

    /* Mark the map as initialized even if directory enumeration fails, so a */
    /* frontend without directory VFS support does not trigger a full rescan */
    /* for every ROM before falling back to canonical filenames. */
    map_created = true;

    RDIR* directory = retro_opendir(path.c_str());
    if (!directory)
    {
        if (log_cb)
            log_cb(RETRO_LOG_WARN,
                   "Could not open ROM directory for CRC32 fallback: %s\n",
                   path.c_str());

        return 1;
    }

    while (retro_readdir(directory))
    {
        const char* name = retro_dirent_get_name(directory);

        if (!name || !*name ||
            (name[0] == '.' &&
             (name[1] == '\0' ||
              (name[1] == '.' && name[2] == '\0'))) ||
            retro_dirent_is_dir(directory, NULL))
        {
            continue;
        }

        const std::string file = join_path(path, name);
        uint32_t checksum = 0;

        if (calculate_crc32(file, checksum))
            crc_map.insert(std::make_pair(checksum, file));
    }

    retro_closedir(directory);

    if (crc_map.empty())
        return 1;

    return 0;
}

int RomLoader_load_crc32(RomLoader* self, const char* debug,
                          const int offset,
                          const int file_length,
                          const uint32_t expected_crc,
                          const uint8_t interleave,
                          const bool verbose)
{
    if ((!map_created || mapped_path != rom_path) && RomLoader_create_map(self) != 0)
    {
        self->loaded = false;
        return 1;
    }

    const std::map<uint32_t, std::string>::const_iterator match =
        crc_map.find(expected_crc);

    if (match == crc_map.end())
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "Unable to locate ROM. Expected name: %s, CRC32: 0x%08x\n",
                   debug,
                   (unsigned)(expected_crc));

        self->loaded = false;
        return 1;
    }

    RFILE* source = filestream_open(
        match->second.c_str(),
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        self->loaded = false;
        return 1;
    }

    std::vector<uint8_t> buffer((size_t)(file_length));
    const bool read_ok = read_exact(source, &buffer[0], buffer.size());
    filestream_close(source);

    if (!read_ok)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "ROM has an unexpected size: %s\n",
                   match->second.c_str());

        self->loaded = false;
        return 1;
    }

    { int i; for (i = 0; i < file_length; i++)
        self->rom[(i * interleave) + offset] = buffer[(size_t)(i)]; }

    if (verbose && log_cb)
        log_cb(RETRO_LOG_INFO,
               "Loaded renamed ROM by CRC32: %s\n",
               match->second.c_str());

    self->loaded = true;
    return 0;
}

int RomLoader_load_binary(RomLoader* self, const char* filename)
{
    RFILE* source = filestream_open(
        filename,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Cannot open file: %s\n", filename);

        self->loaded = false;
        return 1;
    }

    const int64_t file_length = filestream_get_size(source);
    if (file_length <= 0)
    {
        filestream_close(source);
        self->loaded = false;
        return 1;
    }

    RomLoader_unload(self);

    self->length = (uint32_t)(file_length);
    self->rom = new uint8_t[self->length];

    const bool read_ok = read_exact(source, self->rom, self->length);
    filestream_close(source);

    if (!read_ok)
    {
        RomLoader_unload(self);
        return 1;
    }

    self->loaded = true;
    return 0;
}

/* Load from an embedded memory buffer (no external file needed). */
int RomLoader_load_mem(RomLoader* self, const uint8_t* data, uint32_t len)
{
    if (!data || len == 0)
    {
        self->loaded = false;
        return 1;
    }

    RomLoader_unload(self);

    self->length = len;
    self->rom    = new uint8_t[self->length];
    memcpy(self->rom, data, self->length);

    self->loaded = true;
    return 0;
}
