#!/usr/bin/env python3
"""Verify the finite POSIX Issue 7 shell grammar denominator."""

from __future__ import annotations

import argparse
import ast
import hashlib
import html
from html.parser import HTMLParser
import pathlib
import re
import sys

from verify_posix_shell_language_ledger import (
    QEMU_SHELL_TEST_PATH,
    VerificationError,
    read_chapter_text,
    resolve_path,
)

EXPECTED_COLUMNS = (
    "requirement",
    "kind",
    "production",
    "alternative",
    "source_sha256",
    "state",
    "witness",
    "next_action",
)
EXPECTED_LEXICAL_COUNT = 3
EXPECTED_CONTEXT_COUNT = 11
EXPECTED_PRODUCTION_COUNT = 47
EXPECTED_ALTERNATIVE_COUNT = 111
EXPECTED_COUNT = 125
EXPECTED_KEY_SHA256 = "b0fdb308b771070cf97c8487fa584ab141a395cc922aefe96030d230b8c03c38"
EXPECTED_SOURCE_ROW_SHA256 = (
    "db0d57893ee7683fcf8046c52db4b2fde5463f111cc6166c81e56266afa99741"
)
VALID_STATES = {"open", "closed"}
PRODUCTION_PATTERN = re.compile(r"^([a-z][a-z_]*)\s*:\s*(.*?)$")
GRAMMAR_CASES_NAME = "SHELL_GRAMMAR_COMMAND_CASES"
GRAMMAR_RUNNER_NAME = "require_shell_grammar_command_cases"

LEXICAL_REQUIREMENTS = (
    "tag_18_10_01.lex01_operator",
    "tag_18_10_01.lex02_io_number",
    "tag_18_10_01.lex03_token",
)
CONTEXT_REQUIREMENTS = (
    "tag_18_10_02.rule01_command_name",
    "tag_18_10_02.rule02_redirection_filename",
    "tag_18_10_02.rule03_here_document",
    "tag_18_10_02.rule04_case_termination",
    "tag_18_10_02.rule05_for_name",
    "tag_18_10_02.rule06a_case_in",
    "tag_18_10_02.rule06b_for_in_do",
    "tag_18_10_02.rule07a_assignment_first",
    "tag_18_10_02.rule07b_assignment_later",
    "tag_18_10_02.rule08_function_name",
    "tag_18_10_02.rule09_function_body",
)


