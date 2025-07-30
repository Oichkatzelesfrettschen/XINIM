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