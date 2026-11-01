#ifndef SPATIAL_FIR_H
#define SPATIAL_FIR_H

#include <stddef.h>
#include <stdint.h>

#define SPATIAL_SAMPLE_RATE       44100u
#define SPATIAL_BLOCK_FRAMES      1024u
#define SPATIAL_HRIR_LENGTH       512u
#define SPATIAL_HISTORY_LENGTH    (SPATIAL_HRIR_LENGTH - 1u)

typedef struct {
    int16_t history[SPATIAL_HISTORY_LENGTH];
} SpatialFirState;

void spatial_fir_reset(SpatialFirState *state);

/*
 * Process one mono PCM16/Q15 block into interleaved stereo PCM16/Q15.
 * HRIR coefficients are kept in their natural WAV order; do not reverse them.
 */
int spatial_fir_process(SpatialFirState *state,
        const int16_t *input_mono, size_t frame_count,
        const int16_t hrir_left[SPATIAL_HRIR_LENGTH],
        const int16_t hrir_right[SPATIAL_HRIR_LENGTH],
        int16_t *output_stereo);

#endif
