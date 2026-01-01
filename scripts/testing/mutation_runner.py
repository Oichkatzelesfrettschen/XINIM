"""Mutation testing harness built to cooperate with Mull and existing suites."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, Sequence

DEFAULT_CONFIG = {
    "mutations": ["cxx_add_to_sub", "cxx_eq_to_ne"],
    "tests": ["xmake run contract-suite", "xmake run property-suite"],
}


def load_config(path: Path | None) -> Dict[str, Iterable[str]]:
    if path is None:
        return DEFAULT_CONFIG
    payload = json.loads(path.read_text(encoding="utf-8"))
    return {"mutations": payload.get("mutations", []), "tests": payload.get("tests", [])}


def _check_tool(name: str) -> Path | None:
    located = shutil.which(name)
    return Path(located) if located else None


def run_mutation_suite(config: Dict[str, Iterable[str]], mull_runner: Path | None) -> int:
    if mull_runner is None:
        print("mull-runner not found; skipping mutation execution but keeping configuration ready", file=sys.stderr)
        return 0

    mutations = list(config.get("mutations", []))
    tests = list(config.get("tests", []))
    if not tests:
        raise SystemExit("No tests configured for mutation analysis")

    run_cmd = [str(mull_runner), "--mutators", ",".join(mutations), "--"] + tests
    print(f"running: {' '.join(run_cmd)}")
    return subprocess.call(run_cmd)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, help="Optional JSON file overriding default config")
    parser.add_argument("--mull-runner", type=str, default="mull-runner", help="Path to mull-runner")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    mull_runner = _check_tool(args.mull_runner)
    config = load_config(args.config)
    return run_mutation_suite(config, mull_runner)


if __name__ == "__main__":
    raise SystemExit(main())
