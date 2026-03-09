#!/usr/bin/env python3
"""
Programmatic 32-bit Ring 3 xash test for XINIM.

Boots a GRUB image in QEMU, connects to COM2 over a raw TCP backend,
and validates a small command set from the native user-mode shell.
"""

import os
import socket
import subprocess
import sys
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

LANE_NAME = os.environ.get("XINIM_BOOT_LANE_NAME", "i486")
DEFAULT_IMAGE_NAME = {
    "i486": "xinim-i486dx.iso",
    "i586": "xinim-i586.iso",
    "i686": "xinim-i686.iso",
}
DEFAULT_CPU_BY_LANE = {
    "i486": "486",
    "i586": "pentium",
    "i686": "pentium3",
}
DEFAULT_MEMORY_BY_LANE = {
    "i486": "32M",
    "i586": "48M",
    "i686": "64M",
}
SHELL_PORT_BY_LANE = {
    "i486": 4556,
    "i586": 4557,
    "i686": 4558,
}

XINIM_STATE_ROOT = os.environ.get("XINIM_STATE_ROOT", os.path.join(PROJECT_ROOT, "build", "_state"))
XINIM_PROJECT_ROOT = os.environ.get("XINIM_PROJECT_ROOT", PROJECT_ROOT)
XINIM_IMAGE_ROOT = os.environ.get(
    "XINIM_IMAGE_ROOT",
    os.path.join(XINIM_PROJECT_ROOT, "build", LANE_NAME, "Debug", "images", LANE_NAME),
)
XINIM_LOG_ROOT = os.environ.get(
    "XINIM_LOG_ROOT",
    os.path.join(XINIM_PROJECT_ROOT, "build", LANE_NAME, "Debug", "logs"),
)
BOOT_IMAGE_CANDIDATES = [
    os.path.join(
        XINIM_PROJECT_ROOT,
        "build",
        LANE_NAME,
        "Debug",
        "images",
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_PROJECT_ROOT,
        "build",
        f"{LANE_NAME}-cross",
        "Debug",
        "images",
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_STATE_ROOT,
        f"build_{LANE_NAME}_debug_32lib",
        "images",
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_STATE_ROOT,
        "images",
        LANE_NAME,
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_STATE_ROOT,
        "build",
        LANE_NAME,
        "Debug",
        "images",
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_STATE_ROOT,
        "images",
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_IMAGE_ROOT,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_IMAGE_ROOT,
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
    os.path.join(
        XINIM_PROJECT_ROOT,
        "build",
        LANE_NAME,
        "Debug",
        "images",
        LANE_NAME,
        DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso"),
    ),
]
BOOT_IMAGE = os.environ.get(
    "XINIM_QEMU_BOOT_IMAGE",
    next((path for path in BOOT_IMAGE_CANDIDATES if os.path.isfile(path)),
         BOOT_IMAGE_CANDIDATES[0]),
)
QEMU_BIN = os.environ.get("XINIM_QEMU_SYSTEM_BIN", "qemu-system-i386")
QEMU_MACHINE = os.environ.get("XINIM_QEMU_MACHINE", "pc")
QEMU_CPU = os.environ.get("XINIM_QEMU_CPU", DEFAULT_CPU_BY_LANE.get(LANE_NAME, "486"))
QEMU_MEMORY = os.environ.get("XINIM_QEMU_MEMORY", DEFAULT_MEMORY_BY_LANE.get(LANE_NAME, "32M"))
QEMU_VGA = os.environ.get("XINIM_QEMU_VGA", "std")
SHELL_PORT = int(
    os.environ.get("XINIM_QEMU_SHELL_PORT", str(SHELL_PORT_BY_LANE.get(LANE_NAME, 4556)))
)
BOOT_TIMEOUT = int(os.environ.get("XINIM_QEMU_BOOT_TIMEOUT", "15"))
CMD_TIMEOUT = int(os.environ.get("XINIM_QEMU_CMD_TIMEOUT", "5"))
LOG_FILE = os.environ.get(
    "XINIM_QEMU_SHELL_LOG",
    os.path.join(XINIM_LOG_ROOT, f"{LANE_NAME}-kshell.log"),
)
PROMPT = os.environ.get("XINIM_XASH_PROMPT", "xash$ ")


def start_qemu():
    os.makedirs(XINIM_LOG_ROOT, exist_ok=True)
    if os.path.exists(LOG_FILE):
        os.remove(LOG_FILE)

    cmd = [
        QEMU_BIN,
        "-machine",
        QEMU_MACHINE,
        "-cpu",
        QEMU_CPU,
        "-m",
        QEMU_MEMORY,
        "-boot",
        "d",
        "-cdrom",
        BOOT_IMAGE,
        "-vga",
        QEMU_VGA,
        "-display",
        "none",
        "-serial",
        f"file:{LOG_FILE}",
        "-serial",
        f"tcp::{SHELL_PORT},server,nowait",
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


def recv_until_prompt(sock, timeout=CMD_TIMEOUT):
    data = b""
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if PROMPT.encode("utf-8") in data:
                break
        except socket.timeout:
            break
    return data.decode("utf-8", errors="replace")


def send_command(sock, command):
    sock.sendall((command + "\r").encode("utf-8"))
    return recv_until_prompt(sock)


def send_command_sync(sock, command):
    response = send_command(sock, command)
    if PROMPT not in response:
        response += recv_until_prompt(sock)
    return response


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
        initial = recv_until_prompt(shell, timeout=BOOT_TIMEOUT)
        if PROMPT not in initial:
            initial = send_command(shell, "")
        if PROMPT not in initial:
            print(f"FAIL: did not receive {LANE_NAME} shell prompt")
            print(f"  Received: {initial!r}")
            sys.exit(1)

        send_command(shell, "export FOO=global")

        results = [
            require_contains("help", send_command(shell, "help"), "Built-in commands:"),
            require_contains("pid", send_command(shell, "pid"), "pid: 1"),
            require_contains("pwd", send_command(shell, "pwd"), "/"),
            require_contains("token quoting", send_command(shell, 'echo "hello world"'), "hello world"),
            require_contains("escaped token", send_command(shell, "echo a\\ b"), "a b"),
            require_contains(
                "mixed token quoting",
                send_command(shell, "echo \"a b\" 'c d'"),
                "a b c d",
            ),
            require_contains("cd /", send_command(shell, "cd /"), "cd /: ok"),
            require_contains("check /bin/xash", send_command(shell, "check /bin/xash"), "check /bin/xash: ok"),
            require_contains("check /bin/sh", send_command(shell, "check /bin/sh"), "check /bin/sh: ok"),
            require_contains("test -f /bin/hello", send_command(shell, "test -f /bin/hello"), "true"),
            require_contains("test -f /bin/false", send_command(shell, "test -f /bin/false"), "true"),
            require_contains("command -v hello", send_command(shell, "command -v hello"), "/bin/hello"),
            require_contains("cat /etc/motd", send_command(shell, "cat /etc/motd"), "Welcome to xash"),
            require_contains("export baseline", send_command(shell, "env"), "FOO=global"),
            require_contains("command-local env via env builtin", send_command(shell, "FOO=local env"), "FOO=local"),
            require_contains("multi local env via env builtin", send_command(shell, "FOO=local BAZ=1 env"), "BAZ=1"),
            require_contains("command-local env persistence", send_command(shell, "env"), "FOO=global"),
        ]

        require_contains("ls /bin", send_command(shell, "ls /bin"), "hello")
        send_command(shell, "cp /etc/motd /tmp/motd_copy")
        results.append(require_contains("cp status", send_command(shell, "echo $?"), "0"))
        results.append(require_contains("ls /tmp after cp", send_command(shell, "ls /tmp"), "motd_copy"))
        results.append(require_contains("cat copied motd", send_command(shell, "cat /tmp/motd_copy"), "Welcome to xash"))

        hello_response = send_command_sync(shell, "hello from qemu")
        results.extend([
            require_contains("hello", hello_response, "Hello from XINIM"),
            require_contains("hello argv1", hello_response, "argv[1]: from"),
            require_contains("hello argv2", hello_response, "argv[2]: qemu"),
            require_contains("hello env", hello_response, "env[0]: PATH=/bin"),
            require_contains("status after hello", send_command(shell, "echo $?"), "0"),
        ])

        hello_response = send_command_sync(
            shell, "PATH=/tmp /bin/hello env-path-arg"
        )
        results.append(
            require_contains("execve env stack", hello_response, "env[0]: PATH=/tmp")
        )

        send_command_sync(shell, "false")
        results.extend([
            require_contains("status after false", send_command(shell, "echo $?"), "1"),
            require_contains("echo pid path", send_command(shell, "echo $$ $PATH"), "/bin"),
            require_contains("echo", send_command(shell, "echo hello from ring3"), "hello from ring3"),
            require_contains("unknown", send_command(shell, "xyzzy"), "unknown command: xyzzy"),
        ])

        if not all(results):
            sys.exit(1)

        sys.exit(0)
    except (ConnectionRefusedError, OSError) as exc:
        print(f"FAIL: could not connect to {LANE_NAME} shell: {exc}")
        sys.exit(2)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == "__main__":
    main()
