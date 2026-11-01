/*
 * syslinkDsp.c
 *
 * ARM-side SysLink wrapper for the ARM -> DSP -> ARM processing loop.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

#include <ti/syslink/Std.h>
#include <ti/syslink/IpcHost.h>
#include <ti/syslink/SysLink.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>

#include "../../shared/AppCommon.h"
#include "../../shared/SystemCfg.h"
#include "syslinkDsp.h"
#include "../../shared/hrir_catalog.h"

#define SYSLINK_DSP_QUEUE_SIZE          8
#define SYSLINK_DSP_DEFAULT_PROC_NAME   "DSP"
#define SYSLINK_DSP_DEFAULT_FRAME_BYTES 4

#ifndef SYSLINK_DSP_WAIT_TIMEOUT_SEC
#define SYSLINK_DSP_WAIT_TIMEOUT_SEC    5
#endif

typedef struct {
    UInt32  queue[SYSLINK_DSP_QUEUE_SIZE];
    UInt    head;
    UInt    tail;
    UInt32  error;
    sem_t   semH;
} syslinkDspEventQueue;

typedef struct {
    syslinkDspEventQueue eventQueue;
    UInt16              remoteProcId;
    UInt16              lineId;
    UInt32              eventId;
    Char               *bufferPtr;
    IHeap_Handle        heap;
    int                 frameBytes;
    int                 bufferBytes;
    int                 totalBufferBytes;
    int                 ringSlots;
    int                 nextSlot;
    int                 sampleRate;
    int                 channels;
    int                 bitDepth;
    int                 algorithmMode;
    int                 directionA;
    int                 directionB;
    int                 audioCfgValid;
    int                 syslinkReady;
    int                 loadStarted;
    int                 notifyRegistered;
    int                 semCreated;
    int                 processMutexCreated;
    int                 bufferAllocated;
    int                 sharedPtrSent;
    pthread_mutex_t     processMutex;
} syslinkDspModule;

static Int syslinkDspStartRemote(const char *remoteProcName);
static Int syslinkDspCreateNotify(Void);
static Int syslinkDspCreateBuffer(Void);
static Int syslinkDspSendSharedPtr(Void);
static Int syslinkDspSendRingConfig(Void);
static Int syslinkDspSendAudioConfig(Void);
static Int syslinkDspSendAlgorithmMode(Void);
static Int syslinkDspSendDirection(UInt32 cmd, int code);
static Int syslinkDspSendShutdown(Void);
static Int syslinkDspWaitForCommand(UInt32 expectedCmd, UInt32 *eventOut);
static Int syslinkDspWaitForSlotCommand(UInt32 expectedCmd, UInt32 slotId,
        UInt32 *eventOut);
static UInt32 syslinkDspWaitForEvent(syslinkDspEventQueue *eventQueue);
static Void syslinkDspNotifyCB(UInt16 procId, UInt16 lineId, UInt32 eventId,
        UArg arg, UInt32 payload);
static Void syslinkDspResetModule(Void);

static syslinkDspModule Module;

/*
 *  ======== syslinkDspInit ========
 */
