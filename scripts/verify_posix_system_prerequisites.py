#!/usr/bin/env python3
"""Verify the finite prerequisite frontier for POSIX Ring 3 conformance."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys
from typing import Iterable

EXPECTED_COLUMNS = ("prerequisite", "state", "evidence", "witness", "next_action")
EXPECTED_KEYS = (
    "filesystem",
    "ipc",
    "libc",
    "processes",
    "signals",
    "sockets",
    "terminals",
)
EXPECTED_KEY_SHA256 = "f5ef3c3f693787e0e6b4bd31eb49dd026a47283959c328879d422989739eb01c"
VALID_STATES = {"open", "closed"}
QEMU_RING3_WITNESS = "test/boot/x86_64_shell_test.py"
QEMU_RING3_MATRIX_MARKERS = {
    "filesystem": "require_filesystem_q35_ring3_lifecycle_matrix",
    "ipc": "require_ipc_q35_ring3_cross_process_matrix",
    "libc": "require_libc_q35_ring3_abi_behavior_matrix",
    "processes": "require_processes_q35_ring3_exec_wait_signal_matrix",
    "signals": "require_signals_q35_ring3_delivery_masking_matrix",
    "sockets": "require_sockets_q35_ring3_syscall_lifecycle_matrix",
    "terminals": "require_terminals_q35_ring3_session_termios_matrix",
}


class VerificationError(RuntimeError):
    """Report a prerequisite ledger contract failure."""


def parse_args() -> argparse.Namespace:
    repository_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=repository_root)
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        default=pathlib.Path("docs/posix/posix_system_prerequisites.tsv"),
    )
    parser.add_argument("--require-closure", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_path(repository_root: pathlib.Path, candidate: pathlib.Path) -> pathlib.Path:
    if candidate.is_absolute():
        return candidate.resolve()
    return (repository_root / candidate).resolve()


def ordered_sha256(lines: Iterable[str]) -> str:
    return hashlib.sha256("".join(f"{line}\n" for line in lines).encode("ascii")).hexdigest()


def parse_ledger(path: pathlib.Path) -> list[tuple[str, ...]]:
    try:
        text = path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        raise VerificationError(f"cannot read ASCII prerequisite ledger {path}: {error}") from error
    rows: list[tuple[str, ...]] = []
    saw_header = False
    for line_number, line in enumerate(text.splitlines(), start=1):
        if not line or line.startswith("#"):
            continue
        fields = tuple(line.split("\t"))
        if not saw_header:
            saw_header = True
            if fields != EXPECTED_COLUMNS:
                raise VerificationError(f"{path}: expected header {EXPECTED_COLUMNS}, got {fields}")
            continue
        if len(fields) != len(EXPECTED_COLUMNS):
            raise VerificationError(
                f"{path}: line {line_number} has {len(fields)} fields, expected {len(EXPECTED_COLUMNS)}"
            )
        rows.append(fields)
    if not saw_header:
        raise VerificationError(f"{path}: missing prerequisite ledger header")
    return rows


def validate_path_list(
    owner: str, value: str, repository_root: pathlib.Path, field_name: str
) -> list[str]:
    if not value.startswith("tests:"):
        return [f"{owner}: {field_name} must start with 'tests:'"]
    references = value.removeprefix("tests:").split(",")
    failures: list[str] = []
    for reference in references:
        candidate = pathlib.PurePosixPath(reference.partition("#")[0])
        if candidate.is_absolute() or ".." in candidate.parts:
            failures.append(f"{owner}: invalid {field_name} path: {reference}")
            continue
        if not (repository_root / candidate).is_file():
            failures.append(f"{owner}: missing {field_name} path: {reference}")
    return failures


def validate_rows(
    rows: list[tuple[str, ...]], repository_root: pathlib.Path, require_closure: bool = False
) -> list[str]:
    failures: list[str] = []
    keys = [row[0] for row in rows]
    duplicate_keys = sorted({key for key in keys if keys.count(key) > 1})
    if duplicate_keys:
        failures.append(f"duplicate prerequisite keys: {', '.join(duplicate_keys)}")
    if keys != list(EXPECTED_KEYS):
        missing = sorted(set(EXPECTED_KEYS) - set(keys))
        unexpected = sorted(set(keys) - set(EXPECTED_KEYS))
        if missing:
            failures.append(f"missing prerequisite keys: {', '.join(missing)}")
        if unexpected:
            failures.append(f"unexpected prerequisite keys: {', '.join(unexpected)}")
        if not missing and not unexpected:
            failures.append("prerequisite keys are not in official order")
    if len(rows) != len(EXPECTED_KEYS):
        failures.append(
            f"prerequisite denominator mismatch: expected {len(EXPECTED_KEYS)}, got {len(rows)}"
        )
    observed_hash = ordered_sha256(keys)
    if observed_hash != EXPECTED_KEY_SHA256:
        failures.append(
            f"prerequisite key hash mismatch: expected {EXPECTED_KEY_SHA256}, got {observed_hash}"
        )

    open_keys: set[str] = set()
    closed_keys: set[str] = set()
    for prerequisite, state, evidence, witness, next_action in rows:
        if state not in VALID_STATES:
            failures.append(f"{prerequisite}: invalid state: {state}")
            continue
        failures.extend(validate_path_list(prerequisite, evidence, repository_root, "evidence"))
        if state == "open":
            open_keys.add(prerequisite)
            if not witness.startswith("missing:"):
                failures.append(f"{prerequisite}: open witness must name a missing prerequisite")
            if not next_action or next_action == "-":
                failures.append(f"{prerequisite}: open row lacks a next action")
        else:
            closed_keys.add(prerequisite)
            failures.extend(validate_path_list(prerequisite, witness, repository_root, "closed witness"))
            witness_references = witness.removeprefix("tests:").split(",")
            witness_paths = [reference.partition("#")[0] for reference in witness_references]
            if QEMU_RING3_WITNESS not in witness_paths:
                failures.append(f"{prerequisite}: closed witness must include exact Q35 Ring 3 test {QEMU_RING3_WITNESS}")
            expected_marker = f"{QEMU_RING3_WITNESS}#{QEMU_RING3_MATRIX_MARKERS[prerequisite]}"
            if expected_marker not in witness_references:
                failures.append(
                    f"{prerequisite}: closed witness must include exact Q35 Ring 3 matrix marker "
                    f"{expected_marker}"
                )
            if next_action != "-":
                failures.append(f"{prerequisite}: closed row next_action must be '-'")
    if open_keys & closed_keys:
        failures.append("open and closed prerequisite partitions overlap")
    if open_keys | closed_keys != set(keys):
        failures.append("open and closed prerequisite partitions do not cover all rows")
    if require_closure and open_keys:
        failures.append(f"system prerequisites remain open: {', '.join(sorted(open_keys))}")
    return failures


def require_failure(name: str, rows: list[tuple[str, ...]], repository_root: pathlib.Path, fragment: str) -> None:
    failures = validate_rows(rows, repository_root)
    if not any(fragment in failure for failure in failures):
        raise AssertionError(f"{name}: expected {fragment!r}, got {failures}")


def run_self_test(rows: list[tuple[str, ...]], repository_root: pathlib.Path) -> None:
    require_failure(
        "missing prerequisite",
        rows[1:],
        repository_root,
        "denominator mismatch",
    )
    require_failure(
        "duplicate prerequisite",
        rows + [rows[0]],
        repository_root,
        "duplicate prerequisite keys",
    )
    mutated = list(rows[0])
    mutated[1] = "closed"
    mutated[2] = "tests:test/boot/x86_64_shell_test.py"
    mutated[3] = "tests:test/boot/x86_64_shell_test.py"
    mutated[4] = "-"
    marker_failures = validate_rows([tuple(mutated), *rows[1:]], repository_root)
    if not any("matrix marker" in failure for failure in marker_failures):
        raise AssertionError(
            "missing prerequisite witness marker was not rejected: "
            f"got {marker_failures}"
        )
    open_mutated = list(rows[0])
    open_mutated[1] = "open"
    open_mutated[3] = "missing:mutated_q35_ring3_witness"
    open_mutated[4] = "Restore the exact Q35 Ring 3 prerequisite witness."
    closure_failures = validate_rows(
        [tuple(open_mutated), *rows[1:]], repository_root, require_closure=True
    )
    if not any("system prerequisites remain open" in failure for failure in closure_failures):
        raise AssertionError(
            "false prerequisite closure: expected an open-prerequisite failure, "
            f"got {closure_failures}"
        )
    print("POSIX system-prerequisite ledger mutation self-test passed.")


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    ledger_path = resolve_path(repository_root, arguments.ledger)
    try:
        rows = parse_ledger(ledger_path)
        failures = validate_rows(rows, repository_root, arguments.require_closure)
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(rows, repository_root)
            return 0
    except (OSError, UnicodeDecodeError, VerificationError, AssertionError) as error:
        print(f"POSIX system-prerequisite verification failed: {error}", file=sys.stderr)
        return 1
    open_count = sum(row[1] == "open" for row in rows)
    closed_count = sum(row[1] == "closed" for row in rows)
    print(
        "POSIX system-prerequisite ledger verified: "
        f"{closed_count} closed, {open_count} open across {len(rows)} prerequisites."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
