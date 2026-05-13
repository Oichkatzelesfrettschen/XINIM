#!/usr/bin/env python3
"""
Programmatic 32-bit Ring 3 supervised-init shell test for XINIM.

Boots a GRUB image in QEMU, connects to COM2 over a raw TCP backend,
and validates a small command set from the native user-mode init shell.
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
QEMU_DISK_IMAGE = os.environ.get("XINIM_QEMU_DISK_IMAGE", "")
SHELL_PORT = int(
    os.environ.get("XINIM_QEMU_SHELL_PORT", str(SHELL_PORT_BY_LANE.get(LANE_NAME, 4556)))
)
BOOT_TIMEOUT = int(os.environ.get("XINIM_QEMU_BOOT_TIMEOUT", "25"))
CMD_TIMEOUT = int(os.environ.get("XINIM_QEMU_CMD_TIMEOUT", "30"))
PROMPT_SETTLE_TIMEOUT = float(os.environ.get("XINIM_QEMU_PROMPT_SETTLE_TIMEOUT", "1.5"))
LOG_FILE = os.environ.get(
    "XINIM_QEMU_SHELL_LOG",
    os.path.join(XINIM_LOG_ROOT, f"{LANE_NAME}-kshell.log"),
)
PROMPTS = [prompt for prompt in os.environ.get("XINIM_SHELL_PROMPTS", "$ ||#||# ||mksh$ ").split("||") if prompt]
COMMAND_MARKER_PREFIX = "__XINIM_DONE_"
READY_MARKER = "__XINIM_READY__"
command_counter = 0


def contains_prompt(text):
    return any(prompt in text for prompt in PROMPTS)


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
    if QEMU_DISK_IMAGE and os.path.isfile(QEMU_DISK_IMAGE):
        cmd.extend(
            [
                "-drive",
                f"file={QEMU_DISK_IMAGE},format=raw,index=0,media=disk",
            ]
        )
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
    prompt_seen = False
    while time.time() < end_time:
        try:
            wait_timeout = PROMPT_SETTLE_TIMEOUT if prompt_seen else min(0.5, timeout)
            sock.settimeout(wait_timeout)
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if any(prompt.encode("utf-8") in data for prompt in PROMPTS):
                prompt_seen = True
        except socket.timeout:
            if prompt_seen:
                break
    return data.decode("utf-8", errors="replace")


def recv_until_text(sock, needle, timeout=CMD_TIMEOUT):
    data = b""
    end_time = time.time() + timeout
    needle_bytes = needle.encode("utf-8")
    while time.time() < end_time:
        try:
            sock.settimeout(min(0.5, timeout))
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if needle_bytes in data:
                break
        except socket.timeout:
            continue
    return data.decode("utf-8", errors="replace")


def send_command(sock, command):
    global command_counter
    command_counter += 1
    marker = f"{COMMAND_MARKER_PREFIX}{command_counter}__"
    wrapped = f"{command}; __xinim_status=$?; echo {marker}:${{__xinim_status}}"
    sock.sendall((wrapped + "\r").encode("utf-8"))
    response = recv_until_text(sock, marker, timeout=CMD_TIMEOUT)
    response += recv_until_prompt(sock, timeout=PROMPT_SETTLE_TIMEOUT)
    return response


def send_command_sync(sock, command):
    return send_command(sock, command)


def handshake_shell(sock):
    sock.sendall((f"echo {READY_MARKER}\r").encode("utf-8"))
    return recv_until_text(sock, READY_MARKER, timeout=BOOT_TIMEOUT)


def synchronize_shell(sock):
    prompt = recv_until_prompt(sock, timeout=BOOT_TIMEOUT)
    if not contains_prompt(prompt):
        sock.sendall(b"\r")
        prompt += recv_until_prompt(sock, timeout=CMD_TIMEOUT)
    initial = handshake_shell(sock)
    return prompt, initial


def require_contains(name, response, expected):
    if expected not in response:
        print(f"FAIL: {name} response missing {expected!r}")
        print(f"  Response: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def require_any(name, response, expected_values):
    if not any(expected in response for expected in expected_values):
        print(f"FAIL: {name} response missing one of {expected_values!r}")
        print(f"  Response: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def require_not_contains(name, response, unexpected):
    if unexpected in response:
        print(f"FAIL: {name} response unexpectedly contained {unexpected!r}")
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
        prompt, initial = synchronize_shell(shell)
        if READY_MARKER not in initial:
            print(f"FAIL: did not receive {LANE_NAME} shell ready marker")
            print(f"  Prompt: {prompt!r}")
            print(f"  Received: {initial!r}")
            sys.exit(1)

        results = [
            require_contains("shell path", send_command(shell, "echo $SHELL"), "/bin/mksh"),
            require_contains("shell pid", send_command(shell, "echo $$"), "1"),
            require_contains("path", send_command(shell, "echo $PATH"), "/bin"),
            require_contains("pwd", send_command(shell, "pwd"), "/"),
            require_contains("token quoting", send_command(shell, 'echo "hello world"'), "hello world"),
            require_contains("escaped token", send_command(shell, "echo a\\ b"), "a b"),
            require_contains(
                "mixed token quoting",
                send_command(shell, "echo \"a b\" 'c d'"),
                "a b c d",
            ),
            require_contains("test -x /bin/mksh", send_command(shell, "test -x /bin/mksh && echo yes"), "yes"),
            require_contains("test -f /bin/hello", send_command(shell, "test -f /bin/hello && echo yes"), "yes"),
            require_contains("test -f /bin/false", send_command(shell, "test -f /bin/false && echo yes"), "yes"),
            require_contains("test -f /bin/heapprobe", send_command(shell, "test -f /bin/heapprobe && echo yes"), "yes"),
            require_contains("test -f /bin/holdsvc", send_command(shell, "test -f /bin/holdsvc && echo yes"), "yes"),
            require_contains(
                "test -f /persist/etc/persist.txt",
                send_command(shell, "test -f /persist/etc/persist.txt && echo yes"),
                "yes",
            ),
            require_contains("command -v cat", send_command(shell, "command -v cat"), "cat"),
            require_contains("command -v ls", send_command(shell, "command -v ls"), "/bin/ls"),
            require_contains("command -v writefile", send_command(shell, "command -v writefile"), "/bin/writefile"),
            require_contains("command -v mkdir", send_command(shell, "command -v mkdir"), "/bin/mkdir"),
            require_contains("command -v rmdir", send_command(shell, "command -v rmdir"), "/bin/rmdir"),
            require_contains("command -v unlink", send_command(shell, "command -v unlink"), "/bin/unlink"),
            require_contains("command -v mv", send_command(shell, "command -v mv"), "/bin/mv"),
            require_contains("command -v seekwrite", send_command(shell, "command -v seekwrite"), "/bin/seekwrite"),
            require_contains("command -v holecheck", send_command(shell, "command -v holecheck"), "/bin/holecheck"),
            require_contains("command -v seekpatch", send_command(shell, "command -v seekpatch"), "/bin/seekpatch"),
            require_contains("command -v gapcheck", send_command(shell, "command -v gapcheck"), "/bin/gapcheck"),
            require_contains("command -v hello", send_command(shell, "command -v hello"), "/bin/hello"),
            require_contains("command -v heapprobe", send_command(shell, "command -v heapprobe"), "/bin/heapprobe"),
            require_contains("command -v holdsvc", send_command(shell, "command -v holdsvc"), "/bin/holdsvc"),
            require_contains("command -v persist-hello", send_command(shell, "command -v persist-hello"), "/bin/persist-hello"),
            require_any(
                "cat /etc/motd",
                send_command(shell, "cat /etc/motd"),
                ["persistent root", "Welcome to XINIM"],
            ),
            require_contains("test -d /persist", send_command(shell, "test -d /persist && echo yes"), "yes"),
            require_contains("test -d /persist/etc", send_command(shell, "test -d /persist/etc && echo yes"), "yes"),
            require_contains("test -d /persist/var", send_command(shell, "test -d /persist/var && echo yes"), "yes"),
            require_contains("test -x /persist/bin/persist-hello", send_command(shell, "test -x /persist/bin/persist-hello && echo yes"), "yes"),
            require_contains("ls /", send_command(shell, "ls /"), "persist/"),
            require_contains("ls /persist", send_command(shell, "ls /persist"), "etc/"),
            require_contains("ls /persist", send_command(shell, "ls /persist"), "bin/"),
            require_contains("ls /persist", send_command(shell, "ls /persist"), "var/"),
            require_contains("ls /persist/bin", send_command(shell, "ls /persist/bin"), "persist-hello"),
            require_contains("ls /persist/etc", send_command(shell, "ls /persist/etc"), "persist.txt"),
            require_contains("ls /persist/etc", send_command(shell, "ls /persist/etc"), "issue"),
            require_contains("ls /persist/etc", send_command(shell, "ls /persist/etc"), "persist-profile"),
            require_contains("ls /persist/var", send_command(shell, "ls /persist/var"), "disk-marker"),
            require_contains("ls /persist/etc/persist.txt", send_command(shell, "ls /persist/etc/persist.txt"), "/persist/etc/persist.txt"),
            require_contains("ls -l /persist/etc", send_command(shell, "ls -l /persist/etc"), "persist.txt"),
            require_contains(
                "cat /persist/etc/persist.txt",
                send_command(shell, "cat /persist/etc/persist.txt"),
                "persistent-root-ok",
            ),
            require_contains(
                "cat /persist/etc/issue",
                send_command(shell, "cat /persist/etc/issue"),
                "persistent ext2 root",
            ),
            require_contains(
                "source /persist/etc/persist-profile",
                send_command(shell, ". /persist/etc/persist-profile; echo $PERSIST_PROFILE"),
                "disk-root",
            ),
            require_contains(
                "source /etc/persist-profile",
                send_command(shell, ". /etc/persist-profile; echo $PERSIST_PROFILE"),
                "disk-root",
            ),
            require_contains(
                "cat /persist/var/disk-marker",
                send_command(shell, "cat /persist/var/disk-marker"),
                "ata-ext2-ready",
            ),
            require_contains("export baseline", send_command(shell, "export FOO=global; echo $FOO"), "global"),
        ]

        heapprobe_response = send_command_sync(shell, "heapprobe")
        results.append(require_contains("heapprobe", heapprobe_response, "heapprobe: ok"))
        results.append(require_contains("status after heapprobe", send_command(shell, "echo $?"), "0"))

        hello_response = send_command_sync(shell, "/bin/hello qemu")
        results.extend([
            require_contains("hello", hello_response, "Hello from XINIM"),
            require_contains("hello argv1", hello_response, "argv[1]: qemu"),
            require_contains("status after hello", send_command(shell, "echo $?"), "0"),
        ])

        persist_hello_response = send_command_sync(shell, "PATH=/persist/bin:/bin persist-hello disk")
        results.extend([
            require_contains("persist hello", persist_hello_response, "Hello from XINIM"),
            require_contains("persist hello argv1", persist_hello_response, "argv[1]: disk"),
            require_contains("status after persist hello", send_command(shell, "echo $?"), "0"),
        ])

        persist_hello_canonical = send_command_sync(shell, "PATH=/bin persist-hello canonical")
        results.extend([
            require_contains("persist hello canonical", persist_hello_canonical, "Hello from XINIM"),
            require_contains("persist hello canonical argv1", persist_hello_canonical, "argv[1]: canonical"),
            require_contains("status after persist hello canonical", send_command(shell, "echo $?"), "0"),
        ])

        hello_response = send_command_sync(shell, "FOO=local /bin/hello env-path-arg")
        results.append(require_contains("execve env stack", hello_response, "env[0]: FOO=local"))

        send_command_sync(shell, "false")
        results.extend([
            require_contains("status after false", send_command(shell, "/bin/false; echo status=$?"), "status=1"),
            require_contains("echo pid path", send_command(shell, "echo $$ $PATH"), "1 /bin"),
            require_contains("echo", send_command(shell, "echo hello from ring3"), "hello from ring3"),
            require_contains("unknown", send_command(shell, "xyzzy"), "inaccessible or not found"),
        ])

        shell.sendall(b"exit\r")
        time.sleep(1.0)
        resumed = recv_until_prompt(shell, timeout=CMD_TIMEOUT)
        exit_log = ""
        if os.path.exists(LOG_FILE):
            with open(LOG_FILE, "r", encoding="utf-8", errors="replace") as handle:
                exit_log = handle.read()
        results.extend([
            require_contains(
                "shell exit respawn log",
                exit_log,
                "Respawning supervised service init-shell",
            ),
            require_not_contains(
                "shell exit syscall mismatch",
                exit_log,
                "Unhandled i486 syscall eax=1",
            ),
            require_not_contains(
                "shell exit allocator crash",
                exit_log,
                "rogue pointer",
            ),
            require_not_contains("shell exit respawn rescue", resumed, "xinim-i486>"),
            require_contains("shell exit respawn path", send_command(shell, "echo $SHELL"), "/bin/mksh"),
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
