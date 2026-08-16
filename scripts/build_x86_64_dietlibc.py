#!/usr/bin/env python3

import argparse
import os
import re
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(arguments: list[str], working_directory: Path) -> None:
    subprocess.run(arguments, cwd=working_directory, check=True)


def configure_features(features_path: Path) -> None:
    feature_text = features_path.read_text()
    disabled_features = (
        "WANT_LINKER_WARNINGS",
        "WANT_THREAD_SAFE",
        "WANT_TLS",
        "WANT_CTOR",
        "WANT_EXCEPTIONS",
        "WANT_SSP",
    )
    for feature_name in disabled_features:
        feature_text = feature_text.replace(f"#define {feature_name}\n", "")
    full_posix_marker = "/* #define WANT_FULL_POSIX_COMPAT */"
    if full_posix_marker not in feature_text:
        raise RuntimeError(
            f"cannot enable WANT_FULL_POSIX_COMPAT in {features_path}: "
            "expected disabled marker is missing"
        )
    feature_text = feature_text.replace(
        full_posix_marker, "#define WANT_FULL_POSIX_COMPAT", 1
    )
    features_path.write_text(feature_text)


def read_exec_argument_limit(exec_limits_path: Path) -> int:
    limits_text = exec_limits_path.read_text()
    match = re.search(
        r"^#define XINIM_EXEC_ARGUMENT_ENVIRONMENT_BYTES ([1-9][0-9]*)U$",
        limits_text,
        flags=re.MULTILINE,
    )
    if match is None:
        raise RuntimeError(
            f"cannot read XINIM exec argument limit from {exec_limits_path}"
        )
    return int(match.group(1))


def configure_exec_argument_limit(limits_path: Path, argument_limit: int) -> None:
    limits_text = limits_path.read_text()
    pattern = r"^#define ARG_MAX[ \t]+[0-9]+[ \t]+/\* # bytes of args \+ environ for exec\(\) \*/$"
    replacement = (
        f"#define ARG_MAX\t\t{argument_limit}"
        "\t/* # bytes of args + environ for exec() */"
    )
    limits_text, replacement_count = re.subn(
        pattern, replacement, limits_text, count=1, flags=re.MULTILINE
    )
    if replacement_count != 1:
        raise RuntimeError(f"cannot configure ARG_MAX in {limits_path}")
    limits_path.write_text(limits_text)


def self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="xinim-dietlibc-limit-") as directory:
        test_root = Path(directory)
        exec_limits_path = test_root / "exec_limits.h"
        exec_limits_path.write_text(
            "#define XINIM_EXEC_ARGUMENT_ENVIRONMENT_BYTES 64953U\n"
        )
        limits_path = test_root / "limits.h"
        limits_path.write_text(
            "#define ARG_MAX\t\t131072\t/* # bytes of args + environ for exec() */\n"
        )
        argument_limit = read_exec_argument_limit(exec_limits_path)
        if argument_limit != 64953:
            raise RuntimeError("exec argument limit parser returned the wrong value")
        configure_exec_argument_limit(limits_path, argument_limit)
        if "#define ARG_MAX\t\t64953\t" not in limits_path.read_text():
            raise RuntimeError("dietlibc ARG_MAX was not configured")

        limits_path.write_text("#define ARG_MAX unknown\n")
        try:
            configure_exec_argument_limit(limits_path, argument_limit)
        except RuntimeError:
            pass
        else:
            raise RuntimeError("unexpected dietlibc limits.h shape was accepted")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir")
    parser.add_argument("--build-dir")
    parser.add_argument("--cc")
    parser.add_argument("--exec-limits-header")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    if arguments.self_test:
        self_test()
        return 0
    required_arguments = {
        "--source-dir": arguments.source_dir,
        "--build-dir": arguments.build_dir,
        "--cc": arguments.cc,
        "--exec-limits-header": arguments.exec_limits_header,
    }
    missing = [name for name, value in required_arguments.items() if value is None]
    if missing:
        parser.error(f"the following arguments are required: {', '.join(missing)}")

    source_directory = Path(arguments.source_dir).resolve()
    build_directory = Path(arguments.build_dir).resolve()
    exec_limits_path = Path(arguments.exec_limits_header).resolve()
    tree_directory = build_directory / "tree"
    if tree_directory.exists():
        shutil.rmtree(tree_directory)
    tree_directory.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source_directory, tree_directory)
    configure_features(tree_directory / "dietfeatures.h")
    configure_exec_argument_limit(
        tree_directory / "include" / "limits.h",
        read_exec_argument_limit(exec_limits_path),
    )

    compiler = " ".join(shlex.split(arguments.cc))
    make_arguments = [
        "make",
        f"-j{arguments.jobs}",
        "ARCH=x86_64",
        f"CC={compiler}",
        "EXTRACFLAGS=-Werror -fno-stack-protector -fno-pie -fno-pic",
        "bin-x86_64/start.o",
        "bin-x86_64/dietlibc.a",
    ]
    run(make_arguments, tree_directory)

    extra_objects: list[str] = []
    compiler_arguments = shlex.split(arguments.cc)
    for source_subdirectory in ("libshell", "libregex"):
        source_root = tree_directory / source_subdirectory
        if not source_root.exists():
            continue
        for source_path in sorted(source_root.glob("*.c")):
            object_path = tree_directory / "bin-x86_64" / f"{source_path.stem}.o"
            compile_arguments = compiler_arguments + [
                "-I" + str(tree_directory),
                "-isystem",
                str(tree_directory / "include"),
                "-pipe",
                "-nostdinc",
                "-D_REENTRANT",
                "-D__dietlibc__",
                "-Werror",
                "-fno-stack-protector",
                "-fno-pie",
                "-fno-pic",
                "-O2",
                "-fomit-frame-pointer",
                "-c",
                str(source_path),
                "-o",
                str(object_path),
            ]
            run(compile_arguments, tree_directory)
            extra_objects.append(str(object_path))
    if extra_objects:
        run(
            ["ar", "rcs", "bin-x86_64/dietlibc.a", *extra_objects],
            tree_directory,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
