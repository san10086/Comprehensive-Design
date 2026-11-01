/*
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== Server.c ========
 *
 */

/* this define must precede inclusion of any xdc header file */
#define Registry_CURDESC Test__Desc
#define MODULE_NAME "Server"

#include <stdio.h>    /* by Guanqing 20210517  */

/* xdctools header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/System.h>

/* package header files */
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>

/* local header files */
#include "../shared/SystemCfg.h"
#include "../shared/AppCommon.h"
#include "AudioAlgo.h"

/* module header file */
#include "Server.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE   8   
#define SERVER_E_BAD_PAYLOAD    1
#define SERVER_E_PROCESSING     2

/* queue structure */
typedef struct 
{
    UInt32              queue[QUEUESIZE];   /* command queue */
    UInt                head;               /* queue head pointer */
    UInt                tail;               /* queue tail pointer */
    UInt32              error;              /* error flag */
    Semaphore_Struct    semObj;             /* semaphore object */
    Semaphore_Handle    semH;               /* handle to object above */
} Event_Queue;   

/* module structure */
typedef struct {
    Event_Queue     eventQueue;
    UInt16          remoteProcId;
    UInt16          lineId;             /* notify line id */
    UInt32          eventId;            /* notify event id */
    AudioAlgoContext audioAlgo;
    UInt32          audioBlockSeq;
    UInt32          audioSampleRate;
    UInt16          audioChannels;
    UInt16          audioBitDepth;
    UInt16          audioFrameBytes;
    UInt16          ringSlotBytes;
    UInt16          ringSlotCount;
    Int32           directionA;
    Int32           directionB;
    UInt32          directionMask;
} Server_Module;

/* private functions */
static Int Server_recvSharedPtr(SharedRegion_SRPtr* sharedBufferPtr,
        Char** bufferPtr);
static Int Server_handleAudioConfig(UInt32 cmd, UInt32 payload);
static Int Server_handleAlgorithmMode(UInt32 payload);
static Int Server_handleRingConfig(UInt32 payload);
static Int Server_configureAudioAlgo(Void);
static Int Server_processBuffer(Char* bufferPtr, UInt32 size);
static UInt32 Server_waitForEvent(Event_Queue* eventQueue);
static Void Server_notifyCB(UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);

/* private data */
Registry_Desc               Registry_CURDESC;
static Int                  Module_curInit = 0;
static Server_Module        Module;


/*
 *  ======== Server_init ========
 */
Void Server_init(Void)
{
    Registry_Result result;

    if (Module_curInit++ != 0) {
        return;  /* already initialized */
    }

    /* register with xdc.runtime to get a diags mask */
    result = Registry_addModule(&Registry_CURDESC, MODULE_NAME);
    Assert_isTrue(result == Registry_SUCCESS, (Assert_Id)NULL);
}


/*
 *  ======== Server_create ========
 *
 *  1. create sync object
 *  2. register notify callback
 *  3. wait until remote core has also registered notify callback
 */
Int Server_create(UInt16 remoteProcId)
{
    Int                 status      = 0;
    Semaphore_Params    semParams;

    Log_print0(Diags_ENTRY, "--> Server_create:");
    
    /* setting default values */
    Module.eventQueue.head      = 0;               
    Module.eventQueue.tail      = 0;              
    Module.eventQueue.error     = 0;       
    Module.eventQueue.semH      = NULL;
    Module.lineId               = SystemCfg_LineId;
    Module.eventId              = SystemCfg_EventId;
    Module.remoteProcId         = remoteProcId;
    Module.audioBlockSeq        = 0;
    Module.audioSampleRate      = 8000;
    Module.audioChannels        = 2;
    Module.audioBitDepth        = 16;
    Module.audioFrameBytes      = 4;
    Module.ringSlotBytes        = APP_MAX_PAYLOAD_SIZE;
    Module.ringSlotCount        = 1;
    Module.directionA           = -1;
    Module.directionB           = -1;
    Module.directionMask        = 0;
    AudioAlgo_init(&Module.audioAlgo);
    
    /* enable some log events */
    Diags_setMask(MODULE_NAME"+EXF");

    /* 1. create sync object */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_COUNTING;
    
    Semaphore_construct(&Module.eventQueue.semObj, 0, &semParams);

    Module.eventQueue.semH = Semaphore_handle(&Module.eventQueue.semObj);

    /* 2. register notify callback */
    status = Notify_registerEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, Server_notifyCB,(UArg)  &Module.eventQueue);

    if (status < 0) {
        Log_error0("Server_create: Device failed to register notify event");
        goto leave;
    }

    /* 3. wait until remote core has also registered notify callback */
    do {
        status = Notify_sendEvent(Module.remoteProcId,Module.lineId,
                Module.eventId, APP_CMD_NOP, TRUE);

        if (status == Notify_E_EVTNOTREGISTERED) {
            Task_sleep(100);
        }
        
    } while (status == Notify_E_EVTNOTREGISTERED);

    if (status < 0 ) {
        Log_error0("Server_create: Host failed to register callback");
        goto leave;
    }
    
    Log_print0(Diags_INFO, "Server_create: Slave is ready");

