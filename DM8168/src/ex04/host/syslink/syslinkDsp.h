/*
 * syslinkDsp.h
 *
 * ARM-side SysLink wrapper for the ARM -> DSP -> ARM processing loop.
 * This is the integration-facing API that ALSA or the main application
 * should call instead of touching Notify/SharedRegion directly.
 */

#ifndef SYSLINK_DSP_H
#define SYSLINK_DSP_H

#include "../../shared/AppCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *remoteProcName; /* usually "DSP" */
    int frameBytes;            /* channels * bytes_per_sample */
    int bufferBytes;           /* max payload bytes per process call */
    int sampleRate;            /* PCM sample rate, e.g. 8000 */
    int channels;              /* PCM channel count */
    int bitDepth;              /* PCM bits per sample */
    int algorithmMode;         /* APP_ALGO_MODE_* */
    int ringSlots;             /* shared-buffer slots; 0 uses default */
    int directionA;            /* HRIR_DIRECTION_CODE; used by generic spatial/dual */
    int directionB;            /* HRIR_DIRECTION_CODE; used by generic dual only */
} syslinkDspConfig;

/*
 * Initialize SysLink, start the remote DSP image that was loaded by
 * slaveloader/run.sh, register Notify, allocate SharedRegion buffer, and
 * send the buffer pointer to DSP.
 *
 * Returns 0 on success, -1 on failure.
 */
int syslinkDspInit(const syslinkDspConfig *cfg);

/*
 * Process one raw buffer through DSP.
 *
 * byteCount is the exact SysLink payload size. It must be positive, no larger
 * than cfg->bufferBytes, no larger than APP_MAX_PAYLOAD_SIZE, and aligned to
 * cfg->frameBytes because the DSP audio server derives frameCount from it.
 *
 * Returns processed byte count on success, -1 on failure.
 */
int syslinkDspProcessBuffer(const void *inData, void *outData, int byteCount);

/*
 * Process one interleaved PCM block through DSP.
 *
 * frameCount is multiplied by cfg->frameBytes from syslinkDspInit() to derive
 * the SysLink payload size. The buffers are intentionally void* so callers can
 * use S16_LE, S24-in-3-byte, S32_LE, or other frame layouts that match the
 * negotiated cfg->bitDepth.
 *
 * Returns processed frame count on success, -1 on failure.
 */
int syslinkDspProcessFrames(const void *inData, void *outData, int frameCount);

/*
 * Compatibility wrapper for the original S16_LE path.
 */
int syslinkDspProcess(const short *inPcm, short *outPcm, int frameCount);

/*
 * Send shutdown to DSP, unregister Notify, free SharedRegion buffer, stop the
 * SysLink remote processor callback, and destroy SysLink.
 *
 * Returns 0 on success, -1 on failure.
 */
int syslinkDspClose(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSLINK_DSP_H */
