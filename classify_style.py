"""Utility for classifying source files as C++17, K&R, or assembly."""

from __future__ import annotations

import os
import re


TYPE_TOKENS = {
    "int",
    "char",
    "long",
    "short",
    "float",
    "double",
    "void",
    "struct",
}


def _contains_type(text: str) -> bool:
    """Return True if any C type token appears in ``text``."""
    return any(token in text for token in TYPE_TOKENS)


def classify_file(path: str) -> str:
    """Classify a single source file by style."""

    ext = os.path.splitext(path)[1].lower()
    if ext in {".s", ".asm", ".S"}:
        return "assembly"
    if ext not in {".cpp", ".h"}:
        return ""

    try:
        with open(path, "r", errors="ignore") as src:
            lines = src.readlines()
    except OSError:
        return ""

    for i, line in enumerate(lines):
        if "(" not in line or ")" not in line:
            continue
        if line.lstrip().startswith("#"):
            continue
        # Only consider potential function headers.
        header = line.strip()
        # Skip prototypes and macros.
        if header.endswith(";"):
            continue
        # Search ahead for the opening brace.
        j = i
        while j < len(lines) and "{" not in lines[j]:
            j += 1
        if j == len(lines):
            continue
        inside = line.split("(", 1)[1].split(")", 1)[0]
        after = "".join(lines[i + 1 : j])
        if _contains_type(inside):
            return "C++17"
        if ";" in after or inside.strip() == "":
            return "K&R"
    return "unknown"


def walk(root: str) -> str:
    """Return an ASCII tree starting at ``root`` with style labels."""

    out_lines: list[str] = []
    for current, dirs, files in os.walk(root):
        if ".git" in dirs:
            dirs.remove(".git")
        if "__pycache__" in dirs:
            dirs.remove("__pycache__")
        dirs.sort()
        files.sort()
        level = current.cppount(os.sep) - root.cppount(os.sep)
        indent = "  " * level
        out_lines.append(f"{indent}{os.path.basename(current) if level else '.'}")
        for name in files:
            style = classify_file(os.path.join(current, name))
            if style:
                out_lines.append(f"{indent}  {name} ({style})")
            else:
                out_lines.append(f"{indent}  {name}")
    return "\n".join(out_lines)


if __name__ == "__main__":
    print(walk("."))
