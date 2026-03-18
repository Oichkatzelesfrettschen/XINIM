#!/usr/bin/env python3
"""Create a GRUB-bootable qcow2 disk image for the i486 QEMU lane.

Produces a single bootable disk image:
  1. Raw image: MBR gap + ext2 partition with kernel, shell, utilities
  2. GRUB boot.img (MBR) + core.img (gap between MBR and partition)
  3. Convert to qcow2 for efficient storage

The ext2 partition contains /boot/grub/grub.cfg, /boot/xinim (kernel),
/bin/* (all utilities), and /etc/* (configs).
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path
from typing import Iterable


PARTITION_START_LBA = 2048
SECTOR_SIZE = 512
EXT2_BLOCK_SIZE = 1024
# MBR gap = sectors 1..2047 = 2047 * 512 = ~1 MB for GRUB core image
MBR_GAP_BYTES = (PARTITION_START_LBA - 1) * SECTOR_SIZE


def build_mbr(total_sectors: int) -> bytes:
    """Build a minimal MBR with one bootable Linux partition."""
    mbr = bytearray(512)
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
    holdsvc_path: str | None,
    motd_path: str | None,
    guest_bins: Iterable[str],
) -> None:
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

    # Also install shell and holdsvc under /boot for Multiboot2 module loading
    shutil.copy2(shell_path, os.path.join(boot_dir, "mksh"))
    shutil.copy2(shell_path, os.path.join(boot_dir, "xash"))
    if holdsvc_path and os.path.isfile(holdsvc_path):
        shutil.copy2(holdsvc_path, os.path.join(boot_dir, "holdsvc"))

    # GRUB config -- kernel + essential modules loaded via multiboot2
    with open(os.path.join(grub_dir, "grub.cfg"), "w", encoding="utf-8") as f:
        f.write("set timeout=0\n")
        f.write("set default=0\n")
        f.write("set gfxpayload=text\n\n")
        f.write('menuentry "XINIM i486" {\n')
        f.write("    multiboot2 /boot/xinim\n")
        f.write("    module2 /boot/xash /bin/xash\n")
        f.write("    module2 /boot/mksh /bin/mksh\n")
        if os.path.isfile(os.path.join(boot_dir, "holdsvc")):
            f.write("    module2 /boot/holdsvc /bin/holdsvc\n")
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
            f.write("XINIM i486\n")

    install_guest_binaries(root_dir, guest_bins)


def format_ext2_partition(
    output_path: str, partition_sector_count: int, root_dir: str
) -> None:
    mke2fs = shutil.which("mke2fs")
    if mke2fs is None:
        raise SystemExit("mke2fs is required")

    partition_offset = PARTITION_START_LBA * SECTOR_SIZE
    ext2_block_count = partition_sector_count // (EXT2_BLOCK_SIZE // SECTOR_SIZE)

    subprocess.run(
        [
            mke2fs, "-F", "-q", "-t", "ext2",
            "-d", root_dir,
            "-b", str(EXT2_BLOCK_SIZE),
            "-L", "XINIMROOT",
            "-E", f"offset={partition_offset}",
            output_path,
            str(ext2_block_count),
        ],
        check=True,
    )


def install_grub_to_image(
    raw_image: str, grub_platform_dir: str
) -> bool:
    """Install GRUB boot.img + core.img into the raw disk image.

    boot.img goes in the first 440 bytes of the MBR (preserving partition table).
    core.img goes in the MBR gap (sector 1 to PARTITION_START_LBA-1).
    """
    boot_img_path = os.path.join(grub_platform_dir, "boot.img")
    if not os.path.isfile(boot_img_path):
        print(f"WARNING: {boot_img_path} not found, skipping GRUB install")
        return False

    # Find grub-mkimage
    grub_mkimage = shutil.which("grub-mkimage")
    if grub_mkimage is None:
        # Try repo-local
        for candidate in Path(grub_platform_dir).parent.parent.parent.glob("bin/grub-mkimage"):
            grub_mkimage = str(candidate)
            break
    if grub_mkimage is None:
        print("WARNING: grub-mkimage not found, skipping GRUB install")
        return False

    # Generate core.img with required modules
    with tempfile.NamedTemporaryFile(suffix=".img", delete=False) as core_tmp:
        core_img_path = core_tmp.name

    try:
        # Embed a minimal config that points to the partition
        with tempfile.NamedTemporaryFile(mode="w", suffix=".cfg", delete=False) as cfg_tmp:
            cfg_tmp.write("set root=(hd0,msdos1)\n")
            cfg_tmp.write("set prefix=(hd0,msdos1)/boot/grub\n")
            early_cfg = cfg_tmp.name

        subprocess.run(
            [
                grub_mkimage,
                "-O", "i386-pc",
                "-o", core_img_path,
                "-p", "(hd0,msdos1)/boot/grub",
                "-d", grub_platform_dir,
                "-c", early_cfg,
                # Modules needed to find and read the ext2 partition
                "biosdisk", "part_msdos", "ext2", "multiboot2",
                "normal", "search", "configfile", "boot",
            ],
            check=True,
            capture_output=True,
        )
        os.unlink(early_cfg)

        core_size = os.path.getsize(core_img_path)
        if core_size > MBR_GAP_BYTES:
            print(f"WARNING: core.img ({core_size} bytes) exceeds MBR gap ({MBR_GAP_BYTES} bytes)")
            os.unlink(core_img_path)
            return False

        # Read boot.img and core.img
        with open(boot_img_path, "rb") as f:
            boot_img = bytearray(f.read())
        with open(core_img_path, "rb") as f:
            core_img = f.read()
        os.unlink(core_img_path)

        # Patch boot.img: byte at offset 0x5C holds the sector of core.img (sector 1)
        # The boot.img jumps to sector stored at offset 0x5C (little-endian uint32)
        if len(boot_img) >= 0x60:
            struct.pack_into("<I", boot_img, 0x5C, 1)  # core.img starts at sector 1

        # Write to disk image
        with open(raw_image, "r+b") as f:
            # Write boot.img to first 440 bytes of MBR (preserve partition table + signature)
            f.seek(0)
            existing_mbr = bytearray(f.read(512))
            # Copy boot code (first 440 bytes), keep partition table + signature
            existing_mbr[0:440] = boot_img[0:440]
            f.seek(0)
            f.write(bytes(existing_mbr))

            # Write core.img starting at sector 1
            f.seek(SECTOR_SIZE)
            f.write(core_img)

        print(f"GRUB installed: boot.img=440B core.img={core_size}B")
        return True

    except subprocess.CalledProcessError as exc:
        print(f"WARNING: grub-mkimage failed: {exc.stderr.decode()}")
        if os.path.exists(core_img_path):
            os.unlink(core_img_path)
        return False


def convert_to_qcow2(raw_path: str, qcow2_path: str) -> bool:
    qemu_img = shutil.which("qemu-img")
    if qemu_img is None:
        print("WARNING: qemu-img not found, keeping raw image")
        return False
    subprocess.run(
        [qemu_img, "convert", "-f", "raw", "-O", "qcow2", raw_path, qcow2_path],
        check=True,
    )
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a GRUB-bootable qcow2 disk image for XINIM i486"
    )
    parser.add_argument("--output", required=True, help="Path to output image (qcow2)")
    parser.add_argument("--size-mb", type=int, default=32, help="Disk size in MiB")
    parser.add_argument("--kernel", required=True, help="Path to kernel binary")
    parser.add_argument("--shell", required=True, help="Path to shell binary (mksh)")
    parser.add_argument("--holdsvc", default="", help="Path to holdsvc binary")
    parser.add_argument("--motd", default="", help="Path to /etc/motd")
    parser.add_argument("--grub-platform-dir", default="",
                        help="Path to GRUB i386-pc platform directory")
    parser.add_argument("--guest-bin", action="append", default=[],
                        help="Guest binary: SOURCE:TARGET_PATH")
    parser.add_argument("--raw-only", action="store_true",
                        help="Output raw image instead of qcow2")
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

    # Determine output paths
    output = args.output
    if not args.raw_only and output.endswith(".qcow2"):
        raw_path = output.replace(".qcow2", ".raw")
    else:
        raw_path = output
    os.makedirs(os.path.dirname(os.path.abspath(raw_path)), exist_ok=True)

    # Create raw disk image
    with open(raw_path, "wb") as f:
        f.truncate(total_bytes)

    # Write MBR partition table
    with open(raw_path, "r+b") as f:
        f.seek(0)
        f.write(build_mbr(total_sectors))

    # Populate filesystem and format ext2
    with tempfile.TemporaryDirectory(prefix="xinim-boot-") as root_dir:
        populate_root_tree(
            root_dir, args.kernel, args.shell,
            args.holdsvc if args.holdsvc else None,
            args.motd if args.motd else None,
            args.guest_bin,
        )
        format_ext2_partition(raw_path, partition_sector_count, root_dir)

    # Install GRUB bootloader
    grub_dir = args.grub_platform_dir
    if not grub_dir:
        # Auto-detect
        for candidate in [
            "/usr/lib/grub/i386-pc",
            "/usr/share/grub/i386-pc",
        ]:
            if os.path.isdir(candidate):
                grub_dir = candidate
                break
    if grub_dir:
        install_grub_to_image(raw_path, grub_dir)
    else:
        print("WARNING: no GRUB i386-pc platform found, disk not bootable")

    # Convert to qcow2
    if not args.raw_only and output.endswith(".qcow2"):
        if convert_to_qcow2(raw_path, output):
            os.unlink(raw_path)
            print(f"qcow2 boot disk: {output} ({args.size_mb} MiB)")
        else:
            print(f"Raw boot disk: {raw_path} ({args.size_mb} MiB)")
    else:
        print(f"Raw boot disk: {raw_path} ({args.size_mb} MiB)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
