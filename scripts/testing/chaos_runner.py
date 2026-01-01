"""Simple chaos test harness for shelling and perturbing subprocesses.

The runner is intentionally lightweight so it can operate inside developer
workstations and CI sandboxes without requiring privileged capabilities. It
spawns the requested command, randomly pauses/resumes it with POSIX signals,
and reports how many perturbations were issued. The deterministic seed ensures
reproducible runs for debugging failing scenarios.
"""

from __future__ import annotations

import argparse
import os
import random
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from typing import List, Sequence


@dataclass
class ChaosResult:
    """Outcome of a chaos experiment."""

    exit_code: int
    perturbations: int
    duration_seconds: float


class ChaosRunner:
    """Injects lightweight chaos into a running subprocess."""

    def __init__(self, command: Sequence[str], seed: int, interval: float) -> None:
        self._command: List[str] = list(command)
        self._seed = seed
        self._interval = interval
        self._perturbations = 0

    def run(self) -> ChaosResult:
        """Executes the command while injecting SIGSTOP/SIGCONT jitter."""

        start = time.perf_counter()
        process = subprocess.Popen(self._command)
        rng = random.Random(self._seed)

        def _inject() -> None:
            nonlocal process
            while process.poll() is None:
                time.sleep(self._interval)
                if rng.random() < 0.5:
                    os.kill(process.pid, signal.SIGSTOP)
                    self._perturbations += 1
                    time.sleep(self._interval / 2)
                    os.kill(process.pid, signal.SIGCONT)

        injector = threading.Thread(target=_inject, daemon=True)
        injector.start()
        process.wait()
        injector.join(timeout=0.1)

        return ChaosResult(
            exit_code=process.returncode,
            perturbations=self._perturbations,
            duration_seconds=time.perf_counter() - start,
        )


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """CLI argument parser."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs=argparse.ONE_OR_MORE, help="Command to run under chaos")
    parser.add_argument("--seed", type=int, default=7, help="Deterministic seed for reproducibility")
    parser.add_argument("--interval", type=float, default=0.5, help="Seconds between injections")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    """Entry point for the chaos runner."""

    args = parse_args(argv or sys.argv[1:])
    runner = ChaosRunner(args.command, args.seed, args.interval)
    result = runner.run()
    print(
        f"command={' '.join(args.command)} exit={result.exit_code} "
        f"perturbations={result.perturbations} duration={result.duration_seconds:.2f}s",
        flush=True,
    )
    return result.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
