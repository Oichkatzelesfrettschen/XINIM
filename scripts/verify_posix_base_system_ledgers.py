#!/usr/bin/env python3
"""Derive and verify the SUSv4 Base Definitions and System Interfaces ledgers."""

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
    read_safe_members,
    verify_archive_identity,
)

ARCHIVE_SHA256 = "ab6636bca53c7d71d33d2c5149ede574d598fe6ec97fa8b08e0459ef7bcfc104"
BASE_PARENT_COUNT = 95
BASE_PARENT_KEY_SHA256 = "7841b16fa7f92dc3dbc9349d5ee590f1958dca287553b5e0c3ffb6c1faa6a6bd"
BASE_PARENT_ROW_SHA256 = "52ceca77e192bc8535db775fa0a96006537f700ad28ade1494a81ad9ce0f960a"
BASE_CLAUSE_COUNT = 1483
BASE_CLAUSE_KEY_SHA256 = "e357d95bf96ea94101041ab8db05156d0c107addc2318409cdd4fa83f637e575"
BASE_CLAUSE_ROW_SHA256 = "42fbbd0f2c879de5501f33c5d938d5d8bae7788edbc88176b200839c654db458"
SYSTEM_PARENT_COUNT = 1195
SYSTEM_PARENT_KEY_SHA256 = "80e5f8bb20ffadb5a597e5b1183a18d10c459c621dc34501c6357df61ec64a21"
SYSTEM_PARENT_ROW_SHA256 = "964819d6348ec39cfd280250e254a36af5ed03d8bff4202661f513cc84118521"
SYSTEM_CLAUSE_COUNT = 15740
SYSTEM_CLAUSE_KEY_SHA256 = "1db32fccd04b71cadc31d467c8a1a4bf552a058110a61428a9511883a7a5ebc4"
SYSTEM_CLAUSE_ROW_SHA256 = "ee902a95473a45fc66bc580c78005d84747c464eb60d72316a6767bfaab12e52"

PARENT_COLUMNS = (
    "parent",
    "source",
    "title",
    "source_sha256",
    "clause_count",
    "clause_sha256",
    "state",
    "witness",
    "next_action",
)
CLAUSE_COLUMNS = (
    "parent",
    "clause",
    "title",
    "source_sha256",
    "state",
    "witness",
    "next_action",
)
VALID_STATES = {"open", "closed"}
SOURCE_PATH_PATTERN = re.compile(r"(?:basedefs|functions)/[A-Za-z0-9_.+-]+\.html")
BASE_CHAPTER_PATTERN = re.compile(r"V1_chap(?:0[1-9]|1[0-3])\.html")
HEADING_PATTERN = re.compile(
    r'<h(?P<level>[2-5])\b[^>]*>\s*'
    r'<a\s+name="(?P<anchor>tag_[^"]+)"[^>]*>.*?</a>'
    r"(?P<title>.*?)</h(?P=level)>",
    re.IGNORECASE | re.DOTALL,
)
TITLE_PATTERN = re.compile(r"<title>(?P<title>.*?)</title>", re.IGNORECASE | re.DOTALL)
TAG_PATTERN = re.compile(r"<[^>]+>")
CLAUSE_PATTERN = re.compile(r"tag_[0-9]+(?:_[0-9]+)*")
QEMU_RING3_WITNESS = "test/boot/x86_64_shell_test.py"
QEMU_RING3_CASE_GROUP = "SHELL_LANGUAGE_COMMAND_CASES"
QEMU_RING3_CASE = "tag_18_03.rule02_operator_continuation.and_if"
VERIFIER_PATH = "scripts/verify_posix_base_system_ledgers.py"


class VerificationError(RuntimeError):
    """Report a standards-source or ledger contract failure."""


@dataclass(frozen=True)
class ClauseSource:
    parent: str
    clause: str
    title: str
    source_sha256: str


@dataclass(frozen=True)
class ParentSource:
    parent: str
    source: str
    title: str
    source_sha256: str
    clauses: tuple[ClauseSource, ...]


