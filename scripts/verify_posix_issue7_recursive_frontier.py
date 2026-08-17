#!/usr/bin/env python3
"""Derive and verify the next canonical SUSv4 Issue 7 parent frontier."""

from __future__ import annotations

import argparse
import ast
import hashlib
import html
import re
import sys
import tarfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from verify_posix_issue7_archive import (
    ARCHIVE_NAME,
    ARCHIVE_TOP_DIRECTORY,
    verify_archive_identity,
)


QUEUE_OFFSET = 31
FRONTIER_SIZE = 31
QEMU_TEST_PATH = PurePosixPath("test/boot/x86_64_shell_test.py")
SHELL_LANGUAGE_CASE_GROUP = "SHELL_LANGUAGE_COMMAND_CASES"
EXPECTED_ROW_KEYS = (
    "shell:tag_18_09_04_05",
    "shell:tag_18_09_04_07",
    "shell:tag_18_09_04_09",
    "shell:tag_18_09_04_11",
    "shell:tag_18_09_05",
    "shell:tag_18_11",
    "shell:tag_18_12",
    "shell:tag_18_13",
    "shell:tag_18_13_01",
    "shell:tag_18_13_02",
    "shell:tag_18_13_03",
    "shell:tag_18_14",
    "utility:admin",
    "utility:alias",
    "utility:ar",
    "utility:asa",
    "utility:at",
    "utility:awk",
    "utility:basename",
    "utility:batch",
    "utility:bc",
    "utility:bg",
    "utility:break",
    "utility:c99",
    "utility:cal",
    "utility:cat",
    "utility:cd",
    "utility:cflow",
    "utility:chgrp",
    "utility:chmod",
    "utility:chown",
)
SHELL_IMPLEMENTATION_REQUIREMENTS = {
    "tag_18_09_04_05": "Implement case pattern expansion and matching, fall-through control operators, selected-body execution, and exit status.",
    "tag_18_09_04_07": "Implement if and elif condition evaluation, branch selection, nested lists, redirections, and exit status.",
    "tag_18_09_04_09": "Implement while-loop condition and body sequencing, break or continue behavior, redirections, and exit status.",
    "tag_18_09_04_11": "Implement until-loop condition and body sequencing, break or continue behavior, redirections, and exit status.",
    "tag_18_09_05": "Define functions with the required name, body, positional-parameter, variable, redirection, invocation, and status semantics.",
    "tag_18_11": "Deliver the specified signal dispositions, traps, interrupt behavior, and shell error interactions in the Ring 3 execution environment.",
    "tag_18_12": "Preserve the shell execution environment, subshell boundaries, variables, descriptors, working directory, and inherited state across command execution.",
    "tag_18_13": "Implement the complete pattern matching notation used by case matching and pathname expansion.",
    "tag_18_13_01": "Implement single-character pattern matching and its bracket, question-mark, and escaping rules.",
    "tag_18_13_02": "Implement multi-character pattern matching and its star, bracket, and escaping rules.",
    "tag_18_13_03": "Apply pattern matching to filename expansion with directory entries, no-match behavior, and quoting boundaries.",
    "tag_18_14": "Implement the special built-in utilities with their environment mutation, error, exit-status, and command-search semantics.",
}
UTILITY_BEHAVIOR_REQUIREMENTS = {
    "alias": "Implement the Issue 7 alias utility and its definition, expansion, removal, quoting, and execution behavior.",
    "bg": "Implement the Issue 7 bg utility and its job-control, process-group, status, and diagnostic behavior.",
    "break": "Implement the Issue 7 break utility and its loop-depth, operand, diagnostic, and exit-status behavior.",
    "cd": "Implement the Issue 7 cd utility and its logical or physical path resolution, HOME, CDPATH, PWD, OLDPWD, and diagnostics.",
}
SPECIAL_BUILTINS = {
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
}
PARENT_COLUMNS = (
    "row",
    "domain",
    "parent",
    "title",
    "source_member",
    "source_sha256",
    "requirement_count",
    "requirement_sha256",
    "q35_case",
    "state",
    "witness",
    "next_action",
)
REQUIREMENT_COLUMNS = (
    "requirement",
    "row",
    "domain",
    "parent",
    "ordinal",
    "assertion",
    "assertion_sha256",
    "implementation_requirement",
    "state",
    "witness",
    "next_action",
    "falsifier",
)
VALID_STATES = {"open", "closed"}
HEADING_PATTERN = re.compile(
    r'<h(?P<level>[2-5])\b[^>]*>\s*'
    r'<a\s+name="(?P<anchor>tag_(?:18|20)_[0-9_]+)"[^>]*>.*?</a>'
    r"(?P<title>.*?)</h(?P=level)>",
    re.IGNORECASE | re.DOTALL,
)
ELEMENT_PATTERN = re.compile(
    r"<(?P<tag>p|li|dd|dt)\b[^>]*>.*?</(?P=tag)>",
    re.IGNORECASE | re.DOTALL,
)
TAG_PATTERN = re.compile(r"<[^>]+>")


