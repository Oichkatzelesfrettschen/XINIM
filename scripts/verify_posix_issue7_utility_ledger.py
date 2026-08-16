#!/usr/bin/env python3
"""Verify the finite SUSv4 Issue 7 utility denominator and its closure states."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys
import tempfile

EXPECTED_COLUMNS = ("utility", "state", "witness", "next_action")
EXPECTED_COUNT = 175
EXPECTED_KEY_SHA256 = "e1c7db29258b2e7e869d9183306e2462261a87d598750d457722824f7b655e7d"
UTILITY_PATTERN = re.compile(r"[a-z][a-z0-9]*")
VALID_STATES = {"open", "closed"}
REQUIREMENTS_COLUMNS = (
    "requirement",
    "title",
    "source_ref",
    "source_sha256",
    "applicability",
    "state",
    "witness",
    "next_action",
)


def parse_args() -> argparse.Namespace:
    script_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=script_root)
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        default=pathlib.Path("docs/posix/posix_issue7_utility_ledger.tsv"),
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def resolve_ledger(repo_root: pathlib.Path, ledger: pathlib.Path) -> pathlib.Path:
    return ledger if ledger.is_absolute() else repo_root / ledger


def validate_closed_witness(
    utility: str, witness: str, repo_root: pathlib.Path
) -> list[str]:
    if not witness.startswith("tests:"):
        return [f"{utility}: closed witness must start with 'tests:'"]
    witness_paths = witness.removeprefix("tests:").split(",")
    if not witness_paths or any(not path for path in witness_paths):
        return [f"{utility}: closed witness has no test paths"]
    failures: list[str] = []
    for relative_path in witness_paths:
        candidate = pathlib.PurePosixPath(relative_path)
        if candidate.is_absolute() or ".." in candidate.parts:
            failures.append(f"{utility}: invalid witness path: {relative_path}")
            continue
        if not (repo_root / candidate).is_file():
            failures.append(f"{utility}: missing witness file: {relative_path}")
    requirements_path = pathlib.PurePosixPath(
        "docs/posix"
    ) / f"{utility}_requirements.tsv"
    verifier_path = pathlib.PurePosixPath(
        "scripts"
    ) / f"verify_posix_{utility}_requirements.py"
    if str(requirements_path) not in witness_paths:
        failures.append(f"{utility}: closed witness omits its finite subledger")
    if str(verifier_path) not in witness_paths:
        failures.append(f"{utility}: closed witness omits its source verifier")
    failures.extend(
        validate_requirement_states(utility, repo_root / requirements_path)
    )
    return failures


def validate_requirements_text(utility: str, requirements_text: str) -> list[str]:
    failures: list[str] = []
    data_lines = [
        line
        for line in requirements_text.splitlines()
        if line and not line.startswith("#")
    ]
    if not data_lines:
        return [f"{utility}: finite subledger is empty"]
    if tuple(data_lines[0].split("\t")) != REQUIREMENTS_COLUMNS:
        return [f"{utility}: finite subledger header mismatch"]
    required_count = 0
    for line_number, line in enumerate(data_lines[1:], start=2):
        fields = line.split("\t")
        if len(fields) != len(REQUIREMENTS_COLUMNS):
            failures.append(
                f"{utility}: finite subledger line {line_number} is malformed"
            )
            continue
        requirement = fields[0]
        applicability = fields[4]
        state = fields[5]
        if applicability == "required":
            required_count += 1
            if state != "closed":
                failures.append(
                    f"{utility}: mandatory subledger row is not closed: {requirement}"
                )
    if required_count == 0:
        failures.append(f"{utility}: finite subledger has no mandatory rows")
    return failures


def validate_requirement_states(
    utility: str, requirements_path: pathlib.Path
) -> list[str]:
    try:
        requirements_text = requirements_path.read_text(encoding="ascii")
    except (OSError, UnicodeDecodeError) as error:
        return [f"{utility}: cannot read finite ASCII subledger: {error}"]
    return validate_requirements_text(utility, requirements_text)


def validate_ledger(ledger_path: pathlib.Path, repo_root: pathlib.Path) -> list[str]:
    failures: list[str] = []
    try:
        raw_bytes = ledger_path.read_bytes()
        text = raw_bytes.decode("ascii")
    except (OSError, UnicodeDecodeError) as error:
        return [f"cannot read ASCII ledger {ledger_path}: {error}"]

    rows: list[tuple[str, str, str, str]] = []
    saw_header = False
    for line_number, line in enumerate(text.splitlines(), start=1):
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
        utility, state, witness, next_action = fields
        rows.append((utility, state, witness, next_action))

    if not saw_header:
        return failures + ["missing ledger header"]

    keys = [row[0] for row in rows]
    duplicate_keys = sorted({key for key in keys if keys.count(key) > 1})
    if duplicate_keys:
        failures.append(f"duplicate utility keys: {', '.join(duplicate_keys)}")
    if keys != sorted(keys):
        failures.append("utility keys are not in ASCII lexical order")
    if len(keys) != EXPECTED_COUNT:
        failures.append(
            f"denominator count mismatch: expected {EXPECTED_COUNT}, got {len(keys)}"
        )
    key_bytes = "".join(f"{key}\n" for key in keys).encode("ascii")
    observed_hash = hashlib.sha256(key_bytes).hexdigest()
    if observed_hash != EXPECTED_KEY_SHA256:
        failures.append(
            "denominator key hash mismatch: "
            f"expected {EXPECTED_KEY_SHA256}, got {observed_hash}"
        )

    open_keys: set[str] = set()
    closed_keys: set[str] = set()
    for utility, state, witness, next_action in rows:
        if UTILITY_PATTERN.fullmatch(utility) is None:
            failures.append(f"invalid utility key: {utility}")
        if state not in VALID_STATES:
            failures.append(f"{utility}: invalid state: {state}")
            continue
        if state == "open":
            open_keys.add(utility)
            if not witness.startswith("missing:"):
                failures.append(
                    f"{utility}: open witness must name a 'missing:' prerequisite"
                )
            if not next_action or next_action == "-":
                failures.append(f"{utility}: open row lacks a next action")
        else:
            closed_keys.add(utility)
            failures.extend(validate_closed_witness(utility, witness, repo_root))
            if next_action != "-":
                failures.append(f"{utility}: closed row next_action must be '-'")

    if open_keys & closed_keys:
        failures.append("open and closed partitions overlap")
    if open_keys | closed_keys != set(keys):
        failures.append("open and closed partitions do not cover the denominator")
    return failures


def require_failure(
    name: str,
    ledger_text: str,
    expected_fragment: str,
    repo_root: pathlib.Path,
) -> None:
    with tempfile.TemporaryDirectory(
        prefix="xinim-posix-issue7-utility-ledger-"
    ) as temp_dir:
        candidate = pathlib.Path(temp_dir) / "ledger.tsv"
        candidate.write_text(ledger_text, encoding="ascii")
        failures = validate_ledger(candidate, repo_root)
    if not any(expected_fragment in failure for failure in failures):
        raise AssertionError(
            f"{name}: expected failure containing {expected_fragment!r}, got {failures}"
        )


def run_self_test(ledger_path: pathlib.Path, repo_root: pathlib.Path) -> None:
    original = ledger_path.read_text(encoding="ascii")
    lines = original.splitlines()
    header_index = lines.index("\t".join(EXPECTED_COLUMNS))
    first_row_index = header_index + 1
    first_row = lines[first_row_index]

    duplicate_lines = lines.copy()
    duplicate_lines.insert(first_row_index + 1, first_row)
    require_failure(
        "duplicate key",
        "\n".join(duplicate_lines) + "\n",
        "duplicate utility keys",
        repo_root,
    )

    missing_witness_lines = lines.copy()
    fields = missing_witness_lines[first_row_index].split("\t")
    fields[1] = "closed"
    fields[2] = "missing:full_ring3_conformance"
    fields[3] = "-"
    missing_witness_lines[first_row_index] = "\t".join(fields)
    require_failure(
        "closed witness",
        "\n".join(missing_witness_lines) + "\n",
        "closed witness must start with 'tests:'",
        repo_root,
    )

    weak_witness_lines = lines.copy()
    fields = weak_witness_lines[first_row_index].split("\t")
    fields[1] = "closed"
    fields[2] = "tests:test/boot/x86_64_shell_test.py"
    fields[3] = "-"
    weak_witness_lines[first_row_index] = "\t".join(fields)
    require_failure(
        "closed witness without finite subledger",
        "\n".join(weak_witness_lines) + "\n",
        "closed witness omits its finite subledger",
        repo_root,
    )
    require_failure(
        "closed witness without source verifier",
        "\n".join(weak_witness_lines) + "\n",
        "closed witness omits its source verifier",
        repo_root,
    )

    missing_row_lines = lines.copy()
    del missing_row_lines[first_row_index]
    require_failure(
        "missing row",
        "\n".join(missing_row_lines) + "\n",
        "denominator count mismatch",
        repo_root,
    )

    printf_requirements_path = pathlib.PurePosixPath(
        "docs/posix/printf_requirements.tsv"
    )
    printf_requirements = (repo_root / printf_requirements_path).read_text(
        encoding="ascii"
    )
    closed_marker = "\trequired\tclosed\t"
    if closed_marker not in printf_requirements:
        raise AssertionError("printf subledger self-test needs a closed mandatory row")
    reopened_requirements = printf_requirements.replace(
        closed_marker, "\trequired\topen\t", 1
    )
    state_failures = validate_requirements_text("printf", reopened_requirements)
    if not any(
        "mandatory subledger row is not closed" in failure for failure in state_failures
    ):
        raise AssertionError(
            f"printf subledger reopening was not rejected: {state_failures}"
        )


def count_states(ledger_path: pathlib.Path) -> tuple[int, int]:
    open_count = 0
    closed_count = 0
    for line in ledger_path.read_text(encoding="ascii").splitlines():
        if not line or line.startswith(("#", "utility\t")):
            continue
        state = line.split("\t", maxsplit=2)[1]
        if state == "open":
            open_count += 1
        elif state == "closed":
            closed_count += 1
    return open_count, closed_count


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    ledger_path = resolve_ledger(repo_root, args.ledger).resolve()
    failures = validate_ledger(ledger_path, repo_root)
    if failures:
        print(
            "SUSv4 Issue 7 utility ledger verification failed:",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    if args.self_test:
        run_self_test(ledger_path, repo_root)
        print("SUSv4 Issue 7 utility ledger mutation self-test passed.")
        return 0

    open_count, closed_count = count_states(ledger_path)
    print(
        "SUSv4 Issue 7 utility ledger verified: "
        f"{EXPECTED_COUNT} rows, {closed_count} closed, {open_count} open."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