class OrderedListItemParser(HTMLParser):
    """Collect source-ordered list items while retaining nested obligations."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.list_depth = 0
        self.next_sequence = 0
        self.active_items: list[list[object]] = []
        self.items: list[tuple[int, int, str]] = []

    def handle_starttag(
        self, tag: str, attributes: list[tuple[str, str | None]]
    ) -> None:
        del attributes
        if tag in {"ol", "ul"}:
            self.list_depth += 1
        elif tag == "li":
            self.next_sequence += 1
            self.active_items.append([self.next_sequence, self.list_depth, []])

    def handle_endtag(self, tag: str) -> None:
        if tag == "li" and self.active_items:
            sequence, depth, pieces = self.active_items.pop()
            if not isinstance(sequence, int) or not isinstance(depth, int):
                raise VerificationError("invalid ordered-list parser state")
            if not isinstance(pieces, list):
                raise VerificationError("invalid ordered-list text state")
            normalized = " ".join("".join(pieces).split())
            self.items.append((sequence, depth, normalized))
        elif tag in {"ol", "ul"}:
            self.list_depth -= 1

    def handle_data(self, data: str) -> None:
        for active_item in self.active_items:
            pieces = active_item[2]
            if not isinstance(pieces, list):
                raise VerificationError("invalid ordered-list text state")
            pieces.append(data)


def parse_args() -> argparse.Namespace:
    repository_root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=pathlib.Path, default=repository_root)
    parser.add_argument(
        "--archive",
        type=pathlib.Path,
        default=pathlib.Path("build/_state/downloads/posix/susv4-2018.tgz"),
    )
    parser.add_argument(
        "--requirements",
        type=pathlib.Path,
        default=pathlib.Path("docs/posix/posix_shell_grammar_requirements.tsv"),
    )
    parser.add_argument("--emit-template", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def ordered_sha256(lines: list[str]) -> str:
    return hashlib.sha256(
        "".join(f"{line}\n" for line in lines).encode("ascii")
    ).hexdigest()


def split_once(source_text: str, marker: str, description: str) -> tuple[str, str]:
    before_marker, separator, after_marker = source_text.partition(marker)
    if not separator:
        raise VerificationError(f"missing shell grammar source marker: {description}")
    return before_marker, after_marker


def source_interval(
    source_text: str, start_marker: str, end_marker: str, description: str
) -> str:
    _before_start, after_start = split_once(source_text, start_marker, description)
    interval, _after_end = split_once(after_start, end_marker, description)
    return interval


def parse_list_items(source_text: str) -> list[tuple[int, int, str]]:
    parser = OrderedListItemParser()
    parser.feed(source_text)
    parser.close()
    return sorted(parser.items)


def derive_lexical_rows(chapter_text: str) -> list[tuple[str, str, str, str]]:
    lexical_section = source_interval(
        chapter_text,
        '<h4><a name="tag_18_10_01">',
        '<h4><a name="tag_18_10_02">',
        "tag_18_10_01",
    )
    lexical_items = [
        item_text
        for _sequence, depth, item_text in parse_list_items(lexical_section)
        if depth == 1
    ]
    if len(lexical_items) != EXPECTED_LEXICAL_COUNT:
        raise VerificationError(
            "shell grammar lexical count mismatch: "
            f"expected {EXPECTED_LEXICAL_COUNT}, got {len(lexical_items)}"
        )
    return [
        (requirement, "lexical", "-", source_text)
        for requirement, source_text in zip(
            LEXICAL_REQUIREMENTS, lexical_items, strict=True
        )
    ]


def derive_context_rows(chapter_text: str) -> list[tuple[str, str, str, str]]:
    context_section = source_interval(
        chapter_text,
        '<h4><a name="tag_18_10_02">',
        "<pre>",
        "tag_18_10_02 context rules",
    )
    parsed_items = parse_list_items(context_section)
    top_level_items = [item for item in parsed_items if item[1] == 1]
    subrule_items = [item for item in parsed_items if item[1] == 2]
    if len(top_level_items) != 9 or len(subrule_items) != 4:
        raise VerificationError(
            "shell grammar context-rule shape mismatch: "
            f"expected 9 top-level and 4 subrule items, got "
            f"{len(top_level_items)} and {len(subrule_items)}"
        )
    selected_items = [
        *top_level_items[:5],
        *subrule_items,
        *top_level_items[7:],
    ]
    selected_items.sort(key=lambda item: item[0])
    if len(selected_items) != EXPECTED_CONTEXT_COUNT:
        raise VerificationError(
            "shell grammar context count mismatch: "
            f"expected {EXPECTED_CONTEXT_COUNT}, got {len(selected_items)}"
        )
    return [
        (requirement, "context", "-", item_text)
        for requirement, (_sequence, _depth, item_text) in zip(
            CONTEXT_REQUIREMENTS, selected_items, strict=True
        )
    ]


def derive_grammar_rows(chapter_text: str) -> list[tuple[str, str, str, str]]:
    _before_grammar, grammar_and_tail = split_once(
        chapter_text, "%start program", "grammar start"
    )
    raw_grammar, _after_grammar = split_once(grammar_and_tail, "</tt>", "grammar end")
    grammar_text = html.unescape(re.sub(r"<[^>]+>", "", raw_grammar))
    current_production: str | None = None
    alternative_counts: dict[str, int] = {}
    source_rows: list[tuple[str, str, str, str]] = []
    production_order: list[str] = []

    for raw_line in grammar_text.splitlines():
        line = " ".join(raw_line.strip().split())
        production_match = PRODUCTION_PATTERN.fullmatch(line)
        if production_match is not None:
            current_production = production_match.group(1)
            alternative = production_match.group(2)
            if current_production not in alternative_counts:
                production_order.append(current_production)
                alternative_counts[current_production] = 0
        elif current_production is not None and line.startswith("|"):
            alternative = line.removeprefix("|").strip()
        elif line == ";":
            current_production = None
            continue
        else:
            continue

        if current_production is None or not alternative:
            raise VerificationError("empty shell grammar alternative")
        alternative_counts[current_production] += 1
        alternative_number = alternative_counts[current_production]
        requirement = (
            f"tag_18_10_02.grammar.{current_production}.alt{alternative_number:02d}"
        )
        source_rows.append((requirement, "grammar", current_production, alternative))

    if len(production_order) != EXPECTED_PRODUCTION_COUNT:
        raise VerificationError(
            "shell grammar production count mismatch: "
            f"expected {EXPECTED_PRODUCTION_COUNT}, got {len(production_order)}"
        )
    if len(source_rows) != EXPECTED_ALTERNATIVE_COUNT:
        raise VerificationError(
            "shell grammar alternative count mismatch: "
            f"expected {EXPECTED_ALTERNATIVE_COUNT}, got {len(source_rows)}"
        )
    return source_rows


def derive_source_rows(
    chapter_text: str,
) -> list[tuple[str, str, str, str, str]]:
    unverified_rows = [
        *derive_lexical_rows(chapter_text),
        *derive_context_rows(chapter_text),
        *derive_grammar_rows(chapter_text),
    ]
    if len(unverified_rows) != EXPECTED_COUNT:
        raise VerificationError(
            "shell grammar requirement count mismatch: "
            f"expected {EXPECTED_COUNT}, got {len(unverified_rows)}"
        )

    source_rows = [
        (
            requirement,
            kind,
            production,
            alternative,
            hashlib.sha256(alternative.encode("ascii")).hexdigest(),
        )
        for requirement, kind, production, alternative in unverified_rows
    ]
    requirements = [row[0] for row in source_rows]
    if len(requirements) != len(set(requirements)):
        raise VerificationError("duplicate shell grammar source requirements")
    observed_key_hash = ordered_sha256(requirements)
    if observed_key_hash != EXPECTED_KEY_SHA256:
        raise VerificationError(
            "shell grammar key hash mismatch: "
            f"expected {EXPECTED_KEY_SHA256}, got {observed_key_hash}"
        )
    source_lines = ["\t".join(row) for row in source_rows]
    observed_source_hash = ordered_sha256(source_lines)
    if observed_source_hash != EXPECTED_SOURCE_ROW_SHA256:
        raise VerificationError(
            "shell grammar source-row hash mismatch: "
            f"expected {EXPECTED_SOURCE_ROW_SHA256}, got {observed_source_hash}"
        )
    return source_rows


def read_executable_case_ids(repository_root: pathlib.Path) -> set[str]:
    test_path = repository_root / QEMU_SHELL_TEST_PATH
    try:
        test_text = test_path.read_text(encoding="ascii")
        syntax_tree = ast.parse(test_text, filename=str(test_path))
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise VerificationError(
            f"cannot parse ASCII shell test {test_path}: {error}"
        ) from error

    case_assignment = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.Assign)
            and any(
                isinstance(target, ast.Name) and target.id == GRAMMAR_CASES_NAME
                for target in statement.targets
            )
        ),
        None,
    )
    if case_assignment is None:
        return set()
    try:
        command_cases = ast.literal_eval(case_assignment.value)
    except (ValueError, TypeError) as error:
        raise VerificationError(
            f"{QEMU_SHELL_TEST_PATH}: grammar cases are not an ASCII literal"
        ) from error
    if not isinstance(command_cases, tuple):
        raise VerificationError(
            f"{QEMU_SHELL_TEST_PATH}: grammar cases must be a tuple"
        )

    case_ids: list[str] = []
    for case_index, command_case in enumerate(command_cases, start=1):
        if (
            not isinstance(command_case, tuple)
            or len(command_case) != 3
            or not all(isinstance(field, str) for field in command_case)
        ):
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: grammar case {case_index} must contain "
                "an ID, command, and expected output"
            )
        case_id, command, expected_output = command_case
        if not case_id or not command or not expected_output:
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: grammar case {case_index} has an empty field"
            )
        try:
            case_id.encode("ascii")
            command.encode("ascii")
            expected_output.encode("ascii")
        except UnicodeEncodeError as error:
            raise VerificationError(
                f"{QEMU_SHELL_TEST_PATH}: grammar case {case_id!r} is not ASCII"
            ) from error
        case_ids.append(case_id)

    duplicate_case_ids = sorted(
        {case_id for case_id in case_ids if case_ids.count(case_id) > 1}
    )
    if duplicate_case_ids:
        raise VerificationError(
            f"duplicate executable grammar case IDs: {', '.join(duplicate_case_ids)}"
        )

    runner_function = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.FunctionDef)
            and statement.name == GRAMMAR_RUNNER_NAME
        ),
        None,
    )
    if runner_function is None or not any(
        isinstance(node, ast.Name)
        and node.id == GRAMMAR_CASES_NAME
        and isinstance(node.ctx, ast.Load)
        for node in ast.walk(runner_function)
    ):
        raise VerificationError(
            f"{QEMU_SHELL_TEST_PATH}: executable grammar case runner is missing"
        )
    main_function = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.FunctionDef) and statement.name == "main"
        ),
        None,
    )
    if main_function is None or not any(
        isinstance(node, ast.Call)
        and isinstance(node.func, ast.Name)
        and node.func.id == GRAMMAR_RUNNER_NAME
        for node in ast.walk(main_function)
    ):
        raise VerificationError(
            f"{QEMU_SHELL_TEST_PATH}: main does not execute grammar cases"
        )
    return set(case_ids)


def validate_closed_witness(
    requirement: str,
    witness: str,
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    if not witness.startswith("tests:"):
        return [f"{requirement}: closed witness must start with 'tests:'"]
    failures: list[str] = []
    for witness_reference in witness.removeprefix("tests:").split(","):
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
            failures.append(
                f"{requirement}: grammar witness must use {QEMU_SHELL_TEST_PATH}"
            )
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
    source_rows: list[tuple[str, str, str, str, str]],
    repository_root: pathlib.Path,
    executable_case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    rows: list[tuple[str, str, str, str, str, str, str, str]] = []
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
        return failures + ["missing grammar requirements header"]
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
            f"duplicate shell grammar requirements: {', '.join(duplicate_requirements)}"
        )
    if len(rows) != EXPECTED_COUNT:
        failures.append(
            f"grammar requirements count mismatch: expected {EXPECTED_COUNT}, "
            f"got {len(rows)}"
        )

    observed_source_rows = [row[:5] for row in rows]
    if observed_source_rows != source_rows:
        failures.append("shell grammar rows differ from official source")

    open_requirements: set[str] = set()
    closed_requirements: set[str] = set()
    for row in rows:
        requirement, _kind, _production, _alternative, _source_hash = row[:5]
        state, witness, next_action = row[5:]
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
        failures.append("open and closed grammar requirements overlap")
    if open_requirements | closed_requirements != set(requirements):
        failures.append("grammar requirement partitions do not cover the denominator")
    return failures


def require_failure(
    name: str,
    requirements_text: str,
    expected_fragment: str,
    source_rows: list[tuple[str, str, str, str, str]],
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


def render_template(
    source_rows: list[tuple[str, str, str, str, str]],
) -> str:
    output_lines = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        "# source: susv4-2018/utilities/V3_chap02.html tag_18_10 through tag_18_10_02",
        "# derivation: 3 lexical classifications, 11 context rules, and 111 alternatives across 47 productions",
        f"# snapshot-count: {EXPECTED_COUNT}",
        f"# ordered-key-sha256: {EXPECTED_KEY_SHA256}",
        f"# ordered-source-row-sha256: {EXPECTED_SOURCE_ROW_SHA256}",
        "\t".join(EXPECTED_COLUMNS),
    ]
    for requirement, kind, production, alternative, source_hash in source_rows:
        if kind == "grammar":
            next_action = (
                f"Add an exact Q35 Ring 3 discriminator for the {production} "
                f"alternative: {alternative}"
            )
        else:
            next_action = (
                f"Add exact Q35 Ring 3 token-categorization cases for: {alternative}"
            )
        output_lines.append(
            "\t".join(
                (
                    requirement,
                    kind,
                    production,
                    alternative,
                    source_hash,
                    "open",
                    f"missing:exact_tests_for_{requirement}",
                    next_action,
                )
            )
        )
    return "\n".join(output_lines) + "\n"


def run_self_test(
    requirements_path: pathlib.Path,
    chapter_text: str,
    source_rows: list[tuple[str, str, str, str, str]],
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
        "duplicate shell grammar requirements",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    missing_lines = lines.copy()
    del missing_lines[first_row_index]
    require_failure(
        "missing requirement",
        "\n".join(missing_lines) + "\n",
        "grammar requirements count mismatch",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    changed_source_lines = lines.copy()
    changed_source_fields = changed_source_lines[first_row_index].split("\t")
    changed_source_fields[4] = "0" * 64
    changed_source_lines[first_row_index] = "\t".join(changed_source_fields)
    require_failure(
        "changed source hash",
        "\n".join(changed_source_lines) + "\n",
        "rows differ from official source",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    false_closed_lines = lines.copy()
    false_closed_fields = false_closed_lines[first_row_index].split("\t")
    false_closed_fields[5] = "closed"
    false_closed_fields[6] = (
        f"missing:exact_tests_for_{false_closed_fields[0]}"
    )
    false_closed_fields[7] = "-"
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
    unknown_case_fields[5] = "closed"
    unknown_case_fields[6] = (
        f"tests:{QEMU_SHELL_TEST_PATH}#{unknown_case_fields[0]}.missing_executable_case"
    )
    unknown_case_fields[7] = "-"
    unknown_case_lines[first_row_index] = "\t".join(unknown_case_fields)
    require_failure(
        "unknown executable case",
        "\n".join(unknown_case_lines) + "\n",
        "unknown executable case ID",
        source_rows,
        repository_root,
        executable_case_ids,
    )

    for name, original_text, replacement, expected_fragment in (
        (
            "lexical source mutation",
            "identifier <b>IO_NUMBER</b> shall be returned.",
            "altered identifier <b>IO_NUMBER</b> shall be returned.",
            "source-row hash mismatch",
        ),
        (
            "grammar source mutation",
            "complete_commands newline_list complete_command",
            "complete_commands altered_list complete_command",
            "source-row hash mismatch",
        ),
    ):
        try:
            derive_source_rows(chapter_text.replace(original_text, replacement, 1))
        except VerificationError as error:
            if expected_fragment not in str(error):
                raise AssertionError(
                    f"{name}: failed for the wrong reason: {error}"
                ) from error
        else:
            raise AssertionError(f"{name}: mutated source unexpectedly passed")


def count_states(requirements_path: pathlib.Path) -> tuple[int, int]:
    open_count = 0
    closed_count = 0
    for line in requirements_path.read_text(encoding="ascii").splitlines():
        if not line or line.startswith(("#", "requirement\t")):
            continue
        state = line.split("\t")[5]
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
    if not archive_path.is_file():
        print(f"SKIP: POSIX Issue 7 archive not found: {archive_path}")
        return 77
    try:
        chapter_text = read_chapter_text(archive_path)
        source_rows = derive_source_rows(chapter_text)
        if arguments.emit_template:
            print(render_template(source_rows), end="")
            return 0
        executable_case_ids = read_executable_case_ids(repository_root)
        requirements_text = requirements_path.read_text(encoding="ascii")
        failures = validate_requirements_text(
            requirements_text, source_rows, repository_root, executable_case_ids
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
            print("POSIX shell grammar mutation self-test passed.")
            return 0
        open_count, closed_count = count_states(requirements_path)
        print(
            "POSIX shell grammar requirements verified: "
            f"{EXPECTED_COUNT} rows, {closed_count} closed, {open_count} open."
        )
        return 0
    except (OSError, UnicodeDecodeError, VerificationError, AssertionError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
