#!/usr/bin/env python3
"""Verify the finite POSIX Issue 7 Shell Command Language denominator."""

from __future__ import annotations

import argparse
import ast
import hashlib
import html
import pathlib
import re
import sys
import tarfile

from verify_posix_issue7_archive import (
    ARCHIVE_NAME,
    ARCHIVE_TOP_DIRECTORY,
    verify_archive_identity,
)

EXPECTED_COLUMNS = ("anchor", "title", "state", "witness", "next_action")
EXPECTED_COUNT = 70
EXPECTED_KEY_SHA256 = "94198e92e4559a4919ea1f481341e474980b1472c0b66732c906673688f7f5af"
EXPECTED_SOURCE_ROW_SHA256 = (
    "f9be211b16d1e77a9198419d0d85b56d3ca9756ccc6988406c52b9522f737493"
)
VALID_STATES = {"open", "closed"}
QEMU_SHELL_TEST_PATH = pathlib.PurePosixPath("test/boot/x86_64_shell_test.py")
TOKEN_REQUIREMENTS_PATH = pathlib.PurePosixPath(
    "docs/posix/posix_token_recognition_requirements.tsv"
)
GRAMMAR_REQUIREMENTS_PATH = pathlib.PurePosixPath(
    "docs/posix/posix_shell_grammar_requirements.tsv"
)
CASE_GROUP_RUNNERS = {
    "SHELL_LANGUAGE_COMMAND_CASES": "require_shell_language_command_cases",
    "SHELL_GRAMMAR_COMMAND_CASES": "require_shell_grammar_command_cases",
}
PARENT_GROUP_WITNESSES = {
    "tag_18_03": ("SHELL_LANGUAGE_COMMAND_CASES", "tag_18_03"),
    "tag_18_10": ("SHELL_GRAMMAR_COMMAND_CASES", "tag_18_10_"),
    "tag_18_10_01": ("SHELL_GRAMMAR_COMMAND_CASES", "tag_18_10_01."),
    "tag_18_10_02": ("SHELL_GRAMMAR_COMMAND_CASES", "tag_18_10_02."),
}
ANCHOR_PATTERN = re.compile(r"tag_18_[0-9]+(?:_[0-9]+)*")
TAG_PATTERN = re.compile(r"<[^>]+>")
HEADING_PATTERN = re.compile(
    r'<h(?P<level>[2-5])>\s*<a name="(?P<anchor>tag_18_[0-9_]+)">'
    r"(?P<section>.*?)</a>(?P<title>.*?)</h(?P=level)>",
    re.IGNORECASE | re.DOTALL,
)


class VerificationError(RuntimeError):
    """Report a shell-language source or ledger contract failure."""


