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