leave:
    Log_print1(Diags_EXIT, "<-- Server_create: %d", (IArg)status);
    return (status);
}


/*
 *  ======== Server_exec ========
 *  1. wait for shared region pointer address lower two bytes 
 *  2. save only the payload which represents the shared region pointer
 *     address lower two bytes
 *  3. wait for shared region pointer address upper two bytes
 *  4. join shared region pointer address upper and lower two bytes
 *  5. translate shared region pointer to local address space pointer
 *  6. loop over processing commands until shutdown
 */
Int Server_exec()
{
    Int                 status              = 0;
    SharedRegion_SRPtr  sharedBufferPtr     = 0;
    UInt32              event;
    UInt32              cmd;
    UInt32              size;
    UInt32              slotId;
    Char*               bufferPtr;
    Char*               slotPtr;
    Bool                running             = TRUE;

    Log_print0(Diags_ENTRY | Diags_INFO, "--> Server_exec:");

    status = Server_recvSharedPtr(&sharedBufferPtr, &bufferPtr);
    if (status < 0) {
        goto leave;
    }

    while (running) {
        event = Server_waitForEvent(&Module.eventQueue);

        if (event >= APP_E_FAILURE) {
            Log_error1("Server_exec: Received queue error: 0x%x", event);
            status = -1;
            goto leave;
        }

        cmd = event & APP_CMD_MASK;
        size = event & APP_DATA_MASK;
        slotId = APP_SLOT_ID(event);

        if (cmd == APP_CMD_RING_CFG) {
            status = Server_handleRingConfig(event);
            if (status < 0) {
                goto leave;
            }

            status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                    Module.eventId, APP_CMD_RING_CFG_ACK, TRUE);

            if (status < 0) {
                Log_error0("Server_exec: Error sending ring config ack");
                goto leave;
            }
        }
        else if ((cmd == APP_CMD_AUDIO_CFG_RATE) ||
                (cmd == APP_CMD_AUDIO_CFG_FMT)) {
            status = Server_handleAudioConfig(cmd, size);
            if (status < 0) {
                goto leave;
            }

            status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                    Module.eventId, APP_CMD_AUDIO_CFG_ACK, TRUE);

            if (status < 0) {
                Log_error0("Server_exec: Error sending audio config ack");
                goto leave;
            }
        }
        else if ((cmd == APP_CMD_DIRECTION_A) || (cmd == APP_CMD_DIRECTION_B)) {
            UInt32 response;
            if (hrir_direction_index((int)size) < 0) {
                /* A failed update must not reuse the preceding valid code. */
                Module.directionMask &= (cmd == APP_CMD_DIRECTION_A) ? ~1u : ~2u;
                response = APP_CMD_ALGO_CFG_FAILED | 1u;
            }
            else {
                if (cmd == APP_CMD_DIRECTION_A) {
                    Module.directionA = (Int32)size;
                    Module.directionMask |= 1u;
                }
                else {
                    Module.directionB = (Int32)size;
                    Module.directionMask |= 2u;
                }
                response = APP_CMD_DIRECTION_ACK | size;
            }
            status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                    Module.eventId, response, TRUE);
            if (status < 0) goto leave;
        }
        else if (cmd == APP_CMD_ALGO_MODE) {
            status = Server_handleAlgorithmMode(size);
            if (status < 0) {
                status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                        Module.eventId, APP_CMD_ALGO_CFG_FAILED | 2u, TRUE);
                if (status < 0) goto leave;
                continue;
            }

            status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                    Module.eventId, APP_CMD_ALGO_CFG_ACK, TRUE);

            if (status < 0) {
                Log_error0("Server_exec: Error sending algorithm config ack");
                goto leave;
            }
        }
        else if (cmd == APP_CMD_PROCESSING) {
            if ((slotId >= Module.ringSlotCount) ||
                    (size == 0) || (size > Module.ringSlotBytes)) {
                Log_error1("Server_exec: Invalid processing size: %d", size);
                status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                        Module.eventId,
                        APP_CMD_OP_FAILED | APP_SLOT_FIELD(slotId) |
                        SERVER_E_BAD_PAYLOAD, TRUE);
                if (status < 0) {
                    Log_error0("Server_exec: Error sending operation failed");
                    goto leave;
                }
                continue;
            }

            Log_print1(Diags_INFO, "Server_exec: Processing size=%d",
                    (IArg)size);

            slotPtr = bufferPtr + (slotId * Module.ringSlotBytes);
            status = Server_processBuffer(slotPtr, size);
            if (status < 0) {
                status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                        Module.eventId,
                        APP_CMD_OP_FAILED | APP_SLOT_FIELD(slotId) |
                        SERVER_E_PROCESSING, TRUE);
                if (status < 0) {
                    Log_error0("Server_exec: Error sending operation failed");
                    goto leave;
                }
                continue;
            }

            status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                    Module.eventId,
                    APP_CMD_OP_COMPLETE | APP_SLOT_FIELD(slotId), TRUE);

            if (status < 0) {
                Log_error0("Server_exec: Error sending operation complete");
                goto leave;
            }
        }
        else if (cmd == APP_CMD_SHUTDOWN) {
            Log_print0(Diags_INFO, "Server_exec: Received shutdown");

            status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                    Module.eventId, APP_CMD_SHUTDOWN_ACK, TRUE);

            if (status < 0) {
                Log_error0("Server_exec: Error sending shutdown acknowledge");
                goto leave;
            }

            running = FALSE;
        }
        else {
            Log_error1("Server_exec: Unexpected command: 0x%x", cmd);
            status = -1;
            goto leave;
        }
    }

