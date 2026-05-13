#!/usr/bin/env python3
"""
Create/mkdir/rename/unlink persistence test for the 32-bit i486 ext2 root lane.

Boots the same ATA disk image three times to prove:
1. directory creation succeeds
2. file creation through writefile succeeds inside that directory
3. same-directory rename succeeds
4. cross-directory regular-file rename succeeds
5. cross-directory non-empty directory rename succeeds
6. renamed content persists across reboot
7. non-empty rmdir is rejected
8. unlink removes the moved file
9. empty rmdir succeeds
10. removal persists across reboot
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
BOOT_TIMEOUT = int(os.environ.get("XINIM_QEMU_BOOT_TIMEOUT", "40"))
CMD_TIMEOUT = int(os.environ.get("XINIM_QEMU_CMD_TIMEOUT", "30"))
PROMPT_SETTLE_TIMEOUT = float(os.environ.get("XINIM_QEMU_PROMPT_SETTLE_TIMEOUT", "1.5"))
REBOOT_SETTLE_SECONDS = float(os.environ.get("XINIM_QEMU_REBOOT_SETTLE_SECONDS", "2.0"))
XINIM_LOG_ROOT = os.environ.get(
    "XINIM_LOG_ROOT",
    os.path.join(PROJECT_ROOT, "build", LANE_NAME, "Debug", "logs"),
)
LOG_FILE = os.path.join(XINIM_LOG_ROOT, f"{LANE_NAME}-ext2-mutation.log")
PROMPTS = [prompt for prompt in os.environ.get("XINIM_SHELL_PROMPTS", "$ ||#||# ||mksh$ ").split("||") if prompt]
COMMAND_MARKER_PREFIX = "__XINIM_DONE_"
READY_MARKER = "__XINIM_READY__"
command_counter = 0

RUN_ID = str(int(os.path.getmtime(DISK_IMAGE))) if os.path.exists(DISK_IMAGE) else "0"
SOURCE_DIR_PATH = f"/persist/mutdir-src-{QEMU_MACHINE}-{RUN_ID}"
TARGET_DIR_PATH = f"/persist/mutdir-dst-{QEMU_MACHINE}-{RUN_ID}"
FILE_PATH = f"{SOURCE_DIR_PATH}/created.txt"
RENAMED_PATH = f"{SOURCE_DIR_PATH}/renamed.txt"
MOVED_PATH = f"{TARGET_DIR_PATH}/moved.txt"
REPLACE_SOURCE_PATH = f"{SOURCE_DIR_PATH}/replace-src.txt"
REPLACE_TARGET_PATH = f"{TARGET_DIR_PATH}/replace-dst.txt"
DIR_MOVE_SOURCE = f"{SOURCE_DIR_PATH}/subdir"
DIR_MOVE_TARGET = f"{TARGET_DIR_PATH}/subdir-moved"
DIR_MOVE_FILE = f"{DIR_MOVE_SOURCE}/inside.txt"
DIR_MOVE_FILE_MOVED = f"{DIR_MOVE_TARGET}/inside.txt"
FILE_VALUE = f"created-through-ext2-{QEMU_MACHINE}-{RUN_ID}"
REPLACE_OLD_VALUE = f"replace-old-through-ext2-{QEMU_MACHINE}-{RUN_ID}"
REPLACE_NEW_VALUE = f"replace-new-through-ext2-{QEMU_MACHINE}-{RUN_ID}"
DIR_MOVE_VALUE = f"dir-rename-through-ext2-{QEMU_MACHINE}-{RUN_ID}"


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
            require_contains("mv exists", send_command(first_shell, "command -v mv"), "/bin/mv"),
            require_contains("mkdir exists", send_command(first_shell, "command -v mkdir"), "/bin/mkdir"),
            require_contains("rmdir exists", send_command(first_shell, "command -v rmdir"), "/bin/rmdir"),
            require_contains("unlink exists", send_command(first_shell, "command -v unlink"), "/bin/unlink"),
            require_contains("writefile exists", send_command(first_shell, "command -v writefile"), "/bin/writefile"),
            require_contains("mkdir source", send_command(first_shell, f"mkdir {SOURCE_DIR_PATH}; echo $?"), "0"),
            require_contains("mkdir target", send_command(first_shell, f"mkdir {TARGET_DIR_PATH}; echo $?"), "0"),
            require_contains("new source dir visible", send_command(first_shell, f"test -d {SOURCE_DIR_PATH} && echo yes"), "yes"),
            require_contains("new target dir visible", send_command(first_shell, f"test -d {TARGET_DIR_PATH} && echo yes"), "yes"),
            require_contains("file create", send_command(first_shell, f"writefile {FILE_PATH} {FILE_VALUE}; echo $?"), "0"),
            require_contains("new file content", send_command(first_shell, f"cat {FILE_PATH}"), FILE_VALUE),
            require_contains("new file listed", send_command(first_shell, f"ls {SOURCE_DIR_PATH}"), "created.txt"),
            require_contains("same-dir rename", send_command(first_shell, f"mv {FILE_PATH} {RENAMED_PATH}; echo $?"), "0"),
            require_contains("same-dir old path absent", send_command(first_shell, f"test ! -e {FILE_PATH} && echo gone"), "gone"),
            require_contains("same-dir new path content", send_command(first_shell, f"cat {RENAMED_PATH}"), FILE_VALUE),
            require_contains("cross-dir rename", send_command(first_shell, f"mv {RENAMED_PATH} {MOVED_PATH}; echo $?"), "0"),
            require_contains("cross-dir source path absent", send_command(first_shell, f"test ! -e {RENAMED_PATH} && echo gone"), "gone"),
            require_contains("cross-dir target content", send_command(first_shell, f"cat {MOVED_PATH}"), FILE_VALUE),
            require_contains("cross-dir target listed", send_command(first_shell, f"ls {TARGET_DIR_PATH}"), "moved.txt"),
            require_contains("replacement source create", send_command(first_shell, f"writefile {REPLACE_SOURCE_PATH} {REPLACE_NEW_VALUE}; echo $?"), "0"),
            require_contains("replacement target create", send_command(first_shell, f"writefile {REPLACE_TARGET_PATH} {REPLACE_OLD_VALUE}; echo $?"), "0"),
            require_contains("replacement target old content", send_command(first_shell, f"cat {REPLACE_TARGET_PATH}"), REPLACE_OLD_VALUE),
            require_contains("cross-dir replacement rename", send_command(first_shell, f"mv {REPLACE_SOURCE_PATH} {REPLACE_TARGET_PATH}; echo $?"), "0"),
            require_contains("replacement source absent", send_command(first_shell, f"test ! -e {REPLACE_SOURCE_PATH} && echo gone"), "gone"),
            require_contains("replacement target new content", send_command(first_shell, f"cat {REPLACE_TARGET_PATH}"), REPLACE_NEW_VALUE),
            require_contains("mkdir nested source", send_command(first_shell, f"mkdir {DIR_MOVE_SOURCE}; echo $?"), "0"),
            require_contains("nested file create", send_command(first_shell, f"writefile {DIR_MOVE_FILE} {DIR_MOVE_VALUE}; echo $?"), "0"),
            require_contains("nested file content", send_command(first_shell, f"cat {DIR_MOVE_FILE}"), DIR_MOVE_VALUE),
            require_contains("cross-dir directory rename", send_command(first_shell, f"mv {DIR_MOVE_SOURCE} {DIR_MOVE_TARGET}; echo $?"), "0"),
            require_contains("old nested dir absent", send_command(first_shell, f"test ! -e {DIR_MOVE_SOURCE} && echo gone"), "gone"),
            require_contains("moved nested dir visible", send_command(first_shell, f"test -d {DIR_MOVE_TARGET} && echo yes"), "yes"),
            require_contains("moved nested file content", send_command(first_shell, f"cat {DIR_MOVE_FILE_MOVED}"), DIR_MOVE_VALUE),
        ]
        if not all(results):
            sys.exit(1)
    finally:
        try:
            first_shell.close()
        except OSError:
            pass
        terminate_qemu(first_process)

    time.sleep(REBOOT_SETTLE_SECONDS)

    second_process, second_shell = boot_and_connect()
    try:
        results = [
            require_contains("source dir persisted", send_command(second_shell, f"test -d {SOURCE_DIR_PATH} && echo yes"), "yes"),
            require_contains("target dir persisted", send_command(second_shell, f"test -d {TARGET_DIR_PATH} && echo yes"), "yes"),
            require_contains("same-dir old path still absent", send_command(second_shell, f"test ! -e {FILE_PATH} && echo gone"), "gone"),
            require_contains("cross-dir source path still absent", send_command(second_shell, f"test ! -e {RENAMED_PATH} && echo gone"), "gone"),
            require_contains("moved file persisted", send_command(second_shell, f"cat {MOVED_PATH}"), FILE_VALUE),
            require_contains("replacement source still absent", send_command(second_shell, f"test ! -e {REPLACE_SOURCE_PATH} && echo gone"), "gone"),
            require_contains("replacement target persisted", send_command(second_shell, f"cat {REPLACE_TARGET_PATH}"), REPLACE_NEW_VALUE),
            require_contains("nested source dir still absent", send_command(second_shell, f"test ! -e {DIR_MOVE_SOURCE} && echo gone"), "gone"),
            require_contains("nested target dir persisted", send_command(second_shell, f"test -d {DIR_MOVE_TARGET} && echo yes"), "yes"),
            require_contains("nested target file persisted", send_command(second_shell, f"cat {DIR_MOVE_FILE_MOVED}"), DIR_MOVE_VALUE),
            require_contains("source rmdir after moved-out children", send_command(second_shell, f"rmdir {SOURCE_DIR_PATH}; echo $?"), "0"),
            require_contains("source dir absent after rmdir", send_command(second_shell, f"test ! -e {SOURCE_DIR_PATH} && echo gone"), "gone"),
            require_contains("target rmdir rejects non-empty", send_command(second_shell, f"rmdir {TARGET_DIR_PATH}; echo $?"), "1"),
            require_contains("unlink moved file", send_command(second_shell, f"unlink {MOVED_PATH}; echo $?"), "0"),
            require_contains("moved file absent after unlink", send_command(second_shell, f"test ! -e {MOVED_PATH} && echo gone"), "gone"),
            require_contains("unlink replacement target", send_command(second_shell, f"unlink {REPLACE_TARGET_PATH}; echo $?"), "0"),
            require_contains("replacement target absent after unlink", send_command(second_shell, f"test ! -e {REPLACE_TARGET_PATH} && echo gone"), "gone"),
            require_contains("unlink moved nested file", send_command(second_shell, f"unlink {DIR_MOVE_FILE_MOVED}; echo $?"), "0"),
            require_contains("moved nested file absent after unlink", send_command(second_shell, f"test ! -e {DIR_MOVE_FILE_MOVED} && echo gone"), "gone"),
            require_contains("rmdir moved nested dir", send_command(second_shell, f"rmdir {DIR_MOVE_TARGET}; echo $?"), "0"),
            require_contains("moved nested dir absent after rmdir", send_command(second_shell, f"test ! -e {DIR_MOVE_TARGET} && echo gone"), "gone"),
            require_contains("target rmdir empty dir", send_command(second_shell, f"rmdir {TARGET_DIR_PATH}; echo $?"), "0"),
            require_contains("target dir absent after rmdir", send_command(second_shell, f"test ! -e {TARGET_DIR_PATH} && echo gone"), "gone"),
        ]
        if not all(results):
            sys.exit(1)
    finally:
        try:
            second_shell.close()
        except OSError:
            pass
        terminate_qemu(second_process)

    time.sleep(REBOOT_SETTLE_SECONDS)

    third_process, third_shell = boot_and_connect()
    try:
        results = [
            require_contains("source dir still absent third boot", send_command(third_shell, f"test ! -e {SOURCE_DIR_PATH} && echo gone"), "gone"),
            require_contains("target dir still absent third boot", send_command(third_shell, f"test ! -e {TARGET_DIR_PATH} && echo gone"), "gone"),
            require_contains("source file still absent third boot", send_command(third_shell, f"test ! -e {FILE_PATH} && echo gone"), "gone"),
            require_contains("same-dir renamed path still absent third boot", send_command(third_shell, f"test ! -e {RENAMED_PATH} && echo gone"), "gone"),
            require_contains("moved file still absent third boot", send_command(third_shell, f"test ! -e {MOVED_PATH} && echo gone"), "gone"),
            require_contains("replacement source still absent third boot", send_command(third_shell, f"test ! -e {REPLACE_SOURCE_PATH} && echo gone"), "gone"),
            require_contains("replacement target still absent third boot", send_command(third_shell, f"test ! -e {REPLACE_TARGET_PATH} && echo gone"), "gone"),
            require_contains("nested source dir still absent third boot", send_command(third_shell, f"test ! -e {DIR_MOVE_SOURCE} && echo gone"), "gone"),
            require_contains("nested target dir still absent third boot", send_command(third_shell, f"test ! -e {DIR_MOVE_TARGET} && echo gone"), "gone"),
            require_contains("nested source file still absent third boot", send_command(third_shell, f"test ! -e {DIR_MOVE_FILE} && echo gone"), "gone"),
            require_contains("nested moved file still absent third boot", send_command(third_shell, f"test ! -e {DIR_MOVE_FILE_MOVED} && echo gone"), "gone"),
        ]
        if not all(results):
            sys.exit(1)
    finally:
        try:
            third_shell.close()
        except OSError:
            pass
        terminate_qemu(third_process)


if __name__ == "__main__":
    main()
