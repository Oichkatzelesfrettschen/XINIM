# Build Instructions

## Prerequisites
- CMake 3.5 or newer
- Clang 18 toolchain and LLVM utilities (`lld`, `lldb`)
- Doxygen for API documentation
- pre-commit for Git hook management (`pip install pre-commit`)

## Pre-commit Hooks
Install the project's hooks after cloning to automatically run formatting and
static analysis helpers located under `tools/`:

```bash
pre-commit install
```

Run the entire suite manually before pushing changes:

```bash
pre-commit run --all-files
```

These hooks chain `clang-format`, `clang-tidy`, and `cppcheck` through local
scripts to enforce style and detect issues early.

## Configure
```bash
cmake -S . -B build
```

## Compile
```bash
cmake --build build
```

## Generate Documentation
```bash
cmake --build build --target doc
```

The documentation target emits HTML and XML into `docs/doxygen`,
ready for consumption by Sphinx via the Breathe extension.
