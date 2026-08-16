#!/usr/bin/env python3
"""Generate and verify the finite POSIX printf requirement denominator."""

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
    VerificationError,
    resolve_path,
    verify_archive_identity,
)

PRINTF_MEMBER = f"{ARCHIVE_TOP_DIRECTORY}/utilities/printf.html"
FILE_FORMAT_MEMBER = f"{ARCHIVE_TOP_DIRECTORY}/basedefs/V1_chap05.html"
QEMU_TEST_PATH = pathlib.PurePosixPath("test/boot/x86_64_shell_test.py")
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
EXPECTED_SOURCE_SET_SHA256 = (
    "946def388c77a1cf47a11968e1c4a536ce1ce51f1bfed2ddf1630bd44274f280"
)
TAG_PATTERN = re.compile(r"<[^>]+>")
PARAGRAPH_PATTERN = re.compile(r"<p(?: [^>]*)?>(.*?)</p>", re.IGNORECASE | re.DOTALL)
DEFINITION_PATTERN = re.compile(
    r"<dt(?: [^>]*)?>(.*?)</dt>\s*<dd(?: [^>]*)?>(.*?)</dd>",
    re.IGNORECASE | re.DOTALL,
)
TABLE_ROW_PATTERN = re.compile(r"<tr(?: [^>]*)?>(.*?)</tr>", re.IGNORECASE | re.DOTALL)
TABLE_CELL_PATTERN = re.compile(
    r"<t[dh](?: [^>]*)?>(.*?)</t[dh]>", re.IGNORECASE | re.DOTALL
)


@dataclass(frozen=True)
class Requirement:
    identifier: str
    title: str
    selector: str
    applicability: str
    state: str
    witness: str
    next_action: str


@dataclass(frozen=True)
class SourceRow:
    identifier: str
    title: str
    source_ref: str
    source_sha256: str
    applicability: str
    state: str
    witness: str
    next_action: str


def required(
    identifier: str, title: str, selector: str, case_ids: tuple[str, ...] = ()
) -> Requirement:
    if case_ids:
        witness = "tests:" + ",".join(
            f"{QEMU_TEST_PATH}#{case_id}" for case_id in case_ids
        )
        return Requirement(
            identifier, title, selector, "required", "closed", witness, "-"
        )
    return Requirement(
        identifier,
        title,
        selector,
        "required",
        "open",
        f"missing:{identifier}",
        f"Add an exact Q35 Ring 3 witness for {title.lower()}.",
    )


def excluded(
    identifier: str, title: str, selector: str, applicability: str
) -> Requirement:
    return Requirement(
        identifier,
        title,
        selector,
        applicability,
        "excluded",
        f"scope:{applicability}",
        "-",
    )


MANDATORY = ("printf.mandatory_conversion_set",)
FLAGS = ("printf.flags_width_and_precision",)
FORMAT_BYTES = ("printf.format_escape_binary_bytes",)
PERCENT_B_BYTES = ("printf.percent_b_binary_bytes",)

