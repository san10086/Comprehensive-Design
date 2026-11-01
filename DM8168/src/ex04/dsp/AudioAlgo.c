/*
 * AudioAlgo.c
 *
 * DSP algorithm adapter. Existing demonstration modes remain available; the
 * spatial modes downmix stereo input and apply the selected binaural HRIR.
 */

#include "AudioAlgo.h"
#include "dsp_algorithm.h"

Void AudioAlgo_init(AudioAlgoContext *ctx)
{
    AudioAlgoConfig cfg;
    AudioAlgoParams params;
    UInt32 i;

    if (ctx == NULL) {
        return;
    }

    cfg.sampleRate = 8000;
    cfg.channels = 2;
    cfg.bitDepth = 16;
    cfg.frameBytes = 4;
    cfg.maxFrameCount = 128;
    for (i = 0; i < AUDIO_ALGO_RESERVED_WORDS; i++) {
        cfg.reserved[i] = 0;
    }

    params.mode = AUDIO_ALGO_MODE_PASSTHROUGH;
    params.flags = AUDIO_ALGO_FLAG_BYPASS;
    params.gainQ15 = AUDIO_ALGO_GAIN_UNITY_Q15;
    params.directionA = -1;
    params.directionB = -1;
    for (i = 0; i < AUDIO_ALGO_RESERVED_WORDS; i++) {
        params.reserved[i] = 0;
    }

    ctx->userState = NULL;
    (void)AudioAlgo_configure(ctx, &cfg);
    (void)AudioAlgo_setParams(ctx, &params);
}

Int AudioAlgo_configure(AudioAlgoContext *ctx, const AudioAlgoConfig *cfg)
{
    if ((ctx == NULL) || (cfg == NULL) ||
            (cfg->sampleRate == 0) ||
            (cfg->channels == 0) ||
            (cfg->bitDepth == 0) ||
            ((cfg->bitDepth % 8) != 0) ||
            (cfg->frameBytes != (UInt16)(cfg->channels *
                    (cfg->bitDepth / 8)))) {
        return (-1);
    }

    ctx->config = *cfg;
    AudioAlgo_reset(ctx);
    return (0);
}

Int AudioAlgo_setParams(AudioAlgoContext *ctx, const AudioAlgoParams *params)
{
    if ((ctx == NULL) || (params == NULL)) {
        return (-1);
    }
    if (params->mode > APP_ALGO_MODE_MAX) {
        return (-1);
    }
    if (((params->mode == AUDIO_ALGO_MODE_SPATIAL_40E045) ||
         (params->mode == AUDIO_ALGO_MODE_SPATIAL_60E250) ||
         (params->mode == AUDIO_ALGO_MODE_ORBIT) ||
         (params->mode == AUDIO_ALGO_MODE_DUAL_60_40) ||
         (params->mode == AUDIO_ALGO_MODE_SPATIAL) ||
         (params->mode == AUDIO_ALGO_MODE_DUAL)) &&
            ((ctx->config.sampleRate != DSP_SPATIAL_SAMPLE_RATE) ||
             (ctx->config.channels != 2) ||
             (ctx->config.bitDepth != 16))) {
        return (-1);
    }

    if (((params->mode == AUDIO_ALGO_MODE_SPATIAL) ||
         (params->mode == AUDIO_ALGO_MODE_DUAL)) &&
            ((hrir_direction_index(params->directionA) < 0) ||
             ((params->mode == AUDIO_ALGO_MODE_DUAL) &&
              (hrir_direction_index(params->directionB) < 0)))) {
        return (-1);
    }
    ctx->params = *params;
    dsp_spatial_reset(&ctx->spatialState);
    return (0);
}

Void AudioAlgo_reset(AudioAlgoContext *ctx)
{
    UInt32 i;

    if (ctx == NULL) {
        return;
    }

    ctx->stats.blocksProcessed = 0;
    ctx->stats.framesProcessed = 0;
    ctx->stats.samplesProcessed = 0;
    ctx->stats.lastFrameCount = 0;
    ctx->stats.lastStatus = 0;
    for (i = 0; i < AUDIO_ALGO_RESERVED_WORDS; i++) {
        ctx->stats.reserved[i] = 0;
    }
    dsp_spatial_reset(&ctx->spatialState);
}

Void AudioAlgo_getStats(const AudioAlgoContext *ctx, AudioAlgoStats *stats)
{
    if ((ctx == NULL) || (stats == NULL)) {
        return;
    }

    *stats = ctx->stats;
}

