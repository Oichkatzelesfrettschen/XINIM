#!/usr/bin/env python3
"""Verify XINIM source-language ownership and C/C++-assembly boundaries."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass


EXPECTED_COLUMNS = ("path", "language", "role", "cmake", "assembly_contract")
LANGUAGES = {"cpp23", "c23", "assembly"}
ROLES = {
    "shared-abi-header",
    "utility",
    "mksh-abi-adapter",
    "host-test",
    "live",
    "retired",
}
CMAKE_MODES = {"required", "generated", "header", "forbidden"}
ASSEMBLY_CONTRACTS = {"-", "public-symbols"}
OPTIONAL_EXTERNAL_C_REFERENCES = {".scratch/tcc/xinim_stubs.c"}
VENDOR_PREFIXES = ("libc/", "third_party/", "archive/")
SOURCE_PATH_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_.-])(?P<path>(?:src|userland|test|include|boot)/"
    r"[A-Za-z0-9_./+-]+\.c)(?![A-Za-z0-9_.-])"
)
GLOBAL_PATTERN = re.compile(r"^\s*\.(?:global|globl)\s+(?P<symbol>[^\s#]+)")
TYPE_PATTERN = re.compile(r"^\s*\.type\s+(?P<symbol>[^,\s]+)")


@dataclass(frozen=True)
class OwnershipRow:
    path: str
    language: str
    role: str
    cmake: str
    assembly_contract: str


def parse_args() -> argparse.Namespace:
    repository_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=repository_root)
    parser.add_argument(
        "--manifest",
        type=pathlib.Path,
        default=pathlib.Path("docs/ownership/cpp23_c23_assembly_ownership.tsv"),
    )
    parser.add_argument(
        "--cmake", type=pathlib.Path, default=pathlib.Path("CMakeLists.txt")
    )
    parser.add_argument("--compile-commands", type=pathlib.Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(repository_root: pathlib.Path, candidate: pathlib.Path) -> pathlib.Path:
    if candidate.is_absolute():
        return candidate.resolve()
    return (repository_root / candidate).resolve()


def parse_manifest_text(manifest_text: str) -> tuple[list[OwnershipRow], list[str]]:
    failures: list[str] = []
    rows: list[OwnershipRow] = []
    saw_header = False
    for line_number, line in enumerate(manifest_text.splitlines(), start=1):
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        if not saw_header:
            saw_header = True
            if fields != EXPECTED_COLUMNS:
                failures.append(
                    f"line {line_number}: expected header {EXPECTED_COLUMNS}, got {fields}"
                )
            continue
        if len(fields) != len(EXPECTED_COLUMNS):
            failures.append(f"line {line_number}: expected five tab-separated fields")
            continue
        rows.append(OwnershipRow(*fields))
    if not saw_header:
        failures.append("manifest is missing its header")
    return rows, failures


def safe_relative_path(raw_path: str) -> pathlib.PurePosixPath | None:
    candidate = pathlib.PurePosixPath(raw_path)
    if (
        not raw_path
        or candidate.is_absolute()
        or ".." in candidate.parts
        or str(candidate) != raw_path
    ):
        return None
    return candidate


def tracked_paths(repository_root: pathlib.Path) -> list[str]:
    completed = subprocess.run(
        [
            "git",
            "-c",
            "core.fsmonitor=false",
            "ls-files",
            "-co",
            "--exclude-standard",
            "-z",
        ],
        cwd=repository_root,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        diagnostic = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git ls-files failed: {diagnostic}")
    return [
        path.decode("utf-8")
        for path in completed.stdout.split(b"\0")
        if path and (repository_root / path.decode("utf-8")).is_file()
    ]


def project_owned(path: str) -> bool:
    return not path.startswith(VENDOR_PREFIXES) and not path.startswith(".git/")


def read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise RuntimeError(f"{path} is not ASCII: {error}") from error
    except OSError as error:
        raise RuntimeError(f"cannot read {path}: {error}") from error


def cmake_c_references(cmake_text: str) -> tuple[set[str], list[str]]:
    references: set[str] = set()
    failures: list[str] = []
    for line_number, line in enumerate(cmake_text.splitlines(), start=1):
        if ".c" not in line:
            continue
        if "_dietlibc_source_dir" in line or "libc/dietlibc-xinim" in line:
            continue
        for match in SOURCE_PATH_PATTERN.finditer(line):
            references.add(match.group("path"))
        if ".scratch/tcc/xinim_stubs.c" in line:
            references.add(".scratch/tcc/xinim_stubs.c")
        if "CMAKE_C_COMPILER" in line and "dietlibc" not in line:
            failures.append(
                f"line {line_number}: project-owned CMake command uses CMAKE_C_COMPILER"
            )
    return references, failures


def validate_assembly_source(
    source_path: pathlib.Path, row: OwnershipRow
) -> list[str]:
    if row.role == "retired":
        return []
    source_text = read_text(source_path)
    failures: list[str] = []
    globals_found = {
        match.group("symbol")
        for match in map(GLOBAL_PATTERN.match, source_text.splitlines())
        if match is not None
    }
    types_found = {
        match.group("symbol")
        for match in map(TYPE_PATTERN.match, source_text.splitlines())
        if match is not None
    }
    if not globals_found:
        failures.append(f"{row.path}: live assembly source lacks a global declaration")
    missing_types = sorted(symbol for symbol in globals_found if symbol not in types_found)
    if missing_types:
        failures.append(
            f"{row.path}: public assembly symbols lack .type declarations: "
            + ", ".join(missing_types)
        )
    if ".note.GNU-stack" not in source_text:
        failures.append(f"{row.path}: assembly source lacks a GNU-stack note")
    return failures


def validate_compile_commands(
    compile_commands_path: pathlib.Path,
    repository_root: pathlib.Path,
    rows: list[OwnershipRow],
) -> list[str]:
    try:
        compile_commands = json.loads(read_text(compile_commands_path))
    except json.JSONDecodeError as error:
        return [f"cannot parse compile_commands.json: {error}"]
    if not isinstance(compile_commands, list):
        return ["compile_commands.json is not an array"]

    failures: list[str] = []
    for row in rows:
        if row.language != "cpp23":
            continue
        source_path = (repository_root / row.path).resolve()
        matches = []
        for entry in compile_commands:
            if not isinstance(entry, dict):
                continue
            command = str(entry.get("command", ""))
            file_name = str(entry.get("file", ""))
            if str(source_path) in command or file_name == str(source_path):
                matches.append(command)
        if not matches:
            # CMake custom commands and lane-scoped targets do not all appear
            # in compile_commands.json.  The manifest/CMake source contract
            # remains authoritative for those rows; verify C++23 flags when a
            # compilation entry is available for the active lane.
            continue
        if not any(
            flag in command
            for command in matches
            for flag in (
                "-std=c++23",
                "-std=gnu++23",
                "-std=c++2b",
                "-std=gnu++2b",
            )
        ):
            failures.append(f"{row.path}: compile command does not select C++23")
    return failures


def validate_repository(
    repository_root: pathlib.Path,
    manifest_path: pathlib.Path,
    cmake_path: pathlib.Path,
    compile_commands_path: pathlib.Path | None = None,
    explicit_tracked_paths: list[str] | None = None,
) -> list[str]:
    failures: list[str] = []
    try:
        rows, manifest_failures = parse_manifest_text(read_text(manifest_path))
    except RuntimeError as error:
        return [str(error)]
    failures.extend(manifest_failures)

    paths = [row.path for row in rows]
    if len(paths) != len(set(paths)):
        failures.append("ownership manifest contains duplicate paths")

    row_by_path = {row.path: row for row in rows}
    for row in rows:
        candidate = safe_relative_path(row.path)
        if candidate is None:
            failures.append(f"{row.path}: unsafe or non-canonical manifest path")
            continue
        if row.language not in LANGUAGES:
            failures.append(f"{row.path}: invalid language {row.language}")
        if row.role not in ROLES:
            failures.append(f"{row.path}: invalid role {row.role}")
        if row.cmake not in CMAKE_MODES:
            failures.append(f"{row.path}: invalid CMake mode {row.cmake}")
        if row.assembly_contract not in ASSEMBLY_CONTRACTS:
            failures.append(
                f"{row.path}: invalid assembly contract {row.assembly_contract}"
            )
        source_path = repository_root / candidate
        if not source_path.is_file():
            failures.append(f"{row.path}: manifest source is missing")
        if row.language == "cpp23" and not row.path.endswith(".cpp"):
            failures.append(f"{row.path}: C++23 implementation must use .cpp")
        if row.language == "c23" and not row.path.endswith(".h"):
            failures.append(f"{row.path}: C23 ownership rows must be ABI headers")
        if row.language == "assembly":
            if not row.path.endswith((".S", ".s")):
                failures.append(f"{row.path}: assembly ownership row has non-assembly suffix")
            if row.role == "retired" and row.cmake != "forbidden":
                failures.append(f"{row.path}: retired assembly must be forbidden in CMake")
            if row.role == "live" and row.cmake != "required":
                failures.append(f"{row.path}: live assembly must be required in CMake")

    if failures:
        return failures

    try:
        cmake_text = read_text(cmake_path)
    except RuntimeError as error:
        return [str(error)]
    cmake_c_paths, cmake_failures = cmake_c_references(cmake_text)
    failures.extend(cmake_failures)
    if re.search(r"(?<![A-Za-z0-9_])_util_source_c(?![A-Za-z0-9_])", cmake_text):
        failures.append("CMake retains an explicit _util_source_c fallback")
    if re.search(r"Prefer C\+\+23 source; fall back to C", cmake_text):
        failures.append("CMake retains a C fallback policy comment")
    if "set(_util_source_cpp" not in cmake_text:
        failures.append("CMake does not define the C++23 utility source rule")
    if "-std=c++23" not in cmake_text:
        failures.append("CMake does not select C++23 for generated utilities")

    for row in rows:
        if row.language == "cpp23" and row.cmake == "required":
            if row.path not in cmake_text:
                failures.append(f"{row.path}: required C++23 source is not referenced by CMake")

    project_c_paths = {
        path
        for path in cmake_c_paths
        if path not in OPTIONAL_EXTERNAL_C_REFERENCES
    }
    for path in sorted(project_c_paths):
        row = row_by_path.get(path)
        if row is None or row.language != "c23":
            failures.append(f"CMake references undeclared project-owned C source {path}")
    for path in sorted(cmake_c_paths & OPTIONAL_EXTERNAL_C_REFERENCES):
        if (repository_root / path).exists():
            failures.append(
                f"optional third-party CMake input unexpectedly became project-owned: {path}"
            )

    tracked = explicit_tracked_paths if explicit_tracked_paths is not None else tracked_paths(repository_root)
    tracked_project_c = {
        path
        for path in tracked
        if project_owned(path) and path.endswith(".c")
    }
    tracked_project_assembly = {
        path
        for path in tracked
        if project_owned(path) and path.endswith((".S", ".s"))
    }
    declared_c23 = {
        row.path for row in rows if row.language == "c23" and row.path.endswith(".c")
    }
    undeclared_c = sorted(tracked_project_c - declared_c23)
    if undeclared_c:
        failures.extend(
            f"tracked project-owned C source is not declared as a C23 ABI boundary: {path}"
            for path in undeclared_c
        )
    declared_assembly = {row.path for row in rows if row.language == "assembly"}
    if tracked_project_assembly != declared_assembly:
        failures.append(
            "assembly manifest does not match tracked project-owned assembly: "
            f"missing={sorted(tracked_project_assembly - declared_assembly)}, "
            f"unexpected={sorted(declared_assembly - tracked_project_assembly)}"
        )

    for row in rows:
        if row.language != "assembly":
            continue
        source_path = repository_root / row.path
        failures.extend(validate_assembly_source(source_path, row))
        if row.cmake == "required" and row.path not in cmake_text:
            failures.append(f"{row.path}: live assembly is not referenced by CMake")
        if row.cmake == "forbidden" and row.path in cmake_text:
            failures.append(f"{row.path}: retired assembly remains referenced by CMake")

    if compile_commands_path is not None:
        failures.extend(
            validate_compile_commands(compile_commands_path, repository_root, rows)
        )
    return failures


def require_failure(name: str, failures: list[str], fragment: str) -> None:
    if not any(fragment in failure for failure in failures):
        raise AssertionError(f"{name}: expected {fragment!r}, got {failures}")


def run_self_test() -> None:
    manifest = "\n".join(
        [
            "\t".join(EXPECTED_COLUMNS),
            "fixture.cpp\tcpp23\thost-test\trequired\t-",
            "fixture.h\tc23\tshared-abi-header\theader\t-",
            "fixture.S\tassembly\tlive\trequired\tpublic-symbols",
        ]
    ) + "\n"
    cmake = (
        "set(_util_source_cpp fixture.cpp)\n"
        "set(CXX_FLAGS -std=c++23)\n"
        "set(ASSEMBLY fixture.S)\n"
    )
    with tempfile.TemporaryDirectory(prefix="xinim-language-ownership-") as temp_dir:
        root = pathlib.Path(temp_dir)
        (root / "docs").mkdir()
        (root / "fixture.cpp").write_text("int main() { return 0; }\n", encoding="ascii")
        (root / "fixture.h").write_text("#define FIXTURE 1\n", encoding="ascii")
        (root / "fixture.S").write_text(
            ".text\n.global fixture\n.type fixture, @function\nfixture:\n ret\n.note.GNU-stack\n",
            encoding="ascii",
        )
        manifest_path = root / "manifest.tsv"
        cmake_path = root / "CMakeLists.txt"
        manifest_path.write_text(manifest, encoding="ascii")
        cmake_path.write_text(cmake, encoding="ascii")
        tracked = ["fixture.cpp", "fixture.h", "fixture.S"]
        good = validate_repository(
            root, manifest_path, cmake_path, explicit_tracked_paths=tracked
        )
        if good:
            raise AssertionError(f"known-good ownership fixture failed: {good}")

        bad_c = validate_repository(
            root,
            manifest_path,
            cmake_path,
            explicit_tracked_paths=[*tracked, "legacy.c"],
        )
        require_failure("unowned C source", bad_c, "tracked project-owned C source")

        bad_fallback = validate_repository(
            root,
            manifest_path,
            cmake_path,
            explicit_tracked_paths=tracked,
        )
        fallback_cmake = root / "CMakeLists.txt"
        fallback_cmake.write_text(
            cmake + "set(_util_source_c fixture.c)\n", encoding="ascii"
        )
        bad_fallback = validate_repository(
            root, manifest_path, fallback_cmake, explicit_tracked_paths=tracked
        )
        require_failure("C fallback", bad_fallback, "explicit _util_source_c fallback")

        (root / "fixture.S").write_text(
            ".text\n.global fixture\nfixture:\n ret\n.note.GNU-stack\n",
            encoding="ascii",
        )
        bad_assembly = validate_repository(
            root, manifest_path, cmake_path, explicit_tracked_paths=tracked
        )
        require_failure("assembly declaration", bad_assembly, "lack .type declarations")

    print("C++23/C23/assembly ownership mutation self-test passed.")


def main() -> int:
    args = parse_args()
    if args.self_test:
        run_self_test()
        return 0

    repository_root = args.repo_root.resolve()
    manifest_path = resolve_path(repository_root, args.manifest)
    cmake_path = resolve_path(repository_root, args.cmake)
    compile_commands_path = (
        resolve_path(repository_root, args.compile_commands)
        if args.compile_commands is not None
        else None
    )
    try:
        failures = validate_repository(
            repository_root,
            manifest_path,
            cmake_path,
            compile_commands_path,
        )
    except RuntimeError as error:
        failures = [str(error)]
    if failures:
        print("C++23/C23/assembly ownership verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    rows, _ = parse_manifest_text(read_text(manifest_path))
    cpp23_count = sum(row.language == "cpp23" for row in rows)
    c23_count = sum(row.language == "c23" for row in rows)
    assembly_count = sum(row.language == "assembly" for row in rows)
    print(
        "C++23/C23/assembly ownership verified: "
        f"{cpp23_count} C++23 sources, {c23_count} C23 ABI headers, "
        f"{assembly_count} assembly sources, 0 project-owned C implementations."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