int syslinkDspInit(const syslinkDspConfig *cfg)
{
    Int         status;
    const char *remoteProcName = SYSLINK_DSP_DEFAULT_PROC_NAME;
    int         frameBytes = SYSLINK_DSP_DEFAULT_FRAME_BYTES;
    int         bufferBytes = APP_DEFAULT_BUFFER_SIZE;
    int         sampleRate = 0;
    int         channels = 0;
    int         bitDepth = 0;
    int         algorithmMode = APP_ALGO_MODE_PASSTHROUGH;
    int         audioCfgValid = FALSE;
    int         ringSlots = APP_RING_DEFAULT_SLOTS;

    printf("--> syslinkDspInit:\n");

    if (Module.syslinkReady) {
        printf("syslinkDspInit: module already initialized\n");
        return (-1);
    }

    if (cfg != NULL) {
        if (cfg->remoteProcName != NULL) {
            remoteProcName = cfg->remoteProcName;
        }
        if (cfg->frameBytes > 0) {
            frameBytes = cfg->frameBytes;
        }
        if (cfg->bufferBytes > 0) {
            bufferBytes = cfg->bufferBytes;
        }
        if ((cfg->sampleRate > 0) || (cfg->channels > 0) ||
                (cfg->bitDepth > 0)) {
            sampleRate = cfg->sampleRate;
            channels = cfg->channels;
            bitDepth = cfg->bitDepth;
            audioCfgValid = TRUE;
        }
        algorithmMode = cfg->algorithmMode;
        if (cfg->ringSlots > 0) {
            ringSlots = cfg->ringSlots;
        }
    }

    if ((bufferBytes <= 0) || (bufferBytes > APP_MAX_PAYLOAD_SIZE)) {
        printf("syslinkDspInit: invalid bufferBytes=%d\n", bufferBytes);
        return (-1);
    }

    if ((frameBytes <= 0) || (frameBytes > bufferBytes)) {
        printf("syslinkDspInit: invalid frameBytes=%d\n", frameBytes);
        return (-1);
    }

    if (audioCfgValid) {
        if ((sampleRate <= 0) || (sampleRate > APP_DATA_MASK) ||
                (channels <= 0) || (channels > 255) ||
                (bitDepth <= 0) || (bitDepth > 255) ||
                ((bitDepth % 8) != 0) ||
                (frameBytes != channels * (bitDepth / 8))) {
            printf("syslinkDspInit: invalid audio cfg rate=%d ch=%d bit=%d frameBytes=%d\n",
                    sampleRate, channels, bitDepth, frameBytes);
            return (-1);
        }
    }

    if ((algorithmMode < APP_ALGO_MODE_PASSTHROUGH) ||
            (algorithmMode > APP_ALGO_MODE_MAX)) {
        printf("syslinkDspInit: invalid algorithmMode=%d\n",
                algorithmMode);
        return (-1);
    }

    if ((ringSlots <= 0) || (ringSlots > APP_RING_MAX_SLOTS)) {
        printf("syslinkDspInit: invalid ringSlots=%d\n", ringSlots);
        return (-1);
    }

    if (bufferBytes > (INT_MAX / ringSlots)) {
        printf("syslinkDspInit: buffer size overflow, slotBytes=%d slots=%d\n",
                bufferBytes, ringSlots);
        return (-1);
    }

    syslinkDspResetModule();
    Module.lineId = SystemCfg_LineId;
    Module.eventId = SystemCfg_EventId;
    Module.frameBytes = frameBytes;
    Module.bufferBytes = bufferBytes;
    Module.totalBufferBytes = bufferBytes * ringSlots;
    Module.ringSlots = ringSlots;
    Module.nextSlot = 0;
    Module.sampleRate = sampleRate;
    Module.channels = channels;
    Module.bitDepth = bitDepth;
    Module.algorithmMode = algorithmMode;
    Module.directionA = -1;
    Module.directionB = -1;
    if ((algorithmMode == APP_ALGO_MODE_SPATIAL) ||
            (algorithmMode == APP_ALGO_MODE_DUAL)) {
        if ((cfg == NULL) || (hrir_direction_index(cfg->directionA) < 0) ||
                ((algorithmMode == APP_ALGO_MODE_DUAL) &&
                 (hrir_direction_index(cfg->directionB) < 0))) {
            printf("syslinkDspInit: unavailable HRIR direction\n");
            return (-1);
        }
        Module.directionA = cfg->directionA;
        Module.directionB = cfg->directionB;
    }
    Module.audioCfgValid = audioCfgValid;

    if (pthread_mutex_init(&Module.processMutex, NULL) != 0) {
        printf("syslinkDspInit: failed to initialize process mutex\n");
        syslinkDspResetModule();
        return (-1);
    }
    Module.processMutexCreated = TRUE;

    SysLink_setup();
    Module.syslinkReady = TRUE;

    status = syslinkDspStartRemote(remoteProcName);
    if (status < 0) {
        goto fail;
    }

    status = syslinkDspCreateNotify();
    if (status < 0) {
        goto fail;
    }

    status = syslinkDspCreateBuffer();
    if (status < 0) {
        goto fail;
    }

    status = syslinkDspSendSharedPtr();
    if (status < 0) {
        goto fail;
    }

    status = syslinkDspSendRingConfig();
    if (status < 0) {
        goto fail;
    }

    if (Module.audioCfgValid) {
        status = syslinkDspSendAudioConfig();
        if (status < 0) {
            goto fail;
        }
    }

    if ((algorithmMode == APP_ALGO_MODE_SPATIAL) ||
            (algorithmMode == APP_ALGO_MODE_DUAL)) {
        if (syslinkDspSendDirection(APP_CMD_DIRECTION_A, Module.directionA) < 0) goto fail;
        if ((algorithmMode == APP_ALGO_MODE_DUAL) &&
                (syslinkDspSendDirection(APP_CMD_DIRECTION_B, Module.directionB) < 0)) goto fail;
    }
    status = syslinkDspSendAlgorithmMode();
    if (status < 0) {
        goto fail;
    }

    Module.sharedPtrSent = TRUE;
    printf("syslinkDspInit: ready, frameBytes=%d, slotBytes=%d, ringSlots=%d, algorithmMode=%d\n",
            Module.frameBytes, Module.bufferBytes, Module.ringSlots,
            Module.algorithmMode);
    printf("<-- syslinkDspInit:\n");
    return (0);

fail:
    syslinkDspClose();
    printf("<-- syslinkDspInit: failed\n");
    return (-1);
}

