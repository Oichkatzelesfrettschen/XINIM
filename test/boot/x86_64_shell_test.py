#!/usr/bin/env python3
"""
Programmatic staged x86_64 xash shell test for XINIM.

Boots the Limine x86_64 image with the COM2 emergency shell enabled,
issues `continue`, and then validates the staged xash init shell that
takes over the same serial line.
"""

import os
import socket
import subprocess
import sys
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

XINIM_STATE_ROOT = os.environ.get(
    "XINIM_STATE_ROOT", os.path.join(PROJECT_ROOT, "build", "_state")
)
XINIM_BUILD_ROOT = os.environ.get(
    "XINIM_BUILD_ROOT", os.path.join(PROJECT_ROOT, "build", "x86_64", "Debug")
)
XINIM_IMAGE_ROOT = os.environ.get(
    "XINIM_IMAGE_ROOT", os.path.join(XINIM_BUILD_ROOT, "images")
)
BOOT_IMAGE = os.environ.get(
    "XINIM_QEMU_BOOT_IMAGE",
    os.path.join(XINIM_IMAGE_ROOT, "x86_64", "xinim-x86_64.iso"),
)
QEMU_BIN = "qemu-system-x86_64"
SHELL_PORT = 4555
BOOT_TIMEOUT = 20
CMD_TIMEOUT = 5
DBG_PROMPT = "xinim-kernel-dbg> "
XASH_PROMPT = "xash$ "


def start_qemu():
    cmd = [
        QEMU_BIN,
        "-machine",
        "q35",
        "-cpu",
        "qemu64",
        "-m",
        "512M",
        "-smp",
        "1",
        "-cdrom",
        BOOT_IMAGE,
        "-boot",
        "d",
        "-serial",
        "file:/dev/null",
        "-serial",
        f"tcp::{SHELL_PORT},server,nowait",
        "-nographic",
        "-monitor",
        "none",
        "-no-reboot",
        "-no-shutdown",
    ]
    return subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def connect_shell(retries=20, delay=0.5):
    for attempt in range(retries):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(CMD_TIMEOUT)
            sock.connect(("127.0.0.1", SHELL_PORT))
            return sock
        except (ConnectionRefusedError, OSError):
            if attempt == retries - 1:
                raise
            time.sleep(delay)
    raise RuntimeError("shell port was never reachable")


def recv_until_any_prompt(sock, timeout=CMD_TIMEOUT):
    data = b""
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if DBG_PROMPT.encode("utf-8") in data or XASH_PROMPT.encode("utf-8") in data:
                break
        except socket.timeout:
            continue
    return data.decode("utf-8", errors="replace")


def send_command(sock, command):
    sock.sendall((command + "\r").encode("utf-8"))
    return recv_until_any_prompt(sock)


def require_contains(name, response, expected):
    if expected not in response:
        print(f"FAIL: {name} response missing {expected!r}")
        print(f"  Response: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def main():
    if not os.path.isfile(BOOT_IMAGE):
        print(f"SKIP: Boot image not found: {BOOT_IMAGE}")
        sys.exit(77)

    qemu = start_qemu()

    try:
        shell = connect_shell(retries=int(BOOT_TIMEOUT / 0.5))
        initial = recv_until_any_prompt(shell, timeout=BOOT_TIMEOUT)
        if DBG_PROMPT not in initial and XASH_PROMPT not in initial:
            shell.sendall(b"\r")
            initial += recv_until_any_prompt(shell, timeout=BOOT_TIMEOUT)
        if DBG_PROMPT not in initial and XASH_PROMPT not in initial:
            print("FAIL: did not receive initial debug-shell or xash prompt")
            print(f"  Received: {initial!r}")
            sys.exit(1)

        if XASH_PROMPT in initial:
            response = initial
        else:
            response = send_command(shell, "continue")
            if XASH_PROMPT not in response:
                response += recv_until_any_prompt(shell, timeout=BOOT_TIMEOUT)
            if XASH_PROMPT not in response:
                print("FAIL: did not receive staged xash prompt after continue")
                print(f"  Received: {response!r}")
                sys.exit(1)

        results = [
            require_contains("help", send_command(shell, "help"), "Built-in commands:"),
            require_contains("pid", send_command(shell, "pid"), "pid: 1"),
            require_contains("pwd", send_command(shell, "pwd"), "/"),
            require_contains("check xash", send_command(shell, "check /bin/xash"), "check /bin/xash: ok"),
            require_contains("ls /bin", send_command(shell, "ls /bin"), "xash"),
            require_contains("cat /etc/motd", send_command(shell, "cat /etc/motd"), "Welcome to xash"),
            require_contains("env PATH", send_command(shell, "env"), "PATH=/bin"),
            require_contains("echo vars", send_command(shell, "echo $? $$ $PATH"), "/bin"),
        ]

        send_command(shell, "cp /etc/motd /tmp/motd_copy")
        results.extend(
            [
                require_contains("cp status", send_command(shell, "echo $?"), "0"),
                require_contains("ls /tmp", send_command(shell, "ls /tmp"), "motd_copy"),
                require_contains("cat copied motd", send_command(shell, "cat /tmp/motd_copy"), "Welcome to xash"),
                require_contains("command -v xash", send_command(shell, "command -v xash"), "/bin/xash"),
                require_contains("rescue banner", send_command(shell, "rescue"), "xinim-kernel-dbg> "),
                require_contains("rescue continue", send_command(shell, "continue"), "xash$ "),
            ]
        )

        send_command(shell, "halt")
        shell.close()

        if not all(results):
            sys.exit(1)
        sys.exit(0)
    except (ConnectionRefusedError, OSError) as exc:
        print(f"FAIL: could not connect to x86_64 shell: {exc}")
        sys.exit(2)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == "__main__":
    main()