@dataclass(frozen=True)
class VolumeSpec:
    name: str
    parent_ledger: Path
    clause_ledger: Path
    parent_count: int
    parent_key_sha256: str
    parent_row_sha256: str
    clause_count: int
    clause_key_sha256: str
    clause_row_sha256: str


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
        "--base-parent-ledger",
        type=Path,
        default=Path("docs/posix/posix_base_definitions_ledger.tsv"),
    )
    parser.add_argument(
        "--base-clause-ledger",
        type=Path,
        default=Path("docs/posix/posix_base_definitions_clause_ledger.tsv"),
    )
    parser.add_argument(
        "--system-parent-ledger",
        type=Path,
        default=Path("docs/posix/posix_system_interfaces_ledger.tsv"),
    )
    parser.add_argument(
        "--system-clause-ledger",
        type=Path,
        default=Path("docs/posix/posix_system_interfaces_clause_ledger.tsv"),
    )
    parser.add_argument("--derive", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(repository_root: Path, candidate: Path) -> Path:
    if candidate.is_absolute():
        return candidate.resolve()
    return (repository_root / candidate).resolve()


def ordered_sha256(lines: list[str]) -> str:
    return hashlib.sha256("".join(f"{line}\n" for line in lines).encode("ascii")).hexdigest()


def normalize_source_text(raw_text: str) -> str:
    normalized = " ".join(html.unescape(TAG_PATTERN.sub("", raw_text)).split())
    return normalized.encode("ascii", "backslashreplace").decode("ascii")


def source_relative_path(member_name: str) -> str:
    prefix = f"{ARCHIVE_TOP_DIRECTORY}/"
    if not member_name.startswith(prefix):
        raise VerificationError(f"archive member is outside {ARCHIVE_TOP_DIRECTORY}: {member_name}")
    relative = member_name.removeprefix(prefix)
    if PurePosixPath(relative).is_absolute() or ".." in PurePosixPath(relative).parts:
        raise VerificationError(f"unsafe standards source path: {member_name}")
    return relative


def parent_member_names(member_names: set[str], volume: str) -> list[str]:
    relative_names = sorted(source_relative_path(name) for name in member_names)
    if volume == "base-definitions":
        selected = [
            name
            for name in relative_names
            if name.startswith("basedefs/")
            and (
                name.endswith(".h.html")
                or BASE_CHAPTER_PATTERN.fullmatch(name.removeprefix("basedefs/"))
            )
        ]
        expected_count = BASE_PARENT_COUNT
    elif volume == "system-interfaces":
        excluded = {"functions/V2_title.html", "functions/V2_chap01.html", "functions/contents.html"}
        selected = [
            name
            for name in relative_names
            if name.startswith("functions/")
            and name.endswith(".html")
            and name not in excluded
        ]
        expected_count = SYSTEM_PARENT_COUNT
    else:
        raise VerificationError(f"unknown standards volume: {volume}")
    if len(selected) != expected_count:
        raise VerificationError(
            f"{volume} parent source count mismatch: expected {expected_count}, got {len(selected)}"
        )
    return selected


def read_source_text(archive: tarfile.TarFile, source: str) -> tuple[str, str]:
    member_name = f"{ARCHIVE_TOP_DIRECTORY}/{source}"
    source_file = archive.extractfile(member_name)
    if source_file is None:
        raise VerificationError(f"missing standards source member: {member_name}")
    raw_bytes = source_file.read()
    return raw_bytes.decode("latin1"), hashlib.sha256(raw_bytes).hexdigest()


def derive_parent_sources(archive_path: Path, volume: str) -> list[ParentSource]:
    verify_archive_identity(archive_path, expected_sha256=ARCHIVE_SHA256)
    members, _chapter_bytes = read_safe_members(archive_path)
    member_names = {member.name for member in members if member.isfile()}
    sources = parent_member_names(member_names, volume)
    derived: list[ParentSource] = []
    with tarfile.open(archive_path, mode="r:gz") as archive:
        for source in sources:
            source_text, source_sha256 = read_source_text(archive, source)
            title_match = TITLE_PATTERN.search(source_text)
            if title_match is None:
                raise VerificationError(f"missing HTML title in {source}")
            title = normalize_source_text(title_match.group("title"))
            clauses: list[ClauseSource] = []
            for match in HEADING_PATTERN.finditer(source_text):
                clause = match.group("anchor")
                if CLAUSE_PATTERN.fullmatch(clause) is None:
                    raise VerificationError(f"invalid clause anchor in {source}: {clause}")
                clauses.append(
                    ClauseSource(
                        parent=source,
                        clause=clause,
                        title=normalize_source_text(match.group("title")),
                        source_sha256=source_sha256,
                    )
                )
            clause_keys = [item.clause for item in clauses]
            if len(clause_keys) != len(set(clause_keys)):
                raise VerificationError(f"duplicate clause anchor in {source}")
            derived.append(
                ParentSource(
                    parent=source,
                    source=source,
                    title=title,
                    source_sha256=source_sha256,
                    clauses=tuple(clauses),
                )
            )
    return derived


def expected_spec(repository_root: Path, volume: str) -> VolumeSpec:
    if volume == "base-definitions":
        return VolumeSpec(
            name=volume,
            parent_ledger=repository_root / "docs/posix/posix_base_definitions_ledger.tsv",
            clause_ledger=repository_root / "docs/posix/posix_base_definitions_clause_ledger.tsv",
            parent_count=BASE_PARENT_COUNT,
            parent_key_sha256=BASE_PARENT_KEY_SHA256,
            parent_row_sha256=BASE_PARENT_ROW_SHA256,
            clause_count=BASE_CLAUSE_COUNT,
            clause_key_sha256=BASE_CLAUSE_KEY_SHA256,
            clause_row_sha256=BASE_CLAUSE_ROW_SHA256,
        )
    return VolumeSpec(
        name=volume,
        parent_ledger=repository_root / "docs/posix/posix_system_interfaces_ledger.tsv",
        clause_ledger=repository_root / "docs/posix/posix_system_interfaces_clause_ledger.tsv",
        parent_count=SYSTEM_PARENT_COUNT,
        parent_key_sha256=SYSTEM_PARENT_KEY_SHA256,
        parent_row_sha256=SYSTEM_PARENT_ROW_SHA256,
        clause_count=SYSTEM_CLAUSE_COUNT,
        clause_key_sha256=SYSTEM_CLAUSE_KEY_SHA256,
        clause_row_sha256=SYSTEM_CLAUSE_ROW_SHA256,
    )


def parse_ledger(path: Path, expected_columns: tuple[str, ...]) -> list[tuple[str, ...]]:
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
            if fields != expected_columns:
                raise VerificationError(
                    f"{path}: expected header {expected_columns}, got {fields}"
                )
            continue
        if len(fields) != len(expected_columns):
            raise VerificationError(
                f"{path}: line {line_number} has {len(fields)} fields, expected {len(expected_columns)}"
            )
        records.append(fields)
    if not saw_header:
        raise VerificationError(f"{path}: missing ledger header")
    return records


def load_state(
    path: Path,
    expected_columns: tuple[str, ...],
    key_index: int,
    composite_key: bool = False,
) -> dict[str, tuple[str, str, str]]:
    if not path.exists():
        return {}
    rows = parse_ledger(path, expected_columns)
    state: dict[str, tuple[str, str, str]] = {}
    for row in rows:
        key = f"{row[0]}#{row[1]}" if composite_key else row[key_index]
        if key in state:
            raise VerificationError(f"{path}: duplicate state key {key}")
        state[key] = (row[-3], row[-2], row[-1])
    return state


def default_parent_state(volume: str) -> tuple[str, str, str]:
    return (
        "open",
        f"missing:q35_ring3_{volume.replace('-', '_')}_clause_witnesses",
        "Add exact Q35 Ring 3 witnesses for every required clause before closing this parent.",
    )


def default_clause_state(volume: str) -> tuple[str, str, str]:
    return (
        "open",
        f"missing:q35_ring3_{volume.replace('-', '_')}_clause_witness",
        "Add an exact Q35 Ring 3 witness for this clause and close its parent only after all clauses close.",
    )


def parent_rows_for_write(sources: list[ParentSource], spec: VolumeSpec) -> list[tuple[str, ...]]:
    previous = load_state(spec.parent_ledger, PARENT_COLUMNS, 0)
    rows: list[tuple[str, ...]] = []
    for source in sources:
        state, witness, next_action = previous.get(source.parent, default_parent_state(spec.name))
        clause_lines = [f"{item.clause}\t{item.title}" for item in source.clauses]
        rows.append(
            (
                source.parent,
                source.source,
                source.title,
                source.source_sha256,
                str(len(source.clauses)),
                ordered_sha256(clause_lines),
                state,
                witness,
                next_action,
            )
        )
    return rows


def clause_rows_for_write(sources: list[ParentSource], spec: VolumeSpec) -> list[tuple[str, ...]]:
    previous = load_state(spec.clause_ledger, CLAUSE_COLUMNS, 1, composite_key=True)
    rows: list[tuple[str, ...]] = []
    for source in sources:
        for clause in source.clauses:
            state, witness, next_action = previous.get(
                f"{source.parent}#{clause.clause}", default_clause_state(spec.name)
            )
            rows.append(
                (
                    source.parent,
                    clause.clause,
                    clause.title,
                    clause.source_sha256,
                    state,
                    witness,
                    next_action,
                )
            )
    return rows


def write_ledger(path: Path, columns: tuple[str, ...], comments: list[str], rows: list[tuple[str, ...]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [*comments, "\t".join(columns)]
    lines.extend("\t".join(row) for row in rows)
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def derive_ledgers(repository_root: Path, archive_path: Path, spec: VolumeSpec) -> None:
    sources = derive_parent_sources(archive_path, spec.name)
    parent_rows = parent_rows_for_write(sources, spec)
    clause_rows = clause_rows_for_write(sources, spec)
    parent_keys = [row[0] for row in parent_rows]
    parent_identity = [f"{row[0]}\t{row[2]}" for row in parent_rows]
    clause_keys = [f"{row[0]}#{row[1]}" for row in clause_rows]
    clause_identity = [f"{key}\t{row[2]}" for key, row in zip(clause_keys, clause_rows)]
    comments = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        f"# source-archive: {ARCHIVE_NAME}, sha256 {ARCHIVE_SHA256}",
        f"# volume: {spec.name}",
        f"# parent-denominator: {len(parent_rows)}",
        f"# parent-key-sha256: {ordered_sha256(parent_keys)}",
        f"# parent-row-sha256: {ordered_sha256(parent_identity)}",
        f"# clause-denominator: {len(clause_rows)} HTML h2-h5 source headings with tag anchors",
        f"# clause-key-sha256: {ordered_sha256(clause_keys)}",
        f"# clause-row-sha256: {ordered_sha256(clause_identity)}",
        "# state invariant: every parent and clause is exactly one of open or closed",
        "# non-ASCII source titles use ASCII backslash escapes; source bytes remain hash-bound",
    ]
    write_ledger(spec.parent_ledger, PARENT_COLUMNS, comments, parent_rows)
    clause_comments = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        f"# source-archive: {ARCHIVE_NAME}, sha256 {ARCHIVE_SHA256}",
        f"# volume: {spec.name}",
        f"# parent-denominator: {len(parent_rows)}",
        f"# clause-denominator: {len(clause_rows)} HTML h2-h5 source headings with tag anchors",
        f"# clause-key-sha256: {ordered_sha256(clause_keys)}",
        f"# clause-row-sha256: {ordered_sha256(clause_identity)}",
        "# state invariant: every clause is exactly one of open or closed",
        "# non-ASCII source titles use ASCII backslash escapes; source bytes remain hash-bound",
    ]
    write_ledger(spec.clause_ledger, CLAUSE_COLUMNS, clause_comments, clause_rows)


def read_executable_case_ids(repository_root: Path) -> set[str]:
    test_path = repository_root / QEMU_RING3_WITNESS
    try:
        syntax_tree = ast.parse(test_path.read_text(encoding="ascii"), filename=str(test_path))
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise VerificationError(f"cannot parse ASCII Q35 shell test {test_path}: {error}") from error
    assignment = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.Assign)
            and any(
                isinstance(target, ast.Name)
                and target.id == QEMU_RING3_CASE_GROUP
                for target in statement.targets
            )
        ),
        None,
    )
    if assignment is None:
        raise VerificationError(f"{test_path}: missing {QEMU_RING3_CASE_GROUP}")
    try:
        cases = ast.literal_eval(assignment.value)
    except (ValueError, TypeError) as error:
        raise VerificationError(f"{test_path}: Q35 case group is not an ASCII literal") from error
    if not isinstance(cases, tuple):
        raise VerificationError(f"{test_path}: Q35 case group must be a tuple")
    case_ids: set[str] = set()
    for case in cases:
        if not isinstance(case, tuple) or len(case) != 3 or not all(isinstance(value, str) for value in case):
            raise VerificationError(f"{test_path}: malformed Q35 shell case")
        case_id, command, expected = case
        if not case_id or not command or not expected:
            raise VerificationError(f"{test_path}: Q35 shell case has an empty field")
        if case_id in case_ids:
            raise VerificationError(f"{test_path}: duplicate Q35 shell case ID: {case_id}")
        case_ids.add(case_id)
    return case_ids


