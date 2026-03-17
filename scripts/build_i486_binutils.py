#!/usr/bin/env python3
"""
Cross-compile GNU binutils (as + ld) for XINIM i486 target.

Downloads binutils source if not present, configures for i386-elf target,
cross-compiles the assembler and linker.

Usage:
    python3 scripts/build_i486_binutils.py \
        --source-dir .scratch/binutils \
        --build-dir build/i486/Debug/binutils \
        --prefix /usr \
        --output-dir build/i486/Debug/binutils-install
"""

import argparse
import os
import subprocess
import tarfile
import urllib.request
from pathlib import Path

BINUTILS_VERSION = "2.42"
BINUTILS_URL = f"https://ftp.gnu.org/gnu/binutils/binutils-{BINUTILS_VERSION}.tar.xz"


def download_source(dest_dir: Path) -> None:
    tarball = dest_dir.parent / f"binutils-{BINUTILS_VERSION}.tar.xz"
    if not tarball.exists():
        print(f"Downloading binutils from {BINUTILS_URL}...")
        dest_dir.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(BINUTILS_URL, str(tarball))
    if not dest_dir.exists():
        print(f"Extracting to {dest_dir}...")
        import lzma
        with lzma.open(str(tarball)) as xz:
            with tarfile.open(fileobj=xz) as tf:
                tf.extractall(str(dest_dir.parent))
        extracted = dest_dir.parent / f"binutils-{BINUTILS_VERSION}"
        if extracted.exists() and extracted != dest_dir:
            extracted.rename(dest_dir)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--prefix", default="/usr")
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    src_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not src_dir.exists():
        download_source(src_dir)

    if not src_dir.exists():
        print(f"ERROR: source directory {src_dir} does not exist")
        return 1

    configure = src_dir / "configure"
    if not configure.exists():
        print(f"ERROR: {configure} not found")
        return 1

    # Configure for i386-elf cross target
    # Only build gas (assembler) and ld (linker)
    configure_cmd = [
        str(configure),
        f"--prefix={args.prefix}",
        "--target=i386-elf",
        "--disable-nls",
        "--disable-werror",
        "--disable-gdb",
        "--disable-gprof",
        "--enable-targets=i386-elf",
        "--disable-multilib",
    ]

    print("Configuring binutils for i386-elf...")
    subprocess.run(configure_cmd, cwd=str(build_dir), check=True,
                   env={**os.environ, "CFLAGS": "-Os -m32", "LDFLAGS": "-m32"})

    print("Building binutils (as + ld only)...")
    subprocess.run(["make", "-j4", "all-gas", "all-ld"],
                   cwd=str(build_dir), check=True)

    print(f"Installing to {output_dir}...")
    subprocess.run(["make", f"DESTDIR={output_dir}", "install-gas", "install-ld"],
                   cwd=str(build_dir), check=True)

    as_path = output_dir / args.prefix.lstrip("/") / "bin" / "i386-elf-as"
    ld_path = output_dir / args.prefix.lstrip("/") / "bin" / "i386-elf-ld"
    print(f"  as: {as_path}" if as_path.exists() else "  as: NOT FOUND")
    print(f"  ld: {ld_path}" if ld_path.exists() else "  ld: NOT FOUND")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
