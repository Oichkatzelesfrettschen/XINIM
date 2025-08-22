# Project Status

This document tracks repository maintenance and cleanup activities.

## 2025-06-23

- Added `tools/run_qemu.sh` to boot disk images under QEMU.  
- Updated `.gitignore` to exclude:
  - `**/build*/`
  - `build/`
  - `builds/`
  - `*/CMakeFiles/`  
- Ensured no build artifacts are committed by verifying:
  - `find . -name 'build*'` yields no results  
  - `find . -name 'CMakeFiles'` yields no results  
- Confirmed all build artefacts are cleanly excluded from the repository.
## 2025-07-30
- Created `tools/header_sanity_check.sh` with robust quoting for include paths.
- Added explicit `build*/` pattern to `.gitignore` for clarity.
- Verified absence of build artefacts and recorded cleanup activities.

## 2025-07-31
- Performed clang-based build to verify toolchain configuration.
- No `build*/`, `builds/`, or `*/CMakeFiles/` directories remained after cleanup.
- Added redundant ignore patterns to `.gitignore` as an extra safeguard.

## 2025-08-06
- Expanded Doxygen coverage for paging interfaces and miscellaneous file-system routines.
- Generated API reference with Doxygen and attempted Sphinx build (errors remain in legacy `.rst` files).
## 2025-08-22
- Documented kernel system services with granular Doxygen comments.
- Regenerated API reference via Doxygen and verified Sphinx+Breathe integration.
- Modernized `kernel/xt_wini.cpp` with `enum class` and `std::array` and expanded Doxygen coverage.
