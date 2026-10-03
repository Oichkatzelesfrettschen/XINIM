# Build Profiles

This document outlines the configurable build profiles and optional toolchain
features available in the XINIM project. Profiles are selected via the standard
CMake `CMAKE_BUILD_TYPE` cache variable.

## Profiles

| Profile      | Flags                | Intended Use           |
|--------------|---------------------|------------------------|
| `Debug`      | `-O0 -g`            | Developer debugging    |
| `Performance`| `-O3 -g -DNDEBUG`   | Instrumented benchmarking |
| `Release`    | `-O3 -DNDEBUG`      | Production binaries    |

Select a profile when configuring CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Performance
```

## Optional Tooling

Two families of diagnostic tooling and one optimisation pass can be toggled
independently:

- **AddressSanitizer** – enabled with `-DENABLE_ASAN=ON`.
- **UndefinedBehaviorSanitizer** – enabled with `-DENABLE_UBSAN=ON`.
- **Link Time Optimisation** – enabled with `-DENABLE_LTO=ON`.

Sanitizers are only injected for `Debug` and `Performance` builds, while LTO is
applied for `Performance` and `Release` profiles.

Example enabling sanitizers in a debug configuration:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
```

Once configured, build targets as normal:

```sh
cmake --build build
```

## Notes

These profiles intentionally avoid global compiler flags. All options propagate
through the `xinim_common` interface target and can be overridden on a per-target
basis where necessary.

