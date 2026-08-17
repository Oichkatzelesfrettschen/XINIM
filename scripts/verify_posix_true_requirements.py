#!/usr/bin/env python3
"""Derive and verify the finite Issue 7 requirements for true."""

from __future__ import annotations

import argparse
import ast
import hashlib
import html
import pathlib
import re
import sys
import tarfile
from dataclasses import dataclass

from verify_posix_issue7_archive import (
    ARCHIVE_TOP_DIRECTORY,
    VerificationError as ArchiveVerificationError,
    resolve_path,
    verify_archive_identity,
)

ARCHIVE_MEMBER = f"{ARCHIVE_TOP_DIRECTORY}/utilities/true.html"
QEMU_TEST_PATH = pathlib.PurePosixPath("test/boot/x86_64_shell_test.py")
EXPECTED_SOURCE_SET_SHA256 = (
    "cc0afbaeecb841c6257a7470e37ecf0a14cb3d25afedf12f801ae244130a35ff"
)
EXPECTED_COLUMNS = (
    "requirement",
    "title",
    "source_ref",
    "source_sha256",
    "applicability",
    "state",
    "witness",
    "next_action",
)
PARAGRAPH_PATTERN = re.compile(
    r"<p(?: [^>]*)?>(.*?)</p>", re.IGNORECASE | re.DOTALL
)
TAG_PATTERN = re.compile(r"<[^>]+>")


@dataclass(frozen=True)
class RequirementSpec:
    identifier: str
    title: str
    selector: int
    applicability: str
    witness_case: str | None


REQUIREMENTS = (
    RequirementSpec(
        "true.description.exit_zero",
        "Return true with exit code zero",
        1,
        "required",
        "true.success",
    ),
    RequirementSpec(
        "true.options.none", "No utility-specific options", 2, "none", None
    ),
    RequirementSpec("true.operands.none", "No operands", 3, "none", None),
    RequirementSpec(
        "true.stdin.unused", "Standard input is not used", 4, "required", "true.stdin_unused"
    ),
    RequirementSpec("true.input_files.none", "No input files", 5, "none", None),
    RequirementSpec("true.environment.none", "No environment variables", 6, "none", None),
    RequirementSpec(
        "true.async.default", "Default asynchronous-event behavior", 7, "required", "true.success"
    ),
    RequirementSpec(
        "true.stdout.unused", "Standard output is not used", 8, "required", "true.no_output"
    ),
    RequirementSpec(
        "true.stderr.unused", "Standard error is not used", 9, "required", "true.no_output"
    ),
    RequirementSpec("true.output_files.none", "No output files", 10, "none", None),
    RequirementSpec("true.extended.none", "No extended description", 11, "none", None),
    RequirementSpec(
        "true.exit.zero", "Zero exit status", 12, "required", "true.success"
    ),
    RequirementSpec("true.errors.none", "No consequences of errors", 13, "none", None),
)


class VerificationError(RuntimeError):
    """Report a true source or requirement contract failure."""


