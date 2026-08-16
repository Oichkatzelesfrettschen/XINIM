#!/usr/bin/env python3

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import tempfile
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Protocol

DIETLIBC_SENTINEL = "__you_tried_to_link_a_dietlibc_object_against_glibc"
MKSH_POSIX_PROFILE_MARKER = b"KSH_VERSION=@(#)LEGACY KSH R59"

DIETLIBC_BINARIES = frozenset(
    {
        "bootfs-reuse-check",
        "byte-oracle",
        "mkdir",
        "mksh",
        "printf",
        "printf-signal-oracle",
        "rm",
        "sh",
    }
)
SYSCALL_ONLY_BINARIES = frozenset(
    {
        "brk-check",
        "echo",
        "file-operations-check",
        "fs-abi-check",
        "mmap-check",
        "preempt-check",
        "runtime-check",
        "select-check",
        "signal-check",
        "true",
        "tty-abi-check",
    }
)
EXPECTED_PROVIDERS = {
    **{name: "dietlibc" for name in DIETLIBC_BINARIES},
    **{name: "syscall-only" for name in SYSCALL_ONLY_BINARIES},
}
FORBIDDEN_SHELL_NAMES = frozenset(
    {"ash", "bash", "dash", "ksh", "lksh", "pdksh", "xash", "zsh"}
)


@dataclass(frozen=True)
class ElfEvidence:
    elf_class: str
    encoding: str
    elf_type: str
    machine: str
    has_interpreter: bool
    needed_libraries: tuple[str, ...]
    has_dietlibc_sentinel: bool


@dataclass(frozen=True)
class ProviderBuild:
    provider: str
    binary_path: Path


class Inspector(Protocol):
    def inspect(self, binary_path: Path) -> ElfEvidence: ...


