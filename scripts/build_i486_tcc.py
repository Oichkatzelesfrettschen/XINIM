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
import shutil
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
        "tcc.c", "i386-gen.c", "i386-link.c", "i386-asm.c",
        "xinim_stubs.c",
    ]
    found = []
    for name in core:
        path = src_dir / name
        if path.exists():
            found.append(str(path))
    return found


def build_runtime(runtime_dir: Path, build_dir: Path, src_dir: Path,
                  start_o: Path, dietlibc_a: Path, dietlibc_include: Path) -> None:
    runtime_dir.mkdir(parents=True, exist_ok=True)
    (runtime_dir / "tcc").mkdir(parents=True, exist_ok=True)

    crt1_asm = build_dir / "xinim-tcc-crt1.S"
    crt1_asm.write_text(
        ".globl _start\n"
        "_start:\n"
        "    xorl %ebp, %ebp\n"
        "    call main\n"
        "    movl %eax, %ebx\n"
        "    movl $25, %eax\n"
        "    int $0x80\n"
        "1:  jmp 1b\n",
        encoding="utf-8",
    )
    subprocess.run(
        [
            "gcc", "-m32", "-march=i486", "-mtune=i486",
            "-mno-mmx", "-mno-sse",
            "-c", str(crt1_asm), "-o", str(runtime_dir / "crt1.o"),
        ],
        cwd=str(build_dir),
        check=True,
    )

    empty_asm = build_dir / "empty-crt.S"
    empty_asm.write_text(".section .text\n", encoding="utf-8")
    subprocess.run(
        [
            "gcc", "-m32", "-march=i486", "-mtune=i486",
            "-mno-mmx", "-mno-sse",
            "-c", str(empty_asm), "-o", str(runtime_dir / "crti.o"),
        ],
        cwd=str(build_dir),
        check=True,
    )
    shutil.copy2(runtime_dir / "crti.o", runtime_dir / "crtn.o")

    libc_stub_c = build_dir / "xinim-tcc-libc-stubs.c"
    libc_stub_c.write_text(
        "int errno;\n"
        "int *__errno_location(void) { return &errno; }\n"
        "void exit(int status) {\n"
        "    __asm__ __volatile__(\"int $0x80\" : : \"a\"(25), \"b\"(status));\n"
        "    for (;;) {}\n"
        "}\n",
        encoding="utf-8",
    )
    libc_stub_o = build_dir / "xinim-tcc-libc-stubs.o"
    subprocess.run(
        [
            "gcc", "-m32", "-march=i486", "-mtune=i486",
            "-mno-mmx", "-mno-sse",
            "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector", "-fno-builtin",
            "-c", str(libc_stub_c), "-o", str(libc_stub_o),
        ],
        cwd=str(build_dir),
        check=True,
    )
    subprocess.run(
        ["ar", "rcs", str(runtime_dir / "libc.a"), str(libc_stub_o)],
        cwd=str(build_dir),
        check=True,
    )

    libtcc1_c = src_dir / "lib" / "libtcc1.c"
    libtcc1_o = build_dir / "libtcc1.o"
    if libtcc1_c.exists():
        subprocess.run(
            [
                "gcc", "-m32", "-march=i486", "-mtune=i486",
                "-mno-mmx", "-mno-sse",
                f"-I{src_dir}",
                f"-I{dietlibc_include}",
                "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector", "-fno-builtin",
                "-c", str(libtcc1_c), "-o", str(libtcc1_o),
            ],
            cwd=str(build_dir),
            check=True,
        )
        subprocess.run(
            ["ar", "rcs", str(runtime_dir / "tcc" / "libtcc1.a"), str(libtcc1_o)],
            cwd=str(build_dir),
            check=True,
        )
    else:
        subprocess.run(
            ["ar", "rcs", str(runtime_dir / "tcc" / "libtcc1.a")],
            cwd=str(build_dir),
            check=True,
        )

    (runtime_dir / ".xinim-tcc-runtime-ready").write_text("ready\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--start-o", required=True)
    parser.add_argument("--dietlibc-a", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--runtime-dir", default="",
                        help="Directory for crt objects and libraries used by in-guest TCC")
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
        "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector", "-fno-builtin",
        "-Werror", "-Wno-unused-result",
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
    if args.runtime_dir:
        print(f"  RUNTIME {args.runtime_dir}")
        build_runtime(
            Path(args.runtime_dir).resolve(),
            build_dir,
            src_dir,
            Path(args.start_o).resolve(),
            Path(args.dietlibc_a).resolve(),
            dietlibc_include,
        )
    print(f"TCC built: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
