#!/usr/bin/env python3
"""Extract an LLVM .profraw from a VirtualBox COM1 serial log.

WHY: The XINIM bare-metal kernel cannot write files; it dumps LLVM profile
     data to the COM1 serial port (framed as hex) when XINIM_PGO_MODE=generate.
     This script recovers the binary .profraw, then optionally merges it with
     llvm-profdata for use in the PGO 'use' phase.

WHAT: Scans <log> for the XNPGO_START.../XNPGO_END frame, decodes the hex
      payload, writes a .profraw file, and (if --merge) runs llvm-profdata
      merge to produce a .profdata suitable for -fprofile-instr-use.

HOW:
    # After booting the instrumented kernel in VirtualBox (COM1 -> file):
    python3 scripts/extract_pgo_profile.py \\
        --log build/i486/Debug/logs/vbox-pgo-com1.log \\
        --out build/i486/pgo.profraw \\
        --merge build/i486/pgo.profdata
"""

from __future__ import annotations
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


def extract(log_path: Path) -> bytes:
    """Return the raw profile bytes from the XNPGO framed section in the log."""
    text = log_path.read_text(errors="replace")

    start_m = re.search(r"XNPGO_START:([0-9a-f]{8})\n", text)
    if not start_m:
        sys.exit(f"error: XNPGO_START marker not found in {log_path}\n"
                 "       Did the kernel run to completion with PGO mode enabled?")

    end_m = re.search(r"XNPGO_END\n", text[start_m.end():])
    if not end_m:
        sys.exit("error: XNPGO_END marker not found -- log may be truncated")

    expected_size = int(start_m.group(1), 16)
    hex_payload = text[start_m.end(): start_m.end() + end_m.start()]

    # Strip newlines and whitespace; the rest must be hex digits
    hex_clean = re.sub(r"\s+", "", hex_payload)
    if not re.fullmatch(r"[0-9a-f]+", hex_clean):
        sys.exit("error: non-hex characters in profile payload")

    raw = bytes.fromhex(hex_clean)
    if len(raw) != expected_size:
        sys.exit(
            f"error: expected {expected_size} bytes, decoded {len(raw)}\n"
            "       The log may be incomplete or corrupted."
        )
    return raw


def main() -> int:
    ap = argparse.ArgumentParser(description="Extract LLVM .profraw from VBox COM1 log")
    ap.add_argument("--log", required=True, type=Path,
                    help="VirtualBox COM1 serial log file")
    ap.add_argument("--out", required=True, type=Path,
                    help="Output .profraw file path")
    ap.add_argument("--merge", default="", type=Path,
                    help="If set, run llvm-profdata merge and write .profdata here")
    args = ap.parse_args()

    if not args.log.is_file():
        sys.exit(f"error: log file not found: {args.log}")

    raw = extract(args.log)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(raw)
    print(f"Wrote {len(raw)} bytes -> {args.out}")

    if args.merge:
        llvm_profdata = shutil.which("llvm-profdata")
        if not llvm_profdata:
            sys.exit("error: llvm-profdata not found in PATH")
        args.merge.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [llvm_profdata, "merge", str(args.out), "-o", str(args.merge)],
            check=True,
        )
        print(f"Merged -> {args.merge}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
