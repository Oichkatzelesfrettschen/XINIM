#!/usr/bin/env python3
"""Verify bounded Issue 7 closure and the expanded SUSv4 closure boundary."""

from __future__ import annotations

import argparse
import pathlib
import sys

from verify_posix_base_system_ledgers import (
    BASE_CLAUSE_COUNT,
    BASE_PARENT_COUNT,
    SYSTEM_CLAUSE_COUNT,
    SYSTEM_PARENT_COUNT,
    expected_spec,
    verify_ledgers,
)
from verify_posix_issue7_utility_ledger import (
    EXPECTED_COUNT as UTILITY_COUNT,
    validate_ledger as validate_utility_ledger,
)
from verify_posix_shell_language_ledger import (
    EXPECTED_COUNT as SHELL_COUNT,
    derive_source_rows as derive_shell_source_rows,
    read_chapter_text,
    read_executable_case_ids,
    validate_ledger as validate_shell_ledger,
)
from verify_posix_system_prerequisites import (
    EXPECTED_KEYS as PREREQUISITE_KEYS,
    parse_ledger as parse_prerequisite_ledger,
    validate_rows as validate_prerequisite_rows,
)

ARCHIVE_RELATIVE_PATH = pathlib.PurePosixPath("data/external/posix/susv4-2018.tgz")
SHELL_LEDGER_RELATIVE_PATH = pathlib.PurePosixPath("docs/posix/posix_shell_language_ledger.tsv")
UTILITY_LEDGER_RELATIVE_PATH = pathlib.PurePosixPath("docs/posix/posix_issue7_utility_ledger.tsv")
PREREQUISITE_LEDGER_RELATIVE_PATH = pathlib.PurePosixPath("docs/posix/posix_system_prerequisites.tsv")
QEMU_RING3_WITNESS = "test/boot/x86_64_shell_test.py"
BOUNDED_PARENT_COUNT = SHELL_COUNT + UTILITY_COUNT
EXPANDED_PARENT_COUNT = BASE_PARENT_COUNT + SYSTEM_PARENT_COUNT
EXPANDED_CLAUSE_COUNT = BASE_CLAUSE_COUNT + SYSTEM_CLAUSE_COUNT


class VerificationError(RuntimeError):
    """Report a conformance closure contract failure."""


