# Contributing Guidelines

This project employs [pre-commit](https://pre-commit.com) to guarantee stylistic and
compilability checks before any commit reaches the repository.

## Quick start
1. Install the tooling:
   ```bash
   pip install pre-commit
   ```
2. Register the git hook:
   ```bash
   pre-commit install
   ```
3. Run checks manually (optional):
   ```bash
   pre-commit run --files <path/to/file.cpp>
   ```

The configured hooks perform two actions on every staged C++ source file:

* **Formatting** – `clang-format` enforces the project's `.clang-format` style.
* **Compilation** – `g++ -std=c++23 -Wall -Wextra -Werror -pedantic -fsyntax-only`
  validates that the file passes a pedantic compilation step without producing
  warnings.

To update hook versions, execute:
```bash
pre-commit autoupdate
```

