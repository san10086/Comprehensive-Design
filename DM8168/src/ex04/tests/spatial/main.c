#include "spatial_fir.h"
#include "wav_pcm16.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_SIZE 256u

static int16_t q15_reference_result(int64_t accumulator)
{
    int64_t scaled;

    if (accumulator >= 0) {
        scaled = (accumulator + 16384) >> 15;
    }
    else {
        scaled = -(((-accumulator) + 16384) >> 15);
    }
    if (scaled > INT16_MAX) {
        return INT16_MAX;
    }
    if (scaled < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)scaled;
}

static int load_hrir_pair(const char *left_path, const char *right_path,
        WavPcm16 *left, WavPcm16 *right)
{
    char error[ERROR_SIZE];

    if (wav_pcm16_read(left_path, left, error, sizeof(error)) < 0) {
        fprintf(stderr, "error: cannot read left HRIR '%s': %s\n",
                left_path, error);
        return -1;
    }
    if (wav_pcm16_read(right_path, right, error, sizeof(error)) < 0) {
        fprintf(stderr, "error: cannot read right HRIR '%s': %s\n",
                right_path, error);
        wav_pcm16_free(left);
        return -1;
    }
    if ((left->sample_rate != SPATIAL_SAMPLE_RATE) ||
            (right->sample_rate != SPATIAL_SAMPLE_RATE) ||
            (left->channels != 1u) || (right->channels != 1u) ||
            (left->frame_count != SPATIAL_HRIR_LENGTH) ||
            (right->frame_count != SPATIAL_HRIR_LENGTH)) {
        fprintf(stderr,
                "error: HRIR pair must be mono PCM16, 44100 Hz, 512 samples\n");
        wav_pcm16_free(left);
        wav_pcm16_free(right);
        return -1;
    }
    return 0;
}

static int16_t *make_mono(const WavPcm16 *input)
{
    int16_t *mono;
    size_t n;

    if ((input == NULL) || (input->samples == NULL) ||
            ((input->channels != 1u) && (input->channels != 2u))) {
        return NULL;
    }
    mono = (int16_t *)malloc(input->frame_count * sizeof(int16_t));
    if (mono == NULL) {
        return NULL;
    }

    if (input->channels == 1u) {
        memcpy(mono, input->samples,
                input->frame_count * sizeof(int16_t));
    }
    else {
        for (n = 0u; n < input->frame_count; n++) {
            int32_t sum = (int32_t)input->samples[2u * n] +
                    (int32_t)input->samples[(2u * n) + 1u];
            mono[n] = (int16_t)(sum / 2);
        }
    }
    return mono;
}

static int parse_size(const char *text, size_t *value, int allow_zero)
{
    char *end = NULL;
    uintmax_t parsed;

    if ((text == NULL) || (value == NULL) ||
            (*text < '0') || (*text > '9')) {
        return -1;
    }
    errno = 0;
    parsed = strtoumax(text, &end, 10);
    if ((errno != 0) || (end == text) || (*end != '\0') ||
            ((!allow_zero) && (parsed == 0u)) ||
            (parsed > (uintmax_t)SIZE_MAX)) {
        return -1;
    }
    *value = (size_t)parsed;
    return 0;
}

