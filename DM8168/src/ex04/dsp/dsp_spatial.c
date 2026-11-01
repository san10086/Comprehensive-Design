#include "dsp_spatial.h"

#include <limits.h>
#include <string.h>

#include "dsp_spatial_coefficients.inc"
#include "dsp_spatial_fft_twiddles.inc"

#if (DSP_SPATIAL_FFT_SIZE & (DSP_SPATIAL_FFT_SIZE - 1)) != 0
#error DSP_SPATIAL_FFT_SIZE must be a power of two
#endif

#if DSP_SPATIAL_FFT_SIZE < \
        (DSP_SPATIAL_MAX_BLOCK_FRAMES + DSP_SPATIAL_HRIR_LENGTH - 1)
#error DSP_SPATIAL_FFT_SIZE is too short for linear convolution
#endif

static int16_t spatial_downmix(const int16_t *stereoPcm, int frame)
{
    int32_t sum = (int32_t)stereoPcm[2 * frame] +
            (int32_t)stereoPcm[(2 * frame) + 1];

    return (int16_t)(sum / 2);
}

static int16_t spatial_float_to_pcm16(float sample)
{
    float scaled = sample * 32768.0f;

    if (scaled >= (float)INT16_MAX) {
        return INT16_MAX;
    }
    if (scaled <= (float)INT16_MIN) {
        return INT16_MIN;
    }
    if (scaled >= 0.0f) {
        scaled += 0.5f;
    }
    else {
        scaled -= 0.5f;
    }
    return (int16_t)scaled;
}

static int16_t spatial_pcm_float_to_pcm16(float sample)
{
    if (sample >= (float)INT16_MAX) {
        return INT16_MAX;
    }
    if (sample <= (float)INT16_MIN) {
        return INT16_MIN;
    }
    if (sample >= 0.0f) {
        sample += 0.5f;
    }
    else {
        sample -= 0.5f;
    }
    return (int16_t)sample;
}

/* In-place radix-2 complex FFT. Complex values are interleaved real/imag. */
static void spatial_fft(float *data, int inverse)
{
    unsigned int i;
    unsigned int j;
    unsigned int bit;
    unsigned int length;

    for (i = 1u, j = 0u; i < DSP_SPATIAL_FFT_SIZE; i++) {
        float tempReal;
        float tempImag;

        bit = DSP_SPATIAL_FFT_SIZE >> 1;
        while ((j & bit) != 0u) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;

        if (i < j) {
            tempReal = data[2u * i];
            tempImag = data[(2u * i) + 1u];
            data[2u * i] = data[2u * j];
            data[(2u * i) + 1u] = data[(2u * j) + 1u];
            data[2u * j] = tempReal;
            data[(2u * j) + 1u] = tempImag;
        }
    }

    for (length = 2u; length <= DSP_SPATIAL_FFT_SIZE; length <<= 1) {
        unsigned int halfLength = length >> 1;
        unsigned int twiddleStep = DSP_SPATIAL_FFT_SIZE / length;
        unsigned int base;

        for (base = 0u; base < DSP_SPATIAL_FFT_SIZE; base += length) {
            unsigned int offset;

            for (offset = 0u; offset < halfLength; offset++) {
                unsigned int even = base + offset;
                unsigned int odd = even + halfLength;
                unsigned int twiddle = offset * twiddleStep;
                float wr = kSpatialFftTwiddle[2u * twiddle];
                float wi = kSpatialFftTwiddle[(2u * twiddle) + 1u];
                float oddReal;
                float oddImag;
                float productReal;
                float productImag;
                float evenReal;
                float evenImag;

                if (inverse != 0) {
                    wi = -wi;
                }

                oddReal = data[2u * odd];
                oddImag = data[(2u * odd) + 1u];
                productReal = (oddReal * wr) - (oddImag * wi);
                productImag = (oddReal * wi) + (oddImag * wr);
                evenReal = data[2u * even];
                evenImag = data[(2u * even) + 1u];

                data[2u * even] = evenReal + productReal;
                data[(2u * even) + 1u] = evenImag + productImag;
                data[2u * odd] = evenReal - productReal;
                data[(2u * odd) + 1u] = evenImag - productImag;
            }
        }
    }

    if (inverse != 0) {
        const float scale = 1.0f / (float)DSP_SPATIAL_FFT_SIZE;

        for (i = 0u; i < DSP_SPATIAL_FFT_FLOATS; i++) {
            data[i] *= scale;
        }
    }
}

static void spatial_prepare_hrir_spectrum(float *spectrum,
        const int16_t *hrir)
{
    int i;

    memset(spectrum, 0, DSP_SPATIAL_FFT_FLOATS * sizeof(spectrum[0]));
    for (i = 0; i < DSP_SPATIAL_HRIR_LENGTH; i++) {
        spectrum[2 * i] = (float)hrir[i] / 32768.0f;
    }
    spatial_fft(spectrum, 0);
}

