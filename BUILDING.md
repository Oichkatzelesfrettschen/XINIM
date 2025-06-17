# Building and Testing

This document describes how to build the Minix 1 sources and verify that the
build environment works on a Unix-like host.

The codebase is currently a **work in progress** focused on reproducing the
original Minix simplicity on modern arm64 and x86_64 machines using C++23.

## Prerequisites
A 64-bit x86 compiler toolchain supporting C++23 is required. Install Clang++ 18 and the full LLVM 18 suite—including lld and lldb—before building or running tests. GCC 13 or later can still be used. Either NASM 2.14 or YASM 1.3 are known to work. CMake 3.5 or newer is needed when using the CMake build system.

## Building with Makefiles

Each top level component (for example `kernel`, `lib` and `mm`) contains a
traditional `Makefile`.  Change into the directory and run `make` to build the
component.  For example:

```sh
cd lib
make
```

The Makefiles use relative paths to locate headers and the C library.
Headers are taken from `../include` and the library defaults to
`../lib/lib.a`.  Override `CC`, `CFLAGS`, or `LDFLAGS` on the command
line to adjust the build.

## Building with CMake

A more convenient option is to use CMake from the repository root:

```sh
cmake -B build
cmake --build build
```

You can select the AT or PC/XT wini driver using the options described in the
`README.md` file.

## Cross Compiling for x86_64

The build can target a bare x86\_64 system using a cross toolchain.  Specify the
tool prefix through the `CROSS_PREFIX` variable.  When invoking CMake directly
pass `-DCROSS_COMPILE_X86_64=ON` along with the prefix.  Clang will be invoked
with that prefix for cross compiling:

```sh
cmake -B build -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=x86_64-elf-
cmake --build build
```

These commands will call `${CROSS_PREFIX}clang++` for compilation.

The top-level `Makefile` accepts the same variable so the above commands can be
simplified to:

```sh
make CROSS_PREFIX=x86_64-elf-
```

The Makefile passes the prefix to clang++ automatically.

## Testing the Build

To quickly check that the toolchain works you can compile one of the sample
programs in the `test` directory.  For instance:

```sh
make -C test f=t10a LIB=
```

Passing `LIB=` avoids linking against the project library, letting the example
compile even if the rest of the system has not been built yet.  Successful
compilation confirms the compiler and assembler are functioning correctly.

### Running the Example Programs

Execute the newly built program directly from the `test/` directory:

```sh
./test/t10a
echo $?
```

The `t10a.cpp` program simply returns `0`, so `echo $?` prints `0` when the
toolchain is working properly.

## Historical DOS Build Scripts

The `tools/c86` directory stores batch files and legacy utilities once used with
an MS-DOS cross compiler. They are kept for reference but are not executed by
the current build. Modern equivalents written in C (for example `bootblok.cpp`)
provide the needed functionality and are built automatically.

## Modernization Script

A helper script `tools/modernize_cpp17.sh` automates renaming sources to
`.cpppp` and `.hpp`, updates include paths and drops a temporary modernization
header into each file. Invoke it from the repository root when ready to move
the codebase fully to C++23.

## Development Tools

Several optional utilities generate code metrics and perform static analysis.
Install them using the package manager and Python's package installer:

```sh
sudo apt-get install -y cloc cppcheck cscope
python3 -m pip install --user lizard
```

### Running the Tools

Create a directory called `logs/` at the project root and store all reports
there:

```sh
mkdir -p logs
cloc . > logs/cloc.log
lizard -o logs/lizard.log .
cppcheck --enable=all --std=c++23 --output-file=logs/cppcheck.log .
cscope -Rb -q
```

`cscope` writes its database to `cscope.out`. The other commands generate log
files in the `logs/` directory for later review.
