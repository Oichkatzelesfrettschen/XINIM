#!/usr/bin/env python3
"""
Cross-compile NetBSD bmake for XINIM i486 target.

bmake is the BSD make implementation needed for pkgsrc.
Downloads source if not present, cross-compiles with dietlibc.

Usage:
    "$PYTHON" scripts/build_i486_bmake.py \
        --source-dir .scratch/bmake \
        --build-dir build/i486/Debug/bmake \
        --start-o <dietlibc start.o> \
        --dietlibc-a <dietlibc.a> \
        --output build/i486/Debug/bmake-i486
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

BMAKE_VERSION = "20240808"
BMAKE_URL = f"https://www.crufty.net/ftp/pub/sjg/old/bmake-{BMAKE_VERSION}.tar.gz"
BMAKE_SHA256 = "b59189251b483decd4492f1f74387b2a584c03d5aa4637cd48b38ec62b9c0848"


def write_compat_header(header_path: Path) -> None:
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(
        "#ifndef XINIM_BMAKE_COMPAT_H\n"
        "#define XINIM_BMAKE_COMPAT_H\n"
        "\n"
        "#include <sys/types.h>\n"
        "\n"
        "#ifndef XINIM_BMAKE_RANDOM_DEFINED\n"
        "#define XINIM_BMAKE_RANDOM_DEFINED 1\n"
        "static inline long xinim_bmake_random(void) { return 4; }\n"
        "static inline void xinim_bmake_srandom(unsigned int seed) { (void)seed; }\n"
        "#define random xinim_bmake_random\n"
        "#define srandom xinim_bmake_srandom\n"
        "#endif\n"
        "\n"
        "#endif\n",
        encoding="utf-8",
    )


def write_config_header(header_path: Path) -> None:
    """Write the target feature profile without running a host configure probe."""
    feature_macros = [
        "HAVE_AR_H",
        "HAVE_DIRENT_H",
        "HAVE_DIRNAME",
        "HAVE_FCNTL_H",
        "HAVE_FORK",
        "HAVE_GETCWD",
        "HAVE_GETENV",
        "HAVE_GETOPT",
        "HAVE_INTTYPES_H",
        "HAVE_KILLPG",
        "HAVE_LIBGEN_H",
        "HAVE_LIMITS_H",
        "HAVE_LONG_LONG_INT",
        "HAVE_POLL_H",
        "HAVE_REALPATH",
        "HAVE_SELECT",
        "HAVE_SETENV",
        "HAVE_SETPGID",
        "HAVE_SETRLIMIT",
        "HAVE_SETSID",
        "HAVE_SIGACTION",
        "HAVE_SIGADDSET",
        "HAVE_SIGPENDING",
        "HAVE_SIGPROCMASK",
        "HAVE_SIGSUSPEND",
        "HAVE_SNPRINTF",
        "HAVE_STDINT_H",
        "HAVE_STDIO_H",
        "HAVE_STDLIB_H",
        "HAVE_STRERROR",
        "HAVE_STRFTIME",
        "HAVE_STRING_H",
        "HAVE_STRLCPY",
        "HAVE_STRSEP",
        "HAVE_STRTOD",
        "HAVE_STRTOL",
        "HAVE_STRTOLL",
        "HAVE_STRTOUL",
        "HAVE_SYS_PARAM_H",
        "HAVE_SYS_SELECT_H",
        "HAVE_SYS_SOCKET_H",
        "HAVE_SYS_STAT_H",
        "HAVE_SYS_TIME_H",
        "HAVE_SYS_TYPES_H",
        "HAVE_SYS_WAIT_H",
        "HAVE_UNISTD_H",
        "HAVE_UNSIGNED_LONG_LONG_INT",
        "HAVE_UTIME_H",
        "HAVE_VPRINTF",
        "HAVE_VSNPRINTF",
        "HAVE_WAIT3",
        "HAVE_WAIT4",
        "HAVE_WAITPID",
        "HAVE___ATTRIBUTE__",
        "STDC_HEADERS",
    ]
    header_lines = [
        "/* Generated for the XINIM i486 bmake target; no host probe is used. */",
        "#ifndef XINIM_BMAKE_CONFIG_H",
        "#define XINIM_BMAKE_CONFIG_H",
        '#define DEFSHELL_CUSTOM "/bin/sh"',
        '#define DEFSHELL_INDEX 0',
        '#define DEFSHELL_PATH "/bin/sh"',
        "#define BMAKE_PATH_MAX 256",
    ]
    header_lines.extend(f"#define {macro} 1" for macro in feature_macros)
    header_lines.extend(["#endif", ""])
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text("\n".join(header_lines), encoding="utf-8")


def download_source(dest_dir: Path) -> None:
    tarball = dest_dir.parent / f"bmake-{BMAKE_VERSION}.tar.gz"
    if tarball.exists():
        digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
        if digest != BMAKE_SHA256:
            print(f"Discarding invalid bmake archive {tarball} (SHA-256 {digest})")
            tarball.unlink()
    if not tarball.exists():
        print(f"Downloading bmake from {BMAKE_URL}...")
        dest_dir.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            dir=dest_dir.parent, prefix=f".{tarball.name}.", delete=False
        ) as temporary_file:
            temporary_path = Path(temporary_file.name)
        try:
            with urllib.request.urlopen(BMAKE_URL, timeout=30) as response:
                with temporary_path.open("wb") as archive_file:
                    shutil.copyfileobj(response, archive_file)
            digest = hashlib.sha256(temporary_path.read_bytes()).hexdigest()
            if digest != BMAKE_SHA256:
                raise RuntimeError(
                    f"bmake archive SHA-256 mismatch: expected {BMAKE_SHA256}, got {digest}"
                )
            temporary_path.replace(tarball)
        finally:
            temporary_path.unlink(missing_ok=True)
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
    parser.add_argument("--cc", required=True)
    parser.add_argument("--ld", default="")
    parser.add_argument("--compiler-runtime", required=True)
    parser.add_argument("--compat-header", required=True)
    parser.add_argument("--config-header", default="")
    parser.add_argument("--start-o", required=True)
    parser.add_argument("--dietlibc-a", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    src_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)
    compat_header = Path(args.compat_header).resolve()
    write_compat_header(compat_header)
    config_header = (
        Path(args.config_header).resolve()
        if args.config_header
        else build_dir / "config.h"
    )
    write_config_header(config_header)

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

    compiler_arguments = shlex.split(args.cc)
    if not compiler_arguments:
        raise RuntimeError("empty C compiler command")
    cc_flags = compiler_arguments + [
        f"-I{build_dir}",
        f"-I{src_dir}",
        f"-I{dietlibc_include}",
        "-DHAVE_CONFIG_H",
        "-DBMAKE_PATH_MAX=256",
        "-DHAVE_STRTOL=1",
        "-DHAVE_STRTOUL=1",
        f"-include{compat_header}",
        "-std=gnu11",
        "-Os", "-fno-pie", "-fno-pic", "-fno-stack-protector", "-fno-builtin",
        "-Werror",
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
    print(f"bmake built: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
