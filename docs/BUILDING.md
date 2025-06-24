Below is the **fully-merged and expanded “Building and Testing” guide**.
All conflict markers have been removed, duplicate passages deduplicated, and the two alternative sections (“Recommended Compiler Flags” and the richer “Build Modes” table) have been fused into one cohesive chapter.

````markdown
# Building and Testing

This document explains how to build the Minix 1 sources and verify that the
tool-chain works on a Unix-like host.

The codebase is **work-in-progress**, aiming to reproduce classic Minix
simplicity on modern **arm64** and **x86-64** machines using **C++23**.

---

## 1 · Prerequisites

* **C++23 tool-chain** – Clang 18 ✚ LLVM 18 (lld, lldb) is recommended; GCC 13+
  also works.
* **Assemblers** – NASM ≥ 2.14 *or* YASM ≥ 1.3.
* **CMake** ≥ 3.5 if you prefer that build system.
* **Make** / POSIX shell for the traditional Makefiles.
* **libsodium** – development headers for the crypto subsystem (`libsodium-dev`).

All packages can be installed automatically:

```sh
tools/setup.sh            # installs clang/lld/lldb, nasm, cmake …
clang++ --version         # check detected version
````

The script falls back to existing Clang/Llvm if the network is offline.

---

## 2 · Building with Makefiles

Every top-level component (`kernel`, `lib`, `mm`, …) has a classic
`Makefile`. Compile in place:

```sh
cd lib
make
```

* Headers are read from `../include`.
* The default library path is `../lib/lib.a`.

Override `CC`, `CFLAGS`, or `LDFLAGS` on the command line when tuning.

---

## 3 · Building with CMake

From the repository root:

```sh
cmake -B build
cmake --build build
```

Driver variants (AT vs PC/XT) can be selected via CMake options (see
`README.md`).

---

## 4 · Cross-compiling for x86-64

Specify a **cross prefix**:

```sh
cmake -B build -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=x86_64-elf-
cmake --build build
```

or

```sh
make CROSS_PREFIX=x86_64-elf-
```

The build system then invokes `${CROSS_PREFIX}clang++`.

---

## 5 · Build Modes & Recommended Flags

The project provides three canonical modes; each mode is reproducible by
setting `BUILD_MODE=<mode>` when using `make`, or the equivalent CMake
`-DCMAKE_BUILD_TYPE=` flag.

| Mode        | Purpose                        | Representative flags                   |
| ----------- | ------------------------------ | -------------------------------------- |
| **debug**   | Heavy diagnostics + sanitizers | `-g3 -O0 -fsanitize=address,undefined` |
| **release** | Maximum performance / size     | `-O3 -DNDEBUG -flto -march=native`     |
| **profile** | gprof / perf instrumentation   | `-O2 -g -pg`                           |

Typical ad-hoc compilation during development:

```sh
clang++ -std=c++23 -O2 -pipe -Wall -Wextra -Wpedantic -march=native \
        example.cpp -o example
```

Advanced builds may add **LTO**, **PGO**, or LLVM **Polly** (`-mllvm -polly`).

---

## 6 · Testing the Tool-chain

Quick smoke test:

```sh
make -C test f=t10a LIB=       # skips lib.a linkage
```

Or compile directly:

```sh
clang++ -std=c++23 -O2 -pipe -Wall -Wextra -Wpedantic -march=native \
        tests/t10a.cpp -o tests/t10a
./tests/t10a && echo "✓ tool-chain OK"
```

---

## 7 · Running Example Programs

```sh
./test/t10a
echo $?
```

Expected exit status is **0**.

---

## 8 · Historical DOS Build Scripts

Legacy MS-DOS batch files live in `tools/c86`. They are retained for
archaeology only; modern C++ replacements (e.g. `bootblok.cpp`) are built
automatically.

---

## 9 · Modernization Helper

`tools/modernize_cpp17.sh` converts `.c`/`.h` to modern `.cpp`/`.hpp`,
updates include paths, and drops a temporary header banner. Run it when
switching fully to C++23.

---

## 10 · Development Utilities

```sh
sudo apt-get install -y cloc cppcheck cscope
python3 -m pip install --user lizard
```

Generate reports:

```sh
tools/run_cppcheck.sh
```

Outputs land in `build/reports/` (XML / JSON) along with a `cscope` DB.

---

```

*File is now conflict-free, consistently formatted, and combines the best of both original branches.*
```
