/*
 * AudioAlgo.h
 *
 * DSP-side audio algorithm interface. The SysLink server calls this module
 * with one interleaved PCM block at a time. inputPcm/outputPcm are convenience
 * views for the current S16_LE path; inputData/outputData keep the contract
 * open for wider sample formats.
 */

#ifndef AUDIO_ALGO_H
#define AUDIO_ALGO_H

#include <xdc/std.h>
#include "../shared/AppCommon.h"
#include "dsp_spatial.h"

#define AUDIO_ALGO_MODE_PASSTHROUGH      APP_ALGO_MODE_PASSTHROUGH
#define AUDIO_ALGO_MODE_SWAP_STEREO      APP_ALGO_MODE_SWAP_STEREO
#define AUDIO_ALGO_MODE_GAIN_X2          APP_ALGO_MODE_GAIN_X2
#define AUDIO_ALGO_MODE_SPEED2           APP_ALGO_MODE_SPEED2
#define AUDIO_ALGO_MODE_GAIN_X3          APP_ALGO_MODE_GAIN_X3
#define AUDIO_ALGO_MODE_GAIN_X5          APP_ALGO_MODE_GAIN_X5
#define AUDIO_ALGO_MODE_SPATIAL_40E045   APP_ALGO_MODE_SPATIAL_40E045
#define AUDIO_ALGO_MODE_SPATIAL_60E250   APP_ALGO_MODE_SPATIAL_60E250
#define AUDIO_ALGO_MODE_ORBIT             APP_ALGO_MODE_ORBIT
#define AUDIO_ALGO_MODE_DUAL_60_40        APP_ALGO_MODE_DUAL_60_40
#define AUDIO_ALGO_MODE_SPATIAL           APP_ALGO_MODE_SPATIAL
#define AUDIO_ALGO_MODE_DUAL              APP_ALGO_MODE_DUAL
#define AUDIO_ALGO_FLAG_BYPASS           0x00000001u
#define AUDIO_ALGO_GAIN_UNITY_Q15        32768
#define AUDIO_ALGO_RESERVED_WORDS        8

typedef struct {
    UInt32 sampleRate;
    UInt16 channels;
    UInt16 bitDepth;
    UInt16 frameBytes;
    UInt32 maxFrameCount;
    UInt32 reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoConfig;

typedef struct {
    UInt32 mode;
    UInt32 flags;
    Int32  gainQ15;
    Int32  directionA;
    Int32  directionB;
    Int32  reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoParams;

typedef struct {
    const Void  *inputData;
    Void        *outputData;
    const Int16 *inputPcm;
    Int16       *outputPcm;
    Void        *workBuffer;
    UInt32       byteCount;
    UInt32       frameCount;
    UInt32       sampleCount;
    UInt32       sequence;
    UInt32       workBytes;
    UInt16       channels;
    UInt16       bitDepth;
    UInt32       sampleRate;
    UInt32       flags;
    UInt32       reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoFrame;

typedef struct {
    UInt32 blocksProcessed;
    UInt32 framesProcessed;
    UInt32 samplesProcessed;
    UInt32 lastFrameCount;
    Int    lastStatus;
    UInt32 reserved[AUDIO_ALGO_RESERVED_WORDS];
} AudioAlgoStats;

typedef struct {
    AudioAlgoConfig config;
    AudioAlgoParams params;
    AudioAlgoStats  stats;
    DspSpatialState spatialState;
    Void           *userState;
} AudioAlgoContext;

Void AudioAlgo_init(AudioAlgoContext *ctx);
Int AudioAlgo_configure(AudioAlgoContext *ctx, const AudioAlgoConfig *cfg);
Int AudioAlgo_setParams(AudioAlgoContext *ctx, const AudioAlgoParams *params);
Void AudioAlgo_reset(AudioAlgoContext *ctx);
Void AudioAlgo_getStats(const AudioAlgoContext *ctx, AudioAlgoStats *stats);
Int AudioAlgo_process(AudioAlgoContext *ctx, AudioAlgoFrame *frame);

#endif /* AUDIO_ALGO_H */