class VerificationError(RuntimeError):
    """Report an Issue 7 source or recursive-frontier contract failure."""


@dataclass(frozen=True)
class Heading:
    anchor: str
    title: str
    level: int
    start: int
    content_end: int


@dataclass(frozen=True)
class Assertion:
    requirement: str
    row: str
    domain: str
    parent: str
    ordinal: int
    text: str
    source_sha256: str


@dataclass(frozen=True)
class ParentSource:
    row: str
    domain: str
    parent: str
    title: str
    source_member: str
    source_sha256: str
    assertions: tuple[Assertion, ...]


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
        "--parent-ledger",
        type=Path,
        default=Path("docs/posix/posix_issue7_recursive_frontier_ledger.tsv"),
    )
    parser.add_argument(
        "--requirement-ledger",
        type=Path,
        default=Path("docs/posix/posix_issue7_recursive_frontier_requirements.tsv"),
    )
    parser.add_argument(
        "--shell-ledger",
        type=Path,
        default=Path("docs/posix/posix_shell_language_ledger.tsv"),
    )
    parser.add_argument(
        "--utility-ledger",
        type=Path,
        default=Path("docs/posix/posix_issue7_utility_ledger.tsv"),
    )
    parser.add_argument("--derive", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(repository_root: Path, candidate: Path) -> Path:
    return candidate.resolve() if candidate.is_absolute() else (repository_root / candidate).resolve()


def ordered_sha256(lines: list[str]) -> str:
    return hashlib.sha256("".join(f"{line}\n" for line in lines).encode("ascii")).hexdigest()


def normalize_source_text(raw_text: str) -> str:
    normalized = " ".join(html.unescape(TAG_PATTERN.sub("", raw_text)).split())
    return normalized.encode("ascii", "backslashreplace").decode("ascii")


def derive_headings(source_text: str) -> list[Heading]:
    headings: list[Heading] = []
    for match in HEADING_PATTERN.finditer(source_text):
        headings.append(
            Heading(
                anchor=match.group("anchor"),
                title=normalize_source_text(match.group("title")),
                level=int(match.group("level")),
                start=match.start(),
                content_end=match.end(),
            )
        )
    return headings


def section_assertions(
    source_text: str,
    headings: list[Heading],
    heading: Heading,
    row: str,
    domain: str,
    parent: str,
) -> list[Assertion]:
    next_boundary = len(source_text)
    for candidate in headings:
        if candidate.start > heading.start and candidate.level <= heading.level:
            next_boundary = candidate.start
            break
    section_text = source_text[heading.content_end:next_boundary]
    assertions: list[Assertion] = []
    for element in ELEMENT_PATTERN.finditer(section_text):
        absolute_start = heading.content_end + element.start()
        preceding = [candidate for candidate in headings if candidate.start < absolute_start]
        if preceding and preceding[-1].title.casefold() == "examples":
            continue
        text = normalize_source_text(element.group(0))
        if not text or text.casefold() == "examples":
            continue
        ordinal = len(assertions) + 1
        assertions.append(
            Assertion(
                requirement=f"{row}.r{ordinal:03d}",
                row=row,
                domain=domain,
                parent=parent,
                ordinal=ordinal,
                text=text,
                source_sha256=hashlib.sha256(text.encode("ascii")).hexdigest(),
            )
        )
    return assertions


def derive_shell_source(chapter_text: str, parent: str) -> ParentSource:
    headings = derive_headings(chapter_text)
    heading = next((candidate for candidate in headings if candidate.anchor == parent), None)
    if heading is None:
        raise VerificationError(f"missing shell source anchor: {parent}")
    row = f"shell:{parent}"
    assertions = section_assertions(chapter_text, headings, heading, row, "shell", parent)
    if not assertions:
        raise VerificationError(f"shell source section has no assertions: {parent}")
    next_boundary = len(chapter_text)
    for candidate in headings:
        if candidate.start > heading.start and candidate.level <= heading.level:
            next_boundary = candidate.start
            break
    section_text = chapter_text[heading.content_end:next_boundary]
    return ParentSource(
        row=row,
        domain="shell",
        parent=parent,
        title=heading.title,
        source_member=f"{ARCHIVE_TOP_DIRECTORY}/utilities/V3_chap02.html#{parent}",
        source_sha256=hashlib.sha256(section_text.encode("latin1")).hexdigest(),
        assertions=tuple(assertions),
    )


def read_archive_member(archive_path: Path, member_name: str) -> bytes:
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            source_file = archive.extractfile(member_name)
            if source_file is None:
                raise VerificationError(f"missing archive member: {member_name}")
            return source_file.read()
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot read {member_name}: {error}") from error


def derive_utility_source(archive_path: Path, utility: str) -> ParentSource:
    member_name = f"{ARCHIVE_TOP_DIRECTORY}/utilities/{utility}.html"
    if utility in SPECIAL_BUILTINS:
        chapter_text = read_chapter(archive_path)
        special_pattern = re.compile(
            r'<a\s+name="(?P<utility>[a-z][a-z0-9]*)"[^>]*></a>\s*'
            r'<a\s+name="(?P<anchor>tag_18_[0-9_]+)"',
            re.IGNORECASE,
        )
        special_matches = [
            match
            for match in special_pattern.finditer(chapter_text)
            if match.group("utility") in SPECIAL_BUILTINS
        ]
        anchor_match = next(
            (match for match in special_matches if match.group("utility") == utility),
            None,
        )
        if anchor_match is None:
            raise VerificationError(f"missing special built-in source anchor: {utility}")
        anchor = anchor_match.group("anchor")
        current_index = special_matches.index(anchor_match)
        next_start = (
            special_matches[current_index + 1].start()
            if current_index + 1 < len(special_matches)
            else len(chapter_text)
        )
        section_text = chapter_text[anchor_match.end():next_start]
        row = f"utility:{utility}"
        assertions: list[Assertion] = []
        for element in ELEMENT_PATTERN.finditer(section_text):
            text = normalize_source_text(element.group(0))
            if not text or text.casefold() == "examples":
                continue
            ordinal = len(assertions) + 1
            assertions.append(
                Assertion(
                    requirement=f"{row}.r{ordinal:03d}",
                    row=row,
                    domain="utility",
                    parent=utility,
                    ordinal=ordinal,
                    text=text,
                    source_sha256=hashlib.sha256(text.encode("ascii")).hexdigest(),
                )
            )
        if not assertions:
            raise VerificationError(f"special built-in source has no assertions: {utility}")
        return ParentSource(
            row=row,
            domain="utility",
            parent=utility,
            title=utility,
            source_member=f"{ARCHIVE_TOP_DIRECTORY}/utilities/V3_chap02.html#{anchor}",
            source_sha256=hashlib.sha256(section_text.encode("latin1")).hexdigest(),
            assertions=tuple(assertions),
        )
    raw_bytes = read_archive_member(archive_path, member_name)
    source_text = raw_bytes.decode("latin1")
    headings = derive_headings(source_text)
    utility_headings = [heading for heading in headings if heading.anchor.startswith("tag_20_")]
    if not utility_headings:
        raise VerificationError(f"utility source has no normative headings: {utility}")
    top_level_level = min(heading.level for heading in utility_headings)
    top_level = [heading for heading in utility_headings if heading.level == top_level_level]
    if not top_level:
        raise VerificationError(f"utility source has no normative headings: {utility}")
    row = f"utility:{utility}"
    assertions: list[Assertion] = []
    for heading in top_level:
        if heading.title.casefold() == "examples":
            continue
        assertions.extend(
            section_assertions(source_text, headings, heading, row, "utility", utility)
        )
    if not assertions:
        raise VerificationError(f"utility source has no assertions: {utility}")
    return ParentSource(
        row=row,
        domain="utility",
        parent=utility,
        title=utility,
        source_member=f"{member_name}#{utility}",
        source_sha256=hashlib.sha256(raw_bytes).hexdigest(),
        assertions=tuple(
            Assertion(
                requirement=f"{row}.r{ordinal:03d}",
                row=assertion.row,
                domain=assertion.domain,
                parent=assertion.parent,
                ordinal=ordinal,
                text=assertion.text,
                source_sha256=assertion.source_sha256,
            )
            for ordinal, assertion in enumerate(assertions, start=1)
        ),
    )


def read_chapter(archive_path: Path) -> str:
    member_name = f"{ARCHIVE_TOP_DIRECTORY}/utilities/V3_chap02.html"
    return read_archive_member(archive_path, member_name).decode("latin1")


def parse_source_ledger(path: Path, expected_columns: tuple[str, ...]) -> list[tuple[str, ...]]:
    try:
        text = path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        raise VerificationError(f"cannot read ASCII ledger {path}: {error}") from error
    rows: list[tuple[str, ...]] = []
    header_seen = False
    for line_number, line in enumerate(text.splitlines(), start=1):
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        if not header_seen:
            header_seen = True
            if fields != expected_columns:
                raise VerificationError(f"{path}: unexpected source ledger header")
            continue
        if len(fields) != len(expected_columns):
            raise VerificationError(
                f"{path}: line {line_number} has {len(fields)} fields, "
                f"expected {len(expected_columns)}"
            )
        if any(existing[0] == fields[0] for existing in rows):
            raise VerificationError(f"{path}: duplicate source row key {fields[0]}")
        rows.append(fields)
    if not header_seen:
        raise VerificationError(f"{path}: missing source ledger header")
    return rows


def selected_sources(
    archive_path: Path,
    shell_ledger_path: Path,
    utility_ledger_path: Path,
) -> list[ParentSource]:
    shell_rows = parse_source_ledger(shell_ledger_path, ("anchor", "title", "state", "witness", "next_action"))
    utility_rows = parse_source_ledger(utility_ledger_path, ("utility", "state", "witness", "next_action"))
    shell_open = [row[0] for row in shell_rows if len(row) >= 3 and row[2] == "open"]
    utility_open = [row[0] for row in utility_rows if len(row) >= 2 and row[1] == "open"]
    queue = [f"shell:{parent}" for parent in shell_open]
    queue.extend(f"utility:{utility}" for utility in utility_open)
    observed_keys = tuple(queue[QUEUE_OFFSET : QUEUE_OFFSET + FRONTIER_SIZE])
    if observed_keys != EXPECTED_ROW_KEYS:
        raise VerificationError(
            "canonical open-parent queue changed: "
            f"expected {EXPECTED_ROW_KEYS}, got {observed_keys}"
        )
    chapter_text = read_chapter(archive_path)
    shell_parents = [key.removeprefix("shell:") for key in observed_keys if key.startswith("shell:")]
    utility_parents = [key.removeprefix("utility:") for key in observed_keys if key.startswith("utility:")]
    sources = [derive_shell_source(chapter_text, parent) for parent in shell_parents]
    sources.extend(derive_utility_source(archive_path, utility) for utility in utility_parents)
    if tuple(source.row for source in sources) != EXPECTED_ROW_KEYS:
        raise VerificationError("derived source order does not match the canonical frontier")
    return sources


def requirement_digest(assertions: tuple[Assertion, ...]) -> str:
    return ordered_sha256([f"{item.requirement}\t{item.source_sha256}" for item in assertions])


def implementation_requirement(source: ParentSource) -> str:
    if source.domain == "shell":
        return SHELL_IMPLEMENTATION_REQUIREMENTS[source.parent]
    return UTILITY_BEHAVIOR_REQUIREMENTS.get(
        source.parent,
        f"Implement the complete SUSv4 Issue 7 {source.parent} utility contract, including options, operands, errors, environment, output, and exit status.",
    )


def safe_field(value: str) -> str:
    if "\t" in value or "\n" in value or "\r" in value:
        raise VerificationError(f"generated field contains a tab or newline: {value!r}")
    return value


def parent_rows(sources: list[ParentSource]) -> list[tuple[str, ...]]:
    return [
        (
            source.row,
            source.domain,
            source.parent,
            source.title,
            source.source_member,
            source.source_sha256,
            str(len(source.assertions)),
            requirement_digest(source.assertions),
            f"{source.row}.q35",
            "open",
            f"missing:{source.row}_all_recursive_ring3_assertions",
            "Close every derived requirement only after its exact Q35 Ring 3 witness and mutation falsifier pass.",
        )
        for source in sources
    ]


def requirement_rows(sources: list[ParentSource]) -> list[tuple[str, ...]]:
    rows: list[tuple[str, ...]] = []
    for source in sources:
        requirement_text = implementation_requirement(source)
        for assertion in source.assertions:
            rows.append(
                (
                    assertion.requirement,
                    assertion.row,
                    assertion.domain,
                    assertion.parent,
                    str(assertion.ordinal),
                    assertion.text,
                    assertion.source_sha256,
                    requirement_text,
                    "open",
                    f"missing:{assertion.requirement}_q35_ring3_witness",
                    "Add an exact Q35 Ring 3 case for this assertion, then run the mutation falsifier before closing it.",
                    f"Mutate the assertion or remove its witness; verification must reject {assertion.requirement}.",
                )
            )
    return rows


def render_ledger(
    comments: list[str], columns: tuple[str, ...], rows: list[tuple[str, ...]]
) -> str:
    output = [*comments, "\t".join(columns)]
    output.extend("\t".join(safe_field(value) for value in row) for row in rows)
    return "\n".join(output) + "\n"


def generated_ledgers(sources: list[ParentSource]) -> tuple[str, str]:
    parents = parent_rows(sources)
    requirements = requirement_rows(sources)
    parent_comments = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        "# source: susv4-2018/utilities/V3_chap02.html and susv4-2018/utilities/*.html",
        "# derivation: queue slice 31:62 of the canonical open shell-language then utility parent rows",
        f"# queue-offset: {QUEUE_OFFSET}",
        f"# snapshot-count: {len(parents)}",
        f"# shell-parent-count: {sum(row[1] == 'shell' for row in parents)}",
        f"# utility-parent-count: {sum(row[1] == 'utility' for row in parents)}",
        f"# ordered-parent-sha256: {ordered_sha256([row[0] for row in parents])}",
        "# state invariant: every frontier parent remains open until every recursive requirement closes",
    ]
    requirement_comments = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        "# source: source member and assertion hashes are derived from the pinned SUSv4 Issue 7 archive",
        "# derivation: every prose, list, and definition assertion in each selected source section, excluding Examples sections",
        f"# snapshot-count: {len(requirements)}",
        f"# ordered-requirement-sha256: {ordered_sha256([row[0] for row in requirements])}",
        "# state invariant: every recursive requirement is exactly open or closed and every open row has a falsifier",
    ]
    return (
        render_ledger(parent_comments, PARENT_COLUMNS, parents),
        render_ledger(requirement_comments, REQUIREMENT_COLUMNS, requirements),
    )