REQUIREMENTS = (
    required(
        "printf.output.formatted_operands",
        "Formatted operands on standard output",
        "up:1",
        MANDATORY,
    ),
    excluded("printf.options.none", "No utility-specific options", "up:2", "none"),
    required("printf.operand.format", "Format operand", "ud:0", MANDATORY),
    required("printf.operand.argument", "Argument operands", "ud:1", MANDATORY),
    required(
        "printf.stdin.unused",
        "Standard input is not read",
        "up:4",
        ("printf.standard_input_not_read",),
    ),
    excluded("printf.input_files.none", "No input files", "up:5", "none"),
    required(
        "printf.locale.lang",
        "LANG locale default",
        "ud:2",
        ("printf.locale_lang_default",),
    ),
    required(
        "printf.locale.lc_all",
        "LC_ALL locale override",
        "ud:3",
        ("printf.locale_lc_all_overrides_lc_ctype",),
    ),
    required(
        "printf.locale.lc_ctype",
        "LC_CTYPE byte interpretation",
        "ud:4",
        (
            "printf.locale_lang_default",
            "printf.locale_lc_all_overrides_lc_ctype",
        ),
    ),
    required(
        "printf.locale.lc_messages",
        "LC_MESSAGES diagnostics",
        "ud:5",
        ("printf.locale_lc_messages_diagnostic",),
    ),
    excluded(
        "printf.locale.lc_numeric",
        "LC_NUMERIC optional floating output",
        "ud:6",
        "conditional",
    ),
    excluded(
        "printf.locale.nlspath", "NLSPATH XSI message catalogs", "ud:7", "optional"
    ),
    required(
        "printf.async.default",
        "Default asynchronous-event behavior",
        "up:7",
        ("printf.default_signal_action",),
    ),
    excluded(
        "printf.stdout.cross_reference",
        "Standard output uses the format rules",
        "up:8",
        "none",
    ),
    required(
        "printf.stderr.diagnostics_only",
        "Standard error contains diagnostics only",
        "up:9",
        ("printf.numeric_conversion_diagnostic_continues",),
    ),
    excluded(
        "printf.output_files.none",
        "No output files other than standard output",
        "up:10",
        "none",
    ),
    required(
        "printf.format.literal_space",
        "Literal space is copied",
        "up:12",
        ("printf.c_integer_constant_extensions",),
    ),
    excluded(
        "printf.format.delta_literal",
        "File-format delta character outside admitted codeset",
        "up:13",
        "conditional",
    ),
    required(
        "printf.format.octal_escape",
        "One-to-three-digit format octal escape",
        "up:14",
        FORMAT_BYTES,
    ),
    required(
        "printf.integer.no_implicit_decimal_blanks",
        "No implicit blanks for d and u",
        "up:15",
        ("printf.bare_numeric_has_no_implicit_padding",),
    ),
    required(
        "printf.integer.no_implicit_octal_zeros",
        "No implicit leading zeros for o",
        "up:16",
        ("printf.bare_numeric_has_no_implicit_padding",),
    ),
    excluded(
        "printf.floating.optional", "Floating conversion set", "up:17", "optional"
    ),
    required(
        "printf.percent_b.argument",
        "Percent-b string argument",
        "up:18",
        PERCENT_B_BYTES,
    ),
    required(
        "printf.percent_b.simple_escapes",
        "Percent-b simple escapes",
        "up:19",
        PERCENT_B_BYTES,
    ),
    required(
        "printf.percent_b.octal_escape",
        "Percent-b zero-prefixed octal escape",
        "up:20",
        PERCENT_B_BYTES,
    ),
    required(
        "printf.percent_b.stop_escape",
        "Percent-b stop escape",
        "up:21",
        ("printf.percent_b_c_stops_utility_output",),
    ),
    excluded(
        "printf.percent_b.unknown_escape",
        "Unknown percent-b escape interpretation",
        "up:22",
        "unspecified",
    ),
    required(
        "printf.percent_b.precision",
        "Percent-b byte precision",
        "up:23",
        ("printf.percent_b_precision_and_width",),
    ),
    required(
        "printf.arguments.sequential_consumption",
        "Sequential argument consumption",
        "up:24",
        MANDATORY,
    ),
    required(
        "printf.format.reuse_and_defaults",
        "Format reuse and missing-operand defaults",
        "up:25",
        (
            "printf.format_reuse_partial_final_cycle",
            "printf.missing_operands_use_specified_defaults",
        ),
    ),
    excluded(
        "printf.format.no_conversion_with_arguments",
        "Arguments with no consuming conversion",
        "up:25",
        "unspecified",
    ),
    excluded(
        "printf.format.invalid_conversion",
        "Invalid conversion specification",
        "up:26",
        "unspecified",
    ),
    required(
        "printf.character.first_byte",
        "Percent-c first-byte conversion",
        "up:27",
        MANDATORY,
    ),
    excluded(
        "printf.character.empty_argument",
        "Percent-c empty argument",
        "up:27",
        "unspecified",
    ),
    required(
        "printf.arguments.type_conversion",
        "String and integer argument typing",
        "up:28",
        MANDATORY,
    ),
    required(
        "printf.integer.leading_sign",
        "Leading integer sign",
        "up:29",
        ("printf.c_integer_constant_extensions",),
    ),
    required(
        "printf.integer.quoted_character",
        "Quoted-character integer value",
        "up:30",
        ("printf.c_integer_constant_extensions",),
    ),
    excluded(
        "printf.integer.suffix", "Suffixed integer constants", "up:31", "optional"
    ),
    required(
        "printf.integer.conversion_error",
        "Numeric conversion diagnostic, status, continuation, and accumulated value",
        "up:32",
        (
            "printf.numeric_conversion_diagnostic_continues",
            "printf.integer_overflow_boundaries",
        ),
    ),
    required(
        "printf.strings.partial_use",
        "Partial b, c, and s argument use is not an error",
        "up:33",
        MANDATORY,
    ),
    required("printf.exit.success", "Zero status on success", "ud:8", MANDATORY),
    required(
        "printf.exit.error",
        "Nonzero status on error",
        "ud:9",
        ("printf.failed_standard_output_status", "printf.missing_format_diagnostic"),
    ),
    excluded(
        "printf.errors.default",
        "Default environmental consequences of errors",
        "up:35",
        "unspecified",
    ),
    required(
        "printf.xbd.literal_character",
        "Ordinary format characters are copied",
        "bp:3",
        MANDATORY,
    ),
    required(
        "printf.xbd.escape.backslash", "Format backslash escape", "be:0", FORMAT_BYTES
    ),
    required("printf.xbd.escape.alert", "Format alert escape", "be:1", FORMAT_BYTES),
    required(
        "printf.xbd.escape.backspace", "Format backspace escape", "be:2", FORMAT_BYTES
    ),
    required(
        "printf.xbd.escape.form_feed", "Format form-feed escape", "be:3", FORMAT_BYTES
    ),
    required(
        "printf.xbd.escape.newline", "Format newline escape", "be:4", FORMAT_BYTES
    ),
    required(
        "printf.xbd.escape.carriage_return",
        "Format carriage-return escape",
        "be:5",
        FORMAT_BYTES,
    ),
    required("printf.xbd.escape.tab", "Format tab escape", "be:6", FORMAT_BYTES),
    required(
        "printf.xbd.escape.vertical_tab",
        "Format vertical-tab escape",
        "be:7",
        FORMAT_BYTES,
    ),
    required(
        "printf.xbd.conversion_sequence",
        "Percent conversion component sequence",
        "bp:38",
        FLAGS,
    ),
    required(
        "printf.xbd.flags.sequence", "Zero or more flags in any order", "bd:2", FLAGS
    ),
    required(
        "printf.xbd.field_width", "Minimum field width and padding side", "bd:3", FLAGS
    ),
    required("printf.xbd.precision", "Integer and string precision", "bd:4", FLAGS),
    required(
        "printf.xbd.conversion_character",
        "Conversion specifier character",
        "bd:5",
        MANDATORY,
    ),
    required("printf.xbd.flag.left_adjust", "Left-adjust flag", "bd:6", FLAGS),
    required("printf.xbd.flag.always_sign", "Always-sign flag", "bd:7", FLAGS),
    required(
        "printf.xbd.flag.leading_space",
        "Leading-space flag and plus precedence",
        "bd:8",
        FLAGS,
    ),
    required(
        "printf.xbd.flag.alternative_form", "Alternative-form flag", "bd:9", FLAGS
    ),
    required(
        "printf.xbd.flag.zero_pad", "Zero-pad flag and precedence", "bd:10", FLAGS
    ),
    required(
        "printf.xbd.integer_conversions",
        "Mandatory integer conversions",
        "bd:12",
        MANDATORY,
    ),
    required(
        "printf.xbd.character_conversion",
        "Mandatory character conversion",
        "bd:16",
        MANDATORY,
    ),
    required(
        "printf.xbd.string_conversion",
        "Mandatory string conversion",
        "bd:17",
        MANDATORY,
    ),
    required(
        "printf.xbd.percent_conversion",
        "Literal percent conversion",
        "bd:18",
        MANDATORY,
    ),
    required(
        "printf.xbd.field_width_no_truncation",
        "Field width never truncates",
        "bp:45",
        ("printf.field_width_does_not_truncate",),
    ),
)


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
        default=pathlib.Path("docs/posix/printf_requirements.tsv"),
    )
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def normalize(raw_text: str) -> str:
    decoded = html.unescape(TAG_PATTERN.sub("", raw_text)).replace(
        "\N{PLUS-MINUS SIGN}", "<plus-or-minus>"
    )
    normalized = " ".join(decoded.split())
    try:
        normalized.encode("ascii")
    except UnicodeEncodeError as error:
        raise VerificationError(
            f"non-ASCII printf source fragment: {normalized!r}"
        ) from error
    return normalized


