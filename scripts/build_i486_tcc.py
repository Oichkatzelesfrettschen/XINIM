#!/usr/bin/env python3
"""
Cross-compile TCC (Tiny C Compiler) for XINIM i486 target.

TCC is a small (~100KB) C compiler that generates i386 ELF.
It can compile C89/C99 and is self-hosting.

Usage:
    "$PYTHON" scripts/build_i486_tcc.py \
        --source-dir .scratch/tcc \
        --build-dir build/i486/Debug/tcc \
        --start-o <dietlibc start.o> \
        --dietlibc-a <dietlibc.a> \
        --output build/i486/Debug/tcc-i486
"""

import argparse
import hashlib
import shlex
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
from pathlib import Path

TCC_VERSION = "0.9.27"
TCC_URL = f"https://download.savannah.gnu.org/releases/tinycc/tcc-{TCC_VERSION}.tar.bz2"
TCC_SHA256 = "de23af78fca90ce32dff2dd45b3432b2334740bb9bb7b05bf60fdbfc396ceb9c"


def write_stubs_source(stubs_path: Path) -> None:
    stubs_path.parent.mkdir(parents=True, exist_ok=True)
    stubs_path.write_text(
        'extern "C" double ldexp(double value, int exponent) noexcept {\n'
        "    while (exponent > 0) { value *= 2.0; --exponent; }\n"
        "    while (exponent < 0) { value *= 0.5; ++exponent; }\n"
        "    return value;\n"
        "}\n",
        encoding="utf-8",
    )


def write_config_header(header_path: Path) -> None:
    """Write the cross target configuration without running a host probe."""
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(
        "/* Generated for the XINIM i486 TCC target. */\n"
        "#ifndef XINIM_TCC_CONFIG_H\n"
        "#define XINIM_TCC_CONFIG_H\n"
        '#define CONFIG_SYSROOT ""\n'
        '#define CONFIG_TCCDIR "/usr/lib/tcc"\n'
        '#define CONFIG_LDDIR "lib"\n'
        '#define CONFIG_TCC_CRTPREFIX "/usr/lib"\n'
        '#define CONFIG_TCC_SYSINCLUDEPATHS "/usr/lib/tcc/include:/usr/include"\n'
        '#define CONFIG_TCC_LIBPATHS "/usr/lib/tcc/lib:/lib:/usr/lib"\n'
        '#define CONFIG_TRIPLET ""\n'
        "#define CONFIG_TCC_STATIC 1\n"
        "#define CONFIG_TCCBOOT 1\n"
        '#define TCC_VERSION "0.9.27"\n'
        "#endif\n",
        encoding="utf-8",
    )


def download_source(dest_dir: Path) -> None:
    tarball = dest_dir.parent / f"tcc-{TCC_VERSION}.tar.bz2"
    if tarball.exists():
        digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
        if digest != TCC_SHA256:
            print(f"Discarding invalid TCC archive {tarball} (SHA-256 {digest})")
            tarball.unlink()
    if not tarball.exists():
        print(f"Downloading TCC from {TCC_URL}...")
        dest_dir.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            dir=dest_dir.parent, prefix=f".{tarball.name}.", delete=False
        ) as temporary_file:
            temporary_path = Path(temporary_file.name)
        try:
            with urllib.request.urlopen(TCC_URL, timeout=30) as response:
                with temporary_path.open("wb") as archive_file:
                    shutil.copyfileobj(response, archive_file)
            digest = hashlib.sha256(temporary_path.read_bytes()).hexdigest()
            if digest != TCC_SHA256:
                raise RuntimeError(
                    f"TCC archive SHA-256 mismatch: expected {TCC_SHA256}, got {digest}"
                )
            temporary_path.replace(tarball)
        finally:
            temporary_path.unlink(missing_ok=True)
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
        "tcc.c", "tccrun.c", "i386-gen.c", "i386-link.c", "i386-asm.c",
    ]
    found = []
    for name in core:
        path = src_dir / name
        if not path.is_file():
            raise RuntimeError(f"Required TCC source is missing: {path}")
        found.append(str(path))
    return found


