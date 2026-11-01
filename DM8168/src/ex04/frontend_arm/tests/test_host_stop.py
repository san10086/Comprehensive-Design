#!/usr/bin/env python3
"""Compile the actual host main against hardware mocks and test SIGINT cleanup.

This validates host control flow only; it does not validate ALSA or TI firmware.
"""
import os
import re
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
MOCKS = r'''
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "host/audio/audioCapture.h"
#include "host/audio/audioPlayback.h"
#include "host/audio/audioWav.h"
#include "host/syslink/syslinkDsp.h"

static audioCapCallback callback;
static void *callback_data;
static int remaining;
static void pause_ms(long ms) {
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}
audioConfig audioGetConfig(void) {
    audioConfig c;
    memset(&c, 0, sizeof(c));
    c.devCapture = c.devPlayback = "mock";
    c.sampleRate = 44100; c.channels = 2; c.bitDepth = 16;
    c.periodFrames = 1024; c.bufferBytes = 4096;
    return c;
}
int audioCapInit(audioConfig *c) { (void)c; pause_ms(100); return 0; }
int audioCapSetCallback(audioCapCallback cb, void *data) {
    callback = cb; callback_data = data; return 0;
}
int audioCapStart(int duration) {
    short pcm[2048] = {0};
    int i;
    (void)duration;
    puts("MOCK_CAPTURE_START");
    for (i = 0; i < 1000; ++i) {
        pause_ms(10);
        if (callback(pcm, 1024, callback_data)) break;
    }
    return 0;
}
int audioCapStop(void) { return 0; }
void audioCapClose(void) { puts("MOCK_CAPTURE_CLOSE"); }
int audioPbInit(audioConfig *c) { (void)c; return 0; }
int audioPbWrite(short *pcm, int frames) { (void)pcm; return frames; }
int audioPbDrain(void) { return 0; }
void audioPbClose(void) { puts("MOCK_PLAYBACK_CLOSE"); }
int audioWavOpenWrite(audioWavFile *w, const char *name, int rate, int ch, int bit) {
    (void)w; (void)name; (void)rate; (void)ch; (void)bit; return 0;
}
int audioWavOpenRead(audioWavFile *w, const char *name) {
    (void)name;
    memset(w, 0, sizeof(*w));
    w->sampleRate = 44100; w->channels = 2; w->bitDepth = 16;
    w->blockAlign = 4; w->dataSize = 4096 * 1000;
    remaining = 1024 * 1000;
    return 0;
}
int audioWavWriteFrames(audioWavFile *w, short *pcm, int frames) {
    (void)w; (void)pcm; return frames;
}
int audioWavReadFrames(audioWavFile *w, short *pcm, int frames) {
    (void)w;
    if (!remaining) return 0;
    memset(pcm, 0, (size_t)frames * 4);
    remaining -= frames;
    return frames;
}
void audioWavClose(audioWavFile *w) { (void)w; puts("MOCK_WAV_CLOSE"); }
int syslinkDspInit(const syslinkDspConfig *c) {
    printf("MOCK_DSP_INIT algo=%d A=%d B=%d\n", c->algorithmMode, c->directionA, c->directionB);
    pause_ms(100); return 0;
}
int syslinkDspProcessFrames(const void *in, void *out, int frames) {
    pause_ms(10); memcpy(out, in, (size_t)frames * 4); return frames;
}
int syslinkDspClose(void) { puts("MOCK_DSP_CLOSE"); return 0; }
'''


class HostStopTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix="ex04-host-stop-")
        directory = Path(cls.work.name)
        (directory / "alsa").mkdir()
        (directory / "alsa/asoundlib.h").write_text(
            "#ifndef TEST_ALSA_H\n#define TEST_ALSA_H\n"
            "typedef int snd_pcm_format_t; typedef int snd_pcm_access_t;\n#endif\n")
        mocks = directory / "hardware_mocks.c"
        mocks.write_text(MOCKS)
        cls.binary = directory / "app_host"
        subprocess.run([
            os.environ.get("CC", "gcc"), "-std=c99", "-D_DEFAULT_SOURCE",
            "-DPLATFORM_DM8168", "-Wall", "-Wextra", "-Werror", "-pthread",
            "-I" + str(directory), "-I" + str(ROOT),
            str(ROOT / "host/app/main.c"), str(mocks), "-o", str(cls.binary)
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def stopped_run(self, args, marker, dsp):
        process = subprocess.Popen([str(self.binary)] + args,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = b""
        try:
            deadline = time.monotonic() + 3
            while marker not in output:
                self.assertLess(time.monotonic(), deadline, output.decode())
                readable, _, _ = select.select([process.stdout], [], [], 0.1)
                if readable:
                    chunk = os.read(process.stdout.fileno(), 4096)
                    self.assertTrue(chunk, output.decode())
                    output += chunk
            process.send_signal(signal.SIGINT)
            rest, _ = process.communicate(timeout=3)
            output += rest
            self.assertEqual(process.returncode, 0, output.decode())
            self.assertIn(b"MOCK_DSP_CLOSE", output) if dsp else None
            self.assertNotIn(b"frame count mismatch", output)
            return output
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)
            process.stdout.close()

    def test_stop_immediately_after_ready(self):
        for args, dsp in [(["DSP", "smoke"], True),
                          (["record", "out.wav", "0"], False),
                          (["DSP", "audio", "0"], True),
                          (["DSP", "file", "in.wav", "out.wav"], True)]:
            with self.subTest(args=args):
                output = self.stopped_run(args, b"main: control ready\n", dsp)
                self.assertNotIn(b"MOCK_CAPTURE_START", output)

    def test_stop_active_record(self):
        output = self.stopped_run(["record", "out.wav", "0"], b"MOCK_CAPTURE_START\n", False)
        self.assertIn(b"MOCK_CAPTURE_CLOSE", output)

    def test_stop_active_audio(self):
        output = self.stopped_run(["DSP", "audio", "0"], b"MOCK_CAPTURE_START\n", True)
        self.assertIn(b"MOCK_PLAYBACK_CLOSE", output)

    def test_stop_active_file(self):
        self.stopped_run(["DSP", "file", "in.wav", "out.wav"], b"main: file block:", True)

    def test_normal_smoke(self):
        result = subprocess.run([str(self.binary), "DSP", "smoke"], capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 0, result.stdout.decode())
        self.assertEqual(result.stdout.count(b"main: round "), 5)
        self.assertIn(b"MOCK_DSP_CLOSE", result.stdout)

    def test_named_options_without_hardware(self):
        valid = [
            ["file", "--input", "input name.wav", "--output", "out.wav", "--algo", "spatial", "--elevation", "60", "--azimuth", "250"],
            ["audio", "--seconds", "10", "--algo", "spatial", "--elevation", "-40", "--azimuth", "45"],
            ["file", "--input", "in.wav", "--algo", "dual", "--a-elevation", "60", "--a-azimuth", "250", "--b-elevation", "40", "--b-azimuth", "45"],
            ["file", "--input", "in.wav"],
            ["record", "--output", "out.wav", "--seconds", "0"],
            ["smoke", "--algo", "swap"],
            ["audio", "0", "spatial40"], ["file", "in.wav", "out.wav", "dual"],
        ]
        for args in valid:
            with self.subTest(args=args):
                result = subprocess.run([str(self.binary), "--check"] + args,
                                        capture_output=True, timeout=3)
                self.assertEqual(result.returncode, 0, result.stdout.decode())
                self.assertIn(b"check PASS", result.stdout)
                self.assertNotIn(b"MOCK_", result.stdout)

    def test_rejected_options(self):
        base = ["file", "--input", "in.wav", "--algo", "spatial"]
        invalid = [
            ["--unknown"],
            base, base + ["--elevation", "60", "--azimuth", "251"],
            base + ["--elevation", "91", "--azimuth", "0"],
            base + ["--elevation", "0", "--azimuth", "360"],
            base + ["--elevation", "0x0", "--azimuth", "0"],
            ["audio", "--seconds", "2148"], ["audio", "9999999999999999999999999"],
            ["file", "--input", "in.wav", "--seconds", "1"],
            ["file", "--input", "in.wav", "--input", "again.wav"],
            ["file", "in.wav", "--algo", "spatial"],
            ["audio", "10", "--output"],
            ["audio", "10", "--algo", "swap"],
            ["audio", "--algo", "swap", "--elevation", "0", "--azimuth", "0"],
            ["smoke", "--algo", "spatial", "--elevation", "0", "--azimuth", "0"],
            ["audio", "--algo", "speed2"], ["record", "--algo", "swap", "--output", "out.wav"],
            ["file", "--input", "in.wav", "--unknown", "x"],
            ["file", "--input", "in.wav", "--algo", "dual", "--a-elevation", "0", "--a-azimuth", "0"],
        ]
        for args in invalid:
            with self.subTest(args=args):
                result = subprocess.run([str(self.binary), "--check"] + args,
                                        capture_output=True, timeout=3)
                self.assertNotEqual(result.returncode, 0, result.stdout.decode())
                self.assertNotIn(b"MOCK_DSP_INIT", result.stdout)

    def test_directions_and_help(self):
        result = subprocess.run([str(self.binary), "directions"], capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 0)
        catalog = (ROOT / "shared/hrir_catalog.h").read_text()
        count = int(re.search(r"#define HRIR_DIRECTION_COUNT (\d+)", catalog).group(1))
        self.assertEqual(len(result.stdout.decode().splitlines()), count + 2)
        self.assertIn(b"-40 45 18045\n", result.stdout)
        self.assertIn(b"90 0 64800\n", result.stdout)
        self.assertNotIn(b"MOCK_", result.stdout)
        result = subprocess.run([str(self.binary), "--help"], capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 0)
        self.assertNotIn(b"MOCK_", result.stdout)

    def test_new_directions_reach_pipeline(self):
        output = self.stopped_run(["DSP", "audio", "--seconds", "0", "--algo", "spatial",
                                   "--elevation", "-40", "--azimuth", "45"], b"MOCK_CAPTURE_START\n", True)
        self.assertIn(b"MOCK_DSP_INIT algo=10 A=18045 B=-1", output)
        output = self.stopped_run(["DSP", "file", "--input", "in.wav", "--algo", "dual",
                                   "--a-elevation", "60", "--a-azimuth", "250",
                                   "--b-elevation", "40", "--b-azimuth", "45"], b"main: file block:", True)
        self.assertIn(b"MOCK_DSP_INIT algo=11 A=54250 B=46845", output)

    def test_run_wrapper_queries_do_not_load_dsp(self):
        directory = Path(self.work.name)
        # The wrapper uses ./app_host; this is already the mock-linked test binary.
        loader = directory / "slaveloader"
        loader.write_text("#!/bin/sh\necho UNEXPECTED_LOADER\nexit 97\n")
        loader.chmod(0o755)
        for args, expected in [(["directions"], 0), (["--help"], 0),
                               (["file", "--input", "in.wav", "--algo", "spatial"], 1),
                               (["audio", "--seconds", "1", "--check"], 0)]:
            result = subprocess.run(["sh", str(ROOT / "run.sh")] + args, cwd=directory,
                                    capture_output=True, timeout=3)
            self.assertEqual(result.returncode, expected, result.stdout.decode())
            self.assertNotIn(b"UNEXPECTED_LOADER", result.stdout)
            self.assertNotIn(b"MOCK_DSP_INIT", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