/*
 *  ======== syslinkDspProcessBuffer ========
 */
int syslinkDspProcessBuffer(const void *inData, void *outData, int byteCount)
{
    Int     status;
    int     ret = -1;
    int     locked = FALSE;
    UInt32  event;
    UInt32  payloadBytes;
    UInt32  command;
    UInt32  slotId;
    Char   *slotPtr;

    if (!Module.sharedPtrSent) {
        printf("syslinkDspProcessBuffer: module is not initialized\n");
        return (-1);
    }

    if (Module.processMutexCreated) {
        if (pthread_mutex_lock(&Module.processMutex) != 0) {
            printf("syslinkDspProcessBuffer: failed to lock process mutex\n");
            return (-1);
        }
        locked = TRUE;
    }

    if ((inData == NULL) || (outData == NULL) || (byteCount <= 0)) {
        printf("syslinkDspProcessBuffer: invalid arguments\n");
        goto leave;
    }

    if ((Module.frameBytes <= 0) || ((byteCount % Module.frameBytes) != 0)) {
        printf("syslinkDspProcessBuffer: byteCount=%d not aligned to frameBytes=%d\n",
                byteCount, Module.frameBytes);
        goto leave;
    }

    payloadBytes = (UInt32)byteCount;
    if ((payloadBytes == 0) || (payloadBytes > (UInt32)Module.bufferBytes) ||
            (payloadBytes > APP_MAX_PAYLOAD_SIZE)) {
        printf("syslinkDspProcessBuffer: payload too large: %u\n",
                payloadBytes);
        goto leave;
    }

    slotId = (UInt32)Module.nextSlot;
    slotPtr = Module.bufferPtr + (slotId * (UInt32)Module.bufferBytes);

    memcpy(slotPtr, inData, payloadBytes);

    command = APP_CMD_PROCESSING | APP_SLOT_FIELD(slotId) | payloadBytes;
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);

    if (status < 0) {
        printf("syslinkDspProcessBuffer: failed to send processing command\n");
        goto leave;
    }

    status = syslinkDspWaitForSlotCommand(APP_CMD_OP_COMPLETE, slotId,
            &event);
    if (status < 0) {
        if ((event & APP_CMD_MASK) == APP_CMD_OP_FAILED) {
            printf("syslinkDspProcessBuffer: DSP processing failed, slot=%u code=%u\n",
                    (unsigned int)APP_SLOT_ID(event),
                    (unsigned int)(event & APP_DATA_MASK));
        }
        else {
            printf("syslinkDspProcessBuffer: unexpected completion event 0x%x\n",
                    event);
        }
        goto leave;
    }

    memcpy(outData, slotPtr, payloadBytes);
    Module.nextSlot = (Module.nextSlot + 1) % Module.ringSlots;
    ret = (int)payloadBytes;

