# Project Status

## Repository Cleanup
- Verified that no build artifacts (`build*/`, `builds/`, `*/CMakeFiles/`) remain in version control.
- Added new ignore rules to `.gitignore` for these directories.
- Removed duplicate file `tools/README 2.md`.

All build outputs should reside in ignored directories only.

## Repository Cleanup (Aug 2025)

- Removed residual build directories (none were tracked).
- Added ignore patterns for `build*/`, `builds/`, and `*/CMakeFiles/` to `.gitignore`.
- Verified workspace with `cmake` and `ctest`; no build artifacts remain under version control.



## Repository Cleanup (July 2025)
- Confirmed no stray build directories were checked in after test builds.
- Added `tools/header_sanity_check.sh` with safe quoting and updated executable bit.
- Re-ran `cmake` and `ctest` to verify workspace builds cleanly; some unit tests fail as expected.