def validate_witness(
    owner: str,
    witness: str,
    repository_root: Path,
    executable_case_ids: set[str],
) -> list[str]:
    if not witness.startswith("tests:"):
        return [f"{owner}: closed witness must start with 'tests:'"]
    references = witness.removeprefix("tests:").split(",")
    if not references or any(not reference for reference in references):
        return [f"{owner}: closed witness has no test paths"]
    failures: list[str] = []
    exact_q35_case = False
    for reference in references:
        witness_path, separator, case_id = reference.partition("#")
        candidate = PurePosixPath(witness_path)
        if candidate.is_absolute() or ".." in candidate.parts:
            failures.append(f"{owner}: invalid witness path: {reference}")
            continue
        if not (repository_root / candidate).is_file():
            failures.append(f"{owner}: missing witness file: {witness_path}")
        if witness_path == QEMU_RING3_WITNESS:
            if not separator or not case_id or case_id not in executable_case_ids:
                failures.append(f"{owner}: Q35 witness case is not executable: {reference}")
            else:
                exact_q35_case = True
        elif separator:
            failures.append(f"{owner}: non-Q35 witness cannot include a case selector: {reference}")
    if not exact_q35_case:
        failures.append(
            f"{owner}: closed witness must include an exact Q35 Ring 3 case from {QEMU_RING3_WITNESS}"
        )
    if VERIFIER_PATH not in references:
        failures.append(f"{owner}: closed witness must include source verifier {VERIFIER_PATH}")
    return failures


