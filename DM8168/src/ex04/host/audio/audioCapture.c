/*
 * Migrated copy from ALSA/.
 * Ownership note: this module originates from member A's ALSA deliverable.
 * Do not change behavior or public interfaces without requesting approval first.
 */
/*
 * audioCapture.c — 录音模块实现
 */

#include "audioCapture.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* 模块内部状态 */
static snd_pcm_t        *capHandle       = NULL;
static audioConfig       capConfig;
static audioCapCallback  capCallback     = NULL;
static void             *capUserData     = NULL;
static volatile int      capStopFlag     = 0;
static int               capInitialized  = 0;

int audioCapInit(audioConfig *cfg) {
    int rc;

    if (!cfg) return -1;
    if (capInitialized) {
        *cfg = capConfig;
        return 0;  /* 已初始化 */
    }

    capConfig = *cfg;

    /* 打开录音 PCM 设备 */
    rc = snd_pcm_open(&capHandle, capConfig.devCapture,
                      SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "[Capture] unable to open pcm device %s: %s\n",
                capConfig.devCapture, snd_strerror(rc));
        return -1;
    }

    /* 分配硬件参数对象 */
    snd_pcm_hw_params_t *hwParams;
    snd_pcm_hw_params_malloc(&hwParams);
    snd_pcm_hw_params_any(capHandle, hwParams);

    /* 设置硬件参数 */
    rc  = snd_pcm_hw_params_set_access(capHandle, hwParams, capConfig.access);
    rc |= snd_pcm_hw_params_set_format(capHandle, hwParams, capConfig.format);
    rc |= snd_pcm_hw_params_set_channels(capHandle, hwParams, capConfig.channels);

    unsigned int rate = capConfig.sampleRate;
    int dir = 0;
    rc |= snd_pcm_hw_params_set_rate_near(capHandle, hwParams, &rate, &dir);
    capConfig.sampleRate = rate;  /* 记录实际采样率 */

    snd_pcm_uframes_t pf = capConfig.periodFrames;
    rc |= snd_pcm_hw_params_set_period_size_near(capHandle, hwParams, &pf, &dir);
    capConfig.periodFrames = (int)pf;

    unsigned int bufferTime = capConfig.periods * 100000;  /* 微秒 */
    rc |= snd_pcm_hw_params_set_buffer_time_near(capHandle, hwParams, &bufferTime, &dir);

    rc |= snd_pcm_hw_params(capHandle, hwParams);
    if (rc < 0) {
        fprintf(stderr, "[Capture] unable to set hw params: %s\n",
                snd_strerror(rc));
        snd_pcm_hw_params_free(hwParams);
        return -1;
    }

    /* 获取实际 period 大小 */
    snd_pcm_hw_params_get_period_size(hwParams, &pf, &dir);
    capConfig.periodFrames = (int)pf;

    snd_pcm_hw_params_free(hwParams);

    capStopFlag    = 0;
    capInitialized = 1;
    capConfig.bufferBytes = capConfig.periodFrames * capConfig.channels *
            (capConfig.bitDepth / 8);
    *cfg = capConfig;

    printf("[Capture] initialized: dev=%s rate=%u ch=%d period=%d frames\n",
           capConfig.devCapture, capConfig.sampleRate,
           capConfig.channels, capConfig.periodFrames);

    return 0;
}

int audioCapSetCallback(audioCapCallback cb, void *userData) {
    if (!capInitialized) return -1;

    capCallback = cb;
    capUserData = userData;
    return 0;
}

int audioCapStart(int durationMs) {
    int rc;
    snd_pcm_sframes_t framesRead;
    char *buffer;
    int bufferBytes;
    int loopCount;
    unsigned int periodTimeUs;
    int dir;

    if (!capInitialized || !capCallback) {
        fprintf(stderr, "[Capture] not initialized or no callback set\n");
        return -1;
    }

    bufferBytes = capConfig.bufferBytes;
    buffer = (char *)malloc((size_t)bufferBytes);
    if (!buffer) {
        fprintf(stderr, "[Capture] malloc failed\n");
        return -1;
    }

    /* 计算循环次数 */
    snd_pcm_hw_params_t *hwParams;
    snd_pcm_hw_params_malloc(&hwParams);
    snd_pcm_hw_params_current(capHandle, hwParams);
    snd_pcm_hw_params_get_period_time(hwParams, &periodTimeUs, &dir);
    snd_pcm_hw_params_free(hwParams);

    if (durationMs > 0) {
        loopCount = (durationMs * 1000) / (int)periodTimeUs;
        printf("[Capture] start recording %d ms, loopCount=%d, periodTime=%u us\n",
               durationMs, loopCount, periodTimeUs);
    } else {
        loopCount = -1;  /* 无限循环 */
        printf("[Capture] start recording (infinite mode)\n");
    }

    /* 预填充（prepare） */
    rc = snd_pcm_prepare(capHandle);
    if (rc < 0) {
        fprintf(stderr, "[Capture] prepare failed: %s\n", snd_strerror(rc));
        free(buffer);
        return -1;
    }

    capStopFlag = 0;

    while (!capStopFlag && (loopCount < 0 || loopCount > 0)) {
        if (loopCount > 0) loopCount--;

        framesRead = snd_pcm_readi(capHandle, buffer,
                                   (snd_pcm_uframes_t)capConfig.periodFrames);

        if (framesRead == -EPIPE) {
            /* overrun */
            fprintf(stderr, "[Capture] overrun occurred\n");
            snd_pcm_prepare(capHandle);
            continue;
        } else if (framesRead < 0) {
            fprintf(stderr, "[Capture] error from read: %s\n",
                    snd_strerror((int)framesRead));
            snd_pcm_prepare(capHandle);
            continue;
        }

        /* 通过回调把数据交给上层 */
        rc = capCallback((short *)buffer, (int)framesRead, capUserData);
        if (rc != 0) {
            printf("[Capture] stopped by callback\n");
            break;
        }
    }

    snd_pcm_drain(capHandle);
    free(buffer);
    printf("[Capture] recording stopped\n");
    return 0;
}

int audioCapStop(void) {
    capStopFlag = 1;
    return 0;
}

void audioCapClose(void) {
    if (capHandle) {
        snd_pcm_close(capHandle);
        capHandle = NULL;
    }
    capInitialized = 0;
    capCallback    = NULL;
    capUserData    = NULL;
    capStopFlag    = 0;
    printf("[Capture] closed\n");
}