def build_runtime(
    runtime_dir: Path,
    build_dir: Path,
    src_dir: Path,
    start_o: Path,
    dietlibc_a: Path,
    dietlibc_include: Path,
    compiler_arguments: list[str],
) -> None:
    runtime_dir.mkdir(parents=True, exist_ok=True)
    (runtime_dir / "tcc").mkdir(parents=True, exist_ok=True)

    # The guest compiler uses the same argc/argv startup and libc provider as
    # the compiler executable. TCC names its executable startup object crt1.o.
    shutil.copy2(start_o, runtime_dir / "crt1.o")
    shutil.copy2(dietlibc_a, runtime_dir / "libc.a")

    empty_asm = build_dir / "empty-crt.S"
    empty_asm.write_text(".section .text\n", encoding="utf-8")
    subprocess.run(
        compiler_arguments + ["-c", str(empty_asm), "-o", str(runtime_dir / "crti.o")],
        cwd=str(build_dir),
        check=True,
    )
    shutil.copy2(runtime_dir / "crti.o", runtime_dir / "crtn.o")

    libtcc1_c = src_dir / "lib" / "libtcc1.c"
    libtcc1_o = build_dir / "libtcc1.o"
    if libtcc1_c.exists():
        subprocess.run(
            compiler_arguments
            + [
                "-std=gnu11",
                f"-I{src_dir}",
                f"-I{dietlibc_include}",
                "-Wno-invalid-gnu-asm-cast",
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
        raise RuntimeError(f"Required TCC runtime source is missing: {libtcc1_c}")

    (runtime_dir / ".xinim-tcc-runtime-ready").write_text("ready\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--ld", default="")
    parser.add_argument("--compiler-runtime", required=True)
    parser.add_argument("--stubs-source", required=True)
    parser.add_argument("--config-header", default="")
    parser.add_argument("--start-o", required=True)
    parser.add_argument("--dietlibc-a", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--runtime-dir", default="",
                        help="Directory for crt objects and libraries used by in-guest TCC")
    args = parser.parse_args()

    src_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)
    stubs_path = Path(args.stubs_source).resolve()
    if stubs_path.suffix != ".cpp":
        raise RuntimeError("The XINIM TCC math adapter requires a .cpp source path")
    write_stubs_source(stubs_path)
    config_header = (
        Path(args.config_header).resolve()
        if args.config_header
        else build_dir / "config.h"
    )
    write_config_header(config_header)

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

    compiler_arguments = shlex.split(args.cc)
    if not compiler_arguments:
        raise RuntimeError("empty C compiler command")
    cc_flags = compiler_arguments + [
        f"-I{build_dir}",
        f"-I{src_dir}",
        f"-I{dietlibc_include}",
        "-DONE_SOURCE=0",
        "-DTCC_TARGET_I386",
        "-std=gnu11",
        "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector", "-fno-builtin",
        "-Werror", "-Wno-unused-result", "-Wno-string-plus-int", "-Wno-pointer-sign",
    ]

    object_files = []
    for src in sources:
        obj = build_dir / (Path(src).stem + ".o")
        cmd = cc_flags + ["-c", src, "-o", str(obj)]
        print(f"  CC {Path(src).name}")
        subprocess.run(cmd, cwd=str(build_dir), check=True)
        object_files.append(str(obj))

    adapter_object = build_dir / (stubs_path.stem + ".o")
    cxx_arguments = shlex.split(args.cxx)
    if not cxx_arguments:
        raise RuntimeError("empty C++ compiler command")
    subprocess.run(
        cxx_arguments + [
            "-std=c++23", "-Wall", "-Wextra", "-Werror", "-Os",
            "-fno-pie", "-fno-pic", "-fno-stack-protector", "-fno-builtin",
            "-fno-exceptions", "-fno-rtti",
            "-c", str(stubs_path), "-o", str(adapter_object),
        ],
        cwd=str(build_dir), check=True,
    )
    object_files.append(str(adapter_object))

    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    linker_script = src_dir.parent.parent / "linker_xash_i486_user.ld"
    link_inputs = object_files + [
        str(Path(args.start_o).resolve()),
        str(Path(args.dietlibc_a).resolve()),
        str(Path(args.compiler_runtime).resolve()),
    ]
    if args.ld:
        link_cmd = shlex.split(args.ld) + ["-m", "elf_i386"]
        if linker_script.exists():
            link_cmd += ["-T", str(linker_script)]
        link_cmd += ["-o", str(output)] + link_inputs
    else:
        link_cmd = compiler_arguments + [
            "-nostdlib", "-static", "-Wl,-no-pie", "-Wl,--build-id=none",
        ]
        if linker_script.exists():
            link_cmd += [f"-T{linker_script}"]
        link_cmd += ["-o", str(output)] + link_inputs
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
            compiler_arguments,
        )
    print(f"TCC built: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