def parse_ledger(path: Path, columns: tuple[str, ...]) -> list[tuple[str, ...]]:
    try:
        text = path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        raise VerificationError(f"cannot read ASCII ledger {path}: {error}") from error
    records: list[tuple[str, ...]] = []
    saw_header = False
    for line_number, line in enumerate(text.splitlines(), start=1):
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        if not saw_header:
            saw_header = True
            if fields != columns:
                raise VerificationError(f"{path}: expected header {columns}, got {fields}")
            continue
        if len(fields) != len(columns):
            raise VerificationError(
                f"{path}: line {line_number} has {len(fields)} fields, expected {len(columns)}"
            )
        records.append(fields)
    if not saw_header:
        raise VerificationError(f"{path}: missing ledger header")
    return records


def read_executable_case_ids(repository_root: Path) -> set[str]:
    test_path = repository_root / QEMU_TEST_PATH
    try:
        test_text = test_path.read_text(encoding="ascii")
        syntax_tree = ast.parse(test_text, filename=str(test_path))
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise VerificationError(f"cannot parse ASCII shell test {test_path}: {error}") from error
    assignment = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.Assign)
            and any(
                isinstance(target, ast.Name)
                and target.id == SHELL_LANGUAGE_CASE_GROUP
                for target in statement.targets
            )
        ),
        None,
    )
    if assignment is None:
        raise VerificationError(f"{QEMU_TEST_PATH}: missing {SHELL_LANGUAGE_CASE_GROUP}")
    try:
        cases = ast.literal_eval(assignment.value)
    except (ValueError, TypeError) as error:
        raise VerificationError(f"{QEMU_TEST_PATH}: case group is not an ASCII literal") from error
    if not isinstance(cases, tuple):
        raise VerificationError(f"{QEMU_TEST_PATH}: case group must be a tuple")
    case_ids: set[str] = set()
    for case in cases:
        if not isinstance(case, tuple) or len(case) != 3 or not all(isinstance(value, str) for value in case):
            raise VerificationError(f"{QEMU_TEST_PATH}: malformed shell case")
        case_id, command, expected = case
        if not case_id or not command or not expected:
            raise VerificationError(f"{QEMU_TEST_PATH}: shell case has an empty field")
        if case_id in case_ids:
            raise VerificationError(f"{QEMU_TEST_PATH}: duplicate shell case ID: {case_id}")
        case_ids.add(case_id)
    return case_ids