def read_sources(archive_path: pathlib.Path) -> tuple[str, str]:
    verify_archive_identity(archive_path)
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            source_bytes = []
            for member_name in (PRINTF_MEMBER, FILE_FORMAT_MEMBER):
                member = archive.extractfile(member_name)
                if member is None:
                    raise VerificationError(f"missing archive member: {member_name}")
                source_bytes.append(member.read())
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(
            f"cannot read printf source members: {error}"
        ) from error
    return source_bytes[0].decode("latin-1"), source_bytes[1].decode("latin-1")


def paragraphs(source_text: str) -> list[str]:
    return [normalize(fragment) for fragment in PARAGRAPH_PATTERN.findall(source_text)]


def definitions(source_text: str) -> list[str]:
    return [
        f"{normalize(term)} | {normalize(description)}"
        for term, description in DEFINITION_PATTERN.findall(source_text)
    ]


def base_escape_rows(base_text: str) -> list[str]:
    table_start = base_text.index('<a name="tagtcjh_2">')
    table_end = base_text.index("</table>", table_start)
    table_text = base_text[table_start:table_end]
    rows: list[str] = []
    for row_text in TABLE_ROW_PATTERN.findall(table_text):
        cells = [normalize(cell) for cell in TABLE_CELL_PATTERN.findall(row_text)]
        if len(cells) == 3 and cells[0] not in {"Escape", "Sequence"}:
            rows.append(" | ".join(cells))
    if len(rows) != 8:
        raise VerificationError(
            f"printf base escape count mismatch: expected 8, got {len(rows)}"
        )
    return rows


