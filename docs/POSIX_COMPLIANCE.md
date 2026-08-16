# POSIX.2 Shell and Utilities Closure

This document defines a bounded conformance program. It is not a claim that
XINIM is fully conforming.

## Standards denominator

The implementation target is IEEE Std 1003.1-2017 and The Open Group Base
Specifications Issue 7, 2018 edition: POSIX.1-2008 with its Technical
Corrigenda. POSIX.2 is the historical name for the Shell and Utilities volume;
it is not a separate 1992 target in this program.

The denominator is derived from the official `susv4-2018.tgz` personal-use
HTML archive: 160 standalone utility pages plus the 15 special built-ins
defined in the Shell Command Language chapter. The official archive, its
acquisition contract, and its copyright-constrained local retention are
documented in `docs/external_sources/POSIX_SHELL_SOURCES.md`. Linux man-pages
may assist implementation research but are not the canonical specification.

The canonical state ledgers are:

- `docs/posix/posix2_utility_ledger.tsv`
- `docs/posix/posix_shell_language_ledger.tsv`
- `docs/posix/posix_shell_grammar_requirements.tsv`
- `docs/posix/printf_requirements.tsv`

Each ledger records its row count and SHA-256 key-set hash. Every declared row
is exactly one of `open` or `closed`. A closed row requires real test
witnesses; an open row requires an unsatisfied prerequisite and an executable
next action. The verifiers reject duplicate, missing, reordered, or unexpected
keys, source-heading drift, partition errors, false closed rows, and open rows
without next actions.

Current state: 174 open utility rows and 1 closed utility row, plus 63 open
shell-language rows and 7 closed shell-language rows. The token-recognition
subledger has 17 closed rows and no open rows. The shell-grammar subledger has
125 closed rows and no open rows. The exact Q35 Ring 3 gate closes all 3
initial lexical classifications, all 11 context rules, and all 111 production
alternatives, including the simple-command and redirection spine. The selected
dietlibc 0.35 still advertises `_POSIX_VERSION` as `199506L`; selected newer
interfaces do not raise that achieved floor. Existing source files and
successful smoke invocations are implementation evidence, but they do not by
themselves close a complete specified behavior.

## Verified x86_64 shell platform

The versioned platform gate is:

```text
qemu-system-x86_64 11.1.0
-machine pc-q35-11.1
-cpu qemu64
-smp 1
Limine x86_64 ISO
```

The image boots one static mksh R59c source build as Ring 3 PID 1 at
`/bin/sh`. The build uses upstream's legacy POSIX profile so shell arithmetic
uses the x86_64 C implementation's `long`; the same binary is installed as
`/bin/mksh`. No lksh-named or xash shell is staged. Invocation as `/bin/sh`,
the startup file, the `C` locale, and byte-oriented string handling select the
POSIX behavior. The exact QEMU test proves the sole-shell layout, the legacy
POSIX profile, the full 64-bit signed-`long` value range, and disabled brace
and UTF-8 modes survive guest execution. It also proves startup-file
execution, process identity, quoting, parameter expansion, command and
arithmetic substitution, functions, loops, case, pipelines, redirections,
asynchronous lists, wait, interactive monitor mode, foreground jobs, signals,
TTY process groups, and repeated process and pipe lifecycle pressure.

The kernel and libc path also has focused evidence for mmap, stat, directory,
fcntl, select, signal, time, symlink, ownership, locking, and mutable `/tmp`
operations. Bootfs tests cover directory-tree rename, nonempty-directory
rejection, open-unlinked lifetime, reusable storage, `.` and `..`
normalization, root clamping, and creation through symlinked parents.

These witnesses establish a capable shell platform. They do not prove every
shell-language rule, every utility contract, or complete POSIX system-interface
conformance.

The pinned mksh R59c source remains pristine under `mksh-R59c/source`. A
hash-bound repository patch produces `mksh-R59c/patched-source` atomically;
the sole shell builds only from that verified tree. The patch captures the
physical command-substitution source while alias lookup is suppressed, locates
the physical closing parenthesis through recursive token recognition, and then
parses the bounded source with normal alias behavior. Host tests prove identical
captured bytes under benign and hostile alias tables, zero recognition-time
alias lookups, normal bounded-parse alias lookup, raw backslash-newline capture,
and replay exclusion. The exact Q35 Ring 3 test proves the benign alias executes
and the hostile alias returns status 1 with the expected syntax diagnostic
instead of executing text beyond the physical delimiter. A separate contract
test requires dietlibc to declare `__sigsetjmp` with the compiler-visible
`returns_twice` attribute and proves that removing the attribute fails the gate.
The broader command-substitution rows remain open for their other delegated
requirements. The finite token-recognition subledger is closed.

