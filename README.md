# Minix 1 Source Overview

This repository contains the original sources of the Minix 1 operating system as
released in 1987.  The tree is organized into the major components of the
system such as the kernel, file system, libraries and user commands.

Minix 1 was originally distributed as an appendix to Andrew S. Tanenbaum's
book *Operating Systems: Design and Implementation*.  Providing a real
operating system to accompany the text allowed students to explore the source
while reading about its design.  This repository preserves that historical
release.

## Build Requirements

Building now requires a 64-bit x86 compiler toolchain.  Clang is the default
compiler throughout the repository.  Recent versions of GCC (9 or later) also
work, but every build file now selects clang unless explicitly overridden.
NASM 2.14 or YASM 1.3 remain the recommended assemblers.  Older 16-bit
configurations are no longer supported.

More details on building and verifying the project are available in
`BUILDING.md`.

## 64-bit Bootloader
The boot utilities now include a rewritten `bootblok.s` capable of loading a 64-bit kernel. The code performs the classic real mode to protected mode transition and then enables long mode with a minimal page mapping so both BIOS and UEFI firmware can boot the kernel.

## Building

Every directory contains a `makefile` for building that part of the system.
Running `make` inside a component (for example `kernel` or `lib`) produces the
corresponding binaries or libraries.  The `test` directory also provides
makefiles for compiling small test programs.

Alternatively the sources can be built with CMake.  From the repository root
run:

```sh
cmake -B build
cmake --build build
```

A simple root `Makefile` runs these commands as well, so you can just run `make` to build everything and `make clean` to remove the `build` directory.

Both build systems support selecting the kernel wini driver.  Pass
`-DDRIVER_AT=ON` for the AT driver or `-DDRIVER_PC=ON` for the original PC/XT
driver.  When using the root `Makefile` these variables can be specified on the
command line, e.g. `make DRIVER_AT=ON`.  If neither option is specified the
PC/XT driver is used by default.

## Extent Table Helpers

The compatibility layer in `fs/compat.c` now provides `alloc_extent_table()`
alongside `init_extended_inode()`.  Call `init_extended_inode()` when a new
inode is created and then invoke `alloc_extent_table()` to allocate and
initialize the inode's extent table.  The function returns `OK` on success or
an error code such as `ENOMEM` when memory is exhausted.

## License

The historic Minix 1 sources were distributed under a permissive license from
Prentice Hall allowing educational use and redistribution.  As of version 2.0.3
(May 2001) the project adopted the BSD&nbsp;3-Clause license, retroactively
applying it to all earlier releases.  See the [LICENSE](LICENSE) file for the
full text.

## Documentation Note

The repository originally included short `read_me` files under `doc` and `lib`.
These files have been removed for brevity but their history remains available in
Git if needed.

## Repository Tree

The file `TREE.txt` records the directory layout of the repository. To refresh
it run `python3 tools/ascii_tree.py --exclude TREE.txt > TREE.txt` from the
project root.


## Code Style

All C sources adhere to a consistent formatting defined in `.clang-format`.  The
repository follows the LLVM style with four space indentation.  Static analysis
is configured via `.clang-tidy` enabling the `bugprone-*` and `portability-*`
check groups and treating all warnings as errors.
