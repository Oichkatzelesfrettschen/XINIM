````markdown
# Building and Testing

This document explains how to build the Minix 1 sources and verify that the
tool-chain works on a Unix-like host.

The codebase is **work in progress**, aiming to reproduce classic Minix
simplicity on modern **arm64** and **x86-64** machines using **C++23**.

---

## 1 · Prerequisites

* **C++23 tool-chain** – Clang 18 (lld, lldb) is recommended; GCC 13 or newer also works  
  (Clang 20 is detected automatically if installed).
* **Assembler** – NASM ≥ 2.14 *or* YASM ≥ 1.3.
* **CMake** ≥ 3.5 (if you use the CMake flow).
* **POSIX make + sh** for the classic Makefiles.
* **libsodium** development headers for the crypto subsystem.

Install everything with:

```sh
# See tools/setup.md for the step-by-step package list
clang++ --version       # verify the compiler in PATH
````

---

## 2 · Building with Makefiles

```sh
cd lib
make                          # builds the selected component in-place
```

* Headers are taken from `../include`.
* The default static library output is `../lib/lib.a`.

Override `CC`, `CFLAGS`, or `LDFLAGS` on the command line as needed.

---

## 3 · Building with CMake

```sh
cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18
ninja -C build
```

Driver variants (AT vs PC/XT) and other options are configured via CMake
flags—see `README.md`.

---

## 4 · Cross-compiling for x86-64

```sh
cmake -B build -G Ninja -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=x86_64-elf- \
      -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18
ninja -C build
```

or, using Makefiles:

```sh
make CROSS_PREFIX=x86_64-elf-
```

Both flows call `${CROSS_PREFIX}clang++`.

---

## 5 · Build Modes & Recommended Flags

| Mode        | Purpose                        | Representative flags                   |
| ----------- | ------------------------------ | -------------------------------------- |
| **debug**   | Heavy diagnostics + sanitizers | `-g3 -O0 -fsanitize=address,undefined` |
| **release** | Maximum performance / size     | `-O3 -DNDEBUG -flto -march=x86-64-v1`  |
| **profile** | gprof / perf instrumentation   | `-O2 -g -pg`                           |

Example manual build:

```sh
clang++ -std=c++23 -O2 -pipe -Wall -Wextra -Wpedantic -march=x86-64-v1 \
        example.cpp -o example
```

Add **LTO**, **PGO**, or LLVM **Polly** (`-mllvm -polly`) as required.

---

## 6 · Testing the Tool-chain

Quick smoke test:

```sh
make -C test f=t10a LIB=      # builds without lib.a, runs minimal test
```

or compile directly:

```sh
clang++ -std=c++23 -O2 -pipe -Wall -Wextra -Wpedantic -march=x86-64-v1 \
        tests/t10a.cpp -o tests/t10a
./tests/t10a && echo "✓ tool-chain OK"
```

---

## 7 · Running Example Programs

```sh
./tests/t10a
echo $?
```

Expected exit status is **0**.

---

## 8 · Historical DOS Build Scripts

Legacy MS-DOS batch files reside in `tools/c86`.
They are retained for reference only; modern C++ replacements (e.g.,
`bootblok.cpp`) build automatically in native and cross workflows.

---

## 9 · Modernisation Helper

`tools/modernize_cpp17.sh` upgrades legacy `.c`/`.h` sources to modern
`.cpp`/`.hpp`, adjusts include paths, and inserts a temporary banner.
Run it when migrating fully to C++23.

---

## 10 · Development Utilities

```sh
sudo apt-get install -y cloc cppcheck cscope
python3 -m pip install --user lizard
```

Generate analysis reports:

```sh
tools/run_cppcheck.sh
```

Outputs are placed in `build/reports/` (XML / JSON) along with a `cscope`
database.

---

## 11 · Git Hooks

Install the provided pre-commit hook to automatically format staged C and C++ sources:

```sh
ln -s ../../hooks/pre-commit .git/hooks/pre-commit
```

The hook runs `clang-format -i` on each staged file and re-adds the result to the commit.

---

## 12 · Generating Documentation

API reference and architecture manuals combine **Doxygen** with **Sphinx**
using the **Breathe** extension. The mapping in `docs/sphinx/conf.py` is:

```python
breathe_projects = { "XINIM": "../doxygen/xml" }
```

Build the HTML site:

```bash
python3 -m pip install --user sphinx breathe
doxygen docs/Doxyfile
sphinx-build -b html docs/sphinx docs/sphinx/html
```

Open `docs/sphinx/html/index.html` in a browser to view the generated pages.

```
```