static int spatial_prepare_direction(DspSpatialState *state, int direction)
{
    int index = hrir_direction_index(direction);
    if (index < 0) {
        return -1;
    }

    spatial_prepare_hrir_spectrum(state->leftHrirSpectrum, kHrirCoefficients[index][0]);
    spatial_prepare_hrir_spectrum(state->rightHrirSpectrum, kHrirCoefficients[index][1]);
    memset(state->leftOverlap, 0, sizeof(state->leftOverlap));
    memset(state->rightOverlap, 0, sizeof(state->rightOverlap));
    state->preparedDirection = direction;
    state->preparedDual = 0;
    return 0;
}

static int spatial_prepare_dual(DspSpatialState *state, int directionA,
        int directionB)
{
    int indexA = hrir_direction_index(directionA);
    int indexB = hrir_direction_index(directionB);
    if ((indexA < 0) || (indexB < 0)) {
        return -1;
    }
    spatial_prepare_hrir_spectrum(state->leftHrirSpectrum,
            kHrirCoefficients[indexA][0]);
    spatial_prepare_hrir_spectrum(state->rightHrirSpectrum,
            kHrirCoefficients[indexA][1]);
    spatial_prepare_hrir_spectrum(state->secondLeftHrirSpectrum,
            kHrirCoefficients[indexB][0]);
    spatial_prepare_hrir_spectrum(state->secondRightHrirSpectrum,
            kHrirCoefficients[indexB][1]);
    memset(state->leftOverlap, 0, sizeof(state->leftOverlap));
    memset(state->rightOverlap, 0, sizeof(state->rightOverlap));
    memset(state->secondLeftOverlap, 0,
            sizeof(state->secondLeftOverlap));
    memset(state->secondRightOverlap, 0,
            sizeof(state->secondRightOverlap));
    state->preparedDirection = directionA;
    state->preparedSecondDirection = directionB;
    state->preparedDual = 1;
    return 0;
}

static void spatial_convolve_ear(DspSpatialState *state,
        const float *hrirSpectrum, float *overlap,
        float *output, int frameCount, int channel, int accumulate)
{
    int i;

    for (i = 0; i < DSP_SPATIAL_FFT_SIZE; i++) {
        float inputReal = state->inputSpectrum[2 * i];
        float inputImag = state->inputSpectrum[(2 * i) + 1];
        float hrirReal = hrirSpectrum[2 * i];
        float hrirImag = hrirSpectrum[(2 * i) + 1];

        state->work[2 * i] =
                (inputReal * hrirReal) - (inputImag * hrirImag);
        state->work[(2 * i) + 1] =
                (inputReal * hrirImag) + (inputImag * hrirReal);
    }

    spatial_fft(state->work, 1);

    for (i = 0; i < frameCount; i++) {
        float sample = state->work[2 * i];

        if (i < DSP_SPATIAL_HISTORY_LENGTH) {
            sample += overlap[i];
        }
        if (accumulate != 0) {
            output[(2 * i) + channel] += sample;
        }
        else {
            output[(2 * i) + channel] = sample;
        }
    }

    /*
     * Preserve the unconsumed old overlap when frameCount is shorter than
     * 511, then add the current block's convolution tail. Iterating forward
     * is safe because every old-overlap read is from a higher index.
     */
    for (i = 0; i < DSP_SPATIAL_HISTORY_LENGTH; i++) {
        int tailIndex = frameCount + i;
        float oldTail = 0.0f;

        if (tailIndex < DSP_SPATIAL_HISTORY_LENGTH) {
            oldTail = overlap[tailIndex];
        }
        overlap[i] = state->work[2 * tailIndex] + oldTail;
    }
}