Int AudioAlgo_process(AudioAlgoContext *ctx, AudioAlgoFrame *frame)
{
    UInt32 i;
    const Char *inputBytes;
    Char *outputBytes;
    UInt32 bytesPerSample;
    Int algorithmMode;
    Int status;

    if ((ctx == NULL) || (frame == NULL) ||
            (frame->inputData == NULL) || (frame->outputData == NULL) ||
            (frame->channels != ctx->config.channels) ||
            (frame->bitDepth != ctx->config.bitDepth) ||
            (frame->sampleRate != ctx->config.sampleRate) ||
            (frame->byteCount != (frame->frameCount *
                    ctx->config.frameBytes)) ||
            (frame->sampleCount != (frame->frameCount *
                    ctx->config.channels)) ||
            ((ctx->config.maxFrameCount != 0) &&
                    (frame->frameCount > ctx->config.maxFrameCount))) {
        if (ctx != NULL) {
            ctx->stats.lastStatus = -1;
        }
        return (-1);
    }

    bytesPerSample = (UInt32)(ctx->config.bitDepth / 8);
    if ((bytesPerSample == 0) ||
            (frame->sampleCount != (frame->byteCount / bytesPerSample))) {
        ctx->stats.lastStatus = -1;
        return (-1);
    }

    /*
     * Keep the old frame contract: copy first when input/output are separate,
     * then run S16_LE stereo algorithms in-place on outputPcm when requested.
     */
    if (frame->outputData != frame->inputData) {
        inputBytes = (const Char *)frame->inputData;
        outputBytes = (Char *)frame->outputData;
        for (i = 0; i < frame->byteCount; i++) {
            outputBytes[i] = inputBytes[i];
        }
    }

    algorithmMode = (Int)ctx->params.mode;
    if ((ctx->params.flags & AUDIO_ALGO_FLAG_BYPASS) != 0) {
        algorithmMode = AUDIO_ALGO_MODE_PASSTHROUGH;
    }

    if (algorithmMode == AUDIO_ALGO_MODE_PASSTHROUGH) {
        status = 0;
    }
    else if ((frame->channels != 2) || (frame->bitDepth != 16) ||
            (frame->outputPcm == NULL)) {
        status = -1;
    }
    else if (algorithmMode == AUDIO_ALGO_MODE_SPATIAL_40E045) {
        if (frame->sampleRate != DSP_SPATIAL_SAMPLE_RATE) {
            status = -1;
        }
        else {
            status = dsp_spatial_process_inplace(&ctx->spatialState,
                    frame->outputPcm, (int)frame->frameCount,
                    DSP_SPATIAL_DIRECTION_40E045);
        }
    }
    else if (algorithmMode == AUDIO_ALGO_MODE_SPATIAL_60E250) {
        if (frame->sampleRate != DSP_SPATIAL_SAMPLE_RATE) {
            status = -1;
        }
        else {
            status = dsp_spatial_process_inplace(&ctx->spatialState,
                    frame->outputPcm, (int)frame->frameCount,
                    DSP_SPATIAL_DIRECTION_60E250);
        }
    }
    else if (algorithmMode == AUDIO_ALGO_MODE_ORBIT) {
        if (frame->sampleRate != DSP_SPATIAL_SAMPLE_RATE) {
            status = -1;
        }
        else {
            status = dsp_spatial_process_orbit_inplace(&ctx->spatialState,
                    frame->outputPcm, (int)frame->frameCount);
        }
    }
    else if (algorithmMode == AUDIO_ALGO_MODE_DUAL_60_40) {
        if (frame->sampleRate != DSP_SPATIAL_SAMPLE_RATE) {
            status = -1;
        }
        else {
            status = dsp_spatial_process_dual_inplace(&ctx->spatialState,
                    frame->outputPcm, (int)frame->frameCount);
        }
    }
    else if (algorithmMode == AUDIO_ALGO_MODE_SPATIAL) {
        status = dsp_spatial_process_inplace(&ctx->spatialState,
                frame->outputPcm, (int)frame->frameCount,
                (int)ctx->params.directionA);
    }
    else if (algorithmMode == AUDIO_ALGO_MODE_DUAL) {
        status = dsp_spatial_process_dual_directions_inplace(&ctx->spatialState,
                frame->outputPcm, (int)frame->frameCount,
                (int)ctx->params.directionA, (int)ctx->params.directionB);
    }
    else {
        status = dsp_process_audio(frame->outputPcm,
                (int)frame->frameCount, (int)algorithmMode);
    }

    if (status < 0) {
        ctx->stats.lastStatus = -1;
        return (-1);
    }

    ctx->stats.blocksProcessed++;
    ctx->stats.framesProcessed += frame->frameCount;
    ctx->stats.samplesProcessed += frame->sampleCount;
    ctx->stats.lastFrameCount = frame->frameCount;
    ctx->stats.lastStatus = 0;
    return (0);
}
