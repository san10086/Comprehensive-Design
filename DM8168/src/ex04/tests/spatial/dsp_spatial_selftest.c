#include "../../dsp/dsp_spatial.h"
#include "spatial_fir.h"
#include "wav_pcm16.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_INPUT_FRAMES 4097u
#define ERROR_SIZE 256u
#define DSP_FFT_MAX_ERROR_LSB 2
#define DSP_ORBIT_MAX_ERROR_LSB 3
#define DSP_DUAL_MAX_ERROR_LSB 3
static int catalogTest = 0;

static int16_t dual_reference_sample(int16_t sourceA, int16_t sourceB)
{
    int32_t sum = (int32_t)sourceA + (int32_t)sourceB;

    if (sum >= 0) {
        return (int16_t)((sum + 1) / 2);
    }
    return (int16_t)(-(((-sum) + 1) / 2));
}

static int16_t orbit_reference_sample(int16_t measured, int16_t mirrored,
        float mix)
{
    float sample = (float)measured +
            (((float)mirrored - (float)measured) * mix);

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

static int test_orbit_turnaround(void)
{
    DspSpatialState state;
    int16_t silence[16] = {0};
    float step = 1.0f / (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;

    dsp_spatial_reset(&state);
    state.orbitMix = 1.0f - step;
    if ((dsp_spatial_process_orbit_inplace(&state, silence, 4) < 0) ||
            (state.orbitStep >= 0.0f) || (state.orbitMix >= 1.0f)) {
        fprintf(stderr, "DSP spatial orbit selftest: upper turn failed\n");
        return -1;
    }

    memset(silence, 0, sizeof(silence));
    dsp_spatial_reset(&state);
    state.orbitMix = step;
    state.orbitStep = -step;
    if ((dsp_spatial_process_orbit_inplace(&state, silence, 4) < 0) ||
            (state.orbitStep <= 0.0f) || (state.orbitMix <= 0.0f)) {
        fprintf(stderr, "DSP spatial orbit selftest: lower turn failed\n");
        return -1;
    }

    printf("DSP spatial orbit selftest PASS: 4-second triangular path turns smoothly\n");
    return 0;
}

static int test_pair(const char *leftPath, const char *rightPath,
        int direction)
{
    WavPcm16 left = {0};
    WavPcm16 right = {0};
    SpatialFirState referenceState;
    DspSpatialState dspState;
    int16_t *input = NULL;
    int16_t *mono = NULL;
    int16_t *expected = NULL;
    int16_t *actual = NULL;
    const size_t chunks[] = {1u, 17u, 511u, 1024u, 73u};
    uint32_t rng = 0x31415926u;
    size_t chunkIndex = 0u;
    size_t offset = 0u;
    size_t i;
    int maxError = 0;
    char error[ERROR_SIZE];
    int status = 1;

    if ((wav_pcm16_read(leftPath, &left, error, sizeof(error)) < 0) ||
            (wav_pcm16_read(rightPath, &right, error, sizeof(error)) < 0)) {
        fprintf(stderr, "DSP spatial selftest: cannot read HRIR: %s\n",
                error);
        goto leave;
    }
    if ((left.sample_rate != DSP_SPATIAL_SAMPLE_RATE) ||
            (right.sample_rate != DSP_SPATIAL_SAMPLE_RATE) ||
            (left.channels != 1u) || (right.channels != 1u) ||
            (left.frame_count != DSP_SPATIAL_HRIR_LENGTH) ||
            (right.frame_count != DSP_SPATIAL_HRIR_LENGTH)) {
        fprintf(stderr, "DSP spatial selftest: invalid HRIR format\n");
        goto leave;
    }

    input = (int16_t *)malloc(TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    mono = (int16_t *)malloc(TEST_INPUT_FRAMES * sizeof(int16_t));
    expected = (int16_t *)malloc(TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    actual = (int16_t *)malloc(TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    if ((input == NULL) || (mono == NULL) ||
            (expected == NULL) || (actual == NULL)) {
        fprintf(stderr, "DSP spatial selftest: out of memory\n");
        goto leave;
    }

    for (i = 0u; i < TEST_INPUT_FRAMES * 2u; i++) {
        rng = (rng * 1664525u) + 1013904223u;
        input[i] = (int16_t)(rng >> 16);
    }
    for (i = 0u; i < TEST_INPUT_FRAMES; i++) {
        int32_t sum = (int32_t)input[2u * i] +
                (int32_t)input[(2u * i) + 1u];
        mono[i] = (int16_t)(sum / 2);
    }
    memcpy(actual, input, TEST_INPUT_FRAMES * 2u * sizeof(int16_t));

    spatial_fir_reset(&referenceState);
    if (spatial_fir_process(&referenceState, mono, TEST_INPUT_FRAMES,
                left.samples, right.samples, expected) < 0) {
        fprintf(stderr, "DSP spatial selftest: reference failed\n");
        goto leave;
    }

    dsp_spatial_reset(&dspState);
    while (offset < TEST_INPUT_FRAMES) {
        size_t count = chunks[chunkIndex %
                (sizeof(chunks) / sizeof(chunks[0]))];
        if (count > TEST_INPUT_FRAMES - offset) {
            count = TEST_INPUT_FRAMES - offset;
        }
        if (dsp_spatial_process_inplace(&dspState, actual + (2u * offset),
                    (int)count, direction) < 0) {
            fprintf(stderr, "DSP spatial selftest: processing failed\n");
            goto leave;
        }
        offset += count;
        chunkIndex++;
    }

    for (i = 0u; i < TEST_INPUT_FRAMES * 2u; i++) {
        int error = (int)actual[i] - (int)expected[i];

        if (error < 0) {
            error = -error;
        }
        if (error > maxError) {
            maxError = error;
        }
        if (error > DSP_FFT_MAX_ERROR_LSB) {
            fprintf(stderr,
                    "DSP spatial FFT selftest FAIL at sample %zu: "
                    "%d != %d (error %d LSB)\n",
                    i, actual[i], expected[i], error);
            goto leave;
        }
    }

    if (!catalogTest) printf("DSP spatial FFT selftest PASS: %s + %s "
            "(max error %d LSB)\n", leftPath, rightPath, maxError);

    /* Flush a finite input in variable short chunks against the FIR reference. */
    {
        int16_t zeros[511] = {0};
        int16_t tailExpected[1022];
        int16_t tailActual[1022] = {0};
        size_t tailOffset = 0;
        if (spatial_fir_process(&referenceState, zeros, 511,
                    left.samples, right.samples, tailExpected) < 0) goto leave;
        while (tailOffset < 511) {
            size_t count = tailOffset == 0 ? 17 : 511 - tailOffset;
            if (dsp_spatial_process_inplace(&dspState, tailActual + 2 * tailOffset,
                        (int)count, direction) < 0) goto leave;
            tailOffset += count;
        }
        for (i = 0; i < 1022; i++) {
            if (abs((int)tailExpected[i] - tailActual[i]) > DSP_FFT_MAX_ERROR_LSB) {
                fprintf(stderr, "DSP tail mismatch at direction %d sample %zu\n", direction, i);
                goto leave;
            }
        }
    }

    if (direction == DSP_SPATIAL_DIRECTION_40E045) {
        float mix = 0.0f;
        float step = 1.0f /
                (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;

        memcpy(actual, input, TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
        dsp_spatial_reset(&dspState);
        chunkIndex = 0u;
        offset = 0u;
        while (offset < TEST_INPUT_FRAMES) {
            size_t count = chunks[chunkIndex %
                    (sizeof(chunks) / sizeof(chunks[0]))];
            if (count > TEST_INPUT_FRAMES - offset) {
                count = TEST_INPUT_FRAMES - offset;
            }
            if (dsp_spatial_process_orbit_inplace(&dspState,
                        actual + (2u * offset), (int)count) < 0) {
                fprintf(stderr, "DSP spatial orbit selftest: processing failed\n");
                goto leave;
            }
            offset += count;
            chunkIndex++;
        }

        maxError = 0;
        for (i = 0u; i < TEST_INPUT_FRAMES; i++) {
            int16_t measuredLeft = expected[2u * i];
            int16_t measuredRight = expected[(2u * i) + 1u];
            int16_t expectedLeft = orbit_reference_sample(measuredLeft,
                    measuredRight, mix);
            int16_t expectedRight = orbit_reference_sample(measuredRight,
                    measuredLeft, mix);
            int leftError = (int)actual[2u * i] - (int)expectedLeft;
            int rightError = (int)actual[(2u * i) + 1u] -
                    (int)expectedRight;

            if (leftError < 0) {
                leftError = -leftError;
            }
            if (rightError < 0) {
                rightError = -rightError;
            }
            if (leftError > maxError) {
                maxError = leftError;
            }
            if (rightError > maxError) {
                maxError = rightError;
            }
            if ((leftError > DSP_ORBIT_MAX_ERROR_LSB) ||
                    (rightError > DSP_ORBIT_MAX_ERROR_LSB)) {
                fprintf(stderr,
                        "DSP spatial orbit selftest FAIL at frame %zu: "
                        "error L=%d R=%d LSB\n", i, leftError, rightError);
                goto leave;
            }

            mix += step;
            if (mix >= 1.0f) {
                mix = 1.0f;
                step = -1.0f /
                        (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;
            }
            else if (mix <= 0.0f) {
                mix = 0.0f;
                step = 1.0f /
                        (float)DSP_SPATIAL_ORBIT_HALF_CYCLE_FRAMES;
            }
        }
        printf("DSP spatial orbit selftest PASS: variable blocks "
                "remain within %d LSB (max error %d LSB)\n",
                DSP_ORBIT_MAX_ERROR_LSB, maxError);
    }
    status = 0;

leave:
    free(actual);
    free(expected);
    free(mono);
    free(input);
    wav_pcm16_free(&right);
    wav_pcm16_free(&left);
    return status;
}

static int test_dual(const char *left40Path, const char *right40Path,
        const char *left60Path, const char *right60Path,
        int directionA, int directionB, int legacy)
{
    WavPcm16 left40 = {0};
    WavPcm16 right40 = {0};
    WavPcm16 left60 = {0};
    WavPcm16 right60 = {0};
    SpatialFirState sourceAState;
    SpatialFirState sourceBState;
    DspSpatialState dspState;
    int16_t *input = NULL;
    int16_t *sourceA = NULL;
    int16_t *sourceB = NULL;
    int16_t *expectedA = NULL;
    int16_t *expectedB = NULL;
    int16_t *actual = NULL;
    const size_t chunks[] = {1u, 17u, 511u, 1024u, 73u};
    uint32_t rng = 0x27182818u;
    size_t chunkIndex = 0u;
    size_t offset = 0u;
    size_t i;
    int maxError = 0;
    char error[ERROR_SIZE];
    int status = 1;

    if ((wav_pcm16_read(left40Path, &left40, error, sizeof(error)) < 0) ||
            (wav_pcm16_read(right40Path, &right40,
                    error, sizeof(error)) < 0) ||
            (wav_pcm16_read(left60Path, &left60,
                    error, sizeof(error)) < 0) ||
            (wav_pcm16_read(right60Path, &right60,
                    error, sizeof(error)) < 0)) {
        fprintf(stderr, "DSP spatial dual selftest: cannot read HRIR: %s\n",
                error);
        goto leave;
    }
    if ((left40.sample_rate != DSP_SPATIAL_SAMPLE_RATE) ||
            (right40.sample_rate != DSP_SPATIAL_SAMPLE_RATE) ||
            (left60.sample_rate != DSP_SPATIAL_SAMPLE_RATE) ||
            (right60.sample_rate != DSP_SPATIAL_SAMPLE_RATE) ||
            (left40.channels != 1u) || (right40.channels != 1u) ||
            (left60.channels != 1u) || (right60.channels != 1u) ||
            (left40.frame_count != DSP_SPATIAL_HRIR_LENGTH) ||
            (right40.frame_count != DSP_SPATIAL_HRIR_LENGTH) ||
            (left60.frame_count != DSP_SPATIAL_HRIR_LENGTH) ||
            (right60.frame_count != DSP_SPATIAL_HRIR_LENGTH)) {
        fprintf(stderr, "DSP spatial dual selftest: invalid HRIR format\n");
        goto leave;
    }

    input = (int16_t *)malloc(TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    sourceA = (int16_t *)malloc(TEST_INPUT_FRAMES * sizeof(int16_t));
    sourceB = (int16_t *)malloc(TEST_INPUT_FRAMES * sizeof(int16_t));
    expectedA = (int16_t *)malloc(
            TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    expectedB = (int16_t *)malloc(
            TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    actual = (int16_t *)malloc(TEST_INPUT_FRAMES * 2u * sizeof(int16_t));
    if ((input == NULL) || (sourceA == NULL) || (sourceB == NULL) ||
            (expectedA == NULL) || (expectedB == NULL) ||
            (actual == NULL)) {
        fprintf(stderr, "DSP spatial dual selftest: out of memory\n");
        goto leave;
    }

    for (i = 0u; i < TEST_INPUT_FRAMES; i++) {
        rng = (rng * 1664525u) + 1013904223u;
        sourceA[i] = (int16_t)((int16_t)(rng >> 16) / 8);
        rng = (rng * 1664525u) + 1013904223u;
        sourceB[i] = (int16_t)((int16_t)(rng >> 16) / 8);
        input[2u * i] = sourceA[i];
        input[(2u * i) + 1u] = sourceB[i];
    }
    memcpy(actual, input, TEST_INPUT_FRAMES * 2u * sizeof(int16_t));

    spatial_fir_reset(&sourceAState);
    spatial_fir_reset(&sourceBState);
    if ((spatial_fir_process(&sourceAState, sourceA, TEST_INPUT_FRAMES,
                    left60.samples, right60.samples, expectedA) < 0) ||
            (spatial_fir_process(&sourceBState, sourceB, TEST_INPUT_FRAMES,
                    left40.samples, right40.samples, expectedB) < 0)) {
        fprintf(stderr, "DSP spatial dual selftest: reference failed\n");
        goto leave;
    }

    dsp_spatial_reset(&dspState);
    while (offset < TEST_INPUT_FRAMES) {
        size_t count = chunks[chunkIndex %
                (sizeof(chunks) / sizeof(chunks[0]))];
        if (count > TEST_INPUT_FRAMES - offset) {
            count = TEST_INPUT_FRAMES - offset;
        }
        int result = legacy ? dsp_spatial_process_dual_inplace(&dspState,
                    actual + (2u * offset), (int)count) :
                dsp_spatial_process_dual_directions_inplace(&dspState,
                    actual + (2u * offset), (int)count, directionA, directionB);
        if (result < 0) {
            fprintf(stderr, "DSP spatial dual selftest: processing failed\n");
            goto leave;
        }
        offset += count;
        chunkIndex++;
    }

    for (i = 0u; i < TEST_INPUT_FRAMES * 2u; i++) {
        int16_t expected = dual_reference_sample(expectedA[i], expectedB[i]);
        int sampleError = (int)actual[i] - (int)expected;

        if (sampleError < 0) {
            sampleError = -sampleError;
        }
        if (sampleError > maxError) {
            maxError = sampleError;
        }
        if (sampleError > DSP_DUAL_MAX_ERROR_LSB) {
            fprintf(stderr,
                    "DSP spatial dual selftest FAIL at sample %zu: "
                    "%d != %d (error %d LSB)\n",
                    i, actual[i], expected, sampleError);
            goto leave;
        }
    }

    printf("DSP spatial dual selftest PASS: A=%d B=%d (max error %d LSB)\n",
            directionA, directionB, maxError);
    status = 0;

leave:
    free(actual);
    free(expectedB);
    free(expectedA);
    free(sourceB);
    free(sourceA);
    free(input);
    wav_pcm16_free(&right60);
    wav_pcm16_free(&left60);
    wav_pcm16_free(&right40);
    wav_pcm16_free(&left40);
    return status;
}

static int test_saturation(const char *leftPath, const char *rightPath)
{
    WavPcm16 left = {0}, right = {0};
    DspSpatialState state;
    SpatialFirState reference;
    int16_t mono[512], input[1024], expected[1024], actual[1024];
    char error[256];
    int n, ear, k, sign, clipped = 0, status = 1;
    if (wav_pcm16_read(leftPath, &left, error, sizeof(error)) < 0 ||
            wav_pcm16_read(rightPath, &right, error, sizeof(error)) < 0) goto leave;
    if (left.frame_count != 512 || right.frame_count != 512) goto leave;
    for (sign = -1; sign <= 1; sign += 2) {
        for (n = 0; n < 512; n++) {
            mono[n] = (int16_t)(sign * (left.samples[511 - n] >= 0 ? 32760 : -32760));
            input[2 * n] = input[2 * n + 1] = mono[n];
        }
        spatial_fir_reset(&reference);
        if (spatial_fir_process(&reference, mono, 512, left.samples,
                    right.samples, expected) < 0) goto leave;
        memcpy(actual, input, sizeof(input));
        dsp_spatial_reset(&state);
        if (dsp_spatial_process_inplace(&state, actual, 512,
                    DSP_SPATIAL_DIRECTION_40E045) < 0) goto leave;
        for (n = 0; n < 1024; n++) {
            if (expected[n] == INT16_MAX || expected[n] == INT16_MIN) clipped++;
            if (abs((int)actual[n] - expected[n]) > DSP_FFT_MAX_ERROR_LSB) goto leave;
        }
    }
    if (!clipped) goto leave;
    /* Each source clips alone; the unrounded mixed result does not at frame 511. */
    for (n = 0; n < 512; n++) {
        input[2 * n] = mono[n];
        input[2 * n + 1] = (int16_t)(-((int)mono[n] * 3) / 4);
    }
    for (n = 0; n < 512; n++) {
        for (ear = 0; ear < 2; ear++) {
            const int16_t *h = ear ? right.samples : left.samples;
            int64_t accumulator = 0, scaled;
            for (k = 0; k <= n; k++) {
                accumulator += ((int64_t)input[2 * (n - k)] +
                        input[2 * (n - k) + 1]) * h[k];
            }
            scaled = accumulator >= 0 ? (accumulator + 32768) / 65536 :
                    -((-accumulator + 32768) / 65536);
            expected[2 * n + ear] = scaled > INT16_MAX ? INT16_MAX :
                    scaled < INT16_MIN ? INT16_MIN : (int16_t)scaled;
        }
    }
    dsp_spatial_reset(&state);
    memcpy(actual, input, sizeof(input));
    if (dsp_spatial_process_dual_directions_inplace(&state, actual, 512,
                DSP_SPATIAL_DIRECTION_40E045, DSP_SPATIAL_DIRECTION_40E045) < 0) goto leave;
    for (n = 0; n < 1024; n++) {
        if (abs((int)actual[n] - expected[n]) > DSP_DUAL_MAX_ERROR_LSB) goto leave;
    }
    printf("DSP saturation/mix-before-clipping selftest PASS\n");
    status = 0;
leave:
    if (status) fprintf(stderr, "DSP saturation test failed\n");
    wav_pcm16_free(&left);
    wav_pcm16_free(&right);
    return status;
}

static int test_cache_transitions(void)
{
    DspSpatialState used, fresh;
    int16_t block[128], expected[128];
    int i, step;
    const int a = DSP_SPATIAL_DIRECTION_60E250;
    const int b = DSP_SPATIAL_DIRECTION_40E045;
    const int c = HRIR_DIRECTION_CODE(-40, 45);
    dsp_spatial_reset(&used);
    for (step = 0; step < 5; step++) {
        for (i = 0; i < 128; i++) block[i] = expected[i] = (int16_t)(i * 79 - 5000);
        dsp_spatial_reset(&fresh);
        if ((step == 0) || (step == 3)) {
            int dir = step == 0 ? a : b;
            if (dsp_spatial_process_inplace(&used, block, 64, dir) < 0 ||
                    dsp_spatial_process_inplace(&fresh, expected, 64, dir) < 0) return 1;
        }
        else {
            int dirB = step == 1 ? b : c;
            if (dsp_spatial_process_dual_directions_inplace(&used, block, 64, a, dirB) < 0 ||
                    dsp_spatial_process_dual_directions_inplace(&fresh, expected, 64, a, dirB) < 0) return 1;
        }
        if (memcmp(block, expected, sizeof(block)) != 0) {
            fprintf(stderr, "DSP cache transition failed at step %d\n", step);
            return 1;
        }
    }
    memcpy(expected, block, sizeof(block));
    if (dsp_spatial_process_inplace(&used, block, 64, -1) != -1 ||
            dsp_spatial_process_dual_directions_inplace(&used, block, 64, a, 0) != -1 ||
            memcmp(block, expected, sizeof(block)) != 0) return 1;
    printf("DSP cache/invalid-direction selftest PASS\n");
    return 0;
}

static int catalog_path(char *path, size_t size, const char *source, int index, char ear)
{
    const HrirDirection *dir = &kHrirDirections[index];
    int length = snprintf(path, size, "%s/elev%d/%c%de%03da.wav", source,
            dir->elevation, ear, dir->elevation, dir->azimuth);
    return length >= 0 && (size_t)length < size ? 0 : -1;
}

static int test_catalog(const char *source)
{
    char left[1024], right[1024], otherLeft[1024], otherRight[1024];
    int i;
    catalogTest = 1;
    for (i = 0; i < HRIR_DIRECTION_COUNT; i++) {
        int other = (i + 1) % HRIR_DIRECTION_COUNT;
        if (catalog_path(left, sizeof(left), source, i, 'L') < 0 ||
                catalog_path(right, sizeof(right), source, i, 'R') < 0) return 1;
        if (test_pair(left, right, kHrirDirections[i].code)) return 1;
        if (i % 100 == 0 || i == HRIR_DIRECTION_COUNT - 1) {
            if (catalog_path(otherLeft, sizeof(otherLeft), source, other, 'L') < 0 ||
                    catalog_path(otherRight, sizeof(otherRight), source, other, 'R') < 0) return 1;
            if (test_dual(otherLeft, otherRight, left, right,
                        kHrirDirections[i].code, kHrirDirections[other].code, 0) ||
                    test_dual(left, right, left, right,
                        kHrirDirections[i].code, kHrirDirections[i].code, 0)) return 1;
        }
    }
    printf("DSP full catalog PASS: %d directions, variable blocks, tails, generic dual\n",
            HRIR_DIRECTION_COUNT);
    return test_cache_transitions();
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--catalog") == 0) return test_catalog(argv[2]);
    if (argc != 5) {
        fprintf(stderr, "Usage: %s L40 R40 L60 R60\n", argv[0]);
        return 1;
    }
    if (test_pair(argv[1], argv[2], DSP_SPATIAL_DIRECTION_40E045) != 0) {
        return 1;
    }
    if (test_pair(argv[3], argv[4], DSP_SPATIAL_DIRECTION_60E250) != 0) {
        return 1;
    }
    if (test_dual(argv[1], argv[2], argv[3], argv[4],
                DSP_SPATIAL_DIRECTION_60E250, DSP_SPATIAL_DIRECTION_40E045, 1) != 0) {
        return 1;
    }
    if (test_saturation(argv[1], argv[2])) return 1;
    return test_orbit_turnaround() == 0 ? 0 : 1;
}
