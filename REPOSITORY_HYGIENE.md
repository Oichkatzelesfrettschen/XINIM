````text
# Project Status

This document records the initial cleanup audit and codifies ongoing hygiene practices for the XINIM repository. It enumerates additional ignore patterns to maintain a pristine work tree, ensuring that only source code and essential project files are tracked.

---

### 1. Initial Audit Results

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

---

### 2. Ongoing Cleanup Practices

1.  **Use a clean build tree.** All compilation should target directories listed
    under `build*/` or other paths explicitly ignored by Git.
2.  **Avoid committing editor or OS metadata.** Files such as `*~`, `*.swp`, and
    `.DS_Store` are automatically excluded; remove any that appear.
3.  **Keep test binaries out of version control.** Executables generated from
    sources in `tests/` or `commands/` should never be committed.
4.  **Log files and generated reports** (e.g., `*_summary.json`) belong in a
    transient workspace, not in the repository.

The table below enumerates additional ignore patterns introduced to maintain a pristine work tree. Each pattern guards against accidental commits of derived artifacts or ephemeral editor state.

| Pattern                      | Rationale                                                                      |
|------------------------------|--------------------------------------------------------------------------------|
| `*.sw?`                      | Captures Vim swap files (e.g., `.swp`, `.swo`) so transient editor buffers never enter version control. |
| `#*#`, `.#*`                 | Excludes GNU Emacs autosave and lock files that track editing sessions but have no long-term value. |
| `*.orig`, `*.rej`            | Prevents merge and patch conflict leftovers from polluting the repository.       |
| `*.so`, `*.dylib`, `*.dll`   | Filters shared library outputs across Unix-like, macOS, and Windows platforms. |
| `*.obj`, `*.pdb`, `*.lib`    | Removes Microsoft toolchain artifacts: object files, debug symbols, and libraries. |
| `*.lo`, `*.la`               | Ignores libtool-generated intermediates that arise during portable builds.       |
| `*.gcda`, `*.gcno`           | Discards GCC code-coverage traces produced during instrumentation.               |

---

### 3. Optional History Scrubbing

To purge historical artifacts, run

```bash
bfg --delete-files '*.DS_Store' --delete-files '*.o'
````

from a clone outside the working directory, then force-push the rewritten
history. This step is optional and should be coordinated with all collaborators.

-----

### 4\. Future Audits

Periodic reviews using tools like `git ls-files` and `find` help ensure the
repository remains free of unintended files. Document any future cleanups by
appending to this file.

Periodic CI runs execute `git clean -Xn` to surface any ignored files that appear after builds, enabling fast detection of novel artifacts that may require updating `.gitignore`.

```
```