def parse_args() -> argparse.Namespace:
    repository_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=repository_root)
    parser.add_argument(
        "--archive",
        type=pathlib.Path,
        default=pathlib.Path("data/external/posix") / ARCHIVE_NAME,
    )
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        default=pathlib.Path("docs/posix/posix_shell_language_ledger.tsv"),
    )
    parser.add_argument(
        "--token-requirements",
        type=pathlib.Path,
        default=TOKEN_REQUIREMENTS_PATH,
    )
    parser.add_argument(
        "--grammar-requirements",
        type=pathlib.Path,
        default=GRAMMAR_REQUIREMENTS_PATH,
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(
    repository_root: pathlib.Path, candidate: pathlib.Path
) -> pathlib.Path:
    return candidate if candidate.is_absolute() else repository_root / candidate


def ordered_sha256(lines: list[str]) -> str:
    return hashlib.sha256(
        "".join(f"{line}\n" for line in lines).encode("ascii")
    ).hexdigest()


def normalize_heading(raw_heading: str) -> str:
    normalized = " ".join(html.unescape(TAG_PATTERN.sub("", raw_heading)).split())
    try:
        normalized.encode("ascii")
    except UnicodeEncodeError as error:
        raise VerificationError(
            f"non-ASCII Chapter 2 heading: {normalized!r}"
        ) from error
    return normalized


def derive_source_rows(chapter_text: str) -> list[tuple[str, str]]:
    source_rows: list[tuple[str, str]] = []
    for match in HEADING_PATTERN.finditer(chapter_text):
        anchor = match.group("anchor")
        first_section = int(anchor.removeprefix("tag_18_").split("_", maxsplit=1)[0])
        if first_section > 14:
            continue
        title = normalize_heading(match.group("title"))
        if title == "Examples":
            continue
        source_rows.append((anchor, title))

    anchors = [anchor for anchor, _title in source_rows]
    duplicate_anchors = sorted(
        {anchor for anchor in anchors if anchors.count(anchor) > 1}
    )
    if duplicate_anchors:
        raise VerificationError(
            f"duplicate Chapter 2 anchors: {', '.join(duplicate_anchors)}"
        )
    if len(source_rows) != EXPECTED_COUNT:
        raise VerificationError(
            "source denominator count mismatch: "
            f"expected {EXPECTED_COUNT}, got {len(source_rows)}"
        )
    observed_key_hash = ordered_sha256(anchors)
    if observed_key_hash != EXPECTED_KEY_SHA256:
        raise VerificationError(
            "source anchor hash mismatch: "
            f"expected {EXPECTED_KEY_SHA256}, got {observed_key_hash}"
        )
    source_lines = [f"{anchor}\t{title}" for anchor, title in source_rows]
    observed_source_hash = ordered_sha256(source_lines)
    if observed_source_hash != EXPECTED_SOURCE_ROW_SHA256:
        raise VerificationError(
            "source anchor-title hash mismatch: "
            f"expected {EXPECTED_SOURCE_ROW_SHA256}, got {observed_source_hash}"
        )
    return source_rows


def read_chapter_text(archive_path: pathlib.Path) -> str:
    verify_archive_identity(archive_path)
    chapter_member = f"{ARCHIVE_TOP_DIRECTORY}/utilities/V3_chap02.html"
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            chapter_file = archive.extractfile(chapter_member)
            if chapter_file is None:
                raise VerificationError(f"missing archive member: {chapter_member}")
            chapter_bytes = chapter_file.read()
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot read {chapter_member}: {error}") from error
    return chapter_bytes.decode("latin-1")


def read_executable_case_groups(
    repository_root: pathlib.Path,
) -> dict[str, set[str]]:
    test_path = repository_root / QEMU_SHELL_TEST_PATH
    try:
        test_text = test_path.read_text(encoding="ascii")
        syntax_tree = ast.parse(test_text, filename=str(test_path))
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise VerificationError(
            f"cannot parse ASCII shell test {test_path}: {error}"
        ) from error

    main_function = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.FunctionDef) and statement.name == "main"
        ),
        None,
    )
    if main_function is None:
        raise VerificationError(
            f"{QEMU_SHELL_TEST_PATH}: missing main function"
        )

    case_groups: dict[str, set[str]] = {}
    all_case_ids: list[str] = []
    for group_name, runner_name in CASE_GROUP_RUNNERS.items():
        case_assignment = next(
            (
                statement
                for statement in syntax_tree.body
                if isinstance(statement, ast.Assign)
                and any(
                    isinstance(target, ast.Name) and target.id == group_name
                    for target in statement.targets
                )
            ),
            None,
        )
        if case_assignment is None:
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: missing {group_name}"
            )
        try:
            command_cases = ast.literal_eval(case_assignment.value)
        except (ValueError, TypeError) as error:
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: {group_name} is not an ASCII literal"
            ) from error
        if not isinstance(command_cases, tuple):
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: {group_name} must be a tuple"
            )

        group_case_ids: list[str] = []
        for case_index, command_case in enumerate(command_cases, start=1):
            if (
                not isinstance(command_case, tuple)
                or len(command_case) != 3
                or not all(isinstance(field, str) for field in command_case)
            ):
                raise VerificationError(
                    f"{QEMU_SHELL_TEST_PATH}: {group_name} case {case_index} "
                    "must contain an ID, command, and expected output"
                )
            case_id, command, expected_output = command_case
            if not case_id or not command or not expected_output:
                raise VerificationError(
                    f"{QEMU_SHELL_TEST_PATH}: {group_name} case {case_index} "
                    "contains an empty field"
                )
            try:
                case_id.encode("ascii")
                command.encode("ascii")
                expected_output.encode("ascii")
            except UnicodeEncodeError as error:
                raise VerificationError(
                    f"{QEMU_SHELL_TEST_PATH}: case {case_id!r} is not ASCII"
                ) from error
            group_case_ids.append(case_id)

        runner_function = next(
            (
                statement
                for statement in syntax_tree.body
                if isinstance(statement, ast.FunctionDef)
                and statement.name == runner_name
            ),
            None,
        )
        if runner_function is None or not any(
            isinstance(node, ast.Name)
            and node.id == group_name
            and isinstance(node.ctx, ast.Load)
            for node in ast.walk(runner_function)
        ):
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: {group_name} runner is missing"
            )
        if not any(
            isinstance(node, ast.Call)
            and isinstance(node.func, ast.Name)
            and node.func.id == runner_name
            for node in ast.walk(main_function)
        ):
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: main does not execute {group_name}"
            )
        case_groups[group_name] = set(group_case_ids)
        all_case_ids.extend(group_case_ids)

    duplicate_case_ids = sorted(
        {case_id for case_id in all_case_ids if all_case_ids.count(case_id) > 1}
    )
    if duplicate_case_ids:
        raise VerificationError(
            f"duplicate executable shell case IDs: {', '.join(duplicate_case_ids)}"
        )
    return case_groups


