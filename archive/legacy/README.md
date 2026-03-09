# archive/legacy

This tree stores retired XINIM sources and docs for historical reference.

Scope:
- Keep legacy implementations and placeholders available for diffs, audits, and provenance.
- Keep active build/test wiring in `src/`, `test/`, `userland/`, and CMake targets only.

Operational rule:
- Files under `archive/legacy/` are reference-only and are not part of active build targets.
- If a live source is retired, move it here instead of deleting it permanently.
