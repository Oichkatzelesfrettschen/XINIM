# Build and Orchestration Source Index

Date: 2026-03-08
Purpose: Primary-source index for the canonical Conan + CMake build flow, the
repo-local boot-tool bootstrap path, and the current image-generation
contracts.

## Sources

1. Conan `CMakeToolchain`
   - URL: https://docs.conan.io/2/reference/tools/cmake/cmaketoolchain.html
   - Accessed: 2026-03-08
   - Why it matters: anchors the Conan-to-CMake integration used by the active
     presets.

2. Conan `cmake_layout()`
   - URL: https://docs.conan.io/2/reference/tools/cmake/cmake_layout.html
   - Accessed: 2026-03-08
   - Why it matters: informed the build-tree layout decisions now expressed as
     `build/<lane>/<config>`.

3. CMake presets manual
   - URL: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
   - Accessed: 2026-03-08
   - Why it matters: governs the repo-owned preset interface for configure,
     build, and test.

4. CMake `add_test`
   - URL: https://cmake.org/cmake/help/latest/command/add_test.html
   - Accessed: 2026-03-08
   - Why it matters: defines the CTest registration model used by the boot and
     shell harnesses.

5. GNU GRUB `grub-mkrescue`
   - URL: https://www.gnu.org/software/grub/manual/grub/html_node/Invoking-grub_002dmkrescue.html
   - Accessed: 2026-03-08
   - Why it matters: documents the rescue-image backend used for the 32-bit
     ISO lanes and the `--xorriso` override we now pass explicitly.

6. GNU GRUB source releases
   - URL: https://ftp.gnu.org/gnu/grub/
   - Accessed: 2026-03-08
   - Why it matters: this is the upstream archive source used by the new
     repo-local GRUB bootstrap.

7. GNU Multiboot2 specification
   - URL: https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
   - Accessed: 2026-03-08
   - Why it matters: defines the boot-information format consumed by the
     current 32-bit boot adapter.

## Local Evidence Anchors

- [conanfile.py](/home/eirikr/Github/XINIM/conanfile.py)
- [CMakePresets.json](/home/eirikr/Github/XINIM/CMakePresets.json)
- [CMakeLists.txt](/home/eirikr/Github/XINIM/CMakeLists.txt)
- [BootstrapGrub.cmake](/home/eirikr/Github/XINIM/cmake/BootstrapGrub.cmake)
- [BootstrapLimine.cmake](/home/eirikr/Github/XINIM/cmake/BootstrapLimine.cmake)
- [BUILD.md](/home/eirikr/Github/XINIM/docs/BUILD.md)
- [prepare_boot_image.sh](/home/eirikr/Github/XINIM/test/boot/prepare_boot_image.sh)

## Claims Tracked By These Sources

- The canonical build front door is `conan install` plus CMake presets.
- Bootstrapped tools belong under the active build tree, not under legacy
  external-sysroot layouts or repo-root ad hoc paths.
- GRUB/Multiboot2 remains the current 32-bit boot path.
- Limine remains the current x86_64 boot path.
- `xorriso` is a host prerequisite, but `grub-mkrescue` no longer has to be.