static int render_file(const char *input_path, const char *left_path,
        const char *right_path, const char *output_path, size_t block_frames)
{
    WavPcm16 input = {0};
    WavPcm16 left = {0};
    WavPcm16 right = {0};
    SpatialFirState state;
    int16_t *mono = NULL;
    int16_t *output = NULL;
    int16_t tail[SPATIAL_HISTORY_LENGTH] = {0};
    size_t output_frames;
    size_t offset;
    char error[ERROR_SIZE];
    int status = 1;

    if (wav_pcm16_read(input_path, &input, error, sizeof(error)) < 0) {
        fprintf(stderr, "error: cannot read input '%s': %s\n",
                input_path, error);
        goto leave;
    }
    if ((input.sample_rate != SPATIAL_SAMPLE_RATE) ||
            ((input.channels != 1u) && (input.channels != 2u))) {
        fprintf(stderr,
                "error: input must be PCM16 WAV, 44100 Hz, mono or stereo\n");
        goto leave;
    }
    if (load_hrir_pair(left_path, right_path, &left, &right) < 0) {
        goto leave;
    }
    if (input.frame_count > (SIZE_MAX - SPATIAL_HISTORY_LENGTH)) {
        fprintf(stderr, "error: input is too large\n");
        goto leave;
    }
    output_frames = input.frame_count + SPATIAL_HISTORY_LENGTH;
    if (output_frames > (SIZE_MAX / (2u * sizeof(int16_t)))) {
        fprintf(stderr, "error: output size overflow\n");
        goto leave;
    }

    mono = make_mono(&input);
    output = (int16_t *)malloc(output_frames * 2u * sizeof(int16_t));
    if ((mono == NULL) || (output == NULL)) {
        fprintf(stderr, "error: out of memory while rendering\n");
        goto leave;
    }

    spatial_fir_reset(&state);
    offset = 0u;
    while (offset < input.frame_count) {
        size_t count = input.frame_count - offset;
        if (count > block_frames) {
            count = block_frames;
        }
        if (spatial_fir_process(&state, mono + offset, count,
                    left.samples, right.samples, output + (2u * offset)) < 0) {
            fprintf(stderr, "error: spatial FIR failed\n");
            goto leave;
        }
        offset += count;
    }
    if (spatial_fir_process(&state, tail, SPATIAL_HISTORY_LENGTH,
                left.samples, right.samples,
                output + (2u * input.frame_count)) < 0) {
        fprintf(stderr, "error: spatial FIR tail flush failed\n");
        goto leave;
    }

    if (wav_pcm16_write(output_path, SPATIAL_SAMPLE_RATE, 2u,
                output, output_frames, error, sizeof(error)) < 0) {
        fprintf(stderr, "error: cannot write '%s': %s\n",
                output_path, error);
        goto leave;
    }

    printf("render PASS\n");
    printf("  input : %s (%zu frames, %u channel%s)\n",
            input_path, input.frame_count, input.channels,
            input.channels == 1u ? "" : "s");
    printf("  HRIR  : %s + %s\n", left_path, right_path);
    printf("  block : %zu frames\n", block_frames);
    printf("  output: %s (%zu stereo frames, includes 511-frame tail)\n",
            output_path, output_frames);
    status = 0;

leave:
    free(output);
    free(mono);
    wav_pcm16_free(&right);
    wav_pcm16_free(&left);
    wav_pcm16_free(&input);
    return status;
}

static void direct_convolution(const int16_t *input, size_t input_frames,
        const int16_t *left, const int16_t *right, int16_t *output)
{
    size_t n;
    size_t output_frames = input_frames + SPATIAL_HISTORY_LENGTH;

    for (n = 0u; n < output_frames; n++) {
        int64_t left_acc = 0;
        int64_t right_acc = 0;
        size_t k;

        for (k = 0u; k < SPATIAL_HRIR_LENGTH; k++) {
            if ((n >= k) && ((n - k) < input_frames)) {
                int16_t sample = input[n - k];
                left_acc += (int64_t)sample * (int64_t)left[k];
                right_acc += (int64_t)sample * (int64_t)right[k];
            }
        }
        output[2u * n] = q15_reference_result(left_acc);
        output[(2u * n) + 1u] = q15_reference_result(right_acc);
    }
}

