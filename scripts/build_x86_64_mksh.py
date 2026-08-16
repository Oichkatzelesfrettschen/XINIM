#!/usr/bin/env python3

import argparse
import os
import shlex
import shutil
import subprocess
from pathlib import Path

GENERATED_OPTION_FILES = ("rlimits.opt", "sh_flags.opt", "ulimits.opt")
TARGET_FEATURE_OVERRIDES = {
    "HAVE_ST_MTIM": "0",
    "HAVE_ULIMIT_H": "0",
    "HAVE_VALUES_H": "0",
}


def run(
    arguments: list[str],
    working_directory: Path,
    environment: dict[str, str] | None = None,
) -> None:
    subprocess.run(arguments, cwd=working_directory, env=environment, check=True)


def run_logged(
    arguments: list[str],
    working_directory: Path,
    log_path: Path,
    environment: dict[str, str],
) -> None:
    with log_path.open("wb") as log_file:
        result = subprocess.run(
            arguments,
            cwd=working_directory,
            env=environment,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            check=False,
        )
    if result.returncode != 0:
        log_lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        log_tail = "\n".join(log_lines[-80:])
        raise RuntimeError(f"mksh feature configuration failed:\n{log_tail}")


def parse_makefrag(makefrag_path: Path) -> tuple[list[str], list[str]]:
    source_files: list[str] = []
    preprocessor_flags: list[str] = []
    for raw_line in makefrag_path.read_text(encoding="ascii").splitlines():
        line = raw_line.strip()
        if line.startswith("SRCS="):
            source_files = line.split("=", 1)[1].split()
        elif line.startswith("CPPFLAGS="):
            preprocessor_flags = shlex.split(line.split("=", 1)[1].strip())
    if not source_files:
        raise RuntimeError(f"could not parse SRCS from {makefrag_path}")
    if not preprocessor_flags:
        raise RuntimeError(f"could not parse CPPFLAGS from {makefrag_path}")
    return source_files, preprocessor_flags


def generate_makefrag(
    source_directory: Path,
    configure_directory: Path,
    compiler: str,
    shell_mode: str,
) -> Path:
    configure_directory.mkdir(parents=True)
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
    configuration_arguments = ["sh", str(source_directory / "Build.sh")]
    if shell_mode == "lksh":
        configuration_arguments.append("-L")
    configuration_arguments.append("-M")
    run_logged(
        configuration_arguments,
        configure_directory,
        configure_directory / "configure.log",
        environment,
    )
    for option_file in GENERATED_OPTION_FILES:
        generation_environment = environment.copy()
        generation_environment.update(
            {
                "BUILDSH_RUN_GENOPT": "1",
                "srcfile": str(source_directory / option_file),
            }
        )
        run(
            ["sh", str(source_directory / "Build.sh")],
            configure_directory,
            generation_environment,
        )
    return configure_directory / "Makefrag.inc"


def target_cppflags(base_flags: list[str]) -> list[str]:
    filtered_flags: list[str] = []
    skip_next = False
    for flag in base_flags:
        if skip_next:
            skip_next = False
            continue
        if flag == "-I":
            skip_next = True
            continue
        if flag.startswith("-I"):
            continue
        if flag.startswith("-D"):
            feature_name = flag[2:].split("=", 1)[0]
            if feature_name in TARGET_FEATURE_OVERRIDES:
                continue
        filtered_flags.append(flag)
    filtered_flags.extend(
        f"-D{feature_name}={feature_value}"
        for feature_name, feature_value in sorted(TARGET_FEATURE_OVERRIDES.items())
    )
    if "-DMKSH_BINSHPOSIX" not in filtered_flags:
        filtered_flags.append("-DMKSH_BINSHPOSIX")
    filtered_flags.extend(("-DMKSH_ASSUME_UTF8=0", "-D__dietlibc__"))
    return filtered_flags


def require_safe_build_directory(build_directory: Path, source_directory: Path) -> None:
    if build_directory == Path("/") or build_directory == Path.home():
        raise RuntimeError(f"unsafe mksh build directory: {build_directory}")
    if (
        build_directory == source_directory
        or build_directory in source_directory.parents
    ):
        raise RuntimeError("mksh build directory must not contain the source directory")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--start-o", required=True)
    parser.add_argument("--dietlibc-a", required=True)
    parser.add_argument("--linker-script", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--mode", choices=("mksh", "lksh"), default="mksh")
    arguments = parser.parse_args()

    source_directory = Path(arguments.source_dir).resolve()
    build_directory = Path(arguments.build_dir).resolve()
    start_object = Path(arguments.start_o).resolve()
    dietlibc_archive = Path(arguments.dietlibc_a).resolve()
    linker_script = Path(arguments.linker_script).resolve()
    output_path = Path(arguments.output).resolve()
    require_safe_build_directory(build_directory, source_directory)

    configure_directory = build_directory / "configure"
    object_directory = build_directory / "objects"
    for generated_directory in (configure_directory, object_directory):
        if generated_directory.exists():
            shutil.rmtree(generated_directory)
    object_directory.mkdir(parents=True)

    compiler_arguments = shlex.split(arguments.cc)
    if not compiler_arguments:
        raise RuntimeError("empty C compiler command")
    makefrag_path = generate_makefrag(
        source_directory,
        configure_directory,
        compiler_arguments[0],
        arguments.mode,
    )
    source_files, base_cppflags = parse_makefrag(makefrag_path)
    preprocessor_flags = target_cppflags(base_cppflags)
    dietlibc_include_directory = start_object.parent.parent / "include"
    common_flags = [
        "-std=gnu11",
        "-O2",
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
        str(configure_directory),
        "-I",
        str(source_directory),
        "-isystem",
        str(dietlibc_include_directory),
    ]

    object_files: list[str] = []
    for source_name in source_files:
        source_path = source_directory / source_name
        object_path = object_directory / f"{Path(source_name).stem}.o"
        compile_arguments = (
            compiler_arguments
            + common_flags
            + preprocessor_flags
            + ["-c", str(source_path), "-o", str(object_path)]
        )
        run(compile_arguments, object_directory)
        object_files.append(str(object_path))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    link_map_path = build_directory / "mksh-link.map"
    link_arguments = compiler_arguments + [
        "-nostdlib",
        "-static",
        "-no-pie",
        "-Wl,--build-id=none",
        "-Wl,--cref",
        "-Wl,-Map," + str(link_map_path),
        "-Wl,-T," + str(linker_script),
        "-o",
        str(output_path),
        str(start_object),
        *object_files,
        str(dietlibc_archive),
        "-lgcc",
    ]
    run(link_arguments, build_directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
