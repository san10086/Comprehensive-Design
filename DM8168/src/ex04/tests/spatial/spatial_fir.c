#include "spatial_fir.h"

#include <limits.h>
#include <string.h>

static int16_t q15_accumulate_result(int64_t accumulator)
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

void spatial_fir_reset(SpatialFirState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

int spatial_fir_process(SpatialFirState *state,
        const int16_t *input_mono, size_t frame_count,
        const int16_t hrir_left[SPATIAL_HRIR_LENGTH],
        const int16_t hrir_right[SPATIAL_HRIR_LENGTH],
        int16_t *output_stereo)
{
    size_t n;

    if ((state == NULL) || (input_mono == NULL) ||
            (hrir_left == NULL) || (hrir_right == NULL) ||
            (output_stereo == NULL)) {
        return -1;
    }

    for (n = 0u; n < frame_count; n++) {
        int64_t left_acc = 0;
        int64_t right_acc = 0;
        size_t k;

        for (k = 0u; k < SPATIAL_HRIR_LENGTH; k++) {
            int16_t sample;

            if (k <= n) {
                sample = input_mono[n - k];
            }
            else {
                size_t history_index = SPATIAL_HISTORY_LENGTH + n - k;
                sample = state->history[history_index];
            }

            left_acc += (int64_t)sample * (int64_t)hrir_left[k];
            right_acc += (int64_t)sample * (int64_t)hrir_right[k];
        }

        output_stereo[2u * n] = q15_accumulate_result(left_acc);
        output_stereo[(2u * n) + 1u] = q15_accumulate_result(right_acc);
    }

    if (frame_count >= SPATIAL_HISTORY_LENGTH) {
        memcpy(state->history,
                input_mono + frame_count - SPATIAL_HISTORY_LENGTH,
                sizeof(state->history));
    }
    else if (frame_count > 0u) {
        size_t retained = SPATIAL_HISTORY_LENGTH - frame_count;
        memmove(state->history, state->history + frame_count,
                retained * sizeof(state->history[0]));
        memcpy(state->history + retained, input_mono,
                frame_count * sizeof(input_mono[0]));
    }

    return 0;
}
