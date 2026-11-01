#ifndef DSP_SPATIAL_H
#define DSP_SPATIAL_H

#include <stdint.h>
#include "../shared/hrir_catalog.h"

#define DSP_SPATIAL_SAMPLE_RATE       44100
#define DSP_SPATIAL_HRIR_LENGTH       512
#define DSP_SPATIAL_HISTORY_LENGTH    (DSP_SPATIAL_HRIR_LENGTH - 1)
#define DSP_SPATIAL_MAX_BLOCK_FRAMES  1024
#define DSP_SPATIAL_FFT_SIZE          2048
#define DSP_SPATIAL_FFT_FLOATS        (2 * DSP_SPATIAL_FFT_SIZE)
#define DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES \
        (2 * DSP_SPATIAL_SAMPLE_RATE)

#define DSP_SPATIAL_DIRECTION_40E045  HRIR_DIRECTION_CODE(40, 45)
#define DSP_SPATIAL_DIRECTION_60E250  HRIR_DIRECTION_CODE(60, 250)

typedef struct {
    float inputSpectrum[DSP_SPATIAL_FFT_FLOATS];
    float work[DSP_SPATIAL_FFT_FLOATS];
    float leftHrirSpectrum[DSP_SPATIAL_FFT_FLOATS];
    float rightHrirSpectrum[DSP_SPATIAL_FFT_FLOATS];
    float leftOverlap[DSP_SPATIAL_HISTORY_LENGTH];
    float rightOverlap[DSP_SPATIAL_HISTORY_LENGTH];
    float secondLeftHrirSpectrum[DSP_SPATIAL_FFT_FLOATS];
    float secondRightHrirSpectrum[DSP_SPATIAL_FFT_FLOATS];
    float secondLeftOverlap[DSP_SPATIAL_HISTORY_LENGTH];
    float secondRightOverlap[DSP_SPATIAL_HISTORY_LENGTH];
    float outputBlock[2 * DSP_SPATIAL_MAX_BLOCK_FRAMES];
    int16_t secondInput[DSP_SPATIAL_MAX_BLOCK_FRAMES];
    float orbitMix;
    float orbitStep;
    int preparedDirection;
    int preparedSecondDirection;
    int preparedDual;
} DspSpatialState;

void dsp_spatial_reset(DspSpatialState *state);

/*
 * Convert an interleaved stereo PCM16 block to binaural stereo in-place using
 * 2048-point FFT overlap-add convolution. The input channels are averaged to
 * mono before convolution. The state must persist between consecutive blocks.
 */
int dsp_spatial_process_inplace(DspSpatialState *state, int16_t *stereoPcm,
        int frameCount, int direction);

/*
 * Move the 40e045 response continuously between its measured direction and
 * the left/right-ear-swapped mirror. A complete out-and-back cycle is 4 s.
 */
int dsp_spatial_process_orbit_inplace(DspSpatialState *state,
        int16_t *stereoPcm, int frameCount);

/*
 * Treat input left as source A at 60e250 and input right as source B at
 * 40e045. Render both independent sources to one binaural stereo output.
 */
int dsp_spatial_process_dual_inplace(DspSpatialState *state,
        int16_t *stereoPcm, int frameCount);

/* Generic dual renderer; directions use HRIR_DIRECTION_CODE(elevation, azimuth). */
int dsp_spatial_process_dual_directions_inplace(DspSpatialState *state,
        int16_t *stereoPcm, int frameCount, int directionA, int directionB);

#endif
