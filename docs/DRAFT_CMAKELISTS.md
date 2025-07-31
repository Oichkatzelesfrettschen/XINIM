# Draft CMake Build System Design for XINIM OS

This document outlines the planned CMake build system for XINIM OS, corresponding to the target directory structure defined in `docs/TARGET_DIRECTORY_STRUCTURE.md`.

## I. Top-Level `CMakeLists.txt` (Root of the project)

```cmake
cmake_minimum_required(VERSION 3.20) # Kernel/OS dev often needs recent CMake features
project(XINIM_OS VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Prefer explicit compiler flags over extensions

set(CMAKE_C_STANDARD 17) # Or C23 if toolchain fully supports
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# --- Global Compile Options & Definitions ---
add_compile_options(
    -Wall -Wextra -Wpedantic
    -fno-exceptions -fno-rtti # Essential for kernel/embedded
    -fno-strict-aliasing      # Important for low-level C/C++ interop
    -mno-red-zone             # For x86_64 kernel mode
    # -nostdlib -ffreestanding # Will be applied to kernel/LibOS targets specifically
)

# --- Architecture Configuration ---
if(NOT DEFINED XINIM_ARCH)
    set(XINIM_ARCH "x86_64" CACHE STRING "Target architecture (e.g., x86_64, arm64)")
endif()

message(STATUS "Target Architecture: ${XINIM_ARCH}")

if(XINIM_ARCH STREQUAL "x86_64")
    add_compile_options(-march=x86-64-v2) # Or v3/v4 if appropriate
    # Specific x86_64 flags
    set(CMAKE_ASM_COMPILER "nasm") # Or gas, depending on assembly syntax
    enable_language(ASM_NASM)
elseif(XINIM_ARCH STREQUAL "arm64")
    # Specific ARM64 flags
    # add_compile_options(...)
else()
    message(FATAL_ERROR "Unsupported architecture: ${XINIM_ARCH}")
endif()

# --- Build Type Configuration (Debug, Release, etc.) ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)")
endif()

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(-g -O2 -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_compile_options(-Os -DNDEBUG)
endif()

# --- Feature Options ---
option(XINIM_ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(XINIM_ENABLE_ASAN "Enable Address Sanitizer" OFF) # Might be tricky for kernel
option(XINIM_ENABLE_LTO "Enable Link-Time Optimization" ON)
option(XINIM_BUILD_TESTS "Build test suite" ON)
option(XINIM_BUILD_DOCS "Build documentation (Doxygen/Sphinx)" ON)

if(XINIM_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    # Link with sanitizer libraries if needed (target-specific)
endif()
if(XINIM_ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    # Link with sanitizer libraries if needed
endif()

if(XINIM_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# --- Output Directories ---
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Add Subdirectories (Order Matters for Dependencies) ---
# Tools might be needed by other parts (e.g., code generators)
add_subdirectory(TOOLS)

# Kernel is foundational
add_subdirectory(KERNEL)

# Libraries depend on Kernel APIs (potentially)
add_subdirectory(LIB)

# Servers depend on Kernel and Libraries
add_subdirectory(SERVERS)

# Userland applications depend on Libraries
add_subdirectory(USERLAND)

if(XINIM_BUILD_TESTS)
    enable_testing() # Enables CTest
    add_subdirectory(TESTS)
endif()

if(XINIM_BUILD_DOCS)
    # Placeholder for Doxygen/Sphinx integration
    # find_package(Doxygen)
    # if(DOXYGEN_FOUND)
    #     # ... configure Doxygen ...
    # endif()
    # find_package(Sphinx) # Requires FindSphinx.cmake
    # if(SPHINX_FOUND)
    #     # ... configure Sphinx ...
    # endif()
    message(STATUS "Documentation build would be configured here.")
endif()

# --- Limine Bootloader Integration (Example for x86_64) ---
if(XINIM_ARCH STREQUAL "x86_64")
    # Assuming Limine is a submodule or fetched via FetchContent
    # FetchContent_Declare(
    #     limine
    #     GIT_REPOSITORY https://github.com/limine-bootloader/limine.git
    #     GIT_TAG v4.x-branch-binary # Use appropriate tag
    # )
    # FetchContent_MakeAvailable(limine)

    # Custom target to build the bootable image
    add_custom_target(xinim_os_image ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Building XINIM OS Bootable Image..."
        # Actual commands would involve:
        # 1. Compiling the kernel (e.g., KERNEL/XINIM_KERNEL target)
        # 2. Preparing the filesystem/initrd if any
        # 3. Using limine-deploy to create the ISO or disk image
        #    e.g., limine-deploy ${CMAKE_BINARY_DIR}/xinim_os.iso
        DEPENDS XINIM_KERNEL # Depends on the kernel target
        COMMENT "Creating bootable XINIM OS image"
    )
    message(STATUS "Limine integration would be configured for x86_64 target.")
endif()

message(STATUS "XINIM OS CMake configuration complete.")

```

## II. Module-Level `CMakeLists.txt` (Examples)

### A. `KERNEL/CMakeLists.txt`