Noninteractive line input uses mksh's dynamically grown `XString` path rather
than a fixed line buffer. Two exact Q35 witnesses construct 32768-byte physical
lines through fifteen string doublings, then execute one as ordinary shell
input and one as a here-document body. The kernel exec path accepts their
unchanged `/bin/printf` invocation through one 64953-byte aggregate argv and
environment budget derived from the 64 KiB initial stack; null terminators,
maximum pointer tables, auxv words, and alignment are included. Focused tests
cover the exact budget, one-byte rejection, combined argv and environment
accounting, unterminated vectors, arithmetic overflow, and the 32 KiB argument.
This closes the fixed-line-cap and two-mode requirement; it does not close the
independent grammar-categorization row.

The external `/bin/printf` now uses a freestanding C++23 formatting core. Host
and exact Q35 Ring 3 tests cover whole-format reuse, the partial final cycle,
missing-operand defaults, the mandatory non-floating conversions, `%b`, C
integer constant syntax, flags, widths, precisions, continued numeric-error
processing, and write-failure propagation. A freestanding C++23 Ring 3 byte
oracle verifies redirected output containing NUL, alert, backspace, form-feed,
newline, carriage-return, tab, vertical-tab, backslash, and octal bytes without
decoding the result through the serial terminal. Exact tests also cover signed
and unsigned overflow boundaries, a missing-format diagnostic, proof that
standard input remains unread, and a closed-standard-output failure.

The generated `printf` subledger contains 67 source-bound rows derived from the
pinned `utilities/printf.html` and `basedefs/V1_chap05.html` texts. All 53
mandatory rows are closed by exact Q35 witnesses. The other 14 rows remain
visible as optional, conditional, unspecified, or no-requirement source scope.
The admitted locale set is `C` and `POSIX`; exact tests cover LANG fallback,
LC_ALL precedence over LC_CTYPE, byte-oriented character handling, and
LC_MESSAGES diagnostics. A dedicated oracle executes `/bin/printf` against a
blocking pipe, observes its output, delivers SIGTERM, and verifies default
signal termination. The delta glyph is conditional on a codeset that contains
it and is absent from the admitted codeset. The default environmental
consequences of errors are explicitly unspecified by Utility Description
Defaults. The parent `printf` utility row is therefore closed; this does not
close any other utility or the full POSIX denominator.

## Row closure gate

A utility row can move to `closed` only when all of the following are true:

1. The utility or required shell builtin is present in the x86_64 image and
   executes in Ring 3 without relying on xash implementation behavior.
2. Project-owned implementation touched for the row is C++23. Any retained
   C23 header or boundary has a demonstrated external C or assembly consumer.
3. Required operands, options, input forms, stdout, stderr, environment
   effects, exit statuses, and specified error paths have exact tests derived
   from the selected manual page.
4. Filesystem, process, signal, locale, terminal, or IPC prerequisites used by
   the utility have direct kernel and ABI tests.
5. Repeated execution and resource-pressure tests prove descriptor, process,
   memory, and storage lifecycle closure.
6. The ledger row names the repo-relative test witnesses, and the verifier
   accepts the transition.

The `sh` row additionally requires all 70 rows in the shell-language ledger to
close. The source-derived matrix covers the normative grammar, tokenization,
expansions, quoting, redirections, functions, compound commands, execution
environment, traps, jobs, and exit-status rules. The grammar row must retain
all 111 alternatives across the 47 named productions, the 3 initial lexical
classifications, and context rules 1 through 9, including 6a, 6b, 7a, and 7b.
Rows containing UP or XSI option fragments require an explicit selected profile
and profile-specific witnesses. Passing the present interactive shell gate is
necessary but not sufficient.

## Verification

```sh
cmake --build build/x86_64/Debug \
  --target xinim_posix_issue7_archive_check \
           xinim_posix2_utility_ledger_check \
           xinim_posix_printf_requirements_check \
           xinim_posix_shell_language_ledger_check \
           xinim_posix_shell_grammar_requirements_check \
           xinim_posix_token_recognition_requirements_check

ctest --test-dir build/x86_64/Debug \
  -R '^(posix_issue7_archive|posix_issue7_archive_self_test|posix2_utility_ledger|posix2_utility_ledger_self_test|posix_printf_requirements|posix_printf_requirements_self_test|posix_shell_language_ledger|posix_shell_language_ledger_self_test|posix_shell_grammar_requirements|posix_shell_grammar_requirements_self_test|posix_token_recognition_requirements|posix_token_recognition_requirements_self_test|dietlibc_setjmp_contract|dietlibc_setjmp_contract_self_test|mksh_patch_pipeline_self_test|mksh_command_substitution_boundary|test_posix_printf)$' \
  --output-on-failure

ctest --test-dir build/x86_64/Debug \
  -R '^x86_64_shell_test$' --output-on-failure
```

Declare the selected POSIX.2 shell and utility denominator closed only when all
175 utility rows and all 70 shell-language rows are closed, both open and
closed partitions remain disjoint and exhaustive, and the exact Q35 Ring 3
regression gate passes. Do not promote that bounded result into broader
POSIX.1 or Issue 8 system-interface conformance without a separately versioned
denominator and test suite.