leave:
    if (locked) {
        pthread_mutex_unlock(&Module.processMutex);
    }
    return (ret);
}

/*
 *  ======== syslinkDspProcessFrames ========
 */
int syslinkDspProcessFrames(const void *inData, void *outData, int frameCount)
{
    int byteCount;
    int processedBytes;

    if ((frameCount <= 0) || (Module.frameBytes <= 0) ||
            (frameCount > (Module.bufferBytes / Module.frameBytes))) {
        printf("syslinkDspProcessFrames: invalid frameCount=%d\n",
                frameCount);
        return (-1);
    }

    byteCount = frameCount * Module.frameBytes;
    processedBytes = syslinkDspProcessBuffer(inData, outData, byteCount);
    if (processedBytes != byteCount) {
        return (-1);
    }

    return (frameCount);
}

/*
 *  ======== syslinkDspProcess ========
 */
int syslinkDspProcess(const short *inPcm, short *outPcm, int frameCount)
{
    return (syslinkDspProcessFrames(inPcm, outPcm, frameCount));
}

/*
 *  ======== syslinkDspClose ========
 */
int syslinkDspClose(void)
{
    Int     status = 0;
    int     locked = FALSE;

    if (Module.processMutexCreated) {
        if (pthread_mutex_lock(&Module.processMutex) == 0) {
            locked = TRUE;
        }
        else {
            printf("syslinkDspClose: failed to lock process mutex\n");
            status = -1;
        }
    }

    if (Module.notifyRegistered) {
        if (syslinkDspSendShutdown() < 0) {
            status = -1;
        }
    }

    if (Module.bufferAllocated) {
        Memory_free(Module.heap, Module.bufferPtr, Module.totalBufferBytes);
        Module.bufferAllocated = FALSE;
        Module.bufferPtr = NULL;
    }

    if (Module.notifyRegistered) {
        if (Notify_unregisterEvent(Module.remoteProcId, Module.lineId,
                Module.eventId, syslinkDspNotifyCB,
                (UArg)&Module.eventQueue) < 0) {
            printf("syslinkDspClose: failed to unregister notify\n");
            status = -1;
        }
        Module.notifyRegistered = FALSE;
    }

    if (Module.semCreated) {
        sem_destroy(&Module.eventQueue.semH);
        Module.semCreated = FALSE;
    }

    if (locked) {
        pthread_mutex_unlock(&Module.processMutex);
    }

    if (Module.processMutexCreated) {
        pthread_mutex_destroy(&Module.processMutex);
        Module.processMutexCreated = FALSE;
    }

    if (Module.loadStarted) {
        if (Ipc_control(Module.remoteProcId, Ipc_CONTROLCMD_STOPCALLBACK,
                NULL) < 0) {
            printf("syslinkDspClose: stop callback failed\n");
            status = -1;
        }
        Module.loadStarted = FALSE;
    }

    if (Module.syslinkReady) {
        SysLink_destroy();
        Module.syslinkReady = FALSE;
    }

    syslinkDspResetModule();
    return (status >= 0 ? 0 : -1);
}

/*
 *  ======== syslinkDspStartRemote ========
 */