```cmake
# KERNEL/CMakeLists.txt
add_library(XINIM_KERNEL_LIB OBJECT # Compile sources as object files for linking into final kernel
    # CORE sources
    CORE/main.cpp
    CORE/pmm.cpp
    CORE/vmm.cpp
    CORE/scheduler.cpp
    CORE/process.cpp
    CORE/syscall.cpp
    CORE/timer.cpp
    # Add other CORE sources...

    # IPC sources
    IPC/lattice_ipc.cpp
    IPC/wormhole.cpp
    IPC/pqcrypto.cpp
    # Add other IPC sources...

    # ARCH specific sources (conditionally)
    # Example for x86_64
    # ARCH/X86_64/boot/boot.S # Handled separately for linking
    # ARCH/X86_64/cpu/apic.cpp
    # ARCH/X86_64/mm/paging.cpp
)
target_compile_options(XINIM_KERNEL_LIB PRIVATE
    -nostdlib -ffreestanding
    # -fno-stack-protector # Often for kernels
    # -mgeneral-regs-only # If not using FPU/SIMD in kernel
)

target_include_directories(XINIM_KERNEL_LIB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/INCLUDE # For XINIM/kernel_api.hpp etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/CORE/include
    ${CMAKE_CURRENT_SOURCE_DIR}/IPC/include/public
    # Add other public include paths from kernel modules
)
target_include_directories(XINIM_KERNEL_LIB PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/IPC/include # Private IPC headers
    # Arch specific private headers
    ${CMAKE_CURRENT_SOURCE_DIR}/ARCH/${XINIM_ARCH}
)

# --- Algebraic Libraries ---
add_subdirectory(IPC/ALGEBRAIC) # This will build libmathalgebras
target_link_libraries(XINIM_KERNEL_LIB PRIVATE math_algebras)

# --- Final Kernel Executable ---
# This target links the kernel library objects with arch-specific boot code and linker script.
# The exact linking process is highly target-dependent.
# For x86_64 with Limine, Limine expects a standard ELF kernel.

add_executable(XINIM_KERNEL
    ARCH/${XINIM_ARCH}/boot/boot.S # Assembly entry point
    $<TARGET_OBJECTS:XINIM_KERNEL_LIB>
)

target_link_options(XINIM_KERNEL PRIVATE
    # Path to linker script
    "-T${CMAKE_CURRENT_SOURCE_DIR}/ARCH/${XINIM_ARCH}/boot/linker.ld"
    # Other linker flags like -nostdlib, -Z GAPS_OK (for ld), etc.
    -nostdlib
    -Wl,--gc-sections # Remove unused sections
)

# If math_algebras is a static library, it will be linked.
# If it's an INTERFACE library, its properties are inherited.
target_link_libraries(XINIM_KERNEL PRIVATE math_algebras)

# Kernel installation (e.g., to a specific directory for bootloader)
# install(TARGETS XINIM_KERNEL DESTINATION boot)

```

### B. `KERNEL/IPC/ALGEBRAIC/CMakeLists.txt`

```cmake
# KERNEL/IPC/ALGEBRAIC/CMakeLists.txt
add_library(math_algebras STATIC # Or INTERFACE if header-only, or OBJECT
    quaternion/quaternion.cpp
    octonion/octonion.cpp
    sedenion/sedenion.cpp
)
target_compile_options(math_algebras PRIVATE
    # Enable SIMD for performance if applicable
    # -mavx2 -mfma # Example for x86_64
)
target_include_directories(math_algebras PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Export this library for other modules to use
# install(TARGETS math_algebras EXPORT MathAlgebrasTarget ...)
# install(EXPORT MathAlgebrasTarget NAMESPACE XINIM:: ...)
```

### C. `LIB/LIBC/CMakeLists.txt`

```cmake
# LIB/LIBC/CMakeLists.txt
add_library(xinim_libc STATIC # Typically static for OS lib
    # Source files for libc (stdio, string, malloc, etc.)
    src/stdio/printf.c
    src/string/memcpy.c
    src/malloc/umm_libc_glue.c # Glue for UMM
    # ... other libc sources ...

    # System call wrappers
    sysdeps/xinim/syscall_wrappers.c
)
target_compile_options(xinim_libc PRIVATE
    -ffreestanding # Libc might be freestanding if it doesn't link against another libc
    # -nostdinc # If providing all standard headers
)
target_include_directories(xinim_libc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include # Public libc headers (stdio.h, string.h)
)
target_include_directories(xinim_libc PRIVATE
    # Private libc headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
    # Kernel API headers for syscall wrappers
    ${CMAKE_SOURCE_DIR}/KERNEL/INCLUDE
)
# Link against kernel API if needed for syscall numbers or types (unlikely direct link)
# target_link_libraries(xinim_libc INTERFACE XINIM::KernelAPI) # If KernelAPI is an interface library
```

## III. Design Notes

*   **Modularity:** Each component (Kernel, Libc, Servers, Algebraic libs) is a separate CMake target. This allows for better dependency management and parallel builds.
*   **Kernel as Object Library:** `XINIM_KERNEL_LIB` is an OBJECT library. This compiles all kernel C/C++ files into object files, which are then linked by the `XINIM_KERNEL` executable target along with assembly startup code and the linker script. This is a common pattern for kernels.
*   **Architecture Specifics:** Architecture-specific source files and compile flags are handled using conditional logic based on `XINIM_ARCH`.
*   **Freestanding Environment:** Kernel and potentially Libc are compiled in a freestanding environment (`-ffreestanding`, `-nostdlib`).
*   **Feature Toggles:** CMake `option()` provides easy toggles for features like sanitizers, LTO, and building tests/docs.
*   **Limine Integration:** A placeholder for Limine integration shows how a custom target can be used to produce the final bootable OS image. This would involve using Limine's deployment tools.
*   **Testing:** `enable_testing()` and `add_subdirectory(TESTS)` integrate with CTest for running unit and integration tests.
*   **Installation:** `install()` commands (commented out) would be used to install kernel, libraries, and headers to a staging directory or system root if creating a distributable OS image.
*   **Dependencies:** `target_link_libraries` is used to manage dependencies between targets (e.g., Kernel linking against `math_algebras`).

This draft provides a solid foundation for the XINIM OS build system. The actual implementation will involve refining file lists, include paths, and linker script details.
