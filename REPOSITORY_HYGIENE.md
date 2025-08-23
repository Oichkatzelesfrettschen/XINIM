# Repository Hygiene

This document enumerates additional ignore patterns introduced to maintain a pristine work tree. Each pattern guards against accidental commits of derived artifacts or ephemeral editor state.

| Pattern | Rationale |
| --- | --- |
| `*.sw?` | Captures Vim swap files (e.g., `.swp`, `.swo`) so transient editor buffers never enter version control. |
| `#*#`, `.#*` | Excludes GNU Emacs autosave and lock files that track editing sessions but have no long-term value. |
| `*.orig`, `*.rej` | Prevents merge and patch conflict leftovers from polluting the repository. |
| `*.so`, `*.dylib`, `*.dll` | Filters shared library outputs across Unix-like, macOS, and Windows platforms. |
| `*.obj`, `*.pdb`, `*.lib` | Removes Microsoft toolchain artifacts: object files, debug symbols, and libraries. |
| `*.lo`, `*.la` | Ignores libtool-generated intermediates that arise during portable builds. |
| `*.gcda`, `*.gcno` | Discards GCC code-coverage traces produced during instrumentation. |

Periodic CI runs execute `git clean -Xn` to surface any ignored files that appear after builds, enabling fast detection of novel artifacts that may require updating `.gitignore`.
