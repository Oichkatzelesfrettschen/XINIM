"""Exercise the i486 boot disk with the launcher's virtio device present."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time


def digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def receive_until(connection: socket.socket, pattern: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    response = ""
    while time.monotonic() < deadline:
        connection.settimeout(min(0.2, max(0.001, deadline - time.monotonic())))
        try:
            chunk = connection.recv(65536)
        except TimeoutError:
            continue
        if not chunk:
            raise RuntimeError(f"COM2 closed: {response!r}")
        response += chunk.decode("utf-8", errors="replace").replace("\r", "")
        if re.search(pattern, response, re.MULTILINE):
            return response
    raise RuntimeError(f"COM2 deadline waiting for {pattern!r}: {response!r}")


def run(args: argparse.Namespace) -> None:
    disk = args.disk.resolve(strict=True)
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    disk_hash = digest(disk)
    image_info = json.loads(subprocess.check_output(
        ["qemu-img", "info", "--output=json", str(disk)], text=True
    ))
    disk_options = f"file={disk},format={image_info['format']},index=0,media=disk,snapshot=on"
    if args.read_bps:
        disk_options += f",bps_rd={args.read_bps}"
    with tempfile.TemporaryDirectory(prefix="xinim-i486-") as temporary:
        shell_path = Path(temporary) / "com2.sock"
        command = [
            args.qemu, "-machine", "pc-i440fx-11.1", "-accel", "tcg",
            "-cpu", "486", "-smp", "1", "-m", args.memory,
            "-boot", "d" if args.iso else "c", "-drive",
            disk_options,
            "-vga", "std", "-display", "none",
            "-serial", f"file:{output / 'serial.log'}",
            "-serial", f"unix:{shell_path},server=on,wait=off",
            "-monitor", "none", "-no-reboot", "-no-shutdown",
            "-netdev", "user,id=net0,restrict=on",
            "-device", "virtio-net-pci,netdev=net0",
        ]
        if args.iso:
            command.extend(["-cdrom", str(args.iso.resolve(strict=True))])
        (output / "command.json").write_text(json.dumps(command, indent=2) + "\n")
        (output / "qemu-version.txt").write_text(subprocess.check_output(
            [args.qemu, "--version"], text=True
        ))
        with (output / "qemu-stderr.log").open("w") as stderr, \
                (output / "com2.log").open("w") as transcript:
            guest = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=stderr)
            passed = False
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                    deadline = time.monotonic() + 25
                    while True:
                        if guest.poll() is not None:
                            raise RuntimeError(f"QEMU exited with {guest.returncode}")
                        try:
                            connection.connect(str(shell_path))
                            break
                        except (FileNotFoundError, ConnectionRefusedError):
                            if time.monotonic() >= deadline:
                                raise RuntimeError("COM2 socket readiness deadline") from None
                            time.sleep(0.05)
                    transcript.write(receive_until(connection, r"(?:^|\n)[#$] $", 25))
                    cases = [
                        ("identity", 'printf "shell=%s pid=%s\\n" "$SHELL" "$$"',
                         r"^shell=/bin/mksh pid=1$"),
                        ("exec", "/bin/hello qemu486", r"^argv\[1\]: qemu486$"),
                        ("heap", "/bin/heapprobe", r"^heapprobe: ok$"),
                        ("pipe", "printf 'beta\\nalpha\\n' | sort | head -n 1", r"^alpha$"),
                        ("ext2", "printf disk-data > /persist/qemu-check && "
                         "cat /persist/qemu-check && echo", r"^disk-data$"),
                        ("fork-reuse", "completed=0; for count in 1 2 3 4 5 6 7 8 9 10 11 12; "
                         "do /bin/hello reuse >/dev/null || break; completed=$count; "
                         "done; echo reuse=$completed",
                         r"^reuse=12$"),
                        ("invalid-exec-keeps-shell", "printf bad-elf > /persist/bad-elf && "
                         "chmod 755 /persist/bad-elf && test -x /persist/bad-elf && "
                         "{ /persist/bad-elf >/dev/null 2>&1; test $? -ne 0; } && "
                         "/bin/hello survived",
                         r"^argv\[1\]: survived$"),
                        ("subshell-child-exit", "(/bin/hello child >/dev/null && "
                         "printf 'subshell-alive\\n')", r"^subshell-alive$"),
                        ("exec-environment", "export FOO=global; "
                         "FOO=local /bin/hello env-path-arg", r"^env\[\d+\]: FOO=local$"),
                    ]
                    if args.iso:
                        cases.insert(-1, (
                            "exec-storage", ". /persist/etc/persist-profile; "
                            ". /etc/persist-profile; "
                            "PATH=/persist/bin:/bin persist-hello disk && "
                            "PATH=/bin persist-hello canonical", r"^argv\[1\]: canonical$"
                        ))
                    for index, (name, shell_command, expected) in enumerate(cases):
                        marker = f"__I486_DISK_{index}__"
                        wrapped = f"{shell_command}; result=$?; printf '\\n{marker}:%s\\n' $result\r"
                        payload = wrapped.encode()
                        if len(payload) >= 256:
                            raise RuntimeError(f"{name}: command exceeds the 256-byte TTY line buffer")
                        connection.sendall(payload)
                        response = receive_until(
                            connection, rf"^{marker}:\d+\n[\s\S]*[#$] $", 30
                        )
                        transcript.write(response)
                        transcript.flush()
                        if not re.search(expected, response, re.MULTILINE):
                            raise RuntimeError(f"{name}: missing {expected!r}: {response!r}")
                        if not re.search(rf"^{marker}:0$", response, re.MULTILINE):
                            raise RuntimeError(f"{name}: unsuccessful exit: {response!r}")
                        print(f"PASS: {name}", flush=True)
                    serial = (output / "serial.log").read_text()
                    for marker in (
                        "boot protocol: multiboot2",
                        "virtio-net: DRIVER_OK set, device live",
                        "ext2 mount: ready path=/persist",
                        "Launching supervised Ring 3 services under timer scheduler",
                    ):
                        if marker not in serial:
                            raise RuntimeError(f"missing kernel marker: {marker}")
                    allocator = re.search(r"DMA allocator: (\d+) KB available", serial)
                    arena = re.search(
                        r"process image capacity: (\d+)\r?\nprocess arena bytes: (\d+)",
                        serial,
                    )
                    if allocator is None or arena is None:
                        raise RuntimeError("missing process arena reservation accounting")
                    capacity, arena_bytes = map(int, arena.groups())
                    available_bytes = int(allocator.group(1)) * 1024
                    if not (2 <= capacity <= 9 and arena_bytes == capacity * 4 * 1024 * 1024
                            and available_bytes - arena_bytes >= 1024 * 1024):
                        raise RuntimeError("process reservation consumed the device budget")
                    backing = re.search(
                        r"i486 user backing live bytes=(\d+) reserved bytes=(\d+)", serial
                    )
                    if backing is None:
                        raise RuntimeError("missing i486 user backing accounting")
                    live_bytes, reserved_bytes = map(int, backing.groups())
                    if live_bytes != 2 * 4 * 1024 * 1024 or not (
                        live_bytes <= reserved_bytes <= 3 * 4 * 1024 * 1024
                    ):
                        raise RuntimeError(
                            f"unexpected user backing: live={live_bytes} "
                            f"reserved={reserved_bytes}"
                        )
                    for fault in (
                        "rescue shell", "faulted", "i486 fault", "Unhandled i486 syscall",
                        "Respawning supervised service", "PANIC",
                    ):
                        if fault in serial:
                            raise RuntimeError(f"kernel reported {fault!r}")
                    # A departing shell's prompt cannot witness a new shell.
                    connection.sendall(b"PS1='departing-shell> '; kill -KILL $$\r")
                    transcript.write(receive_until(connection, r"(?:^|\n)[#$] $", 25))
                    connection.sendall(
                        b'printf "restart=%s:%s\\n" "$SHELL" "$$"\r'
                    )
                    response = receive_until(
                        connection, r"^restart=/bin/mksh:1\n[\s\S]*[#$] $", 25
                    )
                    transcript.write(response)
                    transcript.flush()
                    serial = (output / "serial.log").read_text()
                    if "Respawning supervised service init-shell" not in serial:
                        raise RuntimeError("supervised shell restart marker missing")
                    for fault in ("rescue shell", "i486 fault", "PANIC"):
                        if fault in serial:
                            raise RuntimeError(f"restart reported {fault!r}")
                    print("PASS: signal-killed shell respawn", flush=True)
                if guest.poll() is not None:
                    raise RuntimeError(f"QEMU exited during guest checks: {guest.returncode}")
                passed = True
            finally:
                guest.terminate()
                try:
                    guest.wait(timeout=5)
                    stop = "host SIGTERM"
                except subprocess.TimeoutExpired:
                    guest.kill()
                    guest.wait(timeout=5)
                    stop = "host SIGKILL after SIGTERM deadline"
                after_hash = digest(disk)
                (output / "result.json").write_text(json.dumps({
                    "disk_sha256_before": disk_hash,
                    "disk_sha256_after": after_hash,
                    "qemu_exit": guest.returncode,
                    "stop": stop,
                    "guest_checks_passed": passed,
                }, indent=2) + "\n")
                if disk_hash != after_hash:
                    raise RuntimeError("base disk hash changed during snapshot run")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disk", type=Path, required=True)
    parser.add_argument("--iso", type=Path, help="Boot an ISO with the disk as persistent storage")
    parser.add_argument("--read-bps", type=int, default=0,
                        help="Limit guest disk read bandwidth to exercise delayed ATA completion")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--qemu", default="qemu-system-i386")
    parser.add_argument("--memory", default="64M")
    args = parser.parse_args()
    if args.read_bps < 0:
        parser.error("--read-bps must be nonnegative")
    run(args)


if __name__ == "__main__":
    main()
