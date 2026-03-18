#!/usr/bin/env python3
"""
Cross-compile NetBSD bmake for XINIM i486 target.

bmake is the BSD make implementation needed for pkgsrc.
Downloads source if not present, cross-compiles with dietlibc.

Usage:
    python3 scripts/build_i486_bmake.py \
        --source-dir .scratch/bmake \
        --build-dir build/i486/Debug/bmake \
        --start-o <dietlibc start.o> \
        --dietlibc-a <dietlibc.a> \
        --output build/i486/Debug/bmake-i486
"""

import argparse
import os
import shutil
import subprocess
import tarfile
import urllib.request
from pathlib import Path

BMAKE_VERSION = "20240808"
BMAKE_URL = f"https://www.crufty.net/ftp/pub/sjg/bmake-{BMAKE_VERSION}.tar.gz"


def download_source(dest_dir: Path) -> None:
    tarball = dest_dir.parent / f"bmake-{BMAKE_VERSION}.tar.gz"
    if not tarball.exists():
        print(f"Downloading bmake from {BMAKE_URL}...")
        dest_dir.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(BMAKE_URL, str(tarball))
    if not dest_dir.exists():
        print(f"Extracting to {dest_dir}...")
        with tarfile.open(str(tarball), "r:gz") as tf:
            tf.extractall(str(dest_dir.parent))
        # bmake extracts to bmake/ subdirectory
        extracted = dest_dir.parent / "bmake"
        if extracted.exists() and extracted != dest_dir:
            extracted.rename(dest_dir)


def find_sources(src_dir: Path) -> list[str]:
    """Find the core bmake C sources."""
    core_sources = [
        "arch.c", "buf.c", "compat.c", "cond.c", "dir.c", "enum.c",
        "for.c", "hash.c", "job.c", "lst.c", "main.c", "make.c",
        "make_malloc.c", "metachar.c", "parse.c", "str.c", "suff.c",
        "targ.c", "trace.c", "var.c", "util.c",
    ]
    found = []
    for name in core_sources:
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

    # Download if needed
    if not src_dir.exists():
        download_source(src_dir)

    if not src_dir.exists():
        print(f"ERROR: source directory {src_dir} does not exist")
        return 1

    sources = find_sources(src_dir)
    if not sources:
        print(f"ERROR: no bmake sources found in {src_dir}")
        return 1

    dietlibc_include = Path(args.start_o).resolve().parent.parent / "include"

    cc_flags = [
        "gcc", "-m32", "-march=i486", "-mtune=i486",
        "-mno-mmx", "-mno-sse",
        f"-I{src_dir}",
        f"-I{dietlibc_include}",
        "-DHAVE_CONFIG_H",
        f"-DBMAKE_PATH_MAX=256",
        f"-include{src_dir}/xinim_compat.h",
        "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector",
        "-w",
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
    linker_script = src_dir.parent.parent / "linker_xash_i486_user.ld"
    link_cmd = [
        "gcc", "-m32", "-nostdlib", "-static", "-no-pie",
        "-Wl,--build-id=none",
    ]
    if linker_script.exists():
        link_cmd += [f"-T{linker_script}"]
    link_cmd += ["-o", str(output)]
    link_cmd += object_files + [
        str(Path(args.start_o).resolve()),
        str(Path(args.dietlibc_a).resolve()),
        "-lgcc",
    ]
    print(f"  LINK {output.name}")
    subprocess.run(link_cmd, cwd=str(build_dir), check=True)
    print(f"bmake built: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
