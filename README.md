# Minix 1 Source Overview

This repository contains the original sources of the Minix 1 operating system as
released in 1987.  The tree is organized into the major components of the
system such as the kernel, file system, libraries and user commands.

## Build Requirements

To compile the sources you need an ANSI C compiler, assembler and linker
suitable for the target architecture.  The code and makefiles assume a Unix
like toolchain and environment.

## Building

Every directory contains a `makefile` for building that part of the system.
Running `make` inside a component (for example `kernel` or `lib`) produces the
corresponding binaries or libraries.  The `test` directory also provides
makefiles for compiling small test programs.

## Documentation Note

The repository originally included short `read_me` files under `doc` and `lib`.
These files have been removed for brevity but their history remains available in
Git if needed.