def validate_state_partition(
    rows: list[tuple[str, ...]],
    owner_index: int,
    state_index: int,
    witness_index: int,
    next_action_index: int,
    repository_root: Path,
    owner_label: str,
    executable_case_ids: set[str],
    composite_owner: bool = False,
) -> list[str]:
    failures: list[str] = []
    owners = [
        f"{row[0]}#{row[1]}" if composite_owner else row[owner_index]
        for row in rows
    ]
    duplicates = sorted({owner for owner in owners if owners.count(owner) > 1})
    if duplicates:
        failures.append(f"{owner_label}: duplicate keys: {', '.join(duplicates[:8])}")
    open_keys: set[str] = set()
    closed_keys: set[str] = set()
    for row in rows:
        owner = f"{row[0]}#{row[1]}" if composite_owner else row[owner_index]
        state = row[state_index]
        witness = row[witness_index]
        next_action = row[next_action_index]
        if state not in VALID_STATES:
            failures.append(f"{owner}: invalid state: {state}")
            continue
        if state == "open":
            open_keys.add(owner)
            if not witness.startswith("missing:"):
                failures.append(f"{owner}: open witness must name a missing prerequisite")
            if not next_action or next_action == "-":
                failures.append(f"{owner}: open row lacks a next action")
        else:
            closed_keys.add(owner)
            failures.extend(
                validate_witness(owner, witness, repository_root, executable_case_ids)
            )
            if next_action != "-":
                failures.append(f"{owner}: closed row next_action must be '-'")
    if open_keys & closed_keys:
        failures.append(f"{owner_label}: open and closed partitions overlap")
    if open_keys | closed_keys != set(owners):
        failures.append(f"{owner_label}: state partitions do not cover all keys")
    return failures


