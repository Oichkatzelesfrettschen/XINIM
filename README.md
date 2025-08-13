# XINIM Project

XINIM is a full C++23 reimplementation of the classic MINIX 1 operating system. The repository combines the kernel, memory management, file system, libraries, commands, and tooling into one cohesive environment. It demonstrates modern systems programming techniques with strong type safety, extensive error handling, and cross-platform support.

For a comprehensive guide with build instructions and architectural details, refer to [docs/README_renamed.md](docs/README_renamed.md).

**XINIM** is a clean-room C++ 23 re-implementation of the original MINIX 1 educational operating system.  
The repository contains the full kernel, memory manager, file-system, classic user-land utilities, and supporting build scripts.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building](#building)
3. [Cleaning the Workspace](#cleaning-the-workspace)
4. [Documentation](#documentation)
5. [License](#license)

---

## Prerequisites

Install the tool-chain and host packages listed in **`docs/TOOL_INSTALL.md`**.
The step-by-step commands formerly automated by `setup.sh` now live in
[`tools/setup.md`](tools/setup.md). Example for Ubuntu 24.04 LTS:

```bash
sudo apt-get update
sudo apt-get install -y $(tr '\n' ' ' < docs/TOOL_INSTALL.md)
````

---

## Building

See [docs/BUILDING.md](docs/BUILDING.md) for canonical Ninja and Clang
instructions, including cross-compilation workflows and QEMU launch notes.

---

## Cleaning the Workspace

```bash
# Remove all build artefacts and test outputs
./tools/clean.sh
```

or, if using CMake directly:

```bash
cmake --build build --target clean
rm -rf build/
```

---

## Documentation

| Document                      | Description                         |
| ----------------------------- | ----------------------------------- |
| `docs/BUILDING.md`            | Full build and flashing guide       |
| `docs/ARCHITECTURE.md`        | Subsystem overview and design notes |
| `docs/TOOL_INSTALL.md`        | OS-specific dependency list         |
| `docs/sphinx/html/index.html` | Generated developer manual in HTML  |

---

## Sort Utility Merge Mode

The modern `sort` command supports a multi-file merge when invoked with the
`-m` flag. Each input file must already be sorted; the utility performs a
streaming k-way merge using the same comparison rules as regular sorting.  When
combined with the `-u` option, duplicate lines encountered across input files
are removed during the merge.

---

## License
Licensed under the **BSD-3-Clause** license. See `LICENSE` for details.
