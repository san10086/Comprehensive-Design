#include "dsp_algorithm.h"
#include "AudioAlgo.h"

static int16_t dsp_saturating_multiply(int16_t value, int factor)
{
    int tmp = value * factor;

    if (tmp > 32767) {
        tmp = 32767;
    }
    if (tmp < -32768) {
        tmp = -32768;
    }

    return (int16_t)tmp;
}

int dsp_process_audio(int16_t *pData, int numSamples, int mode)
{
    int i;

    if ((pData == 0) || (numSamples <= 0)) {
        return -1;
    }

    switch (mode) {
        case AUDIO_ALGO_MODE_PASSTHROUGH:
            break;

        case AUDIO_ALGO_MODE_SWAP_STEREO:
            for (i = 0; i < numSamples; i++) {
                int16_t left = pData[2 * i];
                int16_t right = pData[(2 * i) + 1];

                pData[2 * i] = right;
                pData[(2 * i) + 1] = left;
            }
            break;

        case AUDIO_ALGO_MODE_GAIN_X2:
            for (i = 0; i < 2 * numSamples; i++) {
                pData[i] = dsp_saturating_multiply(pData[i], 2);
            }
            break;

        case AUDIO_ALGO_MODE_SPEED2:
            {
                int outFrames = (numSamples + 1) / 2;
                int srcFrame;

                for (i = 0; i < outFrames; i++) {
                    srcFrame = 2 * i;
                    pData[2 * i] = pData[2 * srcFrame];
                    pData[(2 * i) + 1] = pData[(2 * srcFrame) + 1];
                }
                for (i = outFrames; i < numSamples; i++) {
                    pData[2 * i] = 0;
                    pData[(2 * i) + 1] = 0;
                }
            }
            break;

        case AUDIO_ALGO_MODE_GAIN_X3:
            for (i = 0; i < 2 * numSamples; i++) {
                pData[i] = dsp_saturating_multiply(pData[i], 3);
            }
            break;

        case AUDIO_ALGO_MODE_GAIN_X5:
            for (i = 0; i < 2 * numSamples; i++) {
                pData[i] = dsp_saturating_multiply(pData[i], 10);
            }
            break;

        default:
            return -1;
    }

    return 0;
}
