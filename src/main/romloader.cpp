/***************************************************************************
    Binary File Loader.

    Handles loading an individual binary file to memory.
    Supports reading bytes, words and longs from this area of memory.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <cstddef>
#include <string>
#include <map>
#include <vector>

#include <encodings/crc32.h>
#include <libretro.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "romloader.hpp"

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
    uint8_t* output = static_cast<uint8_t*>(data);

    while (total < size)
    {
        const int64_t count = filestream_read(
            file,
            output + total,
            static_cast<int64_t>(size - total));

        if (count <= 0)
            return false;

        total += static_cast<size_t>(count);
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
            static_cast<int64_t>(sizeof(buffer)));

        if (count < 0)
        {
            filestream_close(file);
            return false;
        }

        if (count == 0)
            break;

        crc = encoding_crc32(crc, buffer, static_cast<size_t>(count));
    }

    filestream_close(file);
    checksum = crc;
    return true;
}

RomLoader::RomLoader()
    : rom(NULL), length(0), loaded(false)
{
    load = &RomLoader::load_auto;
}

RomLoader::~RomLoader()
{
    unload();
}

void RomLoader::init(const uint32_t new_length)
{
    unload();

    length = new_length;
    rom = new uint8_t[length];
    loaded = false;
    load = &RomLoader::load_auto;
}

void RomLoader::unload(void)
{
    delete[] rom;
    rom = NULL;
    length = 0;
    loaded = false;
}

int RomLoader::load_auto(const char* filename,
                         const int offset,
                         const int file_length,
                         const uint32_t expected_crc,
                         const uint8_t interleave,
                         const bool verbose)
{
    // Prefer content identification, matching the modern upstream loader.
    // The canonical filename remains a compatibility fallback for frontends
    // where directory enumeration is unavailable or for legacy ROM sets.
    if (load_crc32(filename, offset, file_length, expected_crc,
                   interleave, false) == 0)
    {
        return 0;
    }

    if (load_rom(filename, offset, file_length, expected_crc,
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
               static_cast<unsigned>(expected_crc));

    loaded = false;
    return 1;
}

int RomLoader::load_rom(const char* filename,
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
        loaded = false;
        return 1;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(file_length));
    const bool read_ok = read_exact(source, &buffer[0], buffer.size());
    filestream_close(source);

    if (!read_ok)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "ROM has an unexpected size: %s\n",
                   path.c_str());

        loaded = false;
        return 1;
    }

    const uint32_t checksum = encoding_crc32(0, &buffer[0], buffer.size());

    // Match the existing Libretro core: report checksum mismatches, but keep
    // loading the named ROM instead of rejecting a set that previously worked.
    if (expected_crc != checksum && verbose && log_cb)
    {
        log_cb(RETRO_LOG_WARN,
               "%s has incorrect checksum. Expected: 0x%08x, Found: 0x%08x\n",
               filename,
               static_cast<unsigned>(expected_crc),
               static_cast<unsigned>(checksum));
    }

    for (int i = 0; i < file_length; i++)
        rom[(i * interleave) + offset] = buffer[static_cast<size_t>(i)];

    loaded = true;
    return 0;
}

int RomLoader::create_map()
{
    const std::string path = rom_path;

    crc_map.clear();
    mapped_path = path;

    // Mark the map as initialized even if directory enumeration fails, so a
    // frontend without directory VFS support does not trigger a full rescan
    // for every ROM before falling back to canonical filenames.
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

int RomLoader::load_crc32(const char* debug,
                          const int offset,
                          const int file_length,
                          const uint32_t expected_crc,
                          const uint8_t interleave,
                          const bool verbose)
{
    if ((!map_created || mapped_path != rom_path) && create_map() != 0)
    {
        loaded = false;
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
                   static_cast<unsigned>(expected_crc));

        loaded = false;
        return 1;
    }

    RFILE* source = filestream_open(
        match->second.c_str(),
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        loaded = false;
        return 1;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(file_length));
    const bool read_ok = read_exact(source, &buffer[0], buffer.size());
    filestream_close(source);

    if (!read_ok)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "ROM has an unexpected size: %s\n",
                   match->second.c_str());

        loaded = false;
        return 1;
    }

    for (int i = 0; i < file_length; i++)
        rom[(i * interleave) + offset] = buffer[static_cast<size_t>(i)];

    if (verbose && log_cb)
        log_cb(RETRO_LOG_INFO,
               "Loaded renamed ROM by CRC32: %s\n",
               match->second.c_str());

    loaded = true;
    return 0;
}

int RomLoader::load_binary(const char* filename)
{
    RFILE* source = filestream_open(
        filename,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Cannot open file: %s\n", filename);

        loaded = false;
        return 1;
    }

    const int64_t file_length = filestream_get_size(source);
    if (file_length <= 0)
    {
        filestream_close(source);
        loaded = false;
        return 1;
    }

    unload();

    length = static_cast<uint32_t>(file_length);
    rom = new uint8_t[length];

    const bool read_ok = read_exact(source, rom, length);
    filestream_close(source);

    if (!read_ok)
    {
        unload();
        return 1;
    }

    loaded = true;
    return 0;
}
