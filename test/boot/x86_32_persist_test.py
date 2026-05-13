#!/usr/bin/env python3
"""
Reboot persistence test for the 32-bit i486 init-shell lane.

Boots the same ATA disk image twice, writes and then shrinks a marker through
the guest writefile utility on the first boot, then verifies on the second boot
that the shortened content persisted and no stale tail bytes leaked back.
"""

import os
import socket
import subprocess
import sys
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

LANE_NAME = os.environ.get("XINIM_BOOT_LANE_NAME", "i486")
QEMU_BIN = os.environ.get("XINIM_QEMU_SYSTEM_BIN", "qemu-system-i386")
BOOT_IMAGE = os.environ.get(
    "XINIM_QEMU_BOOT_IMAGE",
    os.path.join(PROJECT_ROOT, "build", LANE_NAME, "Debug", "images", LANE_NAME, "xinim-i486dx.iso"),
)
DISK_IMAGE = os.environ.get(
    "XINIM_QEMU_DISK_IMAGE",
    os.path.join(PROJECT_ROOT, "build", LANE_NAME, "Debug", "images", LANE_NAME, f"xinim-{LANE_NAME}-ata.img"),
)
QEMU_MACHINE = os.environ.get("XINIM_QEMU_MACHINE", "pc")
QEMU_CPU = os.environ.get("XINIM_QEMU_CPU", "486")
QEMU_MEMORY = os.environ.get("XINIM_QEMU_MEMORY", "32M")
QEMU_VGA = os.environ.get("XINIM_QEMU_VGA", "std")
SHELL_PORT = int(os.environ.get("XINIM_QEMU_SHELL_PORT", "4556"))
BOOT_TIMEOUT = int(os.environ.get("XINIM_QEMU_BOOT_TIMEOUT", "25"))
CMD_TIMEOUT = int(os.environ.get("XINIM_QEMU_CMD_TIMEOUT", "30"))
PROMPT_SETTLE_TIMEOUT = float(os.environ.get("XINIM_QEMU_PROMPT_SETTLE_TIMEOUT", "1.5"))
XINIM_LOG_ROOT = os.environ.get(
    "XINIM_LOG_ROOT",
    os.path.join(PROJECT_ROOT, "build", LANE_NAME, "Debug", "logs"),
)
LOG_FILE = os.path.join(XINIM_LOG_ROOT, f"{LANE_NAME}-persist.log")
PROMPTS = [prompt for prompt in os.environ.get("XINIM_SHELL_PROMPTS", "$ ||#||# ||mksh$ ").split("||") if prompt]
COMMAND_MARKER_PREFIX = "__XINIM_DONE_"
READY_MARKER = "__XINIM_READY__"
command_counter = 0
WRITE_PATH = "/etc/persist-write-slot"
LONG_WRITE_VALUE = f"reboot-proof-{QEMU_MACHINE}-with-extra-tail"
SHORT_WRITE_VALUE = f"trim-{QEMU_MACHINE}"
TAIL_FRAGMENT = "with-extra-tail"
SEED_PATH = f"/persist/zero-seed-{QEMU_MACHINE}.txt"
HOLE_PATH = f"/persist/zero-hole-{QEMU_MACHINE}.txt"
HOLE_SEED_VALUE = "ABCDWXYZ"
HOLE_OFFSET = "4"
HOLE_VALUE = "Z"
PATCH_PATH = f"/persist/gap-hole-{QEMU_MACHINE}.txt"
PATCH_HEAD = "A"
PATCH_OFFSET = "4"
PATCH_TAIL = "Z"


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
        "-drive",
        f"file={DISK_IMAGE},format=raw,index=0,media=disk",
    ]
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def connect_shell():
    for attempt in range(int(BOOT_TIMEOUT / 0.5)):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(CMD_TIMEOUT)
            sock.connect(("127.0.0.1", SHELL_PORT))
            return sock
        except (ConnectionRefusedError, OSError):
            if attempt == int(BOOT_TIMEOUT / 0.5) - 1:
                raise
            time.sleep(0.5)
    raise RuntimeError("shell port never became reachable")


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


def recv_until_marker_line(sock, marker, timeout=CMD_TIMEOUT):
    data = b""
    end_time = time.time() + timeout
    marker_bytes = f"{marker}:".encode("utf-8")
    while time.time() < end_time:
        try:
            sock.settimeout(min(0.5, timeout))
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            offset = data.find(marker_bytes)
            while offset != -1:
                status_offset = offset + len(marker_bytes)
                if status_offset < len(data) and data[status_offset] in b"0123456789":
                    return data.decode("utf-8", errors="replace")
                offset = data.find(marker_bytes, offset + 1)
        except socket.timeout:
            continue
    return data.decode("utf-8", errors="replace")


def send_command(sock, command):
    global command_counter
    command_counter += 1
    marker = f"{COMMAND_MARKER_PREFIX}{command_counter}__"
    wrapped = f"{command}; __xinim_status=$?; echo {marker}:${{__xinim_status}}"
    sock.sendall((wrapped + "\r").encode("utf-8"))
    response = recv_until_marker_line(sock, marker, timeout=CMD_TIMEOUT)
    response += recv_until_prompt(sock, timeout=PROMPT_SETTLE_TIMEOUT)
    return response


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


