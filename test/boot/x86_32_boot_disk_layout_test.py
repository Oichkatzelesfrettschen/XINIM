"""Inspect generated x86 boot disks without a libguestfs appliance."""

from __future__ import annotations

import json
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

SECTOR_SIZE = 512
PARTITION_ENTRY_OFFSET = 446
EXPECTED_FILES = {
    "/etc/persist.txt": "persistent-root-ok",
    "/etc/issue": "persistent ext2 root",
    "/etc/persist-profile": "PERSIST_PROFILE=disk-root",
    "/var/disk-marker": "ata-ext2-ready",
}


def run_command(command: list[str]) -> str:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"{' '.join(command[:2])} exited {result.returncode}: {result.stderr.strip()}"
        )
    return result.stdout


def inspect_disk(disk_path: Path, expected_format: str, temporary_root: Path) -> None:
    image_info = json.loads(run_command(["qemu-img", "info", "--output=json", str(disk_path)]))
    if image_info["format"] != expected_format:
        raise RuntimeError(f"{disk_path}: expected {expected_format}, got {image_info['format']}")
    run_command(["qemu-img", "check", str(disk_path)])

    raw_path = temporary_root / f"{expected_format}.raw"
    partition_path = temporary_root / f"{expected_format}.ext2"
    run_command([
        "qemu-img", "convert", "-f", expected_format, "-O", "raw",
        str(disk_path), str(raw_path),
    ])

    with raw_path.open("rb") as raw_image:
        mbr = raw_image.read(SECTOR_SIZE)
        if len(mbr) != SECTOR_SIZE or mbr[510:512] != b"\x55\xaa":
            raise RuntimeError(f"{disk_path}: invalid MBR signature")
        partition = mbr[PARTITION_ENTRY_OFFSET:PARTITION_ENTRY_OFFSET + 16]
        partition_type = partition[4]
        first_sector, sector_count = struct.unpack_from("<II", partition, 8)
        if partition_type != 0x83 or first_sector == 0 or sector_count == 0:
            raise RuntimeError(f"{disk_path}: missing Linux root partition")
        partition_offset = first_sector * SECTOR_SIZE
        partition_size = sector_count * SECTOR_SIZE
        if partition_offset + partition_size > raw_path.stat().st_size:
            raise RuntimeError(f"{disk_path}: root partition extends past the image")

        raw_image.seek(partition_offset + 1024 + 56)
        if raw_image.read(2) != b"\x53\xef":
            raise RuntimeError(f"{disk_path}: root partition is not ext2")

        raw_image.seek(partition_offset)
        with partition_path.open("wb") as partition_image:
            remaining = partition_size
            while remaining:
                chunk = raw_image.read(min(1024 * 1024, remaining))
                if not chunk:
                    raise RuntimeError(f"{disk_path}: truncated root partition")
                partition_image.write(chunk)
                remaining -= len(chunk)

    directory_listing = run_command(["debugfs", "-R", "ls /etc", str(partition_path)])
    if "persist.txt" not in directory_listing:
        raise RuntimeError(f"{disk_path}: missing /etc/persist.txt directory entry")
    for guest_path, marker in EXPECTED_FILES.items():
        content = run_command(["debugfs", "-R", f"cat {guest_path}", str(partition_path)])
        if marker not in content:
            raise RuntimeError(f"{disk_path}: {guest_path} lacks {marker!r}")


def main() -> int:
    lane_name = os.environ.get("XINIM_BOOT_LANE_NAME", "i486")
    image_root = Path(os.environ.get("XINIM_IMAGE_ROOT", "build/images"))
    boot_disk = Path(os.environ.get(
        "XINIM_QEMU_BOOT_DISK", str(image_root / lane_name / f"xinim-{lane_name}-boot.vmdk")
    ))
    if not boot_disk.is_file():
        print(f"SKIP: boot disk not found: {boot_disk}")
        return 77
    for executable in ("qemu-img", "debugfs"):
        if shutil.which(executable) is None:
            print(f"SKIP: {executable} not found")
            return 77

    images = [(boot_disk, "vmdk")]
    qcow2_disk = boot_disk.with_suffix(".qcow2")
    if qcow2_disk.is_file():
        images.append((qcow2_disk, "qcow2"))
    try:
        with tempfile.TemporaryDirectory(prefix="xinim-disk-layout-") as temporary:
            for disk_path, expected_format in images:
                inspect_disk(disk_path, expected_format, Path(temporary))
    except (OSError, RuntimeError, KeyError, ValueError) as error:
        print(f"FAIL: {error}")
        return 1
    print(f"PASS: {lane_name} boot disk layout test")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