def source_fragments(utility_text: str, base_text: str) -> dict[str, tuple[str, str]]:
    utility_paragraphs = paragraphs(utility_text)
    utility_definitions = definitions(utility_text)
    base_paragraphs = paragraphs(base_text)
    base_definitions = definitions(base_text)
    base_escapes = base_escape_rows(base_text)
    fragments: dict[str, tuple[str, str]] = {}
    for prefix, member_name, values in (
        ("up", PRINTF_MEMBER, utility_paragraphs),
        ("ud", PRINTF_MEMBER, utility_definitions),
        ("bp", FILE_FORMAT_MEMBER, base_paragraphs),
        ("bd", FILE_FORMAT_MEMBER, base_definitions),
        ("be", FILE_FORMAT_MEMBER, base_escapes),
    ):
        for index, value in enumerate(values):
            fragments[f"{prefix}:{index}"] = (f"{member_name}#{prefix}:{index}", value)
    return fragments


def derive_rows(utility_text: str, base_text: str) -> list[SourceRow]:
    fragments = source_fragments(utility_text, base_text)
    rows: list[SourceRow] = []
    hash_lines: list[str] = []
    for requirement in REQUIREMENTS:
        if requirement.selector not in fragments:
            raise VerificationError(
                f"missing printf source selector: {requirement.selector}"
            )
        source_ref, source_text = fragments[requirement.selector]
        source_sha256 = hashlib.sha256(source_text.encode("ascii")).hexdigest()
        rows.append(
            SourceRow(
                requirement.identifier,
                requirement.title,
                source_ref,
                source_sha256,
                requirement.applicability,
                requirement.state,
                requirement.witness,
                requirement.next_action,
            )
        )
        hash_lines.append(f"{requirement.identifier}\t{source_ref}\t{source_sha256}")
    observed_hash = hashlib.sha256(
        "".join(f"{line}\n" for line in hash_lines).encode("ascii")
    ).hexdigest()
    if observed_hash != EXPECTED_SOURCE_SET_SHA256:
        raise VerificationError(
            "printf source-set hash mismatch: "
            f"expected {EXPECTED_SOURCE_SET_SHA256}, got {observed_hash}"
        )
    return rows


