#include "wav_pcm16.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_u16_le(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const unsigned char *p)
{
    return (uint32_t)p[0] |
            ((uint32_t)p[1] << 8) |
            ((uint32_t)p[2] << 16) |
            ((uint32_t)p[3] << 24);
}

static void write_u16_le(FILE *fp, uint16_t value)
{
    unsigned char b[2];

    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    (void)fwrite(b, 1, sizeof(b), fp);
}

static void write_u32_le(FILE *fp, uint32_t value)
{
    unsigned char b[4];

    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    b[2] = (unsigned char)((value >> 16) & 0xffu);
    b[3] = (unsigned char)((value >> 24) & 0xffu);
    (void)fwrite(b, 1, sizeof(b), fp);
}

static void set_error(char *error, size_t error_size, const char *message)
{
    if ((error != NULL) && (error_size > 0u)) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static int skip_bytes(FILE *fp, uint32_t byte_count)
{
    unsigned char buffer[256];
    uint32_t remaining = byte_count;

    while (remaining > 0u) {
        size_t chunk = remaining < sizeof(buffer) ?
                (size_t)remaining : sizeof(buffer);
        if (fread(buffer, 1, chunk, fp) != chunk) {
            return -1;
        }
        remaining -= (uint32_t)chunk;
    }
    return 0;
}

void wav_pcm16_free(WavPcm16 *wav)
{
    if (wav == NULL) {
        return;
    }
    free(wav->samples);
    memset(wav, 0, sizeof(*wav));
}

int wav_pcm16_read(const char *path, WavPcm16 *wav,
        char *error, size_t error_size)
{
    FILE *fp = NULL;
    unsigned char header[12];
    unsigned char chunk_header[8];
    unsigned char fmt[16];
    unsigned char *raw = NULL;
    uint16_t audio_format = 0u;
    uint16_t channels = 0u;
    uint16_t bits_per_sample = 0u;
    uint16_t block_align = 0u;
    uint32_t sample_rate = 0u;
    uint32_t data_size = 0u;
    long data_offset = -1L;
    size_t sample_count;
    size_t i;
    int status = -1;

    if ((path == NULL) || (wav == NULL)) {
        set_error(error, error_size, "invalid WAV arguments");
        return -1;
    }
    memset(wav, 0, sizeof(*wav));

    fp = fopen(path, "rb");
    if (fp == NULL) {
        set_error(error, error_size, "cannot open WAV file");
        return -1;
    }

    if (fread(header, 1, sizeof(header), fp) != sizeof(header)) {
        set_error(error, error_size, "file is too short for a WAV header");
        goto leave;
    }
    if ((memcmp(header, "RIFF", 4) != 0) ||
            (memcmp(header + 8, "WAVE", 4) != 0)) {
        set_error(error, error_size,
                "file is not a RIFF/WAVE file (it may be M4A/MP3 with a .wav name)");
        goto leave;
    }

    while (fread(chunk_header, 1, sizeof(chunk_header), fp) ==
            sizeof(chunk_header)) {
        uint32_t chunk_size = read_u32_le(chunk_header + 4);

        if (memcmp(chunk_header, "fmt ", 4) == 0) {
            if (chunk_size < sizeof(fmt)) {
                set_error(error, error_size, "WAV fmt chunk is too short");
                goto leave;
            }
            if (fread(fmt, 1, sizeof(fmt), fp) != sizeof(fmt)) {
                set_error(error, error_size, "cannot read WAV fmt chunk");
                goto leave;
            }
            audio_format = read_u16_le(fmt);
            channels = read_u16_le(fmt + 2);
            sample_rate = read_u32_le(fmt + 4);
            block_align = read_u16_le(fmt + 12);
            bits_per_sample = read_u16_le(fmt + 14);
            if (skip_bytes(fp, chunk_size - (uint32_t)sizeof(fmt)) < 0) {
                set_error(error, error_size, "truncated WAV fmt chunk");
                goto leave;
            }
        }
        else if (memcmp(chunk_header, "data", 4) == 0) {
            data_offset = ftell(fp);
            data_size = chunk_size;
            if ((data_offset < 0L) || (skip_bytes(fp, chunk_size) < 0)) {
                set_error(error, error_size, "truncated WAV data chunk");
                goto leave;
            }
        }
        else if (skip_bytes(fp, chunk_size) < 0) {
            set_error(error, error_size, "truncated WAV chunk");
            goto leave;
        }

        if ((chunk_size & 1u) != 0u) {
            if (fgetc(fp) == EOF) {
                set_error(error, error_size, "missing WAV chunk padding byte");
                goto leave;
            }
        }
    }

    if ((audio_format != 1u) || (bits_per_sample != 16u)) {
        set_error(error, error_size, "only uncompressed PCM16 WAV is supported");
        goto leave;
    }
    if ((channels == 0u) || (sample_rate == 0u) ||
            (block_align != (uint16_t)(channels * 2u))) {
        set_error(error, error_size, "invalid PCM16 WAV format fields");
        goto leave;
    }
    if ((data_offset < 0L) || (data_size == 0u) ||
            ((data_size % block_align) != 0u)) {
        set_error(error, error_size, "invalid or missing WAV data chunk");
        goto leave;
    }

    raw = (unsigned char *)malloc(data_size);
    sample_count = (size_t)data_size / 2u;
    wav->samples = (int16_t *)malloc(sample_count * sizeof(int16_t));
    if ((raw == NULL) || (wav->samples == NULL)) {
        set_error(error, error_size, "out of memory while reading WAV");
        goto leave;
    }
    if ((fseek(fp, data_offset, SEEK_SET) != 0) ||
            (fread(raw, 1, data_size, fp) != data_size)) {
        set_error(error, error_size, "cannot read WAV sample data");
        goto leave;
    }

    for (i = 0u; i < sample_count; i++) {
        uint16_t value = (uint16_t)raw[2u * i] |
                ((uint16_t)raw[(2u * i) + 1u] << 8);
        wav->samples[i] = (int16_t)value;
    }
    wav->sample_rate = sample_rate;
    wav->channels = channels;
    wav->frame_count = sample_count / channels;
    status = 0;

leave:
    free(raw);
    if (fp != NULL) {
        (void)fclose(fp);
    }
    if (status < 0) {
        wav_pcm16_free(wav);
    }
    return status;
}

int wav_pcm16_write(const char *path, uint32_t sample_rate,
        uint16_t channels, const int16_t *samples, size_t frame_count,
        char *error, size_t error_size)
{
    FILE *fp;
    size_t sample_count;
    size_t data_size_value;
    uint32_t data_size;
    size_t i;

    if ((path == NULL) || (sample_rate == 0u) || (channels == 0u) ||
            (samples == NULL)) {
        set_error(error, error_size, "invalid WAV write arguments");
        return -1;
    }
    if (frame_count > (SIZE_MAX / channels)) {
        set_error(error, error_size, "WAV sample count overflow");
        return -1;
    }
    sample_count = frame_count * channels;
    if (sample_count > (SIZE_MAX / 2u)) {
        set_error(error, error_size, "WAV data size overflow");
        return -1;
    }
    data_size_value = sample_count * 2u;
    if (data_size_value > (size_t)(UINT32_MAX - 36u)) {
        set_error(error, error_size, "WAV is too large for RIFF32");
        return -1;
    }
    data_size = (uint32_t)data_size_value;

    fp = fopen(path, "wb");
    if (fp == NULL) {
        set_error(error, error_size, "cannot create output WAV");
        return -1;
    }

    (void)fwrite("RIFF", 1, 4, fp);
    write_u32_le(fp, 36u + data_size);
    (void)fwrite("WAVE", 1, 4, fp);
    (void)fwrite("fmt ", 1, 4, fp);
    write_u32_le(fp, 16u);
    write_u16_le(fp, 1u);
    write_u16_le(fp, channels);
    write_u32_le(fp, sample_rate);
    write_u32_le(fp, sample_rate * (uint32_t)channels * 2u);
    write_u16_le(fp, (uint16_t)(channels * 2u));
    write_u16_le(fp, 16u);
    (void)fwrite("data", 1, 4, fp);
    write_u32_le(fp, data_size);

    for (i = 0u; i < sample_count; i++) {
        write_u16_le(fp, (uint16_t)samples[i]);
    }
    if (ferror(fp) != 0) {
        set_error(error, error_size, "failed while writing WAV data");
        (void)fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        set_error(error, error_size, "failed to finalize output WAV");
        return -1;
    }
    return 0;
}
