"""Compile actual ARM/DSP control code against minimal TI transport mocks.

This verifies protocol/configuration logic on a PC, not the TI ABI or hardware.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADERS = [
    "xdc/std.h", "xdc/runtime/Assert.h", "xdc/runtime/Diags.h",
    "xdc/runtime/Log.h", "xdc/runtime/Registry.h", "xdc/runtime/System.h",
    "ti/ipc/Notify.h", "ti/ipc/SharedRegion.h", "ti/ipc/MultiProc.h",
    "ti/sysbios/knl/Task.h", "ti/sysbios/knl/Semaphore.h", "ti/sysbios/BIOS.h",
    "ti/syslink/Std.h", "ti/syslink/IpcHost.h", "ti/syslink/SysLink.h",
    "ti/syslink/utils/Memory.h",
]
TI = r'''
#ifndef TEST_TI_H
#define TEST_TI_H
#include <stdint.h>
#include <stddef.h>
typedef void Void;
typedef int Int;
typedef int Bool;
typedef unsigned int UInt;
typedef char Char;
typedef char *String;
typedef void *Ptr;
typedef int16_t Int16;
typedef uint16_t UInt16;
typedef int32_t Int32;
typedef uint32_t UInt32;
typedef uintptr_t UArg;
typedef intptr_t IArg;
typedef void *IHeap_Handle;
typedef uint32_t SharedRegion_SRPtr;
typedef int Registry_Desc;
typedef int Registry_Result;
typedef int Assert_Id;
typedef struct { int unused; } Semaphore_Struct;
typedef void *Semaphore_Handle;
typedef struct { int mode; } Semaphore_Params;
#define TRUE 1
#define FALSE 0
#define Notify_S_SUCCESS 0
#define Notify_E_EVTNOTREGISTERED -2
#define Registry_SUCCESS 0
#define Diags_ENTRY 0
#define Diags_EXIT 0
#define Diags_INFO 0
#define Semaphore_Mode_COUNTING 0
#define BIOS_WAIT_FOREVER 0
#define Ipc_CONTROLCMD_LOADCALLBACK 0
#define Ipc_CONTROLCMD_STARTCALLBACK 1
#define Ipc_CONTROLCMD_STOPCALLBACK 2
#define Assert_isTrue(test, id) ((void)(test))
#define Log_print0(...) ((void)0)
#define Log_print1(...) ((void)0)
#define Log_print2(...) ((void)0)
#define Log_error0(...) ((void)0)
#define Log_error1(...) ((void)0)
int Notify_sendEvent(UInt16, UInt16, UInt32, UInt32, int);
int Notify_registerEvent(UInt16, UInt16, UInt32, void (*)(UInt16, UInt16, UInt32, UArg, UInt32), UArg);
int Notify_unregisterEvent(UInt16, UInt16, UInt32, void (*)(UInt16, UInt16, UInt32, UArg, UInt32), UArg);
void *SharedRegion_getPtr(SharedRegion_SRPtr);
void *SharedRegion_getHeap(int);
SharedRegion_SRPtr SharedRegion_getSRPtr(void *, int);
int Registry_addModule(Registry_Desc *, const char *);
void Diags_setMask(const char *);
void Semaphore_Params_init(Semaphore_Params *);
void Semaphore_construct(Semaphore_Struct *, int, Semaphore_Params *);
Semaphore_Handle Semaphore_handle(Semaphore_Struct *);
void Semaphore_destruct(Semaphore_Struct *);
void Semaphore_post(Semaphore_Handle);
int Semaphore_pend(Semaphore_Handle, int);
void Task_sleep(int);
UInt16 MultiProc_getId(String);
int Ipc_control(UInt16, int, void *);
void SysLink_setup(void);
void SysLink_destroy(void);
void *Memory_calloc(IHeap_Handle, int, int, void *);
void Memory_free(IHeap_Handle, void *, int);
#endif
'''
SERVER = r'''
#include <assert.h>
#include <string.h>
#include "dsp/Server.c"
static UInt32 events[64], replies[64];
static int position, length, count;
static short pcm[2048];
int Semaphore_pend(Semaphore_Handle h, int timeout) {
    (void)h; (void)timeout;
    assert(position < length);
    Module.eventQueue.queue[Module.eventQueue.tail] = events[position++];
    return 1;
}
int Notify_sendEvent(UInt16 p, UInt16 l, UInt32 e, UInt32 value, int wait) {
    (void)p; (void)l; (void)e; (void)wait;
    assert(count < 64); replies[count++] = value; return 0;
}
void *SharedRegion_getPtr(SharedRegion_SRPtr ptr) { (void)ptr; return pcm; }
static void begin(void) {
    memset(&Module, 0, sizeof(Module));
    memset(pcm, 0, sizeof(pcm));
    AudioAlgo_init(&Module.audioAlgo);
    Module.audioSampleRate = 8000; Module.audioChannels = 2;
    Module.audioBitDepth = 16; Module.audioFrameBytes = 4;
    Module.ringSlotCount = 1; Module.ringSlotBytes = sizeof(pcm);
    position = length = count = 0;
    events[length++] = APP_SPTR_LADDR;
    events[length++] = APP_SPTR_HADDR;
    events[length++] = APP_CMD_AUDIO_CFG_RATE | 44100;
    events[length++] = APP_CMD_AUDIO_CFG_FMT | (2 << 8) | 16;
}
static void finish(void) {
    events[length++] = APP_CMD_SHUTDOWN;
    assert(Server_exec() == 0);
    assert(position == length);
    assert(replies[count - 1] == APP_CMD_SHUTDOWN_ACK);
}
int main(void) {
    const int a = HRIR_DIRECTION_CODE(-40, 45), b = HRIR_DIRECTION_CODE(90, 0);
    AudioAlgoParams params;
    begin();
    events[length++] = APP_CMD_DIRECTION_A | a;
    events[length++] = APP_CMD_DIRECTION_B | b;
    events[length++] = APP_CMD_ALGO_MODE | APP_ALGO_MODE_DUAL;
    events[length++] = APP_CMD_PROCESSING | 256;
    finish();
    assert(replies[4] == (APP_CMD_DIRECTION_ACK | (UInt32)a));
    assert(replies[5] == (APP_CMD_DIRECTION_ACK | (UInt32)b));
    assert(replies[6] == APP_CMD_ALGO_CFG_ACK);
    assert(replies[7] == APP_CMD_OP_COMPLETE);
    assert(Module.audioAlgo.params.directionA == a);
    assert(Module.audioAlgo.params.directionB == b);
    assert(Module.audioAlgo.spatialState.preparedDual);

    begin();
    events[length++] = APP_CMD_DIRECTION_A | a;
    events[length++] = APP_CMD_DIRECTION_A; /* Missing direction -90,0 invalidates A. */
    events[length++] = APP_CMD_ALGO_MODE | APP_ALGO_MODE_SPATIAL;
    finish();
    assert(replies[5] == (APP_CMD_ALGO_CFG_FAILED | 1u));
    assert(replies[6] == (APP_CMD_ALGO_CFG_FAILED | 2u));

    begin();
    events[length++] = APP_CMD_DIRECTION_A | a;
    events[length++] = APP_CMD_ALGO_MODE | APP_ALGO_MODE_DUAL; /* B missing. */
    events[length++] = APP_CMD_ALGO_MODE | APP_ALGO_MODE_SPATIAL;
    events[length++] = APP_CMD_ALGO_MODE | APP_ALGO_MODE_SPATIAL; /* Must send fresh A. */
    finish();
    assert(replies[5] == (APP_CMD_ALGO_CFG_FAILED | 2u));
    assert(replies[6] == APP_CMD_ALGO_CFG_ACK);
    assert(replies[7] == (APP_CMD_ALGO_CFG_FAILED | 2u));

    params = Module.audioAlgo.params;
    params.directionA = -1;
    assert(AudioAlgo_setParams(&Module.audioAlgo, &params) < 0);
    params.directionA = a;
    Module.audioAlgo.config.sampleRate = 48000;
    assert(AudioAlgo_setParams(&Module.audioAlgo, &params) < 0);
    puts("DSP protocol/algorithm adapter PASS");
    return 0;
}
'''
HOST = r'''
#include <assert.h>
#include "host/syslink/syslinkDsp.c"
static int responseMode;
static UInt32 lastCommand;
int Notify_sendEvent(UInt16 p, UInt16 l, UInt32 e, UInt32 value, int wait) {
    UInt32 reply;
    (void)p; (void)l; (void)e; (void)wait;
    lastCommand = value;
    if ((value & APP_CMD_MASK) == APP_CMD_ALGO_MODE) reply = APP_CMD_ALGO_CFG_ACK;
    else reply = APP_CMD_DIRECTION_ACK | (value & APP_DATA_MASK);
    if (responseMode == 1) reply = APP_CMD_ALGO_CFG_FAILED | 1;
    if (responseMode == 2) reply ^= 1;
    Module.eventQueue.queue[Module.eventQueue.head] = reply;
    Module.eventQueue.head = (Module.eventQueue.head + 1) % SYSLINK_DSP_QUEUE_SIZE;
    sem_post(&Module.eventQueue.semH);
    return 0;
}
int main(void) {
    const int code = HRIR_DIRECTION_CODE(90, 0);
    syslinkDspResetModule();
    assert(sem_init(&Module.eventQueue.semH, 0, 0) == 0);
    assert(syslinkDspSendDirection(APP_CMD_DIRECTION_A, code) == 0);
    assert(lastCommand == (APP_CMD_DIRECTION_A | (UInt32)code));
    assert(syslinkDspSendDirection(APP_CMD_DIRECTION_B, code) == 0);
    assert(lastCommand == (APP_CMD_DIRECTION_B | (UInt32)code));
    responseMode = 1;
    assert(syslinkDspSendDirection(APP_CMD_DIRECTION_A, code) < 0);
    responseMode = 2;
    assert(syslinkDspSendDirection(APP_CMD_DIRECTION_A, code) < 0);
    responseMode = 0;
    Module.algorithmMode = APP_ALGO_MODE_DUAL;
    assert(syslinkDspSendAlgorithmMode() == 0);
    responseMode = 1;
    assert(syslinkDspSendAlgorithmMode() < 0);
    sem_destroy(&Module.eventQueue.semH);
    puts("ARM direction/algorithm acknowledgements PASS");
    return 0;
}
'''


class ProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="ex04-ti-mocks-")
        cls.directory = Path(cls.temporary.name)
        (cls.directory / "test_ti.h").write_text(TI)
        for name in HEADERS:
            path = cls.directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "test_ti.h"\n')

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def compile_and_run(self, name, source, additional):
        path = self.directory / (name + ".c")
        binary = self.directory / name
        path.write_text(source)
        subprocess.run([os.environ.get("CC", "gcc"), "-std=c99", "-D_DEFAULT_SOURCE",
                        "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                        "-Wno-int-to-pointer-cast", "-O2", "-ffunction-sections", "-fdata-sections",
                        "-Wl,--gc-sections", "-pthread", "-I" + str(self.directory), "-I" + str(ROOT),
                        str(path)] + [str(ROOT / name) for name in additional] + ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=5)

    def test_dsp_config_and_dispatch(self):
        self.compile_and_run("server", SERVER,
                             ["dsp/AudioAlgo.c", "dsp/dsp_spatial.c", "dsp/dsp_algorithm.c"])

    def test_arm_direction_acknowledgements(self):
        self.compile_and_run("host", HOST, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