static int run_selftest(const char *left_path, const char *right_path)
{
    WavPcm16 left = {0};
    WavPcm16 right = {0};
    SpatialFirState state;
    int16_t impulse[SPATIAL_BLOCK_FRAMES] = {0};
    int16_t impulse_output[SPATIAL_BLOCK_FRAMES * 2u];
    int16_t *input = NULL;
    int16_t *expected = NULL;
    int16_t *actual = NULL;
    int16_t tail[SPATIAL_HISTORY_LENGTH] = {0};
    const size_t input_frames = 4097u;
    const size_t output_frames = input_frames + SPATIAL_HISTORY_LENGTH;
    const size_t chunks[] = {1u, 17u, 511u, 1024u, 73u};
    uint32_t rng = 0x12345678u;
    size_t i;
    size_t offset;
    size_t chunk_index;
    int status = 1;

    if (load_hrir_pair(left_path, right_path, &left, &right) < 0) {
        return 1;
    }

    impulse[0] = INT16_MAX;
    spatial_fir_reset(&state);
    if (spatial_fir_process(&state, impulse, SPATIAL_BLOCK_FRAMES,
                left.samples, right.samples, impulse_output) < 0) {
        fprintf(stderr, "selftest FAIL: impulse processing failed\n");
        goto leave;
    }
    for (i = 0u; i < SPATIAL_BLOCK_FRAMES; i++) {
        int16_t expected_left = 0;
        int16_t expected_right = 0;
        if (i < SPATIAL_HRIR_LENGTH) {
            expected_left = q15_reference_result(
                    (int64_t)INT16_MAX * left.samples[i]);
            expected_right = q15_reference_result(
                    (int64_t)INT16_MAX * right.samples[i]);
        }
        if ((impulse_output[2u * i] != expected_left) ||
                (impulse_output[(2u * i) + 1u] != expected_right)) {
            fprintf(stderr,
                    "selftest FAIL: impulse mismatch at frame %zu\n", i);
            goto leave;
        }
    }
    printf("selftest PASS: impulse response keeps natural HRIR order\n");

    input = (int16_t *)malloc(input_frames * sizeof(int16_t));
    expected = (int16_t *)malloc(output_frames * 2u * sizeof(int16_t));
    actual = (int16_t *)malloc(output_frames * 2u * sizeof(int16_t));
    if ((input == NULL) || (expected == NULL) || (actual == NULL)) {
        fprintf(stderr, "selftest FAIL: out of memory\n");
        goto leave;
    }
    for (i = 0u; i < input_frames; i++) {
        rng = (rng * 1664525u) + 1013904223u;
        input[i] = (int16_t)(((int32_t)(rng >> 20) & 0xfff) - 2048);
    }

    direct_convolution(input, input_frames, left.samples, right.samples,
            expected);
    spatial_fir_reset(&state);
    offset = 0u;
    chunk_index = 0u;
    while (offset < input_frames) {
        size_t count = chunks[chunk_index %
                (sizeof(chunks) / sizeof(chunks[0]))];
        if (count > (input_frames - offset)) {
            count = input_frames - offset;
        }
        if (spatial_fir_process(&state, input + offset, count,
                    left.samples, right.samples,
                    actual + (2u * offset)) < 0) {
            fprintf(stderr, "selftest FAIL: segmented processing failed\n");
            goto leave;
        }
        offset += count;
        chunk_index++;
    }
    if (spatial_fir_process(&state, tail, SPATIAL_HISTORY_LENGTH,
                left.samples, right.samples,
                actual + (2u * input_frames)) < 0) {
        fprintf(stderr, "selftest FAIL: tail processing failed\n");
        goto leave;
    }
    for (i = 0u; i < output_frames * 2u; i++) {
        if (actual[i] != expected[i]) {
            fprintf(stderr,
                    "selftest FAIL: block continuity mismatch at sample %zu "
                    "(actual=%d expected=%d)\n",
                    i, actual[i], expected[i]);
            goto leave;
        }
    }
    printf("selftest PASS: variable-size blocks equal direct convolution\n");
    printf("selftest PASS: 511-sample tail flush is correct\n");
    status = 0;

leave:
    free(actual);
    free(expected);
    free(input);
    wav_pcm16_free(&right);
    wav_pcm16_free(&left);
    return status;
}