def parse_args() -> argparse.Namespace:
    repository_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=repository_root)
    parser.add_argument(
        "--archive", type=pathlib.Path, default=ARCHIVE_RELATIVE_PATH
    )
    parser.add_argument(
        "--shell-ledger", type=pathlib.Path, default=SHELL_LEDGER_RELATIVE_PATH
    )
    parser.add_argument(
        "--utility-ledger", type=pathlib.Path, default=UTILITY_LEDGER_RELATIVE_PATH
    )
    parser.add_argument(
        "--prerequisite-ledger",
        type=pathlib.Path,
        default=PREREQUISITE_LEDGER_RELATIVE_PATH,
    )
    parser.add_argument("--require-bounded-closure", action="store_true")
    parser.add_argument("--require-full-closure", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(repository_root: pathlib.Path, candidate: pathlib.Path) -> pathlib.Path:
    if candidate.is_absolute():
        return candidate.resolve()
    return (repository_root / candidate).resolve()


def parse_state_rows(path: pathlib.Path, state_index: int) -> list[tuple[str, ...]]:
    try:
        text = path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        raise VerificationError(f"cannot read ASCII ledger {path}: {error}") from error
    rows: list[tuple[str, ...]] = []
    saw_header = False
    for line_number, line in enumerate(text.splitlines(), start=1):
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        if not saw_header:
            saw_header = True
            continue
        if len(fields) <= state_index:
            raise VerificationError(f"{path}: malformed row at line {line_number}")
        rows.append(fields)
    if not saw_header:
        raise VerificationError(f"{path}: missing ledger header")
    return rows


def closed_count(rows: list[tuple[str, ...]], state_index: int) -> int:
    return sum(row[state_index] == "closed" for row in rows)


def open_keys(rows: list[tuple[str, ...]], state_index: int, key_index: int = 0) -> list[str]:
    return [row[key_index] for row in rows if row[state_index] == "open"]


def require_q35_witnesses(
    rows: list[tuple[str, ...]], state_index: int, witness_index: int, label: str
) -> list[str]:
    failures: list[str] = []
    for row in rows:
        if row[state_index] == "closed":
            witness = row[witness_index]
            references = witness.removeprefix("tests:").removeprefix("test-group:").split(",")
            witness_paths = [reference.partition("#")[0] for reference in references]
            if QEMU_RING3_WITNESS not in witness_paths:
                failures.append(f"{label} {row[0]} lacks exact Q35 Ring 3 witness {QEMU_RING3_WITNESS}")
    return failures


def closure_gate_failures(
    shell_closed: int,
    utility_closed: int,
    prerequisite_closed: int,
    expanded_closed: int,
    require_bounded: bool,
    require_full: bool,
) -> list[str]:
    failures: list[str] = []
    bounded_closed = shell_closed + utility_closed
    if (require_bounded or require_full) and bounded_closed != BOUNDED_PARENT_COUNT:
        failures.append(
            "bounded Issue 7 Shell and Utilities closure requires "
            f"{BOUNDED_PARENT_COUNT}/{BOUNDED_PARENT_COUNT} parent rows; "
            f"observed {bounded_closed}/{BOUNDED_PARENT_COUNT}"
        )
    if (require_bounded or require_full) and prerequisite_closed != len(PREREQUISITE_KEYS):
        failures.append(
            "bounded Issue 7 closure depends on all seven system prerequisites; "
            f"observed {prerequisite_closed}/{len(PREREQUISITE_KEYS)} closed"
        )
    if require_full and expanded_closed != EXPANDED_PARENT_COUNT + EXPANDED_CLAUSE_COUNT:
        expanded_total = EXPANDED_PARENT_COUNT + EXPANDED_CLAUSE_COUNT
        failures.append(
            "full SUSv4/POSIX closure requires the expanded Base Definitions and "
            f"System Interfaces denominator {expanded_total}/{expanded_total}; "
            f"observed {expanded_closed}/{expanded_total}"
        )
    return failures


def verify_inputs(
    repository_root: pathlib.Path,
    archive_path: pathlib.Path,
    shell_ledger: pathlib.Path,
    utility_ledger: pathlib.Path,
    prerequisite_ledger: pathlib.Path,
) -> tuple[list[str], dict[str, object]]:
    failures: list[str] = []
    shell_source_rows = derive_shell_source_rows(read_chapter_text(archive_path))
    executable_case_ids = read_executable_case_ids(repository_root)
    failures.extend(
        validate_shell_ledger(
            shell_ledger, shell_source_rows, repository_root, executable_case_ids
        )
    )
    failures.extend(validate_utility_ledger(utility_ledger, repository_root))
    prerequisite_rows = parse_prerequisite_ledger(prerequisite_ledger)
    failures.extend(validate_prerequisite_rows(prerequisite_rows, repository_root))
    base_spec = expected_spec(repository_root, "base-definitions")
    system_spec = expected_spec(repository_root, "system-interfaces")
    expanded_failures, expanded_rows = verify_ledgers(
        repository_root, archive_path, (base_spec, system_spec)
    )
    failures.extend(expanded_failures)
    shell_rows = parse_state_rows(shell_ledger, 2)
    utility_rows = parse_state_rows(utility_ledger, 1)
    failures.extend(require_q35_witnesses(shell_rows, 2, 3, "shell row"))
    failures.extend(require_q35_witnesses(utility_rows, 1, 2, "utility row"))
    expanded_closed = 0
    for rows in expanded_rows.values():
        parent_rows, clause_rows = rows
        expanded_closed += closed_count(parent_rows, 6)
        expanded_closed += closed_count(clause_rows, 4)
    status = {
        "shell_rows": shell_rows,
        "utility_rows": utility_rows,
        "prerequisite_rows": prerequisite_rows,
        "shell_closed": closed_count(shell_rows, 2),
        "utility_closed": closed_count(utility_rows, 1),
        "prerequisite_closed": closed_count(prerequisite_rows, 1),
        "expanded_closed": expanded_closed,
    }
    return failures, status


def run_self_test(status: dict[str, object]) -> None:
    shell_closed = int(status["shell_closed"])
    utility_closed = int(status["utility_closed"])
    prerequisite_closed = int(status["prerequisite_closed"])
    expanded_closed = int(status["expanded_closed"])
    failures = closure_gate_failures(
        shell_closed,
        utility_closed,
        prerequisite_closed,
        expanded_closed,
        require_bounded=True,
        require_full=False,
    )
    if not any("245/245" in failure for failure in failures):
        raise AssertionError(f"bounded closure mutation was not rejected: {failures}")
    full_failures = closure_gate_failures(
        BOUNDED_PARENT_COUNT // 2,
        BOUNDED_PARENT_COUNT - BOUNDED_PARENT_COUNT // 2,
        len(PREREQUISITE_KEYS),
        expanded_closed,
        require_bounded=False,
        require_full=True,
    )
    if not any("expanded Base Definitions" in failure for failure in full_failures):
        raise AssertionError(f"expanded closure mutation was not rejected: {full_failures}")
    print("SUSv4 conformance closure mutation self-test passed.")


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive)
    shell_ledger = resolve_path(repository_root, arguments.shell_ledger)
    utility_ledger = resolve_path(repository_root, arguments.utility_ledger)
    prerequisite_ledger = resolve_path(repository_root, arguments.prerequisite_ledger)
    if not archive_path.is_file():
        print(f"SKIP: SUSv4 archive not found: {archive_path}")
        return 77
    try:
        failures, status = verify_inputs(
            repository_root,
            archive_path,
            shell_ledger,
            utility_ledger,
            prerequisite_ledger,
        )
        if failures:
            raise VerificationError("\n".join(failures))
        gate_failures = closure_gate_failures(
            int(status["shell_closed"]),
            int(status["utility_closed"]),
            int(status["prerequisite_closed"]),
            int(status["expanded_closed"]),
            arguments.require_bounded_closure,
            arguments.require_full_closure,
        )
        if gate_failures:
            raise VerificationError("\n".join(gate_failures))
        if arguments.self_test:
            run_self_test(status)
            return 0
    except (OSError, UnicodeDecodeError, VerificationError, AssertionError) as error:
        print(f"SUSv4 conformance closure verification failed: {error}", file=sys.stderr)
        return 1
    shell_closed = int(status["shell_closed"])
    utility_closed = int(status["utility_closed"])
    prerequisite_closed = int(status["prerequisite_closed"])
    expanded_closed = int(status["expanded_closed"])
    print(
        "SUSv4 conformance closure status: "
        f"Issue 7 Shell and Utilities {shell_closed + utility_closed}/{BOUNDED_PARENT_COUNT} "
        f"parent rows closed ({SHELL_COUNT} shell + {UTILITY_COUNT} utility); "
        f"system prerequisites {prerequisite_closed}/{len(PREREQUISITE_KEYS)}; "
        f"expanded Base Definitions/System Interfaces {expanded_closed}/"
        f"{EXPANDED_PARENT_COUNT + EXPANDED_CLAUSE_COUNT} rows."
    )
    if shell_closed + utility_closed != BOUNDED_PARENT_COUNT:
        print(
            "Bounded Issue 7 Shell and Utilities closure is not claimed; "
            "245/245 parent rows and prerequisite gates are required."
        )
    if expanded_closed != EXPANDED_PARENT_COUNT + EXPANDED_CLAUSE_COUNT:
        print(
            "Full SUSv4/POSIX conformance is not claimed; the expanded "
            "Base Definitions and System Interfaces denominator remains open."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