def require_not_contains(name, response, unexpected):
    if unexpected in response:
        print(f"FAIL: {name} response unexpectedly contained {unexpected!r}")
        print(f"  Response: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def terminate_qemu(process):
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def boot_and_connect():
    process = start_qemu()
    try:
        shell = connect_shell()
        prompt, initial = synchronize_shell(shell)
        if READY_MARKER not in initial:
            print(f"FAIL: did not receive {LANE_NAME} shell ready marker")
            print(f"  Prompt: {prompt!r}")
            print(f"  Received: {initial!r}")
            terminate_qemu(process)
            sys.exit(1)
        return process, shell
    except Exception:
        terminate_qemu(process)
        raise


def main():
    if not os.path.isfile(BOOT_IMAGE):
        print(f"SKIP: boot image not found: {BOOT_IMAGE}")
        sys.exit(77)
    if not os.path.isfile(DISK_IMAGE):
        print(f"SKIP: disk image not found: {DISK_IMAGE}")
        sys.exit(77)

    first_process, first_shell = boot_and_connect()
    try:
        results = [
            require_contains("writefile exists", send_command(first_shell, "command -v writefile"), "/bin/writefile"),
            require_contains("unlink exists", send_command(first_shell, "command -v unlink"), "/bin/unlink"),
            require_contains("seekwrite exists", send_command(first_shell, "command -v seekwrite"), "/bin/seekwrite"),
            require_contains("holecheck exists", send_command(first_shell, "command -v holecheck"), "/bin/holecheck"),
            require_contains("seekpatch exists", send_command(first_shell, "command -v seekpatch"), "/bin/seekpatch"),
            require_contains("gapcheck exists", send_command(first_shell, "command -v gapcheck"), "/bin/gapcheck"),
            require_contains(
                "writefile long update",
                send_command(first_shell, f"writefile {WRITE_PATH} {LONG_WRITE_VALUE}; echo $?"),
                "0",
            ),
            require_contains(
                "updated long content first boot",
                send_command(first_shell, f"cat {WRITE_PATH}"),
                LONG_WRITE_VALUE,
            ),
            require_contains(
                "writefile short update",
                send_command(first_shell, f"writefile {WRITE_PATH} {SHORT_WRITE_VALUE}; echo $?"),
                "0",
            ),
            require_contains(
                "updated short content first boot",
                send_command(first_shell, f"cat {WRITE_PATH}"),
                SHORT_WRITE_VALUE,
            ),
            require_not_contains(
                "stale tail removed first boot",
                send_command(first_shell, f"cat {WRITE_PATH}"),
                TAIL_FRAGMENT,
            ),
            require_contains(
                "seed dirty block",
                send_command(first_shell, f"writefile {SEED_PATH} {HOLE_SEED_VALUE}; echo $?"),
                "0",
            ),
            require_contains(
                "seed unlink",
                send_command(first_shell, f"unlink {SEED_PATH}; echo $?"),
                "0",
            ),
            require_contains(
                "seekwrite hole file",
                send_command(first_shell, f"seekwrite {HOLE_PATH} {HOLE_OFFSET} {HOLE_VALUE}; echo $?"),
                "0",
            ),
            require_contains(
                "holecheck first boot",
                send_command(first_shell, f"holecheck {HOLE_PATH} {HOLE_OFFSET} {HOLE_VALUE}; echo $?"),
                "holecheck ok",
            ),
            require_contains(
                "holecheck first boot status",
                send_command(first_shell, f"holecheck {HOLE_PATH} {HOLE_OFFSET} {HOLE_VALUE}; echo $?"),
                "0",
            ),
            require_contains(
                "seed patch base file",
                send_command(first_shell, f"writefile {PATCH_PATH} {PATCH_HEAD}; echo $?"),
                "0",
            ),
            require_contains(
                "seekpatch existing file growth",
                send_command(first_shell, f"seekpatch {PATCH_PATH} {PATCH_OFFSET} {PATCH_TAIL}; echo $?"),
                "0",
            ),
            require_contains(
                "gapcheck first boot",
                send_command(first_shell, f"gapcheck {PATCH_PATH} {PATCH_HEAD} 3 {PATCH_TAIL}; echo $?"),
                "gapcheck ok",
            ),
            require_contains(
                "gapcheck first boot status",
                send_command(first_shell, f"gapcheck {PATCH_PATH} {PATCH_HEAD} 3 {PATCH_TAIL}; echo $?"),
                "0",
            ),
        ]
        if not all(results):
            sys.exit(1)
    finally:
        try:
            first_shell.close()
        except OSError:
            pass
        time.sleep(0.5)
        terminate_qemu(first_process)

    time.sleep(0.5)

    second_process, second_shell = boot_and_connect()
    try:
        results = [
            require_contains(
                "persisted content second boot",
                send_command(second_shell, f"cat {WRITE_PATH}"),
                SHORT_WRITE_VALUE,
            ),
            require_not_contains(
                "stale tail removed second boot",
                send_command(second_shell, f"cat {WRITE_PATH}"),
                TAIL_FRAGMENT,
            ),
            require_contains(
                "holecheck second boot",
                send_command(second_shell, f"holecheck {HOLE_PATH} {HOLE_OFFSET} {HOLE_VALUE}; echo $?"),
                "holecheck ok",
            ),
            require_contains(
                "holecheck second boot status",
                send_command(second_shell, f"holecheck {HOLE_PATH} {HOLE_OFFSET} {HOLE_VALUE}; echo $?"),
                "0",
            ),
            require_contains(
                "gapcheck second boot",
                send_command(second_shell, f"gapcheck {PATCH_PATH} {PATCH_HEAD} 3 {PATCH_TAIL}; echo $?"),
                "gapcheck ok",
            ),
            require_contains(
                "gapcheck second boot status",
                send_command(second_shell, f"gapcheck {PATCH_PATH} {PATCH_HEAD} 3 {PATCH_TAIL}; echo $?"),
                "0",
            ),
        ]
        if not all(results):
            sys.exit(1)
    finally:
        try:
            second_shell.close()
        except OSError:
            pass
        terminate_qemu(second_process)


if __name__ == "__main__":
    main()
