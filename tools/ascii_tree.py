import argparse
import os
from typing import List, Optional


def list_directory(
    path: str,
    prefix: str = "",
    show_hidden: bool = False,
    exclude: Optional[List[str]] = None,
) -> List[str]:
    """Recursively build an ASCII representation of ``path``.

    Parameters
    ----------
    path:
        Directory to walk.
    prefix:
        Indentation prefix for the current level.
    show_hidden:
        Whether to include hidden files.
    exclude:
        A list of file or directory names to skip.

    Returns
    -------
    List[str]
        Lines composing the ASCII tree.
    """
    if exclude is None:
        exclude = []
    entries = sorted(os.listdir(path))
    lines: List[str] = []
    for index, entry in enumerate(entries):
        if entry in exclude:
            # Skip explicitly excluded names.
            continue
        if not show_hidden and entry.startswith("."):
            # Skip hidden files unless explicitly requested.
            continue
        # Determine connectors based on whether this is the last entry.
        is_last = index == len(entries) - 1
        connector = "└── " if is_last else "├── "
        full_path = os.path.join(path, entry)
        lines.append(f"{prefix}{connector}{entry}")
        if os.path.isdir(full_path):
            extension = "    " if is_last else "│   "
            lines.extend(
                list_directory(
                    full_path,
                    prefix + extension,
                    show_hidden=show_hidden,
                    exclude=exclude,
                )
            )
    return lines


def main() -> None:
    """Entry point for command line execution."""
    parser = argparse.ArgumentParser(description="Output an ASCII file tree.")
    parser.add_argument(
        "path", nargs="?", default=".", help="Root of the directory tree"
    )
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        help="Include hidden files in the listing",
    )
    parser.add_argument(
        "-e",
        "--exclude",
        action="append",
        default=[],
        help="File or directory names to exclude",
    )
    args = parser.parse_args()

    tree_lines = [os.path.abspath(args.path)]
    tree_lines += list_directory(args.path, show_hidden=args.all, exclude=args.exclude)
    print("\n".join(tree_lines))


if __name__ == "__main__":
    main()