leave:
    Log_print1(Diags_EXIT, "<-- Server_exec: %d", (IArg)status);
    return (status);
}

/*
 *  ======== Server_handleAudioConfig ========
 */
static Int Server_handleAudioConfig(UInt32 cmd, UInt32 payload)
{
    UInt16 channels;
    UInt16 bitDepth;

    if (cmd == APP_CMD_AUDIO_CFG_RATE) {
        if (payload == 0) {
            Log_error0("Server_exec: Invalid audio sample rate");
            return (-1);
        }

        Module.audioSampleRate = payload;
        if (Server_configureAudioAlgo() < 0) {
            Log_error0("Server_exec: Audio algorithm configure failed");
            return (-1);
        }

        Log_print1(Diags_INFO, "Server_exec: Audio sampleRate=%d",
                (IArg)Module.audioSampleRate);
        return (0);
    }

    channels = (UInt16)((payload >> 8) & 0xFF);
    bitDepth = (UInt16)(payload & 0xFF);
    if ((channels == 0) || (bitDepth == 0) || ((bitDepth % 8) != 0)) {
        Log_error1("Server_exec: Invalid audio format payload: 0x%x",
                payload);
        return (-1);
    }

    Module.audioChannels = channels;
    Module.audioBitDepth = bitDepth;
    Module.audioFrameBytes = (UInt16)(channels * (bitDepth / 8));
    if (Server_configureAudioAlgo() < 0) {
        Log_error0("Server_exec: Audio algorithm configure failed");
        return (-1);
    }

    Log_print2(Diags_INFO, "Server_exec: Audio channels=%d bitDepth=%d",
            (IArg)Module.audioChannels, (IArg)Module.audioBitDepth);
    return (0);
}

/*
 *  ======== Server_handleAlgorithmMode ========
 */
