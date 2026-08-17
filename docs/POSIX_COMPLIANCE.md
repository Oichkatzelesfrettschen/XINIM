# SUSv4 Issue 7 Shell and Utilities Closure

This document defines a bounded conformance program. It is not a claim that
XINIM is fully conforming.

## Standards denominator

The implementation target is IEEE Std 1003.1-2017 and The Open Group Base
Specifications Issue 7, 2018 edition: POSIX.1-2008 with its Technical
Corrigenda. The active requirements are the modern unified Issue 7 Shell and
Utilities requirements derived from SUSv4. Historical POSIX.2/POSIXv2
requirements are not a parallel denominator and cannot close an Issue 7 row.

The former `posix2_*` ledger and verifier names are retired. The active
denominator is named `posix_issue7_utility_ledger`; the pinned SUSv4 archive is
the only standards source for its rows.

The denominator is derived from the official `susv4-2018.tgz` personal-use
HTML archive: 160 standalone utility pages plus the 15 special built-ins
defined in the Shell Command Language chapter. The official archive, its
acquisition contract, and its copyright-constrained local retention are
documented in `docs/external_sources/POSIX_SHELL_SOURCES.md`. Linux man-pages
may assist implementation research but are not the canonical specification.

The canonical state ledgers are:

- `docs/posix/posix_issue7_utility_ledger.tsv`
- `docs/posix/posix_shell_language_ledger.tsv`
- `docs/posix/posix_shell_frontier_ledger.tsv`
- `docs/posix/posix_shell_frontier_requirements.tsv`
- `docs/posix/posix_issue7_recursive_frontier_ledger.tsv`
- `docs/posix/posix_issue7_recursive_frontier_requirements.tsv`
- `docs/posix/posix_shell_grammar_requirements.tsv`
- `docs/posix/printf_requirements.tsv`
- `docs/posix/posix_system_prerequisites.tsv`
- `docs/posix/posix_base_definitions_ledger.tsv`
- `docs/posix/posix_base_definitions_clause_ledger.tsv`
- `docs/posix/posix_system_interfaces_ledger.tsv`
- `docs/posix/posix_system_interfaces_clause_ledger.tsv`

Each ledger records its row count and SHA-256 key-set hash. Every declared row
is exactly one of `open` or `closed`. A closed row requires real test
witnesses; an open row requires an unsatisfied prerequisite and an executable
next action. The verifiers reject duplicate, missing, reordered, or unexpected
keys, source-heading drift, partition errors, false closed rows, and open rows
without next actions.

Current state: 173 open utility rows and 2 closed utility rows, plus 43 open
shell-language rows and 27 closed shell-language rows. All seven system
prerequisites are closed by exact Q35 Ring 3 matrices. The socket prerequisite
is closed for the declared AF_INET loopback datagram scope, including creation,
bind, connect, readiness, send/receive, descriptor duplication, fork/exec
close-on-exec, shutdown, teardown, and error paths; TCP/IP, AF_UNIX, stream
listen/accept, ancillary messages, and socket pairs remain outside that closed
scope. The expanded standards denominator contains
95 Base Definitions parents and 1,483 clause rows, plus 1,195 System
Interfaces parents and 15,740 clause rows; all 18,513 expanded rows remain
open. The
token-recognition subledger has 17 closed rows and no open rows. The
shell-grammar subledger has 125 closed rows and no open rows. The exact Q35
Ring 3 gate closes all 3 initial lexical classifications, all 11 context rules,
and all 111 production alternatives, including the simple-command and
redirection spine. The selected dietlibc 0.35 still advertises `_POSIX_VERSION`
as `199506L`; selected newer interfaces do not raise that achieved floor.
Existing source files and successful smoke invocations are implementation
evidence, but they do not by themselves close a complete specified behavior.

## Recursive shell-language frontier

The next canonical frontier is the first 31 open parent rows in source order:
`tag_18_01` through `tag_18_09_04_03`, excluding parents already closed by the
shell-language ledger. `scripts/verify_posix_shell_frontier.py` derives their
source sections from the pinned Chapter 2 HTML, records the source hash and
ordered assertion hash, and emits 724 recursive assertion rows. Each parent
also has one exact Q35 Ring 3 probe case in
`test/boot/x86_64_shell_test.py`; those probes establish executable coverage
but are not parent-closure witnesses.

All 31 parents and all 724 recursive assertions remain open. A parent cannot
close while any assertion beneath it is open. Each assertion names its
mechanism-specific implementation requirement, exact missing witness, next
action, and mutation falsifier. The verifier rejects source drift, row-set
drift, missing Q35 probe IDs, false parent closure, and mutations that remove
an assertion or its dependency evidence. This frontier changes the evidence
surface from generic parent TODOs to an executable finite queue; it does not
change the bounded Issue 7 count, which remains 29/245.

