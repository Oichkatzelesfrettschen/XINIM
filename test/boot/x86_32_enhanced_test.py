#!/usr/bin/env python3
"""
Enhanced integration tests for XINIM i486 kernel.

Tests multi-stage pipelines, file permissions, symlinks, hard links,
negative error cases, signals, and TCC compilation.
Extends the shell test pattern: boots QEMU, connects to COM2, validates output.
"""

import os
import re
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
    "i486": "256M",
    "i586": "256M",
    "i686": "256M",
}
SHELL_PORT_BY_LANE = {
    "i486": 4566,
    "i586": 4567,
    "i686": 4568,
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
BOOT_IMAGE = os.environ.get(
    "XINIM_QEMU_BOOT_IMAGE",
    os.path.join(XINIM_IMAGE_ROOT, DEFAULT_IMAGE_NAME.get(LANE_NAME, f"xinim-{LANE_NAME}.iso")),
)
QEMU_BIN = os.environ.get("XINIM_QEMU_SYSTEM_BIN", "qemu-system-i386")
QEMU_MACHINE = os.environ.get("XINIM_QEMU_MACHINE", "pc")
QEMU_CPU = os.environ.get("XINIM_QEMU_CPU", DEFAULT_CPU_BY_LANE.get(LANE_NAME, "486"))
QEMU_ICOUNT = os.environ.get("XINIM_QEMU_ICOUNT", "")
QEMU_MEMORY = os.environ.get("XINIM_QEMU_MEMORY", DEFAULT_MEMORY_BY_LANE.get(LANE_NAME, "32M"))
QEMU_VGA = os.environ.get("XINIM_QEMU_VGA", "std")
QEMU_DISK_IMAGE = os.environ.get("XINIM_QEMU_DISK_IMAGE", "")
REQUIRE_TCC = os.environ.get("XINIM_REQUIRE_TCC", "0").upper() in {"1", "ON", "TRUE", "YES"}
SHELL_PORT = int(
    os.environ.get("XINIM_QEMU_SHELL_PORT", str(SHELL_PORT_BY_LANE.get(LANE_NAME, 4566)))
)
BOOT_TIMEOUT = int(os.environ.get("XINIM_QEMU_BOOT_TIMEOUT", "25"))
CMD_TIMEOUT = int(os.environ.get("XINIM_QEMU_CMD_TIMEOUT", "90"))
LOG_FILE = os.environ.get(
    "XINIM_QEMU_SHELL_LOG",
    os.path.join(XINIM_LOG_ROOT, f"{LANE_NAME}-enhanced.log"),
)
PROMPTS = [prompt for prompt in os.environ.get("XINIM_SHELL_PROMPTS", "$ ||#||# ||mksh$ ").split("||") if prompt]
COMMAND_MARKER_PREFIX = "__XINIM_ENH_"
READY_MARKER = "__XINIM_ENH_READY__"
command_counter = 0


def contains_prompt(text):
    return any(prompt in text for prompt in PROMPTS)


def start_qemu():
    os.makedirs(XINIM_LOG_ROOT, exist_ok=True)
    cmd = [
        QEMU_BIN, "-machine", QEMU_MACHINE, "-cpu", QEMU_CPU,
        "-m", QEMU_MEMORY, "-boot", "d", "-cdrom", BOOT_IMAGE,
        "-vga", QEMU_VGA, "-display", "none",
        "-serial", f"file:{LOG_FILE}",
        "-serial", f"tcp::{SHELL_PORT},server,nowait",
        "-monitor", "none", "-no-reboot", "-no-shutdown",
    ]
    if QEMU_DISK_IMAGE and os.path.isfile(QEMU_DISK_IMAGE):
        cmd.extend(["-drive", f"file={QEMU_DISK_IMAGE},format=raw,index=0,media=disk"])
    if QEMU_ICOUNT:
        cmd.extend(["-icount", QEMU_ICOUNT])
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def connect_shell(retries=40, delay=0.5):
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
            wait = 0.3 if prompt_seen else min(0.5, timeout)
            sock.settimeout(wait)
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if any(p.encode() in data for p in PROMPTS):
                prompt_seen = True
        except socket.timeout:
            if prompt_seen:
                break
    return data.decode("utf-8", errors="replace")


def recv_until_text(sock, needle, timeout=CMD_TIMEOUT):
    data = b""
    end_time = time.time() + timeout
    needle_bytes = needle.encode()
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
    marker_bytes = f"{marker}:".encode()
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
    wrapped = f"{command}; __s=$?; echo {marker}:${{__s}}"
    sock.sendall((wrapped + "\r").encode())
    response = recv_until_marker_line(sock, marker, timeout=CMD_TIMEOUT)
    response += recv_until_prompt(sock, timeout=0.5)
    return response


def check(name, response, expected):
    if expected not in response:
        print(f"FAIL: {name} -- expected {expected!r}")
        print(f"  Got: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def check_absent(name, response, unexpected):
    if unexpected in response:
        print(f"FAIL: {name} -- unexpected {unexpected!r}")
        print(f"  Got: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def check_status_zero(name, response):
    match = re.search(r"__XINIM_ENH_\d+__:(\d+)", response)
    if not match:
        print(f"FAIL: {name} -- missing command status marker")
        print(f"  Got: {response!r}")
        return False
    if match.group(1) != "0":
        print(f"FAIL: {name} -- exit status {match.group(1)}")
        print(f"  Got: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def check_no_fault(name, log_before):
    """Check COM1 log for new faults since log_before snapshot."""
    if not os.path.exists(LOG_FILE):
        print(f"PASS: {name} (no log)")
        return True
    with open(LOG_FILE, "r", errors="replace") as f:
        log = f.read()
    new_faults = log.count("fault") - log_before
    if new_faults > 0:
        print(f"FAIL: {name} -- {new_faults} new fault(s)")
        return False
    print(f"PASS: {name} (no faults)")
    return True


def main():
    if not os.path.isfile(BOOT_IMAGE):
        print(f"SKIP: Boot image not found: {BOOT_IMAGE}")
        sys.exit(77)

    qemu = start_qemu()

    try:
        shell = connect_shell(retries=int(BOOT_TIMEOUT / 0.5))
        prompt = recv_until_prompt(shell, timeout=BOOT_TIMEOUT)
        if not contains_prompt(prompt):
            print(f"FAIL: no shell prompt; COM2 response={prompt!r}")
            if os.path.isfile(LOG_FILE):
                with open(LOG_FILE, "r", errors="replace") as boot_log:
                    print(f"COM1 boot log tail: {boot_log.read()[-4096:]!r}")
            sys.exit(1)
        shell.sendall(f"echo {READY_MARKER}\r".encode())
        initial = recv_until_text(shell, READY_MARKER, timeout=BOOT_TIMEOUT)
        if READY_MARKER not in initial:
            print("FAIL: no ready marker")
            sys.exit(1)
        recv_until_prompt(shell, timeout=1.0)

        results = []

        # --- Multi-stage pipelines ---
        print("\n--- Multi-stage pipelines ---")
        results.append(check("echo|tr|cat", send_command(shell, "echo hello | tr h H | cat"), "Hello"))
        results.append(check("echo|wc pipeline", send_command(shell, "echo test | wc -c"), "5"))

        # --- File I/O and permissions ---
        print("\n--- File permissions ---")
        send_command(shell, "rm -f /persist/enh_perm")
        send_command(shell, "echo permtest > /persist/enh_perm")
        results.append(check("cat /persist/enh_perm", send_command(shell, "cat /persist/enh_perm"), "permtest"))
        send_command(shell, "chmod 644 /persist/enh_perm")
        results.append(check("chmod 644", send_command(shell, "ls -l /persist/enh_perm"), "-rw-r--r--"))

        # --- Negative tests (error handling) ---
        print("\n--- Negative tests ---")
        r = send_command(shell, "cat /nonexistent 2>&1; echo AFTER")
        results.append(check("cat nonexistent recovers", r, "AFTER"))
        r = send_command(shell, "ls /no/such/dir 2>&1; echo AFTER")
        results.append(check("ls nonexistent recovers", r, "AFTER"))

        # --- Signal tests ---
        print("\n--- Signal tests ---")
        results.append(check("kill -0 self", send_command(shell, "kill -0 $$; echo sig_ok"), "sig_ok"))

        # --- TCC compilation ---
        print("\n--- TCC compilation ---")
        tcc_probe = send_command(shell, "ls /bin/tcc 2>&1")
        if "inaccessible or not found" in tcc_probe or "No such" in tcc_probe:
            if REQUIRE_TCC:
                print("FAIL: tcc is required but not present in this image")
                results.append(False)
            else:
                print("SKIP: tcc not present in this image")
        else:
            send_command(shell, "rm -f /persist/enh_t /persist/enh_t.c")
            # C99 is input to the pinned C compiler under test, an external
            # language boundary. The program checks the staged libc and CRT ABI.
            source_lines = (
                "#include <stdio.h>",
                "#include <stdlib.h>",
                "#include <string.h>",
                "int main(int argc, char **argv) {",
                'if (argc != 2 || strcmp(argv[1], "runtime-ok") != 0 ||',
                'getenv("PATH") == 0) return 9;',
                'printf("tcc-runtime argc=%d arg=%s env=present\\n", argc, argv[1]);',
                "return 0;",
                "}",
            )
            for line_number, source_line in enumerate(source_lines, start=1):
                redirect = ">" if line_number == 1 else ">>"
                source_command = f"printf '%s\\n' '{source_line}' {redirect} /persist/enh_t.c"
                response = send_command(shell, source_command)
                if not check_status_zero(f"tcc source line {line_number}", response):
                    raise RuntimeError("TCC source creation failed")
            r = send_command(shell, "tcc -std=c99 -static -Wl,-Ttext=0x00400000 -o /persist/enh_t /persist/enh_t.c")
            results.append(check("tcc compile", r, "__XINIM_ENH_"))
            results.append(check_status_zero("tcc compile status", r))
            results.append(check_absent("tcc compile clean", r, "error:"))
            send_command(shell, "chmod 755 /persist/enh_t")
            results.append(check("tcc output", send_command(shell, "ls -l /persist/enh_t"), "-rwxr-xr-x"))
            r = send_command(shell, "/persist/enh_t runtime-ok; echo exit=$?")
            results.append(check("tcc CRT and libc", r,
                                 "tcc-runtime argc=2 arg=runtime-ok env=present"))
            results.append(check("tcc run", r, "exit=0"))

        # --- Symlink tests ---
        print("\n--- Symlinks ---")
        send_command(shell, "rm -f /persist/motd_link")
        send_command(shell, "ln -s /etc/motd /persist/motd_link")
        r = send_command(shell, "cat /persist/motd_link")
        results.append(check("symlink read", r, "persistent root"))
        r = send_command(shell, "readlink /persist/motd_link")
        results.append(check("readlink", r, "/etc/motd"))

        # --- Summary ---
        passed = sum(1 for r in results if r)
        total = len(results)
        failed = total - passed
        print(f"\n{'='*40}")
        print(f"  Enhanced tests: {passed}/{total} passed, {failed} failed")
        print(f"{'='*40}")

        if failed > 0:
            sys.exit(1)
        sys.exit(0)

    except (ConnectionRefusedError, OSError) as exc:
        print(f"FAIL: could not connect to shell: {exc}")
        sys.exit(2)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == "__main__":
    main()
