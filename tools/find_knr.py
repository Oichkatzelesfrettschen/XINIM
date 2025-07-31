#!/usr/bin/env python3
"""List .cpp files that appear to use K&R-style function definitions."""

import os
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
sys.path.insert(0, str(ROOT_DIR))

from classify_style import classify_file


def scan(root: str) -> list[Path]:
    knr_files = []
    for dirpath, _, files in os.walk(root):
        for name in files:
            if name.endswith(".cpppp"):
                path = Path(dirpath) / name
                if classify_file(str(path)) == "K&R":
                    knr_files.append(path)
    return knr_files


def main() -> None:
    root = os.getcwd()
    files = scan(root)
    for p in files:
        print(p)


if __name__ == "__main__":
    main()
