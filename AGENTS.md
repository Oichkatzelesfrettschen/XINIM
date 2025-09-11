# Repository Guidelines

## Project Structure & Module Organization
- `kernel/`, `mm/`, `fs/`: Core OS subsystems.
- `lib/`, `include/`, `h/`: Libraries and public headers.
- `commands/`: User‑land utilities (e.g., `sort`, `svcctl`).
- `crypto/`: PQ/crypto helpers and stubs.
- `tests/`: Standalone C++ test executables; CMake drives `ctest`.
- `tools/`: Developer scripts (`clean.sh`, `run_cppcheck.sh`, `run_clang_tidy.sh`).
- `docs/`: Build, architecture, and Sphinx/Doxygen configs.

## Build, Test, and Development Commands
- Configure + build (Ninja):
  - `cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++-18`
  - `ninja -C build`
- Run tests:
  - `ctest --test-dir build` (or `ninja -C build test`)
- Per‑component Makefiles (legacy flow):
  - `make -C lib` (override `CC/CFLAGS` as needed)
- Clean workspace:
  - `./tools/clean.sh`
- Static analysis:
  - `./tools/run_cppcheck.sh` and `./tools/run_clang_tidy.sh`
- Docs site:
  - `doxygen docs/Doxyfile && sphinx-build -b html docs/sphinx docs/sphinx/html`

## Multi‑Toolchain & Cross‑Compile
- Linux Clang 18:
  - `cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 && ninja -C build`
- Linux GCC 13:
  - `cmake -B build -G Ninja -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 && ninja -C build`
- macOS (Apple Silicon) native build:
  - `brew install llvm cmake ninja nasm yasm libsodium`
  - `LLVM=$(brew --prefix llvm); export PATH="$LLVM/bin:$PATH"`
  - `cmake -B build -G Ninja -DCMAKE_C_COMPILER="$LLVM/bin/clang" -DCMAKE_CXX_COMPILER="$LLVM/bin/clang++" && ninja -C build`
- macOS → x86_64 ELF with cross‑binutils:
  - `brew install x86_64-elf-binutils x86_64-elf-gcc`
  - `cmake -B build -G Ninja -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=x86_64-elf- && ninja -C build`
- macOS → x86_64 ELF with Clang+LLD (no gcc):
  - `LLVM=$(brew --prefix llvm)`
  - `cmake -B build -G Ninja -DCROSS_COMPILE_X86_64=ON \`
    `-DCMAKE_C_COMPILER="$LLVM/bin/clang" -DCMAKE_CXX_COMPILER="$LLVM/bin/clang++" \`
    `-DCMAKE_C_FLAGS="--target=x86_64-unknown-elf" -DCMAKE_CXX_FLAGS="--target=x86_64-unknown-elf" \`
    `-DCMAKE_LINKER="$LLVM/bin/ld.lld" && ninja -C build`
- Notes: NASM/YASM should emit `elf64` objects for cross builds; macOS system `ld` cannot link ELF—use `ld.lld`.

## Presets & Make Targets
- Configure: `cmake --preset dev-default` (writes to `build/`)
- Build builder: `cmake --build --preset tools`
- Stub image: `cmake --build --preset stub` or `make stub`
- QEMU stub: `cmake --build --preset qemu-stub` or `make qemu_stub`
- List presets: `cmake --list-presets`

## x86_64 Modernization & QEMU
- Baseline: C++23 with strict warnings. Apply x86_64 CPU flags per-target (not globally) to keep host tool builds portable. Prefer fixed‑width types (`uint32_t`, `uintptr_t`), avoid `long` size assumptions, and audit struct packing/alignment for LP64.
- Build system components:
  - `cmake -B build -G Ninja -DBUILD_TOOLS=ON` (default ON)
  - `ninja -C build tools` (builds `build/tools/minix_tool_build`)
- Compose bootable image (raw):
  - Prepare a 512‑byte bootblock (`bootblok`), plus `kernel`, `mm`, `fs`, `init`, `fsck` binaries.
  - Option A (script): `tools/make_image.sh build/tools/minix_tool_build bootblok kernel mm fs init fsck build/xinim.img`
  - Option B (CMake target): set cache vars `BOOTBLOCK_BIN`, `KERNEL_BIN`, `MM_BIN`, `FS_BIN`, `INIT_BIN`, `FSCK_BIN`. If you have a prebuilt image builder, set `IMAGE_BUILDER=/abs/path/minix_tool_build`. Then run `ninja -C build xinim_image`.
- Run in QEMU (x86_64):
  - `./tools/run_qemu.sh build/xinim.img -m 512M -smp 2 -accel hvf` (macOS) or `-accel kvm` (Linux)
  - Or `ninja -C build qemu` after setting `BOOTBLOCK_BIN`… cache vars; optionally set `-D QEMU_ACCEL=hvf|kvm|tcg`.
  - macOS tips: `brew install qemu`; HVF accel: add `-accel hvf`.

## Quick Stub Image (Plumbing Check)
- Build builder: `ninja -C build minix_tool_build`
- Create stub image: `ninja -C build xinim_stub_image` (raw blobs, no patching)
- Run QEMU: `ninja -C build qemu_stub` (or `./tools/run_qemu.sh build/xinim-stub.img -accel hvf`)

## Coding Style & Naming Conventions
- Formatter: `.clang-format` (LLVM base, 4‑space indent, 100 cols, no tabs). Run via the pre‑commit hook (`hooks/pre-commit`).
- Linting: `.clang-tidy` focuses on `bugprone-*` and `portability-*`; treat warnings as errors.
- Files: C++ sources use `.cpp/.hpp`; tests use `test_*.cpp` or `tNN*.cpp`. Prefer lower_snake_case for file names.

## Testing Guidelines
- Framework: standalone C++23 executables added with `add_test`; run with `ctest`.
- Location: all tests live in `tests/`; many integrate kernel sources via CMake targets.
- Conventions: name tests `test_<area>.cpp`; for quick probes, `t10a.cpp`‑style is acceptable.
- Debugging: use `-fsanitize=address,undefined` in Debug builds (default in `tests/CMakeLists.txt`).

## Commit & Pull Request Guidelines
- Messages: follow Conventional Commits when possible (`feat:`, `fix:`, `docs:`). Keep the subject ≤ 72 chars; add a concise body.
- Scope: one logical change per PR. Link related issues. Include reproduction steps and `ctest` output when fixing bugs.
- Quality gates: run `clang-format`, `clang-tidy`, and `ctest` locally; avoid committing generated artifacts (e.g., `build/`, docs outputs).

## Security & Configuration Tips
- Do not commit secrets or large binaries. Install toolchain per `docs/TOOL_INSTALL.md` and `tools/setup.md`.
- Crypto code depends on `libsodium`; prefer stubs in `tests/` for host‑only runs.
