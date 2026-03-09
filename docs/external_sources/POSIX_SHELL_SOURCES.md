# POSIX Shell Source Index

Date: 2026-03-08
Purpose: Primary-source index for the native `xash` shell rebuild, Austin
Group grounding, POSIX.1-2008 shell requirements, and the long-term
Issue 8 / POSIX.1-2024 roadmap.

## Sources

1. Austin Group overview
   - URL: https://www.opengroup.org/austin/
   - Accessed: 2026-03-08
   - Why it matters: authoritative home for the standards process behind the
     shell requirements XINIM intends to track.

2. POSIX.1-2008 Shell Command Language
   - URL: https://pubs.opengroup.org/onlinepubs/9699919799.2013edition/utilities/V3_chap02.html
   - Accessed: 2026-03-08
   - Why it matters: normative baseline for tokenization, quoting, expansion,
     redirection, command search, and execution semantics.

3. POSIX.1-2017 Base Definitions, Conformance
   - URL: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap02.html
   - Accessed: 2026-03-08
   - Why it matters: long-term roadmap anchor for what a future stricter shell
     and utility environment should satisfy.

4. POSIX Issue 8 Shell and Utilities
   - URL: https://pubs.opengroup.org/onlinepubs/9799919799/utilities/
   - Accessed: 2026-03-08
   - Why it matters: current Open Group Shell and Utilities publication for the
     long-term conformance target vocabulary used in repo docs.

5. POSIX Issue 8 rationale for Shell and Utilities
   - URL: https://pubs.opengroup.org/onlinepubs/9799919799/xrat/V4_xcu_chap01.html
   - Accessed: 2026-03-08
   - Why it matters: useful rationale when selecting which non-POSIX
     conveniences can coexist with a standards-first shell design.

## Local Evidence Anchors

- [XINIM_TOOLS_NATIVE_PORT_PLAN.md](/home/eirikr/Github/XINIM/docs/specs/XINIM_TOOLS_NATIVE_PORT_PLAN.md)
- [syscall_i386.hpp](/home/eirikr/Github/XINIM/include/xinim/userland/syscall_i386.hpp)
- [xash_smoke_test.sh](/home/eirikr/Github/XINIM/test/userland/xash_smoke_test.sh)
- [i486_syscall_smoke.cpp](/home/eirikr/Github/XINIM/test/userland/i486_syscall_smoke.cpp)

## Claims Tracked By These Sources

- `xash` is standards-first and should target POSIX.1-2008 behavior before
  broader compatibility.
- The shell and its first-wave tools should be rebuilt natively in C++ rather
  than shipped as direct imports of historical implementations.
- The 32-bit lane is incomplete until it can launch a true user-mode shell.
