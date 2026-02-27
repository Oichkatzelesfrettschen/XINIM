#!/usr/bin/env python3
"""
Programmatic kshell test for XINIM.

Boots QEMU with debug_shell cmdline, connects to COM2 via TCP,
sends kshell commands, and validates responses.

Exit codes:
  0 = all checks passed
  1 = test failure
  2 = timeout / connection failure
"""

import socket
import subprocess
import sys
import time
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

KERNEL_IMAGE = os.path.join(PROJECT_ROOT, "build", "Debug", "xinim")
QEMU_BIN = "qemu-system-x86_64"
KSHELL_PORT = 4555
BOOT_TIMEOUT = 15  # seconds to wait for QEMU to boot
CMD_TIMEOUT = 5    # seconds to wait for command response


def start_qemu():
    """Start QEMU with dual serial and debug_shell cmdline."""
    cmd = [
        QEMU_BIN,
        "-machine", "q35",
        "-cpu", "qemu64",
        "-m", "512M",
        "-smp", "1",
        "-kernel", KERNEL_IMAGE,
        "-serial", "file:/dev/null",  # COM1: discard logs
        "-serial", f"tcp::{KSHELL_PORT},server,nowait",  # COM2: kshell
        "-nographic",
        "-no-reboot",
        "-append", "debug_shell",
    ]
    return subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def connect_kshell(retries=10, delay=1.0):
    """Connect to the kshell TCP port with retries."""
    for attempt in range(retries):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(CMD_TIMEOUT)
            sock.connect(("127.0.0.1", KSHELL_PORT))
            return sock
        except (ConnectionRefusedError, OSError):
            if attempt < retries - 1:
                time.sleep(delay)
            else:
                raise
    return None


def recv_until_prompt(sock, timeout=CMD_TIMEOUT):
    """Read from socket until we see 'xinim> ' prompt or timeout."""
    data = b""
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if b"xinim> " in data:
                return data.decode("utf-8", errors="replace")
        except socket.timeout:
            break
    return data.decode("utf-8", errors="replace")


def send_command(sock, cmd):
    """Send a command and return the response up to the next prompt."""
    sock.sendall((cmd + "\r").encode("utf-8"))
    return recv_until_prompt(sock)


def test_help(sock):
    """Test the help command lists expected commands."""
    resp = send_command(sock, "help")
    required = ["help", "info", "ps", "mem", "reboot", "halt"]
    for word in required:
        if word not in resp:
            print(f"FAIL: 'help' response missing '{word}'")
            print(f"  Response: {resp!r}")
            return False
    print("PASS: help")
    return True


def test_info(sock):
    """Test the info command returns kernel version string."""
    resp = send_command(sock, "info")
    if "XINIM Kernel" not in resp:
        print(f"FAIL: 'info' response missing 'XINIM Kernel'")
        print(f"  Response: {resp!r}")
        return False
    print("PASS: info")
    return True


def test_ps(sock):
    """Test the ps command shows process table header."""
    resp = send_command(sock, "ps")
    if "PID" not in resp:
        print(f"FAIL: 'ps' response missing 'PID' header")
        print(f"  Response: {resp!r}")
        return False
    if "Total active:" not in resp:
        print(f"FAIL: 'ps' response missing 'Total active:' summary")
        print(f"  Response: {resp!r}")
        return False
    print("PASS: ps")
    return True


def test_mem(sock):
    """Test the mem command shows heap usage."""
    resp = send_command(sock, "mem")
    if "Kernel heap:" not in resp:
        print(f"FAIL: 'mem' response missing 'Kernel heap:'")
        print(f"  Response: {resp!r}")
        return False
    if "bytes" not in resp:
        print(f"FAIL: 'mem' response missing 'bytes'")
        print(f"  Response: {resp!r}")
        return False
    print("PASS: mem")
    return True


def test_unknown(sock):
    """Test that unknown commands produce an error message."""
    resp = send_command(sock, "xyzzy")
    if "Unknown command" not in resp:
        print(f"FAIL: unknown command did not produce error")
        print(f"  Response: {resp!r}")
        return False
    print("PASS: unknown command error")
    return True


def main():
    if not os.path.isfile(KERNEL_IMAGE):
        print(f"SKIP: Kernel image not found: {KERNEL_IMAGE}")
        print("Build with: cmake --build --preset debug")
        sys.exit(0)  # Skip, not fail

    print(f"Starting QEMU (kernel: {KERNEL_IMAGE})")
    qemu = start_qemu()

    try:
        # Wait for QEMU to boot and kshell to start
        print(f"Connecting to kshell on TCP port {KSHELL_PORT}...")
        sock = connect_kshell(retries=BOOT_TIMEOUT)

        # Wait for initial prompt
        initial = recv_until_prompt(sock, timeout=BOOT_TIMEOUT)
        if "xinim>" not in initial and "xinim-kernel-dbg>" not in initial:
            print(f"FAIL: Did not receive kshell prompt")
            print(f"  Received: {initial!r}")
            sys.exit(1)

        print("Connected to kshell.")

        # Run tests
        passed = 0
        failed = 0
        for test_fn in [test_help, test_info, test_ps, test_mem, test_unknown]:
            if test_fn(sock):
                passed += 1
            else:
                failed += 1

        # Send halt to cleanly terminate
        sock.sendall(b"halt\r")
        sock.close()

        print(f"\nResults: {passed} passed, {failed} failed")
        sys.exit(1 if failed > 0 else 0)

    except (ConnectionRefusedError, OSError) as e:
        print(f"FAIL: Could not connect to kshell: {e}")
        sys.exit(2)

    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == "__main__":
    main()
