# Minix 1 Source Overview

This repository contains the original sources of the Minix 1 operating system as
released in 1987.  The tree is organized into the major components of the
system such as the kernel, file system, libraries and user commands.

## Build Requirements

To compile the sources you need an ANSI C compiler, assembler and linker
suitable for the target architecture.  The code and makefiles assume a Unix
like toolchain and environment.  Recent versions of GCC (9 or later) work well
for building the C sources.  Either NASM 2.14 or YASM 1.3 can be used to
assemble the few hand written assembly files.

More details on building and verifying the project are available in
`BUILDING.md`.

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

The kernel wini driver can be selected by passing `-DDRIVER_AT=ON` for the AT
driver or `-DDRIVER_PC=ON` for the original PC/XT driver.  If neither option is
specified the PC/XT driver is used.

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