static Int syslinkDspStartRemote(const char *remoteProcName)
{
    Int status;

    Module.remoteProcId = MultiProc_getId((String)remoteProcName);
    printf("syslinkDspInit: remote=%s, procId=%u\n",
            remoteProcName, (unsigned int)Module.remoteProcId);
    fflush(stdout);

    printf("syslinkDspInit: invoking load callback\n");
    fflush(stdout);
    status = Ipc_control(Module.remoteProcId, Ipc_CONTROLCMD_LOADCALLBACK,
            NULL);
    if (status < 0) {
        printf("syslinkDspInit: load callback failed: %d\n",
                status);
        fflush(stdout);
        return (-1);
    }

    printf("syslinkDspInit: invoking start callback\n");
    fflush(stdout);
    status = Ipc_control(Module.remoteProcId, Ipc_CONTROLCMD_STARTCALLBACK,
            NULL);
    if (status < 0) {
        printf("syslinkDspInit: start callback failed: %d\n", status);
        fflush(stdout);
        return (-1);
    }

    printf("syslinkDspInit: start callback ok\n");
    fflush(stdout);
    Module.loadStarted = TRUE;
    return (0);
}

/*
 *  ======== syslinkDspCreateNotify ========
 */
static Int syslinkDspCreateNotify(Void)
{
    Int status;
    int retStatus;

    retStatus = sem_init(&Module.eventQueue.semH, 0, 0);
    if (retStatus == -1) {
        printf("syslinkDspInit: could not initialize semaphore\n");
        return (-1);
    }
    Module.semCreated = TRUE;

    status = Notify_registerEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, syslinkDspNotifyCB, (UArg)&Module.eventQueue);
    if (status < 0) {
        printf("syslinkDspInit: failed to register notify event\n");
        return (-1);
    }
    Module.notifyRegistered = TRUE;

    do {
        status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                Module.eventId, APP_CMD_NOP, TRUE);

        if (status == Notify_E_EVTNOTREGISTERED) {
            usleep(100);
        }
    } while (status == Notify_E_EVTNOTREGISTERED);

    if (status != Notify_S_SUCCESS) {
        printf("syslinkDspInit: remote notify registration failed\n");
        return (-1);
    }

    return (0);
}

/*
 *  ======== syslinkDspCreateBuffer ========
 */
static Int syslinkDspCreateBuffer(Void)
{
    Module.heap = (IHeap_Handle)SharedRegion_getHeap(SHARED_REGION_1);
    if (Module.heap == NULL) {
        printf("syslinkDspInit: shared region heap does not exist\n");
        return (-1);
    }

    Module.bufferPtr = (Char *)Memory_calloc(Module.heap,
            Module.totalBufferBytes, 0, NULL);
    if (Module.bufferPtr == NULL) {
        printf("syslinkDspInit: failed to allocate shared buffer\n");
        return (-1);
    }

    Module.bufferAllocated = TRUE;
    return (0);
}

/*
 *  ======== syslinkDspSendRingConfig ========
 */
static Int syslinkDspSendRingConfig(Void)
{
    Int     status;
    UInt32  event;
    UInt32  command;

    command = APP_CMD_RING_CFG |
            APP_SLOT_FIELD((UInt32)Module.ringSlots) |
            ((UInt32)Module.bufferBytes & APP_DATA_MASK);

    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);
    if (status < 0) {
        printf("syslinkDspInit: failed to send ring config\n");
        return (-1);
    }

    status = syslinkDspWaitForCommand(APP_CMD_RING_CFG_ACK, &event);
    if (status < 0) {
        printf("syslinkDspInit: unexpected ring config ack 0x%x\n", event);
        return (-1);
    }

    printf("syslinkDspInit: ring cfg slots=%d slotBytes=%d totalBytes=%d\n",
            Module.ringSlots, Module.bufferBytes, Module.totalBufferBytes);
    return (0);
}

/*
 *  ======== syslinkDspSendSharedPtr ========
 */
