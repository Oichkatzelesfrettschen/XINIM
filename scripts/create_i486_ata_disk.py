#!/usr/bin/env python3
"""Create a tiny reproducible raw ATA disk image for the i486 QEMU lane."""

from __future__ import annotations

import argparse
import os
import struct
import shutil
import subprocess
import tempfile
from typing import Iterable


PARTITION_START_LBA = 2048
SECTOR_SIZE = 512
EXT2_BLOCK_SIZE = 1024


def build_mbr(total_sectors: int) -> bytes:
    mbr = bytearray(512)
    marker = b"XINIMHD0"
    mbr[0:len(marker)] = marker

    start_lba = PARTITION_START_LBA
    sector_count = max(total_sectors - start_lba, 1)
    entry = bytearray(16)
    entry[0] = 0x00
    entry[1:4] = b"\x00\x02\x00"
    entry[4] = 0x83
    entry[5:8] = b"\xFF\xFF\xFF"
    entry[8:12] = struct.pack("<I", start_lba)
    entry[12:16] = struct.pack("<I", sector_count)
    mbr[446:462] = entry
    mbr[510:512] = b"\x55\xAA"
    return bytes(mbr)


def install_guest_binaries(root_dir: str, guest_bins: Iterable[str]) -> None:
    bin_dir = os.path.join(root_dir, "bin")
    os.makedirs(bin_dir, exist_ok=True)

    for spec in guest_bins:
        source, separator, target = spec.partition(":")
        if not separator or not source or not target:
            raise SystemExit(f"invalid --guest-bin value: {spec!r}")
        if not os.path.isfile(source):
            raise SystemExit(f"guest binary not found: {source}")

        target_path = target if target.startswith("/") else f"/bin/{target}"
        destination = os.path.join(root_dir, target_path.lstrip("/"))
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        shutil.copy2(source, destination)
        os.chmod(destination, 0o755)


def install_include_tree(root_dir: str, include_dir: str | None) -> None:
    if include_dir and os.path.isdir(include_dir):
        usr_include = os.path.join(root_dir, "usr", "include")
        shutil.copytree(include_dir, usr_include, dirs_exist_ok=True)


def install_tcc_runtime(root_dir: str, runtime_dir: str | None) -> None:
    if not runtime_dir:
        return
    if not os.path.isdir(runtime_dir):
        raise SystemExit(f"TCC runtime directory not found: {runtime_dir}")

    usr_lib = os.path.join(root_dir, "usr", "lib")
    tcc_lib = os.path.join(usr_lib, "tcc")
    os.makedirs(tcc_lib, exist_ok=True)

    for name in ("crt1.o", "crti.o", "crtn.o", "libc.a"):
        source = os.path.join(runtime_dir, name)
        if not os.path.isfile(source):
            raise SystemExit(f"TCC runtime file not found: {source}")
        shutil.copy2(source, os.path.join(usr_lib, name))

    libtcc1 = os.path.join(runtime_dir, "tcc", "libtcc1.a")
    if not os.path.isfile(libtcc1):
        raise SystemExit(f"TCC runtime file not found: {libtcc1}")
    shutil.copy2(libtcc1, os.path.join(tcc_lib, "libtcc1.a"))


def populate_root_tree(root_dir: str, guest_bins: Iterable[str],
                       include_dir: str | None,
                       tcc_runtime_dir: str | None) -> None:
    etc_dir = os.path.join(root_dir, "etc")
    bin_dir = os.path.join(root_dir, "bin")
    var_dir = os.path.join(root_dir, "var")
    os.makedirs(etc_dir, exist_ok=True)
    os.makedirs(bin_dir, exist_ok=True)
    os.makedirs(var_dir, exist_ok=True)

    with open(os.path.join(etc_dir, "persist.txt"), "w", encoding="utf-8") as handle:
        handle.write("persistent-root-ok\n")

    with open(os.path.join(etc_dir, "issue"), "w", encoding="utf-8") as handle:
        handle.write("XINIM i486 persistent ext2 root\n")

    with open(os.path.join(etc_dir, "motd"), "w", encoding="utf-8") as handle:
        handle.write("Welcome to XINIM i486 persistent root\n")

    with open(os.path.join(etc_dir, "persist-profile"), "w", encoding="utf-8") as handle:
        handle.write("export PERSIST_PROFILE=disk-root\n")

    with open(os.path.join(etc_dir, "persist-write-slot"), "w", encoding="utf-8") as handle:
        handle.write("x" * 256 + "\n")

    with open(os.path.join(etc_dir, "profile"), "w", encoding="utf-8") as handle:
        handle.write("export PATH=/bin:/usr/bin\n")
        handle.write("export HOME=/\n")
        handle.write("export TERM=vt100\n")
        handle.write("export SHELL=/bin/mksh\n")

    with open(os.path.join(etc_dir, "mkshrc"), "w", encoding="utf-8") as handle:
        handle.write("PS1='$ '\n")

    with open(os.path.join(var_dir, "disk-marker"), "w", encoding="utf-8") as handle:
        handle.write("ata-ext2-ready\n")

    install_include_tree(root_dir, include_dir)
    install_tcc_runtime(root_dir, tcc_runtime_dir)
    install_guest_binaries(root_dir, guest_bins)


def format_ext2_partition(output_path: str,
                          partition_sector_count: int,
                          guest_bins: Iterable[str],
                          include_dir: str | None,
                          tcc_runtime_dir: str | None) -> None:
    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        raise SystemExit("mke2fs is required to build the i486 ATA ext2 image")

    partition_offset = PARTITION_START_LBA * SECTOR_SIZE
    ext2_block_count = partition_sector_count // (EXT2_BLOCK_SIZE // SECTOR_SIZE)

    with tempfile.TemporaryDirectory(prefix="xinim-i486-root-") as root_dir:
        populate_root_tree(root_dir, guest_bins, include_dir, tcc_runtime_dir)
        subprocess.run(
            [
                mke2fs,
                "-F",
                "-q",
                "-t",
                "ext2",
                "-d",
                root_dir,
                "-b",
                str(EXT2_BLOCK_SIZE),
                "-L",
                "XINIMROOT",
                "-E",
                f"offset={partition_offset}",
                output_path,
                str(ext2_block_count),
            ],
            check=True,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, help="Path to the raw disk image")
    parser.add_argument("--size-mb", type=int, default=16, help="Disk size in MiB")
    parser.add_argument(
        "--guest-bin",
        action="append",
        default=[],
        help="Guest binary copy spec SOURCE:TARGET_PATH",
    )
    parser.add_argument("--include-dir", default="",
                        help="Path to C header directory to install at /usr/include")
    parser.add_argument("--tcc-runtime-dir", default="",
                        help="Path to staged TCC runtime files to install under /usr/lib")
    args = parser.parse_args()

    if args.size_mb < 4:
        raise SystemExit("--size-mb must be at least 4")

    total_bytes = args.size_mb * 1024 * 1024
    total_sectors = total_bytes // 512

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as handle:
        handle.truncate(total_bytes)

    partition_sector_count = max(total_sectors - PARTITION_START_LBA, 1)

    with open(args.output, "r+b") as handle:
        handle.seek(0)
        handle.write(build_mbr(total_sectors))

    format_ext2_partition(
        args.output,
        partition_sector_count,
        args.guest_bin,
        args.include_dir if args.include_dir else None,
        args.tcc_runtime_dir if args.tcc_runtime_dir else None,
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
