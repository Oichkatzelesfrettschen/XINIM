#!/usr/bin/env python3
"""Create a GRUB-bootable raw disk image for the i486 QEMU lane.

Produces a single disk image with:
  - MBR with GRUB stage 1
  - Partition 1: ext2 filesystem containing kernel, shell, and all utilities

This replaces the ISO+ATA-disk two-image boot flow with a single bootable disk.
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
from typing import Iterable


PARTITION_START_LBA = 2048
SECTOR_SIZE = 512
EXT2_BLOCK_SIZE = 1024


def build_mbr(total_sectors: int) -> bytes:
    """Build a minimal MBR with one Linux partition."""
    mbr = bytearray(512)
    marker = b"XINIMBT0"
    mbr[0 : len(marker)] = marker

    start_lba = PARTITION_START_LBA
    sector_count = max(total_sectors - start_lba, 1)
    entry = bytearray(16)
    entry[0] = 0x80  # Bootable flag
    entry[1:4] = b"\x00\x02\x00"
    entry[4] = 0x83  # Linux
    entry[5:8] = b"\xFF\xFF\xFF"
    entry[8:12] = struct.pack("<I", start_lba)
    entry[12:16] = struct.pack("<I", sector_count)
    mbr[446:462] = entry
    mbr[510:512] = b"\x55\xAA"
    return bytes(mbr)


def install_guest_binaries(root_dir: str, guest_bins: Iterable[str]) -> None:
    """Copy guest binaries into the filesystem tree."""
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


def populate_root_tree(
    root_dir: str,
    kernel_path: str,
    shell_path: str,
    grub_cfg_path: str,
    motd_path: str | None,
    guest_bins: Iterable[str],
) -> None:
    """Populate the filesystem tree with kernel, GRUB config, and utilities."""
    boot_dir = os.path.join(root_dir, "boot")
    grub_dir = os.path.join(boot_dir, "grub")
    bin_dir = os.path.join(root_dir, "bin")
    etc_dir = os.path.join(root_dir, "etc")
    dev_dir = os.path.join(root_dir, "dev")
    tmp_dir = os.path.join(root_dir, "tmp")
    var_dir = os.path.join(root_dir, "var")

    for d in (boot_dir, grub_dir, bin_dir, etc_dir, dev_dir, tmp_dir, var_dir):
        os.makedirs(d, exist_ok=True)

    # Kernel
    shutil.copy2(kernel_path, os.path.join(boot_dir, "xinim"))

    # Shell -- install as mksh, xash, and sh
    for name in ("mksh", "xash", "sh"):
        dest = os.path.join(bin_dir, name)
        shutil.copy2(shell_path, dest)
        os.chmod(dest, 0o755)

    # GRUB config for disk boot (simpler than ISO -- no module2 lines needed
    # since utilities live on the ext2 partition and are loaded by execve)
    with open(os.path.join(grub_dir, "grub.cfg"), "w", encoding="utf-8") as f:
        f.write("set timeout=0\n")
        f.write("set default=0\n\n")
        f.write('menuentry "XINIM i486" {\n')
        f.write("    multiboot2 /boot/xinim\n")
        # Shell must be a Multiboot2 module so it is available before ext2 init
        f.write("    module2 /boot/xinim /boot/xinim\n")
        f.write("    boot\n")
        f.write("}\n")

    # /etc files
    with open(os.path.join(etc_dir, "profile"), "w", encoding="utf-8") as f:
        f.write("export PATH=/bin:/usr/bin\n")
        f.write("export HOME=/\n")
        f.write("export TERM=vt100\n")
        f.write("export SHELL=/bin/mksh\n")

    with open(os.path.join(etc_dir, "mkshrc"), "w", encoding="utf-8") as f:
        f.write("PS1='$ '\n")

    if motd_path and os.path.isfile(motd_path):
        shutil.copy2(motd_path, os.path.join(etc_dir, "motd"))
    else:
        with open(os.path.join(etc_dir, "motd"), "w", encoding="utf-8") as f:
            f.write("XINIM i486 -- booted from disk\n")

    install_guest_binaries(root_dir, guest_bins)


def format_ext2_partition(
    output_path: str, partition_sector_count: int, root_dir: str
) -> None:
    """Create ext2 filesystem inside the partition area of the disk image."""
    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        raise SystemExit("mke2fs is required to build the boot disk image")

    partition_offset = PARTITION_START_LBA * SECTOR_SIZE
    ext2_block_count = partition_sector_count // (EXT2_BLOCK_SIZE // SECTOR_SIZE)

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


def install_grub(output_path: str) -> None:
    """Install GRUB to the disk image MBR using grub-install."""
    grub_install = shutil.which("grub-install")
    if grub_install is None:
        print("WARNING: grub-install not found; disk will not be directly bootable")
        print("         Use with QEMU -kernel or chain-load from ISO instead")
        return

    # Create a temporary loop device for grub-install
    # This requires root. If not available, skip and warn.
    try:
        subprocess.run(
            [
                grub_install,
                "--target=i386-pc",
                "--boot-directory=/boot",
                f"--install-modules=multiboot2 part_msdos ext2 normal",
                output_path,
            ],
            check=True,
            capture_output=True,
        )
    except (subprocess.CalledProcessError, PermissionError) as exc:
        print(f"WARNING: grub-install failed ({exc}); disk has no bootloader")
        print("         Boot with: qemu-system-i386 -kernel xinim -hda disk.img")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a bootable disk image for XINIM i486"
    )
    parser.add_argument("--output", required=True, help="Path to the raw disk image")
    parser.add_argument("--size-mb", type=int, default=32, help="Disk size in MiB")
    parser.add_argument("--kernel", required=True, help="Path to the kernel binary")
    parser.add_argument("--shell", required=True, help="Path to the shell binary (mksh)")
    parser.add_argument("--grub-cfg", default="", help="Path to grub.cfg template")
    parser.add_argument("--motd", default="", help="Path to /etc/motd file")
    parser.add_argument(
        "--guest-bin",
        action="append",
        default=[],
        help="Guest binary copy spec SOURCE:TARGET_PATH",
    )
    parser.add_argument(
        "--no-grub-install",
        action="store_true",
        help="Skip grub-install (produce data disk, not bootable disk)",
    )
    args = parser.parse_args()

    if args.size_mb < 8:
        raise SystemExit("--size-mb must be at least 8")
    if not os.path.isfile(args.kernel):
        raise SystemExit(f"kernel not found: {args.kernel}")
    if not os.path.isfile(args.shell):
        raise SystemExit(f"shell not found: {args.shell}")

    total_bytes = args.size_mb * 1024 * 1024
    total_sectors = total_bytes // SECTOR_SIZE
    partition_sector_count = max(total_sectors - PARTITION_START_LBA, 1)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    # Create the raw disk image
    with open(args.output, "wb") as f:
        f.truncate(total_bytes)

    # Write MBR
    with open(args.output, "r+b") as f:
        f.seek(0)
        f.write(build_mbr(total_sectors))

    # Populate filesystem tree and format ext2
    with tempfile.TemporaryDirectory(prefix="xinim-boot-") as root_dir:
        populate_root_tree(
            root_dir,
            args.kernel,
            args.shell,
            args.grub_cfg,
            args.motd if args.motd else None,
            args.guest_bin,
        )
        format_ext2_partition(args.output, partition_sector_count, root_dir)

    # Install GRUB bootloader
    if not args.no_grub_install:
        install_grub(args.output)

    print(f"Boot disk image created: {args.output} ({args.size_mb} MiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
