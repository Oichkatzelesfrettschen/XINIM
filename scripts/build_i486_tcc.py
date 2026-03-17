#!/usr/bin/env python3
"""
Cross-compile TCC (Tiny C Compiler) for XINIM i486 target.

TCC is a small (~100KB) C compiler that generates i386 ELF.
It can compile C89/C99 and is self-hosting.

Usage:
    python3 scripts/build_i486_tcc.py \
        --source-dir .scratch/tcc \
        --build-dir build/i486/Debug/tcc \
        --start-o <dietlibc start.o> \
        --dietlibc-a <dietlibc.a> \
        --output build/i486/Debug/tcc-i486
"""

import argparse
import os
import subprocess
import tarfile
import urllib.request
from pathlib import Path

TCC_VERSION = "0.9.27"
TCC_URL = f"https://download.savannah.gnu.org/releases/tinycc/tcc-{TCC_VERSION}.tar.bz2"


def download_source(dest_dir: Path) -> None:
    tarball = dest_dir.parent / f"tcc-{TCC_VERSION}.tar.bz2"
    if not tarball.exists():
        print(f"Downloading TCC from {TCC_URL}...")
        dest_dir.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(TCC_URL, str(tarball))
    if not dest_dir.exists():
        print(f"Extracting to {dest_dir}...")
        with tarfile.open(str(tarball), "r:bz2") as tf:
            tf.extractall(str(dest_dir.parent))
        extracted = dest_dir.parent / f"tcc-{TCC_VERSION}"
        if extracted.exists() and extracted != dest_dir:
            extracted.rename(dest_dir)


def find_sources(src_dir: Path) -> list[str]:
    """Find the core TCC sources for i386 target."""
    core = [
        "libtcc.c", "tccpp.c", "tccgen.c", "tccelf.c", "tccasm.c",
        "tccrun.c", "tcc.c", "i386-gen.c", "i386-link.c", "i386-asm.c",
    ]
    found = []
    for name in core:
        path = src_dir / name
        if path.exists():
            found.append(str(path))
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--start-o", required=True)
    parser.add_argument("--dietlibc-a", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    src_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    if not src_dir.exists():
        download_source(src_dir)

    if not src_dir.exists():
        print(f"ERROR: source directory {src_dir} does not exist")
        return 1

    sources = find_sources(src_dir)
    if not sources:
        print(f"ERROR: no TCC sources found in {src_dir}")
        return 1

    dietlibc_include = Path(args.start_o).resolve().parent.parent / "include"

    cc_flags = [
        "gcc", "-m32", "-march=i486", "-mtune=i486",
        "-mno-mmx", "-mno-sse",
        f"-I{src_dir}",
        f"-I{dietlibc_include}",
        "-DONE_SOURCE=0",
        "-DTCC_TARGET_I386",
        "-DCONFIG_TCC_STATIC",
        f'-DCONFIG_TCCDIR="/usr/lib/tcc"',
        "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector",
        "-Wno-error",
    ]

    object_files = []
    for src in sources:
        obj = build_dir / (Path(src).stem + ".o")
        cmd = cc_flags + ["-c", src, "-o", str(obj)]
        print(f"  CC {Path(src).name}")
        subprocess.run(cmd, cwd=str(build_dir), check=True)
        object_files.append(str(obj))

    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    link_cmd = [
        "gcc", "-m32", "-nostdlib", "-static", "-no-pie",
        "-o", str(output),
    ] + object_files + [
        str(Path(args.start_o).resolve()),
        str(Path(args.dietlibc_a).resolve()),
        "-lgcc",
    ]
    print(f"  LINK {output.name}")
    subprocess.run(link_cmd, cwd=str(build_dir), check=True)
    print(f"TCC built: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