void dsp_spatial_reset(DspSpatialState *state)
{
    if (state != 0) {
        memset(state->leftOverlap, 0, sizeof(state->leftOverlap));
        memset(state->rightOverlap, 0, sizeof(state->rightOverlap));
        memset(state->secondLeftOverlap, 0,
                sizeof(state->secondLeftOverlap));
        memset(state->secondRightOverlap, 0,
                sizeof(state->secondRightOverlap));
        state->orbitMix = 0.0f;
        state->orbitStep = 1.0f /
                (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;
        state->preparedDirection = -1;
        state->preparedSecondDirection = -1;
        state->preparedDual = 0;
    }
}

int dsp_spatial_process_inplace(DspSpatialState *state, int16_t *stereoPcm,
        int frameCount, int direction)
{
    int i;

    if ((state == 0) || (stereoPcm == 0) || (frameCount <= 0) ||
            (frameCount > DSP_SPATIAL_MAX_BLOCK_FRAMES)) {
        return -1;
    }
    if (((state->preparedDirection != direction) || state->preparedDual) &&
            (spatial_prepare_direction(state, direction) < 0)) {
        return -1;
    }

    for (i = 0; i < frameCount; i++) {
        int16_t mono = spatial_downmix(stereoPcm, i);

        state->inputSpectrum[2 * i] = (float)mono / 32768.0f;
        state->inputSpectrum[(2 * i) + 1] = 0.0f;
    }
    for (i = frameCount; i < DSP_SPATIAL_FFT_SIZE; i++) {
        state->inputSpectrum[2 * i] = 0.0f;
        state->inputSpectrum[(2 * i) + 1] = 0.0f;
    }
    spatial_fft(state->inputSpectrum, 0);

    spatial_convolve_ear(state, state->leftHrirSpectrum,
            state->leftOverlap, state->outputBlock, frameCount, 0, 0);
    spatial_convolve_ear(state, state->rightHrirSpectrum,
            state->rightOverlap, state->outputBlock, frameCount, 1, 0);
    for (i = 0; i < frameCount; i++) {
        stereoPcm[2 * i] = spatial_float_to_pcm16(
                state->outputBlock[2 * i]);
        stereoPcm[(2 * i) + 1] = spatial_float_to_pcm16(
                state->outputBlock[(2 * i) + 1]);
    }
    return 0;
}

int dsp_spatial_process_orbit_inplace(DspSpatialState *state,
        int16_t *stereoPcm, int frameCount)
{
    int i;

    if (dsp_spatial_process_inplace(state, stereoPcm, frameCount,
                DSP_SPATIAL_DIRECTION_40E045) < 0) {
        return -1;
    }

    for (i = 0; i < frameCount; i++) {
        int16_t measuredLeft = stereoPcm[2 * i];
        int16_t measuredRight = stereoPcm[(2 * i) + 1];
        float mix = state->orbitMix;
        float left = (float)measuredLeft +
                (((float)measuredRight - (float)measuredLeft) * mix);
        float right = (float)measuredRight +
                (((float)measuredLeft - (float)measuredRight) * mix);

        stereoPcm[2 * i] = spatial_pcm_float_to_pcm16(left);
        stereoPcm[(2 * i) + 1] = spatial_pcm_float_to_pcm16(right);

        state->orbitMix += state->orbitStep;
        if (state->orbitMix >= 1.0f) {
            state->orbitMix = 1.0f;
            state->orbitStep = -1.0f /
                    (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;
        }
        else if (state->orbitMix <= 0.0f) {
            state->orbitMix = 0.0f;
            state->orbitStep = 1.0f /
                    (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;
        }
    }
    return 0;
}

int dsp_spatial_process_dual_inplace(DspSpatialState *state,
        int16_t *stereoPcm, int frameCount)
{
    return dsp_spatial_process_dual_directions_inplace(state, stereoPcm,
            frameCount, DSP_SPATIAL_DIRECTION_60E250,
            DSP_SPATIAL_DIRECTION_40E045);
}

int dsp_spatial_process_dual_directions_inplace(DspSpatialState *state,
        int16_t *stereoPcm, int frameCount, int directionA, int directionB)
{
    int i;

    if ((state == 0) || (stereoPcm == 0) || (frameCount <= 0) ||
            (frameCount > DSP_SPATIAL_MAX_BLOCK_FRAMES)) {
        return -1;
    }
    if ((!state->preparedDual || (state->preparedDirection != directionA) ||
            (state->preparedSecondDirection != directionB)) &&
            (spatial_prepare_dual(state, directionA, directionB) < 0)) {
        return -1;
    }

    for (i = 0; i < frameCount; i++) {
        state->secondInput[i] = stereoPcm[(2 * i) + 1];
        state->inputSpectrum[2 * i] =
                (float)stereoPcm[2 * i] / 32768.0f;
        state->inputSpectrum[(2 * i) + 1] = 0.0f;
    }
    for (i = frameCount; i < DSP_SPATIAL_FFT_SIZE; i++) {
        state->inputSpectrum[2 * i] = 0.0f;
        state->inputSpectrum[(2 * i) + 1] = 0.0f;
    }
    spatial_fft(state->inputSpectrum, 0);

    spatial_convolve_ear(state, state->leftHrirSpectrum,
            state->leftOverlap, state->outputBlock, frameCount, 0, 0);
    spatial_convolve_ear(state, state->rightHrirSpectrum,
            state->rightOverlap, state->outputBlock, frameCount, 1, 0);

    for (i = 0; i < frameCount; i++) {
        state->inputSpectrum[2 * i] =
                (float)state->secondInput[i] / 32768.0f;
        state->inputSpectrum[(2 * i) + 1] = 0.0f;
    }
    for (i = frameCount; i < DSP_SPATIAL_FFT_SIZE; i++) {
        state->inputSpectrum[2 * i] = 0.0f;
        state->inputSpectrum[(2 * i) + 1] = 0.0f;
    }
    spatial_fft(state->inputSpectrum, 0);

    spatial_convolve_ear(state, state->secondLeftHrirSpectrum,
            state->secondLeftOverlap, state->outputBlock,
            frameCount, 0, 1);
    spatial_convolve_ear(state, state->secondRightHrirSpectrum,
            state->secondRightOverlap, state->outputBlock,
            frameCount, 1, 1);

    /* Equal linear weights leave headroom when both sources peak together. */
    for (i = 0; i < frameCount; i++) {
        stereoPcm[2 * i] = spatial_float_to_pcm16(
                0.5f * state->outputBlock[2 * i]);
        stereoPcm[(2 * i) + 1] = spatial_float_to_pcm16(
                0.5f * state->outputBlock[(2 * i) + 1]);
    }
    return 0;
}
