# Clang-tidy for the compiled i486 lane

The root `.clang-tidy` is the warning-as-error profile for project-owned
freestanding C++23 kernel code and its compiled native contracts. The i486
analysis workflow selects changed C++ translation units and compiled consumers
of changed headers from the build's compile database. Clang 22 analyzes those
exact commands. Compiler warnings remain errors independently of this profile.

The profile retains `bugprone-*`, `cert-*`, and `clang-analyzer-*` defect
checks, redundant-expression detection, and brace requirements. Five checks
inside those families are excluded:

- `bugprone-easily-swappable-parameters` judges adjacent same-typed parameters
  by name. Hardware, syscall, and filesystem interfaces carry fixed ABI and
  on-disk argument order; renaming parameters cannot establish a defect.
- `bugprone-suspicious-include` rejects native contract tests that include a
  kernel implementation in one translation unit to exercise private device
  and filesystem logic. The include is their explicit test seam.
- `clang-analyzer-optin.performance.Padding` proposes record layouts without
  knowing packed disk, boot, or assembly layouts. ABI layout belongs to the
  assembly contract and binary fixture gates.
- `clang-analyzer-core.FixedAddressDereference` flags the intentional VGA
  framebuffer at physical address `0xB8000`. The console driver owns that
  address and the guest boot tests exercise its output path.
- `cert-err52-cpp` proposes C++ exceptions in signal dispatch tests that use
  `setjmp` and `longjmp` to model signal control flow. The freestanding kernel
  cannot use exceptions and the native signal contract requires that seam.

The former all-family profile produced 5,008 distinct hosted diagnostics at
`7e6d3e27`. The broad result remains audit evidence in
`build/i486/Debug/evidence/i486-hosted-tidy-7e6d3e27.log`; the profile does
not claim those diagnostics were all source defects. The excluded families
assume hosted `std::array` or `std::print`, reject C and assembly arrays,
physical address casts, register macros, fixed public boot records, indexed
buffers, and `#pragma once`, or encode formatting and naming preferences.
The root naming rule also conflicted with the owned `kCamelCase` constexpr
convention while uppercase ABI constants remain in use. The compiler, runtime
ownership verifier, native ABI tests, and QEMU boot tests police those
boundaries. A change to any ABI record or physical-memory access still needs
its owning contract test.

Run the compiled-source check with Clang 22:

```sh
clang-tidy -p build/i486/Debug \
  --header-filter='<checkout>/(src|include|test)/' \
  <compiled-source.cpp>
```

The CI workflow uses `clang-tidy-22` and `WarningsAsErrors: '*'`. A formatter
can add braces at reviewed diagnostic ranges. The 326 new brace findings in
the i486 compile database were repaired using Clang 22 `InsertBraces: true`
and only the diagnostic line ranges; all 51 compiled owned C++ translation
units then passed the profile. `clang-apply-replacements --format --style=file`
formats reviewed replacement ranges; it cannot decide ABI, arithmetic width,
or error-handling semantics. Do not run an unfiltered whole-tree fix.