def parse_args() -> argparse.Namespace:
    repository_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=repository_root)
    parser.add_argument(
        "--archive",
        type=pathlib.Path,
        default=pathlib.Path("data/external/posix/susv4-2018.tgz"),
    )
    parser.add_argument(
        "--requirements",
        type=pathlib.Path,
        default=pathlib.Path("docs/posix/true_requirements.tsv"),
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def normalize(raw_text: str) -> str:
    normalized = " ".join(html.unescape(TAG_PATTERN.sub("", raw_text)).split())
    try:
        normalized.encode("ascii")
    except UnicodeEncodeError as error:
        raise VerificationError(f"non-ASCII true source fragment: {normalized!r}") from error
    return normalized


def read_source(archive_path: pathlib.Path) -> str:
    try:
        verify_archive_identity(archive_path)
        with tarfile.open(archive_path, mode="r:gz") as archive:
            member = archive.extractfile(ARCHIVE_MEMBER)
            if member is None:
                raise VerificationError(f"missing archive member: {ARCHIVE_MEMBER}")
            return member.read().decode("latin-1")
    except (OSError, tarfile.TarError, ArchiveVerificationError) as error:
        raise VerificationError(f"cannot read {ARCHIVE_MEMBER}: {error}") from error


def source_paragraphs(source_text: str) -> list[str]:
    paragraphs = [normalize(fragment) for fragment in PARAGRAPH_PATTERN.findall(source_text)]
    if len(paragraphs) != 22:
        raise VerificationError(
            f"true paragraph count mismatch: expected 22, got {len(paragraphs)}"
        )
    return paragraphs


def source_set_hash(rows: list[tuple[str, ...]]) -> str:
    hash_lines = [f"{row[0]}\t{row[2]}\t{row[3]}" for row in rows]
    return hashlib.sha256("".join(f"{line}\n" for line in hash_lines).encode("ascii")).hexdigest()


def derive_rows(source_text: str) -> list[tuple[str, ...]]:
    paragraphs = source_paragraphs(source_text)
    rows: list[tuple[str, ...]] = []
    for requirement in REQUIREMENTS:
        source_fragment = paragraphs[requirement.selector]
        source_ref = f"{ARCHIVE_MEMBER}#up:{requirement.selector}"
        source_sha256 = hashlib.sha256(source_fragment.encode("ascii")).hexdigest()
        if requirement.applicability == "required":
            if requirement.witness_case is None:
                raise VerificationError(f"required true row lacks witness: {requirement.identifier}")
            state = "closed"
            witness = f"tests:{QEMU_TEST_PATH}#{requirement.witness_case}"
        else:
            state = "excluded"
            witness = "scope:none"
        rows.append(
            (
                requirement.identifier,
                requirement.title,
                source_ref,
                source_sha256,
                requirement.applicability,
                state,
                witness,
                "-",
            )
        )
    observed_hash = source_set_hash(rows)
    if observed_hash != EXPECTED_SOURCE_SET_SHA256:
        raise VerificationError(
            "true source-set hash mismatch: "
            f"expected {EXPECTED_SOURCE_SET_SHA256}, got {observed_hash}"
        )
    return rows


def read_case_ids(repository_root: pathlib.Path) -> set[str]:
    test_path = repository_root / QEMU_TEST_PATH
    try:
        test_text = test_path.read_text(encoding="ascii")
        syntax_tree = ast.parse(test_text, filename=str(test_path))
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise VerificationError(f"cannot parse true Q35 test: {error}") from error
    assignment = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.Assign)
            and any(
                isinstance(target, ast.Name)
                and target.id == "TRUE_UTILITY_COMMAND_CASES"
                for target in statement.targets
            )
        ),
        None,
    )
    if assignment is None:
        raise VerificationError("missing TRUE_UTILITY_COMMAND_CASES")
    try:
        cases = ast.literal_eval(assignment.value)
    except (ValueError, TypeError) as error:
        raise VerificationError("true Q35 cases are not literals") from error
    if not isinstance(cases, tuple):
        raise VerificationError("true Q35 cases must be a tuple")
    case_ids: list[str] = []
    for case in cases:
        if (
            not isinstance(case, tuple)
            or len(case) != 3
            or not all(isinstance(field, str) and field for field in case)
        ):
            raise VerificationError("each true Q35 case needs an ID, command, and output")
        case_ids.append(case[0])
    duplicates = sorted({case_id for case_id in case_ids if case_ids.count(case_id) > 1})
    if duplicates:
        raise VerificationError(f"duplicate true Q35 cases: {', '.join(duplicates)}")
    if "require_true_utility_command_cases(sock)" not in test_text:
        raise VerificationError("true Q35 case runner is not invoked")
    return set(case_ids)


def parse_ledger(ledger_text: str) -> tuple[str | None, list[tuple[str, ...]]]:
    source_hash = None
    rows: list[tuple[str, ...]] = []
    saw_header = False
    for line_number, line in enumerate(ledger_text.splitlines(), start=1):
        if line.startswith("# ordered-source-sha256:"):
            source_hash = line.partition(":")[2].strip()
            continue
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        if not saw_header:
            saw_header = True
            if fields != EXPECTED_COLUMNS:
                raise VerificationError(f"line {line_number}: true requirements header mismatch")
            continue
        if len(fields) != len(EXPECTED_COLUMNS):
            raise VerificationError(f"line {line_number}: true requirement row width mismatch")
        rows.append(fields)
    if not saw_header:
        raise VerificationError("true requirements ledger is missing its header")
    return source_hash, rows