def validate_volume(
    repository_root: Path,
    spec: VolumeSpec,
    sources: list[ParentSource],
    parent_rows: list[tuple[str, ...]],
    clause_rows: list[tuple[str, ...]],
    executable_case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    expected_parent_identity = [
        (source.parent, source.source, source.title, source.source_sha256, str(len(source.clauses)), ordered_sha256([f"{item.clause}\t{item.title}" for item in source.clauses]))
        for source in sources
    ]
    observed_parent_identity = [row[:6] for row in parent_rows]
    if observed_parent_identity != expected_parent_identity:
        failures.append(f"{spec.name}: parent source identity differs from the archive")
    expected_clause_identity = [
        (item.parent, item.clause, item.title, item.source_sha256)
        for source in sources
        for item in source.clauses
    ]
    observed_clause_identity = [row[:4] for row in clause_rows]
    if observed_clause_identity != expected_clause_identity:
        failures.append(f"{spec.name}: clause source identity differs from the archive")

    parent_keys = [row[0] for row in parent_rows]
    clause_keys = [f"{row[0]}#{row[1]}" for row in clause_rows]
    if len(parent_rows) != spec.parent_count:
        failures.append(f"{spec.name}: parent denominator mismatch: expected {spec.parent_count}, got {len(parent_rows)}")
    if len(clause_rows) != spec.clause_count:
        failures.append(f"{spec.name}: clause denominator mismatch: expected {spec.clause_count}, got {len(clause_rows)}")
    parent_key_hash = ordered_sha256(parent_keys)
    clause_key_hash = ordered_sha256(clause_keys)
    parent_row_hash = ordered_sha256([f"{row[0]}\t{row[2]}" for row in parent_rows])
    clause_row_hash = ordered_sha256([f"{key}\t{row[2]}" for key, row in zip(clause_keys, clause_rows)])
    if parent_key_hash != spec.parent_key_sha256:
        failures.append(f"{spec.name}: parent key hash mismatch: expected {spec.parent_key_sha256}, got {parent_key_hash}")
    if parent_row_hash != spec.parent_row_sha256:
        failures.append(f"{spec.name}: parent row hash mismatch: expected {spec.parent_row_sha256}, got {parent_row_hash}")
    if clause_key_hash != spec.clause_key_sha256:
        failures.append(f"{spec.name}: clause key hash mismatch: expected {spec.clause_key_sha256}, got {clause_key_hash}")
    if clause_row_hash != spec.clause_row_sha256:
        failures.append(f"{spec.name}: clause row hash mismatch: expected {spec.clause_row_sha256}, got {clause_row_hash}")
    failures.extend(
        validate_state_partition(
            parent_rows,
            0,
            6,
            7,
            8,
            repository_root,
            f"{spec.name} parent ledger",
            executable_case_ids,
        )
    )
    failures.extend(
        validate_state_partition(
            clause_rows,
            1,
            4,
            5,
            6,
            repository_root,
            f"{spec.name} clause ledger",
            executable_case_ids,
            composite_owner=True,
        )
    )
    clauses_by_parent: dict[str, list[tuple[str, ...]]] = {}
    for row in clause_rows:
        clauses_by_parent.setdefault(row[0], []).append(row)
    for row in parent_rows:
        if row[6] == "closed":
            open_clauses = [f"{item[0]}#{item[1]}" for item in clauses_by_parent.get(row[0], []) if item[4] != "closed"]
            if open_clauses:
                failures.append(f"{row[0]}: parent is closed while clauses remain open: {', '.join(open_clauses[:4])}")
    return failures


def verify_ledgers(repository_root: Path, archive_path: Path, specs: tuple[VolumeSpec, ...]) -> tuple[list[str], dict[str, tuple[list[tuple[str, ...]], list[tuple[str, ...]]]]]:
    try:
        verify_archive_identity(archive_path, expected_sha256=ARCHIVE_SHA256)
        results: dict[str, tuple[list[tuple[str, ...]], list[tuple[str, ...]]]] = {}
        failures: list[str] = []
        executable_case_ids = read_executable_case_ids(repository_root)
        for spec in specs:
            sources = derive_parent_sources(archive_path, spec.name)
            parent_rows = parse_ledger(spec.parent_ledger, PARENT_COLUMNS)
            clause_rows = parse_ledger(spec.clause_ledger, CLAUSE_COLUMNS)
            failures.extend(
                validate_volume(
                    repository_root,
                    spec,
                    sources,
                    parent_rows,
                    clause_rows,
                    executable_case_ids,
                )
            )
            results[spec.name] = (parent_rows, clause_rows)
        return failures, results
    except (OSError, tarfile.TarError, VerificationError) as error:
        return [str(error)], {}


def expect_failure(name: str, failures: list[str], fragment: str) -> None:
    if not any(fragment in failure for failure in failures):
        raise AssertionError(f"{name}: expected {fragment!r}, got {failures}")


def run_self_test(repository_root: Path, archive_path: Path, specs: tuple[VolumeSpec, ...]) -> None:
    failures, results = verify_ledgers(repository_root, archive_path, specs)
    if failures:
        raise AssertionError(f"known-good ledgers failed: {failures}")
    base_spec = specs[0]
    base_sources = derive_parent_sources(archive_path, base_spec.name)
    parent_rows, clause_rows = results[base_spec.name]
    executable_case_ids = read_executable_case_ids(repository_root)

    duplicate_parent = parent_rows + [parent_rows[0]]
    expect_failure(
        "duplicate parent",
        validate_volume(
            repository_root,
            base_spec,
            base_sources,
            duplicate_parent,
            clause_rows,
            executable_case_ids,
        ),
        "parent denominator mismatch",
    )
    missing_clause = clause_rows[1:]
    expect_failure(
        "missing clause",
        validate_volume(
            repository_root,
            base_spec,
            base_sources,
            parent_rows,
            missing_clause,
            executable_case_ids,
        ),
        "clause source identity differs",
    )
    changed_title = list(parent_rows[0])
    changed_title[2] = "Mutated source title"
    mutated_parent_rows = [tuple(changed_title), *parent_rows[1:]]
    expect_failure(
        "source title mutation",
        validate_volume(
            repository_root,
            base_spec,
            base_sources,
            mutated_parent_rows,
            clause_rows,
            executable_case_ids,
        ),
        "parent source identity differs",
    )
    closed_parent = list(parent_rows[0])
    closed_parent[6] = "closed"
    closed_parent[7] = f"tests:{QEMU_RING3_WITNESS}#{QEMU_RING3_CASE},{VERIFIER_PATH}"
    closed_parent[8] = "-"
    mutated_closed_rows = [tuple(closed_parent), *parent_rows[1:]]
    expect_failure(
        "parent dependency mutation",
        validate_volume(
            repository_root,
            base_spec,
            base_sources,
            mutated_closed_rows,
            clause_rows,
            executable_case_ids,
        ),
        "parent is closed while clauses remain open",
    )
    print("SUSv4 Base Definitions/System Interfaces ledger mutation self-test passed.")


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive)
    specs = (
        VolumeSpec(
            name="base-definitions",
            parent_ledger=resolve_path(repository_root, arguments.base_parent_ledger),
            clause_ledger=resolve_path(repository_root, arguments.base_clause_ledger),
            parent_count=BASE_PARENT_COUNT,
            parent_key_sha256=BASE_PARENT_KEY_SHA256,
            parent_row_sha256=BASE_PARENT_ROW_SHA256,
            clause_count=BASE_CLAUSE_COUNT,
            clause_key_sha256=BASE_CLAUSE_KEY_SHA256,
            clause_row_sha256=BASE_CLAUSE_ROW_SHA256,
        ),
        VolumeSpec(
            name="system-interfaces",
            parent_ledger=resolve_path(repository_root, arguments.system_parent_ledger),
            clause_ledger=resolve_path(repository_root, arguments.system_clause_ledger),
            parent_count=SYSTEM_PARENT_COUNT,
            parent_key_sha256=SYSTEM_PARENT_KEY_SHA256,
            parent_row_sha256=SYSTEM_PARENT_ROW_SHA256,
            clause_count=SYSTEM_CLAUSE_COUNT,
            clause_key_sha256=SYSTEM_CLAUSE_KEY_SHA256,
            clause_row_sha256=SYSTEM_CLAUSE_ROW_SHA256,
        ),
    )
    if not archive_path.is_file():
        print(f"SKIP: SUSv4 archive not found: {archive_path}")
        return 77
    try:
        if arguments.derive:
            for spec in specs:
                derive_ledgers(repository_root, archive_path, spec)
        failures, _results = verify_ledgers(repository_root, archive_path, specs)
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(repository_root, archive_path, specs)
            return 0
    except (OSError, tarfile.TarError, VerificationError, AssertionError) as error:
        print(f"SUSv4 Base Definitions/System Interfaces verification failed: {error}", file=sys.stderr)
        return 1
    print(
        "SUSv4 Base Definitions/System Interfaces ledgers verified: "
        f"{BASE_PARENT_COUNT}/{BASE_PARENT_COUNT} Base Definitions parents, "
        f"{SYSTEM_PARENT_COUNT}/{SYSTEM_PARENT_COUNT} System Interfaces parents, "
        f"{BASE_CLAUSE_COUNT} and {SYSTEM_CLAUSE_COUNT} clause rows."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
