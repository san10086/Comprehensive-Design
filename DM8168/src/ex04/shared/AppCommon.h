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
 *  ======== AppCommon.h ========
 *
 */

#ifndef AppCommon__include
#define AppCommon__include
#if defined (__cplusplus)
extern "C" {
#endif


/*
 *  ======== Application Configuration ========
 */

/* notify commands 00 - FF */
#define APP_CMD_MASK            0xFF000000
#define APP_DATA_MASK           0x0000FFFF

/* used by host and remote device to synchronize states with each other*/
#define APP_CMD_NOP             0x10000000  /* cc------ */
#define APP_CMD_OP_COMPLETE     0x11000000  /* cc------ */
#define APP_CMD_OP_FAILED       0x12000000  /* cc--eeee */
#define APP_SPTR_MASK           0x0000FFFF  /* ----pppp */
#define APP_SPTR_HADDR          0x20000000  /* cc--hhhh */
#define APP_SPTR_LADDR          0x21000000  /* cc--llll */
#define APP_SPTR_ADDR_ACK       0x22000000  /* cc------ */
#define APP_CMD_SHUTDOWN_ACK    0x30000000  /* cc------ */
#define APP_CMD_SHUTDOWN        0x31000000  /* cc------ */
#define APP_CMD_PROCESSING      0x50000000  /* cc--ssss */
#define APP_CMD_AUDIO_CFG_RATE  0x60000000  /* cc--rrrr: sample rate */
#define APP_CMD_AUDIO_CFG_FMT   0x61000000  /* cc--ccbb: channels, bitDepth */
#define APP_CMD_AUDIO_CFG_ACK   0x62000000  /* cc------ */
#define APP_CMD_ALGO_MODE       0x63000000  /* cc--mmmm: algorithm mode */
#define APP_CMD_ALGO_CFG_ACK    0x64000000  /* cc------ */
#define APP_CMD_RING_CFG        0x65000000  /* ccNNssss: slots, slot bytes */
#define APP_CMD_RING_CFG_ACK    0x66000000  /* cc------ */
#define APP_CMD_DIRECTION_A     0x67000000  /* cc--dddd: stable direction code */
#define APP_CMD_DIRECTION_B     0x68000000
#define APP_CMD_DIRECTION_ACK   0x69000000  /* cc--dddd: echoes validated code */
#define APP_CMD_ALGO_CFG_FAILED 0x6A000000  /* cc--eeee: invalid configuration */

#define APP_SLOT_SHIFT          16
#define APP_SLOT_MASK           0x00FF0000
#define APP_RING_MAX_SLOTS      8
#define APP_RING_DEFAULT_SLOTS  4
#define APP_SLOT_FIELD(slot) \
        ((((slot) & 0xFF) << APP_SLOT_SHIFT) & APP_SLOT_MASK)
#define APP_SLOT_ID(payload) \
        (((payload) & APP_SLOT_MASK) >> APP_SLOT_SHIFT)

#define APP_ALGO_MODE_PASSTHROUGH   0
#define APP_ALGO_MODE_SWAP_STEREO   1
#define APP_ALGO_MODE_GAIN_X2       2
#define APP_ALGO_MODE_SPEED2        3
#define APP_ALGO_MODE_GAIN_X3       4
#define APP_ALGO_MODE_GAIN_X5       5
#define APP_ALGO_MODE_SPATIAL_40E045 6
#define APP_ALGO_MODE_SPATIAL_60E250 7
#define APP_ALGO_MODE_ORBIT         8
#define APP_ALGO_MODE_DUAL_60_40    9
#define APP_ALGO_MODE_SPATIAL       10
#define APP_ALGO_MODE_DUAL          11
#define APP_ALGO_MODE_MAX           APP_ALGO_MODE_DUAL

/* program's shared region number */
#define SHARED_REGION_1         0x1

/* default payload size for smoke tests; protocol can carry up to 16-bit size */
#define APP_DEFAULT_BUFFER_SIZE 1024
#define APP_MAX_PAYLOAD_SIZE    APP_DATA_MASK
#define APP_TEST_DATA_SIZE      32
#define APP_TEST_ROUNDS         5

/* queue error message*/
#define APP_E_FAILURE           0xF0000000
#define APP_E_OVERFLOW          0xF1000000
#define APP_E_TIMEOUT           0xF2000000

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
#endif /* AppCommon__include */