def validate_ledger(
    ledger_text: str,
    expected_rows: list[tuple[str, ...]],
    repository_root: pathlib.Path,
    case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    try:
        observed_source_hash, observed_rows = parse_ledger(ledger_text)
    except VerificationError as error:
        return [str(error)]
    if observed_source_hash != EXPECTED_SOURCE_SET_SHA256:
        failures.append(
            "true requirements source hash mismatch: "
            f"expected {EXPECTED_SOURCE_SET_SHA256}, got {observed_source_hash}"
        )
    if observed_rows != expected_rows:
        failures.append("true requirements rows do not match the pinned source derivation")
        for row_number, (expected, observed) in enumerate(
            zip(expected_rows, observed_rows), start=1
        ):
            if expected != observed:
                failures.append(
                    f"true requirement row {row_number} differs: expected {expected[0]}, "
                    f"got {observed[0] if observed else '<missing>'}"
                )
                break
        if len(observed_rows) != len(expected_rows):
            failures.append(
                f"true requirements count mismatch: expected {len(expected_rows)}, "
                f"got {len(observed_rows)}"
            )
    for row in observed_rows:
        if row[4] == "required":
            if row[5] != "closed":
                failures.append(f"required true row is not closed: {row[0]}")
            witness_path, separator, case_id = row[6].partition("#")
            if (
                not separator
                or witness_path != f"tests:{QEMU_TEST_PATH}"
                or case_id not in case_ids
            ):
                failures.append(f"required true row has no executable Q35 witness: {row[0]}")
        elif row[5] != "excluded":
            failures.append(f"non-required true row has invalid state: {row[0]}")
        if row[6].startswith("tests:"):
            witness_path = pathlib.PurePosixPath(row[6].removeprefix("tests:").partition("#")[0])
            if witness_path.is_absolute() or ".." in witness_path.parts:
                failures.append(f"true row has unsafe witness path: {row[0]}")
            elif not (repository_root / witness_path).is_file():
                failures.append(f"true row has missing witness path: {row[0]}")
    return failures


def run_self_test(
    ledger_text: str,
    expected_rows: list[tuple[str, ...]],
    repository_root: pathlib.Path,
    case_ids: set[str],
) -> None:
    failures = validate_ledger(ledger_text, expected_rows, repository_root, case_ids)
    if failures:
        raise AssertionError(f"valid true requirements ledger failed: {failures}")
    rows = ledger_text.splitlines()
    required_line = next(
        index
        for index, line in enumerate(rows)
        if line.startswith("true.description.exit_zero\t")
    )
    fields = rows[required_line].split("\t")
    fields[5] = "open"
    rows[required_line] = "\t".join(fields)
    mutated_failures = validate_ledger("\n".join(rows), expected_rows, repository_root, case_ids)
    if not any("required true row is not closed" in failure for failure in mutated_failures):
        raise AssertionError(f"true requirement state mutation was not rejected: {mutated_failures}")
    mutated_hash = ledger_text.replace(EXPECTED_SOURCE_SET_SHA256, "0" * 64, 1)
    hash_failures = validate_ledger(mutated_hash, expected_rows, repository_root, case_ids)
    if not any("source hash mismatch" in failure for failure in hash_failures):
        raise AssertionError(f"true requirement hash mutation was not rejected: {hash_failures}")
    print("true requirement ledger mutation self-test passed.")


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive)
    requirements_path = resolve_path(repository_root, arguments.requirements)
    try:
        expected_rows = derive_rows(read_source(archive_path))
        case_ids = read_case_ids(repository_root)
        ledger_text = requirements_path.read_text(encoding="ascii")
        failures = validate_ledger(ledger_text, expected_rows, repository_root, case_ids)
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(ledger_text, expected_rows, repository_root, case_ids)
            return 0
    except (OSError, UnicodeDecodeError, VerificationError, AssertionError) as error:
        print(f"true requirement verification failed: {error}", file=sys.stderr)
        return 1
    required_count = sum(row[4] == "required" for row in expected_rows)
    print(
        "true requirement ledger verified: "
        f"{len(expected_rows)} source rows, {required_count} required rows closed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
