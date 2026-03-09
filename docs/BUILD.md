# Build Instructions

This is the canonical build flow for XINIM as of 2026-03-08.

## Summary

XINIM now uses a pure Conan + CMake workflow:

- Run `conan install` into a preset-owned build tree.
- Configure with the matching CMake preset.
- Build and test from that same build tree.

The canonical layout is:

```text
build/
  x86_64/Debug/
  x86_64/Release/
  i486/Debug/
  i586/Debug/
  i686/Debug/
  x86_32_core2/Debug/
  x86_32_athlon/Debug/
  x86_32_phenom/Debug/
```

Each build tree owns its own `generators/`, `images/`, `logs/`, and `tools/`
directories under `CMAKE_BINARY_DIR`.

## Prerequisites

Install the packages listed in [REQUIREMENTS.md](REQUIREMENTS.md), including:

- `clang` and `lld`
- `cmake` and `ninja`
- `python3` and `conan`
- `qemu-system-x86`
- `xorriso`

## Configure and Build

x86_64 debug:

```bash
conan install . \
  -pr:h=conan/profiles/clang-x86_64 \
  -s build_type=Debug \
  -o '&:lane=x86_64' \
  -o '&:vfs_profile=default' \
  -of build/x86_64/Debug \
  --build=missing

cmake --preset x86_64-debug
cmake --build --preset x86_64-debug
ctest --preset x86_64-debug
```

x86_64 release:

```bash
conan install . \
  -pr:h=conan/profiles/clang-x86_64 \
  -s build_type=Release \
  -o '&:lane=x86_64' \
  -of build/x86_64/Release \
  --build=missing

cmake --preset x86_64-release
cmake --build --preset x86_64-release
ctest --preset x86_64-release
```

i486 debug:

```bash
conan install . \
  -pr:h=conan/profiles/clang-x86_32 \
  -s build_type=Debug \
  -o '&:lane=i486' \
  -o '&:vfs_profile=auto' \
  -of build/i486/Debug \
  --build=missing

cmake --preset i486-debug
cmake --build --preset i486-debug
ctest --preset i486-debug
```

Additional 32-bit lanes follow the same pattern:

- `i586-debug`
- `i686-debug`
- `x86_32_core2-debug`
- `x86_32_athlon-debug`
- `x86_32_phenom-debug`

Optional 32-bit ELF cross-toolchain lane:

```bash
conan install . \
  -pr:h=conan/profiles/clang-x86_64 \
  -s build_type=Debug \
  -o '&:lane=i486' \
  -o '&:x86_32_toolchain_mode=cross-elf' \
  -o '&:x86_elf_toolchain_triple=i386-elf' \
  -o '&:vfs_profile=tiny' \
  -of build/i486-cross/Debug \
  --build=missing

cmake --preset i486-cross-debug
cmake --build --preset i486-cross-debug
ctest --preset i486-cross-debug
```

Cross-lane notes:
- The default 32-bit flow remains `clang -m32` with the `clang-x86_32` Conan profile.
- The optional `cross-elf` flow keeps Conan on the host side and switches the
  32-bit guest build tree to an ELF cross compiler before `project()`.
- The low-RAM VFS profile is now also selectable from Conan with
  `-o '&:vfs_profile=auto|default|tiny'`.
- `auto` resolves to `tiny` on x86_32 lanes and `default` on x86_64.
- `i486` and `i586` default to `i386-elf`.
- `i686`, `x86_32_core2`, `x86_32_athlon`, and `x86_32_phenom` default to
  `i686-elf`.
- Use `-DXINIM_X86_ELF_TOOLCHAIN_ROOT=/path/to/toolchain` if the cross tools
  are not on `PATH`.

## QEMU

The build system itself no longer depends on wrapper scripts. QEMU launchers
remain available as runtime utilities.

x86_64:

```bash
cmake --build --preset x86_64-debug --target xinim_x86_64_image
./scripts/qemu_x86_64.sh --boot-image build/x86_64/Debug/images/x86_64/xinim-x86_64.iso
```

i486:

```bash
cmake --build --preset i486-debug --target xinim_bootstrap_grub
cmake --build --preset i486-debug --target xinim_i486_image
./scripts/qemu_i486.sh --boot-image build/i486/Debug/images/i486/xinim-i486dx.iso
```

Lane-aware launcher:

```bash
python3 scripts/qemu_matrix.py --build-dir build/i586/Debug --lane i586 --launch
```

## Notes

- `conan install` must be run before `cmake --preset ...`, because the preset
  expects `generators/conan_toolchain.cmake` to exist in the matching build
  tree.
- Each build tree configures exactly one lane, so 32-bit presets only register
  the active lane's guest/image/shell tests.
- Cross presets use separate build trees such as `build/i486-cross/Debug` so
  the optional ELF toolchain mode never collides with the default Clang lane.
- 32-bit image targets now bootstrap a pinned repo-local GNU GRUB under
  `build/<lane>/<config>/tools/grub/<version>/install` and use that local
  `grub-mkrescue` by default.
- `xorriso` remains a host prerequisite for ISO assembly. Override it with
  `-DXINIM_XORRISO_EXECUTABLE=/path/to/xorriso` if needed.
- No additional Conan runtime packages are required for the low-RAM bootfs/VFS
  lane; it stays freestanding and dependency-light on purpose.
- The x86_64 image bootstrap is now driven by CMake via
  [BootstrapLimine.cmake](/home/eirikr/Github/XINIM/cmake/BootstrapLimine.cmake),
  not a shell wrapper.
