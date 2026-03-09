# Unified 32/64 Kernel Decomposition

Date: 2026-03-08

## Target Outcome

One shared kernel architecture with lane-specific entry glue:
- Shared core for correctness and feature velocity.
- Thin architecture adapters for i486 and x86_64.
- Compatibility profiles layered at syscall/IPC boundaries.

## Proposed Module Layout

1. Core (shared)
- `src/kernel/core/` (scheduler hooks, process lifecycle, capability policies)
- `src/mm/core/` (allocator/pmm abstractions, shared VM policy)
- `src/vfs/core/` (inode/vnode/path/fd shared core)

2. Architecture glue
- `src/kernel/arch/x86_64/` (syscall entry, irq/trap, context switch glue)
- `src/kernel/i486/` (int80/trap, ring3 glue, early boot handoff)

3. ABI compatibility layer
- `src/kernel/abi/` (syscall compatibility translation and argument marshalling)
- `include/xinim/abi/` public headers for lattice changer contracts

4. Userland runtime profiles
- `libc/dietlibc-xinim/` baseline C runtime
- future `libc/dietlibcpp/` or equivalent overlay for C++ runtime/ABI

## Compatibility Strategy

1. Native XINIM ABI is authoritative.
2. Linux/BSD/SVR4 compatibility profiles map into native syscalls.
3. Mach-style integrations use message/port bridge semantics, not direct syscall number mirroring.

## Refactor Sequence

1. Extract syscall translation interface (`abi/`).
2. Introduce per-profile mapping tables for minimal syscall set.
3. Route dispatch path through translator behind feature toggle.
4. Expand supported syscall surface by subsystem:
   - file/dir I/O
   - process lifecycle
   - signal/time
   - networking/socket semantics
5. Add lane-parity tests for i486/x86_64 at each slice.

## Performance Guardrails

- Do not add compatibility overhead to native fast path when disabled.
- Keep translation tables cache-friendly and branch-light.
- Benchmark syscall hot paths before and after each expansion.

## Done Criteria for Phase 1

- Same compatibility translator interface compiles for i486 and x86_64.
- Native path unaffected when compatibility translation is disabled.
- Minimal compatibility set works for read/write/open/close/exit/getpid/exec/fork/wait.
