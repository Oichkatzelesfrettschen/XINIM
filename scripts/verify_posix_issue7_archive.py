#!/usr/bin/env python3
"""Verify and extract the pinned POSIX Issue 7, 2018 HTML archive."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import sys
import tarfile
import tempfile
from collections.abc import Callable
from pathlib import Path, PurePosixPath

ARCHIVE_NAME = "susv4-2018.tgz"
ARCHIVE_SHA256 = "ab6636bca53c7d71d33d2c5149ede574d598fe6ec97fa8b08e0459ef7bcfc104"
ARCHIVE_SIZE = 4_976_241
ARCHIVE_TOP_DIRECTORY = "susv4-2018"
ARCHIVE_MEMBER_COUNT = 1_680
ARCHIVE_FILE_COUNT = 1_668
ARCHIVE_DIRECTORY_COUNT = 12
EXPECTED_STANDALONE_COUNT = 160
EXPECTED_STANDALONE_KEY_SHA256 = (
    "e4d9aa4398b7a4da71aa647dbb958c710905b3964c27a56bdb2d9fa0004d1906"
)
EXPECTED_FULL_COUNT = 175
EXPECTED_FULL_KEY_SHA256 = (
    "e1c7db29258b2e7e869d9183306e2462261a87d598750d457722824f7b655e7d"
)
SPECIAL_BUILTINS = (
    "break",
    "colon",
    "continue",
    "dot",
    "eval",
    "exec",
    "exit",
    "export",
    "readonly",
    "return",
    "set",
    "shift",
    "times",
    "trap",
    "unset",
)
NON_UTILITY_HTML_PAGES = {
    "V3_chap01.html",
    "V3_chap02.html",
    "V3_chap03.html",
    "V3_chap04.html",
    "V3_title.html",
    "contents.html",
    "toc.html",
}
UTILITY_NAME_PATTERN = re.compile(r"[a-z][a-z0-9]*")


class VerificationError(RuntimeError):
    """Report a standards-source identity or corpus failure."""


def parse_args() -> argparse.Namespace:
    repository_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=repository_root)
    parser.add_argument(
        "--archive",
        type=Path,
        default=Path("data/external/posix") / ARCHIVE_NAME,
    )
    parser.add_argument(
        "--extract-root",
        type=Path,
        default=Path("build/_state/cache/posix"),
    )
    parser.add_argument(
        "--ledger",
        type=Path,
        default=Path("docs/posix/posix_issue7_utility_ledger.tsv"),
    )
    parser.add_argument("--extract", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(repository_root: Path, candidate: Path) -> Path:
    if candidate.is_absolute():
        return candidate.resolve()
    return (repository_root / candidate).resolve()


def key_sha256(keys: set[str]) -> str:
    encoded_keys = "".join(f"{key}\n" for key in sorted(keys)).encode("ascii")
    return hashlib.sha256(encoded_keys).hexdigest()


def verify_archive_identity(
    archive_path: Path,
    expected_size: int = ARCHIVE_SIZE,
    expected_sha256: str = ARCHIVE_SHA256,
) -> None:
    try:
        archive_size = archive_path.stat().st_size
        archive_bytes = archive_path.read_bytes()
    except OSError as error:
        raise VerificationError(
            f"cannot read archive {archive_path}: {error}"
        ) from error
    if archive_size != expected_size:
        raise VerificationError(
            f"archive size mismatch: expected {expected_size}, got {archive_size}"
        )
    observed_sha256 = hashlib.sha256(archive_bytes).hexdigest()
    if observed_sha256 != expected_sha256:
        raise VerificationError(
            "archive SHA-256 mismatch: "
            f"expected {expected_sha256}, got {observed_sha256}"
        )


def validate_member_path(member_name: str) -> PurePosixPath:
    if "\\" in member_name:
        raise VerificationError(f"archive member contains a backslash: {member_name}")
    member_path = PurePosixPath(member_name)
    if member_path.is_absolute() or not member_path.parts:
        raise VerificationError(f"archive member is not relative: {member_name}")
    if any(part in {"", ".", ".."} for part in member_path.parts):
        raise VerificationError(f"archive member has unsafe components: {member_name}")
    if member_path.parts[0] != ARCHIVE_TOP_DIRECTORY:
        raise VerificationError(
            f"archive member is outside {ARCHIVE_TOP_DIRECTORY}: {member_name}"
        )
    return member_path


def read_safe_members(archive_path: Path) -> tuple[list[tarfile.TarInfo], bytes]:
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            members = archive.getmembers()
            member_names: set[str] = set()
            regular_file_count = 0
            directory_count = 0
            for member in members:
                validate_member_path(member.name)
                if member.name in member_names:
                    raise VerificationError(
                        f"archive contains duplicate member: {member.name}"
                    )
                member_names.add(member.name)
                if member.isfile():
                    regular_file_count += 1
                elif member.isdir():
                    directory_count += 1
                else:
                    raise VerificationError(
                        f"archive member has unsupported type: {member.name}"
                    )

            if len(members) != ARCHIVE_MEMBER_COUNT:
                raise VerificationError(
                    "archive member count mismatch: "
                    f"expected {ARCHIVE_MEMBER_COUNT}, got {len(members)}"
                )
            if regular_file_count != ARCHIVE_FILE_COUNT:
                raise VerificationError(
                    "archive file count mismatch: "
                    f"expected {ARCHIVE_FILE_COUNT}, got {regular_file_count}"
                )
            if directory_count != ARCHIVE_DIRECTORY_COUNT:
                raise VerificationError(
                    "archive directory count mismatch: "
                    f"expected {ARCHIVE_DIRECTORY_COUNT}, got {directory_count}"
                )

            chapter_member_name = f"{ARCHIVE_TOP_DIRECTORY}/utilities/V3_chap02.html"
            chapter_file = archive.extractfile(chapter_member_name)
            if chapter_file is None:
                raise VerificationError(
                    f"missing archive member: {chapter_member_name}"
                )
            chapter_bytes = chapter_file.read()
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(
            f"cannot parse archive {archive_path}: {error}"
        ) from error
    return members, chapter_bytes


def derive_standalone_utility_keys(member_names: set[str]) -> set[str]:
    utilities_prefix = f"{ARCHIVE_TOP_DIRECTORY}/utilities/"
    html_pages = {
        member_name.removeprefix(utilities_prefix)
        for member_name in member_names
        if member_name.startswith(utilities_prefix)
        and "/" not in member_name.removeprefix(utilities_prefix)
        and member_name.endswith(".html")
    }
    utility_pages = html_pages - NON_UTILITY_HTML_PAGES
    standalone_keys = {page.removesuffix(".html") for page in utility_pages}
    invalid_keys = sorted(
        key for key in standalone_keys if UTILITY_NAME_PATTERN.fullmatch(key) is None
    )
    if invalid_keys:
        raise VerificationError(
            f"invalid standalone utility keys: {', '.join(invalid_keys)}"
        )
    if len(standalone_keys) != EXPECTED_STANDALONE_COUNT:
        raise VerificationError(
            "standalone utility count mismatch: "
            f"expected {EXPECTED_STANDALONE_COUNT}, got {len(standalone_keys)}"
        )
    observed_sha256 = key_sha256(standalone_keys)
    if observed_sha256 != EXPECTED_STANDALONE_KEY_SHA256:
        raise VerificationError(
            "standalone utility key hash mismatch: "
            f"expected {EXPECTED_STANDALONE_KEY_SHA256}, got {observed_sha256}"
        )
    return standalone_keys


def verify_special_builtin_anchors(chapter_bytes: bytes) -> set[str]:
    missing_builtins = [
        builtin
        for builtin in SPECIAL_BUILTINS
        if f'<a name="{builtin}"></a>'.encode("ascii") not in chapter_bytes
    ]
    if missing_builtins:
        raise VerificationError(
            f"missing special built-in anchors: {', '.join(missing_builtins)}"
        )
    return set(SPECIAL_BUILTINS)


def read_ledger_keys(ledger_path: Path) -> set[str]:
    try:
        ledger_text = ledger_path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        raise VerificationError(
            f"cannot read ASCII ledger {ledger_path}: {error}"
        ) from error
    keys: list[str] = []
    saw_header = False
    for line in ledger_text.splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if not saw_header:
            if fields != ["utility", "state", "witness", "next_action"]:
                raise VerificationError("unexpected utility ledger header")
            saw_header = True
            continue
        keys.append(fields[0])
    if len(keys) != len(set(keys)):
        raise VerificationError("utility ledger contains duplicate keys")
    return set(keys)


def verify_utility_denominator(
    members: list[tarfile.TarInfo], chapter_bytes: bytes, ledger_path: Path
) -> None:
    member_names = {member.name for member in members}
    standalone_keys = derive_standalone_utility_keys(member_names)
    special_builtin_keys = verify_special_builtin_anchors(chapter_bytes)
    overlap = standalone_keys & special_builtin_keys
    if overlap:
        raise VerificationError(
            f"standalone and special built-in keys overlap: {', '.join(sorted(overlap))}"
        )
    official_keys = standalone_keys | special_builtin_keys
    if len(official_keys) != EXPECTED_FULL_COUNT:
        raise VerificationError(
            f"full utility count mismatch: expected {EXPECTED_FULL_COUNT}, "
            f"got {len(official_keys)}"
        )
    observed_sha256 = key_sha256(official_keys)
    if observed_sha256 != EXPECTED_FULL_KEY_SHA256:
        raise VerificationError(
            "full utility key hash mismatch: "
            f"expected {EXPECTED_FULL_KEY_SHA256}, got {observed_sha256}"
        )

    ledger_keys = read_ledger_keys(ledger_path)
    missing_ledger_keys = sorted(official_keys - ledger_keys)
    unexpected_ledger_keys = sorted(ledger_keys - official_keys)
    if missing_ledger_keys or unexpected_ledger_keys:
        raise VerificationError(
            "ledger differs from official utility keys: "
            f"missing={missing_ledger_keys}, unexpected={unexpected_ledger_keys}"
        )


def extract_archive(
    archive_path: Path, extract_root: Path, members: list[tarfile.TarInfo]
) -> Path:
    extract_root.mkdir(parents=True, exist_ok=True)
    destination = extract_root / ARCHIVE_TOP_DIRECTORY
    if destination.parent != extract_root or destination.name != ARCHIVE_TOP_DIRECTORY:
        raise VerificationError(f"unsafe extraction destination: {destination}")

    with tempfile.TemporaryDirectory(
        prefix=".susv4-2018-extract-", dir=extract_root
    ) as temporary_directory:
        temporary_path = Path(temporary_directory)
        try:
            with tarfile.open(archive_path, mode="r:gz") as archive:
                archive.extractall(temporary_path, members=members, filter="data")
        except (OSError, tarfile.TarError) as error:
            raise VerificationError(f"cannot extract archive: {error}") from error
        extracted_tree = temporary_path / ARCHIVE_TOP_DIRECTORY
        if not extracted_tree.is_dir():
            raise VerificationError(
                f"archive did not create {ARCHIVE_TOP_DIRECTORY} directory"
            )
        if destination.exists():
            shutil.rmtree(destination)
        os.replace(extracted_tree, destination)
    return destination


def expect_failure(
    name: str, action: Callable[[], None], expected_fragment: str
) -> None:
    try:
        action()
    except VerificationError as error:
        if expected_fragment not in str(error):
            raise AssertionError(
                f"{name}: expected {expected_fragment!r}, got {str(error)!r}"
            ) from error
    else:
        raise AssertionError(f"{name}: mutation unexpectedly passed")


def run_self_test(ledger_path: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="xinim-posix-issue7-") as temp_directory:
        archive_path = Path(temp_directory) / "identity.bin"
        archive_path.write_bytes(b"official archive identity")
        expected_hash = hashlib.sha256(archive_path.read_bytes()).hexdigest()
        verify_archive_identity(
            archive_path,
            expected_size=archive_path.stat().st_size,
            expected_sha256=expected_hash,
        )
        archive_path.write_bytes(b"official archive identitx")
        expect_failure(
            "archive hash mutation",
            lambda: verify_archive_identity(
                archive_path,
                expected_size=archive_path.stat().st_size,
                expected_sha256=expected_hash,
            ),
            "archive SHA-256 mismatch",
        )

    reference_standalone_keys = read_ledger_keys(ledger_path) - set(SPECIAL_BUILTINS)
    synthetic_names = {
        f"{ARCHIVE_TOP_DIRECTORY}/utilities/{key}.html"
        for key in reference_standalone_keys
    } | {f"{ARCHIVE_TOP_DIRECTORY}/utilities/{page}" for page in NON_UTILITY_HTML_PAGES}
    derive_standalone_utility_keys(synthetic_names)
    missing_page_names = synthetic_names - {
        f"{ARCHIVE_TOP_DIRECTORY}/utilities/admin.html"
    }
    expect_failure(
        "missing utility page",
        lambda: derive_standalone_utility_keys(missing_page_names),
        "standalone utility count mismatch",
    )
    unexpected_page_names = synthetic_names | {
        f"{ARCHIVE_TOP_DIRECTORY}/utilities/notposix.html"
    }
    expect_failure(
        "unexpected utility page",
        lambda: derive_standalone_utility_keys(unexpected_page_names),
        "standalone utility count mismatch",
    )

    chapter_bytes = b"\n".join(
        f'<a name="{builtin}"></a>'.encode("ascii") for builtin in SPECIAL_BUILTINS
    )
    verify_special_builtin_anchors(chapter_bytes)
    missing_anchor_bytes = chapter_bytes.replace(b'<a name="unset"></a>', b"", 1)
    expect_failure(
        "missing special built-in",
        lambda: verify_special_builtin_anchors(missing_anchor_bytes),
        "missing special built-in anchors: unset",
    )


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    ledger_path = resolve_path(repository_root, arguments.ledger)
    if arguments.self_test:
        run_self_test(ledger_path)
        print("POSIX Issue 7 archive verifier mutation self-test passed.")
        return 0

    archive_path = resolve_path(repository_root, arguments.archive)
    extract_root = resolve_path(repository_root, arguments.extract_root)
    if not archive_path.exists():
        print(
            f"POSIX Issue 7 archive is not locally retained: {archive_path}",
            file=sys.stderr,
        )
        return 77

    try:
        verify_archive_identity(archive_path)
        members, chapter_bytes = read_safe_members(archive_path)
        verify_utility_denominator(members, chapter_bytes, ledger_path)
        extracted_path = None
        if arguments.extract:
            extracted_path = extract_archive(archive_path, extract_root, members)
    except VerificationError as error:
        print(f"POSIX Issue 7 archive verification failed: {error}", file=sys.stderr)
        return 1

    print(
        "POSIX Issue 7 archive verified: "
        f"{ARCHIVE_MEMBER_COUNT} members, {EXPECTED_STANDALONE_COUNT} utility "
        f"pages, {len(SPECIAL_BUILTINS)} Chapter 2 special built-ins, "
        f"{EXPECTED_FULL_COUNT} ledger keys."
    )
    if extracted_path is not None:
        print(f"Extracted verified archive to {extracted_path}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
