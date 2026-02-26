# Long-Term Roadmap

NOTE: The authoritative roadmap is `docs/analysis/ROADMAP_CONSOLIDATED.md`.
This document provides the high-level direction. The 74-task Hypergranular
Technical Debt Resolution was completed 2026-02-26 (see
`docs/IMPLEMENTATION_ROADMAP_TRACKER.md` for summary).

This document outlines the high level plan for progressing the project toward a modern C++ implementation that targets both QEMU and WebAssembly.

## 1. Stable x86_64 Build (In Progress)

1. ~~Ensure the entire codebase builds cleanly on modern x86_64 toolchains.~~ DONE (Clang 21, zero warnings)
2. ~~Provide CMake + Conan scripts that default to C++23.~~ DONE (CMakePresets.json, conan profile)
3. Validate the kernel boots successfully in QEMU. (Infrastructure done; boot test registered)
4. Establish a repeatable CI pipeline for continuous testing. (CTest with 22 tests; CI runner pending)

## 2. Modernization and POSIX Compliance

1. Incrementally replace legacy drivers with permissive, non-GNU alternatives.
2. Implement the required subset of POSIX.1-2008 for compatibility with modern tooling.
3. Refactor code to fully embrace C++23 idioms and eliminate unsafe constructs.
4. Maintain cross‑platform support for both 32‑bit and 64‑bit builds.
5. Replace remaining C arrays with `std::array` and `constexpr` initialization for compile-time safety.

## 3. Toward WebAssembly

1. Once the QEMU build is stable, prototype a WASM layer using wasi-sdk and Emscripten.
2. Identify pieces that can run in a pure WASI environment and those that require browser interaction.
3. Package the system into a single executable WASM module when practical.
4. Allow the same codebase to run under QEMU or a browser with minimal changes.

## 4. Branch 2025 and Beyond

1. Continue modernizing the codebase toward C++23 and C++27 where supported.
2. Explore memory-safe extensions without sacrificing performance.
3. Keep the tooling flexible so users can target everything from embedded boards to browsers.
4. Document each milestone so that the community can track progress and participate in design discussions.

## 5. Ongoing Debate

While there are many approaches for bringing MINIX into the browser, the current focus remains on solidifying the x86_64 QEMU build. WASM support will be explored in parallel once the foundation is stable.