static Int Server_handleAlgorithmMode(UInt32 payload)
{
    AudioAlgoParams params;
    UInt32 i;

    if (payload > APP_ALGO_MODE_MAX) {
        Log_error1("Server_exec: Invalid algorithm mode: %d",
                (IArg)payload);
        return (-1);
    }

    params.mode = payload;
    params.flags = (payload == APP_ALGO_MODE_PASSTHROUGH) ?
            AUDIO_ALGO_FLAG_BYPASS : 0u;
    params.gainQ15 = AUDIO_ALGO_GAIN_UNITY_Q15;
    params.directionA = Module.directionA;
    params.directionB = Module.directionB;
    if (((payload == APP_ALGO_MODE_SPATIAL) && !(Module.directionMask & 1u)) ||
            ((payload == APP_ALGO_MODE_DUAL) && ((Module.directionMask & 3u) != 3u))) {
        return (-1);
    }
    for (i = 0; i < AUDIO_ALGO_RESERVED_WORDS; i++) {
        params.reserved[i] = 0;
    }

    if (AudioAlgo_setParams(&Module.audioAlgo, &params) < 0) {
        Log_error0("Server_exec: Audio algorithm params failed");
        return (-1);
    }
    Module.directionMask = 0; /* Each generic configuration requires fresh directions. */

    Log_print1(Diags_INFO, "Server_exec: Algorithm mode=%d",
            (IArg)payload);
    return (0);
}

/*
 *  ======== Server_handleRingConfig ========
 */
static Int Server_handleRingConfig(UInt32 payload)
{
    UInt16 slotCount;
    UInt16 slotBytes;

    slotCount = (UInt16)APP_SLOT_ID(payload);
    slotBytes = (UInt16)(payload & APP_DATA_MASK);

    if ((slotCount == 0) || (slotCount > APP_RING_MAX_SLOTS) ||
            (slotBytes == 0)) {
        Log_error1("Server_exec: Invalid ring config payload: 0x%x",
                payload);
        return (-1);
    }

    Module.ringSlotCount = slotCount;
    Module.ringSlotBytes = slotBytes;
    Log_print2(Diags_INFO, "Server_exec: Ring slots=%d slotBytes=%d",
            (IArg)slotCount, (IArg)slotBytes);
    return (0);
}

/*
 *  ======== Server_configureAudioAlgo ========
 */
static Int Server_configureAudioAlgo(Void)
{
    AudioAlgoConfig cfg;
    UInt32 i;

    cfg.sampleRate = Module.audioSampleRate;
    cfg.channels = Module.audioChannels;
    cfg.bitDepth = Module.audioBitDepth;
    cfg.frameBytes = Module.audioFrameBytes;
    cfg.maxFrameCount = APP_MAX_PAYLOAD_SIZE / Module.audioFrameBytes;
    for (i = 0; i < AUDIO_ALGO_RESERVED_WORDS; i++) {
        cfg.reserved[i] = 0;
    }

    return (AudioAlgo_configure(&Module.audioAlgo, &cfg));
}

/*
 *  ======== Server_recvSharedPtr ========
 */
static Int Server_recvSharedPtr(SharedRegion_SRPtr* sharedBufferPtr,
        Char** bufferPtr)
{
    Int     status  = 0;
    UInt32  event;
    UInt32  cmd;

    event = Server_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        Log_error1("Server_exec: Received queue error: 0x%x", event);
        status = -1;
        goto leave;
    }

    cmd = event & APP_CMD_MASK;
    if (cmd == APP_CMD_SHUTDOWN) {
        status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                Module.eventId, APP_CMD_SHUTDOWN_ACK, TRUE);
        if (status < 0) {
            Log_error0("Server_exec: Error sending early shutdown acknowledge");
        }
        goto leave;
    }

    if (cmd != APP_SPTR_LADDR) {
        Log_error1("Server_exec: Expected low address, got 0x%x", event);
        status = -1;
        goto leave;
    }

    *sharedBufferPtr = event & APP_SPTR_MASK;

    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_SPTR_ADDR_ACK, TRUE);

    if (status < 0) {
        Log_error0("Server_exec: Error sending low address acknowledge");
        goto leave;
    }

    event = Server_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        Log_error1("Server_exec: Received queue error: 0x%x", event);
        status = -1;
        goto leave;
    }

    if ((event & APP_CMD_MASK) != APP_SPTR_HADDR) {
        Log_error1("Server_exec: Expected high address, got 0x%x", event);
        status = -1;
        goto leave;
    }

    *sharedBufferPtr = ((event & APP_SPTR_MASK) << 16) | *sharedBufferPtr;

    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_SPTR_ADDR_ACK, TRUE);

    if (status < 0) {
        Log_error0("Server_exec: Error sending high address acknowledge");
        goto leave;
    }

    *bufferPtr = SharedRegion_getPtr(*sharedBufferPtr);
    if (*bufferPtr == NULL) {
        Log_error0("Server_exec: SharedRegion_getPtr failed");
        status = -1;
        goto leave;
    }

    Log_print0(Diags_INFO, "Server_exec: Shared buffer pointer received");

