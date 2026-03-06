# CMake toolchain file for Xinim i386 target.
#
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/i386.cmake -B build/i386
#
# This is a stub for v1.4.0. The kernel core, crypto, and VFS compile
# for i386. Boot assembly and GDT/IDT for i386 are deferred to v1.5.0.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

# Target 32-bit x86
set(CMAKE_C_FLAGS_INIT "-m32 -march=i686")
set(CMAKE_CXX_FLAGS_INIT "-m32 -march=i686 -stdlib=libc++")
set(CMAKE_ASM_FLAGS_INIT "-m32")

# Override the architecture define
add_compile_definitions(XINIM_ARCH_I386)
# Do NOT define XINIM_ARCH_X86_64 -- the main CMakeLists.txt must be
# updated to conditionally define this based on CMAKE_SYSTEM_PROCESSOR.
