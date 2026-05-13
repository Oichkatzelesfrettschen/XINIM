#!/usr/bin/env python3
"""Instrumented QEMU runner for XINIM 32-bit lanes."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_BUILD_DIR = REPO_ROOT / "build" / "i686" / "Debug"
TRACE_PROFILES = {
    "crash": "int,cpu_reset,guest_errors,unimp",
    "exec": "int,cpu_reset,guest_errors,unimp,in_asm",
    "mmu": "int,cpu_reset,guest_errors,unimp,mmu",
}
MAX_SCAN_REPOS = 24
MAX_SCAN_FILES_PER_REPO = 5000
MAX_SCAN_HITS_PER_REPO = 40
SERIAL_MARKERS = [
    "XINIM {banner} Booting",
    "boot protocol: multiboot2",
    "bootfs promoted entries:",
    "DMA allocator:",
    "ATA primary master: present",
    "ATA primary master ext2:",
    "PCI bus enumeration complete",
    "ext2 reader: ready",
    "ext2 mount: ready path=/persist",
    "XINIM {banner} supervised-init lane ready",
    "Seeding supervised init service from Multiboot2 module",
    "Prepared supervised support service hold-service",
    "Launching supervised Ring 3 services under timer scheduler",
]
EXCEPTION_NAMES = {
    "00": "divide error",
    "01": "debug",
    "03": "breakpoint",
    "06": "invalid opcode",
    "08": "double fault",
    "0d": "general protection fault",
    "0e": "page fault",
}


@dataclass
class DebugPaths:
    output_dir: pathlib.Path
    serial_log: pathlib.Path
    qemu_trace: pathlib.Path
    qemu_stderr: pathlib.Path
    device_help: pathlib.Path
    summary: pathlib.Path
    repo_scan: pathlib.Path


def load_manifest(build_dir: pathlib.Path) -> dict:
    path = build_dir / "x86_cpu_lanes.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise SystemExit(f"lane manifest not found: {path}") from exc
    lanes = data.get("lanes")
    if not isinstance(lanes, dict):
        raise SystemExit(f"invalid lane manifest: {path}")
    return lanes


def lane_executable(build_dir: pathlib.Path, lane: str) -> pathlib.Path:
    return build_dir / f"xinim-{lane}"


def make_paths(build_dir: pathlib.Path, lane: str, boot_mode: str) -> DebugPaths:
    output_dir = build_dir / "logs" / "qemu-debug" / lane / boot_mode
    output_dir.mkdir(parents=True, exist_ok=True)
    return DebugPaths(
        output_dir=output_dir,
        serial_log=output_dir / "serial.log",
        qemu_trace=output_dir / "qemu-trace.log",
        qemu_stderr=output_dir / "qemu-stderr.log",
        device_help=output_dir / "qemu-device-help.txt",
        summary=output_dir / "summary.md",
        repo_scan=output_dir / "local-driver-source-scan.md",
    )


def run_capture(command: list[str], output: pathlib.Path, timeout: int | None = None) -> int:
    with output.open("w", encoding="utf-8", errors="replace") as handle:
        try:
            completed = subprocess.run(
                command,
                stdout=handle,
                stderr=subprocess.STDOUT,
                check=False,
                timeout=timeout,
                text=True,
            )
            return completed.returncode
        except subprocess.TimeoutExpired:
            handle.write(f"\nTIMEOUT after {timeout}s\n")
            return 124


def qemu_command(
    lane_meta: dict,
    paths: DebugPaths,
    boot_mode: str,
    trace: str,
    gdb: bool,
) -> list[str]:
    qemu_bin = lane_meta["qemu_bin"]
    command = [
        qemu_bin,
        "-machine",
        lane_meta["qemu_machine"],
        "-cpu",
        lane_meta["qemu_cpu"],
        "-m",
        lane_meta["qemu_memory"],
        "-vga",
        lane_meta["qemu_vga"],
        "-display",
        "none",
        "-serial",
        f"file:{paths.serial_log}",
        "-monitor",
        "none",
        "-no-reboot",
        "-no-shutdown",
        "-d",
        trace,
        "-D",
        str(paths.qemu_trace),
    ]
    if boot_mode == "disk":
        command.extend(
            [
                "-boot",
                "c",
                "-drive",
                (
                    f"file={lane_meta['boot_disk_path']},"
                    f"format={lane_meta.get('boot_disk_format', 'raw')},"
                    "index=0,media=disk"
                ),
            ]
        )
    else:
        command.extend(["-boot", "d", "-cdrom", lane_meta["image_path"]])
        ata = lane_meta.get("ata_disk_path", "")
        if ata:
            command.extend(["-drive", f"file={ata},format=raw,index=0,media=disk"])
    if gdb:
        command.extend(["-S", "-s"])
    return command


def check_image(path: str, fmt: str, summary: list[str]) -> None:
    if not path:
        return
    image_path = pathlib.Path(path)
    if not image_path.exists():
        summary.append(f"- image missing: `{image_path}`")
        return
    file_bin = shutil.which("file")
    qemu_img = shutil.which("qemu-img")
    if file_bin:
        completed = subprocess.run([file_bin, str(image_path)], text=True, capture_output=True, check=False)
        summary.append(f"- file: `{completed.stdout.strip()}`")
    if qemu_img and fmt in {"raw", "qcow2", "vmdk"}:
        info = subprocess.run([qemu_img, "info", str(image_path)], text=True, capture_output=True, check=False)
        first = next((line for line in info.stdout.splitlines() if line.startswith("file format:")), "")
        size = next((line for line in info.stdout.splitlines() if line.startswith("virtual size:")), "")
        if first or size:
            summary.append(f"- qemu-img: `{first}` `{size}`")
        check = subprocess.run([qemu_img, "check", str(image_path)], text=True, capture_output=True, check=False)
        verdict = "ok" if check.returncode == 0 else f"failed rc={check.returncode}"
        summary.append(f"- qemu-img check: `{verdict}`")


def classify_serial(serial_text: str, banner: str) -> tuple[list[str], list[str]]:
    seen: list[str] = []
    missing: list[str] = []
    for marker in SERIAL_MARKERS:
        expanded = marker.format(banner=banner)
        if expanded in serial_text:
            seen.append(expanded)
        else:
            missing.append(expanded)
    return seen, missing


def symbolicate_ip(executable: pathlib.Path, ip: str) -> str:
    if not executable.exists() or shutil.which("addr2line") is None:
        return ""
    completed = subprocess.run(
        ["addr2line", "-e", str(executable), "-f", "-C", ip],
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        return ""
    lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
    return " / ".join(lines[:2])


def classify_trace(trace_text: str, executable: pathlib.Path) -> list[str]:
    findings: list[str] = []
    if "Triple fault" in trace_text:
        findings.append("- qemu trace: `Triple fault`")
    for match in re.finditer(r"v=([0-9a-fA-F]{2}).*?IP=[0-9a-fA-F]+:([0-9a-fA-F]{8})", trace_text):
        vector = match.group(1).lower()
        ip = f"0x{match.group(2)}"
        name = EXCEPTION_NAMES.get(vector, "unknown exception")
        location = symbolicate_ip(executable, ip)
        suffix = f" -> {location}" if location else ""
        findings.append(f"- qemu exception: vector 0x{vector} ({name}) at `{ip}`{suffix}")
    if not findings:
        findings.append("- qemu trace: no CPU exception marker found")
    return findings[:12]


def capture_device_help(qemu_bin: str, paths: DebugPaths) -> None:
    if shutil.which(qemu_bin) is None:
        paths.device_help.write_text(f"{qemu_bin} not found\n", encoding="utf-8")
        return
    run_capture([qemu_bin, "-machine", "pc", "-device", "help"], paths.device_help, timeout=10)


def scan_local_repos(paths: DebugPaths) -> None:
    roots = sorted(root for root in REPO_ROOT.parent.glob("*") if (root / ".git").exists())
    source_suffixes = {".c", ".h", ".cc", ".cpp", ".hpp", ".s", ".S", ".asm", ".a", ".md", ".txt"}
    skip_parts = {
        ".git",
        "build",
        "build-i386",
        "build_verify",
        ".mypy_cache",
        ".pytest_cache",
        ".venv",
        "__pycache__",
        "node_modules",
        "venv",
    }
    tokens = (
        "ide",
        "ata",
        "pci",
        "virtio",
        "e1000",
        "ne2000",
        "rtl8139",
        "uart",
        "serial",
        "picirq",
        "pic",
        "pit",
        "lapic",
        "ioapic",
        "trap",
        "intr",
        "disk",
        "block",
        "driver",
    )

    def interesting_path(file_path: pathlib.Path, root: pathlib.Path) -> bool:
        if any(part in skip_parts or part.startswith("build") for part in file_path.relative_to(root).parts):
            return False
        if file_path.suffix not in source_suffixes:
            return False
        rel = str(file_path.relative_to(root)).lower()
        parts = re.split(r"[^a-z0-9]+", rel)
        part_set = set(parts)
        if part_set.intersection(tokens):
            return True
        return any(
            name in rel
            for name in (
                "picirq",
                "ioapic",
                "lapic",
                "memide",
                "trapasm",
                "pci.c",
                "ide.c",
                "uart.c",
                "fd.c",
            )
        )
    lines = [
        "# Local Driver Source Scan",
        "",
        "Use this as a provenance map, not as permission to copy code. Prefer BSD/MIT/public-domain sources for direct adaptation; use GPL/ancient proprietary-looking trees only for behavioral comparison unless relicensing is confirmed.",
        "",
    ]
    scanned_repos = 0
    for root in roots:
        if root.resolve() == REPO_ROOT:
            continue
        if scanned_repos >= MAX_SCAN_REPOS:
            lines.append(f"- scan truncated after {MAX_SCAN_REPOS} sibling git repositories")
            break
        license_files = list(root.glob("LICENSE*")) + list(root.glob("COPYING*")) + list(root.glob("README*"))
        hits = []
        scanned_files = 0
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [
                dirname for dirname in dirnames
                if dirname not in skip_parts and not dirname.startswith("build")
            ]
            current_dir = pathlib.Path(dirpath)
            for filename in filenames:
                file_path = current_dir / filename
                scanned_files += 1
                if scanned_files > MAX_SCAN_FILES_PER_REPO:
                    break
                rel = str(file_path.relative_to(root))
                if interesting_path(file_path, root):
                    hits.append(rel)
                if len(hits) >= MAX_SCAN_HITS_PER_REPO:
                    break
            if scanned_files > MAX_SCAN_FILES_PER_REPO or len(hits) >= MAX_SCAN_HITS_PER_REPO:
                break
        if not hits:
            continue
        scanned_repos += 1
        lines.append(f"## {root.name}")
        for license_file in sorted(license_files)[:6]:
            lines.append(f"- license/readme: `{license_file.relative_to(root)}`")
        for hit in hits:
            lines.append(f"- driver-ish: `{hit}`")
        lines.append("")
    paths.repo_scan.write_text("\n".join(lines), encoding="utf-8")


def write_summary(
    paths: DebugPaths,
    lane: str,
    lane_meta: dict,
    boot_mode: str,
    qemu_rc: int,
    command: list[str],
    image_summary: list[str],
    executable: pathlib.Path,
) -> None:
    serial_text = paths.serial_log.read_text(encoding="utf-8", errors="replace") if paths.serial_log.exists() else ""
    trace_text = paths.qemu_trace.read_text(encoding="utf-8", errors="replace") if paths.qemu_trace.exists() else ""
    seen, missing = classify_serial(serial_text, lane_meta.get("banner_name", lane))
    trace_findings = classify_trace(trace_text, executable)
    lines = [
        f"# QEMU Debug Summary: {lane} {boot_mode}",
        "",
        f"- qemu return: `{qemu_rc}` (124 means timeout after successful live run)",
        f"- executable: `{executable}`",
        f"- serial log: `{paths.serial_log}`",
        f"- qemu trace: `{paths.qemu_trace}`",
        f"- device help: `{paths.device_help}`",
        f"- command: `{' '.join(command)}`",
        "",
        "## Image Checks",
        *image_summary,
        "",
        "## Serial Markers Seen",
        *(f"- `{marker}`" for marker in seen),
        "",
        "## First Missing Serial Markers",
        *(f"- `{marker}`" for marker in missing[:8]),
        "",
        "## Trace Classification",
        *trace_findings,
        "",
        "## Fault Buckets",
        "- build/config: CMake target or host flags prevent image creation",
        "- image/container: qemu-img check, wrong format, corrupt MBR/partition/ext2",
        "- bootloader/handoff: no XINIM banner or no multiboot2 marker",
        "- CPU exception: QEMU trace vectors, triple fault, addr2line symbolication",
        "- device/driver: ATA/PCI/virtio markers absent or QEMU device help mismatch",
        "- filesystem: ext2 superblock/path markers absent",
        "- userspace: supervised-init marker present but no shell prompt/command response",
        "",
    ]
    paths.summary.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIR))
    parser.add_argument("--lane", default="i686")
    parser.add_argument("--boot-mode", choices=("disk", "iso"), default="disk")
    parser.add_argument("--trace-profile", choices=sorted(TRACE_PROFILES), default="crash")
    parser.add_argument("--timeout", type=int, default=20)
    parser.add_argument("--gdb", action="store_true", help="start paused with -S -s for gdb on tcp::1234")
    parser.add_argument("--scan-local-repos", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    build_dir = pathlib.Path(args.build_dir)
    lanes = load_manifest(build_dir)
    if args.lane not in lanes:
        raise SystemExit(f"unknown lane: {args.lane}")
    lane_meta = lanes[args.lane]
    paths = make_paths(build_dir, args.lane, args.boot_mode)
    executable = lane_executable(build_dir, args.lane)
    trace = TRACE_PROFILES[args.trace_profile]
    command = qemu_command(lane_meta, paths, args.boot_mode, trace, args.gdb)

    print("$", " ".join(command))
    if args.dry_run:
        return 0

    capture_device_help(lane_meta["qemu_bin"], paths)
    image_summary: list[str] = []
    if args.boot_mode == "disk":
        check_image(lane_meta.get("boot_disk_path", ""), lane_meta.get("boot_disk_format", "raw"), image_summary)
    else:
        check_image(lane_meta.get("image_path", ""), "iso", image_summary)
        check_image(lane_meta.get("ata_disk_path", ""), "raw", image_summary)

    if args.scan_local_repos:
        scan_local_repos(paths)

    with paths.qemu_stderr.open("w", encoding="utf-8", errors="replace") as stderr:
        try:
            qemu = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=stderr, timeout=args.timeout, check=False)
            qemu_rc = qemu.returncode
        except subprocess.TimeoutExpired:
            qemu_rc = 124

    write_summary(paths, args.lane, lane_meta, args.boot_mode, qemu_rc, command, image_summary, executable)
    print(f"summary: {paths.summary}")
    print(f"serial:  {paths.serial_log}")
    print(f"trace:   {paths.qemu_trace}")
    if args.scan_local_repos:
        print(f"repos:   {paths.repo_scan}")
    return 0 if qemu_rc in (0, 124) else qemu_rc


if __name__ == "__main__":
    sys.exit(main())
