# Xinim Requirements

Version: 1.4
Date: 2026-03-08
Status: Active

## Scope

This document describes the requirements for the actively maintained XINIM
build and validation flow.

Current truth:
- The verified build flow is a pure Conan + CMake flow rooted in
  `build/<lane>/<config>`.
- The active CMake graph now registers reusable 32-bit x86 lanes for `i486`,
  `i586`, `i686`, `x86_32_core2`, `x86_32_athlon`, and `x86_32_phenom`.
- The default 32-bit lane still uses Clang with `-m32`, and there is now an
  optional ELF cross-toolchain mode for the 32-bit guest build trees.
- The verified QEMU harness is image-oriented. If no bootable image exists,
  the boot tests skip rather than pretending to validate a guest boot.
- The verified i486 lane now produces a GRUB/Multiboot2 ISO and boots under
  `qemu-system-i386 -M pc -cpu 486`.
- The verified i486 lane now reaches an interactive Ring 3 `xash` session on
  COM2.
- The same reusable harness now reaches the shell for `i586` and `i686`.
- The repo build now includes a canonical hosted native C++ `xash` target.

For a reality snapshot, see [CURRENT_REALITY.md](CURRENT_REALITY.md).

## Build Tree Layout

Build products must live in preset-owned out-of-source build trees.

Canonical layout:
- `build/x86_64/Debug`
- `build/x86_64/Release`
- `build/i486/Debug`
- `build/i486-cross/Debug`
- `build/i586/Debug`
- `build/i586-cross/Debug`
- `build/i686/Debug`
- `build/i686-cross/Debug`
- `build/x86_32_core2/Debug`
- `build/x86_32_core2-cross/Debug`
- `build/x86_32_athlon/Debug`
- `build/x86_32_athlon-cross/Debug`
- `build/x86_32_phenom/Debug`
- `build/x86_32_phenom-cross/Debug`

Required Conan/CMake contract:
- `conan install` generates `generators/conan_toolchain.cmake` in the selected
  build tree.
- `cmake --preset <lane>-<config>` configures that same build tree.
- Images, logs, and bootstrapped tools are stored under that tree.
- Conan options now carry both the CPU lane and the low-RAM VFS profile:
  `-o '&:lane=<lane>' -o '&:vfs_profile=auto|default|tiny'`.

## Supported and Planned Targets

Verified now:
- Host build: Linux x86_64
- Host tests: Clang 21 + libc++ + Ninja
- QEMU harness scripts: `qemu-system-x86_64` and `qemu-system-i386`
- i486 bootstrap lane: `qemu-system-i386 -M pc -cpu 486` with a generated GRUB
  ISO and Multiboot2 handoff
- i586 and i686 bootstrap lanes: generated GRUB ISOs plus reusable boot/layout
  and shell harnesses

In progress:
- expanding the pure Conan + CMake migration across the remaining legacy docs
- Framebuffer metadata plumbing via `BootInfo`
- native `xinim-tools` rescope and Heirloom behavior intake

Planned but not yet complete:
- 486-safe early console path with linear-framebuffer drawing
- advanced 32-bit QEMU validation for `x86_32_core2`, `x86_32_athlon`, and
  `x86_32_phenom` beyond compile/image scaffolding
- Linear framebuffer/VBE handoff consumption beyond metadata parsing
- Modern QEMU graphics work after the 486 bootstrap lane is stable
- native C++ Heirloom-derived tool rebuilds beyond `xash`
- expanding the 32-bit lane beyond the initial PID 1 `xash` shell
- POSIX.1-2008 shell compliance work for the native shell

## Required Tools

Core build tools:
- CMake 3.28+
- Conan 2.0+
- Ninja
- Clang 21+ and LLVM binutils
- Python 3.10+
  Required for the build-graph audit, boot-test harnesses, and active-doc
  language verifier (`scripts/audit_build_coverage.py`,
  `scripts/verify_posix_language.py`, `test/boot/*.py`)
- Git 2.20+

Runtime and validation tools:
- `qemu-system-x86_64`
- `qemu-system-i386`
- `xorriso`
- `file`, `readelf`, and standard POSIX shell tools
- `python` for the repo boot and shell harnesses under `test/boot/`

Optional 32-bit ELF cross-toolchain tools:
- `i386-elf-gcc` plus `i386-elf-binutils` for `i486` and `i586`
- `i686-elf-gcc` plus `i686-elf-binutils` for `i686` and the higher 32-bit lanes

Documentation tools:
- Doxygen
- Sphinx
- Breathe
- Graphviz

## Package Baselines

Arch / CachyOS:

```bash
sudo pacman -Syu --needed \
  cmake \
  ninja \
  conan \
  clang \
  llvm \
  lld \
  lldb \
  libc++ \
  git \
  python \
  doxygen \
  graphviz \
  python-sphinx \
  python-sphinx-rtd-theme \
  python-breathe \
  qemu-system-x86
```

Optional Arch / AUR cross-toolchain packages:

```bash
yay -S --needed i386-elf-gcc i386-elf-binutils
yay -S --needed i686-elf-gcc i686-elf-binutils
```

Ubuntu / Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  git \
  clang \
  lld \
  lldb \
  libc++-dev \
  libc++abi-dev \
  doxygen \
  graphviz \
  qemu-system-x86 \
  file
python3 -m pip install --user conan sphinx breathe sphinx-rtd-theme
```

Fedora:

```bash
sudo dnf install -y \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  git \
  clang \
  llvm \
  lld \
  lldb \
  libcxx-devel \
  doxygen \
  graphviz \
  qemu-system-x86 \
  file
