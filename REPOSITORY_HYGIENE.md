# Repository Hygiene

This document records the initial cleanup audit and codifies ongoing hygiene
practices for the XINIM repository.

## 1. Initial Audit Results

The following generated files, editor leftovers, and binary build artifacts were
present in the repository and have been removed:

- `include/.DS_Store`
- `fs/.DS_Store`
- `test_makefile`
- `test_architecture_demo`
- `kr_cpp_summary.json`
- `commands/mined_final`
- `commands/mined_final_improved`
- `commands/mined_final_test`
- `tests/t10a`
- `tests/t10a 2`

The `.gitignore` file has been expanded to prevent these and similar artifacts
from returning.

## 2. Ongoing Cleanup Practices

1. **Use a clean build tree.** All compilation should target directories listed
   under `build*/` or other paths explicitly ignored by Git.
2. **Avoid committing editor or OS metadata.** Files such as `*~`, `*.swp`, and
   `.DS_Store` are automatically excluded; remove any that appear.
3. **Keep test binaries out of version control.** Executables generated from
   sources in `tests/` or `commands/` should never be committed.
4. **Log files and generated reports** (e.g., `*_summary.json`) belong in a
   transient workspace, not in the repository.

## 3. Optional History Scrubbing

To purge historical artifacts, run

```bash
bfg --delete-files '*.DS_Store' --delete-files '*.o'
```

from a clone outside the working directory, then force-push the rewritten
history. This step is optional and should be coordinated with all collaborators.

## 4. Future Audits

Periodic reviews using tools like `git ls-files` and `find` help ensure the
repository remains free of unintended files. Document any future cleanups by
appending to this file.

