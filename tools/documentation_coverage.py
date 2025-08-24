#!/usr/bin/env python3
"""! @file
@brief Documentation coverage analyzer for the XINIM project.

This script scans specified directories for source and documentation files,
measures the proportion of lines containing Doxygen-style comments, and
emits a simple coverage report. The build fails if the measured coverage
falls below a configurable threshold.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys
from typing import Iterable, Tuple

# Regular expressions that identify Doxygen comment lines.
COMMENT_PATTERNS = [
    re.compile(r"^\s*///"),
    re.compile(r"^\s*/\*\*"),
    re.compile(r"^\s*\*"),
]


def count_documented_lines(path: Path) -> Tuple[int, int]:
    r"""! \brief Count documented and total lines in a file.

    \param path Path to the file being analyzed.
    \return A tuple ``(documented, total)`` with the number of documented and
            total non-empty lines.
    """

    documented = 0
    total = 0
    with path.open("r", encoding="utf-8", errors="ignore") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped:
                continue
            total += 1
            if any(pattern.match(stripped) for pattern in COMMENT_PATTERNS):
                documented += 1
    return documented, total


def scan_directories(directories: Iterable[Path]) -> float:
    r"""! \brief Compute coverage across directories.

    \param directories Iterable of directory paths to scan.
    \return Percentage of documented lines across all files.
    """

    documented_total = 0
    line_total = 0
    for directory in directories:
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix in {".h", ".hpp", ".c", ".cpp", ".md", ".rst"}:
                documented, lines = count_documented_lines(path)
                documented_total += documented
                line_total += lines
    if line_total == 0:
        return 100.0
    return 100.0 * documented_total / line_total


def main() -> None:
    """! \brief Program entry point."""

    parser = argparse.ArgumentParser(description="Measure documentation coverage.")
    parser.add_argument("paths", nargs="+", type=Path, help="Directories to scan")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("docs/analysis/documentation_coverage.txt"),
        help="Destination report file",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=80.0,
        help="Minimum acceptable coverage percentage",
    )
    args = parser.parse_args()

    coverage = scan_directories(args.paths)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(f"Documentation coverage: {coverage:.2f}%\n")

    if coverage < args.threshold:
        print(
            f"Documentation coverage {coverage:.2f}% is below threshold {args.threshold}%",
            file=sys.stderr,
        )
        raise SystemExit(1)

    print(
        f"Documentation coverage {coverage:.2f}% meets threshold {args.threshold}%",
    )


if __name__ == "__main__":
    main()