python3 -m pip install --user conan sphinx breathe sphinx-rtd-theme
```

## Module Notes

Kernel and boot:
- Freestanding kernel code is built from source in this repo.
- Limine headers are vendored under `third_party/limine`.
- 32-bit ISO generation now bootstraps a pinned repo-local GNU GRUB under the
  active build tree rather than requiring a host-installed `grub-mkrescue`.
- `xorriso` remains the required host-side ISO backend for both GRUB and
  Limine image generation.
- Conan is used for the host build/toolchain flow, not as a kernel runtime
  dependency source.

Hosted tools:
- `xinim_sh_host` is now part of the main repo build graph and emits `xash`.
- `xinim_tools_hosted` is the current umbrella target for hosted native tools.
- the repo now has an initial i486 userland syscall ABI scaffold at
  [syscall_i386.hpp](/home/eirikr/Github/XINIM/include/xinim/userland/syscall_i386.hpp)
- the repo now boots the i486 lane into an initial Ring 3 `xash` shell backed
  by that ABI
- Heirloom is being used as reference material for a native C++ rebuild, not
  as the intended final shipped implementation.
- Conan packages for hosted tools must remain host-side and become mandatory
  only when a native tool target actually consumes them.
- No extra Conan packages are currently justified for the shared low-RAM
  bootfs/VFS lane; the resident-path design stays freestanding by intent.

Tests:
- `ctest` now includes per-lane prepare, boot smoke, image layout, and shell
  tests for the registered 32-bit x86 lanes.
- The x86_64 QEMU harness tests require a bootable image to exercise a real
  guest.
- The active x86_64 shell smoke test is `python test/boot/x86_64_shell_test.py`.
- The 32-bit QEMU harness verifies both boot and shell reachability for the
  lanes that complete guest bring-up.

Toolchain and sysroot:
- The long-term custom sysroot and cross-toolchain work is still present, but
  much of it still assumes older external-sysroot defaults.
- Those scripts are not part of the canonical Conan + CMake build flow.
- The active optional cross path is a repo-owned CMake lane that can switch a
  32-bit guest build tree to `i386-elf-*` or `i686-elf-*` without changing the
  default Clang `-m32` flow.

Graphics:
- There is no verified SVGA, VESA, VBE, virtio-gpu, or framebuffer driver in
  the active kernel lane yet.
- Current boot metadata now carries framebuffer information for both Limine and
  Multiboot2 handoff paths.

## Warning Policy

Policy:
- New work should compile warning-free.
- The repo option `XINIM_ENABLE_WERROR=ON` is enabled by default.

Known debt:
- `-Wconversion` and `-Wsign-conversion` are still temporarily demoted from
  errors because of existing debt in scheduler, wait-graph, service, and
  crypto code.
- That debt is tracked in [analysis/TODO_TRACKER.md](analysis/TODO_TRACKER.md).

## Verification

Minimum verification commands:

```bash
conan install . \
  -pr:h=conan/profiles/clang-x86_64 \
  -s build_type=Debug \
  -o '&:lane=x86_64' \
  -of build/x86_64/Debug \
  --build=missing

cmake --preset x86_64-debug
cmake --build --preset x86_64-debug
ctest --preset x86_64-debug
```

Optional QEMU image validation:

```bash
ctest --preset x86_64-debug -L "boot|kshell"

conan install . \
  -pr:h=conan/profiles/clang-x86_32 \
  -s build_type=Debug \
  -o '&:lane=i486' \
  -of build/i486/Debug \
  --build=missing

cmake --preset i486-debug
cmake --build --preset i486-debug
ctest --preset i486-debug
```

Note:
- The x86_64 commands above use the repo-generated ISO path under
  `build/x86_64/Debug/images/x86_64/`.
- The i486 commands above use the repo-built GRUB ISO under
  `build/i486/Debug/images/i486/`.

Optional 32-bit ELF cross-lane verification:

```bash
conan install . \
  -pr:h=conan/profiles/clang-x86_64 \
  -s build_type=Debug \
  -o '&:lane=i486' \
  -o '&:x86_32_toolchain_mode=cross-elf' \
  -o '&:x86_elf_toolchain_triple=i386-elf' \
  -of build/i486-cross/Debug \
  --build=missing

cmake --preset i486-cross-debug
cmake --build --preset i486-cross-debug
ctest --preset i486-cross-debug
```

Cross-lane note:
- The optional `cross-elf` mode may use the normal `clang-x86_64` Conan host
  profile because Conan still manages only host-side configuration and generated
  toolchain files. The freestanding 32-bit guest tree switches to the ELF cross
  compiler inside CMake before `project()`.

## Related Documentation

- [BUILD.md](BUILD.md)
- [CURRENT_REALITY.md](CURRENT_REALITY.md)
- [ROADMAP.md](ROADMAP.md)
- [MODERNIZATION_STATUS.md](MODERNIZATION_STATUS.md)
- [analysis/TODO_TRACKER.md](analysis/TODO_TRACKER.md)
- [external_sources/I486_QEMU_PORTABILITY_SOURCES.md](external_sources/I486_QEMU_PORTABILITY_SOURCES.md)
- [external_sources/BUILD_ORCHESTRATION_SOURCES.md](external_sources/BUILD_ORCHESTRATION_SOURCES.md)
- [external_sources/POSIX_SHELL_SOURCES.md](external_sources/POSIX_SHELL_SOURCES.md)
- [specs/XINIM_TOOLS_NATIVE_PORT_PLAN.md](specs/XINIM_TOOLS_NATIVE_PORT_PLAN.md)
