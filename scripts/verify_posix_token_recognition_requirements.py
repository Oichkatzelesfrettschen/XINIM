#!/usr/bin/env python3
"""Verify the finite POSIX token-recognition requirement denominator."""

from __future__ import annotations

import argparse
import hashlib
import html
import pathlib
import re
import sys

from verify_posix_shell_language_ledger import (
    QEMU_SHELL_TEST_PATH,
    VerificationError,
    read_chapter_text,
    read_executable_case_ids,
    resolve_path,
    validate_case_group_witness,
)
from verify_posix_shell_grammar_requirements import (
    count_states as count_grammar_states,
    derive_source_rows as derive_grammar_source_rows,
    read_executable_case_ids as read_grammar_case_ids,
    validate_requirements_text as validate_grammar_requirements_text,
)

EXPECTED_COLUMNS = (
    "requirement",
    "title",
    "source_sha256",
    "state",
    "witness",
    "next_action",
)
EXPECTED_REQUIREMENTS = (
    ("tag_18_03.p01_line_input_modes", "Line input and parsing modes"),
    ("tag_18_03.p02_here_document_mode", "Here-document mode trigger"),
    ("tag_18_03.p03_first_applicable_rule", "First-applicable token rule"),
    ("tag_18_03.rule01_end_of_input", "End of input"),
    ("tag_18_03.rule02_operator_continuation", "Operator continuation"),
    ("tag_18_03.rule03_operator_delimitation", "Operator delimitation"),
    ("tag_18_03.rule04_quoted_text", "Quoted text"),
    ("tag_18_03.rule05_substitution_recursion", "Substitution recursion"),
    ("tag_18_03.rule06_operator_start", "Operator start"),
    ("tag_18_03.rule07_blank_delimitation", "Blank delimitation"),
    ("tag_18_03.rule08_word_continuation", "Word continuation"),
    ("tag_18_03.rule09_comment", "Comment"),
    ("tag_18_03.rule10_word_start", "Word start"),
    ("tag_18_03.p04_grammar_categorization", "Grammar categorization"),
    (
        "tag_18_03_01.a01_command_name_substitution",
        "Command-name alias substitution",
    ),
    ("tag_18_03_01.a02_trailing_blank_chain", "Trailing-blank alias chain"),
    (
        "tag_18_03_01.a03_environment_noninheritance",
        "Alias environment noninheritance",
    ),
)
EXPECTED_COUNT = len(EXPECTED_REQUIREMENTS)
EXPECTED_SOURCE_SHA256 = (
    "09e8a0cb31d41033af878004b9096a68cce6bc30ccf8962a27d343c7a90c50c5"
)
VALID_STATES = {"open", "closed"}
GRAMMAR_CATEGORIZATION_REQUIREMENT = "tag_18_03.p04_grammar_categorization"
GRAMMAR_CASE_GROUP = "SHELL_GRAMMAR_COMMAND_CASES"
GRAMMAR_CASE_PREFIX = "tag_18_10_"
TAG_PATTERN = re.compile(r"<[^>]+>")
PARAGRAPH_PATTERN = re.compile(r"<p>(.*?)</p>", re.IGNORECASE | re.DOTALL)
RULE_PATTERN = re.compile(r"<li>\s*<p>(.*?)</p>\s*</li>", re.IGNORECASE | re.DOTALL)


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
        default=pathlib.Path("docs/posix/posix_token_recognition_requirements.tsv"),
    )
    parser.add_argument(
        "--grammar-requirements",
        type=pathlib.Path,
        default=pathlib.Path("docs/posix/posix_shell_grammar_requirements.tsv"),
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def normalize_source_text(raw_text: str) -> str:
    normalized = " ".join(html.unescape(TAG_PATTERN.sub("", raw_text)).split())
    try:
        normalized.encode("ascii")
    except UnicodeEncodeError as error:
        raise VerificationError(
            f"non-ASCII token-recognition source text: {normalized!r}"
        ) from error
    return normalized


def split_once(source_text: str, marker: str, description: str) -> tuple[str, str]:
    before_marker, separator, after_marker = source_text.partition(marker)
    if not separator:
        raise VerificationError(
            f"missing token-recognition source marker: {description}"
        )
    return before_marker, after_marker


def derive_source_rows(chapter_text: str) -> list[tuple[str, str, str]]:
    _before_section, token_section = split_once(
        chapter_text, '<h3><a name="tag_18_03">', "tag_18_03"
    )
    token_section, _after_section = split_once(
        token_section, '<h3><a name="tag_18_04">', "tag_18_04"
    )
    ordinary_section, alias_section = split_once(
        token_section, '<h4><a name="tag_18_03_01">', "tag_18_03_01"
    )
    ordinary_preamble, ordered_rules_and_tail = split_once(
        ordinary_section, "<ol>", "token rule list start"
    )
    ordered_rules, ordinary_tail = split_once(
        ordered_rules_and_tail, "</ol>", "token rule list end"
    )

    raw_requirements = [
        *PARAGRAPH_PATTERN.findall(ordinary_preamble),
        *RULE_PATTERN.findall(ordered_rules),
        *PARAGRAPH_PATTERN.findall(ordinary_tail),
        *PARAGRAPH_PATTERN.findall(alias_section),
    ]
    if len(raw_requirements) != EXPECTED_COUNT:
        raise VerificationError(
            "token-recognition source count mismatch: "
            f"expected {EXPECTED_COUNT}, got {len(raw_requirements)}"
        )

    source_rows: list[tuple[str, str, str]] = []
    hash_lines: list[str] = []
    for (requirement, title), raw_requirement in zip(
        EXPECTED_REQUIREMENTS, raw_requirements, strict=True
    ):
        normalized = normalize_source_text(raw_requirement)
        source_sha256 = hashlib.sha256(normalized.encode("ascii")).hexdigest()
        source_rows.append((requirement, title, source_sha256))
        hash_lines.append(f"{requirement}\t{source_sha256}")

    observed_source_sha256 = hashlib.sha256(
        "".join(f"{line}\n" for line in hash_lines).encode("ascii")
    ).hexdigest()
    if observed_source_sha256 != EXPECTED_SOURCE_SHA256:
        raise VerificationError(
            "token-recognition source hash mismatch: "
            f"expected {EXPECTED_SOURCE_SHA256}, got {observed_source_sha256}"
        )
    return source_rows


def validate_closed_witness(
    requirement: str,
    witness: str,
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    if requirement == GRAMMAR_CATEGORIZATION_REQUIREMENT:
        return validate_case_group_witness(
            requirement,
            witness,
            repository_root,
            GRAMMAR_CASE_GROUP,
            GRAMMAR_CASE_PREFIX,
        )
    if not witness.startswith("tests:"):
        return [f"{requirement}: closed witness must start with 'tests:'"]
    witness_references = witness.removeprefix("tests:").split(",")
    failures: list[str] = []
    for witness_reference in witness_references:
        relative_path, separator, case_id = witness_reference.partition("#")
        candidate = pathlib.PurePosixPath(relative_path)
        if (
            not separator
            or not relative_path
            or not case_id
            or candidate.is_absolute()
            or ".." in candidate.parts
        ):
            failures.append(
                f"{requirement}: invalid witness reference: {witness_reference}"
            )
            continue
        if candidate != QEMU_SHELL_TEST_PATH:
            failures.append(f"{requirement}: witness must use {QEMU_SHELL_TEST_PATH}")
            continue
        if not (repository_root / candidate).is_file():
            failures.append(f"{requirement}: missing witness file: {relative_path}")
        if not case_id.startswith(f"{requirement}."):
            failures.append(
                f"{requirement}: witness case ID belongs to another requirement"
            )
        if case_id not in executable_case_ids:
            failures.append(f"{requirement}: unknown executable case ID: {case_id}")
    return failures


def validate_requirements_text(
    requirements_text: str,
    source_rows: list[tuple[str, str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    rows: list[tuple[str, str, str, str, str, str]] = []
    saw_header = False
    for line_number, line in enumerate(requirements_text.splitlines(), start=1):
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
            failures.append(
                f"line {line_number}: expected {len(EXPECTED_COLUMNS)} tab fields"
            )
            continue
        rows.append(fields)

    if not saw_header:
        return failures + ["missing requirements header"]
    requirements = [row[0] for row in rows]
    duplicate_requirements = sorted(
        {
            requirement
            for requirement in requirements
            if requirements.count(requirement) > 1
        }
    )
    if duplicate_requirements:
        failures.append(
            f"duplicate token requirements: {', '.join(duplicate_requirements)}"
        )
    if len(rows) != EXPECTED_COUNT:
        failures.append(
            f"requirements count mismatch: expected {EXPECTED_COUNT}, got {len(rows)}"
        )

    observed_source_rows = [
        (requirement, title, source_sha256)
        for requirement, title, source_sha256, *_rest in rows
    ]
    if observed_source_rows != source_rows:
        failures.append("token-recognition source rows differ from official source")

    open_requirements: set[str] = set()
    closed_requirements: set[str] = set()
    for requirement, _title, _source_sha256, state, witness, next_action in rows:
        if state not in VALID_STATES:
            failures.append(f"{requirement}: invalid state: {state}")
            continue
        if state == "open":
            open_requirements.add(requirement)
            if not witness.startswith("missing:") or requirement not in witness:
                failures.append(
                    f"{requirement}: open witness must name its missing prerequisite"
                )
            if not next_action or next_action == "-":
                failures.append(f"{requirement}: open row lacks a next action")
        else:
            closed_requirements.add(requirement)
            failures.extend(
                validate_closed_witness(
                    requirement, witness, repository_root, executable_case_ids
                )
            )
            if next_action != "-":
                failures.append(f"{requirement}: closed row next_action must be '-'")

    if open_requirements & closed_requirements:
        failures.append("open and closed token requirements overlap")
    if open_requirements | closed_requirements != set(requirements):
        failures.append("token requirement partitions do not cover the denominator")
    return failures


def require_failure(
    name: str,
    requirements_text: str,
    expected_fragment: str,
    source_rows: list[tuple[str, str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> None:
    failures = validate_requirements_text(
        requirements_text, source_rows, repository_root, executable_case_ids
    )
    if not any(expected_fragment in failure for failure in failures):
        raise AssertionError(
            f"{name}: expected failure containing {expected_fragment!r}, got {failures}"
        )


def requirement_state(requirements_text: str, requirement: str) -> str | None:
    for line in requirements_text.splitlines():
        if not line or line.startswith(("#", "requirement\t")):
            continue
        fields = line.split("\t")
        if len(fields) == len(EXPECTED_COLUMNS) and fields[0] == requirement:
            return fields[3]
    return None


def validate_grammar_dependency(
    requirements_text: str, grammar_open_count: int
) -> list[str]:
    grammar_requirement = GRAMMAR_CATEGORIZATION_REQUIREMENT
    if (
        requirement_state(requirements_text, grammar_requirement) == "closed"
        and grammar_open_count != 0
    ):
        return [
            f"{grammar_requirement}: cannot close while "
            f"{grammar_open_count} shell grammar requirements remain open"
        ]
    return []


def run_self_test(
    requirements_path: pathlib.Path,
    chapter_text: str,
    source_rows: list[tuple[str, str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> None:
    original = requirements_path.read_text(encoding="ascii")
    lines = original.splitlines()
    header_index = lines.index("\t".join(EXPECTED_COLUMNS))
    first_row_index = header_index + 1

    duplicate_lines = lines.copy()
    duplicate_lines.insert(first_row_index + 1, duplicate_lines[first_row_index])
    require_failure(
        "duplicate requirement",
        "\n".join(duplicate_lines) + "\n",
        "duplicate token requirements",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    missing_lines = lines.copy()
    del missing_lines[first_row_index]
    require_failure(
        "missing requirement",
        "\n".join(missing_lines) + "\n",
        "requirements count mismatch",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    changed_source_lines = lines.copy()
    changed_source_fields = changed_source_lines[first_row_index].split("\t")
    changed_source_fields[2] = "0" * 64
    changed_source_lines[first_row_index] = "\t".join(changed_source_fields)
    require_failure(
        "changed source hash",
        "\n".join(changed_source_lines) + "\n",
        "source rows differ",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    false_closed_lines = lines.copy()
    false_closed_fields = false_closed_lines[first_row_index].split("\t")
    false_closed_fields[3] = "closed"
    false_closed_fields[4] = f"missing:{false_closed_fields[0]}"
    false_closed_fields[5] = "-"
    false_closed_lines[first_row_index] = "\t".join(false_closed_fields)
    require_failure(
        "false closure",
        "\n".join(false_closed_lines) + "\n",
        "closed witness must start with 'tests:'",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    unknown_case_lines = lines.copy()
    unknown_case_fields = unknown_case_lines[first_row_index].split("\t")
    unknown_case_fields[3] = "closed"
    unknown_case_fields[4] = (
        f"tests:{QEMU_SHELL_TEST_PATH}#{unknown_case_fields[0]}.missing_executable_case"
    )
    unknown_case_fields[5] = "-"
    unknown_case_lines[first_row_index] = "\t".join(unknown_case_fields)
    require_failure(
        "unknown executable case",
        "\n".join(unknown_case_lines) + "\n",
        "unknown executable case ID",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    try:
        derive_source_rows(
            chapter_text.replace(
                "If the end of input is recognized",
                "If an altered end of input is recognized",
                1,
            )
        )
    except VerificationError as error:
        if "source hash mismatch" not in str(error):
            raise AssertionError(
                f"source mutation failed for the wrong reason: {error}"
            ) from error
    else:
        raise AssertionError("altered token-recognition source unexpectedly passed")

    dependency_lines = lines.copy()
    grammar_requirement = GRAMMAR_CATEGORIZATION_REQUIREMENT
    dependency_index = next(
        index
        for index, line in enumerate(dependency_lines)
        if line.startswith(f"{grammar_requirement}\t")
    )
    dependency_fields = dependency_lines[dependency_index].split("\t")
    dependency_fields[3] = "closed"
    dependency_lines[dependency_index] = "\t".join(dependency_fields)
    dependency_failures = validate_grammar_dependency(
        "\n".join(dependency_lines) + "\n", 1
    )
    if not any(
        "shell grammar requirements remain open" in failure
        for failure in dependency_failures
    ):
        raise AssertionError(
            "grammar dependency mutation did not reject premature p04 closure"
        )


def count_states(requirements_path: pathlib.Path) -> tuple[int, int]:
    open_count = 0
    closed_count = 0
    for line in requirements_path.read_text(encoding="ascii").splitlines():
        if not line or line.startswith(("#", "requirement\t")):
            continue
        state = line.split("\t")[3]
        if state == "open":
            open_count += 1
        elif state == "closed":
            closed_count += 1
    return open_count, closed_count


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive).resolve()
    requirements_path = resolve_path(repository_root, arguments.requirements).resolve()
    grammar_requirements_path = resolve_path(
        repository_root, arguments.grammar_requirements
    ).resolve()
    if not archive_path.is_file():
        print(f"SKIP: POSIX Issue 7 archive not found: {archive_path}")
        return 77
    try:
        chapter_text = read_chapter_text(archive_path)
        source_rows = derive_source_rows(chapter_text)
        executable_case_ids = read_executable_case_ids(repository_root)
        requirements_text = requirements_path.read_text(encoding="ascii")
        failures = validate_requirements_text(
            requirements_text, source_rows, repository_root, executable_case_ids
        )
        grammar_source_rows = derive_grammar_source_rows(chapter_text)
        grammar_case_ids = read_grammar_case_ids(repository_root)
        grammar_requirements_text = grammar_requirements_path.read_text(
            encoding="ascii"
        )
        grammar_failures = validate_grammar_requirements_text(
            grammar_requirements_text,
            grammar_source_rows,
            repository_root,
            grammar_case_ids,
        )
        failures.extend(
            f"shell grammar dependency: {failure}" for failure in grammar_failures
        )
        grammar_open_count, _grammar_closed_count = count_grammar_states(
            grammar_requirements_path
        )
        failures.extend(
            validate_grammar_dependency(requirements_text, grammar_open_count)
        )
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(
                requirements_path,
                chapter_text,
                source_rows,
                repository_root,
                executable_case_ids,
            )
            print("POSIX token-recognition mutation self-test passed.")
            return 0
        open_count, closed_count = count_states(requirements_path)
        print(
            "POSIX token-recognition requirements verified: "
            f"{EXPECTED_COUNT} rows, {closed_count} closed, {open_count} open."
        )
        return 0
    except (OSError, UnicodeDecodeError, VerificationError, AssertionError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