static Int syslinkDspSendSharedPtr(Void)
{
    Int                 status;
    SharedRegion_SRPtr  sharedBufferPtr;
    UInt32              command;
    UInt32              event;

    sharedBufferPtr = SharedRegion_getSRPtr(Module.bufferPtr,
            SHARED_REGION_1);

    command = APP_SPTR_LADDR | (sharedBufferPtr & APP_SPTR_MASK);
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);
    if (status < 0) {
        printf("syslinkDspInit: failed to send low shared pointer\n");
        return (-1);
    }

    status = syslinkDspWaitForCommand(APP_SPTR_ADDR_ACK, &event);
    if (status < 0) {
        printf("syslinkDspInit: unexpected low pointer ack 0x%x\n", event);
        return (-1);
    }

    command = APP_SPTR_HADDR | ((sharedBufferPtr >> 16) & APP_SPTR_MASK);
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);
    if (status < 0) {
        printf("syslinkDspInit: failed to send high shared pointer\n");
        return (-1);
    }

    status = syslinkDspWaitForCommand(APP_SPTR_ADDR_ACK, &event);
    if (status < 0) {
        printf("syslinkDspInit: unexpected high pointer ack 0x%x\n", event);
        return (-1);
    }

    return (0);
}

/*
 *  ======== syslinkDspSendAudioConfig ========
 */
static Int syslinkDspSendAudioConfig(Void)
{
    Int     status;
    UInt32  event;
    UInt32  command;

    command = APP_CMD_AUDIO_CFG_RATE |
            ((UInt32)Module.sampleRate & APP_DATA_MASK);
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);
    if (status < 0) {
        printf("syslinkDspInit: failed to send audio sample rate\n");
        return (-1);
    }

    status = syslinkDspWaitForCommand(APP_CMD_AUDIO_CFG_ACK, &event);
    if (status < 0) {
        printf("syslinkDspInit: unexpected audio rate ack 0x%x\n", event);
        return (-1);
    }

    command = APP_CMD_AUDIO_CFG_FMT |
            ((((UInt32)Module.channels & 0xFF) << 8) |
             ((UInt32)Module.bitDepth & 0xFF));
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);
    if (status < 0) {
        printf("syslinkDspInit: failed to send audio format\n");
        return (-1);
    }

    status = syslinkDspWaitForCommand(APP_CMD_AUDIO_CFG_ACK, &event);
    if (status < 0) {
        printf("syslinkDspInit: unexpected audio format ack 0x%x\n", event);
        return (-1);
    }

    printf("syslinkDspInit: audio cfg rate=%d ch=%d bit=%d\n",
            Module.sampleRate, Module.channels, Module.bitDepth);
    return (0);
}

/*
 *  ======== syslinkDspSendAlgorithmMode ========
 */
static Int syslinkDspSendDirection(UInt32 cmd, int code)
{
    UInt32 event = 0;
    if (Notify_sendEvent(Module.remoteProcId, Module.lineId, Module.eventId,
            cmd | ((UInt32)code & APP_DATA_MASK), TRUE) < 0) return (-1);
    if ((syslinkDspWaitForCommand(APP_CMD_DIRECTION_ACK, &event) < 0) ||
            ((event & APP_DATA_MASK) != (UInt32)code)) {
        printf("syslinkDspInit: direction rejected or unsupported, code=%d ack=0x%x\n",
                code, event);
        return (-1);
    }
    printf("syslinkDspInit: direction elevation=%d azimuth=%d\n",
            code / 360 - 90, code % 360);
    return (0);
}

static Int syslinkDspSendAlgorithmMode(Void)
{
    Int     status;
    UInt32  event;
    UInt32  command;

    command = APP_CMD_ALGO_MODE |
            ((UInt32)Module.algorithmMode & APP_DATA_MASK);
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, command, TRUE);
    if (status < 0) {
        printf("syslinkDspInit: failed to send algorithm mode\n");
        return (-1);
    }

    status = syslinkDspWaitForCommand(APP_CMD_ALGO_CFG_ACK, &event);
    if (status < 0) {
        printf("syslinkDspInit: unexpected algorithm mode ack 0x%x\n",
                event);
        return (-1);
    }

    printf("syslinkDspInit: algorithm mode=%d\n",
            Module.algorithmMode);
    return (0);
}

/*
 *  ======== syslinkDspSendShutdown ========
 */