def read_executable_case_ids(repository_root: pathlib.Path) -> set[str]:
    return set().union(*read_executable_case_groups(repository_root).values())


def validate_case_group_witness(
    owner: str,
    witness: str,
    repository_root: pathlib.Path,
    expected_group: str,
    expected_prefix: str,
) -> list[str]:
    if not witness.startswith("test-group:"):
        return [f"{owner}: parent witness must start with 'test-group:'"]
    reference = witness.removeprefix("test-group:")
    relative_path, separator, selector = reference.partition("#")
    group_name, prefix_separator, case_prefix = selector.partition("@")
    candidate = pathlib.PurePosixPath(relative_path)
    if (
        not separator
        or not prefix_separator
        or not relative_path
        or not group_name
        or not case_prefix
        or candidate.is_absolute()
        or ".." in candidate.parts
    ):
        return [f"{owner}: invalid test-group witness: {reference}"]
    if candidate != QEMU_SHELL_TEST_PATH:
        return [f"{owner}: test-group witness must use {QEMU_SHELL_TEST_PATH}"]
    if group_name != expected_group or case_prefix != expected_prefix:
        return [
            f"{owner}: expected test group {expected_group}@{expected_prefix}, "
            f"got {group_name}@{case_prefix}"
        ]
    case_groups = read_executable_case_groups(repository_root)
    matching_cases = sorted(
        case_id
        for case_id in case_groups.get(group_name, set())
        if case_id.startswith(case_prefix)
    )
    if not matching_cases:
        return [f"{owner}: test-group witness selects no executable cases"]
    return []


