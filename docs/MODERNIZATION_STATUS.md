# Modernization Status

Date: 2026-03-08

## Snapshot

Green:
- Pure Conan + CMake configure works with repo-owned presets
- `build/x86_64/Debug` configure, build, and `ctest` pass
- Boot handoff now carries framebuffer metadata
- i486 GRUB/Multiboot2 image generation works
- i486 QEMU boot smoke test passes
- i486 QEMU shell smoke test passes
- Advanced 32-bit lanes build and boot through the shared lane matrix
- x86_64 ISO boot and shell coverage pass again
- Hosted native `xash` builds in the main repo build
- Hosted `xash` smoke test passes
- Initial i486 userland syscall ABI scaffold compiles

Yellow:
- `XINIM_ENABLE_WERROR` is on, yet conversion/sign-conversion debt still has
  temporary no-error carve-outs
- x86_64 remains the only fully featured kernel architecture

Red:
- No active SVGA, VBE, or framebuffer console driver
- Legacy toolchain scripts still assume repo-local or `/opt` layout in places

## Build System

State:
- Conan and CMake are the canonical build entrypoints
- Each preset owns one build tree under `build/<lane>/<config>`
- `conanfile.py` emits lane-aware cache variables directly into the CMake cache
- `CMakePresets.json` points at per-lane generator paths under `build/`

Verified command set:

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

## Boot and QEMU

State:
- The current kernel is Limine-oriented and x86_64-first
- Raw QEMU `-kernel` boot is not valid for the current ELF
- The active QEMU launcher now prefers bootable images

What changed:
- Boot tests no longer claim to validate a guest boot from the raw ELF
- They require a boot image or skip cleanly
- The i486 lane now also validates an interactive shell over COM2

What is still missing:
- complete migration of older helper docs and non-canonical scripts

## Architecture Support

Verified:
- x86_64 host build and tests
- i486 bootstrap image generation and boot smoke test

Scaffold only:
- `cmake/i386.cmake`
- `conan/profiles/xinim-i386`

Implemented in the bootstrap lane:
- Multiboot2 32-bit adapter
- i486-safe kernel split
- Basic 486 capability reporting in the 32-bit lane
- true Ring 3 `xash` launch as PID 1 on COM2
- fault visibility for `#UD`, `#GP`, and `#PF`
- legacy PIC masking before entering Ring 3

Not yet implemented:
- 486SX/DX-specific floating-point policy beyond the current early capability
  report
- full 32-bit process management beyond the initial PID 1 shell
- a working zero-argument `getpid()` syscall path for the i486 shell ABI

## Native Tools

Implemented:
- `xinim_sh_host` is part of the main CMake build and emits `xash`
- `xinim_tools_hosted` is now the hosted-tools umbrella target
- the hosted build emits `xash` as the only public shell binary
- the repo has an initial i486 userland syscall ABI scaffold

Not implemented:
- Heirloom behavior capture matrix
- native C++ ports of Heirloom tools beyond the current `xash`
- Conan-backed hosted-tool package set for the future `xinim-tools` lane
- full POSIX.1-2008 shell behavior in the 32-bit Ring 3 lane

## Graphics

Implemented:
- Framebuffer metadata fields in `BootInfo`
- Limine shim plumbing for framebuffer handoff

Not implemented:
- Early linear framebuffer console
- VBE/VESA mode setup
- Cirrus/Bochs/virtio-gpu/VMware SVGA device drivers

## Warning Debt

Current status:
- The tree builds, but not with full warnings-as-errors discipline for all
  conversion classes

Known debt clusters:
- scheduler and service internals
- wait-graph and lock-manager indexing
- fano octonion math indexing
- Kyber and SHAKE code

Tracked in:
- [analysis/TODO_TRACKER.md](analysis/TODO_TRACKER.md)

## Near-Term Priorities

1. Add a reproducible x86_64 image-generation path under `XINIM_IMAGE_ROOT`.
2. Fix the current x86_64 Limine handoff so the generated ISO boots the kernel.
3. Burn down conversion/sign-conversion debt until `-Werror` is fully honest.
4. Add an early framebuffer/VGA-text console abstraction that the i486 lane can
   use beyond metadata reporting.
5. Harden the i486 user ABI beyond the current bootstrap shell, starting with
   `getpid`, `access`, and then `fork`/`execve`/`wait4`.
6. Stand up `xinim::tools::core`, define the `xash` contract, and start the
   Heirloom-to-C++ tool rebuild lane.
7. Migrate remaining legacy scripts, containers, and Python harnesses to the
   pure Conan + CMake build-tree model.
