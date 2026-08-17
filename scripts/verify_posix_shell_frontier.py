#!/usr/bin/env python3
"""Derive and verify the next finite SUSv4 Issue 7 shell-language frontier."""

from __future__ import annotations

import argparse
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
from verify_posix_shell_language_ledger import read_executable_case_ids


FRONTIER_PARENTS = (
    "tag_18_01",
    "tag_18_02",
    "tag_18_04",
    "tag_18_05",
    "tag_18_05_01",
    "tag_18_05_02",
    "tag_18_05_03",
    "tag_18_06",
    "tag_18_06_02",
    "tag_18_06_03",
    "tag_18_06_04",
    "tag_18_06_07",
    "tag_18_07",
    "tag_18_07_01",
    "tag_18_07_02",
    "tag_18_07_03",
    "tag_18_07_04",
    "tag_18_07_05",
    "tag_18_07_06",
    "tag_18_07_07",
    "tag_18_08",
    "tag_18_08_01",
    "tag_18_09",
    "tag_18_09_01",
    "tag_18_09_01_01",
    "tag_18_09_02",
    "tag_18_09_03",
    "tag_18_09_03_02",
    "tag_18_09_04",
    "tag_18_09_04_01",
    "tag_18_09_04_03",
)

IMPLEMENTATION_REQUIREMENTS = {
    "tag_18_01": "Preserve the shell input-to-command lifecycle for simple and compound commands, redirections, functions, built-ins, scripts, waiting, and status collection.",
    "tag_18_02": "Preserve quoting boundaries and the specified backslash, single-quote, double-quote, command-substitution, and here-document interactions.",
    "tag_18_04": "Recognize reserved words only in the grammar contexts that permit them and keep quoted or embedded spellings as ordinary words.",
    "tag_18_05": "Implement named, numeric, and special parameters with the specified expansion, assignment, inheritance, and unset behavior.",
    "tag_18_05_01": "Maintain positional-parameter cardinality, ordering, reassignment, shifting, and function or script scope across Ring 3 execution.",
    "tag_18_05_02": "Expose the special parameters with their required values, field behavior, and status or process relationships without leaking host-only state.",
    "tag_18_05_03": "Maintain shell variables, environment export state, IFS behavior, HOME, PATH, and variable assignment effects across command execution.",
    "tag_18_06": "Execute the specified expansion sequence and preserve field boundaries, pathname expansion, and final quote removal.",
    "tag_18_06_02": "Implement parameter expansion operators, length expansion, nested expansion, unset and null distinctions, and quoted field behavior.",
    "tag_18_06_03": "Implement command substitution with nested parsing, trailing-newline removal, status propagation, and quoted or unquoted field behavior.",
    "tag_18_06_04": "Implement arithmetic expansion with shell parameter lookup, integer evaluation, nesting, errors, and expansion ordering.",
    "tag_18_06_07": "Remove only syntactic quoting characters after all required expansions while preserving literal data and field cardinality.",
    "tag_18_07": "Apply redirections in lexical order with expansion, descriptor lifetime, error, and command-execution semantics defined by the standard.",
    "tag_18_07_01": "Open and attach input files with the required pathname expansion, descriptor replacement, failure status, and close-on-command behavior.",
    "tag_18_07_02": "Open and attach output files with noclobber and clobber override semantics, truncation, creation, failure, and descriptor replacement.",
    "tag_18_07_03": "Open output files in append mode with atomic append positioning and the specified failure and descriptor behavior.",
    "tag_18_07_04": "Parse here-document delimiters after quote removal, collect bodies in source order, apply tab stripping and expansion rules, and report errors.",
    "tag_18_07_05": "Duplicate input descriptors with the required numeric validation, closure, redirection order, and error status behavior.",
    "tag_18_07_06": "Duplicate output descriptors with the required numeric validation, closure, redirection order, and error status behavior.",
    "tag_18_07_07": "Open descriptors for simultaneous reading and writing with the required mode, positioning, errors, and command lifetime.",
    "tag_18_08": "Return command statuses and handle shell errors without masking specified diagnostics, abort behavior, or continuation behavior.",
    "tag_18_08_01": "Apply the standard consequences for expansion, redirection, special-builtin, utility, syntax, and asynchronous-list errors in interactive and non-interactive shells.",
    "tag_18_09": "Parse and execute the complete shell command forms with their expansion, redirection, waiting, status, and environment rules.",
    "tag_18_09_01": "Execute simple commands with assignment words, redirections, command names, arguments, expansion order, and status rules.",
    "tag_18_09_01_01": "Search and execute command names through the specified function, special-builtin, PATH, executable, error, and environment precedence.",
    "tag_18_09_02": "Construct pipelines with the required process and descriptor topology, waiting behavior, negation option, and exit status.",
    "tag_18_09_03": "Execute sequential, asynchronous, AND, and OR lists with their short-circuit, waiting, process-group, and status semantics.",
    "tag_18_09_03_02": "Launch asynchronous lists with the required background input, process identity, waiting, status, and descriptor behavior.",
    "tag_18_09_04": "Execute compound commands with their nested lists, redirections, variable scope, status, and error behavior.",
    "tag_18_09_04_01": "Implement subshell and current-environment grouping with the required environment mutation and compound-command status boundaries.",
    "tag_18_09_04_03": "Implement for-loop name assignment, word expansion, iteration, redirection, empty-list, break or continue, and exit-status behavior.",
    "tag_18_09_04_05": "Implement case pattern expansion and matching, fall-through control operators, selected-body execution, and exit status.",
    "tag_18_09_04_07": "Implement if and elif condition evaluation, branch selection, nested lists, redirections, and exit status.",
    "tag_18_09_04_09": "Implement while-loop condition and body sequencing, break or continue behavior, redirections, and exit status.",
    "tag_18_09_04_11": "Implement until-loop condition and body sequencing, break or continue behavior, redirections, and exit status.",
    "tag_18_09_05": "Define functions with the required name, body, positional-parameter, variable, redirection, invocation, and status semantics.",
}

