# -*- coding: utf-8 -*-
"""Generate a JSON summary of K&R-style C++ source files."""

# Import standard libraries with explicit names for clarity
import json
import os
from typing import List

# Import the classify_file helper from classify_style.py
from classify_style import classify_file


def find_kr_cpp_files(root: str = ".") -> List[str]:
    """Return a list of K&R style C++ files under *root*."""
    kr_files: List[str] = []  # Accumulate matching file paths
    for current, _, files in os.walk(root):
        # Skip version control directories
        if ".git" in current.split(os.sep):
            continue
        for name in files:
            # Only process .cpp files
            if name.lower().endswith(".cpp"):
                path = os.path.join(current, name)
                style = classify_file(path)
                if style == "K&R":
                    kr_files.append(os.path.relpath(path, root))
    return kr_files


def main() -> None:
    """Write summary JSON to ``kr_cpp_summary.json``."""
    kr_list = find_kr_cpp_files()
    summary = {
        "count": len(kr_list),
        "files": sorted(kr_list),
    }
    with open("kr_cpp_summary.json", "w", encoding="utf-8") as out:
        json.dump(summary, out, indent=2)


if __name__ == "__main__":
    main()
