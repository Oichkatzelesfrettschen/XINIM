#!/usr/bin/env python3
"""Scan source files for architecture and clang constructs.

This script walks the repository tree, skipping version control and build
artifacts. It detects architecture-specific macros as well as common
clang/C++ features to help classify sources as modern or legacy.
"""

from __future__ import annotations

import os
import re
from typing import Dict, List, Tuple

# Macros indicating architecture specific handling
ARCH_MACROS = [
    r"__x86_64__",
    r"__i386__",
    r"__aarch64__",
    r"__arm__",
    r"__arm64__",
]

# Patterns describing clang-specific constructs and C++ features
CLANG_PATTERNS: Dict[str, str] = {
    "clang_attribute": r"__attribute__",
    "clang_builtin": r"__builtin_\w+",
    "constexpr": r"\bconstexpr\b",
    "auto": r"\bauto\b",
    "lambda": r"\[.*?\]\(.*?\)",
    "template": r"\btemplate\b",
    "namespace": r"\bnamespace\b",
    "std": r"\bstd::",
    "cplusplus_macro": r"__cplusplus",
}


def find_sources(root: str) -> List[str]:
    """Return a list of candidate source files under *root*."""
    extensions = (".cpp", ".hpp", ".c", ".h", ".s", ".asm")
    paths: List[str] = []
    for current, _, files in os.walk(root):
        if ".git" in current or current.startswith(os.path.join(root, "build")):
            # Skip version control and build artifacts
            continue
        for name in files:
            if name.endswith(extensions):
                paths.append(os.path.join(current, name))
    return paths


def scan_clang_features(text: str) -> List[str]:
    """Return clang constructs and C++ features found in *text*."""
    found: List[str] = []
    for key, pattern in CLANG_PATTERNS.items():
        if re.search(pattern, text):
            found.append(key)
    return found


def analyze(path: str) -> Tuple[str, List[str], List[str]]:
    """Classify a single file and return status, macros, and features."""
    macros: List[str] = []
    try:
        with open(path, "r", errors="ignore") as src:
            text = src.read()
    except OSError:
        return "unknown", macros, []

    for macro in ARCH_MACROS:
        if re.search(rf"\b{macro}\b", text):
            macros.append(macro)

    features = scan_clang_features(text)

    if path.endswith((".cpp", ".hpp")):
        status = "modern"
    else:
        status = "legacy"
    return status, macros, features


def main(root: str = ".") -> None:
    """Scan *root* and print a report of modern vs legacy files."""
    modern: Dict[str, Tuple[List[str], List[str]]] = {}
    legacy: List[str] = []

    for file in find_sources(root):
        status, macros, features = analyze(file)
        if status == "modern":
            modern[file] = (macros, features)
        else:
            legacy.append(file)

    print("Modern files:")
    for file, data in sorted(modern.items()):
        macros, features = data
        macro_list = ", ".join(macros) if macros else ""
        feature_list = ", ".join(features) if features else ""
        line = f"{file}"
        if macro_list:
            line += f" [macros: {macro_list}]"
        if feature_list:
            line += f" [features: {feature_list}]"
        print(line)

    print("\nLegacy files:")
    for file in legacy:
        print(file)


if __name__ == "__main__":
    main()