class ReadelfInspector:
    def __init__(self, readelf_path: Path) -> None:
        self.readelf_path = readelf_path

    def run(self, option: str, binary_path: Path) -> str:
        environment = os.environ.copy()
        environment["LC_ALL"] = "C"
        result = subprocess.run(
            [str(self.readelf_path), option, "--wide", str(binary_path)],
            check=False,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"readelf {option} failed for {binary_path}: {result.stdout.strip()}"
            )
        return result.stdout

    @staticmethod
    def header_value(header_text: str, field_name: str) -> str:
        match = re.search(
            rf"^[ \t]*{re.escape(field_name)}:[ \t]*(.+)$",
            header_text,
            flags=re.MULTILINE,
        )
        if match is None:
            raise RuntimeError(f"readelf header omits {field_name}")
        return match.group(1).strip()

    def inspect(self, binary_path: Path) -> ElfEvidence:
        header_text = self.run("--file-header", binary_path)
        program_text = self.run("--program-headers", binary_path)
        dynamic_text = self.run("--dynamic", binary_path)
        symbol_text = self.run("--symbols", binary_path)
        needed_libraries = tuple(re.findall(r"\(NEEDED\).*?\[([^]]+)\]", dynamic_text))
        return ElfEvidence(
            elf_class=self.header_value(header_text, "Class"),
            encoding=self.header_value(header_text, "Data"),
            elf_type=self.header_value(header_text, "Type"),
            machine=self.header_value(header_text, "Machine"),
            has_interpreter=re.search(r"\bINTERP\b", program_text) is not None,
            needed_libraries=needed_libraries,
            has_dietlibc_sentinel=DIETLIBC_SENTINEL in symbol_text,
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for block in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_provider_manifest(manifest_path: Path) -> dict[str, ProviderBuild]:
    try:
        manifest_lines = manifest_path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise RuntimeError(f"cannot read provider manifest {manifest_path}: {error}") from error

    expected_header = "provider\tname\tbuild_path"
    if not manifest_lines or manifest_lines[0] != expected_header:
        raise RuntimeError(
            f"provider manifest {manifest_path} must begin with {expected_header!r}"
        )

    provider_builds: dict[str, ProviderBuild] = {}
    for line_number, manifest_line in enumerate(manifest_lines[1:], start=2):
        fields = manifest_line.split("\t")
        if len(fields) != 3:
            raise RuntimeError(
                f"provider manifest {manifest_path}:{line_number} must have three fields"
            )
        provider, binary_name, build_path_text = fields
        if binary_name in provider_builds:
            raise RuntimeError(
                f"provider manifest {manifest_path}:{line_number} duplicates {binary_name}"
            )
        expected_provider = EXPECTED_PROVIDERS.get(binary_name)
        if expected_provider is None:
            raise RuntimeError(
                f"provider manifest {manifest_path}:{line_number} names unknown binary "
                f"{binary_name}"
            )
        if provider != expected_provider:
            raise RuntimeError(
                f"provider manifest {manifest_path}:{line_number} classifies "
                f"{binary_name} as {provider}, expected {expected_provider}"
            )
        build_path = Path(build_path_text)
        if not build_path.is_absolute():
            raise RuntimeError(
                f"provider manifest {manifest_path}:{line_number} build path is not "
                f"absolute: {build_path}"
            )
        provider_builds[binary_name] = ProviderBuild(
            provider=provider,
            binary_path=build_path,
        )

    missing_names = sorted(set(EXPECTED_PROVIDERS) - set(provider_builds))
    if missing_names:
        raise RuntimeError(
            "provider manifest omits binaries: " + ", ".join(missing_names)
        )
    return provider_builds


def verify_runtime(
    stage_directory: Path,
    inspector: Inspector,
    provider_builds: dict[str, ProviderBuild],
) -> list[str]:
    errors: list[str] = []
    binary_directory = stage_directory / "bin"
    if not binary_directory.is_dir():
        return [f"missing staged binary directory: {binary_directory}"]

    entries = {entry.name: entry for entry in binary_directory.iterdir()}
    actual_names = set(entries)
    expected_names = set(EXPECTED_PROVIDERS)
    missing_names = sorted(expected_names - actual_names)
    unexpected_names = sorted(actual_names - expected_names)
    if missing_names:
        errors.append("missing /bin entries: " + ", ".join(missing_names))
    if unexpected_names:
        errors.append("unexpected /bin entries: " + ", ".join(unexpected_names))
    forbidden_names = sorted(actual_names & FORBIDDEN_SHELL_NAMES)
    if forbidden_names:
        errors.append("alternate shell entries present: " + ", ".join(forbidden_names))

    for binary_name in sorted(actual_names & expected_names):
        binary_path = entries[binary_name]
        if binary_path.is_symlink() or not binary_path.is_file():
            errors.append(f"/bin/{binary_name} is not a regular file")
            continue
        if not os.access(binary_path, os.X_OK):
            errors.append(f"/bin/{binary_name} is not executable")

        provider_build = provider_builds[binary_name]
        build_binary_path = provider_build.binary_path
        if build_binary_path.is_symlink() or not build_binary_path.is_file():
            errors.append(
                f"/bin/{binary_name} provider build artifact is not a regular file: "
                f"{build_binary_path}"
            )
        elif sha256_file(binary_path) != sha256_file(build_binary_path):
            errors.append(
                f"/bin/{binary_name} does not match provider build artifact "
                f"{build_binary_path}"
            )
        try:
            evidence = inspector.inspect(binary_path)
        except RuntimeError as error:
            errors.append(str(error))
            continue

        if not evidence.elf_class.startswith("ELF64"):
            errors.append(f"/bin/{binary_name} is not ELF64: {evidence.elf_class}")
        if not evidence.encoding.startswith("2's complement, little endian"):
            errors.append(
                f"/bin/{binary_name} is not little-endian ELF: {evidence.encoding}"
            )
        if not evidence.elf_type.startswith("EXEC"):
            errors.append(f"/bin/{binary_name} is not ET_EXEC: {evidence.elf_type}")
        if "X86-64" not in evidence.machine:
            errors.append(f"/bin/{binary_name} is not x86_64: {evidence.machine}")
        if evidence.has_interpreter:
            errors.append(f"/bin/{binary_name} contains a PT_INTERP loader request")
        if evidence.needed_libraries:
            needed = ", ".join(evidence.needed_libraries)
            errors.append(f"/bin/{binary_name} contains DT_NEEDED entries: {needed}")

        provider = provider_build.provider
        if provider == "dietlibc" and not evidence.has_dietlibc_sentinel:
            errors.append(f"/bin/{binary_name} does not carry the dietlibc sentinel")
        if provider == "syscall-only" and evidence.has_dietlibc_sentinel:
            errors.append(
                f"/bin/{binary_name} unexpectedly carries the dietlibc sentinel"
            )

    shell_path = entries.get("sh")
    mksh_path = entries.get("mksh")
    if shell_path is not None and mksh_path is not None:
        if shell_path.is_file() and mksh_path.is_file():
            if sha256_file(shell_path) != sha256_file(mksh_path):
                errors.append("/bin/sh and /bin/mksh are not byte-identical")
            mksh_bytes = mksh_path.read_bytes()
            if MKSH_POSIX_PROFILE_MARKER not in mksh_bytes:
                errors.append("/bin/mksh is not the mksh R59 legacy POSIX profile")

    return errors


class FakeInspector:
    def __init__(self, evidence_by_name: dict[str, ElfEvidence]) -> None:
        self.evidence_by_name = evidence_by_name

    def inspect(self, binary_path: Path) -> ElfEvidence:
        return self.evidence_by_name[binary_path.name]


def valid_evidence(provider: str) -> ElfEvidence:
    return ElfEvidence(
        elf_class="ELF64",
        encoding="2's complement, little endian",
        elf_type="EXEC (Executable file)",
        machine="Advanced Micro Devices X86-64",
        has_interpreter=False,
        needed_libraries=(),
        has_dietlibc_sentinel=provider == "dietlibc",
    )


def require_error(errors: list[str], expected_fragment: str) -> None:
    if not any(expected_fragment in error for error in errors):
        rendered_errors = "\n".join(errors)
        raise RuntimeError(
            f"mutation did not produce {expected_fragment!r}:\n{rendered_errors}"
        )


def self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="xinim-runtime-ownership-") as directory:
        stage_directory = Path(directory)
        binary_directory = stage_directory / "bin"
        build_directory = stage_directory / "build"
        binary_directory.mkdir()
        build_directory.mkdir()
        evidence_by_name = {
            name: valid_evidence(provider)
            for name, provider in EXPECTED_PROVIDERS.items()
        }
        manifest_lines = ["provider\tname\tbuild_path"]
        for binary_name, provider in EXPECTED_PROVIDERS.items():
            contents = b"\x7fELF" + binary_name.encode("ascii")
            if binary_name in {"mksh", "sh"}:
                contents = b"\x7fELF" + MKSH_POSIX_PROFILE_MARKER
            binary_path = binary_directory / binary_name
            binary_path.write_bytes(contents)
            binary_path.chmod(0o755)
            build_path = build_directory / binary_name
            build_path.write_bytes(contents)
            build_path.chmod(0o755)
            manifest_lines.append(f"{provider}\t{binary_name}\t{build_path}")

        manifest_path = stage_directory / "providers.tsv"
        manifest_path.write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")
        provider_builds = load_provider_manifest(manifest_path)

        inspector = FakeInspector(evidence_by_name)
        baseline_errors = verify_runtime(stage_directory, inspector, provider_builds)
        if baseline_errors:
            raise RuntimeError("valid runtime rejected:\n" + "\n".join(baseline_errors))

        alternate_shell_path = binary_directory / "xash"
        alternate_shell_path.write_bytes(b"\x7fELFxash")
        alternate_shell_path.chmod(0o755)
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "alternate shell entries present: xash",
        )
        alternate_shell_path.unlink()

        unknown_binary_path = binary_directory / "unclassified-tool"
        unknown_binary_path.write_bytes(b"\x7fELFunclassified")
        unknown_binary_path.chmod(0o755)
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "unexpected /bin entries: unclassified-tool",
        )
        unknown_binary_path.unlink()

        rm_path = binary_directory / "rm"
        rm_contents = rm_path.read_bytes()
        rm_path.unlink()
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "missing /bin entries: rm",
        )
        rm_path.write_bytes(rm_contents)
        rm_path.chmod(0o755)

        shell_path = binary_directory / "sh"
        shell_contents = shell_path.read_bytes()
        shell_path.write_bytes(shell_contents + b"different")
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/sh and /bin/mksh are not byte-identical",
        )
        shell_path.write_bytes(shell_contents)
        shell_path.chmod(0o755)

        true_build_path = provider_builds["true"].binary_path
        true_build_contents = true_build_path.read_bytes()
        true_build_path.write_bytes(true_build_contents + b"different")
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/true does not match provider build artifact",
        )
        true_build_path.write_bytes(true_build_contents)
        true_build_path.chmod(0o755)

        evidence_by_name["printf"] = replace(
            evidence_by_name["printf"], has_dietlibc_sentinel=False
        )
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/printf does not carry the dietlibc sentinel",
        )
        evidence_by_name["printf"] = valid_evidence("dietlibc")

        evidence_by_name["true"] = replace(
            evidence_by_name["true"], has_dietlibc_sentinel=True
        )
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/true unexpectedly carries the dietlibc sentinel",
        )
        evidence_by_name["true"] = valid_evidence("syscall-only")

        evidence_by_name["mksh"] = replace(
            evidence_by_name["mksh"], has_interpreter=True
        )
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/mksh contains a PT_INTERP loader request",
        )
        evidence_by_name["mksh"] = replace(
            valid_evidence("dietlibc"), needed_libraries=("libc.so.6",)
        )
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/mksh contains DT_NEEDED entries: libc.so.6",
        )
        evidence_by_name["mksh"] = valid_evidence("dietlibc")

        mksh_path = binary_directory / "mksh"
        mksh_path.write_bytes(b"\x7fELFKSH_VERSION=@(#)MIRBSD KSH R59")
        shell_path.write_bytes(mksh_path.read_bytes())
        require_error(
            verify_runtime(stage_directory, inspector, provider_builds),
            "/bin/mksh is not the mksh R59 legacy POSIX profile",
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage-dir")
    parser.add_argument("--provider-manifest")
    parser.add_argument("--readelf")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    if arguments.self_test:
        self_test()
        print("PASS: x86_64 runtime ownership verifier self-test")
        return 0
    if arguments.stage_dir is None:
        parser.error("--stage-dir is required unless --self-test is used")
    if arguments.provider_manifest is None:
        parser.error("--provider-manifest is required unless --self-test is used")
    readelf_name = arguments.readelf or shutil.which("readelf")
    if readelf_name is None:
        parser.error("readelf was not found")

    stage_directory = Path(arguments.stage_dir).resolve()
    try:
        provider_builds = load_provider_manifest(
            Path(arguments.provider_manifest).resolve()
        )
        errors = verify_runtime(
            stage_directory,
            ReadelfInspector(Path(readelf_name).resolve()),
            provider_builds,
        )
    except RuntimeError as error:
        errors = [str(error)]
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    print("PASS: x86_64 runtime has one mksh shell and dietlibc-only libc ownership")
    for binary_name in sorted(EXPECTED_PROVIDERS):
        print(f"  /bin/{binary_name}: {EXPECTED_PROVIDERS[binary_name]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
