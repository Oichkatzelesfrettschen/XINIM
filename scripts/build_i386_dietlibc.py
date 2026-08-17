#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess


VENDOR_CLANG_COMPAT_FLAGS = (
    "-Wno-unknown-attributes",
    "-Wno-deprecated-non-prototype",
    "-Wno-int-in-bool-context",
    "-Wno-null-pointer-subtraction",
    "-Wno-switch",
    "-fno-builtin-bcmp",
)


SYSNO_MAP = {
    "__NR_open": 3,
    "__NR_close": 4,
    "__NR_read": 5,
    "__NR_write": 6,
    "__NR_lseek": 7,
    "__NR_stat": 8,
    "__NR_fstat": 9,
    "__NR_access": 10,
    "__NR_dup": 11,
    "__NR_dup2": 12,
    "__NR_pipe": 13,
    "__NR_ioctl": 14,
    "__NR_fcntl": 15,
    "__NR_chdir": 18,
    "__NR_getcwd": 19,
    "__NR_exit": 25,
    "__NR_fork": 26,
    "__NR_execve": 27,
    "__NR_waitpid": 28,
    "__NR_wait4": 28,
    "__NR_getpid": 29,
    "__NR_getppid": 30,
    "__NR_kill": 31,
    "__NR_signal": 32,
    "__NR_sigaction": 33,
    "__NR_rt_sigaction": 33,
    "__NR_brk": 34,
    "__NR_time": 42,
    "__NR_gettimeofday": 43,
    "__NR_clock_gettime": 44,
    "__NR_nanosleep": 45,
    "__NR_getuid": 46,
    "__NR_geteuid": 47,
    "__NR_getgid": 48,
    "__NR_getegid": 49,
    "__NR_setuid": 50,
    "__NR_setgid": 51,
    "__NR_lstat": -1,
    "__NR_flock": -1,
    "__NR_ftruncate": -1,
    "__NR_mmap": -1,
    "__NR_munmap": -1,
    "__NR_readlink": -1,
    "__NR_getdents": -1,
    "__NR_setpgid": -1,
    "__NR_getpgrp": -1,
    "__NR_setsid": -1,
    "__NR_getsid": -1,
    "__NR_getrlimit": -1,
    "__NR_setrlimit": -1,
    "__NR_rt_sigprocmask": -1,
    "__NR_rt_sigsuspend": -1,
    "__NR_sigreturn": -1,
}


def patch_i386_syscalls(syscalls_path: Path) -> None:
    text = syscalls_path.read_text()
    if "__NR_debug_write" in text and "__NR_monotonic_ns" in text:
        return
    for name, value in SYSNO_MAP.items():
        pattern = rf"(^#define\s+{re.escape(name)}\s+)-?\d+"
        replacement = rf"\g<1>{value}"
        text, count = re.subn(pattern, replacement, text, flags=re.MULTILINE)
        if count == 0:
            continue
    syscalls_path.write_text(text)


def patch_dietfeatures(dietfeatures_path: Path) -> None:
    text = dietfeatures_path.read_text()
    text = text.replace("#define WANT_I386_SOCKETCALL\n", "")
    text = text.replace("#define WANT_LINKER_WARNINGS\n", "")
    text = text.replace("#define WANT_THREAD_SAFE\n", "")
    text = text.replace("#define WANT_TLS\n", "")
    text = text.replace("#define WANT_CTOR\n", "")
    text = text.replace("#define WANT_EXCEPTIONS\n", "")
    text = text.replace("#define WANT_SYSENTER\n", "")
    text = text.replace("#define WANT_SSP\n", "")
    dietfeatures_path.write_text(text)


def run(args: list[str], cwd: Path) -> None:
    subprocess.run(args, cwd=cwd, check=True)


def configure_vendor_binutils(
    makefile_path: Path,
    ar_path: Path,
    strip_path: Path,
    assembler_path: Path,
    preprocessor_path: Path,
) -> None:
    makefile_text = makefile_path.read_text()
    substitutions = {
        "$(CROSS)ar": "$(XINIM_AR)",
        "$(CROSS)strip": "$(XINIM_STRIP)",
        "$(CROSS)as": "$(XINIM_AS)",
        "$(CROSS)cpp": "$(XINIM_CPP)",
    }
    for source_token, replacement in substitutions.items():
        if source_token not in makefile_text:
            raise RuntimeError(
                f"dietlibc Makefile no longer exposes the expected {source_token} tool boundary"
            )
        makefile_text = makefile_text.replace(source_token, replacement)
    makefile_path.write_text(makefile_text)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--ar", required=True)
    parser.add_argument("--ranlib", required=True)
    parser.add_argument("--strip", required=True)
    parser.add_argument("--assembler", required=True)
    parser.add_argument("--preprocessor", required=True)
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parsed = parser.parse_args()

    source_dir = Path(parsed.source_dir).resolve()
    build_dir = Path(parsed.build_dir).resolve()
    tree_dir = build_dir / "tree"
    if tree_dir.exists():
        shutil.rmtree(tree_dir)
    tree_dir.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source_dir, tree_dir)

    ar_path = Path(parsed.ar).resolve()
    ranlib_path = Path(parsed.ranlib).resolve()
    strip_path = Path(parsed.strip).resolve()
    assembler_path = Path(parsed.assembler).resolve()
    preprocessor_path = Path(parsed.preprocessor).resolve()
    configure_vendor_binutils(
        tree_dir / "Makefile",
        ar_path,
        strip_path,
        assembler_path,
        preprocessor_path,
    )
    patch_i386_syscalls(tree_dir / "i386" / "syscalls.h")
    patch_dietfeatures(tree_dir / "dietfeatures.h")

    cc = shlex.split(parsed.cc)
    make_cmd = [
        "make",
        f"-j{parsed.jobs}",
        "ARCH=i386",
        f"CC={' '.join(cc)}",
        (
            "EXTRACFLAGS=-std=gnu11 -Werror "
            f"{' '.join(VENDOR_CLANG_COMPAT_FLAGS)} "
            "-fno-stack-protector -fno-pie -fno-pic"
        ),
        f"XINIM_AR={ar_path}",
        f"XINIM_STRIP={strip_path}",
        f"XINIM_AS={assembler_path}",
        f"XINIM_CPP={preprocessor_path}",
        "bin-i386/start.o",
        "bin-i386/dietlibc.a",
    ]
    run(make_cmd, tree_dir)

    # Compile and add libshell (fnmatch, glob) and libregex (rx) to archive
    extra_sources = []
    for subdir in ["libshell", "libregex"]:
        src_dir = tree_dir / subdir
        if not src_dir.exists():
            continue
        for src in sorted(src_dir.glob("*.c")):
            obj = tree_dir / "bin-i386" / f"{src.stem}.o"
            compile_cmd = cc + [
                "-I" + str(tree_dir),
                "-isystem", str(tree_dir / "include"),
                "-std=gnu11",
                "-pipe", "-nostdinc", "-D_REENTRANT",
                "-Werror", *VENDOR_CLANG_COMPAT_FLAGS,
                "-fno-stack-protector", "-fno-pie", "-fno-pic",
                "-O2", "-fomit-frame-pointer",
                "-c", str(src), "-o", str(obj),
                "-D__dietlibc__",
            ]
            run(compile_cmd, tree_dir)
            extra_sources.append(str(obj))

    if extra_sources:
        ar_cmd = [str(ar_path), "rcs", "bin-i386/dietlibc.a"] + extra_sources
        run(ar_cmd, tree_dir)

    run([str(ranlib_path), "bin-i386/dietlibc.a"], tree_dir)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
