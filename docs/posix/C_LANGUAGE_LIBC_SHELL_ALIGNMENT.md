# C Language, Libc, and Shell Alignment

This document defines the standards and runtime ownership contract for the
supported x86_64 QEMU lane. It separates the language used to build a component
from the interface version that the component is expected to implement.

## Selected standards baseline

The target is IEEE Std 1003.1-2017 and The Open Group Base Specifications
Issue 7, 2018 edition, including Technical Corrigenda 1 and 2. The retained
`susv4-2018.tgz` archive was last modified by the publisher on 2022-04-08. Its
identity and acquisition procedure are recorded in
`docs/external_sources/POSIX_SHELL_SOURCES.md`.

The retained Base Definitions name ISO/IEC 9899:1999 plus Corrigenda 1, 2, and
3 as the ISO C reference. The C-language development option requires the
`c99` utility. A conforming POSIX C application defines
`_POSIX_C_SOURCE=200809L` before including a header. An XSI application defines
`_XOPEN_SOURCE=700`; that value also enables the 200809L POSIX interface set.

The resulting language matrix is:

| Surface | Required mode | Meaning |
| --- | --- | --- |
| Issue 7 application contract | ISO C99 plus Corrigenda 1-3 | Normative C language baseline |
| POSIX-only C test | `-std=c99`, `_POSIX_C_SOURCE=200809L` | Tests the Issue 7 POSIX interface |
| XSI C test | `-std=c99`, `_XOPEN_SOURCE=700` | Tests an explicitly selected XSI row |
| Pinned dietlibc source | `-std=gnu11` | Reproducible vendor build dialect |
| Pinned mksh source | `-std=gnu11` | Reproducible vendor build dialect |
| XINIM-owned implementation | `-std=c++23` | Project language ownership policy |
| Genuine C-only boundary header | C23 | Project ABI boundary policy, not the POSIX test dialect |

C17 and C++17 have no normative role in this Issue 7 target. C17 would be a
valid implementation language for code that still presents the required C99
and POSIX interfaces, but selecting it would not improve the conformance claim.
C++ cannot replace the required C application interface. XINIM-owned C++23 can
implement that interface behind narrow `extern "C"` ABI boundaries.

GNU C11 is selected for the unchanged upstream dietlibc and mksh C sources
because both trees use implementation extensions and because an explicit mode
prevents host compiler defaults from drifting. Those sources still compile
with the project Clang driver. GNU C11 is not described as the POSIX
conformance level.

## Libc ownership

The supported x86_64 guest has one libc provider: the pinned XINIM dietlibc
tree. A syscall-only assembly executable may use no libc. No executable may
use glibc, musl, another static libc, a dynamic loader, or a shared library.
Clang compiler-rt builtins are an explicitly linked compiler runtime and are
not a libc.

The build establishes this through all of these constraints:

1. Pinned dietlibc and mksh sources retain upstream provenance.
2. Dietlibc and mksh compile with `-nostdinc` and the built dietlibc headers.
3. Dietlibc consumers link with `-nostdlib -static`, explicit `start.o`, the
   explicit `dietlibc.a`, and the Clang compiler-rt builtins archive.
4. A CMake-generated manifest binds each provider row to its exact build
   artifact, and the image verifier requires byte identity with the staged
   executable.
5. The image verifier rejects `PT_INTERP` and every `DT_NEEDED` entry.
6. Every dietlibc consumer must expose dietlibc's link-guard sentinel symbol.
7. Every syscall-only executable must be named in the finite provider table
   and must not expose the dietlibc sentinel.
8. Every unexpected `/bin` entry fails image construction.

The current finite x86_64 provider denominator is:

| Provider | Guest executables |
| --- | --- |
| dietlibc | `bootfs-reuse-check`, `byte-oracle`, `mkdir`, `mksh`, `printf`, `printf-signal-oracle`, `rm`, `sh` |
| syscall-only | `brk-check`, `echo`, `file-operations-check`, `fs-abi-check`, `mmap-check`, `preempt-check`, `runtime-check`, `select-check`, `signal-check`, `true`, `tty-abi-check` |

`scripts/verify_x86_64_runtime_ownership.py` is the executable owner of this
table. Its mutation self-test proves that the gate rejects an alternate shell,
an unknown binary, a missing binary, mismatched shell aliases, a dynamic
loader, a shared-library dependency, a staged/build artifact mismatch, a
missing dietlibc sentinel, and a misclassified syscall-only binary.

This proves build and runtime ownership. It does not prove that dietlibc
implements every Issue 7 interface. The selected dietlibc 0.35 headers still
advertise `_POSIX_VERSION` as `199506L`. Enabling `WANT_FULL_POSIX_COMPAT`
selects additional implementation behavior; it is not a certification and
does not authorize raising `_POSIX_VERSION`.

