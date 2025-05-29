# Building and Testing

This document describes how to build the Minix 1 sources and verify that the
build environment works on a Unix-like host.

## Prerequisites

A 64-bit x86 compiler toolchain is required.  GCC 9 or later and either NASM
2.14 or YASM 1.3 are known to work.  CMake 3.5 or newer is needed when using the
CMake build system.

## Building with Makefiles

Each top level component (for example `kernel`, `lib` and `mm`) contains a
traditional `makefile`.  Change into the directory and run `make` to build the
component.  For example:

```sh
cd lib
make
```

## Building with CMake

A more convenient option is to use CMake from the repository root:

```sh
cmake -B build
cmake --build build
```

You can select the AT or PC/XT wini driver using the options described in the
`README.md` file.

## Testing the Build

To quickly check that the toolchain works you can compile one of the sample
programs in the `test` directory.  For instance:

```sh
make -C test f=test0
```

Successful compilation of these small programs indicates the compiler and
assembler are functioning correctly.
