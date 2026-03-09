#!/usr/bin/env python3
"""Audit first-party source coverage in the active CMake graph."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
from collections import Counter, defaultdict


SOURCE_DIRS = ("src", "test", "userland")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".s", ".S", ".asm"}
REFERENCE_PATTERN = re.compile(
    r"((?:src|test|userland)/[A-Za-z0-9_./+\-]+\.(?:cxx|cpp|cc|asm|S|s|c))"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--markdown-output")
    parser.add_argument("--json-output")
    parser.add_argument("--top", type=int, default=12)
    return parser.parse_args()


def cmake_reference_paths(repo_root: pathlib.Path) -> set[str]:
    text_parts: list[str] = []
    root_cmake = repo_root / "CMakeLists.txt"
    if root_cmake.is_file():
        text_parts.append(root_cmake.read_text(encoding="utf-8", errors="ignore"))

    cmake_dir = repo_root / "cmake"
    if cmake_dir.is_dir():
        for path in sorted(cmake_dir.rglob("*.cmake")):
            text_parts.append(path.read_text(encoding="utf-8", errors="ignore"))

    references: set[str] = set()
    for text in text_parts:
        for match in REFERENCE_PATTERN.finditer(text):
            references.add(match.group(1))
    return references


def first_party_sources(repo_root: pathlib.Path) -> list[str]:
    paths: list[str] = []
    for source_dir in SOURCE_DIRS:
        root = repo_root / source_dir
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                continue
            paths.append(path.relative_to(repo_root).as_posix())
    return paths


def group_key(path: str) -> str:
    parts = path.split("/")
    if len(parts) >= 2:
        return "/".join(parts[:2])
    return parts[0]


def summarize_unwired(paths: list[str]) -> tuple[Counter[str], dict[str, list[str]]]:
    grouped = Counter(group_key(path) for path in paths)
    samples: dict[str, list[str]] = defaultdict(list)
    for path in paths:
        key = group_key(path)
        if len(samples[key]) < 5:
            samples[key].append(path)
    return grouped, samples


def render_markdown(
    repo_root: pathlib.Path,
    referenced: set[str],
    unwired_by_root: dict[str, list[str]],
    top: int,
) -> str:
    lines = [
        "# Build Graph Audit",
        "",
        f"Repository root: `{repo_root}`",
        "",
        "This report scans `CMakeLists.txt` and `cmake/*.cmake` for first-party",
        "source references and compares that set to on-disk sources under `src`,",
        "`test`, and `userland`.",
        "",
        "## Summary",
        "",
        f"- referenced first-party sources: `{len(referenced)}`",
    ]

    total_unwired = sum(len(paths) for paths in unwired_by_root.values())
    lines.append(f"- unwired first-party sources: `{total_unwired}`")
    for root in SOURCE_DIRS:
        lines.append(f"- unwired `{root}` sources: `{len(unwired_by_root.get(root, []))}`")

    for root in SOURCE_DIRS:
        unwired = unwired_by_root.get(root, [])
        grouped, samples = summarize_unwired(unwired)
        lines.extend(
            [
                "",
                f"## {root}",
                "",
                f"- unwired files: `{len(unwired)}`",
            ]
        )
        if not unwired:
            lines.append("- status: fully referenced by the current CMake graph")
            continue

        lines.append("- largest unwired groups:")
        for name, count in grouped.most_common(top):
            lines.append(f"  - `{name}`: `{count}`")

        lines.append("- sample unwired files:")
        for path in unwired[:top]:
            lines.append(f"  - `{path}`")

        lines.append("- sample groups with examples:")
        for name, _count in grouped.most_common(min(top, len(grouped))):
            example_text = ", ".join(f"`{item}`" for item in samples[name])
            lines.append(f"  - `{name}`: {example_text}")

    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    repo_root = pathlib.Path(args.repo_root).resolve()
    referenced = cmake_reference_paths(repo_root)
    source_paths = first_party_sources(repo_root)

    unwired_by_root: dict[str, list[str]] = {name: [] for name in SOURCE_DIRS}
    for path in source_paths:
        root = path.split("/", 1)[0]
        if path not in referenced:
            unwired_by_root[root].append(path)

    markdown = render_markdown(repo_root, referenced, unwired_by_root, args.top)
    print(markdown, end="")

    if args.markdown_output:
        markdown_path = pathlib.Path(args.markdown_output)
        markdown_path.parent.mkdir(parents=True, exist_ok=True)
        markdown_path.write_text(markdown, encoding="utf-8")

    if args.json_output:
        json_path = pathlib.Path(args.json_output)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "repo_root": str(repo_root),
            "referenced_count": len(referenced),
            "unwired": unwired_by_root,
        }
        json_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
