/*
 * main.c
 *
 * Mature ARM-side entry point. It uses host/syslink/syslinkDsp.h instead of
 * touching SysLink Notify/SharedRegion directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "../../shared/AppCommon.h"
#include "../../shared/hrir_catalog.h"
#include "../audio/audioConfig.h"
#include "../audio/audioCapture.h"
#include "../audio/audioPlayback.h"
#include "../audio/audioWav.h"
#include "../syslink/syslinkDsp.h"

#define APP_FRAME_BYTES 4
#define APP_DEFAULT_AUDIO_SECONDS 5
#define APP_AUDIO_ASYNC_BLOCKS 4

typedef struct {
    short *inPcm;
    short *outPcm;
    int    frameCount;
    int    payloadBytes;
} audioAsyncBlock;

typedef struct audioPipelineContext audioPipelineContext;

static void fillPcm(short *pcm, int sampleCount, int round);
static int verifyPassthrough(short *inPcm, short *outPcm, int byteCount);
static int verifyAlgorithmOutput(int algorithmMode, short *inPcm,
        short *outPcm, int frameCount);
static short saturatingMultiply(short value, int factor);
static void dumpBytes(const char *label, short *pcm, int byteCount);
static int runSmokeTest(const char *remoteProcName, int algorithmMode);
static int runAudioPipeline(const char *remoteProcName, int durationSec,
        const char *outFileName, int algorithmMode, int directionA, int directionB);
static int runRecordPipeline(const char *fileName, int durationSec);
static int runFilePipeline(const char *remoteProcName, const char *fileName,
        const char *outFileName, int algorithmMode, int directionA, int directionB);
static int audioPipelineCallback(short *pcmData, int frameCount, void *userData);
static int recordCallback(short *pcmData, int frameCount, void *userData);
static int audioAsyncStart(audioPipelineContext *ctx, int blockBytes);
static void audioAsyncStop(audioPipelineContext *ctx);
static int audioAsyncSubmit(audioPipelineContext *ctx, short *pcmData,
        int frameCount, int payloadBytes);
static void *audioAsyncWorker(void *arg);
static void audioAsyncAbort(audioPipelineContext *ctx, int activeIndex);
static void audioAsyncReturnReadyBlocks(audioPipelineContext *ctx);
static void audioAsyncQueuePush(int *queue, int *tail, int *count, int value);
static int audioAsyncQueuePop(int *queue, int *head, int *count);
static void handleSignal(int signum);
static void printUsage(const char *prog);
static int parseAlgorithmMode(const char *text, int *algorithmMode);
static int parseDurationSec(const char *text, int *durationSec);
static const char *algorithmModeName(int algorithmMode);
static int isAppMode(const char *text);
static int parseNamedOptions(int argc, char **argv, int argi, const char *mode,
        const char **input, const char **output, int *seconds, int *algorithm,
        int *directionA, int *directionB);

typedef struct audioPipelineContext {
    int    outBytes;
    int    frameBytes;
    int    blocks;
    int    status;
    int    asyncReady;
    int    workerStarted;
    int    stopping;
    int    droppedBlocks;
    audioAsyncBlock blocksPool[APP_AUDIO_ASYNC_BLOCKS];
    int    freeQueue[APP_AUDIO_ASYNC_BLOCKS];
    int    readyQueue[APP_AUDIO_ASYNC_BLOCKS];
    int    freeHead;
    int    freeTail;
    int    freeCount;
    int    readyHead;
    int    readyTail;
    int    readyCount;
    pthread_t worker;
    pthread_mutex_t lock;
    pthread_cond_t freeCond;
    pthread_cond_t readyCond;
    int    pbReady;
    int    saveOutput;
    audioConfig pbCfg;
    audioWavFile wav;
} audioPipelineContext;

typedef struct {
    audioWavFile wav;
    int          blocks;
    int          status;
    int          frameBytes;
} recordPipelineContext;

static volatile sig_atomic_t gStopRequested = 0;

int main(int argc, char *argv[])
{
    const char *remoteProcName = "DSP";
    const char *mode = "smoke";
    const char *modeArg = NULL;
    const char *modeArg2 = NULL;
    int         argi = 1;
    int         durationSec = APP_DEFAULT_AUDIO_SECONDS;
    int         algorithmMode = APP_ALGO_MODE_PASSTHROUGH;
    int         audioAlgorithmSet = 0;
    int         parsedAlgorithmMode = 0;
    int         directionA = -1;
    int         directionB = -1;
    int         checkOnly = 0;
    int         scan;

    for (scan = 1; scan < argc; scan++) {
        if (strcmp(argv[scan], "--check") == 0) {
            int move;
            if (checkOnly) return (1);
            checkOnly = 1;
            for (move = scan; move < argc - 1; move++) argv[move] = argv[move + 1];
            argc--;
            scan--;
        }
    }
    if ((argc == 2) && ((strcmp(argv[1], "--help") == 0) ||
            (strcmp(argv[1], "-h") == 0))) {
        printUsage(argv[0]);
        return (0);
    }

    /* Flush pipe logs promptly and install cooperative stop before init. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGINT, handleSignal);
    printf("main: control ready\n");

    if ((argi < argc) && !isAppMode(argv[argi])) {
        if (argv[argi][0] == '-') {
            printUsage(argv[0]);
            return (1);
        }
        remoteProcName = argv[argi++];
    }

    if (argi < argc) {
        if (!isAppMode(argv[argi])) {
            printUsage(argc > 0 ? argv[0] : "app_host");
            return (1);
        }
        mode = argv[argi++];
    }

    if (strcmp(mode, "directions") == 0) {
        if (argi != argc) return (1);
        printf("HRIR directions: %d (elevation, azimuth, code)\n", HRIR_DIRECTION_COUNT);
        for (scan = 0; scan < HRIR_DIRECTION_COUNT; scan++) {
            printf("%d %d %d\n", kHrirDirections[scan].elevation,
                    kHrirDirections[scan].azimuth, kHrirDirections[scan].code);
        }
        return (0);
    }

    if ((argi < argc) && (strncmp(argv[argi], "--", 2) != 0)) {
        for (scan = argi; scan < argc; scan++) {
            if (strncmp(argv[scan], "--", 2) == 0) {
                printf("main: do not mix positional arguments with named options\n");
                return (1);
            }
        }
    }
    if ((argi < argc) && (strncmp(argv[argi], "--", 2) == 0)) {
        const char *input = NULL;
        const char *output = NULL;
        if (parseNamedOptions(argc, argv, argi, mode, &input, &output,
                &durationSec, &algorithmMode, &directionA, &directionB) < 0) {
            printUsage(argv[0]);
            return (1);
        }
        modeArg = (strcmp(mode, "file") == 0) ? input : output;
        modeArg2 = output;
        argi = argc;
    }
    else if (strcmp(mode, "smoke") == 0) {
        if (argi < argc) {
            if (parseAlgorithmMode(argv[argi++], &algorithmMode) < 0) {
                printUsage(argc > 0 ? argv[0] : "app_host");
                return (1);
            }
        }
    }
    else if (strcmp(mode, "audio") == 0) {
        if (argi < argc) {
            if (parseDurationSec(argv[argi], &durationSec) == 0) {
                argi++;
            }
            else if ((argv[argi][0] != '\0') &&
                    (strspn(argv[argi], "0123456789") == strlen(argv[argi]))) {
                printf("main: seconds must be in range 0..2147\n");
                return (1);
            }
        }

        while (argi < argc) {
            if (parseAlgorithmMode(argv[argi], &parsedAlgorithmMode) == 0) {
                if (audioAlgorithmSet != 0) {
                    printUsage(argc > 0 ? argv[0] : "app_host");
                    return (1);
                }
                algorithmMode = parsedAlgorithmMode;
                audioAlgorithmSet = 1;
                argi++;
            }
            else if (modeArg == NULL) {
                modeArg = argv[argi++];
            }
            else {
                printUsage(argc > 0 ? argv[0] : "app_host");
                return (1);
            }
        }
    }
    else if (strcmp(mode, "record") == 0) {
        if (argi >= argc) {
            printUsage(argc > 0 ? argv[0] : "app_host");
            return (1);
        }
        modeArg = argv[argi++];
        if (argi < argc) {
            if (parseDurationSec(argv[argi++], &durationSec) < 0) {
                printUsage(argc > 0 ? argv[0] : "app_host");
                return (1);
            }
        }
    }
    else if (strcmp(mode, "file") == 0) {
        if (argi >= argc) {
            printUsage(argc > 0 ? argv[0] : "app_host");
            return (1);
        }
        modeArg = argv[argi++];
        if (argi < argc) {
            if (parseAlgorithmMode(argv[argi], &algorithmMode) == 0) {
                argi++;
            }
            else {
                modeArg2 = argv[argi++];
                if (argi < argc) {
                    if (parseAlgorithmMode(argv[argi++],
                            &algorithmMode) < 0) {
                        printUsage(argc > 0 ? argv[0] : "app_host");
                        return (1);
                    }
                }
            }
        }
    }

    if (argi != argc) {
        printUsage(argc > 0 ? argv[0] : "app_host");
        return (1);
    }

    if (durationSec < 0) {
        durationSec = 0;
    }

    if ((strcmp(mode, "audio") == 0) &&
            (algorithmMode == APP_ALGO_MODE_SPEED2)) {
        printUsage(argc > 0 ? argv[0] : "app_host");
        return (1);
    }
    if ((strcmp(mode, "smoke") == 0) &&
            ((algorithmMode == APP_ALGO_MODE_SPATIAL_40E045) ||
             (algorithmMode == APP_ALGO_MODE_SPATIAL_60E250) ||
             (algorithmMode == APP_ALGO_MODE_ORBIT) ||
             (algorithmMode == APP_ALGO_MODE_DUAL_60_40) ||
             (algorithmMode == APP_ALGO_MODE_SPATIAL) ||
             (algorithmMode == APP_ALGO_MODE_DUAL))) {
        printf("main: spatial modes require 44100-Hz file or audio mode\n");
        return (1);
    }

    if (checkOnly) {
        printf("check PASS: mode=%s algo=%s directionA=%d directionB=%d\n",
                mode, algorithmModeName(algorithmMode), directionA, directionB);
        return (0);
    }

    if (strcmp(mode, "smoke") == 0) {
        return (runSmokeTest(remoteProcName, algorithmMode));
    }

    if (strcmp(mode, "audio") == 0) {
        return (runAudioPipeline(remoteProcName, durationSec, modeArg,
                algorithmMode, directionA, directionB));
    }

    if (strcmp(mode, "record") == 0) {
        return (runRecordPipeline(modeArg, durationSec));
    }

    if (strcmp(mode, "file") == 0) {
        return (runFilePipeline(remoteProcName, modeArg, modeArg2,
                algorithmMode, directionA, directionB));
    }

    printUsage(argc > 0 ? argv[0] : "app_host");
    return (1);
}

static int runSmokeTest(const char *remoteProcName, int algorithmMode)
{
    syslinkDspConfig cfg;
    short inPcm[APP_TEST_DATA_SIZE / sizeof(short)];
    short outPcm[APP_TEST_DATA_SIZE / sizeof(short)];
    int frameCount = APP_TEST_DATA_SIZE / APP_FRAME_BYTES;
    int sampleCount = APP_TEST_DATA_SIZE / sizeof(short);
    int round;
    int status = 0;

    memset(&cfg, 0, sizeof(cfg));
    cfg.remoteProcName = remoteProcName;
    cfg.frameBytes = APP_FRAME_BYTES;
    cfg.bufferBytes = APP_DEFAULT_BUFFER_SIZE;
    cfg.algorithmMode = algorithmMode;
    cfg.directionA = -1;
    cfg.directionB = -1;

    printf("--> main: remote=%s, algorithm=%s\n",
            remoteProcName, algorithmModeName(algorithmMode));

    if (syslinkDspInit(&cfg) < 0) {
        printf("main: syslinkDspInit failed\n");
        return (1);
    }

    for (round = 0; round < APP_TEST_ROUNDS && !gStopRequested; round++) {
        fillPcm(inPcm, sampleCount, round);

        printf("main: round %d, frameCount=%d\n", round, frameCount);
        dumpBytes("main: before DSP", inPcm, APP_TEST_DATA_SIZE);

        if (syslinkDspProcessFrames(inPcm, outPcm, frameCount) < 0) {
            printf("main: syslinkDspProcessFrames failed\n");
            status = 1;
            break;
        }

        dumpBytes("main: after DSP ", outPcm, APP_TEST_DATA_SIZE);

        if (verifyAlgorithmOutput(algorithmMode, inPcm, outPcm,
                frameCount) < 0) {
            printf("main: DSP algorithm verify failed\n");
            status = 1;
            break;
        }
    }

    if (syslinkDspClose() < 0) {
        printf("main: syslinkDspClose failed\n");
        status = 1;
    }

    printf("<-- main: status=%d\n", status);
    return (status);
}

static int runAudioPipeline(const char *remoteProcName, int durationSec,
        const char *outFileName, int algorithmMode, int directionA, int directionB)
{
    audioConfig cfg;
    audioConfig pbCfg;
    syslinkDspConfig dspCfg;
    audioPipelineContext ctx;
    int durationMs;
    int status = 0;

    memset(&ctx, 0, sizeof(ctx));
    cfg = audioGetConfig();
    ctx.saveOutput = (outFileName != NULL);

    printf("--> main: audio pipeline, remote=%s, seconds=%d, algorithm=%s\n",
            remoteProcName, durationSec, algorithmModeName(algorithmMode));
    printf("main: requested audio cfg capture=%s playback=%s rate=%u ch=%d period=%d\n",
            cfg.devCapture, cfg.devPlayback, cfg.sampleRate, cfg.channels,
            cfg.periodFrames);

    if (audioCapInit(&cfg) < 0) {
        printf("main: audioCapInit failed\n");
        return (1);
    }

    printf("main: actual capture cfg rate=%u ch=%d bit=%d period=%d payload=%d\n",
            cfg.sampleRate, cfg.channels, cfg.bitDepth, cfg.periodFrames,
            cfg.bufferBytes);

    pbCfg = cfg;
    if (audioPbInit(&pbCfg) < 0) {
        printf("main: audioPbInit failed\n");
        audioPbClose();
        status = 1;
        goto leave;
    }
    ctx.pbReady = 1;

    printf("main: actual playback cfg rate=%u ch=%d bit=%d period=%d payload=%d\n",
            pbCfg.sampleRate, pbCfg.channels, pbCfg.bitDepth,
            pbCfg.periodFrames, pbCfg.bufferBytes);

    if ((pbCfg.sampleRate != cfg.sampleRate) ||
            (pbCfg.channels != cfg.channels) ||
            (pbCfg.bitDepth != cfg.bitDepth)) {
        printf("main: capture/playback format mismatch: capture rate=%u ch=%d bit=%d, playback rate=%u ch=%d bit=%d\n",
                cfg.sampleRate, cfg.channels, cfg.bitDepth,
                pbCfg.sampleRate, pbCfg.channels, pbCfg.bitDepth);
        status = 1;
        goto leave;
    }

    ctx.frameBytes = cfg.channels * (cfg.bitDepth / 8);
    ctx.outBytes = cfg.bufferBytes;
    ctx.pbCfg = pbCfg;

    if (ctx.saveOutput) {
        if (audioWavOpenWrite(&ctx.wav, outFileName, (int)cfg.sampleRate,
                    cfg.channels, cfg.bitDepth) < 0) {
            printf("main: failed to create audio output WAV file: %s\n",
                    outFileName);
            status = 1;
            goto leave;
        }
        printf("main: audio output=%s\n", outFileName);
    }

    memset(&dspCfg, 0, sizeof(dspCfg));
    dspCfg.remoteProcName = remoteProcName;
    dspCfg.frameBytes = ctx.frameBytes;
    dspCfg.bufferBytes = cfg.bufferBytes;
    dspCfg.sampleRate = (int)cfg.sampleRate;
    dspCfg.channels = cfg.channels;
    dspCfg.bitDepth = cfg.bitDepth;
    dspCfg.algorithmMode = algorithmMode;
    dspCfg.directionA = directionA;
    dspCfg.directionB = directionB;
    dspCfg.ringSlots = APP_AUDIO_ASYNC_BLOCKS;

    if (syslinkDspInit(&dspCfg) < 0) {
        printf("main: syslinkDspInit failed\n");
        status = 1;
        goto leave;
    }

    if (audioAsyncStart(&ctx, cfg.bufferBytes) < 0) {
        printf("main: audio async worker start failed\n");
        status = 1;
        goto leave;
    }

    if (audioCapSetCallback(audioPipelineCallback, &ctx) < 0) {
        printf("main: audioCapSetCallback failed\n");
        status = 1;
        goto leave;
    }

    durationMs = (durationSec > 0 ? durationSec * 1000 : 0);
    printf("main: starting threaded ALSA -> DSP -> ALSA pipeline\n");

    if (!gStopRequested && audioCapStart(durationMs) < 0) {
        printf("main: audioCapStart failed\n");
        status = 1;
    }

leave:
    audioAsyncStop(&ctx);
    if (ctx.status < 0) {
        status = 1;
    }
    audioCapClose();
    if (ctx.pbReady) {
        audioPbDrain();
        audioPbClose();
    }
    audioWavClose(&ctx.wav);

    if (syslinkDspClose() < 0) {
        printf("main: syslinkDspClose failed\n");
        status = 1;
    }

    printf("<-- main: audio status=%d, blocks=%d, dropped=%d\n",
            status, ctx.blocks, ctx.droppedBlocks);
    return (status);
}

static int runRecordPipeline(const char *fileName, int durationSec)
{
    audioConfig cfg;
    recordPipelineContext ctx;
    int durationMs;
    int status = 0;

    memset(&ctx, 0, sizeof(ctx));
    cfg = audioGetConfig();

    printf("--> main: record pipeline, file=%s, seconds=%d\n",
            fileName, durationSec);
    printf("main: requested record cfg capture=%s rate=%u ch=%d period=%d\n",
            cfg.devCapture, cfg.sampleRate, cfg.channels, cfg.periodFrames);

    if (audioCapInit(&cfg) < 0) {
        printf("main: audioCapInit failed\n");
        return (1);
    }

    printf("main: actual record cfg rate=%u ch=%d bit=%d period=%d payload=%d\n",
            cfg.sampleRate, cfg.channels, cfg.bitDepth, cfg.periodFrames,
            cfg.bufferBytes);
    ctx.frameBytes = cfg.channels * (cfg.bitDepth / 8);

    if (audioWavOpenWrite(&ctx.wav, fileName, (int)cfg.sampleRate,
                cfg.channels, cfg.bitDepth) < 0) {
        printf("main: failed to create WAV file: %s\n", fileName);
        status = 1;
        goto leave;
    }

    if (audioCapSetCallback(recordCallback, &ctx) < 0) {
        printf("main: audioCapSetCallback failed\n");
        status = 1;
        goto leave;
    }

    durationMs = (durationSec > 0 ? durationSec * 1000 : 0);
    if (!gStopRequested && audioCapStart(durationMs) < 0) {
        printf("main: audioCapStart failed\n");
        status = 1;
    }

    if (ctx.status < 0) {
        status = 1;
    }

leave:
    audioCapClose();
    audioWavClose(&ctx.wav);
    printf("<-- main: record status=%d, blocks=%d\n", status, ctx.blocks);
    return (status);
}

static int runFilePipeline(const char *remoteProcName, const char *fileName,
        const char *outFileName, int algorithmMode, int directionA, int directionB)
{
    audioWavFile wav;
    audioWavFile outWav;
    audioConfig cfg;
    syslinkDspConfig dspCfg;
    short *inPcm = NULL;
    short *outPcm = NULL;
    int frameBytes;
    int bufferBytes;
    int framesRead;
    int processed;
    int written;
    int inputFrames;
    int processedFrames = 0;
    int blocks = 0;
    int status = 0;
    int saveOutput = (outFileName != NULL);
    int playOutput = !saveOutput;

    memset(&wav, 0, sizeof(wav));
    memset(&outWav, 0, sizeof(outWav));

    if (audioWavOpenRead(&wav, fileName) < 0) {
        printf("main: failed to open WAV file: %s\n", fileName);
        return (1);
    }

    cfg = audioGetConfig();
    if ((wav.sampleRate != (int)cfg.sampleRate) ||
            (wav.channels != cfg.channels) ||
            (wav.bitDepth != cfg.bitDepth)) {
        printf("main: WAV format mismatch: got rate=%d ch=%d bit=%d, "
               "expected rate=%u ch=%d bit=%d\n",
               wav.sampleRate, wav.channels, wav.bitDepth,
               cfg.sampleRate, cfg.channels, cfg.bitDepth);
        audioWavClose(&wav);
        return (1);
    }

    if (wav.blockAlign != wav.channels * (wav.bitDepth / 8)) {
        printf("main: WAV blockAlign mismatch: got=%d expected=%d\n",
                wav.blockAlign, wav.channels * (wav.bitDepth / 8));
        audioWavClose(&wav);
        return (1);
    }
    inputFrames = wav.dataSize / wav.blockAlign;

    cfg.sampleRate = (unsigned int)wav.sampleRate;
    cfg.channels = wav.channels;
    cfg.bitDepth = wav.bitDepth;
    cfg.bufferBytes = cfg.periodFrames * wav.blockAlign;

    frameBytes = wav.blockAlign;
    bufferBytes = cfg.periodFrames * frameBytes;

    inPcm = (short *)malloc((size_t)bufferBytes);
    outPcm = (short *)malloc((size_t)bufferBytes);
    if ((inPcm == NULL) || (outPcm == NULL)) {
        printf("main: failed to allocate file pipeline buffers\n");
        status = 1;
        goto leave_wav;
    }

    printf("--> main: file pipeline, remote=%s, file=%s, algorithm=%s\n",
            remoteProcName, fileName, algorithmModeName(algorithmMode));
    printf("main: wav cfg rate=%d ch=%d bit=%d period=%d payload=%d\n",
            wav.sampleRate, wav.channels, wav.bitDepth,
            cfg.periodFrames, bufferBytes);

    if (saveOutput) {
        if (audioWavOpenWrite(&outWav, outFileName, wav.sampleRate,
                    wav.channels, wav.bitDepth) < 0) {
            printf("main: failed to create output WAV file: %s\n",
                    outFileName);
            status = 1;
            goto leave_wav;
        }
        printf("main: file output=%s\n", outFileName);
        printf("main: offline render selected; use aplay on the output file\n");
    }

    memset(&dspCfg, 0, sizeof(dspCfg));
    dspCfg.remoteProcName = remoteProcName;
    dspCfg.frameBytes = frameBytes;
    dspCfg.bufferBytes = bufferBytes;
    dspCfg.sampleRate = wav.sampleRate;
    dspCfg.channels = wav.channels;
    dspCfg.bitDepth = wav.bitDepth;
    dspCfg.algorithmMode = algorithmMode;
    dspCfg.directionA = directionA;
    dspCfg.directionB = directionB;

    if (syslinkDspInit(&dspCfg) < 0) {
        printf("main: syslinkDspInit failed\n");
        status = 1;
        goto leave_wav;
    }

    if (playOutput && (audioPbInit(&cfg) < 0)) {
        printf("main: audioPbInit failed\n");
        audioPbClose();
        status = 1;
        goto leave_dsp;
    }

    while (!gStopRequested && (framesRead = audioWavReadFrames(&wav, inPcm,
                    cfg.periodFrames)) > 0) {
        processed = syslinkDspProcessFrames(inPcm, outPcm, framesRead);
        if (processed != framesRead) {
            printf("main: syslinkDspProcessFrames file block failed\n");
            status = 1;
            break;
        }

        if (playOutput) {
            written = audioPbWrite(outPcm, framesRead);
            if (written != framesRead) {
                printf("main: audioPbWrite mismatch: asked=%d written=%d\n",
                        framesRead, written);
                status = 1;
                break;
            }
        }

        if (saveOutput) {
            written = audioWavWriteFrames(&outWav, outPcm, framesRead);
            if (written != framesRead) {
                printf("main: audioWavWriteFrames mismatch: asked=%d written=%d\n",
                        framesRead, written);
                status = 1;
                break;
            }
        }

        blocks++;
        processedFrames += framesRead;
        if (blocks <= 3) {
            dumpBytes("main: file block", inPcm, framesRead * frameBytes);
        }
    }

    if (playOutput) {
        audioPbDrain();
        audioPbClose();
    }

    if ((status == 0) && !gStopRequested && (processedFrames != inputFrames)) {
        printf("main: file frame count mismatch: input=%d processed=%d\n",
                inputFrames, processedFrames);
        status = 1;
    }

leave_dsp:
    if (syslinkDspClose() < 0) {
        printf("main: syslinkDspClose failed\n");
        status = 1;
    }

leave_wav:
    audioWavClose(&outWav);
    audioWavClose(&wav);
    free(inPcm);
    free(outPcm);
    printf("<-- main: file status=%d, blocks=%d, frames=%d/%d\n",
            status, blocks, processedFrames, inputFrames);
    return (status);
}

static int audioPipelineCallback(short *pcmData, int frameCount, void *userData)
{
    audioPipelineContext *ctx = (audioPipelineContext *)userData;
    int payloadBytes;

    if (gStopRequested) {
        return (-1);
    }

    if ((ctx == NULL) || (pcmData == NULL) || (frameCount <= 0)) {
        return (-1);
    }

    payloadBytes = frameCount * ctx->frameBytes;
    if ((payloadBytes <= 0) || (payloadBytes > ctx->outBytes)) {
        printf("main: audio payload too large: %d bytes\n", payloadBytes);
        ctx->status = -1;
        return (-1);
    }

    return (audioAsyncSubmit(ctx, pcmData, frameCount, payloadBytes));
}

static int audioAsyncStart(audioPipelineContext *ctx, int blockBytes)
{
    int i;

    if ((ctx == NULL) || (blockBytes <= 0)) {
        return (-1);
    }

    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        return (-1);
    }
    if (pthread_cond_init(&ctx->freeCond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->lock);
        return (-1);
    }
    if (pthread_cond_init(&ctx->readyCond, NULL) != 0) {
        pthread_cond_destroy(&ctx->freeCond);
        pthread_mutex_destroy(&ctx->lock);
        return (-1);
    }

    ctx->asyncReady = 1;
    ctx->freeHead = 0;
    ctx->freeTail = 0;
    ctx->freeCount = 0;
    ctx->readyHead = 0;
    ctx->readyTail = 0;
    ctx->readyCount = 0;
    ctx->stopping = 0;
    ctx->droppedBlocks = 0;

    for (i = 0; i < APP_AUDIO_ASYNC_BLOCKS; i++) {
        ctx->blocksPool[i].inPcm = (short *)malloc((size_t)blockBytes);
        ctx->blocksPool[i].outPcm = (short *)malloc((size_t)blockBytes);
        if ((ctx->blocksPool[i].inPcm == NULL) ||
                (ctx->blocksPool[i].outPcm == NULL)) {
            audioAsyncStop(ctx);
            return (-1);
        }
        audioAsyncQueuePush(ctx->freeQueue, &ctx->freeTail,
                &ctx->freeCount, i);
    }

    if (pthread_create(&ctx->worker, NULL, audioAsyncWorker, ctx) != 0) {
        audioAsyncStop(ctx);
        return (-1);
    }

    ctx->workerStarted = 1;
    return (0);
}

static void audioAsyncStop(audioPipelineContext *ctx)
{
    int i;

    if ((ctx == NULL) || !ctx->asyncReady) {
        return;
    }

    pthread_mutex_lock(&ctx->lock);
    ctx->stopping = 1;
    pthread_cond_broadcast(&ctx->readyCond);
    pthread_cond_broadcast(&ctx->freeCond);
    pthread_mutex_unlock(&ctx->lock);

    if (ctx->workerStarted) {
        pthread_join(ctx->worker, NULL);
        ctx->workerStarted = 0;
    }

    for (i = 0; i < APP_AUDIO_ASYNC_BLOCKS; i++) {
        free(ctx->blocksPool[i].inPcm);
        free(ctx->blocksPool[i].outPcm);
        ctx->blocksPool[i].inPcm = NULL;
        ctx->blocksPool[i].outPcm = NULL;
    }

    pthread_cond_destroy(&ctx->readyCond);
    pthread_cond_destroy(&ctx->freeCond);
    pthread_mutex_destroy(&ctx->lock);
    ctx->asyncReady = 0;
}

static int audioAsyncSubmit(audioPipelineContext *ctx, short *pcmData,
        int frameCount, int payloadBytes)
{
    int index;

    if ((ctx == NULL) || !ctx->asyncReady || (pcmData == NULL) ||
            (frameCount <= 0) || (payloadBytes <= 0)) {
        return (-1);
    }

    pthread_mutex_lock(&ctx->lock);
    if (ctx->stopping || (ctx->status < 0)) {
        pthread_mutex_unlock(&ctx->lock);
        return (-1);
    }

    if (ctx->freeCount == 0) {
        ctx->droppedBlocks++;
        pthread_mutex_unlock(&ctx->lock);
        return (0);
    }

    index = audioAsyncQueuePop(ctx->freeQueue, &ctx->freeHead,
            &ctx->freeCount);
    pthread_mutex_unlock(&ctx->lock);

    if (index < 0) {
        return (-1);
    }

    memcpy(ctx->blocksPool[index].inPcm, pcmData, (size_t)payloadBytes);
    ctx->blocksPool[index].frameCount = frameCount;
    ctx->blocksPool[index].payloadBytes = payloadBytes;

    pthread_mutex_lock(&ctx->lock);
    if (ctx->stopping || (ctx->status < 0)) {
        audioAsyncQueuePush(ctx->freeQueue, &ctx->freeTail,
                &ctx->freeCount, index);
        pthread_mutex_unlock(&ctx->lock);
        return (-1);
    }
    else {
        audioAsyncQueuePush(ctx->readyQueue, &ctx->readyTail,
                &ctx->readyCount, index);
        pthread_cond_signal(&ctx->readyCond);
    }
    pthread_mutex_unlock(&ctx->lock);

    return (0);
}

static void *audioAsyncWorker(void *arg)
{
    audioPipelineContext *ctx = (audioPipelineContext *)arg;
    audioAsyncBlock *block;
    int index;
    int processed;
    int written;
    int blockNo;

    for (;;) {
        pthread_mutex_lock(&ctx->lock);
        while ((ctx->readyCount == 0) && !ctx->stopping) {
            pthread_cond_wait(&ctx->readyCond, &ctx->lock);
        }

        if ((ctx->readyCount == 0) && ctx->stopping) {
            pthread_mutex_unlock(&ctx->lock);
            break;
        }

        index = audioAsyncQueuePop(ctx->readyQueue, &ctx->readyHead,
                &ctx->readyCount);
        pthread_mutex_unlock(&ctx->lock);

        if (index < 0) {
            continue;
        }

        block = &ctx->blocksPool[index];
        processed = syslinkDspProcessFrames(block->inPcm, block->outPcm,
                block->frameCount);
        if (processed != block->frameCount) {
            printf("main: syslinkDspProcessFrames audio failed\n");
            audioAsyncAbort(ctx, index);
            break;
        }

        written = audioPbWrite(block->outPcm, block->frameCount);
        if (written != block->frameCount) {
            printf("main: audioPbWrite mismatch: asked=%d written=%d\n",
                    block->frameCount, written);
            audioAsyncAbort(ctx, index);
            break;
        }

        if (ctx->saveOutput) {
            written = audioWavWriteFrames(&ctx->wav, block->outPcm,
                    block->frameCount);
            if (written != block->frameCount) {
                printf("main: audioWavWriteFrames mismatch: asked=%d written=%d\n",
                        block->frameCount, written);
                audioAsyncAbort(ctx, index);
                break;
            }
        }

        pthread_mutex_lock(&ctx->lock);
        ctx->blocks++;
        blockNo = ctx->blocks;
        pthread_mutex_unlock(&ctx->lock);

        if (blockNo <= 3) {
            dumpBytes("main: audio block", block->inPcm,
                    block->payloadBytes);
        }

        pthread_mutex_lock(&ctx->lock);
        audioAsyncQueuePush(ctx->freeQueue, &ctx->freeTail,
                &ctx->freeCount, index);
        pthread_cond_signal(&ctx->freeCond);
        pthread_mutex_unlock(&ctx->lock);
    }

    return (NULL);
}

static void audioAsyncAbort(audioPipelineContext *ctx, int activeIndex)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->status = -1;
    ctx->stopping = 1;
    audioAsyncReturnReadyBlocks(ctx);
    if (activeIndex >= 0) {
        audioAsyncQueuePush(ctx->freeQueue, &ctx->freeTail,
                &ctx->freeCount, activeIndex);
    }
    pthread_cond_broadcast(&ctx->freeCond);
    pthread_cond_broadcast(&ctx->readyCond);
    pthread_mutex_unlock(&ctx->lock);
    audioCapStop();
}

static void audioAsyncReturnReadyBlocks(audioPipelineContext *ctx)
{
    int index;

    while (ctx->readyCount > 0) {
        index = audioAsyncQueuePop(ctx->readyQueue, &ctx->readyHead,
                &ctx->readyCount);
        if (index < 0) {
            break;
        }
        audioAsyncQueuePush(ctx->freeQueue, &ctx->freeTail,
                &ctx->freeCount, index);
    }
}

static void audioAsyncQueuePush(int *queue, int *tail, int *count, int value)
{
    if (*count >= APP_AUDIO_ASYNC_BLOCKS) {
        return;
    }

    queue[*tail] = value;
    *tail = (*tail + 1) % APP_AUDIO_ASYNC_BLOCKS;
    (*count)++;
}

static int audioAsyncQueuePop(int *queue, int *head, int *count)
{
    int value;

    if (*count <= 0) {
        return (-1);
    }

    value = queue[*head];

    *head = (*head + 1) % APP_AUDIO_ASYNC_BLOCKS;
    (*count)--;
    return (value);
}

static int recordCallback(short *pcmData, int frameCount, void *userData)
{
    recordPipelineContext *ctx = (recordPipelineContext *)userData;
    int written;

    if (gStopRequested) {
        return (-1);
    }

    if ((ctx == NULL) || (pcmData == NULL) || (frameCount <= 0)) {
        return (-1);
    }

    written = audioWavWriteFrames(&ctx->wav, pcmData, frameCount);
    if (written != frameCount) {
        printf("main: audioWavWriteFrames mismatch: asked=%d written=%d\n",
                frameCount, written);
        ctx->status = -1;
        return (-1);
    }

    ctx->blocks++;
    if (ctx->blocks <= 3) {
        dumpBytes("main: record block", pcmData, frameCount * ctx->frameBytes);
    }

    return (0);
}

static void handleSignal(int signum)
{
    (void)signum;
    gStopRequested = 1;
}

static void printUsage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s directions\n", prog);
    printf("  %s [DSP] file --input in.wav [--output out.wav] --algo spatial --elevation E --azimuth A\n", prog);
    printf("  %s [DSP] audio [--seconds N] [--output out.wav] --algo spatial --elevation E --azimuth A\n", prog);
    printf("  %s [DSP] file --input sources.wav --algo dual --a-elevation E --a-azimuth A --b-elevation E --b-azimuth A\n", prog);
    printf("  %s record --output out.wav [--seconds N]\n", prog);
    printf("  --algo accepts existing algorithms; --check validates without hardware.\n");
    printf("  New spatial/dual angles must match an entry in directions; no interpolation.\n");
    printf("  %s [DSP] smoke [passthrough|swap|gain2|gain3|gain5|speed2]\n", prog);
    printf("  %s [DSP] audio [seconds] [output.wav] [passthrough|swap|gain2|gain3|gain5|spatial40|spatial60|orbit|dual4060]\n", prog);
    printf("  %s [DSP] record output.wav [seconds]\n", prog);
    printf("  %s [DSP] file input.wav [output.wav] [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]\n", prog);
    printf("  %s smoke [passthrough|swap|gain2|gain3|gain5|speed2]\n", prog);
    printf("  %s audio [seconds] [output.wav] [passthrough|swap|gain2|gain3|gain5|spatial40|spatial60|orbit|dual4060]\n", prog);
    printf("  %s record output.wav [seconds]\n", prog);
    printf("  %s file input.wav [output.wav] [passthrough|swap|gain2|gain3|gain5|speed2|spatial40|spatial60|orbit|dual4060]\n", prog);
}

static int parseAlgorithmMode(const char *text, int *algorithmMode)
{
    int mode;

    if ((text == NULL) || (algorithmMode == NULL)) {
        return (-1);
    }

    if ((strcmp(text, "passthrough") == 0) ||
            (strcmp(text, "pass") == 0) ||
            (strcmp(text, "bypass") == 0) ||
            (strcmp(text, "0") == 0)) {
        mode = APP_ALGO_MODE_PASSTHROUGH;
    }
    else if ((strcmp(text, "swap") == 0) ||
            (strcmp(text, "swap_stereo") == 0) ||
            (strcmp(text, "stereo_swap") == 0) ||
            (strcmp(text, "1") == 0)) {
        mode = APP_ALGO_MODE_SWAP_STEREO;
    }
    else if ((strcmp(text, "gain2") == 0) ||
            (strcmp(text, "gain_x2") == 0) ||
            (strcmp(text, "x2") == 0) ||
            (strcmp(text, "2") == 0)) {
        mode = APP_ALGO_MODE_GAIN_X2;
    }
    else if ((strcmp(text, "speed2") == 0) ||
            (strcmp(text, "speed_x2") == 0) ||
            (strcmp(text, "2x") == 0) ||
            (strcmp(text, "3") == 0)) {
        mode = APP_ALGO_MODE_SPEED2;
    }
    else if ((strcmp(text, "gain3") == 0) ||
            (strcmp(text, "gain_x3") == 0) ||
            (strcmp(text, "x3") == 0) ||
            (strcmp(text, "4") == 0)) {
        mode = APP_ALGO_MODE_GAIN_X3;
    }
    else if ((strcmp(text, "gain5") == 0) ||
            (strcmp(text, "gain_x5") == 0) ||
            (strcmp(text, "x5") == 0) ||
            (strcmp(text, "5") == 0)) {
        mode = APP_ALGO_MODE_GAIN_X5;
    }
    else if ((strcmp(text, "spatial40") == 0) ||
            (strcmp(text, "spatial_40e045") == 0) ||
            (strcmp(text, "40e045") == 0) ||
            (strcmp(text, "6") == 0)) {
        mode = APP_ALGO_MODE_SPATIAL_40E045;
    }
    else if ((strcmp(text, "spatial60") == 0) ||
            (strcmp(text, "spatial_60e250") == 0) ||
            (strcmp(text, "60e250") == 0) ||
            (strcmp(text, "7") == 0)) {
        mode = APP_ALGO_MODE_SPATIAL_60E250;
    }
    else if ((strcmp(text, "orbit") == 0) ||
            (strcmp(text, "spatial_orbit") == 0) ||
            (strcmp(text, "8") == 0)) {
        mode = APP_ALGO_MODE_ORBIT;
    }
    else if ((strcmp(text, "dual4060") == 0) ||
            (strcmp(text, "dual_60_40") == 0) ||
            (strcmp(text, "dual") == 0) ||
            (strcmp(text, "9") == 0)) {
        mode = APP_ALGO_MODE_DUAL_60_40;
    }
    else {
        return (-1);
    }

    *algorithmMode = mode;
    return (0);
}

static const char *algorithmModeName(int algorithmMode)
{
    switch (algorithmMode) {
        case APP_ALGO_MODE_PASSTHROUGH:
            return "passthrough";
        case APP_ALGO_MODE_SWAP_STEREO:
            return "swap";
        case APP_ALGO_MODE_GAIN_X2:
            return "gain2";
        case APP_ALGO_MODE_SPEED2:
            return "speed2";
        case APP_ALGO_MODE_GAIN_X3:
            return "gain3";
        case APP_ALGO_MODE_GAIN_X5:
            return "gain5";
        case APP_ALGO_MODE_SPATIAL_40E045:
            return "spatial40";
        case APP_ALGO_MODE_SPATIAL_60E250:
            return "spatial60";
        case APP_ALGO_MODE_ORBIT:
            return "orbit";
        case APP_ALGO_MODE_DUAL_60_40:
            return "dual4060";
        case APP_ALGO_MODE_SPATIAL:
            return "spatial";
        case APP_ALGO_MODE_DUAL:
            return "dual";
        default:
            return "unknown";
    }
}

static int parseDurationSec(const char *text, int *durationSec)
{
    int value = 0;
    int i;

    if ((text == NULL) || (text[0] == '\0') || (durationSec == NULL)) {
        return (-1);
    }

    for (i = 0; text[i] != '\0'; i++) {
        if ((text[i] < '0') || (text[i] > '9')) {
            return (-1);
        }
        if (value > (2147 - (text[i] - '0')) / 10) return (-1);
        value = (value * 10) + (text[i] - '0');
    }

    *durationSec = value;
    return (0);
}

static int isAppMode(const char *text)
{
    if (text == NULL) {
        return (0);
    }

    return ((strcmp(text, "smoke") == 0) ||
            (strcmp(text, "audio") == 0) ||
            (strcmp(text, "record") == 0) ||
            (strcmp(text, "directions") == 0) ||
            (strcmp(text, "file") == 0));
}

static int parseAngle(const char *text, int minimum, int maximum, int *angle)
{
    char *end;
    long value;
    const char *digit = text;
    if (*digit == '-') digit++;
    if (*digit == '\0' || strspn(digit, "0123456789") != strlen(digit)) return (-1);
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || (*end != '\0') || (value < minimum) || (value > maximum)) return (-1);
    *angle = (int)value;
    return (0);
}

static int parseNamedOptions(int argc, char **argv, int argi, const char *mode,
        const char **input, const char **output, int *seconds, int *algorithm,
        int *directionA, int *directionB)
{
    const char *names[] = {"--input", "--output", "--seconds", "--algo",
        "--elevation", "--azimuth", "--a-elevation", "--a-azimuth",
        "--b-elevation", "--b-azimuth"};
    unsigned int seen = 0;
    int angles[6] = {0};
    while (argi < argc) {
        int option;
        const char *value;
        for (option = 0; option < 10; option++) {
            if (strcmp(argv[argi], names[option]) == 0) break;
        }
        if ((option == 10) || (seen & (1u << option)) || (++argi >= argc)) {
            printf("main: unknown, duplicate, or incomplete option\n");
            return (-1);
        }
        seen |= 1u << option;
        value = argv[argi++];
        if (*value == '\0') return (-1);
        if (option == 0) *input = value;
        else if (option == 1) *output = value;
        else if (option == 2) {
            if (parseDurationSec(value, seconds) < 0) return (-1);
        }
        else if (option == 3) {
            if (strcmp(value, "spatial") == 0) *algorithm = APP_ALGO_MODE_SPATIAL;
            else if (strcmp(value, "dual") == 0) *algorithm = APP_ALGO_MODE_DUAL;
            else if (parseAlgorithmMode(value, algorithm) < 0) return (-1);
        }
        else if (parseAngle(value, (option % 2 == 0) ? -90 : 0,
                (option % 2 == 0) ? 90 : 359, &angles[option - 4]) < 0) return (-1);
    }
    if (strcmp(mode, "file") == 0) {
        if ((*input == NULL) || (seen & 4u)) return (-1);
    }
    else if (strcmp(mode, "audio") == 0) {
        if (seen & 1u) return (-1);
    }
    else if (strcmp(mode, "record") == 0) {
        return ((*output != NULL) && !(seen & ~6u)) ? 0 : -1;
    }
    else if (strcmp(mode, "smoke") == 0) {
        if (seen & ~8u) return (-1);
    }
    if (*algorithm == APP_ALGO_MODE_SPATIAL) {
        if ((seen & 0x3f0u) != 0x30u) return (-1);
        *directionA = HRIR_DIRECTION_CODE(angles[0], angles[1]);
    }
    else if (*algorithm == APP_ALGO_MODE_DUAL) {
        if ((seen & 0x3f0u) != 0x3c0u) return (-1);
        *directionA = HRIR_DIRECTION_CODE(angles[2], angles[3]);
        *directionB = HRIR_DIRECTION_CODE(angles[4], angles[5]);
    }
    else if (seen & 0x3f0u) return (-1);
    if (((*algorithm == APP_ALGO_MODE_SPATIAL) || (*algorithm == APP_ALGO_MODE_DUAL)) &&
            ((hrir_direction_index(*directionA) < 0) ||
             ((*algorithm == APP_ALGO_MODE_DUAL) && (hrir_direction_index(*directionB) < 0)))) {
        printf("main: requested HRIR direction is unavailable; use ./run.sh directions\n");
        return (-1);
    }
    return (0);
}

static void fillPcm(short *pcm, int sampleCount, int round)
{
    int i;

    for (i = 0; i < sampleCount; i++) {
        pcm[i] = (short)(round + i);
    }
}

static int verifyPassthrough(short *inPcm, short *outPcm, int byteCount)
{
    if (memcmp(inPcm, outPcm, (size_t)byteCount) != 0) {
        dumpBytes("main: expected", inPcm, byteCount);
        dumpBytes("main: actual  ", outPcm, byteCount);
        return (-1);
    }

    printf("main: DSP passthrough verified\n");
    return (0);
}

static int verifyAlgorithmOutput(int algorithmMode, short *inPcm,
        short *outPcm, int frameCount)
{
    int i;
    short expectedL;
    short expectedR;

    if (algorithmMode == APP_ALGO_MODE_PASSTHROUGH) {
        return (verifyPassthrough(inPcm, outPcm,
                frameCount * APP_FRAME_BYTES));
    }

    for (i = 0; i < frameCount; i++) {
        if (algorithmMode == APP_ALGO_MODE_SWAP_STEREO) {
            expectedL = inPcm[(2 * i) + 1];
            expectedR = inPcm[2 * i];
        }
        else if (algorithmMode == APP_ALGO_MODE_GAIN_X2) {
            expectedL = saturatingMultiply(inPcm[2 * i], 2);
            expectedR = saturatingMultiply(inPcm[(2 * i) + 1], 2);
        }
        else if (algorithmMode == APP_ALGO_MODE_SPEED2) {
            if (i < ((frameCount + 1) / 2)) {
                expectedL = inPcm[4 * i];
                expectedR = inPcm[(4 * i) + 1];
            }
            else {
                expectedL = 0;
                expectedR = 0;
            }
        }
        else if (algorithmMode == APP_ALGO_MODE_GAIN_X3) {
            expectedL = saturatingMultiply(inPcm[2 * i], 3);
            expectedR = saturatingMultiply(inPcm[(2 * i) + 1], 3);
        }
        else if (algorithmMode == APP_ALGO_MODE_GAIN_X5) {
            expectedL = saturatingMultiply(inPcm[2 * i], 10);
            expectedR = saturatingMultiply(inPcm[(2 * i) + 1], 10);
        }
        else {
            return (-1);
        }

        if ((outPcm[2 * i] != expectedL) ||
                (outPcm[(2 * i) + 1] != expectedR)) {
            dumpBytes("main: input   ", inPcm, frameCount * APP_FRAME_BYTES);
            dumpBytes("main: actual  ", outPcm, frameCount * APP_FRAME_BYTES);
            return (-1);
        }
    }

    printf("main: DSP %s verified\n", algorithmModeName(algorithmMode));
    return (0);
}

static short saturatingMultiply(short value, int factor)
{
    int tmp = (int)value * factor;

    if (tmp > 32767) {
        tmp = 32767;
    }
    if (tmp < -32768) {
        tmp = -32768;
    }

    return ((short)tmp);
}

static void dumpBytes(const char *label, short *pcm, int byteCount)
{
    unsigned char *bytes = (unsigned char *)pcm;
    int count = (byteCount < 8 ? byteCount : 8);
    int i;

    printf("%s:", label);
    for (i = 0; i < count; i++) {
        printf(" %02x", (unsigned int)bytes[i]);
    }
    printf("\n");
}