leave:
    return (status);
}

/*
 *  ======== Server_processBuffer ========
 */
static Int Server_processBuffer(Char* bufferPtr, UInt32 size)
{
    AudioAlgoFrame frame;
    UInt32 i;

    if ((bufferPtr == NULL) || (Module.audioFrameBytes == 0) ||
            (Module.audioBitDepth == 0) || ((Module.audioBitDepth % 8) != 0) ||
            ((size % Module.audioFrameBytes) != 0)) {
        Log_error1("Server_exec: Invalid PCM payload size=%d", (IArg)size);
        return (-1);
    }

    frame.inputData = (const Void *)bufferPtr;
    frame.outputData = (Void *)bufferPtr;
    frame.inputPcm = (const Int16 *)bufferPtr;
    frame.outputPcm = (Int16 *)bufferPtr;
    frame.workBuffer = NULL;
    frame.byteCount = size;
    frame.channels = Module.audioChannels;
    frame.bitDepth = Module.audioBitDepth;
    frame.frameCount = size / Module.audioFrameBytes;
    frame.sampleCount = size / (Module.audioBitDepth / 8);
    frame.sequence = Module.audioBlockSeq++;
    frame.workBytes = 0;
    frame.sampleRate = Module.audioSampleRate;
    frame.flags = 0;
    for (i = 0; i < AUDIO_ALGO_RESERVED_WORDS; i++) {
        frame.reserved[i] = 0;
    }

    return (AudioAlgo_process(&Module.audioAlgo, &frame));
}

/*
 *  ======== Server_delete ========
 *  1. unregister notify callback
 *  2. delete sync object
 */
    
Int Server_delete()
{
    Int     status = 0; 

    Log_print0(Diags_ENTRY, "--> Server_delete:");

    /* 1. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, Server_notifyCB,(UArg) &Module.eventQueue);

    if (status < 0) {
        Log_error0("Server_delete: Unregistering event has failed");
        goto leave;
    }

    /* 2. delete sync object */
    Semaphore_destruct(&Module.eventQueue.semObj);
    
    Log_print0(Diags_INFO,"Server_delete: Cleanup complete");

leave:
    Log_print1(Diags_EXIT, "<-- Server_delete: %d", (IArg)status);
    return (status);
}

/*
 *  ======== Server_exit ========
 */

Void Server_exit(Void)
{

    if (Module_curInit-- != 1) {
        return;  /* object still being used */
    }

    /*
     * Note that there isn't a Registry_removeModule() yet:
     *     https://bugs.eclipse.org/bugs/show_bug.cgi?id=315448
     *
     * ... but this is where we'd call it.
     */
}

/*
 *  ======== Server_notifyCB ========
 *
 *  This function is called from within an ISR
 */
Void Server_notifyCB( UInt16 procId, UInt16 lineId, UInt32  eventId, UArg arg,
        UInt32 payload)
{
    UInt            next;
    Event_Queue*    eventQueue = (Event_Queue *) arg;

    /* ignore no-op events */
    if (payload == APP_CMD_NOP) {
        return;
    }

    /* compute next slot in queue */
    next = (eventQueue->head + 1) % QUEUESIZE;

    if (next == eventQueue->tail) {
        /* queue is full, drop event and set error flag */
        eventQueue->error = APP_E_OVERFLOW;
        Semaphore_post(eventQueue->semH);
    }
    else {
        eventQueue->queue[eventQueue->head] = payload;
        
        /* queue head is only written to within this function */
        eventQueue->head = next;
        
        /* signal semaphore (counting) that new event is in queue */
        Semaphore_post(eventQueue->semH);
    }

    return;
}

/*
 *  ======== Server_waitForEvent ========
 * 
 */
static UInt32 Server_waitForEvent(Event_Queue* eventQueue)
{
    UInt32 event;

    if (eventQueue->error >= APP_E_FAILURE) {
        event = eventQueue->error;
    }
    else {
        /* use counting semaphore to wait for next event */
        Semaphore_pend(eventQueue->semH,  BIOS_WAIT_FOREVER);

        /* remove next command from queue */
        event = eventQueue->queue[eventQueue->tail];
        
        /* queue tail is only written to within this function */
        eventQueue->tail = (eventQueue->tail + 1) % QUEUESIZE;
    }

    return (event);
}
