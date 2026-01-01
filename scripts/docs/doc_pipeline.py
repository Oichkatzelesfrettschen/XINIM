"""Documentation-as-code pipeline hooking Doxygen and Sphinx+Breathe."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Sequence


DOXYFILE = Path("docs/Doxyfile")
SPHINX_CONF_DIR = Path("docs/sphinx")


def _ensure_tool(name: str) -> Path:
    located = shutil.which(name)
    if not located:
        raise SystemExit(f"tool '{name}' not found on PATH")
    return Path(located)


def run_doxygen(doxygen: Path) -> None:
    subprocess.run([str(doxygen), str(DOXYFILE)], check=True)


def run_sphinx(sphinx_build: Path, output: Path, builder: str) -> None:
    subprocess.run(
        [str(sphinx_build), "-b", builder, str(SPHINX_CONF_DIR), str(output)],
        check=True,
    )


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--builder", default="html", choices=["html", "dirhtml", "json"], help="Sphinx builder"
    )
    parser.add_argument(
        "--output", type=Path, default=Path("build/docs"), help="Destination directory for Sphinx output"
    )
    parser.add_argument("--doxygen", type=str, default="doxygen", help="Doxygen executable")
    parser.add_argument("--sphinx-build", type=str, default="sphinx-build", help="Sphinx build executable")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    doxygen = _ensure_tool(args.doxygen)
    sphinx = _ensure_tool(args.sphinx_build)

    print("[doc] generating Doxygen XML artifacts")
    run_doxygen(doxygen)
    print(f"[doc] rendering Sphinx site with builder={args.builder}")
    run_sphinx(sphinx, args.output, args.builder)
    print(f"[doc] documentation emitted to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
