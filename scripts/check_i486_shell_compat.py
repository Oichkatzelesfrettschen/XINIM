#!/usr/bin/env python3
"""
Validate staged 32-bit shell payloads against the i486 ELF loader contract.
"""

import argparse
import os
import struct
import sys
from typing import List, Tuple


ELF32_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
ELF32_PROGRAM_HEADER = struct.Struct("<IIIIIIII")

ELF_MAGIC = b"\x7fELF"
ELF_CLASS_32 = 1
ELF_DATA_LSB = 1
ELF_TYPE_EXEC = 2
ELF_MACHINE_386 = 3
PT_LOAD = 1

USER_VIRTUAL_BASE = 0x00400000
USER_ADDRESS_SPACE_SIZE = 0x00100000

INIT_SHELL_PRIORITY = ["/bin/mksh", "/bin/sh", "/bin/xash"]


def stage_path(stage_dir: str, image_path: str) -> str:
    return os.path.join(stage_dir, image_path.lstrip("/"))


def inspect_elf(path: str) -> Tuple[bool, str]:
    try:
        with open(path, "rb") as handle:
            image = handle.read()
    except OSError as exc:
        return False, f"read failed: {exc}"

    if len(image) < ELF32_HEADER.size:
        return False, "file too small for ELF32 header"

    header = ELF32_HEADER.unpack_from(image, 0)
    ident = header[0]
    elf_type = header[1]
    machine = header[2]
    entry = header[4]
    phoff = header[5]
    phentsize = header[9]
    phnum = header[10]

    if ident[:4] != ELF_MAGIC:
        return False, "not an ELF image"
    if ident[4] != ELF_CLASS_32 or ident[5] != ELF_DATA_LSB:
        return False, "not a 32-bit little-endian ELF image"
    if elf_type != ELF_TYPE_EXEC or machine != ELF_MACHINE_386:
        return False, "not an ELF32 i386 executable"
    if phentsize != ELF32_PROGRAM_HEADER.size:
        return False, "unexpected program-header size"
    if phnum == 0:
        return False, "missing program headers"

    table_size = phnum * ELF32_PROGRAM_HEADER.size
    if phoff > len(image) or table_size > (len(image) - phoff):
        return False, "program-header table is truncated"

    saw_loadable_segment = False
    for index in range(phnum):
        base = phoff + (index * ELF32_PROGRAM_HEADER.size)
        program = ELF32_PROGRAM_HEADER.unpack_from(image, base)
        program_type = program[0]
        offset = program[1]
        vaddr = program[2]
        filesz = program[4]
        memsz = program[5]

        if program_type != PT_LOAD:
            continue
        if memsz == 0 and filesz == 0:
            continue

        saw_loadable_segment = True

        if memsz < filesz:
            return False, f"segment {index} has memsz < filesz"
        if offset > len(image) or filesz > (len(image) - offset):
            return False, f"segment {index} extends past the file"
        if vaddr < USER_VIRTUAL_BASE:
            return False, (
                f"segment {index} loads below 0x{USER_VIRTUAL_BASE:08x}: "
                f"0x{vaddr:08x}"
            )

        region_offset = vaddr - USER_VIRTUAL_BASE
        if region_offset > USER_ADDRESS_SPACE_SIZE:
            return False, f"segment {index} starts outside the user window"
        if memsz > (USER_ADDRESS_SPACE_SIZE - region_offset):
            return False, f"segment {index} exceeds the user window"

    if not saw_loadable_segment:
        return False, "missing PT_LOAD segments"

    return True, f"loadable entry=0x{entry:08x}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate staged i486 shell binaries against the kernel loader"
    )
    parser.add_argument("--stage-dir", required=True, help="Path to the staged image root")
    parser.add_argument("--report", help="Optional output path for a text report")
    parser.add_argument(
        "--require-file",
        action="append",
        default=[],
        help="Image path that must exist under the stage directory",
    )
    args = parser.parse_args()

    if not os.path.isdir(args.stage_dir):
        print(f"SKIP: stage directory not found: {args.stage_dir}")
        return 77

    lines: List[str] = []
    missing_required: List[str] = []
    compatible_shells: List[str] = []
    incompatible_present: List[str] = []

    for image_path in args.require_file:
        if not os.path.isfile(stage_path(args.stage_dir, image_path)):
            missing_required.append(image_path)

    paths_to_check = list(dict.fromkeys(args.require_file + INIT_SHELL_PRIORITY + ["/boot/mksh", "/boot/xash"]))

    for image_path in paths_to_check:
        host_path = stage_path(args.stage_dir, image_path)
        if not os.path.isfile(host_path):
            lines.append(f"missing: {image_path}")
            continue

        compatible, detail = inspect_elf(host_path)
        status = "compatible" if compatible else "incompatible"
        lines.append(f"{status}: {image_path}: {detail}")
        if image_path in INIT_SHELL_PRIORITY and compatible:
            compatible_shells.append(image_path)
        if not compatible and image_path in ("/bin/mksh", "/boot/mksh", "/bin/sh", "/bin/xash", "/boot/xash"):
            incompatible_present.append(image_path)

    if missing_required:
        lines.append(
            "FAIL: missing required staged files: " + ", ".join(missing_required)
        )

    selected_shell = next((path for path in INIT_SHELL_PRIORITY if path in compatible_shells), None)
    if selected_shell is None:
        lines.append("FAIL: no kernel-loadable init shell is staged")
    else:
        lines.append(f"selected init shell: {selected_shell}")

    if "/bin/mksh" in incompatible_present:
        lines.append(
            "WARNING: /bin/mksh is staged but does not satisfy the i486 loader contract"
        )

    report = "\n".join(lines) + "\n"
    sys.stdout.write(report)

    if args.report:
        os.makedirs(os.path.dirname(args.report), exist_ok=True)
        with open(args.report, "w", encoding="utf-8") as handle:
            handle.write(report)

    if missing_required or selected_shell is None:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
