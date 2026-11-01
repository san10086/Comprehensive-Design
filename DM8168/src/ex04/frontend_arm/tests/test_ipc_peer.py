#!/usr/bin/env python3
"""Linux integration tests for the real C peer, without Qt or third-party modules."""
import ctypes
import errno
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import unittest


class SemOp(ctypes.Structure):
    _fields_ = [("number", ctypes.c_ushort), ("change", ctypes.c_short),
                ("flags", ctypes.c_short)]


class Message(ctypes.Structure):
    _fields_ = [("sequence", ctypes.c_uint), ("text", ctypes.c_char * 256)]


LIBC = ctypes.CDLL(None, use_errno=True)
LIBC.semget.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
LIBC.semget.restype = ctypes.c_int
LIBC.semop.argtypes = [ctypes.c_int, ctypes.POINTER(SemOp), ctypes.c_size_t]
LIBC.semop.restype = ctypes.c_int
LIBC.shmget.argtypes = [ctypes.c_int, ctypes.c_size_t, ctypes.c_int]
LIBC.shmget.restype = ctypes.c_int
LIBC.shmat.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
LIBC.shmat.restype = ctypes.c_void_p
LIBC.shmdt.argtypes = [ctypes.c_void_p]
LIBC.shmdt.restype = ctypes.c_int
LIBC.shmctl.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
LIBC.shmctl.restype = ctypes.c_int
PEER = Path(os.environ.get("EX04_IPC_PEER", "frontend_arm/build/bin/ex04_ipc_peer")).resolve()


class PeerTest(unittest.TestCase):
    def setUp(self):
        self.sem = LIBC.semget(0, 2, 0o600 | 0o1000)
        self.assertGreaterEqual(self.sem, 0)
        self.assertEqual(LIBC.semctl(self.sem, 0, 16, 1), 0)  # SETVAL, writable=1
        self.shm = -1
        self.address = None
        self.peer = None

    def tearDown(self):
        if self.peer and self.peer.poll() is None:
            self.peer.kill()
            self.peer.wait(timeout=2)
        if self.peer:
            self.peer.communicate(timeout=2)
        if self.address is not None:
            LIBC.shmdt(self.address)
        if self.shm >= 0:
            self.assertEqual(LIBC.shmctl(self.shm, 0, None), 0)
        if self.sem >= 0:
            self.assertEqual(LIBC.semctl(self.sem, 0, 0), 0)

    def start(self, mode):
        args = [str(PEER), mode, str(self.sem)]
        if mode == "data":
            self.shm = LIBC.shmget(0, ctypes.sizeof(Message), 0o600 | 0o1000)
            self.assertGreaterEqual(self.shm, 0)
            self.address = LIBC.shmat(self.shm, None, 0)
            self.assertNotEqual(self.address, ctypes.c_void_p(-1).value)
            args.append(str(self.shm))
        self.peer = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def semop(self, number, change, timeout=3):
        deadline = time.monotonic() + timeout
        operation = SemOp(number, change, 0o4000)  # IPC_NOWAIT
        while LIBC.semop(self.sem, ctypes.byref(operation), 1) < 0:
            code = ctypes.get_errno()
            if code not in (errno.EAGAIN, errno.EINTR):
                self.fail(os.strerror(code))
            if time.monotonic() >= deadline:
                self.fail("semaphore timeout")
            time.sleep(0.01)

    def receive_all(self, data):
        self.start("data" if data else "sync")
        for sequence in range(1, 11):
            self.semop(1, -1)
            if data:
                message = Message.from_address(self.address)
                self.assertEqual(message.sequence, sequence)
                self.assertEqual(message.text.decode(), f"ARM C process: message {sequence}")
            self.semop(0, 1)
        out, err = self.peer.communicate(timeout=3)
        self.assertEqual(self.peer.returncode, 0, err)
        self.assertEqual(out.count(b"IPC_SENT"), 10)

    def test_semaphore_roundtrip(self):
        self.receive_all(False)

    def test_shared_memory_roundtrip(self):
        self.receive_all(True)

    def test_waits_for_consumer_before_overwriting(self):
        self.start("data")
        self.semop(1, -1)
        first = bytes(Message.from_address(self.address))
        time.sleep(0.5)
        self.assertEqual(bytes(Message.from_address(self.address)), first)
        self.assertIsNone(self.peer.poll())
        self.peer.send_signal(signal.SIGTERM)
        self.assertEqual(self.peer.wait(timeout=2), 0)

    def test_signal_interrupts_wait(self):
        self.start("sync")
        self.semop(1, -1)
        time.sleep(0.3)
        self.peer.send_signal(signal.SIGTERM)
        self.assertEqual(self.peer.wait(timeout=2), 0)

    def test_removed_semaphore_exits_with_error(self):
        self.start("sync")
        self.semop(1, -1)
        time.sleep(0.3)
        self.assertEqual(LIBC.semctl(self.sem, 0, 0), 0)
        self.sem = -1
        self.assertNotEqual(self.peer.wait(timeout=2), 0)

    def test_invalid_arguments(self):
        for args in ([], ["unknown", "1"], ["sync", "-1"], ["data", "abc", "2"]):
            result = subprocess.run([str(PEER)] + args, capture_output=True, timeout=2)
            self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    if not sys.platform.startswith("linux"):
        raise SystemExit("Run this test under Linux (System V IPC is required).")
    unittest.main(verbosity=2)
