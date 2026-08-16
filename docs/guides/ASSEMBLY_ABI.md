# C, C++, and assembly interfaces

XINIM uses preprocessed `.S` sources through CMake's ASM language. Clang is the
assembler driver, so assembly receives the same target definitions and include
paths as C and C++. Symbols that cross the language boundary use C linkage:
assembly exports an unmangled global symbol, and C++ declares it with
`extern "C"`.

The boundary is not only a symbol name. Each call must also agree on register
roles, structure offsets, stack alignment, direction flag state, segment
selectors, and the architectural return instruction.

## Shared ABI ownership

- `include/xinim/abi/x86_segment_selectors.h` owns the x86 GDT selectors.
- `include/xinim/abi/x86_64_context_layout.h` owns persistent x86_64 context
  offsets and the assembly interrupt-frame layout.
- `include/xinim/abi/x86_32_context_layout.h` owns `RegisterFrame` and
  `UserContext` offsets.
- `include/xinim/abi/native_syscall_numbers.h` exposes native XINIM syscall
  numbers to assembly. `include/xinim/sys/syscalls.h` derives `SYS_exit` from
  `XINIM_NATIVE_SYS_EXIT`. These values do not belong to the heritage MINIX or
  Linux syscall namespaces.
- `include/xinim/abi/syscall_dispatch.h` owns the C linkage and seven-parameter
  dispatcher signature used by the x86_64 syscall entry.

The headers are macro-only where assembly consumes them. Production C++ types
use `static_assert(sizeof(...))` and `static_assert(offsetof(...))` so a layout
change fails compilation instead of silently changing the assembly ABI.
Routine-private push offsets remain local to their assembly routine; they are
not persistent context storage.

Numeric equality does not merge namespaces. Native XINIM exit is syscall 25,
heritage MINIX call 25 is STIME, Linux x86_64 exit is syscall 60, and byte 60
in the i386 `UserContext` is the saved user-SS field. The context byte offset is
owned only by `x86_32_context_layout.h`; it is not syscall storage.

## x86_64 SYSCALL to C++

`src/arch/x86_64/syscall_handler.S` receives the architectural SYSCALL state:

```text
RAX  syscall number       RDI,RSI,RDX,R10,R8,R9  user arguments
RCX  user return RIP      R11                    user RFLAGS
RSP  user stack
```

The entry records user RSP in single-CPU scratch storage, switches to the
kernel syscall stack, builds an IRETQ frame, and saves all user GPRs except the
architectural RCX and R11 clobbers. It clears DF, saves the user x87/MMX/SSE image,
and calls:

```text
xinim_syscall_dispatch(number, a0, a1, a2, a3, a4, a5)
```

The first six C++ parameters use SysV registers; `a5`, the seventh parameter,
uses an aligned stack slot. The return value replaces the saved RAX slot,
the user x87/MMX/SSE image and preserved registers are restored, and IRETQ restores
the explicit user SS, RSP, RFLAGS, CS, and RIP. IRETQ is required by the active
GDT ordering; that ordering does not provide the adjacent selector pair SYSRET
would derive.

The syscall scratch stack is valid only while `NR_CPUS == 1`. An SMP change
must replace both the saved user RSP and syscall stack with per-CPU storage.

## x86_64 interrupts to C++

Every live IDT address is an assembly gate in
`src/arch/x86_64/interrupts.S`. The gate saves all GPRs and segment selectors,
loads kernel data selectors, clears DF, aligns the interrupted stack down to a
16-byte boundary, saves the x87/XMM/MXCSR state with FXSAVE64, and passes
`X86_64InterruptFrame*` in RDI. The C++ callback returns to the gate, which
restores the FPU/SIMD state and exact saved stack before executing IRETQ. This
is required because an asynchronous C++ callback may use compiler-generated
SSE instructions even when the interrupted code did not call it.

The fixed frame ends at RFLAGS. Hardware appends RSP and SS only when an
interrupt changes privilege level, so C++ must inspect CS before treating that
optional tail as present.

