#!/usr/bin/env python3
"""Verify the independently buildable x86_64 assembly ABI surface."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import subprocess
import sys
from collections.abc import Sequence


REQUIRED_FLAGS = ("-mno-red-zone", "-fno-pic", "-fno-pie")
EXCEPTION_ERROR_CODE_VECTORS = frozenset({8, 10, 11, 12, 13, 14, 17, 21, 29, 30})
SYSCALL_SAVED_GPRS = (
    "%rbx",
    "%rbp",
    "%r12",
    "%r13",
    "%r14",
    "%r15",
    "%rdi",
    "%rsi",
    "%rdx",
    "%r10",
    "%r8",
    "%r9",
    "%rax",
)
INTERRUPT_SAVED_GPRS = (
    "%rax",
    "%rbx",
    "%rcx",
    "%rdx",
    "%rsi",
    "%rdi",
    "%rbp",
    "%r8",
    "%r9",
    "%r10",
    "%r11",
    "%r12",
    "%r13",
    "%r14",
    "%r15",
)
CONTEXT_GPR_OFFSETS = (
    ("%r15", 0x00),
    ("%r14", 0x08),
    ("%r13", 0x10),
    ("%r12", 0x18),
    ("%r11", 0x20),
    ("%r10", 0x28),
    ("%r9", 0x30),
    ("%r8", 0x38),
    ("%rbp", 0x40),
    ("%rdi", 0x48),
    ("%rsi", 0x50),
    ("%rdx", 0x58),
    ("%rcx", 0x60),
    ("%rbx", 0x68),
    ("%rax", 0x70),
)
REQUIRED_SYMBOLS = {
    "context_restore.S.o": ("load_context", "load_context_ring3"),
    "entry_x86_64.S.o": ("_start",),
    "gdt_load.S.o": ("gdt_load",),
    "interrupts.S.o": (
        "timer_interrupt_handler",
        "generic_interrupt_handler",
        "keyboard_interrupt_handler",
        "com1_interrupt_handler",
        "com2_interrupt_handler",
        "x86_64_exception_0",
        "x86_64_exception_8",
        "x86_64_exception_31",
        "x86_64_exception_common",
        "spurious_interrupt_handler",
        "lidt",
    ),
    "syscall_handler.S.o": ("syscall_handler",),
    "tss_load.S.o": ("tss_load",),
}


def run_tool(command: Sequence[str]) -> str:
    completed = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"{' '.join(command)} failed:\n{completed.stdout}")
    return completed.stdout


def instruction_mnemonics(disassembly: str) -> list[str]:
    mnemonics: list[str] = []
    for line in disassembly.splitlines():
        match = re.search(
            r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+([a-z][a-z0-9]*)\b",
            line,
        )
        if match is None:
            match = re.search(r"\t([a-z][a-z0-9]*)\b", line)
        if match is not None:
            mnemonics.append(match.group(1))
    return mnemonics


def register_operands(disassembly: str, mnemonic: str) -> list[str]:
    return re.findall(
        rf"\b{re.escape(mnemonic)}\s+(%(?:r(?:ax|bx|cx|dx|si|di|bp|sp|8|9|1[0-5])))\b",
        disassembly,
    )


def syscall_argument_loads(disassembly: str) -> list[tuple[int, str]]:
    return [
        (int(offset, 16), register)
        for offset, register in re.findall(
            r"\bmov\s+0x([0-9a-f]+)\(%rsp\),"
            r"(%(?:rdi|rsi|rdx|rcx|r8|r9|rax))\b",
            disassembly,
        )
    ]


def normalized_instructions(disassembly: str) -> str:
    return re.sub(r"\s+", " ", disassembly)


def stack_operand(offset: int, base_register: str) -> str:
    if offset == 0:
        return f"({base_register})"
    return f"0x{offset:x}({base_register})"


def validate_syscall_disassembly(disassembly: str) -> list[str]:
    errors: list[str] = []
    lowered = disassembly.lower()
    mnemonics = instruction_mnemonics(lowered)
    if "sysret" in lowered:
        errors.append("syscall handler still contains SYSRET")
    if "iretq" not in mnemonics:
        errors.append("syscall handler lacks IRETQ")
    if "cld" not in mnemonics:
        errors.append("syscall handler enters C++ without CLD")
    if "xinim_syscall_dispatch" not in lowered:
        errors.append("syscall handler does not call xinim_syscall_dispatch")
    if re.search(r"lea[q]?\s+0x200\(%rsp\),\s*%rdi", lowered) is None:
        errors.append("syscall handler does not pass the checked frame pointer")
    if re.search(r"sub\s+\$0x200,%rsp", lowered) is None:
        errors.append("syscall handler lacks aligned FXSAVE storage")
    if re.search(r"add\s+\$0x200,%rsp", lowered) is None:
        errors.append("syscall handler does not release FXSAVE storage")

    ordered_fpu_bridge = (
        "fxsave64" in mnemonics
        and "call" in mnemonics
        and "fxrstor64" in mnemonics
        and mnemonics.index("fxsave64")
        < mnemonics.index("call")
        < mnemonics.index("fxrstor64")
    )
    if not ordered_fpu_bridge:
        errors.append("syscall handler does not preserve FPU/SIMD state around C++")

    call_index = mnemonics.index("call") if "call" in mnemonics else len(mnemonics)
    pushes_before_call = mnemonics[:call_index].count("push")
    if pushes_before_call != 18:
        errors.append(
            f"syscall frame has {pushes_before_call} pushes before CALL, expected 18"
        )
    expected_register_pushes = ["%r11", "%rcx", *SYSCALL_SAVED_GPRS]
    register_pushes = register_operands(lowered, "push")
    if register_pushes != expected_register_pushes:
        errors.append(
            "syscall register-save sequence is "
            f"{register_pushes}, expected {expected_register_pushes}"
        )

    expected_register_pops = list(reversed(SYSCALL_SAVED_GPRS))
    register_pops = register_operands(lowered, "pop")
    if register_pops != expected_register_pops:
        errors.append(
            "syscall register-restore sequence is "
            f"{register_pops}, expected {expected_register_pops}"
        )
    return errors


def validate_interrupt_disassembly(disassembly: str) -> list[str]:
    errors: list[str] = []
    lowered = disassembly.lower()
    for callback in (
        "timer_interrupt_c_handler",
        "handle_unhandled_irq",
        "handle_unhandled_exception",
        "keyboard_interrupt_c_handler",
        "com1_interrupt_c_handler",
        "com2_interrupt_c_handler",
    ):
        if callback not in lowered:
            errors.append(f"interrupt gates do not reference {callback}")
    if "cld" not in instruction_mnemonics(lowered):
        errors.append("interrupt gates enter C++ without CLD")
    if "iretq" not in instruction_mnemonics(lowered):
        errors.append("interrupt gates lack IRETQ")

    callback_gates = (
        "timer_interrupt_handler",
        "generic_interrupt_handler",
        "keyboard_interrupt_handler",
        "com1_interrupt_handler",
        "com2_interrupt_handler",
        "x86_64_exception_common",
    )
    for gate_name in callback_gates:
        gate = lowered.partition(f"<{gate_name}>:")[2].partition("\n\n")[0]
        gate_mnemonics = instruction_mnemonics(gate)
        expected_pushes = list(INTERRUPT_SAVED_GPRS) + ["%rax"] * 4
        expected_pops = ["%rax"] * 4 + list(reversed(INTERRUPT_SAVED_GPRS))
        gate_pushes = register_operands(gate, "push")
        gate_pops = register_operands(gate, "pop")
        if gate_pushes != expected_pushes:
            errors.append(
                f"{gate_name} register-save sequence is {gate_pushes}, "
                f"expected {expected_pushes}"
            )
        if gate_pops != expected_pops:
            errors.append(
                f"{gate_name} register-restore sequence is {gate_pops}, "
                f"expected {expected_pops}"
            )
        if re.search(r"and\s+\$0xfffffffffffffff0,%rsp", gate) is None:
            errors.append(f"{gate_name} does not align RSP before calling C++")
        normalized_gate = normalized_instructions(gate)
        if "mov %rsp,%rdi" not in normalized_gate:
            errors.append(f"{gate_name} does not pass the interrupt frame in RDI")
        ordered_bridge = (
            re.search(r"sub\s+\$0x200,%rsp", gate) is not None
            and "fxsave64" in gate_mnemonics
            and "call" in gate_mnemonics
            and "fxrstor64" in gate_mnemonics
            and gate_mnemonics.index("fxsave64")
            < gate_mnemonics.index("call")
            < gate_mnemonics.index("fxrstor64")
        )
        if not ordered_bridge:
            errors.append(f"{gate_name} does not preserve FPU/SIMD state around C++")

    for vector in range(32):
        if f"<x86_64_exception_{vector}>:" not in lowered:
            errors.append(f"interrupt gates lack exception vector {vector}")

    for vector in range(32):
        gate = lowered.partition(f"<x86_64_exception_{vector}>:")[2].partition("\n\n")[
            0
        ]
        pushed_immediates = [
            int(value, 16) for value in re.findall(r"push\s+\$0x([0-9a-f]+)", gate)
        ]
        expected_immediates = (
            [vector] if vector in EXCEPTION_ERROR_CODE_VECTORS else [0, vector]
        )
        if pushed_immediates != expected_immediates:
            errors.append(
                f"exception vector {vector} pushes {pushed_immediates}, "
                f"expected {expected_immediates}"
            )
        if "x86_64_exception_common" not in gate:
            errors.append(f"exception vector {vector} does not enter the common gate")

    common_gate = lowered.partition("<x86_64_exception_common>:")[2].partition("\n\n")[
        0
    ]
    if re.search(r"add\s+\$0x10,%rsp", common_gate) is None:
        errors.append("exception common gate does not remove vector and error words")
    return errors


def validate_context_disassembly(disassembly: str) -> list[str]:
    errors: list[str] = []
    lowered = disassembly.lower()
    load_context = lowered.partition("<load_context>:")[2].partition(
        "<load_context_ring3>:"
    )[0]
    load_context_ring3 = lowered.partition("<load_context_ring3>:")[2]

    functions = {
        "load_context": load_context,
        "load_context_ring3": load_context_ring3,
    }
    normalized_functions = {
        function_name: normalized_instructions(function_body)
        for function_name, function_body in functions.items()
    }

    restore_sources = {
        "load_context": "%rdi",
        "load_context_ring3": "%rdi",
    }
    for function_name, base_register in restore_sources.items():
        for register, offset in CONTEXT_GPR_OFFSETS:
            restore_instruction = (
                f"mov {stack_operand(offset, base_register)},{register}"
            )
            if restore_instruction not in normalized_functions[function_name]:
                errors.append(
                    f"{function_name} does not restore {register} from context offset "
                    f"0x{offset:x}"
                )

    kernel_frame_offsets = (0xB0, 0xA8, 0xA0, 0x98)
    for function_name, base_register in (("load_context", "%rdi"),):
        frame_instructions = (
            f"mov {stack_operand(kernel_frame_offsets[0], base_register)},%rsp",
            *(
                f"push {stack_operand(offset, base_register)}"
                for offset in kernel_frame_offsets[1:]
            ),
        )
        for instruction in frame_instructions:
            if instruction not in normalized_functions[function_name]:
                errors.append(
                    f"{function_name} lacks kernel IRETQ frame field: {instruction}"
                )

    ring3_frame_offsets = (0xB8, 0xB0, 0xA8, 0xA0, 0x98)
    for offset in ring3_frame_offsets:
        instruction = f"push {stack_operand(offset, '%rdi')}"
        if instruction not in normalized_functions["load_context_ring3"]:
            errors.append(f"load_context_ring3 lacks IRETQ frame field: {instruction}")

    for function_name, function_body in functions.items():
        mnemonics = instruction_mnemonics(function_body)
        if "fxrstor64" not in mnemonics:
            errors.append(f"{function_name} does not restore FPU/SIMD state")
        if "cli" not in mnemonics:
            errors.append(f"{function_name} does not mask interrupts during restore")
        if "iretq" not in mnemonics:
            errors.append(f"{function_name} does not finish restoration with IRETQ")
        if "cli" in mnemonics and "fxrstor64" in mnemonics and "iretq" in mnemonics:
            if not (
                mnemonics.index("cli")
                < mnemonics.index("fxrstor64")
                < mnemonics.index("iretq")
            ):
                errors.append(
                    f"{function_name} restores state outside the CLI-to-IRETQ window"
                )
    for function_name in ("load_context", "load_context_ring3"):
        if "fxrstor64 0xd0(%rdi)" not in normalized_functions[function_name]:
            errors.append(
                f"{function_name} restores FPU/SIMD state from the wrong offset"
            )
    if "popf" in instruction_mnemonics(lowered):
        errors.append("context restore enables saved RFLAGS before the final transfer")
    return errors


def validate_user_entry(
    disassembly: str, section_table: str, symbol_table: str
) -> list[str]:
    errors: list[str] = []
    lowered = disassembly.lower()
    if "$0x19" not in lowered or "syscall" not in instruction_mnemonics(lowered):
        errors.append("x86_64 user entry does not issue native SYS_exit 25")
    if "XINIM_NATIVE_SYS_EXIT" in symbol_table:
        errors.append("syscall-number macro leaked a linkable symbol")
    if re.search(r"\s+[BbDd]\s+", symbol_table) is not None:
        errors.append("x86_64 user entry unexpectedly defines writable storage")
    for line in section_table.splitlines():
        match = re.search(
            r"\]\s+\.(?:data|bss)\s+\S+\s+\S+\s+\S+\s+([0-9a-fA-F]+)\s",
            line,
        )
        if match is not None and int(match.group(1), 16) != 0:
            errors.append("x86_64 user entry contains nonempty data or BSS")
    return errors


def validate_self_test() -> None:
    good = "\n".join(
        [
            "\tpush $0x23",
            "\tpush 0x0(%rip)",
            "\tpush %r11",
            "\tpush $0x1b",
            "\tpush %rcx",
        ]
        + [f"\tpush {register}" for register in SYSCALL_SAVED_GPRS]
        + [
            "\tcld ",
            "\tsub $0x200,%rsp",
            "\tfxsave64 (%rsp)",
            "\tlea 0x200(%rsp),%rdi",
            "\tcall   xinim_syscall_dispatch_frame",
            "\tfxrstor64 (%rsp)",
            "\tadd $0x200,%rsp",
            *[f"\tpop {register}" for register in reversed(SYSCALL_SAVED_GPRS)],
            "\tiretq ",
        ]
    )
    if validate_syscall_disassembly(good):
        raise RuntimeError("known-good syscall fixture was rejected")
    bad = (
        good.replace("\tcld ", "\tnop ")
        .replace("\tlea 0x200(%rsp),%rdi", "\tnop")
        .replace("\tiretq ", "\tsysretq ")
    )
    bad_errors = validate_syscall_disassembly(bad)
    if not any("SYSRET" in error for error in bad_errors):
        raise RuntimeError("known-bad SYSRET fixture was accepted")
    if not any("CLD" in error for error in bad_errors):
        raise RuntimeError("known-bad DF fixture was accepted")
    if not any("frame pointer" in error for error in bad_errors):
        raise RuntimeError("known-bad frame-pointer fixture was accepted")

    syscall_fpu_bad = good.replace("\tfxrstor64 (%rsp)", "\tnop")
    if not any(
        "does not preserve FPU/SIMD" in error
        for error in validate_syscall_disassembly(syscall_fpu_bad)
    ):
        raise RuntimeError("known-bad syscall FPU fixture was accepted")

    syscall_fpu_storage_bad = good.replace("\tsub $0x200,%rsp", "\tnop", 1)
    if not any(
        "lacks aligned FXSAVE storage" in error
        for error in validate_syscall_disassembly(syscall_fpu_storage_bad)
    ):
        raise RuntimeError("known-bad syscall FXSAVE storage fixture was accepted")

    syscall_argument_bad = good.replace(
        "\tlea 0x200(%rsp),%rdi", "\tlea 0x210(%rsp),%rdi"
    )
    if not any(
        "frame pointer" in error
        for error in validate_syscall_disassembly(syscall_argument_bad)
    ):
        raise RuntimeError("known-bad syscall frame-pointer fixture was accepted")

    syscall_register_bad = good.replace("\tpop %r8", "\tpop %r9", 1)
    if not any(
        "register-restore sequence" in error
        for error in validate_syscall_disassembly(syscall_register_bad)
    ):
        raise RuntimeError("known-bad syscall register fixture was accepted")

    def interrupt_gate_fixture(
        gate_name: str, callback: str, final_instructions: Sequence[str] = ("\tiretq",)
    ) -> str:
        pushes = [f"\tpush {register}" for register in INTERRUPT_SAVED_GPRS]
        pushes.extend(["\tpush %rax"] * 4)
        pops = ["\tpop %rax"] * 4
        pops.extend(f"\tpop {register}" for register in reversed(INTERRUPT_SAVED_GPRS))
        return "\n".join(
            [
                f"<{gate_name}>:",
                *pushes,
                "\tmov %rsp,%rbp",
                "\tmov %rsp,%rdi",
                "\tand $0xfffffffffffffff0,%rsp",
                "\tsub $0x200,%rsp",
                "\tfxsave64 (%rsp)",
                "\tcld",
                f"\tcall {callback}",
                "\tfxrstor64 (%rsp)",
                "\tmov %rbp,%rsp",
                *pops,
                *final_instructions,
            ]
        )

    interrupt_good = "\n\n".join(
        [
            interrupt_gate_fixture("generic_interrupt_handler", "handle_unhandled_irq"),
            interrupt_gate_fixture(
                "timer_interrupt_handler", "timer_interrupt_c_handler"
            ),
            interrupt_gate_fixture(
                "keyboard_interrupt_handler", "keyboard_interrupt_c_handler"
            ),
            interrupt_gate_fixture(
                "com1_interrupt_handler", "com1_interrupt_c_handler"
            ),
            interrupt_gate_fixture(
                "com2_interrupt_handler", "com2_interrupt_c_handler"
            ),
        ]
        + [
            f"<x86_64_exception_{vector}>:\n"
            + (
                f"\tpush $0x{vector:x}"
                if vector in EXCEPTION_ERROR_CODE_VECTORS
                else f"\tpush $0x0\n\tpush $0x{vector:x}"
            )
            + "\n\tjmp x86_64_exception_common"
            for vector in range(32)
        ]
        + [
            interrupt_gate_fixture(
                "x86_64_exception_common",
                "handle_unhandled_exception",
                ("\tadd $0x10,%rsp", "\tiretq"),
            )
        ]
    )
    if validate_interrupt_disassembly(interrupt_good):
        raise RuntimeError("known-good interrupt fixture was rejected")
    interrupt_bad = interrupt_good.replace(
        "call handle_unhandled_exception", "call handle_unhandled_irq"
    ).replace("add $0x10,%rsp", "add $0x8,%rsp")
    interrupt_errors = validate_interrupt_disassembly(interrupt_bad)
    if not any("handle_unhandled_exception" in error for error in interrupt_errors):
        raise RuntimeError("known-bad exception callback fixture was accepted")
    if not any("vector and error" in error for error in interrupt_errors):
        raise RuntimeError("known-bad exception stack fixture was accepted")

    interrupt_fpu_bad = interrupt_good.replace("\tfxrstor64 (%rsp)", "\tnop", 1)
    if not any(
        "generic_interrupt_handler does not preserve FPU" in error
        for error in validate_interrupt_disassembly(interrupt_fpu_bad)
    ):
        raise RuntimeError("known-bad interrupt FPU fixture was accepted")

    interrupt_alignment_bad = interrupt_good.replace(
        "\tand $0xfffffffffffffff0,%rsp", "\tnop", 1
    )
    if not any(
        "generic_interrupt_handler does not align RSP" in error
        for error in validate_interrupt_disassembly(interrupt_alignment_bad)
    ):
        raise RuntimeError("known-bad interrupt alignment fixture was accepted")

    interrupt_register_bad = interrupt_good.replace("\tpush %r15", "\tnop", 1)
    if not any(
        "generic_interrupt_handler register-save sequence" in error
        for error in validate_interrupt_disassembly(interrupt_register_bad)
    ):
        raise RuntimeError("known-bad interrupt register fixture was accepted")

    wrong_error_vector = interrupt_good.replace(
        "<x86_64_exception_14>:\n\tpush $0xe",
        "<x86_64_exception_14>:\n\tpush $0x0\n\tpush $0xe",
    )
    if not any(
        "exception vector 14 pushes" in error
        for error in validate_interrupt_disassembly(wrong_error_vector)
    ):
        raise RuntimeError("known-bad error-code vector fixture was accepted")
    wrong_no_error_vector = interrupt_good.replace(
        "<x86_64_exception_15>:\n\tpush $0x0\n\tpush $0xf",
        "<x86_64_exception_15>:\n\tpush $0xf",
    )
    if not any(
        "exception vector 15 pushes" in error
        for error in validate_interrupt_disassembly(wrong_no_error_vector)
    ):
        raise RuntimeError("known-bad no-error vector fixture was accepted")

    context_restore_rdi_fixture = [
        f"\tmov {stack_operand(offset, '%rdi')},{register}"
        for register, offset in CONTEXT_GPR_OFFSETS
    ]
    context_good = "\n\n".join(
        [
            "\n".join(
                [
                    "<load_context>:",
                    "\tcli",
                    "\tfxrstor64 0xd0(%rdi)",
                    "\tmov 0xb0(%rdi),%rsp",
                    "\tpush 0xa8(%rdi)",
                    "\tpush 0xa0(%rdi)",
                    "\tpush 0x98(%rdi)",
                    *context_restore_rdi_fixture,
                    "\tiretq",
                ]
            ),
            "\n".join(
                [
                    "<load_context_ring3>:",
                    "\tcli",
                    "\tfxrstor64 0xd0(%rdi)",
                    "\tpush 0xb8(%rdi)",
                    "\tpush 0xb0(%rdi)",
                    "\tpush 0xa8(%rdi)",
                    "\tpush 0xa0(%rdi)",
                    "\tpush 0x98(%rdi)",
                    *context_restore_rdi_fixture,
                    "\tiretq",
                ]
            ),
        ]
    )
    if validate_context_disassembly(context_good):
        raise RuntimeError("known-good context fixture was rejected")
    context_bad = context_good.replace("\tcli", "\tpopf", 1).replace(
        "\tfxrstor64 0xd0(%rdi)", "\tnop", 1
    )
    context_errors = validate_context_disassembly(context_bad)
    if not any("load_context does not mask" in error for error in context_errors):
        raise RuntimeError("known-bad context interrupt-window fixture was accepted")
    if not any(
        "load_context does not restore FPU" in error for error in context_errors
    ):
        raise RuntimeError("known-bad context FPU fixture was accepted")

    context_offset_bad = context_good.replace(
        "\tmov (%rdi),%r15", "\tmov 0x8(%rdi),%r15", 1
    )
    if not any(
        "load_context does not restore %r15 from context offset 0x0" in error
        for error in validate_context_disassembly(context_offset_bad)
    ):
        raise RuntimeError("known-bad context offset fixture was accepted")

    entry_good = "\tmov $0x19,%rax\n\tsyscall"
    if validate_user_entry(entry_good, ".text PROGBITS", "T _start"):
        raise RuntimeError("known-good storage-free user entry fixture was rejected")
    entry_bad_errors = validate_user_entry(
        "\tmov $0x3c,%rax\n\tsyscall",
        "[ 2] .data PROGBITS 0000000000000000 000040 000008 00 WA 0 0 8",
        "0000000000000000 0000000000000008 D context_storage",
    )
    if not any("SYS_exit 25" in error for error in entry_bad_errors):
        raise RuntimeError("known-bad syscall-number fixture was accepted")
    if not any("writable storage" in error for error in entry_bad_errors):
        raise RuntimeError("known-bad storage-symbol fixture was accepted")
    if not any("data or BSS" in error for error in entry_bad_errors):
        raise RuntimeError("known-bad storage-section fixture was accepted")


def find_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required host tool is unavailable: {name}")
    return path


def check_compile_flags(compile_commands_path: pathlib.Path) -> list[str]:
    entries = json.loads(compile_commands_path.read_text(encoding="utf-8"))
    kernel_entries = [
        entry
        for entry in entries
        if entry.get("file", "").endswith("/src/kernel/main.cpp")
    ]
    if not kernel_entries:
        return ["compile_commands.json has no x86_64 kernel main.cpp entry"]
    command = kernel_entries[0].get("command", "")
    return [flag for flag in REQUIRED_FLAGS if flag not in command]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile-commands", type=pathlib.Path)
    parser.add_argument("--objects", nargs="*", type=pathlib.Path, default=[])
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    validate_self_test()
    if arguments.self_test and not arguments.objects:
        print("assembly ABI verifier self-test passed")
        return 0
    if arguments.compile_commands is None:
        parser.error("--compile-commands is required when objects are supplied")

    objdump = find_tool("objdump")
    readelf = find_tool("readelf")
    nm = find_tool("nm")
    errors = [
        f"x86_64 kernel compile command lacks {flag}"
        for flag in check_compile_flags(arguments.compile_commands)
    ]

    objects_by_name = {path.name: path for path in arguments.objects}
    for object_name, symbols in REQUIRED_SYMBOLS.items():
        object_path = objects_by_name.get(object_name)
        if object_path is None:
            errors.append(f"missing ABI object: {object_name}")
            continue

        section_table = run_tool((readelf, "-SW", str(object_path)))
        stack_line = next(
            (line for line in section_table.splitlines() if ".note.GNU-stack" in line),
            "",
        )
        if not stack_line:
            errors.append(f"{object_name} lacks .note.GNU-stack")
        elif re.search(
            r"\.note\.GNU-stack\s+PROGBITS\s+\S+\s+\S+\s+\S+\s+\S*X", stack_line
        ):
            errors.append(f"{object_name} requests an executable stack")

        symbol_table = run_tool((nm, "-S", "--defined-only", str(object_path)))
        for symbol in symbols:
            match = re.search(
                rf"^[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+[Tt]\s+{re.escape(symbol)}$",
                symbol_table,
                flags=re.MULTILINE,
            )
            if match is None:
                errors.append(f"{object_name} lacks sized text symbol {symbol}")
            elif int(match.group(1), 16) == 0:
                errors.append(f"{object_name} exports zero-sized symbol {symbol}")

        disassembly = run_tool((objdump, "-drwC", str(object_path)))
        if object_name == "syscall_handler.S.o":
            errors.extend(validate_syscall_disassembly(disassembly))
        elif object_name == "interrupts.S.o":
            errors.extend(validate_interrupt_disassembly(disassembly))
        elif object_name == "context_restore.S.o":
            errors.extend(validate_context_disassembly(disassembly))
        elif object_name == "entry_x86_64.S.o":
            errors.extend(validate_user_entry(disassembly, section_table, symbol_table))

    if errors:
        for error in errors:
            print(f"assembly ABI verification failed: {error}", file=sys.stderr)
        return 1

    print(f"assembly ABI verification passed for {len(arguments.objects)} objects")
    return 0


if __name__ == "__main__":
    sys.exit(main())
