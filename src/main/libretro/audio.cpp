/***************************************************************************
    Libretro Audio Code.
    
    It takes the output from the PCM and YM chips, mixes them and then
    outputs appropriately.
    
    In order to achieve seamless audio, when audio is enabled the framerate
    is adjusted to essentially sync the video to the audio output.
    
    This is based upon code from the Atari800 emulator project.
    Copyright (c) 1998-2008 Atari800 development team
***************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <streams/file_stream.h>
#include "audio.hpp"
#include "frontend/config.hpp" /* fps */
#include "engine/audio/osoundint.hpp"
#include <libretro.h>

static void Audio_clear_buffers(Audio* self);
static void Audio_pause_audio(Audio* self);
static void Audio_resume_audio(Audio* self);

extern retro_log_printf_t                 log_cb;

#ifdef __PS3__
#define free std::free
#endif

extern retro_audio_sample_batch_t  audio_batch_cb;

/* Note that these variables are accessed by two separate threads. */
uint8_t* dsp_buffer;
static int dsp_buffer_bytes;
static int dsp_write_pos;
static int dsp_read_pos;
static int bytes_per_sample; /* Number of bytes per sample entry (usually 4 bytes if stereo and 16-bit sound) */

void Audio_ctor(Audio* self)
{
    self->custom_wav_volume = 200;
}


void Audio_init(Audio* self)
{
    if (config.sound.enabled)
        Audio_start_audio(self);
}

void Audio_start_audio(Audio* self)
{
    if (!self->sound_enabled)
    {
        bytes_per_sample = CHANNELS * (BITS / 8);

        /* Start Audio */
        self->sound_enabled = true;

        /* how many fragments in the dsp buffer */
        const int DSP_BUFFER_FRAGS = 5;
        { int specified_delay_samps = (FREQ * SND_DELAY) / 1000;
        int dsp_buffer_samps = SAMPLES * DSP_BUFFER_FRAGS + specified_delay_samps;
        dsp_buffer_bytes = CHANNELS * dsp_buffer_samps * (BITS / 8);
        dsp_buffer = new uint8_t[dsp_buffer_bytes];

        /* Create Buffer For Mixing */
        uint16_t buffer_size = (FREQ / config.fps) * CHANNELS;
        self->mix_buffer = new uint16_t[buffer_size];

        Audio_clear_buffers(self);
        Audio_clear_wav(self);
     }}
}static 

void Audio_clear_buffers(Audio* self)
{
    dsp_read_pos  = 0;
    { int specified_delay_samps = (FREQ * SND_DELAY) / 1000;
    dsp_write_pos = (specified_delay_samps+SAMPLES) * bytes_per_sample;

    { int i; for (i = 0; i < dsp_buffer_bytes; i++)
        dsp_buffer[i] = 0; }

    { uint16_t buffer_size = (FREQ / config.fps) * CHANNELS;
    { int i; for (i = 0; i < buffer_size; i++)
        self->mix_buffer[i] = 0; }
 } }}

void Audio_stop_audio(Audio* self)
{
    if (self->sound_enabled)
    {
        self->sound_enabled = false;

        delete[] dsp_buffer;
        delete[] self->mix_buffer;
    }
}static 

void Audio_pause_audio(Audio* self)
{
}static 

void Audio_resume_audio(Audio* self)
{
    if (self->sound_enabled)
        Audio_clear_buffers(self);
}

/* Called every frame to update the audio */
void Audio_tick(Audio* self)
{
    static const unsigned SND_RATE = 44100;

    { int bytes_written = 0;
    int newpos = 0;

    if (!self->sound_enabled)
        return;

    osoundint.pcm->stream_update();
    osoundint.ym->stream_update();

    { int16_t* pcm_buffer = osoundint.pcm->sc.get_buffer();
    int16_t* ym_buffer  = osoundint.ym->sc.get_buffer();
    int16_t* wav_buffer = self->wavfile.data;

    const int samples_written =
        osoundint.pcm->sc.buffer_size;

    { int i; for (i = 0; i < samples_written; i++)
    {
        const int32_t wav_sample =
            (int32_t)(
                ((int64_t)wav_buffer[self->wavfile.pos] *
                 (int64_t)self->custom_wav_volume) /
                100);

        int32_t mix_data =
            wav_sample +
            (int32_t)pcm_buffer[i] +
            (int32_t)ym_buffer[i];

        if (mix_data > 32767)
            mix_data = 32767;
        else if (mix_data < -32768)
            mix_data = -32768;

        self->mix_buffer[i] =
            (uint16_t)(int16_t)mix_data;

        if (++self->wavfile.pos >= self->wavfile.length)
            self->wavfile.pos = 0;
    } }

    uint8_t* mbuf8 =
        (uint8_t*)self->mix_buffer;

    bytes_written =
        BITS == 8
            ? samples_written
            : samples_written * 2;

    newpos =
        dsp_write_pos + bytes_written;

    if (newpos / dsp_buffer_bytes ==
        dsp_write_pos / dsp_buffer_bytes)
    {
        memcpy(
            dsp_buffer +
                (dsp_write_pos % dsp_buffer_bytes),
            mbuf8,
            bytes_written);
    }
    else
    {
        const int first_part_size =
            dsp_buffer_bytes -
            (dsp_write_pos % dsp_buffer_bytes);

        memcpy(
            dsp_buffer +
                (dsp_write_pos % dsp_buffer_bytes),
            mbuf8,
            first_part_size);

        memcpy(
            dsp_buffer,
            mbuf8 + first_part_size,
            bytes_written - first_part_size);
    }

    dsp_write_pos = newpos;
    dsp_read_pos += bytes_written;

    while (dsp_read_pos > dsp_buffer_bytes)
    {
        dsp_write_pos -= dsp_buffer_bytes;
        dsp_read_pos  -= dsp_buffer_bytes;
    }

    { const int audio_frames =
        SND_RATE / config.fps;

    audio_batch_cb(
        (const int16_t*)self->mix_buffer,
        audio_frames);
 } } }}

