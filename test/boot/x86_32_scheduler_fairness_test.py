#!/usr/bin/env python3
"""Check timer-driven progress of two CPU-bound i486 Ring 3 children."""

import os
import re
import subprocess
import sys

import x86_32_enhanced_test as guest


def main():
    if not os.path.isfile(guest.BOOT_IMAGE):
        print(f"SKIP: Boot image not found: {guest.BOOT_IMAGE}")
        return 77

    emulator = guest.start_qemu()
    try:
        shell = guest.connect_shell(retries=int(guest.BOOT_TIMEOUT / 0.5))
        prompt = guest.recv_until_prompt(shell, timeout=guest.BOOT_TIMEOUT)
        if not guest.contains_prompt(prompt):
            print("FAIL: no shell prompt")
            return 1

        response = guest.send_command(shell, "/bin/preempt_test")
        if not guest.check_status_zero("CPU-bound children", response):
            return 1
        if "PASS: both children completed (preemption works)" not in response:
            print(f"FAIL: preemption program did not complete: {response!r}")
            return 1

        if "Forking two CPU-bound children..." not in response:
            print(f"FAIL: preemption program header is missing: {response!r}")
            return 1
        output = response.split("Forking two CPU-bound children...", 1)[1]
        output = output.split("PASS: both children completed", 1)[0]
        tags = "".join(re.findall(r"[AB]", output))
        if re.search(r"ABA|BAB", tags) is None:
            print(f"FAIL: CPU-bound child output did not interleave: {tags!r}")
            return 1
        print(f"PASS: CPU-bound child output interleaved: {tags!r}")
        return 0
    finally:
        emulator.terminate()
        try:
            emulator.wait(timeout=5)
        except subprocess.TimeoutExpired:
            emulator.kill()


if __name__ == "__main__":
    sys.exit(main())
