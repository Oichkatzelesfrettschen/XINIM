# ADR 0004: Limine Boot Protocol

Status: Accepted

## Context

The kernel needs a boot protocol that supports 64-bit long mode, higher-half kernels,
and provides memory maps, framebuffer info, and SMP trampoline. Early versions used
Multiboot2; Limine is the current standard for modern bare-metal x86-64 kernels.

## Decision

Use the Limine boot protocol (v3+ requests structure). The Limine protocol headers
are vendored in `third_party/limine/`. The kernel exposes request structures in the
`.limine_requests` section (via KEEP(*(.limine.requests)) in linker.ld).

The Limine boot shim is in `src/boot/limine/limine_boot.cpp` and `shim.cpp`.

## Consequences

- Root `linker.ld` includes `.limine_requests` section before `.text`.
- The kernel entry point `_start` is set up in the Limine shim.
- Multiboot header is retained in linker.ld for future dual-boot compatibility but
  is not the primary boot path.

## Date

2026-02-26
