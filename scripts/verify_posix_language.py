#!/usr/bin/env python3
"""Fail on misleading broad POSIX-compliance language in active docs."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


DEFAULT_INCLUDE = ("README.md", "docs")
DEFAULT_EXCLUDE_PARTS = (
    "archive/legacy",
    "build/",
    ".xinim/",
    "docs/CHANGELOG.md",
    "docs/analysis/CLAIMS_AUDIT.md",
    "docs/analysis/tooling/",
)
ALLOWED_FILES = {
    "docs/posix/POSIX_COMPLIANCE_REPORT.md",
}
FORBIDDEN_PATTERNS = (
    re.compile(r"\b97\.22%\s+POSIX compliance\b", re.IGNORECASE),
    re.compile(r"\bPOSIX compliant\b", re.IGNORECASE),
    re.compile(r"\bSUSv5\b", re.IGNORECASE),
    re.compile(r"\b100%\s+POSIX compliance\b", re.IGNORECASE),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".")
    return parser.parse_args()


def should_skip(path: pathlib.Path, repo_root: pathlib.Path) -> bool:
    rel = path.relative_to(repo_root).as_posix()
    if rel in ALLOWED_FILES:
        return False
    return any(part in rel for part in DEFAULT_EXCLUDE_PARTS)


def iter_markdown_files(repo_root: pathlib.Path) -> list[pathlib.Path]:
    paths: list[pathlib.Path] = []
    for include in DEFAULT_INCLUDE:
        root = repo_root / include
        if root.is_file():
            paths.append(root)
            continue
        if not root.is_dir():
            continue
        paths.extend(sorted(root.rglob("*.md")))
    return paths


def check_text(rel: str, text: str) -> list[str]:
    failures: list[str] = []
    for lineno, line in enumerate(text.splitlines(), start=1):
        for pattern in FORBIDDEN_PATTERNS:
            if pattern.search(line):
                failures.append(f"{rel}:{lineno}: forbidden phrase: {line.strip()}")
        lowered = line.lower()
        if "posix compliance" not in lowered:
            continue
        if any(token in lowered for token in ("claims audit", "archived", "historical", "placeholder")):
            continue
        if "staged" in lowered or "progress" in lowered or "roadmap" in lowered:
            continue
        failures.append(f"{rel}:{lineno}: unqualified 'POSIX compliance': {line.strip()}")
    return failures


def main() -> int:
    args = parse_args()
    repo_root = pathlib.Path(args.repo_root).resolve()
    failures: list[str] = []
    for path in iter_markdown_files(repo_root):
        if should_skip(path, repo_root):
            continue
        rel = path.relative_to(repo_root).as_posix()
        text = path.read_text(encoding="utf-8", errors="ignore")
        failures.extend(check_text(rel, text))

    if failures:
        print("POSIX language verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print("POSIX language verification passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
