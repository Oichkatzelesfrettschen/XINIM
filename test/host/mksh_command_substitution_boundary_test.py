#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


CAPTURE_PATTERN = re.compile(
    r"XINIM_CAPTURE bytes=([0-9a-f]*) scan_alias_lookups=([0-9]+)"
)
EXPECTED_ALIAS_CAPTURE = "7820706879736963616c5f7461696c29"
EXPECTED_FOLDED_CAPTURE = "7072696e7466202573206c6566745c0a726967687429"
EXPECTED_REPLAY_CAPTURE = (
    "28206563686f202428747275652920242828312b32292920297c74722075207829"
)


def replace_once(source_text: str, old_text: str, new_text: str) -> str:
    if source_text.count(old_text) != 1:
        raise RuntimeError(f"mutation anchor count is not one: {old_text!r}")
    return source_text.replace(old_text, new_text, 1)


def verify_mechanism_source(lex_text: str) -> None:
    required_fragments = (
        "(frame->type == SALIAS || frame->type == SREREAD)",
        "source_alias_suppression_depth == 0",
        "source_read_byte(source) : getsc_bn()",
        "while ((c = source_read_byte(s)) == 0)",
    )
    for fragment in required_fragments:
        if fragment not in lex_text:
            raise RuntimeError(f"missing command-substitution mechanism: {fragment}")


def require_mutation_failure(lex_text: str, old_text: str, new_text: str) -> None:
    mutated_text = replace_once(lex_text, old_text, new_text)
    try:
        verify_mechanism_source(mutated_text)
    except RuntimeError:
        return
    raise RuntimeError(f"mechanism mutation unexpectedly passed: {old_text}")


def run_shell(shell_path: Path, script: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(shell_path)],
        input=script,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def captures(result: subprocess.CompletedProcess[str]) -> list[tuple[str, int]]:
    return [
        (match.group(1), int(match.group(2)))
        for match in CAPTURE_PATTERN.finditer(result.stderr)
    ]


def require_capture(
    result: subprocess.CompletedProcess[str], expected_hex: str
) -> None:
    matching = [count for data, count in captures(result) if data == expected_hex]
    if matching != [0]:
        raise RuntimeError(
            f"expected one alias-free capture {expected_hex}, got {captures(result)!r}"
        )


def build_trace_shell(source_directory: Path, compiler: str, build_root: Path) -> Path:
    build_directory = build_root / "source"
    shutil.copytree(source_directory, build_directory)
    environment = os.environ.copy()
    environment.update(
        {
            "CC": compiler,
            "CFLAGS": "-std=gnu11 -O2 -Wall -Wextra -Werror",
            "CPPFLAGS": "-DMKSH_BINSHPOSIX -DMKSH_XINIM_CAPTURE_TEST",
            "LC_ALL": "C",
        }
    )
    subprocess.run(
        ["sh", "Build.sh", "-r"],
        cwd=build_directory,
        env=environment,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return build_directory / "mksh"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--cc", default="clang")
    arguments = parser.parse_args()

    source_directory = Path(arguments.source_dir).resolve()
    lex_text = (source_directory / "lex.c").read_text(encoding="latin-1")
    verify_mechanism_source(lex_text)
    require_mutation_failure(
        lex_text,
        "source_alias_suppression_depth == 0",
        "source_alias_suppression_depth != 0",
    )
    require_mutation_failure(
        lex_text,
        "(frame->type == SALIAS || frame->type == SREREAD)",
        "(frame->type == SALIAS)",
    )
    require_mutation_failure(
        lex_text,
        "source_read_byte(source) : getsc_bn()",
        "*source->str++ : getsc_bn()",
    )

    with tempfile.TemporaryDirectory(prefix="xinim-mksh-boundary-test-") as temporary:
        shell_path = build_trace_shell(source_directory, arguments.cc, Path(temporary))

        benign = run_shell(
            shell_path,
            "alias x='printf benign'\nprint -- \"$(x physical_tail)\"\n",
        )
        hostile = run_shell(
            shell_path,
            "alias x='printf hostile )'\nprint -- \"$(x physical_tail)\"\n",
        )
        require_capture(benign, EXPECTED_ALIAS_CAPTURE)
        require_capture(hostile, EXPECTED_ALIAS_CAPTURE)
        if "XINIM_ALIAS_LOOKUP name=x" not in benign.stderr:
            raise RuntimeError("bounded parse did not perform the benign alias lookup")
        if benign.returncode != 0 or "benign" not in benign.stdout:
            raise RuntimeError(f"benign alias parse failed: {benign!r}")
        if (
            hostile.returncode == 0
            or "syntax error: unexpected ')'" not in hostile.stderr
        ):
            raise RuntimeError(
                f"hostile alias was not rejected by bounded parse: {hostile!r}"
            )
        if "hostile physical_tail)" in hostile.stdout:
            raise RuntimeError(
                "hostile alias moved the physical substitution delimiter"
            )

        alias_grammar = run_shell(
            shell_path,
            "alias OPEN='{' CLOSE='};'\n"
            'var=$({ OPEN echo hi3; CLOSE }) && echo "$var"\n',
        )
        if alias_grammar.returncode != 0 or "hi3" not in alias_grammar.stdout:
            raise RuntimeError(
                f"bounded parse lost alias-dependent grammar: {alias_grammar!r}"
            )
        if not all(count == 0 for _, count in captures(alias_grammar)):
            raise RuntimeError("boundary recognition performed an alias lookup")

        folded = run_shell(
            shell_path,
            'print -- "$(printf %s left\\\nright)"\n',
        )
        require_capture(folded, EXPECTED_FOLDED_CAPTURE)
        if folded.returncode != 0 or "leftright" not in folded.stdout:
            raise RuntimeError(f"backslash-newline execution failed: {folded!r}")

        replay = run_shell(
            shell_path,
            'print -- "$(( echo $(true) $((1+2)) )|tr u x)"\n',
        )
        require_capture(replay, EXPECTED_REPLAY_CAPTURE)
        if replay.returncode != 0:
            raise RuntimeError(f"dollar-double-paren replay failed: {replay!r}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
