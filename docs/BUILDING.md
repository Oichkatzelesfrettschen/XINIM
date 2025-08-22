# Building XINIM

## Prerequisites

Install Clang 18 and the accompanying LLVM tools:

```bash
sudo apt-get install clang-18 lld-18 lldb-18
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Pre-commit Hook

Enable automatic formatting of staged C++ sources by installing the provided hook:

```bash
ln -s ../../hooks/pre-commit .git/hooks/pre-commit
```

The hook invokes `clang-format -i` on staged `.cc`, `.cpp`, `.cxx`, `.hh`, `.hpp`, and `.hxx` files and re-stages the formatted results.
