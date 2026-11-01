#ifndef WAV_PCM16_H
#define WAV_PCM16_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    size_t frame_count;
    int16_t *samples;
} WavPcm16;

int wav_pcm16_read(const char *path, WavPcm16 *wav,
        char *error, size_t error_size);
int wav_pcm16_write(const char *path, uint32_t sample_rate,
        uint16_t channels, const int16_t *samples, size_t frame_count,
        char *error, size_t error_size);
void wav_pcm16_free(WavPcm16 *wav);

#endif
