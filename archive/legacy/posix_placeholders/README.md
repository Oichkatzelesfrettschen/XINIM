# POSIX Placeholder Archive

Date: 2026-03-08

This directory quarantines historical POSIX compliance placeholder programs and
tests that were not part of the active Conan + CMake build graph.

Why archived:
- they overstated coverage and conformance relative to the current repository
  reality
- they were showing up as unwired first-party sources in the build audit
- they are still useful as historical reference material, but not as active
  evidence

Authoritative current references:
- `docs/analysis/CLAIMS_AUDIT.md`
- `docs/POSIX_COMPLIANCE.md`
- `docs/CURRENT_REALITY.md`