Each exception vector has a dedicated assembly stub. Stubs for exceptions
without a hardware error code push a synthetic zero; stubs for exceptions with
a hardware error code preserve the CPU value. The hardware-error set is 8,
10-14, 17, 21, 29, and 30. Every stub then pushes its vector number and enters
a common gate. `X86_64ExceptionFrame` exposes the normalized `vector`,
`error_code`, and `rip` fields, with compile-time offset assertions. The
unhandled-exception callback reports those fields and halts instead of returning
to a repeat fault.

Timer, keyboard, COM1, COM2, and generic IRQ callbacks issue LAPIC EOI.
Exception vectors 0 through 31 use separate callbacks that never issue EOI.
The LAPIC spurious gate returns without EOI as required for a spurious
local-APIC interrupt.

## x86_64 context loading

`src/arch/x86_64/context_restore.S` consumes `CpuContext_x86_64` offsets from
the shared layout header. The C++ scheduler calls only `load_context` and
`load_context_ring3`; both are non-returning restore operations. No exported
assembly routine claims to save the outgoing scheduler context. A new context
starts with x87 FCW 0x037F, an empty x87 tag word, and MXCSR 0x1F80. Every load
path executes FXRSTOR64. Restore paths execute CLI before changing state and
keep interrupts masked until a final IRETQ atomically installs RIP, CS, and
RFLAGS; no half-restored context can observe a saved IF bit.
`load_context_ring3` builds a five-word privilege-transition frame
and uses the same CLI-to-IRETQ rule. Function `.size` directives terminate at
each function, so ELF symbol ranges no longer overlap.

The timer callback invokes `schedule()`, but no x86_64 path copies the
interrupt frame into the current `CpuContext` or restores a different process.
The active assembly boundary therefore proves initial context restoration, not
runtime preemptive context switching.

## i486 INT 0x80 and i686 SYSENTER

The i486 interrupt path pushes `RegisterFrame` in the same order declared in
`ring3_internal.hpp`. Before each C++ call it clears DF and realigns the stack
to the i386 ABI. `UserContext` restore offsets come from the shared x86_32
layout header.

The i686 SYSENTER path has no hardware interrupt frame. Its assembly entry
therefore constructs the canonical EIP, CS, EFLAGS, ESP, and SS tail before
calling `i486_handle_syscall(RegisterFrame*)`. Interrupts remain disabled while
that frame is under construction. The entry preserves the wrapper-visible
callee-saved registers, restores user data selectors through CX so EAX remains
the syscall result, and returns with IRET.

## User entry

The xash entry objects call `xinim_user_main` using their platform C ABI. The
i486 entry retains the loader stack while deriving argc, argv, and envp, then
establishes 16-byte pre-call alignment before entering C++. If the C++ entry
returns, both platforms issue native `SYS_exit`, currently 25. The shared
numeric header emits no storage or symbol; preprocessing reduces each use to an
immediate move. The only entry-object text relocation is the intended call to
`xinim_user_main`.

## Retired live wiring

`src/kernel/mpx64_stubs.S`, `src/kernel/syscall.cpp`, and
`src/kernel/arch/x86_64/isr.S` are no longer part of the x86_64 target. Their
MINIX-era save/restart path did not match `proc::p_reg`, switched beyond its
allocated stack, and returned through incomplete interrupt frames. The unified
interrupt gates and `arch/x86_64/syscall_handler.S` own the live boundaries.

## Verification

Run the focused x86_64 contract independently of the full kernel:

```sh
cmake --build BUILD_DIR --target xinim_assembly_abi_check
ctest --test-dir BUILD_DIR -R assembly_abi --output-on-failure
```

The contract compiles the production headers and assembly with warnings as
errors, checks required kernel flags, validates symbol sizes and GNU-stack
notes, and inspects disassembly for the call and return invariants. It also
proves that the xash entry object has no data, BSS, or syscall-number symbol.
Its syscall, interrupt, exception-vector, context-restore, FPU-preservation,
and storage checks are calibrated with known-good and known-bad fixtures.

The i486 and i686 kernel plus xash targets provide full compile and link gates.
The current x86_64 boot enters a non-returning staged in-kernel shell before
initializing the IDT or SYSCALL path. Therefore the existing x86_64 shell test
does not constitute runtime evidence for userspace entry, interrupts, context
switching, or SYSCALL. Full runtime canary tests require making that userspace
handoff reachable first.