static int compare_files(const char *actual_path, const char *expected_path,
        int tolerance)
{
    WavPcm16 actual = {0};
    WavPcm16 expected = {0};
    char error[ERROR_SIZE];
    size_t sample_count;
    size_t i;
    uint64_t absolute_sum = 0u;
    int maximum = 0;
    size_t above_tolerance = 0u;
    int status = 1;

    if (wav_pcm16_read(actual_path, &actual, error, sizeof(error)) < 0) {
        fprintf(stderr, "error: cannot read actual WAV: %s\n", error);
        goto leave;
    }
    if (wav_pcm16_read(expected_path, &expected, error, sizeof(error)) < 0) {
        fprintf(stderr, "error: cannot read expected WAV: %s\n", error);
        goto leave;
    }
    if ((actual.sample_rate != expected.sample_rate) ||
            (actual.channels != expected.channels) ||
            (actual.frame_count != expected.frame_count)) {
        fprintf(stderr,
                "compare FAIL: format/length mismatch\n"
                "  actual  : %u Hz, %u channel(s), %zu frames\n"
                "  expected: %u Hz, %u channel(s), %zu frames\n",
                actual.sample_rate, actual.channels, actual.frame_count,
                expected.sample_rate, expected.channels,
                expected.frame_count);
        goto leave;
    }

    sample_count = actual.frame_count * actual.channels;
    for (i = 0u; i < sample_count; i++) {
        int difference = (int)actual.samples[i] - (int)expected.samples[i];
        int absolute = difference < 0 ? -difference : difference;
        absolute_sum += (uint64_t)absolute;
        if (absolute > maximum) {
            maximum = absolute;
        }
        if (absolute > tolerance) {
            above_tolerance++;
        }
    }

    printf("compare %s\n", above_tolerance == 0u ? "PASS" : "FAIL");
    printf("  samples             : %zu\n", sample_count);
    printf("  max absolute error  : %d LSB\n", maximum);
    printf("  mean absolute error : %.6f LSB\n",
            sample_count == 0u ? 0.0 :
            (double)absolute_sum / (double)sample_count);
    printf("  over tolerance (%d) : %zu\n", tolerance, above_tolerance);
    status = above_tolerance == 0u ? 0 : 2;

leave:
    wav_pcm16_free(&expected);
    wav_pcm16_free(&actual);
    return status;
}

static void print_usage(const char *program)
{
    printf("Usage:\n");
    printf("  %s selftest LEFT_HRIR.wav RIGHT_HRIR.wav\n", program);
    printf("  %s render INPUT_PCM16.wav LEFT_HRIR.wav RIGHT_HRIR.wav "
           "OUTPUT.wav [block_frames]\n", program);
    printf("  %s compare ACTUAL.wav EXPECTED.wav [tolerance_lsb]\n",
            program);
}

int main(int argc, char **argv)
{
    if ((argc == 4) && (strcmp(argv[1], "selftest") == 0)) {
        return run_selftest(argv[2], argv[3]);
    }
    if (((argc == 6) || (argc == 7)) &&
            (strcmp(argv[1], "render") == 0)) {
        size_t block_frames = SPATIAL_BLOCK_FRAMES;
        if ((argc == 7) &&
                (parse_size(argv[6], &block_frames, 0) < 0)) {
            fprintf(stderr, "error: invalid block_frames '%s'\n", argv[6]);
            return 1;
        }
        return render_file(argv[2], argv[3], argv[4], argv[5],
                block_frames);
    }
    if (((argc == 4) || (argc == 5)) &&
            (strcmp(argv[1], "compare") == 0)) {
        size_t tolerance = 1u;
        if ((argc == 5) && (parse_size(argv[4], &tolerance, 1) < 0)) {
            fprintf(stderr, "error: invalid tolerance '%s'\n", argv[4]);
            return 1;
        }
        if (tolerance > (size_t)INT_MAX) {
            fprintf(stderr, "error: tolerance is too large\n");
            return 1;
        }
        return compare_files(argv[2], argv[3], (int)tolerance);
    }

    print_usage(argc > 0 ? argv[0] : "spatial_q15");
    return 1;
}