## Shell ownership and profile

The supported x86_64 image contains one shell implementation from one pinned
mksh R59c source tree. That tree is built with upstream's `-L` mode, which
defines `MKSH_LEGACY_MODE`, and with `MKSH_BINSHPOSIX`.

This profile is required on x86_64 because Issue 7 requires at least signed
`long` arithmetic for shell arithmetic expansion. Full mksh deliberately uses
fixed 32-bit arithmetic on 64-bit hosts. The upstream `-L` profile instead
defines its arithmetic type as the C implementation's `long` and is upstream's
recommended closer POSIX profile.

The one resulting executable is installed byte-for-byte as both:

```text
/bin/mksh
/bin/sh
```

Invocation as `/bin/sh` enables POSIX mode through `MKSH_BINSHPOSIX`.
Invocation as `/bin/mksh` requires `-o posix` when it is used as a conformance
test shell. `/bin/lksh`, `/bin/xash`, and other shell implementations are not
installed. The phrase "mksh-only" therefore means one mksh R59c source and
binary implementation; it does not mean the non-legacy full-mksh feature
profile. The binary's `LEGACY KSH R59` identity remains visible so the selected
upstream profile is not disguised.

The `C` locale and disabled mksh UTF-8 mode remain part of the selected POSIX
shell profile. A passing shell smoke test is necessary but does not close the
70-row shell-language denominator.

## POSIX conformance test admission

The imported `third_party/gpl/posixtestsuite-main` tree is test source, not a
conformance result. Many tests explicitly select `_POSIX_C_SOURCE=200112L` or
`_XOPEN_SOURCE=600`; those tests target an older profile and cannot be counted
as Issue 7 rows without a reviewed, source-specific adaptation.

A POSIX C test counts as x86_64 XINIM evidence only after all of these gates
pass:

1. A finite manifest names the test, the exact Issue 7 requirement, whether it
   is POSIX or XSI, its feature-test macros, prerequisites, and expected result.
2. The source compiles as strict C99 with `-Wall -Wextra -Werror` and no host
   include paths. POSIX rows define `_POSIX_C_SOURCE=200809L`; XSI rows define
   `_XOPEN_SOURCE=700`. A source that intentionally tests an older macro value
   remains an older-profile row and is labeled as such.
3. The link uses `-nostdlib -static`, the XINIM dietlibc `start.o` and
   `dietlibc.a`, and only the required compiler runtime.
4. The runtime ownership verifier classifies the resulting guest executable as
   dietlibc. The finite provider denominator must be updated in the same change.
5. The test executes in Ring 3 inside the supported QEMU
   `pc-q35-11.1`, `qemu64`, single-vCPU guest. A host-glibc build or host
   execution is harness evidence only.
6. Shell-language tests execute `/bin/sh` under `LC_ALL=C`; they do not use the
   host shell and do not invoke an alternate guest shell.
7. The manifest records pass, fail, unresolved, unsupported, or untested for
   every admitted row. Only an exact pass can close a requirement.

The C-language development option also requires an in-system `c99` utility.
Cross-compiling tests with the host compiler does not prove that optional
development environment. XINIM must not claim that option until the guest
compiler, headers, linker behavior, and `c99` utility contract have their own
finite tests.

## Verification

```sh
python3 scripts/build_x86_64_dietlibc.py --self-test
python3 scripts/verify_x86_64_runtime_ownership.py --self-test

cmake --build build/x86_64/Debug \
  --target xinim_x86_64_image xinim_x86_64_runtime_ownership_check -j2

ctest --test-dir build/x86_64/Debug \
  -R '^(x86_64_runtime_ownership_self_test|x86_64_runtime_ownership|x86_64_shell_test)$' \
  --output-on-failure
```

The runtime gate and exact QEMU gate are required together. Static ELF
ownership cannot prove shell semantics, and shell output cannot prove which
headers, archive, or loader produced an executable.

## Normative and implementation evidence

- `build/_state/cache/posix/susv4-2018/basedefs/V1_chap01.html`, ISO C reference
- `build/_state/cache/posix/susv4-2018/basedefs/V1_chap02.html`, conformance and feature-test requirements
- `build/_state/cache/posix/susv4-2018/functions/V2_chap02.html`, feature-test macro rules
- `build/_state/cache/posix/susv4-2018/utilities/V3_chap02.html`, shell arithmetic requirements
- mksh R59c `sh.h`, arithmetic type selection
- mksh R59c `Build.sh`, `-L`, `MKSH_BINSHPOSIX`, and install guidance
- mksh R59c `mksh.faq`, POSIX profile and locale guidance
- `docs/external_sources/POSIX_SHELL_SOURCES.md`, archive provenance