The following queue slice is retained separately in
`docs/posix/posix_issue7_recursive_frontier_ledger.tsv` and
`docs/posix/posix_issue7_recursive_frontier_requirements.tsv`. It selects the
next 31 open parent rows after that first shell frontier: 12 shell-language
parents and the first 19 utility parents, with 2,644 source-derived recursive
assertions. `scripts/verify_posix_issue7_recursive_frontier.py` derives the
slice from the canonical open queue, binds every parent to one exact Q35 Ring 3
probe ID, and rejects source, key-order, witness, dependency, and mutation
drift. All 31 parents and all 2,644 recursive assertions remain open; these
probes are evidence for the active queue, not parent-closure transitions.

## Expanded SUSv4 denominator

The complete applicable Base Definitions and System Interfaces source surface
is derived from the pinned `susv4-2018.tgz` archive and retained in four
source-bound ledgers. The archive identity is SHA-256
`ab6636bca53c7d71d33d2c5149ede574d598fe6ec97fa8b08e0459ef7bcfc104`.

The finite denominator is:

| Volume | Parent rows | Clause rows | Total rows |
| --- | ---: | ---: | ---: |
| Base Definitions | 95 | 1,483 | 1,578 |
| System Interfaces | 1,195 | 15,740 | 16,935 |
| Expanded denominator | 1,290 | 17,223 | 18,513 |

Parent and clause keys, source bytes, titles, and state partitions are checked
against the archive by `scripts/verify_posix_base_system_ledgers.py`. These
ledgers establish a finite, versioned denominator; they do not imply that any
row is implemented or closed.

The prerequisite frontier is deliberately ordered before row closure. The
seven required capabilities are libc, filesystem, processes, signals,
terminals, IPC, and sockets. A bounded Issue 7 claim requires all seven
prerequisites to be closed with exact Q35 Ring 3 evidence before the 245 shell
and utility parent rows can close.

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

The prerequisite frontier closes first. No bounded shell or utility closure
claim is admissible until libc, filesystem, processes, signals, terminals, IPC,
and sockets are all closed in `posix_system_prerequisites.tsv` with exact Q35
Ring 3 witnesses. Closing the socket prerequisite is bounded to the AF_INET
datagram contract recorded in that row and does not imply full networking
conformance.

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
           xinim_posix_issue7_utility_ledger_check \
           xinim_posix_printf_requirements_check \
           xinim_posix_true_requirements_check \
           xinim_posix_shell_language_ledger_check \
           xinim_posix_shell_frontier_check \
           xinim_posix_shell_grammar_requirements_check \
           xinim_posix_token_recognition_requirements_check \
           xinim_posix_base_system_ledgers_check \
           xinim_posix_system_prerequisites_check \
           xinim_posix_conformance_status_check

ctest --test-dir build/x86_64/Debug \
  -R '^(posix_issue7_archive|posix_issue7_archive_self_test|posix_issue7_utility_ledger|posix_issue7_utility_ledger_self_test|posix_printf_requirements|posix_printf_requirements_self_test|posix_true_requirements|posix_true_requirements_self_test|posix_shell_language_ledger|posix_shell_language_ledger_self_test|posix_shell_frontier|posix_shell_frontier_self_test|posix_shell_grammar_requirements|posix_shell_grammar_requirements_self_test|posix_token_recognition_requirements|posix_token_recognition_requirements_self_test|posix_base_system_ledgers|posix_base_system_ledgers_self_test|posix_system_prerequisites|posix_system_prerequisites_self_test|posix_conformance_closure|posix_conformance_closure_self_test|dietlibc_setjmp_contract|dietlibc_setjmp_contract_self_test|mksh_patch_pipeline_self_test|mksh_command_substitution_boundary|test_posix_printf)$' \
  --output-on-failure

ctest --test-dir build/x86_64/Debug \
  -R '^x86_64_shell_test$' --output-on-failure
```

Declare bounded Issue 7 Shell and Utilities closure only at 245/245 parent
rows: all 175 utility rows and all 70 shell-language rows must be closed, all
seven system prerequisites must be closed, both state partitions must remain
disjoint and exhaustive, and the exact Q35 Ring 3 regression gate must pass.
The explicit `xinim_posix_issue7_closure_gate` remains failing until those
conditions hold.

Declare full SUSv4/POSIX conformance only after the expanded Base Definitions
and System Interfaces denominator also closes: all 1,290 parent rows and
17,223 clause rows, 18,513 rows in total, with their exact Q35 witnesses and
dependency checks. The explicit `xinim_posix_full_conformance_gate` remains
failing until that expanded denominator closes.