FRONTIER_CASES = {
    parent: f"{parent}.frontier_q35" for parent in FRONTIER_PARENTS
}

PARENT_COLUMNS = (
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
QEMU_TEST_PATH = PurePosixPath("test/boot/x86_64_shell_test.py")
HEADING_PATTERN = re.compile(
    r'<h(?P<level>[2-5])\b[^>]*>\s*'
    r'<a\s+name="(?P<anchor>tag_18_[0-9_]+)"[^>]*>.*?</a>'
    r"(?P<title>.*?)</h(?P=level)>",
    re.IGNORECASE | re.DOTALL,
)
ELEMENT_PATTERN = re.compile(
    r"<(?P<tag>p|li|dd|dt)\b[^>]*>.*?</(?P=tag)>",
    re.IGNORECASE | re.DOTALL,
)
TAG_PATTERN = re.compile(r"<[^>]+>")


class VerificationError(RuntimeError):
    """Report a standards-source or recursive-frontier contract failure."""


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
    parent: str
    ordinal: int
    text: str
    source_sha256: str


@dataclass(frozen=True)
class ParentSource:
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
        default=Path("docs/posix/posix_shell_frontier_ledger.tsv"),
    )
    parser.add_argument(
        "--requirement-ledger",
        type=Path,
        default=Path("docs/posix/posix_shell_frontier_requirements.tsv"),
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
    try:
        return normalized.encode("ascii").decode("ascii")
    except UnicodeEncodeError:
        return normalized.encode("ascii", "backslashreplace").decode("ascii")


def read_chapter(archive_path: Path) -> str:
    verify_archive_identity(archive_path)
    member_name = f"{ARCHIVE_TOP_DIRECTORY}/utilities/V3_chap02.html"
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            source_file = archive.extractfile(member_name)
            if source_file is None:
                raise VerificationError(f"missing archive member: {member_name}")
            return source_file.read().decode("latin1")
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot read {member_name}: {error}") from error


def derive_headings(chapter_text: str) -> list[Heading]:
    headings: list[Heading] = []
    for match in HEADING_PATTERN.finditer(chapter_text):
        title = normalize_source_text(match.group("title"))
        headings.append(
            Heading(
                anchor=match.group("anchor"),
                title=title,
                level=int(match.group("level")),
                start=match.start(),
                content_end=match.end(),
            )
        )
    return headings


def selected_parent_sections(chapter_text: str) -> list[ParentSource]:
    headings = derive_headings(chapter_text)
    by_anchor = {heading.anchor: heading for heading in headings}
    missing = [parent for parent in FRONTIER_PARENTS if parent not in by_anchor]
    if missing:
        raise VerificationError(f"missing frontier source anchors: {', '.join(missing)}")

    derived: list[ParentSource] = []
    for parent in FRONTIER_PARENTS:
        heading = by_anchor[parent]
        next_boundary = len(chapter_text)
        for candidate in headings:
            if candidate.start <= heading.start:
                continue
            if candidate.level <= heading.level:
                next_boundary = candidate.start
                break
        section_text = chapter_text[heading.content_end:next_boundary]
        section_start = heading.content_end
        assertions: list[Assertion] = []
        for element in ELEMENT_PATTERN.finditer(section_text):
            absolute_start = section_start + element.start()
            preceding = [candidate for candidate in headings if candidate.start < absolute_start]
            if preceding and preceding[-1].title.casefold() == "examples":
                continue
            text = normalize_source_text(element.group(0))
            if not text or text.casefold() == "examples":
                continue
            ordinal = len(assertions) + 1
            requirement = f"{parent}.r{ordinal:03d}"
            assertions.append(
                Assertion(
                    requirement=requirement,
                    parent=parent,
                    ordinal=ordinal,
                    text=text,
                    source_sha256=hashlib.sha256(text.encode("ascii")).hexdigest(),
                )
            )
        if not assertions:
            raise VerificationError(f"frontier source section has no assertions: {parent}")
        raw_section_sha256 = hashlib.sha256(section_text.encode("latin1")).hexdigest()
        derived.append(
            ParentSource(
                parent=parent,
                title=heading.title,
                source_member="susv4-2018/utilities/V3_chap02.html#" + parent,
                source_sha256=raw_section_sha256,
                assertions=tuple(assertions),
            )
        )
    return derived


def requirement_digest(assertions: tuple[Assertion, ...]) -> str:
    return ordered_sha256([f"{item.requirement}\t{item.source_sha256}" for item in assertions])


def safe_field(value: str) -> str:
    if "\t" in value or "\n" in value or "\r" in value:
        raise VerificationError(f"generated field contains a tab or newline: {value!r}")
    return value


def parent_rows(sources: list[ParentSource]) -> list[tuple[str, ...]]:
    rows: list[tuple[str, ...]] = []
    for source in sources:
        rows.append(
            (
                source.parent,
                source.title,
                source.source_member,
                source.source_sha256,
                str(len(source.assertions)),
                requirement_digest(source.assertions),
                FRONTIER_CASES[source.parent],
                "open",
                f"missing:{source.parent}_all_recursive_ring3_assertions",
                "Close every derived requirement only after its exact Q35 Ring 3 witness and mutation falsifier pass.",
            )
        )
    return rows


def requirement_rows(sources: list[ParentSource]) -> list[tuple[str, ...]]:
    rows: list[tuple[str, ...]] = []
    for source in sources:
        implementation_requirement = IMPLEMENTATION_REQUIREMENTS[source.parent]
        for assertion in source.assertions:
            rows.append(
                (
                    assertion.requirement,
                    assertion.parent,
                    str(assertion.ordinal),
                    assertion.text,
                    assertion.source_sha256,
                    implementation_requirement,
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
        "# source: susv4-2018/utilities/V3_chap02.html",
        "# derivation: the first 31 open parent anchors in canonical shell-language ledger order",
        f"# snapshot-count: {len(parents)}",
        f"# ordered-parent-sha256: {ordered_sha256([row[0] for row in parents])}",
        "# state invariant: every frontier parent remains open until every recursive requirement closes",
    ]
    requirement_comments = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        "# source: susv4-2018/utilities/V3_chap02.html",
        "# derivation: every prose, list, and definition assertion in each selected parent section, excluding Examples sections",
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


def validate_parent_rows(
    rows: list[tuple[str, ...]],
    sources: list[ParentSource],
    executable_case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    expected = parent_rows(sources)
    if len(rows) != len(expected):
        failures.append(f"parent denominator mismatch: expected {len(expected)}, got {len(rows)}")
        return failures
    observed_keys = [row[0] for row in rows]
    if observed_keys != list(FRONTIER_PARENTS):
        failures.append("frontier parent keys are missing, duplicated, or out of order")
    for row, expected_row in zip(rows, expected):
        parent = row[0]
        if row[1:6] != expected_row[1:6]:
            failures.append(f"{parent}: source-derived parent fields changed")
        if row[6] != expected_row[6]:
            failures.append(f"{parent}: q35 case changed from {expected_row[6]}")
        if row[6] not in executable_case_ids:
            failures.append(f"{parent}: q35 case is not executable: {row[6]}")
        if row[7] not in VALID_STATES:
            failures.append(f"{parent}: invalid state {row[7]}")
        if row[7] == "open":
            if not row[8].startswith(f"missing:{parent}"):
                failures.append(f"{parent}: open row lacks its missing recursive witness")
            if not row[9] or row[9] == "-":
                failures.append(f"{parent}: open row lacks a next action")
        elif not row[8].startswith("tests:"):
            failures.append(f"{parent}: closed parent requires a complete tests witness")
    return failures


def validate_requirement_rows(
    rows: list[tuple[str, ...]],
    sources: list[ParentSource],
) -> list[str]:
    failures: list[str] = []
    expected = requirement_rows(sources)
    if len(rows) != len(expected):
        failures.append(
            f"recursive requirement denominator mismatch: expected {len(expected)}, got {len(rows)}"
        )
        return failures
    expected_keys = [row[0] for row in expected]
    observed_keys = [row[0] for row in rows]
    if observed_keys != expected_keys:
        failures.append("recursive requirement keys are missing, duplicated, or out of order")
    for row, expected_row in zip(rows, expected):
        requirement = row[0]
        if row[1:6] != expected_row[1:6]:
            failures.append(f"{requirement}: source assertion or implementation requirement changed")
        if row[6] not in VALID_STATES:
            failures.append(f"{requirement}: invalid state {row[6]}")
        if row[6] == "open":
            if not row[7].startswith(f"missing:{requirement}"):
                failures.append(f"{requirement}: open row lacks an exact missing witness")
            if not row[8] or row[8] == "-":
                failures.append(f"{requirement}: open row lacks a next action")
            if not row[9] or requirement not in row[9]:
                failures.append(f"{requirement}: open row lacks its mutation falsifier")
        elif not row[7].startswith("tests:"):
            failures.append(f"{requirement}: closed row requires a tests witness")
    return failures


def validate_dependencies(
    parent_rows_value: list[tuple[str, ...]],
    requirement_rows_value: list[tuple[str, ...]],
) -> list[str]:
    failures: list[str] = []
    open_requirements_by_parent: dict[str, int] = {}
    for row in requirement_rows_value:
        if row[6] == "open":
            open_requirements_by_parent[row[1]] = open_requirements_by_parent.get(row[1], 0) + 1
    for row in parent_rows_value:
        if row[7] == "closed" and open_requirements_by_parent.get(row[0], 0):
            failures.append(
                f"{row[0]}: parent closed with {open_requirements_by_parent[row[0]]} open recursive requirements"
            )
    return failures


def require_failure(
    name: str,
    failures: list[str],
    expected_fragment: str,
) -> None:
    if not any(expected_fragment in failure for failure in failures):
        raise AssertionError(f"{name}: expected {expected_fragment!r}, got {failures}")


def run_self_test(
    parent_path: Path,
    requirement_path: Path,
    sources: list[ParentSource],
    executable_case_ids: set[str],
) -> None:
    parent_text = parent_path.read_text(encoding="ascii")
    requirement_text = requirement_path.read_text(encoding="ascii")
    parent_lines = parent_text.splitlines()
    requirement_lines = requirement_text.splitlines()

    first_parent = next(index for index, line in enumerate(parent_lines) if line.startswith("tag_18_01\t"))
    duplicate_parent = parent_lines.copy()
    duplicate_parent.insert(first_parent + 1, duplicate_parent[first_parent])
    duplicate_rows = [
        tuple(line.split("\t"))
        for line in duplicate_parent
        if line and not line.startswith("#") and not line.startswith("parent\t")
    ]
    require_failure(
        "duplicate parent",
        validate_parent_rows(duplicate_rows, sources, executable_case_ids),
        "parent denominator mismatch",
    )

    first_requirement = next(
        index for index, line in enumerate(requirement_lines) if line.startswith("tag_18_01.r001\t")
    )
    mutated_requirement = requirement_lines.copy()
    fields = mutated_requirement[first_requirement].split("\t")
    fields[3] = fields[3] + " mutated"
    mutated_requirement[first_requirement] = "\t".join(fields)
    mutated_rows = [
        tuple(line.split("\t"))
        for line in mutated_requirement
        if line and not line.startswith("#") and not line.startswith("requirement\t")
    ]
    require_failure(
        "mutated assertion",
        validate_requirement_rows(mutated_rows, sources),
        "source assertion or implementation requirement changed",
    )

    closed_parent = parent_lines.copy()
    closed_fields = closed_parent[first_parent].split("\t")
    closed_fields[7] = "closed"
    closed_fields[8] = "tests:test/boot/x86_64_shell_test.py#tag_18_01.frontier_q35"
    closed_parent[first_parent] = "\t".join(closed_fields)
    closed_rows = [
        tuple(line.split("\t"))
        for line in closed_parent
        if line and not line.startswith("#") and not line.startswith("parent\t")
    ]
    # The dependency mutation is tested directly with the canonical rows.
    canonical_requirement_rows = [
        tuple(line.split("\t"))
        for line in requirement_lines
        if line and not line.startswith("#") and not line.startswith("requirement\t")
    ]
    dependency_failures = validate_dependencies(closed_rows, canonical_requirement_rows)
    require_failure("closed parent dependency", dependency_failures, "parent closed with")


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive)
    parent_path = resolve_path(repository_root, arguments.parent_ledger)
    requirement_path = resolve_path(repository_root, arguments.requirement_ledger)
    if not archive_path.is_file():
        print(f"SKIP: POSIX Issue 7 archive not found: {archive_path}")
        return 77
    try:
        sources = selected_parent_sections(read_chapter(archive_path))
        if arguments.derive:
            parent_text, requirement_text = generated_ledgers(sources)
            parent_path.write_text(parent_text, encoding="ascii")
            requirement_path.write_text(requirement_text, encoding="ascii")
            print(
                f"derived shell frontier: {len(sources)} parents, "
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
        parent_open = sum(row[7] == "open" for row in parent_rows_value)
        requirement_open = sum(row[6] == "open" for row in requirement_rows_value)
        print(
            f"POSIX shell frontier verified: {len(parent_rows_value)} parents, "
            f"{parent_open} open; {len(requirement_rows_value)} recursive assertions, "
            f"{requirement_open} open"
        )
        return 0
    except (OSError, VerificationError, AssertionError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