def read_case_ids(repository_root: pathlib.Path) -> set[str]:
    test_path = repository_root / QEMU_TEST_PATH
    try:
        test_text = test_path.read_text(encoding="ascii")
        syntax_tree = ast.parse(test_text, filename=str(test_path))
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise VerificationError(f"cannot parse printf QEMU test: {error}") from error
    assignment = next(
        (
            statement
            for statement in syntax_tree.body
            if isinstance(statement, ast.Assign)
            and any(
                isinstance(target, ast.Name)
                and target.id == "PRINTF_UTILITY_COMMAND_CASES"
                for target in statement.targets
            )
        ),
        None,
    )
    if assignment is None:
        raise VerificationError("missing PRINTF_UTILITY_COMMAND_CASES")
    try:
        cases = ast.literal_eval(assignment.value)
    except (ValueError, TypeError) as error:
        raise VerificationError("printf QEMU cases are not literals") from error
    case_ids: list[str] = []
    if not isinstance(cases, tuple):
        raise VerificationError("printf QEMU cases must be a tuple")
    for case in cases:
        if (
            not isinstance(case, tuple)
            or len(case) != 3
            or not all(isinstance(field, str) and field for field in case)
        ):
            raise VerificationError(
                "each printf QEMU case needs an ID, command, and output"
            )
        case_ids.append(case[0])
    duplicates = sorted(
        {case_id for case_id in case_ids if case_ids.count(case_id) > 1}
    )
    if duplicates:
        raise VerificationError(f"duplicate printf QEMU cases: {', '.join(duplicates)}")
    if "require_printf_utility_command_cases(sock)" not in test_text:
        raise VerificationError("printf QEMU case runner is not invoked")
    return set(case_ids)


def render(rows: list[SourceRow]) -> str:
    required_count = sum(row.applicability == "required" for row in rows)
    excluded_count = len(rows) - required_count
    source_hash = EXPECTED_SOURCE_SET_SHA256
    lines = [
        "# authority: IEEE Std 1003.1-2017 and The Open Group Base Specifications Issue 7, 2018 edition",
        "# sources: utilities/printf.html and basedefs/V1_chap05.html from pinned susv4-2018.tgz",
        f"# snapshot-count: {len(rows)}",
        f"# required-count: {required_count}",
        f"# excluded-scope-count: {excluded_count}",
        f"# ordered-source-sha256: {source_hash}",
        "\t".join(EXPECTED_COLUMNS),
    ]
    for row in rows:
        lines.append(
            "\t".join(
                (
                    row.identifier,
                    row.title,
                    row.source_ref,
                    row.source_sha256,
                    row.applicability,
                    row.state,
                    row.witness,
                    row.next_action,
                )
            )
        )
    return "\n".join(lines) + "\n"


def validate_text(
    requirements_text: str,
    source_rows: list[SourceRow],
    case_ids: set[str],
) -> list[str]:
    failures: list[str] = []
    data_lines = [
        line
        for line in requirements_text.splitlines()
        if line and not line.startswith("#")
    ]
    if not data_lines:
        return ["missing printf requirements header"]
    if tuple(data_lines[0].split("\t")) != EXPECTED_COLUMNS:
        failures.append("printf requirements header mismatch")
    parsed_rows = [line.split("\t") for line in data_lines[1:]]
    if any(len(row) != len(EXPECTED_COLUMNS) for row in parsed_rows):
        failures.append("printf requirements row width mismatch")
        return failures
    identifiers = [row[0] for row in parsed_rows]
    duplicates = sorted(
        {identifier for identifier in identifiers if identifiers.count(identifier) > 1}
    )
    if duplicates:
        failures.append(f"duplicate printf requirements: {', '.join(duplicates)}")
    if len(parsed_rows) != len(source_rows):
        failures.append(
            f"printf requirements count mismatch: expected {len(source_rows)}, got {len(parsed_rows)}"
        )
    for row in parsed_rows:
        (
            identifier,
            _title,
            _source_ref,
            _source_hash,
            applicability,
            state,
            witness,
            next_action,
        ) = row
        if applicability == "required":
            if state not in {"open", "closed"}:
                failures.append(f"{identifier}: required row has invalid state {state}")
            if state == "open":
                if (
                    witness != f"missing:{identifier}"
                    or not next_action
                    or next_action == "-"
                ):
                    failures.append(
                        f"{identifier}: open row lacks an exact missing gate"
                    )
            elif state == "closed":
                if not witness.startswith("tests:") or next_action != "-":
                    failures.append(
                        f"{identifier}: closed row lacks exact test witnesses"
                    )
                else:
                    for reference in witness.removeprefix("tests:").split(","):
                        path_text, separator, case_id = reference.partition("#")
                        if not separator or path_text != str(QEMU_TEST_PATH):
                            failures.append(
                                f"{identifier}: invalid QEMU witness {reference}"
                            )
                        elif case_id not in case_ids:
                            failures.append(
                                f"{identifier}: unknown QEMU case {case_id}"
                            )
        elif applicability in {"optional", "conditional", "unspecified", "none"}:
            if (
                state != "excluded"
                or witness != f"scope:{applicability}"
                or next_action != "-"
            ):
                failures.append(f"{identifier}: excluded scope row is inconsistent")
        else:
            failures.append(f"{identifier}: invalid applicability {applicability}")
    if requirements_text != render(source_rows):
        failures.append(
            "printf requirements differ from generated official-source rows"
        )
    return failures


