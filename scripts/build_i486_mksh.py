#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess


GENERATED_OPTION_FILES = ("rlimits.opt", "sh_flags.opt", "ulimits.opt")


MKSH_OVERRIDES = {
    "HAVE_FLOCK": "1",
    "HAVE_FTRUNCATE": "1",
    "HAVE_GETSID": "1",
    "HAVE_KILLPG": "0",
    "HAVE_LANGINFO_CODESET": "0",
    "HAVE_LOCK_FCNTL": "0",
    "HAVE_MMAP": "0",
    "HAVE_NICE": "0",
    "HAVE_PERSISTENT_HISTORY": "0",
    "HAVE_SIG_T": "1",
    "HAVE_SELECT": "1",
    "HAVE_SETGROUPS": "0",
    "HAVE_SETLOCALE_CTYPE": "0",
    "HAVE_SETRESUGID": "0",
    "HAVE_ST_MTIM": "0",
    "HAVE_ULIMIT_H": "0",
    "HAVE_VALUES_H": "0",
}

MKSH_EXTRA_DEFINES = [
    "MKSH_ASSUME_UTF8",
    "MKSH_BUILDSH",
    "MKSH_NO_CMDLINE_EDITING",
    "MKSH_NO_PWD",
    "MKSH__NO_SYMLINK",
    "__dietlibc__",
]


def run(
    args: list[str],
    cwd: Path,
    environment: dict[str, str] | None = None,
) -> None:
    subprocess.run(args, cwd=cwd, env=environment, check=True)


def run_logged(
    args: list[str], cwd: Path, log_path: Path, environment: dict[str, str]
) -> None:
    with log_path.open("wb") as log_file:
        result = subprocess.run(
            args,
            cwd=cwd,
            env=environment,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            check=False,
        )
    if result.returncode != 0:
        log_lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        raise RuntimeError(
            "mksh feature configuration failed:\n" + "\n".join(log_lines[-80:])
        )


def generate_makefrag(
    source_dir: Path, configure_dir: Path, compiler: str
) -> Path:
    configure_dir.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment.update(
        {
            "CC": compiler,
            "CFLAGS": "-O2 -Wall -Wextra -Werror",
            "CPPFLAGS": "-DMKSH_BINSHPOSIX",
            "LC_ALL": "C",
            "TARGET_OS": "Linux",
        }
    )
    run_logged(
        ["sh", str(source_dir / "Build.sh"), "-M"],
        configure_dir,
        configure_dir / "configure.log",
        environment,
    )
    for option_file in GENERATED_OPTION_FILES:
        option_environment = environment.copy()
        option_environment.update(
            {
                "BUILDSH_RUN_GENOPT": "1",
                "srcfile": str(source_dir / option_file),
            }
        )
        run(["sh", str(source_dir / "Build.sh")], configure_dir, option_environment)
    return configure_dir / "Makefrag.inc"


def patch_source_for_i486(src_path: Path, dest_path: Path, cppflags: list[str]) -> Path:
    text = src_path.read_text()
    if src_path.name == "misc.c" and "-DMKSH__NO_SYMLINK" in cppflags:
        text = text.replace(
            "XString xs;\n\tsize_t pos, len;\n\tint llen;\n",
            "XString xs;\n\tsize_t len;\n\tint llen;\n#ifndef MKSH__NO_SYMLINK\n\tsize_t pos;\n#endif\n",
        )
        text = text.replace(
            "\t/* max. recursion depth */\n\tint symlinks = 32;\n",
            "\t/* max. recursion depth */\n#ifndef MKSH__NO_SYMLINK\n\tint symlinks = 32;\n#endif\n",
        )
        text = text.replace(
            "\t\t/* store output position away, then append slash to output */\n\t\tpos = Xsavepos(xs, xp);\n",
            "\t\t/* store output position away, then append slash to output */\n#ifndef MKSH__NO_SYMLINK\n\t\tpos = Xsavepos(xs, xp);\n#endif\n",
        )
    if text == src_path.read_text():
        return src_path
    dest_path.write_text(text)
    return dest_path


