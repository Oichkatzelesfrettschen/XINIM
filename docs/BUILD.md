# Build Instructions

This is the canonical build flow for XINIM.

## Summary

XINIM now uses a pure CMake workflow:

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

Each build tree owns its own `images/`, `logs/`, and `tools/` directories under
`CMAKE_BINARY_DIR`.

## Prerequisites

Install the packages listed in [REQUIREMENTS.md](REQUIREMENTS.md), including:

- `clang` and `lld`
- `cmake` and `ninja`
- `python3`
- `qemu-system-x86`
- `qemu-img`
- `xorriso`

## Configure and Build

x86_64 debug:

```bash
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -B build/x86_64/Debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DXINIM_CPU_LANE=x86_64
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build build/x86_64/Debug
ctest --test-dir build/x86_64/Debug --output-on-failure
```

i486 debug:

```bash
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --preset i486-standalone
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build build/i486/Debug --target i486_boot_disk
ctest --test-dir build/i486/Debug --output-on-failure
```

i686 CMOV-capable debug:

```bash
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --preset i686-standalone
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build build/i686/Debug --target i686_boot_disk
ctest --test-dir build/i686/Debug --output-on-failure
```

Additional 32-bit lanes follow the same pattern:

- `i586-standalone`
- `i686-standalone`
- `x86_32_core2`, `x86_32_athlon`, and `x86_32_phenom` can be configured
  manually with `-DXINIM_CPU_LANE=<lane>` until presets are added.

Optional 32-bit ELF cross-toolchain lane:

```bash
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -B build/i486-cross/Debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DXINIM_CPU_LANE=i486 \
  -DXINIM_X86_32_TOOLCHAIN_MODE=cross-elf \
  -DXINIM_X86_ELF_TOOLCHAIN_TRIPLE=i386-elf \
  -DXINIM_VFS_PROFILE=tiny
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake --build build/i486-cross/Debug
ctest --test-dir build/i486-cross/Debug --output-on-failure
```

Cross-lane notes:
- The default 32-bit flow remains `clang -m32`.
- The optional `cross-elf` flow keeps the host `clang` and `clang++`
  drivers and supplies `--target=<triple>`; target binutils provide only the
  linker and binary utilities.
- The low-RAM VFS profile is selectable with `-DXINIM_VFS_PROFILE=auto|default|tiny`.
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
cmake --build build/i486/Debug --target xinim_bootstrap_grub
cmake --build build/i486/Debug --target xinim_i486_image
./scripts/qemu_i486.sh --boot-image build/i486/Debug/images/i486/xinim-i486dx.iso
```

Lane-aware launcher:

```bash
python3 scripts/qemu_matrix.py --build-dir build/i686/Debug --lane i686 --launch
```

## Notes

- Run the lane commands with `CFLAGS`, `CXXFLAGS`, and `LDFLAGS` unset so host
  optimization defaults do not leak into freestanding 32-bit builds.
- Each build tree configures exactly one lane, so 32-bit presets only register
  the active lane's guest/image/shell tests.
- Cross presets use separate build trees such as `build/i486-cross/Debug` so
  the optional ELF toolchain mode never collides with the default Clang lane.
- 32-bit image targets now bootstrap a pinned repo-local GNU GRUB under
  `build/<lane>/<config>/tools/grub/<version>/install` and use that local
  `grub-mkrescue` by default.
- `xorriso` remains a host prerequisite for ISO assembly. Override it with
  `-DXINIM_XORRISO_EXECUTABLE=/path/to/xorriso` if needed.
- No additional runtime packages are required for the low-RAM bootfs/VFS lane;
  it stays freestanding and dependency-light on purpose.
- The x86_64 image bootstrap is now driven by CMake via
  [BootstrapLimine.cmake](/home/eirikr/Github/XINIM/cmake/BootstrapLimine.cmake),
  not a shell wrapper.