def require_failure(
    name: str,
    text: str,
    fragment: str,
    rows: list[SourceRow],
    case_ids: set[str],
) -> None:
    failures = validate_text(text, rows, case_ids)
    if not any(fragment in failure for failure in failures):
        raise AssertionError(f"{name}: expected {fragment!r}, got {failures}")


def run_self_test(
    original: str,
    rows: list[SourceRow],
    case_ids: set[str],
    utility_text: str,
    base_text: str,
) -> None:
    lines = original.splitlines()
    header_index = lines.index("\t".join(EXPECTED_COLUMNS))
    first_row_index = header_index + 1
    duplicate = lines.copy()
    duplicate.insert(first_row_index + 1, duplicate[first_row_index])
    require_failure(
        "duplicate", "\n".join(duplicate) + "\n", "duplicate printf", rows, case_ids
    )
    missing = lines.copy()
    del missing[first_row_index]
    require_failure(
        "missing", "\n".join(missing) + "\n", "count mismatch", rows, case_ids
    )
    changed_hash = lines.copy()
    fields = changed_hash[first_row_index].split("\t")
    fields[3] = "0" * 64
    changed_hash[first_row_index] = "\t".join(fields)
    require_failure(
        "source hash",
        "\n".join(changed_hash) + "\n",
        "generated official-source",
        rows,
        case_ids,
    )
    unknown_case = original.replace(
        "printf.mandatory_conversion_set",
        "printf.missing_executable_case",
        1,
    )
    require_failure("unknown case", unknown_case, "unknown QEMU case", rows, case_ids)
    mutated_utility = utility_text.replace(
        "shall write formatted operands", "shall alter formatted operands", 1
    )
    try:
        derive_rows(mutated_utility, base_text)
    except VerificationError as error:
        if "source-set hash mismatch" not in str(error):
            raise AssertionError(
                f"source mutation failed incorrectly: {error}"
            ) from error
    else:
        raise AssertionError("mutated printf source unexpectedly passed")


def main() -> int:
    arguments = parse_args()
    repository_root = arguments.repo_root.resolve()
    archive_path = resolve_path(repository_root, arguments.archive)
    requirements_path = resolve_path(repository_root, arguments.requirements)
    if not archive_path.is_file():
        print(f"SKIP: POSIX Issue 7 archive not found: {archive_path}")
        return 77
    try:
        utility_text, base_text = read_sources(archive_path)
        rows = derive_rows(utility_text, base_text)
        case_ids = read_case_ids(repository_root)
        generated_text = render(rows)
        if arguments.generate:
            requirements_path.parent.mkdir(parents=True, exist_ok=True)
            requirements_path.write_text(generated_text, encoding="ascii")
        requirements_text = requirements_path.read_text(encoding="ascii")
        failures = validate_text(requirements_text, rows, case_ids)
        if failures:
            raise VerificationError("\n".join(failures))
        if arguments.self_test:
            run_self_test(requirements_text, rows, case_ids, utility_text, base_text)
            print("POSIX printf requirement mutation self-test passed.")
            return 0
        open_count = sum(row.state == "open" for row in rows)
        closed_count = sum(row.state == "closed" for row in rows)
        excluded_count = sum(row.state == "excluded" for row in rows)
        print(
            "POSIX printf requirements verified: "
            f"{len(rows)} rows, {closed_count} closed, {open_count} open, "
            f"{excluded_count} excluded by source scope."
        )
        return 0
    except (OSError, UnicodeDecodeError, VerificationError, AssertionError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
