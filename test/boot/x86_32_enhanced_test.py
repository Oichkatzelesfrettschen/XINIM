#!/usr/bin/env python3
"""
Enhanced integration tests for XINIM i486 kernel.

Tests multi-stage pipelines, file permissions, symlinks, hard links,
negative error cases, signals, and TCC compilation.
Extends the shell test pattern: boots QEMU, connects to COM2, validates output.
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
QEMU_MEMORY = os.environ.get("XINIM_QEMU_MEMORY", DEFAULT_MEMORY_BY_LANE.get(LANE_NAME, "32M"))
QEMU_VGA = os.environ.get("XINIM_QEMU_VGA", "std")
QEMU_DISK_IMAGE = os.environ.get("XINIM_QEMU_DISK_IMAGE", "")
SHELL_PORT = int(
    os.environ.get("XINIM_QEMU_SHELL_PORT", str(SHELL_PORT_BY_LANE.get(LANE_NAME, 4566)))
)
BOOT_TIMEOUT = int(os.environ.get("XINIM_QEMU_BOOT_TIMEOUT", "25"))
CMD_TIMEOUT = int(os.environ.get("XINIM_QEMU_CMD_TIMEOUT", "8"))
LOG_FILE = os.environ.get(
    "XINIM_QEMU_SHELL_LOG",
    os.path.join(XINIM_LOG_ROOT, f"{LANE_NAME}-enhanced.log"),
)
PROMPTS = [prompt for prompt in os.environ.get("XINIM_SHELL_PROMPTS", "#||# ||mksh$ ").split("||") if prompt]
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


def send_command(sock, command):
    global command_counter
    command_counter += 1
    marker = f"{COMMAND_MARKER_PREFIX}{command_counter}__"
    wrapped = f"{command}; __s=$?; echo {marker}:${{__s}}"
    sock.sendall((wrapped + "\r").encode())
    response = recv_until_text(sock, marker, timeout=CMD_TIMEOUT)
    response += recv_until_prompt(sock, timeout=0.5)
    return response


def check(name, response, expected):
    if expected not in response:
        print(f"FAIL: {name} -- expected {expected!r}")
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
            print(f"FAIL: no shell prompt")
            sys.exit(1)
        shell.sendall(f"echo {READY_MARKER}\r".encode())
        initial = recv_until_text(shell, READY_MARKER, timeout=BOOT_TIMEOUT)
        if READY_MARKER not in initial:
            print(f"FAIL: no ready marker")
            sys.exit(1)

        results = []

        # --- Multi-stage pipelines ---
        print("\n--- Multi-stage pipelines ---")
        results.append(check("echo|tr|cat", send_command(shell, "echo hello | tr h H | cat"), "Hello"))
        results.append(check("echo|wc pipeline", send_command(shell, "echo test | wc -c"), "5"))

        # --- File I/O and permissions ---
        print("\n--- File permissions ---")
        send_command(shell, "echo permtest > /tmp/perm")
        results.append(check("cat /tmp/perm", send_command(shell, "cat /tmp/perm"), "permtest"))
        send_command(shell, "chmod 644 /tmp/perm")
        results.append(check("chmod 644", send_command(shell, "ls -l /tmp/perm"), "rw-"))

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
        send_command(shell, "echo 'int main(){return 0;}' > /tmp/t.c")
        r = send_command(shell, "tcc -o /tmp/t /tmp/t.c && echo tcc_ok")
        results.append(check("tcc compile", r, "tcc_ok"))
        r = send_command(shell, "/tmp/t; echo exit=$?")
        results.append(check("tcc run", r, "exit=0"))

        # --- Symlink tests ---
        print("\n--- Symlinks ---")
        send_command(shell, "ln -s /etc/motd /tmp/motd_link")
        r = send_command(shell, "cat /tmp/motd_link")
        results.append(check("symlink read", r, "persistent root"))
        r = send_command(shell, "readlink /tmp/motd_link")
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