def validate_parent_rows(
    rows: list[tuple[str, ...]],
    sources: list[ParentSource],
    executable_case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    expected = parent_rows(sources)
    if len(rows) != len(expected):
        return [f"parent denominator mismatch: expected {len(expected)}, got {len(rows)}"]
    if [row[0] for row in rows] != list(EXPECTED_ROW_KEYS):
        failures.append("frontier parent keys are missing, duplicated, or out of order")
    for row, expected_row in zip(rows, expected):
        owner = row[0]
        if row[1:8] != expected_row[1:8]:
            failures.append(f"{owner}: source-derived parent fields changed")
        if row[8] != expected_row[8]:
            failures.append(f"{owner}: q35 case changed from {expected_row[8]}")
        if row[8] not in executable_case_ids:
            failures.append(f"{owner}: q35 case is not executable: {row[8]}")
        if row[9] not in VALID_STATES:
            failures.append(f"{owner}: invalid state {row[9]}")
        if row[9] == "open":
            if not row[10].startswith(f"missing:{owner}"):
                failures.append(f"{owner}: open row lacks its missing recursive witness")
            if not row[11] or row[11] == "-":
                failures.append(f"{owner}: open row lacks a next action")
        elif not row[10].startswith("tests:"):
            failures.append(f"{owner}: closed parent requires a complete tests witness")
    return failures


def validate_requirement_rows(
    rows: list[tuple[str, ...]],
    sources: list[ParentSource],
) -> list[str]:
    failures: list[str] = []
    expected = requirement_rows(sources)
    if len(rows) != len(expected):
        return [
            "recursive requirement denominator mismatch: "
            f"expected {len(expected)}, got {len(rows)}"
        ]
    if [row[0] for row in rows] != [row[0] for row in expected]:
        failures.append("recursive requirement keys are missing, duplicated, or out of order")
    for row, expected_row in zip(rows, expected):
        requirement = row[0]
        if row[1:8] != expected_row[1:8]:
            failures.append(f"{requirement}: source assertion or implementation requirement changed")
        if row[8] not in VALID_STATES:
            failures.append(f"{requirement}: invalid state {row[8]}")
        if row[8] == "open":
            if not row[9].startswith(f"missing:{requirement}"):
                failures.append(f"{requirement}: open row lacks an exact missing witness")
            if not row[10] or row[10] == "-":
                failures.append(f"{requirement}: open row lacks a next action")
            if not row[11] or requirement not in row[11]:
                failures.append(f"{requirement}: open row lacks its mutation falsifier")
        elif not row[9].startswith("tests:"):
            failures.append(f"{requirement}: closed row requires a tests witness")
    return failures


def validate_dependencies(
    parent_rows_value: list[tuple[str, ...]],
    requirement_rows_value: list[tuple[str, ...]],
) -> list[str]:
    open_requirements_by_row: dict[str, int] = {}
    for row in requirement_rows_value:
        if row[8] == "open":
            open_requirements_by_row[row[1]] = open_requirements_by_row.get(row[1], 0) + 1
    failures: list[str] = []
    for row in parent_rows_value:
        if row[9] == "closed" and open_requirements_by_row.get(row[0], 0):
            failures.append(
                f"{row[0]}: parent closed with {open_requirements_by_row[row[0]]} open recursive requirements"
            )
    return failures


def require_failure(name: str, failures: list[str], expected_fragment: str) -> None:
    if not any(expected_fragment in failure for failure in failures):
        raise AssertionError(f"{name}: expected {expected_fragment!r}, got {failures}")


def run_self_test(
    parent_path: Path,
    requirement_path: Path,
    sources: list[ParentSource],
    executable_case_ids: set[str],
) -> None:
    parent_lines = parent_path.read_text(encoding="ascii").splitlines()
    requirement_lines = requirement_path.read_text(encoding="ascii").splitlines()
    parent_rows_value = [
        tuple(line.split("\t"))
        for line in parent_lines
        if line and not line.startswith("#") and not line.startswith("row\t")
    ]
    requirement_rows_value = [
        tuple(line.split("\t"))
        for line in requirement_lines
        if line and not line.startswith("#") and not line.startswith("requirement\t")
    ]
    duplicate_rows = parent_rows_value.copy()
    duplicate_rows.insert(1, duplicate_rows[0])
    require_failure(
        "duplicate parent",
        validate_parent_rows(duplicate_rows, sources, executable_case_ids),
        "parent denominator mismatch",
    )
    mutated_rows = requirement_rows_value.copy()
    mutated_fields = list(mutated_rows[0])
    mutated_fields[5] += " mutated"
    mutated_rows[0] = tuple(mutated_fields)
    require_failure(
        "mutated assertion",
        validate_requirement_rows(mutated_rows, sources),
        "source assertion or implementation requirement changed",
    )
    mutated_parent = parent_rows_value.copy()
    mutated_parent_fields = list(mutated_parent[0])
    mutated_parent_fields[8] = f"{mutated_parent_fields[8]}.missing"
    mutated_parent[0] = tuple(mutated_parent_fields)
    require_failure(
        "mutated Q35 witness",
        validate_parent_rows(mutated_parent, sources, executable_case_ids),
        "q35 case changed",
    )
    closed_parent = parent_rows_value.copy()
    closed_fields = list(closed_parent[0])
    closed_fields[9] = "closed"
    closed_fields[10] = f"tests:{QEMU_TEST_PATH}#{closed_fields[8]}"
    closed_parent[0] = tuple(closed_fields)
    require_failure(
        "closed parent dependency",
        validate_dependencies(closed_parent, requirement_rows_value),
        "parent closed with",
    )


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive)
    parent_path = resolve_path(repository_root, arguments.parent_ledger)
    requirement_path = resolve_path(repository_root, arguments.requirement_ledger)
    shell_ledger_path = resolve_path(repository_root, arguments.shell_ledger)
    utility_ledger_path = resolve_path(repository_root, arguments.utility_ledger)
    if not archive_path.is_file():
        print(f"SKIP: POSIX Issue 7 archive not found: {archive_path}")
        return 77
    try:
        verify_archive_identity(archive_path)
        sources = selected_sources(archive_path, shell_ledger_path, utility_ledger_path)
        if arguments.derive:
            parent_text, requirement_text = generated_ledgers(sources)
            parent_path.write_text(parent_text, encoding="ascii")
            requirement_path.write_text(requirement_text, encoding="ascii")
            print(
                f"derived Issue 7 recursive frontier: {len(sources)} parents, "
                f"{sum(len(source.assertions) for source in sources)} recursive assertions"
            )
            return 0
        parent_rows_value = parse_ledger(parent_path, PARENT_COLUMNS)
        requirement_rows_value = parse_ledger(requirement_path, REQUIREMENT_COLUMNS)
        executable_case_ids = read_executable_case_ids(repository_root)
        failures = validate_parent_rows(parent_rows_value, sources, executable_case_ids)
        failures.extend(validate_requirement_rows(requirement_rows_value, sources))
        failures.extend(validate_dependencies(parent_rows_value, requirement_rows_value))
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(parent_path, requirement_path, sources, executable_case_ids)
        parent_open = sum(row[9] == "open" for row in parent_rows_value)
        requirement_open = sum(row[8] == "open" for row in requirement_rows_value)
        shell_count = sum(row.domain == "shell" for row in sources)
        utility_count = sum(row.domain == "utility" for row in sources)
        print(
            f"POSIX Issue 7 recursive frontier verified: {len(parent_rows_value)} parents "
            f"({shell_count} shell, {utility_count} utility), {parent_open} open; "
            f"{len(requirement_rows_value)} recursive assertions, {requirement_open} open"
        )
        return 0
    except (OSError, VerificationError, AssertionError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
