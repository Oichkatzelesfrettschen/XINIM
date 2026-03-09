# i486 / QEMU / Portability Source Index

Date: 2026-03-08
Purpose: Primary-source index for the current 32-bit bring-up direction,
QEMU lane validation, and low-end x86 optimization work.

## Sources

1. QEMU direct Linux boot
   - URL: https://www.qemu.org/docs/master/system/linuxboot.html
   - Accessed: 2026-03-08
   - Why it matters: documents why the raw kernel ELF should not be treated as
     a Linux `-kernel` payload for this repo's current boot flow.

2. QEMU i440FX / `pc` machine model
   - URL: https://www.qemu.org/docs/master/system/i386/pc.html
   - Accessed: 2026-03-08
   - Why it matters: anchors the legacy PC machine model used by the current
     486-oriented bring-up lane.

3. QEMU CPU model documentation
   - URL: https://www.qemu.org/docs/master/system/qemu-cpu-models.html
   - Accessed: 2026-03-08
   - Why it matters: informs the staged CPU-lane validation across `486`,
     Pentium-class, and later 32-bit x86 models.

4. GNU GRUB `grub-mkrescue`
   - URL: https://www.gnu.org/software/grub/manual/grub/html_node/Invoking-grub_002dmkrescue.html
   - Accessed: 2026-03-08
   - Why it matters: anchors the current BIOS ISO generation backend for the
     32-bit lanes.

5. Agner Fog optimization manuals
   - URL: https://agner.org/optimize/
   - Accessed: 2026-03-08
   - Why it matters: reference set for 486/Pentium-safe code generation and
     later hotpath tuning.

## Local Evidence Anchors

- [CMakeLists.txt](/home/eirikr/Github/XINIM/CMakeLists.txt)
- [X86CpuLanes.cmake](/home/eirikr/Github/XINIM/cmake/X86CpuLanes.cmake)
- [BootstrapGrub.cmake](/home/eirikr/Github/XINIM/cmake/BootstrapGrub.cmake)
- [bootinfo.hpp](/home/eirikr/Github/XINIM/include/xinim/boot/bootinfo.hpp)
- [bootinfo.cpp](/home/eirikr/Github/XINIM/src/boot/multiboot2/bootinfo.cpp)
- [qemu_matrix.py](/home/eirikr/Github/XINIM/scripts/qemu_matrix.py)
- [x86_32_smoke_test.sh](/home/eirikr/Github/XINIM/test/boot/x86_32_smoke_test.sh)
- [x86_32_shell_test.py](/home/eirikr/Github/XINIM/test/boot/x86_32_shell_test.py)

## Claims Tracked By These Sources

- The 32-bit lane should validate through bootable images, not raw ELF launch.
- The 486 lane should stay anchored to QEMU `pc` hardware assumptions first.
- The CPU-lane matrix should grow upward from 486 through later 32-bit models
  without breaking the low-end baseline.
- Portability decisions should stay rooted in the preset-owned Conan + CMake
  flow rather than environment-script conventions.
