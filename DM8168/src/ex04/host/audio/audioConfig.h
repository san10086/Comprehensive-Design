/*
 * Migrated copy from ALSA/.
 * Ownership note: this module originates from member A's ALSA deliverable.
 * Do not change behavior or public interfaces without requesting approval first.
 */
/*
 * audioConfig.h — 音频硬件参数集中管理
 * 通过 Makefile 传入 PLATFORM_X86 或 PLATFORM_DM8168 选择平台。
 */

#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include <alsa/asoundlib.h>

/* ===== 目标平台选择（二选一，由 Makefile 通过 -D 传入） ===== */
#if !defined(PLATFORM_X86) && !defined(PLATFORM_DM8168)
  #error "Please define one platform macro: PLATFORM_X86 or PLATFORM_DM8168"
#endif

#if defined(PLATFORM_X86) && defined(PLATFORM_DM8168)
  #error "Please define only one platform macro"
#endif

/* ===== 平台差异参数 ===== */
#ifdef PLATFORM_DM8168
  #define AUDIO_DEV_CAPTURE      "default"
  #define AUDIO_DEV_PLAYBACK     "default"
  #define AUDIO_SAMPLE_RATE      44100
  #define AUDIO_PERIOD_FRAMES    1024
#endif

#ifdef PLATFORM_X86
  #define AUDIO_DEV_CAPTURE      "hw:0,0"
  #define AUDIO_DEV_PLAYBACK     "hw:0,0"
  #define AUDIO_SAMPLE_RATE      44100
  #define AUDIO_PERIOD_FRAMES    256
#endif

/* ===== 通用参数（两平台一致） ===== */
#define AUDIO_CHANNELS          2
#define AUDIO_FORMAT            SND_PCM_FORMAT_S16_LE
#define AUDIO_ACCESS            SND_PCM_ACCESS_RW_INTERLEAVED
#define AUDIO_PERIODS           4
#define AUDIO_BIT_DEPTH         16

/* 计算 Buffer 大小（字节）：frames * channels * (bitDepth/8) */
#define AUDIO_BUFFER_SIZE(frames) \
    ((frames) * AUDIO_CHANNELS * (AUDIO_BIT_DEPTH / 8))

/* ===== 导出给外部模块的配置结构体 ===== */
typedef struct {
    const char    *devCapture;
    const char    *devPlayback;
    unsigned int   sampleRate;
    int            channels;
    snd_pcm_format_t format;
    snd_pcm_access_t access;
    int            periodFrames;
    int            periods;
    int            bitDepth;
    int            bufferBytes;
} audioConfig;

/* 获取当前平台配置 */
audioConfig audioGetConfig(void);

#endif /* AUDIO_CONFIG_H */