def validate_closed_witness(
    anchor: str,
    witness: str,
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    if anchor in PARENT_GROUP_WITNESSES:
        expected_group, expected_prefix = PARENT_GROUP_WITNESSES[anchor]
        return validate_case_group_witness(
            anchor,
            witness,
            repository_root,
            expected_group,
            expected_prefix,
        )
    if not witness.startswith("tests:"):
        return [f"{anchor}: closed witness must start with 'tests:'"]
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
            failures.append(f"{anchor}: invalid witness reference: {witness_reference}")
            continue
        if candidate != QEMU_SHELL_TEST_PATH:
            failures.append(
                f"{anchor}: shell-language witness must use {QEMU_SHELL_TEST_PATH}"
            )
            continue
        if not (repository_root / candidate).is_file():
            failures.append(f"{anchor}: missing witness file: {relative_path}")
        if not case_id.startswith(f"{anchor}."):
            failures.append(
                f"{anchor}: witness case ID belongs to another source anchor: {case_id}"
            )
        if case_id not in executable_case_ids:
            failures.append(f"{anchor}: unknown executable case ID: {case_id}")
    return failures


def validate_ledger_text(
    ledger_text: str,
    source_rows: list[tuple[str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    rows: list[tuple[str, str, str, str, str]] = []
    saw_header = False
    for line_number, line in enumerate(ledger_text.splitlines(), start=1):
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
        return failures + ["missing ledger header"]

    anchors = [row[0] for row in rows]
    duplicate_anchors = sorted(
        {anchor for anchor in anchors if anchors.count(anchor) > 1}
    )
    if duplicate_anchors:
        failures.append(f"duplicate shell anchors: {', '.join(duplicate_anchors)}")
    if len(rows) != EXPECTED_COUNT:
        failures.append(
            f"denominator count mismatch: expected {EXPECTED_COUNT}, got {len(rows)}"
        )

    observed_source_rows = [(anchor, title) for anchor, title, *_rest in rows]
    if observed_source_rows != source_rows:
        expected_by_anchor = dict(source_rows)
        observed_by_anchor = dict(observed_source_rows)
        missing = sorted(set(expected_by_anchor) - set(observed_by_anchor))
        unexpected = sorted(set(observed_by_anchor) - set(expected_by_anchor))
        if missing:
            failures.append(f"missing shell anchors: {', '.join(missing)}")
        if unexpected:
            failures.append(f"unexpected shell anchors: {', '.join(unexpected)}")
        for anchor in sorted(set(expected_by_anchor) & set(observed_by_anchor)):
            if observed_by_anchor[anchor] != expected_by_anchor[anchor]:
                failures.append(
                    f"{anchor}: title mismatch: expected {expected_by_anchor[anchor]!r}, "
                    f"got {observed_by_anchor[anchor]!r}"
                )
        if (
            not missing
            and not unexpected
            and not any("title mismatch" in failure for failure in failures)
        ):
            failures.append("shell anchors are not in official source order")

    open_anchors: set[str] = set()
    closed_anchors: set[str] = set()
    for anchor, _title, state, witness, next_action in rows:
        if ANCHOR_PATTERN.fullmatch(anchor) is None:
            failures.append(f"invalid shell anchor: {anchor}")
        if state not in VALID_STATES:
            failures.append(f"{anchor}: invalid state: {state}")
            continue
        if state == "open":
            open_anchors.add(anchor)
            if not witness.startswith("missing:") or anchor not in witness:
                failures.append(
                    f"{anchor}: open witness must name a missing prerequisite for its anchor"
                )
            if not next_action or next_action == "-":
                failures.append(f"{anchor}: open row lacks a next action")
        else:
            closed_anchors.add(anchor)
            failures.extend(
                validate_closed_witness(
                    anchor, witness, repository_root, executable_case_ids
                )
            )
            if next_action != "-":
                failures.append(f"{anchor}: closed row next_action must be '-'")

    if open_anchors & closed_anchors:
        failures.append("open and closed shell partitions overlap")
    if open_anchors | closed_anchors != set(anchors):
        failures.append("open and closed shell partitions do not cover the denominator")
    return failures


def validate_ledger(
    ledger_path: pathlib.Path,
    source_rows: list[tuple[str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    try:
        ledger_text = ledger_path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        return [f"cannot read ASCII ledger {ledger_path}: {error}"]
    return validate_ledger_text(
        ledger_text, source_rows, repository_root, executable_case_ids
    )


def require_ledger_failure(
    name: str,
    ledger_text: str,
    expected_fragment: str,
    source_rows: list[tuple[str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> None:
    failures = validate_ledger_text(
        ledger_text, source_rows, repository_root, executable_case_ids
    )
    if not any(expected_fragment in failure for failure in failures):
        raise AssertionError(
            f"{name}: expected failure containing {expected_fragment!r}, got {failures}"
        )


def require_source_failure(
    name: str, chapter_text: str, expected_fragment: str
) -> None:
    try:
        derive_source_rows(chapter_text)
    except VerificationError as error:
        if expected_fragment in str(error):
            return
        raise AssertionError(
            f"{name}: expected failure containing {expected_fragment!r}, got {error}"
        ) from error
    raise AssertionError(f"{name}: mutated source unexpectedly passed")


def run_self_test(
    ledger_path: pathlib.Path,
    chapter_text: str,
    source_rows: list[tuple[str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> None:
    original = ledger_path.read_text(encoding="ascii")
    lines = original.splitlines()
    header_index = lines.index("\t".join(EXPECTED_COLUMNS))
    first_row_index = header_index + 1

    duplicate_lines = lines.copy()
    duplicate_lines.insert(first_row_index + 1, duplicate_lines[first_row_index])
    require_ledger_failure(
        "duplicate anchor",
        "\n".join(duplicate_lines) + "\n",
        "duplicate shell anchors",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    missing_lines = lines.copy()
    del missing_lines[first_row_index]
    require_ledger_failure(
        "missing anchor",
        "\n".join(missing_lines) + "\n",
        "denominator count mismatch",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    changed_title_lines = lines.copy()
    changed_title_fields = changed_title_lines[first_row_index].split("\t")
    changed_title_fields[1] = "Changed title"
    changed_title_lines[first_row_index] = "\t".join(changed_title_fields)
    require_ledger_failure(
        "changed title",
        "\n".join(changed_title_lines) + "\n",
        "title mismatch",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    closed_without_tests = lines.copy()
    closed_fields = closed_without_tests[first_row_index].split("\t")
    closed_fields[2] = "closed"
    closed_fields[3] = f"missing:exact_tests_for_{closed_fields[0]}"
    closed_fields[4] = "-"
    closed_without_tests[first_row_index] = "\t".join(closed_fields)
    require_ledger_failure(
        "closed witness",
        "\n".join(closed_without_tests) + "\n",
        "closed witness must start with 'tests:'",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    unknown_case_lines = lines.copy()
    unknown_case_fields = unknown_case_lines[first_row_index].split("\t")
    unknown_case_fields[2] = "closed"
    unknown_case_fields[3] = (
        f"tests:{QEMU_SHELL_TEST_PATH}#{unknown_case_fields[0]}.missing_case"
    )
    unknown_case_fields[4] = "-"
    unknown_case_lines[first_row_index] = "\t".join(unknown_case_fields)
    require_ledger_failure(
        "unknown executable case",
        "\n".join(unknown_case_lines) + "\n",
        "unknown executable case ID",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    closed_row = next(
        (
            line
            for line in lines[first_row_index:]
            if line.split("\t", maxsplit=3)[2] == "closed"
        ),
        None,
    )
    if closed_row is None:
        raise AssertionError("removed executable witness: ledger has no closed row")
    closed_witness = closed_row.split("\t")[3]
    first_witness_reference = closed_witness.removeprefix("tests:").split(",")[0]
    removed_case_id = first_witness_reference.partition("#")[2]
    executable_cases_without_witness = executable_case_ids - {removed_case_id}
    require_ledger_failure(
        "removed executable witness",
        original,
        "unknown executable case ID",
        source_rows,
        repository_root,
        executable_cases_without_witness,
    )

    require_source_failure(
        "missing source anchor",
        chapter_text.replace('name="tag_18_01"', 'name="tag_19_01"', 1),
        "source denominator count mismatch",
    )
    inserted_heading = '<h5><a name="tag_18_14_01"></a>Unexpected Clause</h5>'
    require_source_failure(
        "unexpected source anchor",
        chapter_text.replace('<a name="break"></a>', inserted_heading, 1),
        "source denominator count mismatch",
    )


def count_states(ledger_path: pathlib.Path) -> tuple[int, int]:
    open_count = 0
    closed_count = 0
    for line in ledger_path.read_text(encoding="ascii").splitlines():
        if not line or line.startswith(("#", "anchor\t")):
            continue
        state = line.split("\t", maxsplit=3)[2]
        if state == "open":
            open_count += 1
        elif state == "closed":
            closed_count += 1
    return open_count, closed_count


def row_state(ledger_text: str, anchor: str) -> str | None:
    for line in ledger_text.splitlines():
        if not line or line.startswith(("#", "anchor\t")):
            continue
        fields = line.split("\t")
        if len(fields) == len(EXPECTED_COLUMNS) and fields[0] == anchor:
            return fields[2]
    return None


def count_requirement_states(
    requirements_path: pathlib.Path, expected_count: int
) -> tuple[int, int]:
    try:
        lines = requirements_path.read_text(encoding="ascii").splitlines()
    except (OSError, UnicodeDecodeError) as error:
        raise VerificationError(
            f"cannot read ASCII dependency ledger {requirements_path}: {error}"
        ) from error
    records = [line for line in lines if line and not line.startswith("#")]
    if not records:
        raise VerificationError(f"dependency ledger is empty: {requirements_path}")
    header = records[0].split("\t")
    if "state" not in header:
        raise VerificationError(
            f"dependency ledger lacks a state column: {requirements_path}"
        )
    state_index = header.index("state")
    rows = records[1:]
    if len(rows) != expected_count:
        raise VerificationError(
            f"dependency ledger row count mismatch for {requirements_path}: "
            f"expected {expected_count}, got {len(rows)}"
        )
    open_count = 0
    closed_count = 0
    for line_number, line in enumerate(rows, start=2):
        fields = line.split("\t")
        if len(fields) <= state_index or fields[state_index] not in VALID_STATES:
            raise VerificationError(
                f"dependency ledger invalid state at logical line {line_number}: "
                f"{requirements_path}"
            )
        if fields[state_index] == "open":
            open_count += 1
        else:
            closed_count += 1
    return open_count, closed_count


def validate_parent_dependencies(
    ledger_text: str, token_open_count: int, grammar_open_count: int
) -> list[str]:
    failures: list[str] = []
    if row_state(ledger_text, "tag_18_03") == "closed" and token_open_count != 0:
        failures.append(
            "tag_18_03: cannot close while "
            f"{token_open_count} token-recognition requirements remain open"
        )
    for grammar_parent in ("tag_18_10", "tag_18_10_01", "tag_18_10_02"):
        if row_state(ledger_text, grammar_parent) == "closed" and grammar_open_count:
            failures.append(
                f"{grammar_parent}: cannot close while "
                f"{grammar_open_count} shell grammar requirements remain open"
            )
    return failures


def close_row_for_dependency_test(ledger_text: str, anchor: str) -> str:
    lines = ledger_text.splitlines()
    row_index = next(
        index for index, line in enumerate(lines) if line.startswith(f"{anchor}\t")
    )
    fields = lines[row_index].split("\t")
    fields[2] = "closed"
    lines[row_index] = "\t".join(fields)
    return "\n".join(lines) + "\n"


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive).resolve()
    ledger_path = resolve_path(repository_root, arguments.ledger).resolve()
    token_requirements_path = resolve_path(
        repository_root, arguments.token_requirements
    ).resolve()
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
        failures = validate_ledger(
            ledger_path, source_rows, repository_root, executable_case_ids
        )
        ledger_text = ledger_path.read_text(encoding="ascii")
        token_open_count, _token_closed_count = count_requirement_states(
            token_requirements_path, 17
        )
        grammar_open_count, _grammar_closed_count = count_requirement_states(
            grammar_requirements_path, 125
        )
        failures.extend(
            validate_parent_dependencies(
                ledger_text, token_open_count, grammar_open_count
            )
        )
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(
                ledger_path,
                chapter_text,
                source_rows,
                repository_root,
                executable_case_ids,
            )
            token_dependency_failures = validate_parent_dependencies(
                close_row_for_dependency_test(ledger_text, "tag_18_03"), 1, 0
            )
            if not any(
                "token-recognition requirements remain open" in failure
                for failure in token_dependency_failures
            ):
                raise AssertionError(
                    "token dependency mutation did not reject parent closure"
                )
            grammar_dependency_failures = validate_parent_dependencies(
                close_row_for_dependency_test(ledger_text, "tag_18_10"), 0, 1
            )
            if not any(
                "shell grammar requirements remain open" in failure
                for failure in grammar_dependency_failures
            ):
                raise AssertionError(
                    "grammar dependency mutation did not reject parent closure"
                )
            print("POSIX shell-language ledger mutation self-test passed.")
            return 0
        open_count, closed_count = count_states(ledger_path)
        print(
            "POSIX shell-language ledger verified: "
            f"{EXPECTED_COUNT} rows, {closed_count} closed, {open_count} open."
        )
        return 0
    except VerificationError as error:
        print(f"POSIX shell-language verification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
