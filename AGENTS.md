# XINIM Agent Guidance

## Scope

XINIM is a freestanding operating-system project. The supported modern boot
lane is the Limine x86_64 image running on QEMU `pc-q35-11.1` with the
`qemu64` CPU model. Work must preserve real Ring 3 process, syscall,
filesystem, terminal, and driver behavior. A fallback shell, Ring 0 shell,
fake userspace result, warning suppression, or timeout masking is not an
acceptable substitute for fixing the responsible subsystem.

## Language Ownership

- All project-owned C++ implementation and interface work uses C++23.
- When a project-owned legacy `.c`, `.cpp`, or `.hpp` implementation is
  touched, migrate the owned implementation to C++23 in the same change. Do
  not add or deepen project-owned C as part of implementation work.
- `.cpp` and `.hpp` files contain C++23, including freestanding C++23 where
  the kernel cannot use hosted-library facilities.
- Use `.hpp` for project-owned C++ interfaces. If a `.h` file has no real C
  or assembly consumer, migrate it to `.hpp` when its interface is touched.
- Retain `.h` only for a genuine C, assembly, firmware, bootloader, libc, or
  other external ABI boundary. A C-only header must be valid C23. A header
  shared by C and C++ must be valid in both C23 and C++23 and use narrowly
  scoped `#ifdef __cplusplus` and `extern "C"` guards where linkage requires
  them.
- C linkage describes an ABI; it does not permit project-owned implementation
  logic to remain in C. Keep `extern "C"` declarations and definitions at the
  smallest external boundary and implement owned behavior in C++23 behind it.
- Assembly remains assembly where architecture entry, interrupt, context,
  syscall, or instruction-level control requires it. Keep its ABI documented
  and verified by contract tests.
- Do not silently rewrite pinned third-party sources such as imported libc or
  shell code. Keep upstream source provenance intact. New or changed
  XINIM-owned adapters and integration behavior use C++23 unless an external
  build or ABI contract proves that a C23 boundary file is unavoidable.
- Treat compiler, linker, linter, and test warnings as errors. Do not suppress
  a warning to avoid correcting owned code.

## Implementation Discipline

- Trace failures through bootloader data, CPU setup, interrupt routing,
  timers, memory management, syscalls, libc, userspace, and test harnesses as
  applicable. Fix root causes at their owning layer.
- Use QEMU source as primary evidence for emulated hardware topology and
  register behavior. Record exact source paths and model versions. Implement
  clean-room guest drivers; do not copy GPL QEMU implementation code.
- Preserve user-owned worktree changes. Make minimal diffs and do not reset,
  discard, or overwrite unrelated work.
- Keep source comments mechanism-focused and durable. Put chronology, task
  labels, and review history in commits or review artifacts, not source.
- Use descriptive identifiers. Avoid single-letter names outside compact
  mathematical notation or conventional indices with obvious local scope.
- Do not use emoji in source, documentation, tests, generated text, or commit
  messages. UTF-8 text and technical notation are otherwise allowed.
- Use `docker compose`; never use the legacy `docker-compose` command.

## Build and Verification

The default CMake and kernel compiler lane is Clang: `clang` and `clang++`
from `CMakePresets.json`, with C++23 enabled for project-owned C++ and
`XINIM_ENABLE_WERROR=ON`. Verify the resolved paths and versions from the
build-tree cache or emitted command line before diagnosing a compiler issue;
the host installation is not a substitute for the configured lane. The
default 32-bit lane is Clang with `-m32`. `cross-elf` is an explicit 32-bit
alternative that resolves the selected `<triple>-gcc` and `<triple>-g++`
tools. The x86_64 dietlibc and pinned mksh userland recipes are separate,
intentional GCC/G++ build boundaries in `CMakeLists.txt`; that exception does
not change the kernel CMake compiler policy. Vendor C uses the explicit GNU
C11 profile, while Issue 7 C tests use strict C99 and their documented
feature-test macros.

Configure and build the x86_64 debug lane with warnings enabled as errors. For
the active boot and userspace boundary, use these focused gates:

```sh
cmake --build build/x86_64/Debug \
  --target test_bootfs_promote test_x86_64_userspace_abi \
  xinim_assembly_abi_check xinim_x86_64_image -j2

ctest --test-dir build/x86_64/Debug \
  -R '^(test_bootfs_promote|test_x86_64_userspace_abi|assembly_abi_verifier_self_test|assembly_abi_contract)$' \
  --output-on-failure

ctest --test-dir build/x86_64/Debug \
  -R '^x86_64_shell_test$' --output-on-failure
```

The exact boot-platform test uses `qemu-system-x86_64`,
`-machine pc-q35-11.1`, `-cpu qemu64`, and `-smp 1`. Do not weaken the machine,
CPU, privilege, timeout, or expected-output checks to obtain a pass.

Do not claim complete SUSv4 Issue 7 Shell and Utilities conformance until the
repository carries the modern standards-derived shell and utility ledger and
every required shell, utility, filesystem, process, signal, terminal, and
conformance gate passes in Ring 3 on the exact QEMU platform. Historical
POSIX.2/POSIXv2 requirements are not an alternate closure path. Do not claim
full SUSv4 or POSIX conformance until the complete applicable denominator,
including Base Definitions and System Interfaces, is versioned and closed.

The x86_64 image has one libc provider and one shell implementation. Every
staged `/bin` ELF must pass `scripts/verify_x86_64_runtime_ownership.py`: it is
either explicitly dietlibc-linked or an enumerated syscall-only assembly
binary. The sole mksh R59c source build uses its upstream legacy POSIX profile
and is installed byte-identically as `/bin/mksh` and `/bin/sh`; no alternate
shell is staged. Vendor C builds use explicit GNU C11, while Issue 7 C
conformance tests use strict C99 and the profile-specific feature-test macros
documented in `docs/posix/C_LANGUAGE_LIBC_SHELL_ALIGNMENT.md`.

## References

- `docs/external_sources/QEMU_X86_PC_SOURCES.md`
- `docs/guides/ASSEMBLY_ABI.md`
- `boot/limine/limine.cfg`
- `test/boot/x86_64_shell_test.py`
