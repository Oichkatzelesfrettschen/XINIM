# mksh Integration

## Active x86_64 lane

The supported x86_64 image uses pinned mksh R59c as its sole shell source.
`scripts/acquire_mksh_source.py` verifies the upstream archive, and
`scripts/prepare_mksh_source.py` applies the hash-bound lexer conformance patch
without modifying the pristine source tree.

The target build is performed by `scripts/build_x86_64_mksh.py` with:

```text
-std=gnu11
-nostdinc
-nostdlib
-static
MKSH_LEGACY_MODE
MKSH_BINSHPOSIX
MKSH_ASSUME_UTF8=0
```

The upstream legacy profile is intentional. SUSv4 Issue 7 requires at least
signed `long` shell arithmetic. Full mksh uses fixed 32-bit arithmetic on
64-bit systems, while the legacy profile uses the C implementation's `long`.
The same resulting executable is staged byte-identically as `/bin/mksh` and
`/bin/sh`. Invocation as `/bin/sh` enables POSIX mode. No `/bin/lksh`,
`/bin/xash`, or other shell is staged in the x86_64 image.

mksh compiles only against the built dietlibc headers and links with explicit
dietlibc `start.o`, `dietlibc.a`, and the compiler runtime. The image ownership
gate binds every staged `/bin` executable to its exact provider build artifact
and rejects another libc, a dynamic loader, shared-library dependencies,
alternate shells, and unclassified binaries.

## Verification

```sh
python3 scripts/verify_x86_64_runtime_ownership.py --self-test

cmake --build build/x86_64/Debug \
  --target xinim_x86_64_image xinim_x86_64_runtime_ownership_check -j2

ctest --test-dir build/x86_64/Debug \
  -R '^(x86_64_runtime_ownership|x86_64_shell_test)$' \
  --output-on-failure
```

The exact shell gate runs in Ring 3 on QEMU `pc-q35-11.1`, `qemu64`, and one
vCPU. Passing it does not by itself establish complete POSIX shell conformance;
the finite shell-language ledger remains the conformance denominator.

## Other lanes

The i486 and hosted shell targets have separate build and runtime histories.
They are not evidence for the supported x86_64 runtime ownership contract.

See `docs/posix/C_LANGUAGE_LIBC_SHELL_ALIGNMENT.md` for the C99 application
contract, GNU C11 vendor dialect, C++23 ownership boundary, dietlibc policy,
and POSIX test-suite admission rules.
