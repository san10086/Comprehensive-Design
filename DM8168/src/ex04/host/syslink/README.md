# ARM SysLink DSP Wrapper

This directory contains the ARM-side integration wrapper for the SysLink
ARM -> DSP -> ARM processing loop.

## Ownership

This module is project-owned integration code. It may call `host/audio/`, but it
must not modify member A's ALSA module behavior or public interfaces without
approval.

## Public API

Use `syslinkDsp.h`:

```c
syslinkDspConfig cfg;

memset(&cfg, 0, sizeof(cfg));
cfg.remoteProcName = "DSP";
cfg.frameBytes = channels * sizeof(short);
cfg.bufferBytes = 1024;
cfg.sampleRate = 44100;
cfg.channels = channels;
cfg.bitDepth = 16;
cfg.algorithmMode = APP_ALGO_MODE_PASSTHROUGH;

if (syslinkDspInit(&cfg) < 0) {
    /* handle error */
}

ret = syslinkDspProcessFrames(inPcm, outPcm, frameCount);

syslinkDspClose();
```

## ALSA Mapping

For the current `host/audio/` module:

```c
cfg.frameBytes = audioCfg.channels * (audioCfg.bitDepth / 8);
frameCount = callbackFrameCount;
```

`syslinkDspProcessFrames()` computes:

```c
payloadBytes = frameCount * cfg.frameBytes;
```

For non-`short` PCM layouts or callers that already measure payloads in bytes,
use the lower-level API:

```c
ret = syslinkDspProcessBuffer(inData, outData, byteCount);
```

`byteCount` must be aligned to `cfg.frameBytes`, because the DSP server derives
`frameCount` from the byte payload size.

The default smoke-test buffer size is `APP_DEFAULT_BUFFER_SIZE` from
`shared/AppCommon.h`. A caller may request a larger `cfg.bufferBytes` up to
`APP_MAX_PAYLOAD_SIZE`, currently 65535 bytes because the event payload carries
the data size in 16 bits.

## Current DSP Behavior

The paired DSP code in `dsp/Server.c` selects the algorithm through
`cfg.algorithmMode`. Supported modes are `APP_ALGO_MODE_PASSTHROUGH`,
`APP_ALGO_MODE_SWAP_STEREO`, `APP_ALGO_MODE_GAIN_X2`,
`APP_ALGO_MODE_SPEED2`, `APP_ALGO_MODE_GAIN_X3`,
`APP_ALGO_MODE_GAIN_X5`, `APP_ALGO_MODE_SPATIAL_40E045`, and
`APP_ALGO_MODE_SPATIAL_60E250`.

The processing contract is still the same Notify/SharedRegion protocol.
