# dietlibc++ Options for XINIM

Date: 2026-03-08

## Goal

Support C++ userland and selected kernel-adjacent utilities while keeping the
dietlibc footprint philosophy (small binary size, predictable startup, low RAM).

## Option A: "Dietlibc + libc++abi-lite" (Recommended)

Approach:
- Keep `dietlibc` as libc.
- Build a minimal C++ ABI/runtime subset:
  - `new/delete`
  - RTTI and exception stubs configurable per target
  - thread-local storage hooks as needed
- Link against a pruned libc++ surface for userland tools that need STL subsets.

Pros:
- Strong size control.
- Incremental adoption path.
- Works with current lane split (i486 and x86_64).

Cons:
- Requires careful ABI boundary tests.
- Exceptions and unwinding are the hardest part.

## Option B: "No Full STL in Base, Curated C++ Runtime"

Approach:
- Keep C as baseline for most userland.
- Provide a compact C++ runtime plus selected headers/containers only.
- Gate advanced STL features behind optional profile.

Pros:
- Best control over footprint and determinism.
- Lower integration risk for kernel-adjacent binaries.

Cons:
- Developer ergonomics lower for modern C++ application code.
- Requires explicit library policy and documentation discipline.

## Option C: "Dual Runtime Profiles" (Small + Full)

Approach:
- Profile 1: dietlibc + minimal c++ runtime (`tiny`).
- Profile 2: dietlibc + wider libc++ subset (`full`).
- Choose by target/lane in CMake.

Pros:
- Flexible for benchmarking and staged rollout.
- Lets us preserve minimal default while enabling richer tools.

Cons:
- More CI matrix complexity.
- Requires strict symbol/version policy to avoid drift.

## Recommendation

Use Option A immediately, with Option C profile gating after baseline stability.

Practical rollout:
1. Implement ABI-core (`operator new/delete`, termination handlers).
2. Add "no-exceptions/no-rtti" and "exceptions/rtti" build toggles.
3. Validate with i486 and x86_64 smoke binaries.
4. Introduce selected libc++ containers only after ABI and startup-cost metrics pass.