/* Empty Wav Buffer */
static int16_t EMPTY_BUFFER[] = {0, 0, 0, 0};

static uint16_t wav_read_le16(const uint8_t* data)
{
    return
        (uint16_t)data[0] |
        ((uint16_t)data[1] << 8);
}

static uint32_t wav_read_le32(const uint8_t* data)
{
    return
        (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static int16_t wav_read_pcm_sample(
    const uint8_t* data,
    uint16_t bits_per_sample)
{
    switch (bits_per_sample)
    {
        case 8:
            return
                (int16_t)(
                    ((int32_t)data[0] - 128) *
                    256);

        case 16:
            return
                (int16_t)wav_read_le16(data);

        case 24:
        {
            int32_t value =
                (int32_t)data[0] |
                ((int32_t)data[1] << 8) |
                ((int32_t)data[2] << 16);

            if (value & 0x00800000)
                value |= (int32_t)0xff000000;

            return
                (int16_t)(value / 256);
        }

        case 32:
        {
            const int32_t value =
                (int32_t)wav_read_le32(data);

            return
                (int16_t)(value / 65536);
        }
    }

    return 0;
}

void Audio_load_wav(Audio* self, const char* filename)
{
    void* file_buffer = NULL;
    int64_t file_length = 0;

    if (!self->sound_enabled)
        return;

    Audio_clear_wav(self);

    if (!filestream_read_file(
            filename,
            &file_buffer,
            &file_length) ||
        !file_buffer ||
        file_length < 12)
    {
        if (file_buffer)
            free(file_buffer);

        if (log_cb)
            log_cb(
                RETRO_LOG_ERROR,
                "[Cannonball]: Could not load WAV: %s\n",
                filename);

        return;
    }

    { const uint8_t* file_data =
        (const uint8_t*)file_buffer;

    const size_t file_size =
        (size_t)file_length;

    if (memcmp(file_data, "RIFF", 4) != 0 ||
        memcmp(file_data + 8, "WAVE", 4) != 0)
    {
        if (log_cb)
            log_cb(
                RETRO_LOG_ERROR,
                "[Cannonball]: Invalid RIFF/WAVE file: %s\n",
                filename);

        free(file_buffer);
        return;
    }

    { uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t sample_rate = 0;

    const uint8_t* pcm_data = NULL;
    size_t pcm_size = 0;

    size_t offset = 12;

    while (offset + 8 <= file_size)
    {
        const uint8_t* chunk_id =
            file_data + offset;

        const uint32_t chunk_size =
            wav_read_le32(file_data + offset + 4);

        offset += 8;

        if ((size_t)chunk_size >
            file_size - offset)
        {
            if (log_cb)
                log_cb(
                    RETRO_LOG_ERROR,
                    "[Cannonball]: Truncated WAV chunk: %s\n",
                    filename);

            free(file_buffer);
            return;
        }

        if (memcmp(chunk_id, "fmt ", 4) == 0 &&
            chunk_size >= 16)
        {
            audio_format =
                wav_read_le16(file_data + offset);

            channels =
                wav_read_le16(file_data + offset + 2);

            sample_rate =
                wav_read_le32(file_data + offset + 4);

            bits_per_sample =
                wav_read_le16(file_data + offset + 14);
        }
        else if (memcmp(chunk_id, "data", 4) == 0)
        {
            pcm_data =
                file_data + offset;

            pcm_size =
                chunk_size;
        }

        offset += chunk_size;

        if (chunk_size & 1)
            offset++;
    }

    { const bool supported_bits =
        bits_per_sample == 8  ||
        bits_per_sample == 16 ||
        bits_per_sample == 24 ||
        bits_per_sample == 32;

    if (!pcm_data ||
        audio_format != 1 ||
        (channels != 1 && channels != 2) ||
        !supported_bits ||
        sample_rate == 0)
    {
        if (log_cb)
            log_cb(
                RETRO_LOG_ERROR,
                "[Cannonball]: Unsupported WAV format: "
                "%s [format=%u, %u Hz, "
                "%u channel(s), %u bit]\n",
                filename,
                audio_format,
                sample_rate,
                channels,
                bits_per_sample);

        free(file_buffer);
        return;
    }

    { const size_t bytes_per_input_sample =
        bits_per_sample / 8;

    const size_t input_frame_bytes =
        channels * bytes_per_input_sample;

    const uint64_t input_frames =
        pcm_size / input_frame_bytes;

    if (input_frames == 0)
    {
        if (log_cb)
            log_cb(
                RETRO_LOG_ERROR,
                "[Cannonball]: Empty WAV data: %s\n",
                filename);

        free(file_buffer);
        return;
    }

    { const uint64_t output_frames =
        (input_frames * (uint64_t)FREQ +
         (sample_rate / 2)) /
        sample_rate;

    const uint64_t output_samples =
        output_frames * CHANNELS;

    if (output_frames == 0 ||
        output_samples > (uint64_t)0xffffffffU)
    {
        if (log_cb)
            log_cb(
                RETRO_LOG_ERROR,
                "[Cannonball]: WAV is too large: %s\n",
                filename);

        free(file_buffer);
        return;
    }

    { int16_t* output_data =
        (int16_t*)malloc(
            (size_t)output_samples *
            sizeof(int16_t));

    if (!output_data)
    {
        if (log_cb)
            log_cb(
                RETRO_LOG_ERROR,
                "[Cannonball]: Not enough memory for WAV: %s\n",
                filename);

        free(file_buffer);
        return;
    }

    { uint64_t output_frame; for (output_frame = 0;
         output_frame < output_frames;
         output_frame++)
    {
        const uint64_t source_position =
            output_frame *
            (uint64_t)sample_rate;

        uint64_t source_frame =
            source_position / FREQ;

        const int64_t fraction =
            (int64_t)(
                source_position % FREQ);

        const int64_t inverse_fraction =
            (int64_t)FREQ - fraction;

        if (source_frame >= input_frames)
            source_frame = input_frames - 1;

        { const uint64_t next_frame =
            source_frame + 1 < input_frames
                ? source_frame + 1
                : source_frame;

        { const uint8_t* current =
            pcm_data +
            (size_t)source_frame *
                input_frame_bytes;

        const uint8_t* next =
            pcm_data +
            (size_t)next_frame *
                input_frame_bytes;

        { uint32_t channel; for (channel = 0;
             channel < CHANNELS;
             channel++)
        {
            const uint32_t source_channel =
                channels == 1
                    ? 0
                    : channel;

            { const size_t sample_offset =
                source_channel *
                bytes_per_input_sample;

            const int16_t sample_current =
                wav_read_pcm_sample(
                    current + sample_offset,
                    bits_per_sample);

            const int16_t sample_next =
                wav_read_pcm_sample(
                    next + sample_offset,
                    bits_per_sample);

            int64_t sample =
                ((int64_t)sample_current *
                    inverse_fraction +
                 (int64_t)sample_next *
                    fraction) /
                (int64_t)FREQ;

            output_data[
                output_frame * CHANNELS +
                channel] =
                    (int16_t)sample;
         }} }
     } }} }

    free(file_buffer);

    self->wavfile.data =
        output_data;

    self->wavfile.length =
        (uint32_t)output_samples;

    self->wavfile.pos = 0;
    self->wavfile.loaded = 1;

    Audio_resume_audio(self);

    if (log_cb)
        log_cb(
            RETRO_LOG_INFO,
            "[Cannonball]: Loaded WAV: %s "
            "(PCM%u, %u Hz, %u channel(s), "
            "%lu output frames)\n",
            filename,
            bits_per_sample,
            sample_rate,
            channels,
            (unsigned long)output_frames);
 } } } } } }}

void Audio_clear_wav(Audio* self)
{
    if (self->wavfile.loaded)
    {
        if (self->wavfile.loaded == 1)
            free(self->wavfile.data);
        else
            delete[] self->wavfile.data;        
    }

    self->wavfile.length = 1;
    self->wavfile.data   = EMPTY_BUFFER;
    self->wavfile.pos    = 0;
    self->wavfile.loaded = false;
}