def parse_makefrag(makefrag: Path) -> tuple[list[str], list[str]]:
    srcs: list[str] = []
    cppflags: list[str] = []
    for raw_line in makefrag.read_text().splitlines():
        line = raw_line.strip()
        if line.startswith("SRCS="):
            srcs = line.split("=", 1)[1].split()
        elif line.startswith("CPPFLAGS="):
            cppflags = shlex.split(line.split("=", 1)[1].strip())
    if not srcs:
        raise RuntimeError(f"could not parse SRCS from {makefrag}")
    if not cppflags:
        raise RuntimeError(f"could not parse CPPFLAGS from {makefrag}")
    return srcs, cppflags


def filtered_cppflags(cppflags: list[str]) -> list[str]:
    result: list[str] = []
    skip_next = False
    for flag in cppflags:
        if skip_next:
            skip_next = False
            continue
        if flag == "-I":
            skip_next = True
            continue
        if flag.startswith("-I"):
            continue
        if not flag.startswith("-D"):
            result.append(flag)
            continue
        name = flag[2:].split("=", 1)[0]
        if name in MKSH_OVERRIDES or name in MKSH_EXTRA_DEFINES:
            continue
        result.append(flag)
    result.extend(f"-D{name}={value}" for name, value in sorted(MKSH_OVERRIDES.items()))
    result.extend(f"-D{name}" for name in MKSH_EXTRA_DEFINES)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--ld", default="")
    parser.add_argument("--compiler-runtime", required=True)
    parser.add_argument("--start-o", required=True)
    parser.add_argument("--dietlibc-a", required=True)
    parser.add_argument("--linker-script", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--mode", choices=("mksh", "lksh"), default="mksh")
    args = parser.parse_args()

    source_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    configure_dir = build_dir / "configure"
    object_dir = build_dir / "objects"
    for generated_dir in (configure_dir, object_dir):
        if generated_dir.exists():
            shutil.rmtree(generated_dir)
    object_dir.mkdir(parents=True, exist_ok=True)

    cc = shlex.split(args.cc)
    if not cc:
        raise RuntimeError("empty C compiler command")
    makefrag_path = generate_makefrag(source_dir, configure_dir, cc[0])
    srcs, base_cppflags = parse_makefrag(makefrag_path)
    cppflags = filtered_cppflags(base_cppflags)

    common_flags = [
        "-std=gnu11",
        "-Os",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-fno-asynchronous-unwind-tables",
        "-fno-pie",
        "-fno-pic",
        "-fno-stack-protector",
        "-fno-strict-aliasing",
        "-fwrapv",
        "-nostdinc",
        "-I",
        str(configure_dir),
        "-I",
        str(source_dir),
        "-isystem",
        str(Path(args.start_o).resolve().parent.parent / "include"),
    ]

    object_files: list[str] = []
    for src in srcs:
        src_path = source_dir / src
        patched_src_path = build_dir / src
        patched_src_path.parent.mkdir(parents=True, exist_ok=True)
        compile_src = patch_source_for_i486(src_path, patched_src_path, cppflags)
        obj_path = object_dir / (Path(src).stem + ".o")
        command = cc + common_flags + cppflags + ["-c", str(compile_src), "-o", str(obj_path)]
        run(command, object_dir)
        object_files.append(str(obj_path))

    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    link_inputs = object_files + [
        str(Path(args.start_o).resolve()),
        str(Path(args.dietlibc_a).resolve()),
        str(Path(args.compiler_runtime).resolve()),
    ]
    if args.ld:
        link_command = shlex.split(args.ld) + [
            "-m",
            "elf_i386",
            "-T",
            str(Path(args.linker_script).resolve()),
            "-o",
            str(output),
        ] + link_inputs
    else:
        link_command = cc + [
            "-nostdlib",
            "-static",
            "-Wl,-no-pie",
            "-Wl,-T," + str(Path(args.linker_script).resolve()),
            "-o",
            str(output),
        ] + link_inputs
    run(link_command, build_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