static Int syslinkDspSendShutdown(Void)
{
    Int     status;
    UInt32  event = 0;

    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_CMD_SHUTDOWN, TRUE);

    if (status < 0) {
        printf("syslinkDspClose: failed to send shutdown\n");
        return (-1);
    }

    if (syslinkDspWaitForCommand(APP_CMD_SHUTDOWN_ACK, &event) < 0) {
        printf("syslinkDspClose: unexpected shutdown event 0x%x\n", event);
        return (-1);
    }

    Module.sharedPtrSent = FALSE;
    return (0);
}

/*
 *  ======== syslinkDspWaitForCommand ========
 */
static Int syslinkDspWaitForCommand(UInt32 expectedCmd, UInt32 *eventOut)
{
    UInt32 event = syslinkDspWaitForEvent(&Module.eventQueue);

    if (eventOut != NULL) {
        *eventOut = event;
    }

    if (event >= APP_E_FAILURE) {
        return (-1);
    }

    if ((event & APP_CMD_MASK) != expectedCmd) {
        return (-1);
    }

    return (0);
}

/*
 *  ======== syslinkDspWaitForSlotCommand ========
 */
static Int syslinkDspWaitForSlotCommand(UInt32 expectedCmd, UInt32 slotId,
        UInt32 *eventOut)
{
    UInt32 event = syslinkDspWaitForEvent(&Module.eventQueue);

    if (eventOut != NULL) {
        *eventOut = event;
    }

    if (event >= APP_E_FAILURE) {
        return (-1);
    }

    if ((event & APP_CMD_MASK) != expectedCmd) {
        return (-1);
    }

    if (APP_SLOT_ID(event) != slotId) {
        return (-1);
    }

    return (0);
}

/*
 *  ======== syslinkDspNotifyCB ========
 */
static Void syslinkDspNotifyCB(UInt16 procId, UInt16 lineId, UInt32 eventId,
        UArg arg, UInt32 payload)
{
    UInt                 next;
    syslinkDspEventQueue *eventQueue = (syslinkDspEventQueue *)arg;

    if (payload == APP_CMD_NOP) {
        return;
    }

    next = (eventQueue->head + 1) % SYSLINK_DSP_QUEUE_SIZE;
    if (next == eventQueue->tail) {
        eventQueue->error = APP_E_OVERFLOW;
        sem_post(&eventQueue->semH);
    }
    else {
        eventQueue->queue[eventQueue->head] = payload;
        eventQueue->head = next;
        sem_post(&eventQueue->semH);
    }
}

/*
 *  ======== syslinkDspWaitForEvent ========
 */
static UInt32 syslinkDspWaitForEvent(syslinkDspEventQueue *eventQueue)
{
    UInt32 event;
    struct timespec deadline;
    int status;

    if (eventQueue->error >= APP_E_FAILURE) {
        event = eventQueue->error;
    }
    else {
        if (clock_gettime(CLOCK_REALTIME, &deadline) < 0) {
            printf("syslinkDspWaitForEvent: clock_gettime failed\n");
            return (APP_E_FAILURE);
        }

        deadline.tv_sec += SYSLINK_DSP_WAIT_TIMEOUT_SEC;

        do {
            status = sem_timedwait(&eventQueue->semH, &deadline);
        } while ((status < 0) && (errno == EINTR));

        if (status < 0) {
            if (errno == ETIMEDOUT) {
                printf("syslinkDspWaitForEvent: timeout after %d seconds\n",
                        SYSLINK_DSP_WAIT_TIMEOUT_SEC);
                return (APP_E_TIMEOUT);
            }

            printf("syslinkDspWaitForEvent: sem_timedwait failed: errno=%d\n",
                    errno);
            return (APP_E_FAILURE);
        }

        event = eventQueue->queue[eventQueue->tail];
        eventQueue->tail = (eventQueue->tail + 1) %
                SYSLINK_DSP_QUEUE_SIZE;
    }

    return (event);
}

/*
 *  ======== syslinkDspResetModule ========
 */
static Void syslinkDspResetModule(Void)
{
    memset(&Module, 0, sizeof(Module));
}
