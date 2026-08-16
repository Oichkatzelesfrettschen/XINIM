#!/usr/bin/env python3

import argparse
from pathlib import Path
import re


DECLARATION_PATTERN = re.compile(
    r"extern\s+int\s+__sigsetjmp\s*\([^;]+\)\s*__THROW\s*"
    r"__attribute__\s*\(\(__returns_twice__\)\)\s*;",
    re.DOTALL,
)


def verify_header(header_text: str) -> None:
    if DECLARATION_PATTERN.search(header_text) is None:
        raise RuntimeError("__sigsetjmp lacks the returns_twice ABI contract")
    required_macro = "#define sigsetjmp(a,b) __sigsetjmp(a,b)"
    if required_macro not in header_text:
        raise RuntimeError("sigsetjmp does not route through __sigsetjmp")


def self_test(header_text: str) -> None:
    verify_header(header_text)
    mutated_text = header_text.replace("__returns_twice__", "__unused__", 1)
    if mutated_text == header_text:
        raise RuntimeError("returns_twice mutation anchor is missing")
    try:
        verify_header(mutated_text)
    except RuntimeError as error:
        if "returns_twice ABI contract" not in str(error):
            raise
    else:
        raise RuntimeError("returns_twice mutation unexpectedly passed")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    header_text = Path(arguments.header).read_text(encoding="ascii")
    if arguments.self_test:
        self_test(header_text)
    else:
        verify_header(header_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
