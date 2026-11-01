#ifndef DSP_ALGORITHM_H
#define DSP_ALGORITHM_H

#include <stdint.h>

/*
 * Process interleaved S16_LE stereo PCM in-place.
 * numSamples is the frame count per channel.
 * mode: 0=passthrough, 1=swap, 2=gain2, 3=speed2, 4=gain3, 5=gain5.
 * Returns 0 on success or -1 on unsupported mode.
 */
int dsp_process_audio(int16_t *pData, int numSamples, int mode);

#endif
