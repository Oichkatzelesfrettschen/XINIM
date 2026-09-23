# Clang-tidy for the compiled i486 lane

The root `.clang-tidy` defines the semantic warning-as-error gate for owned
freestanding C++23 code and its native contracts. Clang 22 reads the actual
commands in `build/i486/Debug/compile_commands.json`. The i486 analysis
workflow selects changed C++ translation units and compiled consumers of
changed headers across `src/`, `include/`, `test/`, and `userland/`. Compiler
warnings independently remain errors.

## Applicable semantic coverage

The profile enables `bugprone-*`, `cert-*`, `clang-analyzer-*`,
`performance-*`, and `portability-*`, subject to the seven named exceptions
below. Named `cppcoreguidelines` checks cover initialization, narrowing,
const removal, unsafe downcasts, slicing, special members, virtual destructors,
exception contracts, forwarding, captures, global initialization, and coroutine
lock lifetime. Named `misc` checks cover expression and assignment defects,
header definitions and cycles, suspicious identifiers, object ownership,
exception handling, unused declarations, and coroutine RAII. Brace requirements
remain enabled. `WarningsAsErrors: '*'` applies to every enabled diagnostic.

The special-member option permits missing move functions when copy functions
are deleted. The static-only PCI interface deletes construction, destruction,
and copying; deleted copy operations also prevent implicit move generation.
The option leaves ordinary movable/copyable class contracts under inspection.

The nested `test/.clang-tidy` inherits the root profile and adds
`modernize-use-std-print` for hosted C++23 contracts. Freestanding kernel output
uses its console and serial APIs. Library ownership, language mode, and linked
symbols remain compiler and runtime-ownership contracts.

## Named exceptions and their boundaries

| Check | Boundary and review obligation |
| --- | --- |
| `bugprone-easily-swappable-parameters` | Adjacent same-typed parameters are an interface-design advisory. Findings include private helpers as well as fixed hardware/syscall interfaces. ABI signatures retain required order; owned private interfaces require call-site and test review when changed. The exclusion establishes neither safe argument order nor a fixed-ABI justification for every function. |
| `bugprone-suspicious-include` | Native ATA and ext2 contracts include their implementation `.cpp` to reach private seams. Ordinary production includes retain header ownership; an additional implementation include requires an explicit single-translation-unit test contract. |
| `cert-err52-cpp` | Native signal dispatch tests use `setjmp`/`longjmp` to intercept a noreturn dispatcher. Skipped automatic frames must remain trivially destructible. Adding a nontrivial destructor across that jump invalidates the seam. Kernel exception support remains disabled. |
| `clang-analyzer-core.FixedAddressDereference` | The VGA console writes the platform framebuffer at physical `0xB8000`. Any additional constant-address dereference requires its device mapping contract; the exclusion grants neither user-pointer validity nor general memory safety. |
| `clang-analyzer-optin.performance.Padding` | Layout optimization requires measured object counts, alignment, and ABI offsets. Findings include private `OpenFile`, `RamFile`, `Process`, and `SupervisedService` records, alongside ABI records. Private layout savings remain a separate footprint investigation; an ABI explanation alone cannot dismiss them. |
| `performance-enum-size` | Explicit underlying widths carry declared kernel and ABI representation policy. Changing an enum width requires consumer, layout, and binary-contract review. |
| `portability-avoid-pragma-once` | The supported Clang toolchain implements the repository's `#pragma once` convention. A compiler-portability expansion must revisit that assumption. |

`performance-no-int-to-ptr` remains enabled. Required physical-address
conversions carry individual `NOLINTNEXTLINE` annotations naming the Multiboot,
DMA, or MMIO boundary. The bootfs ioctl adapter receives a pointer already
translated by the syscall layer and carries the same precise annotation for
its integer ABI round trip. A syscall register alone does not meet that
contract: user addresses require range translation before dereference.

`cppcoreguidelines-pro-type-const-cast` also remains enabled. The boot module
and executable adapters retain individual annotations where `FileRecord` stores
read-only bytes in a legacy mutable field with `read_only=true`. Mutation paths
must reject those records. Internal bootfs lookups instead return genuinely
mutable records; the public lookup preserves its const view. Redundant casts
in ext2 and ARP handling were removed.

## Separate hygiene work

The root profile selects semantic checks by name instead of enabling every
style family. Include cleanup, local const qualification, internal linkage,
anonymous namespaces, naming, magic numbers, array modernization, and broad
formatting remain separately reviewable source-hygiene work. These checks can
apply to freestanding code; hosted-library availability explains only checks
that actually require an unavailable facility.

The broad profile produced 5,008 distinct diagnostics at `7e6d3e27`, retained
in `build/i486/Debug/evidence/i486-hosted-tidy-7e6d3e27.log`. The count
uses canonical path, line, column, and the complete diagnostic check-name list;
counting aliases independently changes the denominator. That historical count
records outstanding advice and defects together. A passing semantic profile
establishes closure only for its enabled checks on the selected compile inputs.

## Replay and review

Run each distinct owned C++ source from the compile database with Clang 22:

```sh
clang-tidy -p build/i486/Debug \
  --header-filter='<checkout>/(src|include|test|userland)/' \
  <compiled-source.cpp>
```

The kernel and native-contract scan passes all 51 distinct compiled C++
sources. Its source list, return codes, and per-source logs live under
`build/i486/Debug/evidence/freestanding-named-profile-final/`. A separate
96-source owned userspace scan lives under
`build/i486/Debug/evidence/freestanding-userland-profile/`: 18 sources pass,
including `userland/tests/syscall_guard_i486.cpp`, and 78 report diagnostics.
The 1,016 distinct diagnostics comprise 900 brace findings, 112 other owned
findings, and four cycles in generated vendor dietlibc headers. Analyzer
findings require source-path adjudication; that baseline establishes neither
confirmed defects for every report nor whole-userspace closure. The 51-source
pass establishes the kernel/native-contract boundary, and the targeted guest
regression adds one passing userspace source. Historical utility cleanup
remains a separate repair scope.
Changed headers require every compiled consumer. Native contracts and the exact
QEMU i486 guest gates remain necessary because static analysis cannot establish
boot, privilege transitions, device behavior, or runtime pointer translation.

Apply formatter edits only to reviewed diagnostic ranges. Clang 22
`InsertBraces: true` and `clang-apply-replacements --format --style=file` can
format those ranges; semantic substitutions require independent inspection.
For example, a const-correctness replacement can mark an inline-assembly output
operand const, violating the writable operand contract. Keep assembly outputs
mutable and verify initialization with the real compiler. Whole-file or
whole-tree formatting, unchecked fix exports, and broad diagnostic suppression
are outside the semantic repair workflow.
