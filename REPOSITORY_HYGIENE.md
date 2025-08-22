# Repository Hygiene

This document codifies cleanup routines, artifact management policies, and the rationale for the project's `.gitignore` entries. Maintaining a lean repository accelerates cloning, reduces build noise, and ensures that contributors share a consistent workspace.

## Cleanup Routines
- **Primary script**: `./tools/clean.sh` purges build directories, temporary files, and generated documentation.
- **CMake clean**: `cmake --build build --target clean` removes artifacts from an existing CMake tree.
- **Full reset**: `git clean -fdx` (use with care) deletes all untracked files, restoring the repository to a pristine state.
- **Garbage collection**: `git gc --prune=now --aggressive` compacts history and eliminates unreachable objects.

Regular invocation of these routines keeps local checkouts deterministic and prevents accidental commits of transient data.

## Artifact Policies
- **Never commit build outputs** such as `build*/`, `builds/`, or `*/CMakeFiles/`.
- **Exclude generated documentation** from Doxygen or Sphinx (`docs/doxygen/`, `docs/sphinx/html/`).
- **Omit logs and temporary files** (`*.log`, `*.tmp`, `output.txt`).
- **Disallow editor and tool caches** (`__pycache__/`, `.cache/`, `*.swp`).
- **Large binaries** belong in release archives or external storage, not in Git history.

## `.gitignore` Rationale
The `.gitignore` file enforces the artifact policies above. Key sections include:
1. **Build and tool-chain artifacts** – ignore `build/`, `build*/`, `builds/`, and `*/CMakeFiles/` to prevent accidental inclusion of compiled objects.
2. **Compiler outputs** – patterns such as `*.o`, `*.a`, and `*.elf` exclude machine-generated binaries.
3. **Documentation generators** – `docs/doxygen/` and `docs/sphinx/` build products are ignored to keep the repository source-only.
4. **Logs and temporaries** – generic rules like `*.log` and `*.tmp` block noisy files produced during debugging.
5. **Caches and editor backups** – safeguards against committing personal or tool-specific state.
6. **Redundant guard lines** reiterate critical patterns (`build*/`, `builds/`, `*/CMakeFiles/`) to reinforce exclusion of build trees.

## Historical Cleanup Summary
- **Initial cleanup** – ensured no build artifacts were tracked, added ignore rules, and removed duplicate files.
- **July 2025** – confirmed the repository remained artifact-free after test builds and added `tools/header_sanity_check.sh`.
- **August 2025** – verified `.gitignore` coverage, revalidated via CMake and tests, and confirmed clean builds.
- **31 July 2025** – full clang-based build confirmed absence of stray directories; ignore patterns were reiterated for safety.

These events established the current hygiene baseline and motivate continued vigilance.

## Size-Optimization Practices
- Favor source code and textual assets; store large datasets or binaries externally.
- Compress ancillary resources before archival or release packaging.
- Periodically run `git gc --prune=now` to remove unreachable objects.
- Review repository size with `du -sh` to detect unexpected growth.

Adhering to these practices keeps the project lightweight and contributor-friendly.
