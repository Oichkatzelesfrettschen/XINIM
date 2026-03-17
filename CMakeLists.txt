# XINIM - Modern C++23 Post-Quantum Microkernel Operating System
# CMake Build Configuration (Refined for Bare-Metal Baseline)

cmake_minimum_required(VERSION 3.28)

set(XINIM_CPU_LANE "${XINIM_CPU_LANE}" CACHE STRING
    "Active XINIM CPU lane for this build tree")
if(NOT XINIM_CPU_LANE)
    set(XINIM_CPU_LANE "x86_64" CACHE STRING
        "Active XINIM CPU lane for this build tree" FORCE)
endif()

set(XINIM_X86_32_TOOLCHAIN_MODE "${XINIM_X86_32_TOOLCHAIN_MODE}" CACHE STRING
    "32-bit lane compiler mode: clang-m32 or cross-elf")
if(NOT XINIM_X86_32_TOOLCHAIN_MODE)
    set(XINIM_X86_32_TOOLCHAIN_MODE "clang-m32" CACHE STRING
        "32-bit lane compiler mode: clang-m32 or cross-elf" FORCE)
endif()

set(XINIM_X86_ELF_TOOLCHAIN_TRIPLE "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}" CACHE STRING
    "Optional ELF cross-toolchain triple for 32-bit cross-elf builds")
set(XINIM_X86_ELF_TOOLCHAIN_ROOT "${XINIM_X86_ELF_TOOLCHAIN_ROOT}" CACHE PATH
    "Optional root or bin directory containing <triple>-gcc style cross tools")
set(XINIM_VFS_PROFILE "${XINIM_VFS_PROFILE}" CACHE STRING
    "Bare-metal VFS resident-memory profile: auto, default, or tiny")
if(NOT XINIM_VFS_PROFILE)
    set(XINIM_VFS_PROFILE "auto" CACHE STRING
        "Bare-metal VFS resident-memory profile: auto, default, or tiny" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(X86CpuLanes)

set_property(CACHE XINIM_X86_32_TOOLCHAIN_MODE PROPERTY STRINGS clang-m32 cross-elf)
set_property(CACHE XINIM_VFS_PROFILE PROPERTY STRINGS auto default tiny)
if(NOT XINIM_X86_32_TOOLCHAIN_MODE STREQUAL "clang-m32" AND
   NOT XINIM_X86_32_TOOLCHAIN_MODE STREQUAL "cross-elf")
    message(FATAL_ERROR
        "Unknown XINIM_X86_32_TOOLCHAIN_MODE='${XINIM_X86_32_TOOLCHAIN_MODE}'. "
        "Expected one of: clang-m32, cross-elf.")
endif()
if(NOT XINIM_VFS_PROFILE STREQUAL "auto" AND
   NOT XINIM_VFS_PROFILE STREQUAL "default" AND
   NOT XINIM_VFS_PROFILE STREQUAL "tiny")
    message(FATAL_ERROR
        "Unknown XINIM_VFS_PROFILE='${XINIM_VFS_PROFILE}'. "
        "Expected one of: auto, default, tiny.")
endif()

if(XINIM_VFS_PROFILE STREQUAL "auto")
    if(XINIM_CPU_LANE STREQUAL "x86_64")
        set(XINIM_EFFECTIVE_VFS_PROFILE "default")
    else()
        set(XINIM_EFFECTIVE_VFS_PROFILE "tiny")
    endif()
else()
    set(XINIM_EFFECTIVE_VFS_PROFILE "${XINIM_VFS_PROFILE}")
endif()

if(DEFINED XINIM_CPU_LANE AND
   NOT XINIM_CPU_LANE STREQUAL "" AND
   NOT XINIM_CPU_LANE STREQUAL "x86_64")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

if(NOT XINIM_CPU_LANE STREQUAL "x86_64" AND
   XINIM_X86_32_TOOLCHAIN_MODE STREQUAL "cross-elf")
    xinim_get_x86_lane_property(${XINIM_CPU_LANE} family _xinim_preproject_lane_family)
    if(NOT _xinim_preproject_lane_family STREQUAL "x86_32")
        message(FATAL_ERROR
            "XINIM_X86_32_TOOLCHAIN_MODE=cross-elf is only valid for 32-bit lanes.")
    endif()

    # Keep host distro optimization flags and libc++ selection from leaking into
    # freestanding ELF cross-compiler detection.
    set(ENV{CFLAGS} "")
    set(ENV{CXXFLAGS} "")
    set(ENV{LDFLAGS} "")
    set(CMAKE_C_FLAGS_INIT "" CACHE STRING "Initial C flags for XINIM cross-elf mode" FORCE)
    set(CMAKE_CXX_FLAGS_INIT "" CACHE STRING "Initial CXX flags for XINIM cross-elf mode" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "" CACHE STRING
        "Initial executable linker flags for XINIM cross-elf mode" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "" CACHE STRING
        "Initial shared linker flags for XINIM cross-elf mode" FORCE)

    xinim_get_x86_lane_property(${XINIM_CPU_LANE} cross_elf_triple _xinim_default_cross_triple)
    if(NOT XINIM_X86_ELF_TOOLCHAIN_TRIPLE)
        set(XINIM_X86_ELF_TOOLCHAIN_TRIPLE "${_xinim_default_cross_triple}" CACHE STRING
            "Optional ELF cross-toolchain triple for 32-bit cross-elf builds" FORCE)
    endif()

    set(_xinim_cross_search_paths)
    if(XINIM_X86_ELF_TOOLCHAIN_ROOT)
        list(APPEND _xinim_cross_search_paths
            "${XINIM_X86_ELF_TOOLCHAIN_ROOT}"
            "${XINIM_X86_ELF_TOOLCHAIN_ROOT}/bin")
    endif()

    macro(_xinim_find_cross_tool out_var tool_name)
        unset(${out_var})
        unset(${out_var} CACHE)
        if(_xinim_cross_search_paths)
            find_program(${out_var}
                NAMES "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-${tool_name}"
                PATHS ${_xinim_cross_search_paths}
                NO_DEFAULT_PATH
            )
        endif()
        if(NOT ${out_var})
            find_program(${out_var}
                NAMES "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-${tool_name}"
            )
        endif()
    endmacro()

    _xinim_find_cross_tool(_xinim_cross_cc gcc)
    _xinim_find_cross_tool(_xinim_cross_cxx g++)
    _xinim_find_cross_tool(_xinim_cross_ar ar)
    _xinim_find_cross_tool(_xinim_cross_as as)
    _xinim_find_cross_tool(_xinim_cross_ld ld)
    _xinim_find_cross_tool(_xinim_cross_ranlib ranlib)
    _xinim_find_cross_tool(_xinim_cross_objcopy objcopy)
    _xinim_find_cross_tool(_xinim_cross_objdump objdump)
    _xinim_find_cross_tool(_xinim_cross_strip strip)

    set(_xinim_missing_cross_tools "")
    if(NOT _xinim_cross_cc)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-gcc")
    endif()
    if(NOT _xinim_cross_cxx)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-g++")
    endif()
    if(NOT _xinim_cross_ar)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-ar")
    endif()
    if(NOT _xinim_cross_as)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-as")
    endif()
    if(NOT _xinim_cross_ld)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-ld")
    endif()
    if(NOT _xinim_cross_ranlib)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-ranlib")
    endif()
    if(NOT _xinim_cross_objcopy)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-objcopy")
    endif()
    if(NOT _xinim_cross_objdump)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-objdump")
    endif()
    if(NOT _xinim_cross_strip)
        list(APPEND _xinim_missing_cross_tools "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}-strip")
    endif()

    if(_xinim_missing_cross_tools)
        string(JOIN ", " _xinim_missing_cross_tools_message ${_xinim_missing_cross_tools})
        message(FATAL_ERROR
            "Unable to enable the optional ELF cross-toolchain lane for ${XINIM_CPU_LANE}. "
            "Missing tools: ${_xinim_missing_cross_tools_message}. "
            "Install the matching ${XINIM_X86_ELF_TOOLCHAIN_TRIPLE} GCC/binutils package set, "
            "set XINIM_X86_ELF_TOOLCHAIN_ROOT to its install prefix or bin directory, "
            "or switch back to -DXINIM_X86_32_TOOLCHAIN_MODE=clang-m32.")
    endif()

    set(CMAKE_SYSTEM_NAME Generic CACHE STRING "Target system name for ELF cross builds" FORCE)
    set(CMAKE_SYSTEM_PROCESSOR "${XINIM_CPU_LANE}" CACHE STRING
        "Target processor for ELF cross builds" FORCE)
    set(CMAKE_C_COMPILER "${_xinim_cross_cc}" CACHE FILEPATH
        "ELF cross C compiler" FORCE)
    set(CMAKE_CXX_COMPILER "${_xinim_cross_cxx}" CACHE FILEPATH
        "ELF cross C++ compiler" FORCE)
    set(CMAKE_ASM_COMPILER "${_xinim_cross_cc}" CACHE FILEPATH
        "ELF cross assembler compiler driver" FORCE)
    set(CMAKE_AR "${_xinim_cross_ar}" CACHE FILEPATH "ELF cross archiver" FORCE)
    set(CMAKE_LINKER "${_xinim_cross_ld}" CACHE FILEPATH "ELF cross linker" FORCE)
    set(CMAKE_RANLIB "${_xinim_cross_ranlib}" CACHE FILEPATH "ELF cross ranlib" FORCE)
    set(CMAKE_OBJCOPY "${_xinim_cross_objcopy}" CACHE FILEPATH "ELF cross objcopy" FORCE)
    set(CMAKE_OBJDUMP "${_xinim_cross_objdump}" CACHE FILEPATH "ELF cross objdump" FORCE)
    set(CMAKE_STRIP "${_xinim_cross_strip}" CACHE FILEPATH "ELF cross strip" FORCE)

    message(STATUS
        "XINIM optional 32-bit ELF cross-toolchain mode enabled for ${XINIM_CPU_LANE}: "
        "${XINIM_X86_ELF_TOOLCHAIN_TRIPLE} via ${_xinim_cross_cc}")
endif()

project(XINIM VERSION 1.0.0 LANGUAGES C CXX ASM)

# =============================================================================
# Pure Conan + CMake Layout
# =============================================================================

set(XINIM_ARTIFACT_ROOT "${XINIM_ARTIFACT_ROOT}" CACHE PATH
    "Root directory for build-tree-local XINIM artifacts")
if(NOT XINIM_ARTIFACT_ROOT)
    set(XINIM_ARTIFACT_ROOT "${CMAKE_BINARY_DIR}" CACHE PATH
        "Root directory for build-tree-local XINIM artifacts" FORCE)
endif()

set(_XINIM_LEGACY_ARTIFACT_ROOT "${CMAKE_SOURCE_DIR}/.xinim")
function(xinim_rewrite_legacy_artifact_path cache_var default_value cache_type doc_string)
    string(FIND "${${cache_var}}" "${_XINIM_LEGACY_ARTIFACT_ROOT}" _xinim_legacy_index)
    if(_xinim_legacy_index EQUAL 0)
        set(${cache_var} "${default_value}" CACHE ${cache_type} "${doc_string}" FORCE)
    endif()
endfunction()

set(XINIM_BUILD_ROOT "${XINIM_BUILD_ROOT}" CACHE PATH
    "Compatibility alias for the active build tree")
if(NOT XINIM_BUILD_ROOT)
    set(XINIM_BUILD_ROOT "${CMAKE_BINARY_DIR}" CACHE PATH
        "Compatibility alias for the active build tree" FORCE)
endif()
xinim_rewrite_legacy_artifact_path(
    XINIM_BUILD_ROOT
    "${CMAKE_BINARY_DIR}"
    PATH
    "Compatibility alias for the active build tree")

set(XINIM_IMAGE_ROOT "${XINIM_IMAGE_ROOT}" CACHE PATH
    "Boot image output root")
if(NOT XINIM_IMAGE_ROOT)
    set(XINIM_IMAGE_ROOT "${XINIM_ARTIFACT_ROOT}/images" CACHE PATH
        "Boot image output root" FORCE)
endif()
xinim_rewrite_legacy_artifact_path(
    XINIM_IMAGE_ROOT
    "${XINIM_ARTIFACT_ROOT}/images"
    PATH
    "Boot image output root")

set(XINIM_LOG_ROOT "${XINIM_LOG_ROOT}" CACHE PATH
    "Log output root")
if(NOT XINIM_LOG_ROOT)
    set(XINIM_LOG_ROOT "${XINIM_ARTIFACT_ROOT}/logs" CACHE PATH
        "Log output root" FORCE)
endif()
xinim_rewrite_legacy_artifact_path(
    XINIM_LOG_ROOT
    "${XINIM_ARTIFACT_ROOT}/logs"
    PATH
    "Log output root")

set(XINIM_TOOLS_ROOT "${XINIM_TOOLS_ROOT}" CACHE PATH
    "Tool bootstrap root")
if(NOT XINIM_TOOLS_ROOT)
    set(XINIM_TOOLS_ROOT "${XINIM_ARTIFACT_ROOT}/tools" CACHE PATH
        "Tool bootstrap root" FORCE)
endif()
xinim_rewrite_legacy_artifact_path(
    XINIM_TOOLS_ROOT
    "${XINIM_ARTIFACT_ROOT}/tools"
    PATH
    "Tool bootstrap root")

set(XINIM_STATE_ROOT "${XINIM_ARTIFACT_ROOT}" CACHE PATH
    "Compatibility alias for the artifact root" FORCE)
set(XINIM_PROJECT_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH
    "Source tree root" FORCE)

message(STATUS "XINIM active lane: ${XINIM_CPU_LANE}")
message(STATUS "XINIM x86_32 toolchain mode: ${XINIM_X86_32_TOOLCHAIN_MODE}")
message(STATUS "XINIM VFS profile: requested=${XINIM_VFS_PROFILE} effective=${XINIM_EFFECTIVE_VFS_PROFILE}")
if(XINIM_X86_ELF_TOOLCHAIN_TRIPLE)
    message(STATUS "XINIM x86 ELF toolchain triple: ${XINIM_X86_ELF_TOOLCHAIN_TRIPLE}")
endif()
message(STATUS "XINIM artifact root: ${XINIM_ARTIFACT_ROOT}")
message(STATUS "XINIM build root: ${XINIM_BUILD_ROOT}")
message(STATUS "XINIM image root: ${XINIM_IMAGE_ROOT}")
message(STATUS "XINIM log root: ${XINIM_LOG_ROOT}")
message(STATUS "XINIM tools root: ${XINIM_TOOLS_ROOT}")

# =============================================================================
# Project Options and Compiler Warnings (from cmake/ modules)
# =============================================================================

include(ProjectOptions)
include(CompilerWarnings)
include(X86CpuLanes)

xinim_get_all_x86_lane_names(_xinim_all_x86_lanes)
set_property(CACHE XINIM_CPU_LANE PROPERTY STRINGS ${_xinim_all_x86_lanes})
list(FIND _xinim_all_x86_lanes "${XINIM_CPU_LANE}" _xinim_lane_index)
if(_xinim_lane_index EQUAL -1)
    message(FATAL_ERROR
        "Unknown XINIM_CPU_LANE='${XINIM_CPU_LANE}'. "
        "Run 'conan install' with -o '&:lane=<lane>' for one of: ${_xinim_all_x86_lanes}.")
endif()

xinim_get_x86_lane_property(${XINIM_CPU_LANE} family XINIM_ACTIVE_LANE_FAMILY)
xinim_get_x86_lane_property(${XINIM_CPU_LANE} target_arch XINIM_ACTIVE_TARGET_ARCH)

if(XINIM_ACTIVE_LANE_FAMILY STREQUAL "x86_64" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "Lane ${XINIM_CPU_LANE} requires a 64-bit Conan host profile. "
        "Run 'conan install' with -pr:h=conan/profiles/clang-x86_64'.")
endif()
if(XINIM_ACTIVE_LANE_FAMILY STREQUAL "x86_32" AND
   XINIM_X86_32_TOOLCHAIN_MODE STREQUAL "clang-m32" AND
   NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Lane ${XINIM_CPU_LANE} requires a 32-bit Conan host profile. "
        "Run 'conan install' with -pr:h=conan/profiles/clang-x86_32'.")
endif()

add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

# =============================================================================
# Testing Infrastructure
# =============================================================================

enable_testing()

# =============================================================================
# Include Directories
# =============================================================================

# Note: global include_directories() removed. Includes are set per-target via
# xinim_apply_target_defaults() in cmake/ProjectOptions.cmake and explicit
# target_include_directories() calls in each test target.

# =============================================================================
# Bootstrapped Tooling
# =============================================================================

set(XINIM_GRUB_VERSION "2.14" CACHE STRING
    "Pinned GNU GRUB release for repo-local grub-mkrescue bootstrapping")
set(XINIM_GRUB_ARCHIVE_SHA256
    "bc8d3c73535b8838d8c8e2654d73edc4e6ae8c8acdb45d5df5dc9a1547446d43"
    CACHE STRING
    "SHA256 for the pinned GNU GRUB source archive")
set(XINIM_GRUB_ROOT "${XINIM_TOOLS_ROOT}/grub/${XINIM_GRUB_VERSION}" CACHE PATH
    "Directory containing bootstrapped GRUB sources and install tree")
xinim_rewrite_legacy_artifact_path(
    XINIM_GRUB_ROOT
    "${XINIM_TOOLS_ROOT}/grub/${XINIM_GRUB_VERSION}"
    PATH
    "Directory containing bootstrapped GRUB sources and install tree")
set(XINIM_GRUB_SOURCE_DIR
    "${XINIM_GRUB_ROOT}/source/grub-${XINIM_GRUB_VERSION}" CACHE PATH
    "Extracted GNU GRUB source directory")
xinim_rewrite_legacy_artifact_path(
    XINIM_GRUB_SOURCE_DIR
    "${XINIM_GRUB_ROOT}/source/grub-${XINIM_GRUB_VERSION}"
    PATH
    "Extracted GNU GRUB source directory")
set(XINIM_GRUB_BUILD_DIR "${XINIM_GRUB_ROOT}/build" CACHE PATH
    "Out-of-source GNU GRUB build directory")
xinim_rewrite_legacy_artifact_path(
    XINIM_GRUB_BUILD_DIR
    "${XINIM_GRUB_ROOT}/build"
    PATH
    "Out-of-source GNU GRUB build directory")
set(XINIM_GRUB_INSTALL_ROOT "${XINIM_GRUB_ROOT}/install" CACHE PATH
    "Repo-local GNU GRUB install prefix")
xinim_rewrite_legacy_artifact_path(
    XINIM_GRUB_INSTALL_ROOT
    "${XINIM_GRUB_ROOT}/install"
    PATH
    "Repo-local GNU GRUB install prefix")
set(XINIM_GRUB_BOOTSTRAP_EXECUTABLE
    "${XINIM_GRUB_INSTALL_ROOT}/bin/grub-mkrescue" CACHE FILEPATH
    "Canonical repo-local grub-mkrescue path")
xinim_rewrite_legacy_artifact_path(
    XINIM_GRUB_BOOTSTRAP_EXECUTABLE
    "${XINIM_GRUB_INSTALL_ROOT}/bin/grub-mkrescue"
    FILEPATH
    "Canonical repo-local grub-mkrescue path")
set(XINIM_GRUB_MKRESCUE_EXECUTABLE "" CACHE FILEPATH
    "Optional override for grub-mkrescue; leave empty to use the repo-local bootstrap")
set(XINIM_XORRISO_EXECUTABLE "" CACHE FILEPATH
    "Path to xorriso used by GRUB and Limine image generation")

if(XINIM_GRUB_MKRESCUE_EXECUTABLE AND
   NOT EXISTS "${XINIM_GRUB_MKRESCUE_EXECUTABLE}")
    message(FATAL_ERROR
        "XINIM_GRUB_MKRESCUE_EXECUTABLE points to a missing file: "
        "${XINIM_GRUB_MKRESCUE_EXECUTABLE}")
endif()

if(XINIM_XORRISO_EXECUTABLE AND
   NOT EXISTS "${XINIM_XORRISO_EXECUTABLE}")
    message(FATAL_ERROR
        "XINIM_XORRISO_EXECUTABLE points to a missing file: "
        "${XINIM_XORRISO_EXECUTABLE}")
endif()

find_program(XINIM_HOST_GRUB_MKRESCUE_EXECUTABLE grub-mkrescue)
find_program(_xinim_host_xorriso xorriso)
if(NOT XINIM_XORRISO_EXECUTABLE AND _xinim_host_xorriso)
    set(XINIM_XORRISO_EXECUTABLE "${_xinim_host_xorriso}" CACHE FILEPATH
        "Path to xorriso used by GRUB and Limine image generation" FORCE)
endif()

if(XINIM_GRUB_MKRESCUE_EXECUTABLE)
    set(XINIM_RESOLVED_GRUB_MKRESCUE_EXECUTABLE
        "${XINIM_GRUB_MKRESCUE_EXECUTABLE}")
else()
    set(XINIM_RESOLVED_GRUB_MKRESCUE_EXECUTABLE
        "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}")
endif()

if(XINIM_GRUB_MKRESCUE_EXECUTABLE)
    set(_xinim_grub_bootstrap_dependencies "")
elseif(EXISTS "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}")
    set(_xinim_grub_bootstrap_dependencies "")
elseif(XINIM_HOST_GRUB_MKRESCUE_EXECUTABLE)
    message(STATUS
        "Host grub-mkrescue detected at ${XINIM_HOST_GRUB_MKRESCUE_EXECUTABLE}, "
        "but the canonical path remains the repo-local bootstrap at "
        "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}.")
    set(_xinim_grub_bootstrap_dependencies "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}")
else()
    set(_xinim_grub_bootstrap_dependencies "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}")
endif()

if(NOT XINIM_GRUB_MKRESCUE_EXECUTABLE)
    add_custom_command(
        OUTPUT "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}"
        COMMAND ${CMAKE_COMMAND}
                -DXINIM_GRUB_VERSION=${XINIM_GRUB_VERSION}
                -DXINIM_GRUB_ARCHIVE_SHA256=${XINIM_GRUB_ARCHIVE_SHA256}
                -DXINIM_GRUB_ROOT=${XINIM_GRUB_ROOT}
                -DXINIM_GRUB_SOURCE_DIR=${XINIM_GRUB_SOURCE_DIR}
                -DXINIM_GRUB_BUILD_DIR=${XINIM_GRUB_BUILD_DIR}
                -DXINIM_GRUB_INSTALL_ROOT=${XINIM_GRUB_INSTALL_ROOT}
                -P ${CMAKE_SOURCE_DIR}/cmake/BootstrapGrub.cmake
        COMMENT "Bootstrapping GNU GRUB into the build-tree tools cache"
        VERBATIM
        USES_TERMINAL
    )
endif()

add_custom_target(xinim_bootstrap_grub
    DEPENDS ${_xinim_grub_bootstrap_dependencies}
)

add_custom_target(xinim_bootstrap_tools)
add_dependencies(xinim_bootstrap_tools xinim_bootstrap_grub)

# =============================================================================
# Main XINIM Kernel Target (Bare-Metal Baseline)
# =============================================================================

if(XINIM_CPU_LANE STREQUAL "x86_64")
add_executable(xinim
    src/kernel/main.cpp
    src/kernel/kernel.cpp
    src/kernel/system.cpp
    src/kernel/syscall.cpp
    src/kernel/proc.cpp
    src/kernel/schedule.cpp
    src/kernel/service.cpp
    src/kernel/tty.cpp
    src/kernel/panic.cpp
    src/boot/limine/shim.cpp
    src/kernel/klib64.cpp
    src/kernel/heap.cpp
    src/kernel/mpx64_stubs.S
    src/kernel/exception_stubs.cpp
    src/kernel/signal_stubs.cpp
    src/kernel/time_stubs.cpp
    src/kernel/wait_graph.cpp
    src/kernel/lock_manager.cpp
    src/kernel/capability_mutex.cpp
    src/kernel/table.cpp
    src/kernel/clock.cpp
    src/kernel/dmp.cpp
    # idt64.cpp removed: superseded by unified path in interrupts.cpp
    src/kernel/interrupts.cpp
    src/kernel/irq.cpp
    src/kernel/memory.cpp
    src/kernel/paging.cpp
    src/mm/alloc.cpp
    src/kernel/scheduler.cpp
    src/kernel/acpi/acpi.cpp
    src/kernel/time/calibrate.cpp
    src/kernel/timer.cpp
    src/kernel/server_spawn.cpp
    src/hal/x86_64/hal/apic.cpp
    src/hal/x86_64/hal/hpet.cpp
    src/hal/x86_64/hal/ioapic.cpp
    src/kernel/printer.cpp
    src/kernel/wini.cpp
    src/kernel/floppy.cpp
    src/kernel/sys/dispatch.cpp
    src/kernel/arch/x86_64/gdt.cpp
    src/kernel/arch/x86_64/idt.cpp
    src/kernel/arch/x86_64/tss.cpp
    src/kernel/arch/x86_64/stub.cpp
    src/kernel/arch/x86_64/syscall_init.cpp
    src/kernel/arch/x86_64/isr.S
    src/arch/x86_64/gdt_load.S
    src/arch/x86_64/tss_load.S
    src/arch/x86_64/syscall_handler.S
    src/arch/x86_64/context_switch.S
    src/arch/x86_64/interrupts.S
    src/kernel/unified_scheduler.cpp
    src/kernel/process_lifecycle.cpp
    src/kernel/lattice_ipc.cpp
    src/kernel/abi/lattice_changer.cpp
    src/kernel/early/serial_16550.cpp
    src/kernel/bare_metal_stubs.cpp
    src/kernel/i486/bootfs.cpp
    src/kernel/x86_64/i486_runtime_compat.cpp
    src/hal/hal.cpp
    # Bare-metal VFS subsystem (ADR-0009 / v1.3.0)
    src/vfs/core_init.cpp
    src/vfs/inode_table.cpp
    src/vfs/vnode_table.cpp
    src/vfs/dirent.cpp
    src/vfs/path_walk.cpp
    src/vfs/seed.cpp
    src/vfs/bootfs_promote.cpp
    src/vfs/ramfs_ops.cpp
    src/vfs/mount_table.cpp
    src/vfs/fd_table.cpp
    src/vfs/buffer_cache.cpp
    src/vfs/vfs_server.cpp
    # PCI bus enumeration (freestanding, no STL)
    src/pci/pci.cpp
    src/hal/x86_64/hal/pci.cpp
    # Drivers
    src/drivers/net/virtio_net.cpp
    # Hardware-accelerated crypto (AES-NI, SHA-NI with software fallback)
    src/crypto/aes_hw.cpp
    src/crypto/sha2_hw.cpp
    # Recovery server (Resurrection Server -- DAG-based service restart)
    src/kernel/recovery/recovery_dag.cpp
    src/kernel/x86_64/staged_xash.cpp
)

set_target_properties(xinim PROPERTIES XINIM_TARGET_ARCH x86_64)

# Apply project-wide target defaults (C++23, defines, includes)
xinim_apply_target_defaults(xinim)
xinim_set_warnings(xinim)

# Bare-metal: disable hosted C++ runtime assumptions
target_compile_options(xinim PRIVATE
    -ffreestanding
    -fno-stack-protector
    -march=x86-64
    -mtune=generic
    -mcmodel=kernel
)
target_compile_definitions(xinim PRIVATE XINIM_BOOT_LIMINE)
if(XINIM_EFFECTIVE_VFS_PROFILE STREQUAL "tiny")
    target_compile_definitions(xinim PRIVATE XINIM_VFS_PROFILE_TINY=1)
endif()

# Enable AES-NI and SHA-NI intrinsics for hardware crypto files
set_source_files_properties(src/crypto/aes_hw.cpp PROPERTIES
    COMPILE_FLAGS "-maes -msse4.1")
set_source_files_properties(src/crypto/sha2_hw.cpp PROPERTIES
    COMPILE_FLAGS "-msha -msse4.1 -mssse3")

# Linker script for bare-metal kernel layout (1MB physical load)
target_link_options(xinim PRIVATE -T ${CMAKE_SOURCE_DIR}/linker.ld)
target_link_options(xinim PRIVATE -nostdlib -static -fno-stack-protector)

# No Conan-managed runtime deps: crypto is vendored, kernel is freestanding.
# target_link_libraries(xinim PRIVATE) -- intentionally empty

set(XINIM_LIMINE_VERSION "v10.8.3-binary" CACHE STRING
    "Pinned Limine binary branch for x86_64 image generation")
set(XINIM_LIMINE_ROOT "${XINIM_TOOLS_ROOT}/limine/${XINIM_LIMINE_VERSION}" CACHE PATH
    "Directory containing bootstrapped Limine assets")
set(XINIM_X86_64_IMAGE_DIR "${XINIM_IMAGE_ROOT}/x86_64")
set(XINIM_X86_64_STAGE_DIR "${XINIM_X86_64_IMAGE_DIR}/staging")
set(XINIM_X86_64_BOOT_IMAGE "${XINIM_X86_64_IMAGE_DIR}/xinim-x86_64.iso")
set(XINIM_LIMINE_CONFIG "${CMAKE_SOURCE_DIR}/boot/limine/limine.cfg")

add_custom_target(xinim_bootstrap_limine
    COMMAND ${CMAKE_COMMAND}
            -DXINIM_LIMINE_VERSION=${XINIM_LIMINE_VERSION}
            -DXINIM_LIMINE_ROOT=${XINIM_LIMINE_ROOT}
            -P ${CMAKE_SOURCE_DIR}/cmake/BootstrapLimine.cmake
    COMMENT "Bootstrapping Limine assets into the build-tree tools cache"
    USES_TERMINAL
)
add_dependencies(xinim_bootstrap_tools xinim_bootstrap_limine)

if(XINIM_CPU_LANE STREQUAL "x86_64")
add_executable(xinim_x86_64_shell
    userland/shell/xash/entry_x86_64.S
    userland/shell/xash/main_i486.cpp
)
set_target_properties(xinim_x86_64_shell PROPERTIES
    OUTPUT_NAME xash_x86_64
    XINIM_TARGET_ARCH x86_64
)
xinim_apply_target_defaults(xinim_x86_64_shell)
target_include_directories(xinim_x86_64_shell PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(xinim_x86_64_shell PRIVATE cxx_std_23)
target_compile_options(xinim_x86_64_shell PRIVATE
    -ffreestanding
    -fno-stack-protector
)
target_link_options(xinim_x86_64_shell PRIVATE
    -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
    -nostdlib
    -static
    -fno-stack-protector
    -no-pie
)
xinim_set_warnings(xinim_x86_64_shell)

endif()

    if(XINIM_XORRISO_EXECUTABLE AND EXISTS "${XINIM_LIMINE_CONFIG}")
    set(_XINIM_LIMINE_REQUIRED_FILES
        "${XINIM_LIMINE_ROOT}/limine"
        "${XINIM_LIMINE_ROOT}/limine-bios.sys"
        "${XINIM_LIMINE_ROOT}/limine-bios-cd.bin"
        "${XINIM_LIMINE_ROOT}/limine-uefi-cd.bin"
        "${XINIM_LIMINE_ROOT}/BOOTX64.EFI"
    )

    add_custom_command(
        OUTPUT "${XINIM_X86_64_BOOT_IMAGE}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${XINIM_X86_64_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_STAGE_DIR}/boot"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_STAGE_DIR}/boot/limine"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_STAGE_DIR}/limine"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_STAGE_DIR}/EFI/BOOT"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_STAGE_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_STAGE_DIR}/etc"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:xinim>" "${XINIM_X86_64_STAGE_DIR}/boot/xinim"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:xinim_x86_64_shell>" "${XINIM_X86_64_STAGE_DIR}/boot/xash"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_ROOT}/limine-bios.sys" "${XINIM_X86_64_STAGE_DIR}/boot/limine-bios.sys"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_ROOT}/limine-bios-cd.bin" "${XINIM_X86_64_STAGE_DIR}/boot/limine-bios-cd.bin"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_ROOT}/limine-uefi-cd.bin" "${XINIM_X86_64_STAGE_DIR}/boot/limine-uefi-cd.bin"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_ROOT}/BOOTX64.EFI" "${XINIM_X86_64_STAGE_DIR}/EFI/BOOT/BOOTX64.EFI"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/limine.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/boot/limine.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/boot/limine/limine.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/limine/limine.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/EFI/BOOT/limine.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/limine.conf"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/boot/limine.conf"
        COMMAND ${CMAKE_COMMAND} -E copy "${XINIM_LIMINE_CONFIG}" "${XINIM_X86_64_STAGE_DIR}/boot/limine/limine.conf"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:xinim_x86_64_shell>" "${XINIM_X86_64_STAGE_DIR}/bin/xash"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:xinim_x86_64_shell>" "${XINIM_X86_64_STAGE_DIR}/bin/sh"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt" "${XINIM_X86_64_STAGE_DIR}/etc/motd"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_X86_64_IMAGE_DIR}"
        COMMAND ${XINIM_XORRISO_EXECUTABLE}
            -as mkisofs
            -b boot/limine-bios-cd.bin
            -no-emul-boot
            -boot-load-size 4
            -boot-info-table
            --efi-boot boot/limine-uefi-cd.bin
            -efi-boot-part
            --efi-boot-image
            --protective-msdos-label
            "${XINIM_X86_64_STAGE_DIR}"
            -o "${XINIM_X86_64_BOOT_IMAGE}"
        COMMAND "${XINIM_LIMINE_ROOT}/limine" bios-install "${XINIM_X86_64_BOOT_IMAGE}"
        DEPENDS
            xinim
            xinim_x86_64_shell
            xinim_bootstrap_limine
            "${XINIM_LIMINE_CONFIG}"
            "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt"
            ${_XINIM_LIMINE_REQUIRED_FILES}
        COMMENT "Generating XINIM x86_64 Limine boot image"
        VERBATIM
    )

    add_custom_target(xinim_x86_64_image DEPENDS "${XINIM_X86_64_BOOT_IMAGE}")
    add_custom_target(run_qemu_x86_64
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/qemu_x86_64.sh" --boot-image "${XINIM_X86_64_BOOT_IMAGE}"
        DEPENDS xinim_x86_64_image
        USES_TERMINAL
    )
else()
    message(WARNING
        "x86_64 image generation requires boot/limine/limine.cfg and a usable "
        "xorriso. Set XINIM_XORRISO_EXECUTABLE or install xorriso if you want "
        "the xinim_x86_64_image target.")
endif()
endif()

# =============================================================================
# 32-bit x86 Bootstrap Lanes
# =============================================================================

set(XINIM_X86_32_KERNEL_SOURCES
    src/boot/multiboot2/entry.S
    src/boot/multiboot2/bootinfo.cpp
    src/kernel/freestanding_memory.cpp
    src/kernel/i486/bootfs.cpp
    src/kernel/i486/elf32_loader.cpp
    src/kernel/i486/ext2_reader.cpp
    src/kernel/i486/ide.cpp
    src/kernel/i486/dma_pages.cpp
    src/kernel/i486/virtio_net_i486.cpp
    src/kernel/i486/main.cpp
    src/kernel/i486/console.cpp
    src/kernel/i486/ring3.cpp
    src/kernel/i486/ring3_entry.S
    src/kernel/i486/shell.cpp
    src/kernel/i486/storage.cpp
    src/kernel/recovery/recovery_dag.cpp
    src/kernel/abi/lattice_changer.cpp
    src/vfs/core_init.cpp
    src/vfs/inode_table.cpp
    src/vfs/vnode_table.cpp
    src/vfs/dirent.cpp
    src/vfs/path_walk.cpp
    src/vfs/seed.cpp
    src/vfs/bootfs_promote.cpp
    src/vfs/ramfs_ops.cpp
    src/vfs/mount_table.cpp
    src/vfs/fd_table.cpp
    src/vfs/buffer_cache.cpp
)

set(XINIM_X86_32_XASH_SOURCES
    userland/shell/xash/entry_i486.S
    userland/shell/xash/main_i486.cpp
)

set(XINIM_X86_32_HELLO_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/hello_i486.cpp
)

set(XINIM_X86_32_FALSE_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/false_i486.cpp
)

set(XINIM_X86_32_HEAPPROBE_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/heapprobe_i486.cpp
)

set(XINIM_X86_32_HOLDSVC_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/holdsvc_i486.cpp
)

set(XINIM_X86_32_LS_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/ls_i486.cpp
)

set(XINIM_X86_32_WRITEFILE_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/writefile_i486.cpp
)

set(XINIM_X86_32_MKDIR_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/mkdir_i486.cpp
)

set(XINIM_X86_32_UNLINK_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/unlink_i486.cpp
)

set(XINIM_X86_32_RMDIR_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/rmdir_i486.cpp
)

set(XINIM_X86_32_MV_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/mv_i486.cpp
)

set(XINIM_X86_32_SEEKWRITE_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/seekwrite_i486.cpp
)

set(XINIM_X86_32_HOLECHECK_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/holecheck_i486.cpp
)

set(XINIM_X86_32_SEEKPATCH_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/seekpatch_i486.cpp
)

set(XINIM_X86_32_GAPCHECK_SOURCES
    userland/shell/xash/entry_i486.S
    userland/tests/gapcheck_i486.cpp
)

function(xinim_add_x86_32_lane lane)
    xinim_get_x86_lane_property(${lane} target_arch _target_arch)
    xinim_get_x86_lane_property(${lane} banner_name _banner_name)
    xinim_get_x86_lane_property(${lane} artifact_stem _artifact_stem)
    xinim_get_x86_lane_property(${lane} image_dir_name _image_dir_name)
    xinim_get_x86_lane_property(${lane} image_name _image_name)
    xinim_get_x86_lane_property(${lane} compile_options _compile_options)
    xinim_get_x86_lane_property(${lane} qemu_cpu _qemu_cpu)
    xinim_get_x86_lane_property(${lane} qemu_bin _qemu_bin)
    xinim_get_x86_lane_property(${lane} qemu_machine _qemu_machine)
    xinim_get_x86_lane_property(${lane} qemu_memory _qemu_memory)
    xinim_get_x86_lane_property(${lane} qemu_vga _qemu_vga)
    xinim_get_x86_lane_property(${lane} image_target _image_target)
    xinim_get_x86_lane_property(${lane} prepare_test _prepare_test)
    xinim_get_x86_lane_property(${lane} smoke_test _smoke_test)
    xinim_get_x86_lane_property(${lane} layout_test _layout_test)
    xinim_get_x86_lane_property(${lane} shell_test _shell_test)
    set(_persist_test "${lane}_persistence_test")
    set(_mutation_test "${lane}_ext2_mutation_test")

    set(_lane_compile_options ${_compile_options})
    set(_lane_link_arch_options
        -m32
        -Wl,-m,elf_i386
    )
    if(XINIM_X86_32_TOOLCHAIN_MODE STREQUAL "cross-elf")
        list(REMOVE_ITEM _lane_compile_options -m32)
        set(_lane_link_arch_options)
    endif()

    set(_kernel_target "xinim_${lane}")
    set(_shell_target "xash_${lane}")
    set(_mksh_target "mksh_${lane}")
    set(_dietlibc_target "dietlibc_${lane}")
    set(_hello_target "hello_${lane}")
    set(_false_target "false_${lane}")
    set(_heapprobe_target "heapprobe_${lane}")
    set(_holdsvc_target "holdsvc_${lane}")
    set(_diethello_target "diethello_${lane}")
    set(_ls_target "ls_${lane}")
    set(_writefile_target "writefile_${lane}")
    set(_mkdir_target "mkdir_${lane}")
    set(_unlink_target "unlink_${lane}")
    set(_rmdir_target "rmdir_${lane}")
    set(_mv_target "mv_${lane}")
    set(_seekwrite_target "seekwrite_${lane}")
    set(_holecheck_target "holecheck_${lane}")
    set(_seekpatch_target "seekpatch_${lane}")
    set(_gapcheck_target "gapcheck_${lane}")
    # Utility target/binary variables are generated by the foreach loop below
    set(_image_dir "${XINIM_IMAGE_ROOT}/${_image_dir_name}")
    set(_stage_dir "${_image_dir}/staging")
    set(_boot_image "${_image_dir}/${_image_name}")
    set(_ata_disk_image "${_image_dir}/xinim-${lane}-ata.img")
    set(_boot_log "${XINIM_LOG_ROOT}/${lane}-smoke.log")
    set(_shell_log "${XINIM_LOG_ROOT}/${lane}-kshell.log")
    set(_compat_report "${XINIM_LOG_ROOT}/${lane}-shell-compat.txt")
    set(_compat_test "${lane}_shell_compat_test")

    add_executable(${_kernel_target} ${XINIM_X86_32_KERNEL_SOURCES})
    set_target_properties(${_kernel_target} PROPERTIES
        OUTPUT_NAME "${_artifact_stem}"
        XINIM_TARGET_ARCH "${_target_arch}"
    )
    xinim_apply_target_defaults(${_kernel_target})
    xinim_set_warnings(${_kernel_target})
    target_compile_options(${_kernel_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_kernel_target} PRIVATE
        XINIM_BOOT_MULTIBOOT2
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    if(XINIM_EFFECTIVE_VFS_PROFILE STREQUAL "tiny")
        target_compile_definitions(${_kernel_target} PRIVATE XINIM_VFS_PROFILE_TINY=1)
    endif()
    target_link_options(${_kernel_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_i486_multiboot2.ld
        -nostdlib
        -static
        -fno-stack-protector
        LINKER:--build-id=none
        ${_lane_link_arch_options}
    )

    add_executable(${_shell_target} ${XINIM_X86_32_XASH_SOURCES})
    set_target_properties(${_shell_target} PROPERTIES
        OUTPUT_NAME "xash-${lane}"
    )
    target_include_directories(${_shell_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_shell_target} PRIVATE cxx_std_23)
    target_compile_options(${_shell_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_shell_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_shell_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_shell_target})

    set(_mksh_source_dir "${CMAKE_SOURCE_DIR}/.scratch/mksh")
    set(_mksh_build_dir "${CMAKE_CURRENT_BINARY_DIR}/mksh-${lane}")
    set(_mksh_binary "${_mksh_build_dir}/mksh")
    set(_dietlibc_source_dir "${CMAKE_SOURCE_DIR}/libc/dietlibc-xinim")
    set(_dietlibc_build_dir "${CMAKE_CURRENT_BINARY_DIR}/dietlibc-${lane}")
    set(_dietlibc_tree_dir "${_dietlibc_build_dir}/tree")
    set(_dietlibc_start "${_dietlibc_tree_dir}/bin-i386/start.o")
    set(_dietlibc_archive "${_dietlibc_tree_dir}/bin-i386/dietlibc.a")
    set(_diethello_binary "${CMAKE_CURRENT_BINARY_DIR}/diethello-${lane}")

    add_custom_command(
        OUTPUT "${_mksh_binary}"
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/build_i486_mksh.py
            --source-dir "${_mksh_source_dir}"
            --build-dir "${_mksh_build_dir}"
            --cc "gcc -m32 -march=i486 -mtune=i486 -mno-mmx -mno-sse"
            --start-o "${_dietlibc_start}"
            --dietlibc-a "${_dietlibc_archive}"
            --linker-script "${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld"
            --output "${_mksh_binary}"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/scripts/build_i486_mksh.py"
            "${_mksh_source_dir}/Makefrag.inc"
            "${_mksh_source_dir}/sh.h"
            "${_mksh_source_dir}/Build.sh"
            "${_dietlibc_start}"
            "${_dietlibc_archive}"
            "${_dietlibc_source_dir}/i386/syscalls.h"
        COMMENT "Building mksh shell for ${lane}"
        VERBATIM
    )
    add_custom_target(${_mksh_target} ALL DEPENDS "${_mksh_binary}")
    set_target_properties(${_mksh_target} PROPERTIES
        EXCLUDE_FROM_ALL FALSE
    )

    add_custom_command(
        OUTPUT "${_dietlibc_archive}" "${_dietlibc_start}"
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/build_i386_dietlibc.py
            --source-dir "${_dietlibc_source_dir}"
            --build-dir "${_dietlibc_build_dir}"
            --cc "gcc -m32 -march=i486 -mtune=i486 -mno-mmx -mno-sse"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/scripts/build_i386_dietlibc.py"
            "${_dietlibc_source_dir}/Makefile"
            "${_dietlibc_source_dir}/dietfeatures.h"
            "${_dietlibc_source_dir}/i386/syscalls.h"
            "${_dietlibc_source_dir}/i386/start.S"
            "${_dietlibc_source_dir}/i386/unified.S"
            "${_dietlibc_source_dir}/lib/getauxval.c"
            "${_dietlibc_source_dir}/lib/__get_elf_aux_value.c"
            "${_dietlibc_source_dir}/libcruft/getpagesize.c"
            "${_dietlibc_source_dir}/README.clang"
        COMMENT "Building retargeted i386 dietlibc for ${lane}"
        VERBATIM
    )
    add_custom_target(${_dietlibc_target} ALL DEPENDS "${_dietlibc_archive}" "${_dietlibc_start}")

    add_custom_command(
        OUTPUT "${_diethello_binary}"
        COMMAND ${CMAKE_C_COMPILER}
            -m32
            -I"${_dietlibc_tree_dir}/include"
            -I"${CMAKE_SOURCE_DIR}/include"
            -O2
            -fno-pie
            -fno-pic
            -fno-stack-protector
            -nostdlib
            -static
            -Wl,--build-id=none
            -o "${_diethello_binary}"
            "${CMAKE_SOURCE_DIR}/userland/tests/diethello_i486.c"
            "${_dietlibc_start}"
            "${_dietlibc_archive}"
            -lgcc
        DEPENDS
            ${_dietlibc_target}
            "${CMAKE_SOURCE_DIR}/userland/tests/diethello_i486.c"
        COMMENT "Building dietlibc-linked i486 guest utility for ${lane}"
        VERBATIM
    )
    add_custom_target(${_diethello_target} ALL DEPENDS "${_diethello_binary}")

    # -- Tier 1 POSIX utilities (dietlibc-linked) --
    set(_posix_util_sources
        cat echo true pwd env cp rm test basename dirname printf
        wc head tail sort uniq cut tr tee grep sed
        kill sleep uname id touch ln xargs dd
        seq yes nproc which chmod chown readlink mktemp expr
        hexdump du date df md5sum tar find ps awk diff patch
    )
    set(_posix_util_binaries "")
    set(_posix_util_targets "")
    foreach(_util IN LISTS _posix_util_sources)
        set(_util_binary "${CMAKE_CURRENT_BINARY_DIR}/${_util}-${lane}")
        set(_util_target "${_util}_${lane}")
        set(_${_util}_binary "${_util_binary}")
        set(_${_util}_target "${_util_target}")
        set(_util_source "${CMAKE_SOURCE_DIR}/userland/bin/${_util}_i486.c")
        add_custom_command(
            OUTPUT "${_util_binary}"
            COMMAND ${CMAKE_C_COMPILER}
                -m32
                -I"${_dietlibc_tree_dir}/include"
                -I"${CMAKE_SOURCE_DIR}/include"
                -O2
                -fno-pie
                -fno-pic
                -fno-stack-protector
                -fno-builtin-bcmp
                -nostdlib
                -static
                -Wl,--build-id=none
                -o "${_util_binary}"
                "${_util_source}"
                "${_dietlibc_start}"
                "${_dietlibc_archive}"
                -lgcc
            DEPENDS
                ${_dietlibc_target}
                "${_util_source}"
            COMMENT "Building dietlibc-linked ${_util} for ${lane}"
            VERBATIM
        )
        add_custom_target(${_util_target} ALL DEPENDS "${_util_binary}")
        list(APPEND _posix_util_binaries "${_util_binary}")
        list(APPEND _posix_util_targets "${_util_target}")
    endforeach()

    add_executable(${_hello_target} ${XINIM_X86_32_HELLO_SOURCES})
    set_target_properties(${_hello_target} PROPERTIES
        OUTPUT_NAME "hello-${lane}"
    )
    target_include_directories(${_hello_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_hello_target} PRIVATE cxx_std_23)
    target_compile_options(${_hello_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_hello_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_hello_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_hello_target})

    add_executable(${_false_target} ${XINIM_X86_32_FALSE_SOURCES})
    set_target_properties(${_false_target} PROPERTIES
        OUTPUT_NAME "false-${lane}"
    )
    target_include_directories(${_false_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_false_target} PRIVATE cxx_std_23)
    target_compile_options(${_false_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_false_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_false_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_false_target})

    add_executable(${_heapprobe_target} ${XINIM_X86_32_HEAPPROBE_SOURCES})
    set_target_properties(${_heapprobe_target} PROPERTIES
        OUTPUT_NAME "heapprobe-${lane}"
    )
    target_include_directories(${_heapprobe_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_heapprobe_target} PRIVATE cxx_std_23)
    target_compile_options(${_heapprobe_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_heapprobe_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_heapprobe_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_heapprobe_target})

    add_executable(${_holdsvc_target} ${XINIM_X86_32_HOLDSVC_SOURCES})
    set_target_properties(${_holdsvc_target} PROPERTIES
        OUTPUT_NAME "holdsvc-${lane}"
    )
    target_include_directories(${_holdsvc_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_holdsvc_target} PRIVATE cxx_std_23)
    target_compile_options(${_holdsvc_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_holdsvc_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_holdsvc_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_holdsvc_target})

    add_executable(${_ls_target} ${XINIM_X86_32_LS_SOURCES})
    set_target_properties(${_ls_target} PROPERTIES
        OUTPUT_NAME "ls-${lane}"
    )
    target_include_directories(${_ls_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_ls_target} PRIVATE cxx_std_23)
    target_compile_options(${_ls_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_ls_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_ls_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_ls_target})

    add_executable(${_writefile_target} ${XINIM_X86_32_WRITEFILE_SOURCES})
    set_target_properties(${_writefile_target} PROPERTIES
        OUTPUT_NAME "writefile-${lane}"
    )
    target_include_directories(${_writefile_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_writefile_target} PRIVATE cxx_std_23)
    target_compile_options(${_writefile_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_writefile_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_writefile_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_writefile_target})

    add_executable(${_mkdir_target} ${XINIM_X86_32_MKDIR_SOURCES})
    set_target_properties(${_mkdir_target} PROPERTIES
        OUTPUT_NAME "mkdir-${lane}"
    )
    target_include_directories(${_mkdir_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_mkdir_target} PRIVATE cxx_std_23)
    target_compile_options(${_mkdir_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_mkdir_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_mkdir_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_mkdir_target})

    add_executable(${_unlink_target} ${XINIM_X86_32_UNLINK_SOURCES})
    set_target_properties(${_unlink_target} PROPERTIES
        OUTPUT_NAME "unlink-${lane}"
    )
    target_include_directories(${_unlink_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_unlink_target} PRIVATE cxx_std_23)
    target_compile_options(${_unlink_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_unlink_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_unlink_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_unlink_target})

    add_executable(${_rmdir_target} ${XINIM_X86_32_RMDIR_SOURCES})
    set_target_properties(${_rmdir_target} PROPERTIES
        OUTPUT_NAME "rmdir-${lane}"
    )
    target_include_directories(${_rmdir_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_rmdir_target} PRIVATE cxx_std_23)
    target_compile_options(${_rmdir_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_rmdir_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_rmdir_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_rmdir_target})

    add_executable(${_mv_target} ${XINIM_X86_32_MV_SOURCES})
    set_target_properties(${_mv_target} PROPERTIES
        OUTPUT_NAME "mv-${lane}"
    )
    target_include_directories(${_mv_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_mv_target} PRIVATE cxx_std_23)
    target_compile_options(${_mv_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_mv_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_mv_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_mv_target})

    add_executable(${_seekwrite_target} ${XINIM_X86_32_SEEKWRITE_SOURCES})
    set_target_properties(${_seekwrite_target} PROPERTIES
        OUTPUT_NAME "seekwrite-${lane}"
    )
    target_include_directories(${_seekwrite_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_seekwrite_target} PRIVATE cxx_std_23)
    target_compile_options(${_seekwrite_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_seekwrite_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_seekwrite_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_seekwrite_target})

    add_executable(${_holecheck_target} ${XINIM_X86_32_HOLECHECK_SOURCES})
    set_target_properties(${_holecheck_target} PROPERTIES
        OUTPUT_NAME "holecheck-${lane}"
    )
    target_include_directories(${_holecheck_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_holecheck_target} PRIVATE cxx_std_23)
    target_compile_options(${_holecheck_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_holecheck_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_holecheck_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_holecheck_target})

    add_executable(${_seekpatch_target} ${XINIM_X86_32_SEEKPATCH_SOURCES})
    set_target_properties(${_seekpatch_target} PROPERTIES
        OUTPUT_NAME "seekpatch-${lane}"
    )
    target_include_directories(${_seekpatch_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_seekpatch_target} PRIVATE cxx_std_23)
    target_compile_options(${_seekpatch_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_seekpatch_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_seekpatch_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_seekpatch_target})

    add_executable(${_gapcheck_target} ${XINIM_X86_32_GAPCHECK_SOURCES})
    set_target_properties(${_gapcheck_target} PROPERTIES
        OUTPUT_NAME "gapcheck-${lane}"
    )
    target_include_directories(${_gapcheck_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_features(${_gapcheck_target} PRIVATE cxx_std_23)
    target_compile_options(${_gapcheck_target} PRIVATE ${_lane_compile_options})
    target_compile_definitions(${_gapcheck_target} PRIVATE
        XINIM_BOOT_LANE_NAME="${_banner_name}"
    )
    target_link_options(${_gapcheck_target} PRIVATE
        -T ${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld
        -nostdlib
        -static
        -fno-stack-protector
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_gapcheck_target})

    add_custom_command(
        OUTPUT "${_ata_disk_image}"
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/create_i486_ata_disk.py
            --output "${_ata_disk_image}"
            --size-mb 16
            --guest-bin "$<TARGET_FILE:${_hello_target}>:/bin/persist-hello"
            --guest-bin "${_cat_binary}:/bin/cat"
            --guest-bin "${_echo_binary}:/bin/echo"
            --guest-bin "${_true_binary}:/bin/true"
            --guest-bin "${_pwd_binary}:/bin/pwd"
            --guest-bin "${_env_binary}:/bin/env"
            --guest-bin "${_cp_binary}:/bin/cp"
            --guest-bin "${_rm_binary}:/bin/rm"
            --guest-bin "${_test_binary}:/bin/test"
            --guest-bin "${_basename_binary}:/bin/basename"
            --guest-bin "${_dirname_binary}:/bin/dirname"
            --guest-bin "${_printf_binary}:/bin/printf"
            --guest-bin "${_wc_binary}:/bin/wc"
            --guest-bin "${_head_binary}:/bin/head"
            --guest-bin "${_tail_binary}:/bin/tail"
            --guest-bin "${_sort_binary}:/bin/sort"
            --guest-bin "${_uniq_binary}:/bin/uniq"
            --guest-bin "${_cut_binary}:/bin/cut"
            --guest-bin "${_tr_binary}:/bin/tr"
            --guest-bin "${_tee_binary}:/bin/tee"
            --guest-bin "${_grep_binary}:/bin/grep"
            --guest-bin "${_sed_binary}:/bin/sed"
            --guest-bin "${_kill_binary}:/bin/kill"
            --guest-bin "${_sleep_binary}:/bin/sleep"
            --guest-bin "${_uname_binary}:/bin/uname"
            --guest-bin "${_id_binary}:/bin/id"
            --guest-bin "${_touch_binary}:/bin/touch"
            --guest-bin "${_ln_binary}:/bin/ln"
            --guest-bin "${_xargs_binary}:/bin/xargs"
            --guest-bin "${_dd_binary}:/bin/dd"
            --guest-bin "${_seq_binary}:/bin/seq"
            --guest-bin "${_yes_binary}:/bin/yes"
            --guest-bin "${_nproc_binary}:/bin/nproc"
            --guest-bin "${_which_binary}:/bin/which"
            --guest-bin "${_chmod_binary}:/bin/chmod"
            --guest-bin "${_chown_binary}:/bin/chown"
            --guest-bin "${_readlink_binary}:/bin/readlink"
            --guest-bin "${_mktemp_binary}:/bin/mktemp"
            --guest-bin "${_expr_binary}:/bin/expr"
            --guest-bin "${_hexdump_binary}:/bin/hexdump"
            --guest-bin "${_du_binary}:/bin/du"
            --guest-bin "${_date_binary}:/bin/date"
            --guest-bin "${_df_binary}:/bin/df"
            --guest-bin "${_md5sum_binary}:/bin/md5sum"
            --guest-bin "${_tar_binary}:/bin/tar"
            --guest-bin "${_find_binary}:/bin/find"
            --guest-bin "${_ps_binary}:/bin/ps"
            --guest-bin "${_awk_binary}:/bin/awk"
            --guest-bin "${_diff_binary}:/bin/diff"
            --guest-bin "${_patch_binary}:/bin/patch"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/scripts/create_i486_ata_disk.py"
            ${_hello_target}
            ${_posix_util_targets}
        COMMENT "Generating reproducible ATA disk image for ${lane}"
        VERBATIM
    )

    if(XINIM_XORRISO_EXECUTABLE)
        add_custom_command(
            OUTPUT "${_boot_image}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${_stage_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_dir}/boot/grub"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_dir}/bin"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_dir}/boot"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_dir}/etc"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_kernel_target}>" "${_stage_dir}/boot/xinim"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_shell_target}>" "${_stage_dir}/boot/xash"
            COMMAND ${CMAKE_COMMAND} -E copy "${_mksh_binary}" "${_stage_dir}/bin/mksh"
                COMMAND ${CMAKE_COMMAND} -E copy "${_mksh_binary}" "${_stage_dir}/boot/mksh"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_hello_target}>" "${_stage_dir}/boot/hello"
                COMMAND ${CMAKE_COMMAND} -E copy "${_diethello_binary}" "${_stage_dir}/boot/diethello"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_ls_target}>" "${_stage_dir}/boot/ls"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_writefile_target}>" "${_stage_dir}/boot/writefile"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_mkdir_target}>" "${_stage_dir}/boot/mkdir"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_unlink_target}>" "${_stage_dir}/boot/unlink"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_rmdir_target}>" "${_stage_dir}/boot/rmdir"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_mv_target}>" "${_stage_dir}/boot/mv"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_seekwrite_target}>" "${_stage_dir}/boot/seekwrite"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_holecheck_target}>" "${_stage_dir}/boot/holecheck"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_seekpatch_target}>" "${_stage_dir}/boot/seekpatch"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_gapcheck_target}>" "${_stage_dir}/boot/gapcheck"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_false_target}>" "${_stage_dir}/boot/false"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_heapprobe_target}>" "${_stage_dir}/boot/heapprobe"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_holdsvc_target}>" "${_stage_dir}/boot/holdsvc"
                COMMAND ${CMAKE_COMMAND} -E copy "${_cat_binary}" "${_stage_dir}/boot/cat"
                COMMAND ${CMAKE_COMMAND} -E copy "${_echo_binary}" "${_stage_dir}/boot/echo"
                COMMAND ${CMAKE_COMMAND} -E copy "${_true_binary}" "${_stage_dir}/boot/true"
                COMMAND ${CMAKE_COMMAND} -E copy "${_pwd_binary}" "${_stage_dir}/boot/pwd"
                COMMAND ${CMAKE_COMMAND} -E copy "${_env_binary}" "${_stage_dir}/boot/env"
                COMMAND ${CMAKE_COMMAND} -E copy "${_cp_binary}" "${_stage_dir}/boot/cp"
                COMMAND ${CMAKE_COMMAND} -E copy "${_rm_binary}" "${_stage_dir}/boot/rm"
                COMMAND ${CMAKE_COMMAND} -E copy "${_test_binary}" "${_stage_dir}/boot/test"
                COMMAND ${CMAKE_COMMAND} -E copy "${_basename_binary}" "${_stage_dir}/boot/basename"
                COMMAND ${CMAKE_COMMAND} -E copy "${_dirname_binary}" "${_stage_dir}/boot/dirname"
                COMMAND ${CMAKE_COMMAND} -E copy "${_printf_binary}" "${_stage_dir}/boot/printf"
                COMMAND ${CMAKE_COMMAND} -E copy "${_wc_binary}" "${_stage_dir}/boot/wc"
                COMMAND ${CMAKE_COMMAND} -E copy "${_head_binary}" "${_stage_dir}/boot/head"
                COMMAND ${CMAKE_COMMAND} -E copy "${_tail_binary}" "${_stage_dir}/boot/tail"
                COMMAND ${CMAKE_COMMAND} -E copy "${_sort_binary}" "${_stage_dir}/boot/sort"
                COMMAND ${CMAKE_COMMAND} -E copy "${_uniq_binary}" "${_stage_dir}/boot/uniq"
                COMMAND ${CMAKE_COMMAND} -E copy "${_cut_binary}" "${_stage_dir}/boot/cut"
                COMMAND ${CMAKE_COMMAND} -E copy "${_tr_binary}" "${_stage_dir}/boot/tr"
                COMMAND ${CMAKE_COMMAND} -E copy "${_tee_binary}" "${_stage_dir}/boot/tee"
                COMMAND ${CMAKE_COMMAND} -E copy "${_grep_binary}" "${_stage_dir}/boot/grep"
                COMMAND ${CMAKE_COMMAND} -E copy "${_sed_binary}" "${_stage_dir}/boot/sed"
                COMMAND ${CMAKE_COMMAND} -E copy "${_kill_binary}" "${_stage_dir}/boot/kill"
                COMMAND ${CMAKE_COMMAND} -E copy "${_sleep_binary}" "${_stage_dir}/boot/sleep"
                COMMAND ${CMAKE_COMMAND} -E copy "${_uname_binary}" "${_stage_dir}/boot/uname"
                COMMAND ${CMAKE_COMMAND} -E copy "${_id_binary}" "${_stage_dir}/boot/id"
                COMMAND ${CMAKE_COMMAND} -E copy "${_touch_binary}" "${_stage_dir}/boot/touch"
                COMMAND ${CMAKE_COMMAND} -E copy "${_ln_binary}" "${_stage_dir}/boot/ln"
                COMMAND ${CMAKE_COMMAND} -E copy "${_xargs_binary}" "${_stage_dir}/boot/xargs"
                COMMAND ${CMAKE_COMMAND} -E copy "${_dd_binary}" "${_stage_dir}/boot/dd"
                COMMAND ${CMAKE_COMMAND} -E copy "${_seq_binary}" "${_stage_dir}/boot/seq"
                COMMAND ${CMAKE_COMMAND} -E copy "${_yes_binary}" "${_stage_dir}/boot/yes"
                COMMAND ${CMAKE_COMMAND} -E copy "${_nproc_binary}" "${_stage_dir}/boot/nproc"
                COMMAND ${CMAKE_COMMAND} -E copy "${_which_binary}" "${_stage_dir}/boot/which"
                COMMAND ${CMAKE_COMMAND} -E copy "${_chmod_binary}" "${_stage_dir}/boot/chmod"
                COMMAND ${CMAKE_COMMAND} -E copy "${_chown_binary}" "${_stage_dir}/boot/chown"
                COMMAND ${CMAKE_COMMAND} -E copy "${_readlink_binary}" "${_stage_dir}/boot/readlink"
                COMMAND ${CMAKE_COMMAND} -E copy "${_mktemp_binary}" "${_stage_dir}/boot/mktemp"
                COMMAND ${CMAKE_COMMAND} -E copy "${_expr_binary}" "${_stage_dir}/boot/expr"
                COMMAND ${CMAKE_COMMAND} -E copy "${_hexdump_binary}" "${_stage_dir}/boot/hexdump"
                COMMAND ${CMAKE_COMMAND} -E copy "${_du_binary}" "${_stage_dir}/boot/du"
                COMMAND ${CMAKE_COMMAND} -E copy "${_date_binary}" "${_stage_dir}/boot/date"
                COMMAND ${CMAKE_COMMAND} -E copy "${_df_binary}" "${_stage_dir}/boot/df"
                COMMAND ${CMAKE_COMMAND} -E copy "${_md5sum_binary}" "${_stage_dir}/boot/md5sum"
                COMMAND ${CMAKE_COMMAND} -E copy "${_tar_binary}" "${_stage_dir}/boot/tar"
                COMMAND ${CMAKE_COMMAND} -E copy "${_find_binary}" "${_stage_dir}/boot/find"
                COMMAND ${CMAKE_COMMAND} -E copy "${_ps_binary}" "${_stage_dir}/boot/ps"
                COMMAND ${CMAKE_COMMAND} -E copy "${_awk_binary}" "${_stage_dir}/boot/awk"
                COMMAND ${CMAKE_COMMAND} -E copy "${_diff_binary}" "${_stage_dir}/boot/diff"
                COMMAND ${CMAKE_COMMAND} -E copy "${_patch_binary}" "${_stage_dir}/boot/patch"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt" "${_stage_dir}/boot/motd"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_shell_target}>" "${_stage_dir}/bin/xash"
            COMMAND ${CMAKE_COMMAND} -E copy "${_mksh_binary}" "${_stage_dir}/bin/sh"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_hello_target}>" "${_stage_dir}/bin/hello"
            COMMAND ${CMAKE_COMMAND} -E copy "${_diethello_binary}" "${_stage_dir}/bin/diethello"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_ls_target}>" "${_stage_dir}/bin/ls"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_writefile_target}>" "${_stage_dir}/bin/writefile"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_mkdir_target}>" "${_stage_dir}/bin/mkdir"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_unlink_target}>" "${_stage_dir}/bin/unlink"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_rmdir_target}>" "${_stage_dir}/bin/rmdir"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_mv_target}>" "${_stage_dir}/bin/mv"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_seekwrite_target}>" "${_stage_dir}/bin/seekwrite"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_holecheck_target}>" "${_stage_dir}/bin/holecheck"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_seekpatch_target}>" "${_stage_dir}/bin/seekpatch"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_gapcheck_target}>" "${_stage_dir}/bin/gapcheck"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_false_target}>" "${_stage_dir}/bin/false"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_heapprobe_target}>" "${_stage_dir}/bin/heapprobe"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_holdsvc_target}>" "${_stage_dir}/bin/holdsvc"
            COMMAND ${CMAKE_COMMAND} -E copy "${_cat_binary}" "${_stage_dir}/bin/cat"
            COMMAND ${CMAKE_COMMAND} -E copy "${_echo_binary}" "${_stage_dir}/bin/echo"
            COMMAND ${CMAKE_COMMAND} -E copy "${_true_binary}" "${_stage_dir}/bin/true"
            COMMAND ${CMAKE_COMMAND} -E copy "${_pwd_binary}" "${_stage_dir}/bin/pwd"
            COMMAND ${CMAKE_COMMAND} -E copy "${_env_binary}" "${_stage_dir}/bin/env"
            COMMAND ${CMAKE_COMMAND} -E copy "${_cp_binary}" "${_stage_dir}/bin/cp"
            COMMAND ${CMAKE_COMMAND} -E copy "${_rm_binary}" "${_stage_dir}/bin/rm"
            COMMAND ${CMAKE_COMMAND} -E copy "${_test_binary}" "${_stage_dir}/bin/test"
            COMMAND ${CMAKE_COMMAND} -E copy "${_basename_binary}" "${_stage_dir}/bin/basename"
            COMMAND ${CMAKE_COMMAND} -E copy "${_dirname_binary}" "${_stage_dir}/bin/dirname"
            COMMAND ${CMAKE_COMMAND} -E copy "${_printf_binary}" "${_stage_dir}/bin/printf"
            COMMAND ${CMAKE_COMMAND} -E copy "${_wc_binary}" "${_stage_dir}/bin/wc"
            COMMAND ${CMAKE_COMMAND} -E copy "${_head_binary}" "${_stage_dir}/bin/head"
            COMMAND ${CMAKE_COMMAND} -E copy "${_tail_binary}" "${_stage_dir}/bin/tail"
            COMMAND ${CMAKE_COMMAND} -E copy "${_sort_binary}" "${_stage_dir}/bin/sort"
            COMMAND ${CMAKE_COMMAND} -E copy "${_uniq_binary}" "${_stage_dir}/bin/uniq"
            COMMAND ${CMAKE_COMMAND} -E copy "${_cut_binary}" "${_stage_dir}/bin/cut"
            COMMAND ${CMAKE_COMMAND} -E copy "${_tr_binary}" "${_stage_dir}/bin/tr"
            COMMAND ${CMAKE_COMMAND} -E copy "${_tee_binary}" "${_stage_dir}/bin/tee"
            COMMAND ${CMAKE_COMMAND} -E copy "${_grep_binary}" "${_stage_dir}/bin/grep"
            COMMAND ${CMAKE_COMMAND} -E copy "${_sed_binary}" "${_stage_dir}/bin/sed"
            COMMAND ${CMAKE_COMMAND} -E copy "${_kill_binary}" "${_stage_dir}/bin/kill"
            COMMAND ${CMAKE_COMMAND} -E copy "${_sleep_binary}" "${_stage_dir}/bin/sleep"
            COMMAND ${CMAKE_COMMAND} -E copy "${_uname_binary}" "${_stage_dir}/bin/uname"
            COMMAND ${CMAKE_COMMAND} -E copy "${_id_binary}" "${_stage_dir}/bin/id"
            COMMAND ${CMAKE_COMMAND} -E copy "${_touch_binary}" "${_stage_dir}/bin/touch"
            COMMAND ${CMAKE_COMMAND} -E copy "${_ln_binary}" "${_stage_dir}/bin/ln"
            COMMAND ${CMAKE_COMMAND} -E copy "${_xargs_binary}" "${_stage_dir}/bin/xargs"
            COMMAND ${CMAKE_COMMAND} -E copy "${_dd_binary}" "${_stage_dir}/bin/dd"
            COMMAND ${CMAKE_COMMAND} -E copy "${_seq_binary}" "${_stage_dir}/bin/seq"
            COMMAND ${CMAKE_COMMAND} -E copy "${_yes_binary}" "${_stage_dir}/bin/yes"
            COMMAND ${CMAKE_COMMAND} -E copy "${_nproc_binary}" "${_stage_dir}/bin/nproc"
            COMMAND ${CMAKE_COMMAND} -E copy "${_which_binary}" "${_stage_dir}/bin/which"
            COMMAND ${CMAKE_COMMAND} -E copy "${_chmod_binary}" "${_stage_dir}/bin/chmod"
            COMMAND ${CMAKE_COMMAND} -E copy "${_chown_binary}" "${_stage_dir}/bin/chown"
            COMMAND ${CMAKE_COMMAND} -E copy "${_readlink_binary}" "${_stage_dir}/bin/readlink"
            COMMAND ${CMAKE_COMMAND} -E copy "${_mktemp_binary}" "${_stage_dir}/bin/mktemp"
            COMMAND ${CMAKE_COMMAND} -E copy "${_expr_binary}" "${_stage_dir}/bin/expr"
            COMMAND ${CMAKE_COMMAND} -E copy "${_hexdump_binary}" "${_stage_dir}/bin/hexdump"
            COMMAND ${CMAKE_COMMAND} -E copy "${_du_binary}" "${_stage_dir}/bin/du"
            COMMAND ${CMAKE_COMMAND} -E copy "${_date_binary}" "${_stage_dir}/bin/date"
            COMMAND ${CMAKE_COMMAND} -E copy "${_df_binary}" "${_stage_dir}/bin/df"
            COMMAND ${CMAKE_COMMAND} -E copy "${_md5sum_binary}" "${_stage_dir}/bin/md5sum"
            COMMAND ${CMAKE_COMMAND} -E copy "${_tar_binary}" "${_stage_dir}/bin/tar"
            COMMAND ${CMAKE_COMMAND} -E copy "${_find_binary}" "${_stage_dir}/bin/find"
            COMMAND ${CMAKE_COMMAND} -E copy "${_ps_binary}" "${_stage_dir}/bin/ps"
            COMMAND ${CMAKE_COMMAND} -E copy "${_awk_binary}" "${_stage_dir}/bin/awk"
            COMMAND ${CMAKE_COMMAND} -E copy "${_diff_binary}" "${_stage_dir}/bin/diff"
            COMMAND ${CMAKE_COMMAND} -E copy "${_patch_binary}" "${_stage_dir}/bin/patch"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt" "${_stage_dir}/etc/motd"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/grub.cfg" "${_stage_dir}/boot/grub/grub.cfg"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_LOG_ROOT}"
            COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_i486_shell_compat.py
                --stage-dir "${_stage_dir}"
                --report "${_compat_report}"
                --require-file /bin/mksh
                --require-file /bin/sh
                --require-file /bin/xash
                --require-file /boot/mksh
                --require-file /boot/xash
            COMMAND ${XINIM_RESOLVED_GRUB_MKRESCUE_EXECUTABLE}
                --xorriso=${XINIM_XORRISO_EXECUTABLE}
                -o "${_boot_image}"
                "${_stage_dir}"
            DEPENDS
                ${_kernel_target}
                ${_shell_target}
                ${_mksh_target}
                ${_diethello_target}
                ${_ls_target}
                ${_writefile_target}
                ${_mkdir_target}
                ${_unlink_target}
                ${_rmdir_target}
                ${_mv_target}
                ${_seekwrite_target}
                ${_holecheck_target}
                ${_seekpatch_target}
                ${_gapcheck_target}
                ${_hello_target}
                ${_false_target}
                ${_heapprobe_target}
                ${_holdsvc_target}
                ${_posix_util_targets}
                xinim_bootstrap_grub
                "${CMAKE_SOURCE_DIR}/boot/grub/i486/grub.cfg"
                "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt"
                "${CMAKE_SOURCE_DIR}/linker_i486_multiboot2.ld"
                "${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld"
            COMMENT "Generating XINIM ${lane} GRUB boot image"
            VERBATIM
        )

        add_custom_target(${_image_target} DEPENDS "${_boot_image}" "${_ata_disk_image}")
        add_custom_target(run_qemu_${lane}
            COMMAND "${CMAKE_SOURCE_DIR}/scripts/qemu_matrix.py" --build-dir "${CMAKE_BINARY_DIR}" --lane "${lane}" --launch
            DEPENDS ${_image_target}
            USES_TERMINAL
        )
    endif()

    set(_lane_env
        "XINIM_ARTIFACT_ROOT=${XINIM_ARTIFACT_ROOT}"
        "XINIM_STATE_ROOT=${XINIM_STATE_ROOT}"
        "XINIM_PROJECT_ROOT=${XINIM_PROJECT_ROOT}"
        "XINIM_BUILD_ROOT=${XINIM_BUILD_ROOT}"
        "XINIM_IMAGE_ROOT=${XINIM_IMAGE_ROOT}"
        "XINIM_LOG_ROOT=${XINIM_LOG_ROOT}"
        "XINIM_BOOT_LANE_NAME=${lane}"
        "XINIM_BOOT_LANE_BANNER=${_banner_name}"
        "XINIM_QEMU_SYSTEM_BIN=${_qemu_bin}"
        "XINIM_QEMU_CPU=${_qemu_cpu}"
        "XINIM_QEMU_MACHINE=${_qemu_machine}"
        "XINIM_QEMU_MEMORY=${_qemu_memory}"
        "XINIM_QEMU_VGA=${_qemu_vga}"
        "XINIM_QEMU_BOOT_IMAGE=${_boot_image}"
        "XINIM_QEMU_DISK_IMAGE=${_ata_disk_image}"
        "XINIM_QEMU_SMOKE_LOG=${_boot_log}"
        "XINIM_QEMU_SHELL_LOG=${_shell_log}"
    )

    if(TARGET ${_image_target})
        add_test(
            NAME ${_prepare_test}
            COMMAND ${CMAKE_SOURCE_DIR}/test/boot/prepare_boot_image.sh ${_image_target}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_prepare_test} PROPERTIES
            LABELS "integration;boot;${lane};prepare"
            ENVIRONMENT
                "${_lane_env};XINIM_ACTIVE_BUILD_DIR=${CMAKE_BINARY_DIR}"
            FIXTURES_SETUP ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 120
        )

        add_test(
            NAME ${_smoke_test}
            COMMAND ${CMAKE_SOURCE_DIR}/test/boot/x86_32_smoke_test.sh
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_smoke_test} PROPERTIES
            LABELS "integration;boot;${lane}"
            ENVIRONMENT "${_lane_env}"
            FIXTURES_REQUIRED ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 30
        )

        if(NOT _layout_test STREQUAL "")
            add_test(
                NAME ${_layout_test}
                COMMAND ${CMAKE_SOURCE_DIR}/test/boot/x86_32_image_layout_test.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            set_tests_properties(${_layout_test} PROPERTIES
                LABELS "integration;boot;${lane};layout"
                ENVIRONMENT "${_lane_env}"
                FIXTURES_REQUIRED ${lane}_image
                SKIP_RETURN_CODE 77
                TIMEOUT 30
            )
        endif()

        add_test(
            NAME ${_compat_test}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_i486_shell_compat.py
                --stage-dir "${_stage_dir}"
                --report "${_compat_report}"
                --require-file /bin/mksh
                --require-file /bin/sh
                --require-file /bin/xash
                --require-file /boot/mksh
                --require-file /boot/xash
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_compat_test} PROPERTIES
            LABELS "integration;boot;${lane};compat"
            ENVIRONMENT "${_lane_env}"
            FIXTURES_REQUIRED ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 30
        )

        add_test(
            NAME ${_shell_test}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_shell_test.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_shell_test} PROPERTIES
            LABELS "integration;kshell;${lane}"
            ENVIRONMENT "${_lane_env}"
            FIXTURES_REQUIRED ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 120
        )

        add_test(
            NAME ${_persist_test}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_persist_test.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_persist_test} PROPERTIES
            LABELS "integration;boot;${lane};persistence"
            ENVIRONMENT "${_lane_env}"
            FIXTURES_REQUIRED ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 90
        )

        add_test(
            NAME ${_mutation_test}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_ext2_mutation_test.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_mutation_test} PROPERTIES
            LABELS "integration;boot;${lane};ext2;mutation"
            ENVIRONMENT "${_lane_env}"
            FIXTURES_REQUIRED ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 120
        )

        if(lane STREQUAL "i486")
            set(_isapc_boot_log "${XINIM_LOG_ROOT}/${lane}-isapc-smoke.log")
            set(_isapc_shell_log "${XINIM_LOG_ROOT}/${lane}-isapc-kshell.log")
            set(_isapc_env
                ${_lane_env}
                "XINIM_QEMU_MACHINE=isapc"
                "XINIM_QEMU_SMOKE_LOG=${_isapc_boot_log}"
                "XINIM_QEMU_SHELL_LOG=${_isapc_shell_log}"
            )

            add_test(
                NAME i486_isapc_boot_smoke_test
                COMMAND ${CMAKE_SOURCE_DIR}/test/boot/x86_32_smoke_test.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            set_tests_properties(i486_isapc_boot_smoke_test PROPERTIES
                LABELS "integration;boot;${lane};isapc"
                ENVIRONMENT "${_isapc_env}"
                FIXTURES_REQUIRED ${lane}_image
                SKIP_RETURN_CODE 77
                TIMEOUT 30
            )

            add_test(
                NAME i486_isapc_kshell_test
                COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_shell_test.py
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            set_tests_properties(i486_isapc_kshell_test PROPERTIES
                LABELS "integration;kshell;${lane};isapc"
                ENVIRONMENT "${_isapc_env}"
                FIXTURES_REQUIRED ${lane}_image
                SKIP_RETURN_CODE 77
                TIMEOUT 120
            )

            add_test(
                NAME i486_isapc_persistence_test
                COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_persist_test.py
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            set_tests_properties(i486_isapc_persistence_test PROPERTIES
                LABELS "integration;boot;${lane};isapc;persistence"
                ENVIRONMENT "${_isapc_env}"
                FIXTURES_REQUIRED ${lane}_image
                SKIP_RETURN_CODE 77
                TIMEOUT 90
            )

            add_test(
                NAME i486_isapc_ext2_mutation_test
                COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_ext2_mutation_test.py
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            set_tests_properties(i486_isapc_ext2_mutation_test PROPERTIES
                LABELS "integration;boot;${lane};isapc;ext2;mutation"
                ENVIRONMENT "${_isapc_env}"
                FIXTURES_REQUIRED ${lane}_image
                SKIP_RETURN_CODE 77
                TIMEOUT 120
            )
        endif()
    endif()
endfunction()

if(XINIM_ACTIVE_LANE_FAMILY STREQUAL "x86_32")
    xinim_add_x86_32_lane(${XINIM_CPU_LANE})
    if(XINIM_XORRISO_EXECUTABLE)
        xinim_get_x86_lane_property(${XINIM_CPU_LANE} image_target _xinim_active_x86_32_image_target)
        add_custom_target(xinim_x86_32_images DEPENDS ${_xinim_active_x86_32_image_target})
    else()
        message(WARNING
            "x86_32 boot image targets require xorriso. Set "
            "XINIM_XORRISO_EXECUTABLE or install xorriso if you want the "
            "current lane's GRUB image target.")
    endif()
endif()

xinim_write_x86_lane_manifest("${CMAKE_BINARY_DIR}/x86_cpu_lanes.json" "${XINIM_CPU_LANE}")

add_custom_target(xinim_audit_build_coverage
    COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_ARTIFACT_ROOT}/analysis"
    COMMAND python3
            ${CMAKE_SOURCE_DIR}/scripts/audit_build_coverage.py
            --repo-root ${CMAKE_SOURCE_DIR}
            --markdown-output ${XINIM_ARTIFACT_ROOT}/analysis/build_graph_audit.md
            --json-output ${XINIM_ARTIFACT_ROOT}/analysis/build_graph_audit.json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Auditing first-party build graph coverage"
    VERBATIM
)

add_custom_target(xinim_verify_posix_language
    COMMAND python3
            ${CMAKE_SOURCE_DIR}/scripts/verify_posix_language.py
            --repo-root ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Verifying active-doc POSIX language"
    VERBATIM
)

# =============================================================================
# Hosted Userland Shells
# =============================================================================

if(XINIM_CPU_LANE STREQUAL "x86_64")
set(XINIM_HOST_COMMAND_RUNTIME_DIR "${XINIM_ARTIFACT_ROOT}/hostbin")

add_library(xinim_fs_host_compat STATIC
    src/host/filesystem_host_compat.cpp
)
target_compile_features(xinim_fs_host_compat PUBLIC cxx_std_23)
target_include_directories(xinim_fs_host_compat PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/xinim
)
target_compile_options(xinim_fs_host_compat PRIVATE -stdlib=libc++ -fexceptions -frtti)
target_link_options(xinim_fs_host_compat PRIVATE -stdlib=libc++)
xinim_set_warnings(xinim_fs_host_compat)

function(xinim_add_host_command TARGET_NAME OUTPUT_NAME SOURCE_PATH)
    add_executable(${TARGET_NAME} ${SOURCE_PATH})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/xinim
        ${CMAKE_SOURCE_DIR}/src/kernel
    )
    target_compile_options(${TARGET_NAME} PRIVATE -stdlib=libc++ -fexceptions -frtti)
    target_link_options(${TARGET_NAME} PRIVATE -stdlib=libc++)
    target_link_libraries(${TARGET_NAME} PRIVATE xinim_fs_host_compat)
    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${OUTPUT_NAME}
        RUNTIME_OUTPUT_DIRECTORY "${XINIM_HOST_COMMAND_RUNTIME_DIR}"
    )
    xinim_set_warnings(${TARGET_NAME})
endfunction()

xinim_add_host_command(xinim_cmd_echo echo src/commands/echo.cpp)
xinim_add_host_command(xinim_cmd_pwd pwd src/commands/pwd.cpp)
xinim_add_host_command(xinim_cmd_true true src/commands/true_cpp23.cpp)
xinim_add_host_command(xinim_cmd_false false src/commands/false_cpp23.cpp)
xinim_add_host_command(xinim_cmd_mkdir mkdir src/commands/mkdir.cpp)
xinim_add_host_command(xinim_cmd_rm rm src/commands/rm.cpp)
xinim_add_host_command(xinim_cmd_touch touch src/commands/touch.cpp)
xinim_add_host_command(xinim_cmd_cp cp src/commands/cp.cpp)
xinim_add_host_command(xinim_cmd_ls ls src/commands/ls.cpp)
xinim_add_host_command(xinim_cmd_cat cat src/commands/cat_cpp23.cpp)
xinim_add_host_command(xinim_cmd_wc wc src/commands/wc.cpp)
xinim_add_host_command(xinim_cmd_ln ln src/commands/ln.cpp)
xinim_add_host_command(xinim_cmd_chmod chmod src/commands/chmod.cpp)
xinim_add_host_command(xinim_cmd_basename basename src/commands/basename.cpp)
xinim_add_host_command(xinim_cmd_head head src/commands/head.cpp)
xinim_add_host_command(xinim_cmd_env env src/commands/env_cpp23.cpp)
xinim_add_host_command(xinim_cmd_sleep sleep src/commands/sleep.cpp)
xinim_add_host_command(xinim_cmd_rev rev src/commands/rev.cpp)
xinim_add_host_command(xinim_cmd_tee tee src/commands/tee.cpp)
xinim_add_host_command(xinim_cmd_cut cut src/commands/cut_cpp23.cpp)
xinim_add_host_command(xinim_cmd_rmdir rmdir src/commands/rmdir.cpp)
xinim_add_host_command(xinim_cmd_sum sum src/commands/sum.cpp)
xinim_add_host_command(xinim_cmd_tr tr src/commands/tr.cpp)
xinim_add_host_command(xinim_cmd_date date src/commands/date.cpp)
xinim_add_host_command(xinim_cmd_comm comm src/commands/comm.cpp)
xinim_add_host_command(xinim_cmd_sync sync src/commands/sync.cpp)
xinim_add_host_command(xinim_cmd_cmp cmp src/commands/cmp.cpp)
xinim_add_host_command(xinim_cmd_uniq uniq src/commands/uniq.cpp)
xinim_add_host_command(xinim_cmd_od od src/commands/od.cpp)
xinim_add_host_command(xinim_cmd_kill kill src/commands/kill.cpp)
xinim_add_host_command(xinim_cmd_chown chown src/commands/chown.cpp)
xinim_add_host_command(xinim_cmd_mv mv src/commands/mv.cpp)
xinim_add_host_command(xinim_cmd_df df src/commands/df.cpp)

add_executable(xinim_sh_host
    userland/shell/xinim-sh/main.cpp
    userland/shell/xinim-sh/parser.cpp
    userland/shell/xinim-sh/execute.cpp
    userland/shell/xinim-sh/job_control.cpp
    userland/shell/xinim-sh/builtins.cpp
)
set_target_properties(xinim_sh_host PROPERTIES OUTPUT_NAME xash)
xinim_apply_target_defaults(xinim_sh_host)
xinim_set_warnings(xinim_sh_host)
target_include_directories(xinim_sh_host PRIVATE
    ${CMAKE_SOURCE_DIR}/userland/shell/xinim-sh
)

add_executable(xinim_userland_hello_host
    userland/tests/hello.c
)
set_target_properties(xinim_userland_hello_host PROPERTIES
    OUTPUT_NAME xinim-hello
    RUNTIME_OUTPUT_DIRECTORY "${XINIM_HOST_COMMAND_RUNTIME_DIR}"
)
target_compile_features(xinim_userland_hello_host PRIVATE c_std_11)
xinim_set_warnings(xinim_userland_hello_host)

add_library(xinim_mksh_integration_reference STATIC
    userland/shell/mksh/integration/xinim_job_control.c
    userland/shell/mksh/integration/xinim_syscalls.c
    userland/shell/mksh/integration/xinim_terminal.c
)
target_compile_features(xinim_mksh_integration_reference PRIVATE c_std_11)
target_compile_definitions(xinim_mksh_integration_reference PRIVATE _GNU_SOURCE)
xinim_set_warnings(xinim_mksh_integration_reference)

add_custom_target(xinim_tools_hosted DEPENDS xinim_sh_host xinim_userland_hello_host xinim_mksh_integration_reference)
add_custom_target(xash_hosted DEPENDS xinim_sh_host)
add_custom_target(xash_host DEPENDS xinim_sh_host)
add_custom_target(xinim_commands_hosted DEPENDS
    xinim_cmd_echo
    xinim_cmd_pwd
    xinim_cmd_true
    xinim_cmd_false
    xinim_cmd_mkdir
    xinim_cmd_rm
    xinim_cmd_touch
    xinim_cmd_cp
    xinim_cmd_ls
    xinim_cmd_cat
    xinim_cmd_wc
    xinim_cmd_ln
    xinim_cmd_chmod
    xinim_cmd_basename
    xinim_cmd_head
    xinim_cmd_env
    xinim_cmd_sleep
    xinim_cmd_rev
    xinim_cmd_tee
    xinim_cmd_cut
    xinim_cmd_rmdir
    xinim_cmd_sum
    xinim_cmd_tr
    xinim_cmd_date
    xinim_cmd_comm
    xinim_cmd_sync
    xinim_cmd_cmp
    xinim_cmd_uniq
    xinim_cmd_od
    xinim_cmd_kill
    xinim_cmd_chown
    xinim_cmd_mv
    xinim_cmd_df
)

add_library(xinim_user_abi_i486_smoke OBJECT
    test/userland/i486_syscall_smoke.cpp
)
target_compile_features(xinim_user_abi_i486_smoke PRIVATE cxx_std_23)
target_include_directories(xinim_user_abi_i486_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_options(xinim_user_abi_i486_smoke PRIVATE
    -ffreestanding
    -fno-pic
    -fno-pie
    -fno-stack-protector
    -m32
    -march=i486
    -mtune=i486
    -mno-mmx
    -mno-sse
)
xinim_set_warnings(xinim_user_abi_i486_smoke)

# =============================================================================
# Kernel Binary Existence Check
# =============================================================================
# Verifies the xinim kernel binary was produced by the build.

add_test(NAME build_check
    COMMAND ${CMAKE_COMMAND} -E true
)
set_tests_properties(build_check PROPERTIES LABELS "build")

# =============================================================================
# Host-Side Unit Tests (compiled as regular hosted C++23 executables)
# =============================================================================
# These tests run on the build host, not inside the kernel.
# They exercise self-contained kernel data structures that don't require
# bare-metal support (schedulers, wait graphs, sync primitives, etc.).

# Common sources shared by multiple host tests
set(XINIM_TEST_COMMON_SOURCES
    src/kernel/wait_graph.cpp
)

set(XINIM_TEST_SCHED_SOURCES
    ${XINIM_TEST_COMMON_SOURCES}
    src/kernel/schedule.cpp
    src/kernel/service.cpp
)

# Capability mutex needs octonion + lock_manager + schedule + service
set(XINIM_TEST_CAPMUTEX_SOURCES
    ${XINIM_TEST_SCHED_SOURCES}
    src/kernel/capability_mutex.cpp
    src/kernel/lock_manager.cpp
)

# Helper function to configure a host-side test target.
# Host tests compile as regular hosted C++23 (exceptions, RTTI, threads).
# They DO NOT inherit -ffreestanding (that is only for the kernel target).
function(xinim_add_host_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_23)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/xinim
        ${CMAKE_SOURCE_DIR}/src/kernel
    )
    target_compile_options(${TEST_NAME} PRIVATE -stdlib=libc++ -pthread)
    target_link_options(${TEST_NAME} PRIVATE -stdlib=libc++ -pthread)
    xinim_set_warnings(${TEST_NAME})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30 LABELS "unit")
endfunction()

# --- Wait Graph Tests ---
xinim_add_host_test(test_wait_graph
    test/test_wait_graph.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)

# --- Scheduler Tests ---
xinim_add_host_test(test_scheduler
    test/test_scheduler.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

# --- Service Contract Tests ---
xinim_add_host_test(test_service_contract
    test/test_service_contract.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

# --- Sync Primitive Tests ---
xinim_add_host_test(test_ticket_spinlock
    test/test_ticket_spinlock.cpp
)

xinim_add_host_test(test_mcs_spinlock
    test/test_mcs_spinlock.cpp
)

xinim_add_host_test(test_phase_rwlock
    test/test_phase_rwlock.cpp
)

# adaptive_mutex test skipped: header calls sched::scheduler.is_blocked()
# which is not yet implemented in the Scheduler class.

xinim_add_host_test(test_capability_mutex
    test/test_capability_mutex.cpp
    ${XINIM_TEST_CAPMUTEX_SOURCES}
)

# --- Additional Scheduler Tests ---
xinim_add_host_test(test_scheduler_edge
    test/test_scheduler_edge.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

xinim_add_host_test(test_scheduler_deadlock
    test/test_scheduler_deadlock.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

# --- Data Structure Tests (header-only, no additional sources) ---
xinim_add_host_test(test_octonion
    test/test_octonion.cpp
)

xinim_add_host_test(test_fano_multiply
    test/test_fano_multiply.cpp
)

xinim_add_host_test(test_core_types
    test/test_core_types.cpp
)

# --- Crypto Tests (host-side, no kernel deps) ---
xinim_add_host_test(test_fips202
    test/test_fips202.cpp
    src/crypto/kyber_impl/fips202.cpp
)
target_include_directories(test_fips202 PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

xinim_add_host_test(test_ntt_roundtrip
    test/test_ntt_roundtrip.cpp
    src/crypto/kyber_impl/ntt.cpp
)
target_include_directories(test_ntt_roundtrip PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

xinim_add_host_test(test_montgomery_reduce
    test/test_montgomery_reduce.cpp
)
target_include_directories(test_montgomery_reduce PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

# --- Wait Graph Additional Tests ---
xinim_add_host_test(test_wait_graph_stress
    test/test_wait_graph_stress.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)

# --- Lock Manager Tests ---
xinim_add_host_test(test_lock_manager
    test/test_lock_manager.cpp
    ${XINIM_TEST_CAPMUTEX_SOURCES}
)

# --- Service Manager Tests ---
xinim_add_host_test(test_service_manager
    test/test_service_manager.cpp
    src/kernel/service.cpp
)

# --- Crypto Constants Tests (header-only) ---
xinim_add_host_test(test_kyber_constants
    test/test_kyber_constants.cpp
)
target_include_directories(test_kyber_constants PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

# Apply subcategory labels for test filtering.
# The macro already applies LABELS "unit"; these extend it.
set_tests_properties(test_wait_graph test_wait_graph_stress PROPERTIES LABELS "unit;kernel")
set_tests_properties(test_scheduler test_scheduler_edge test_scheduler_deadlock test_service_contract PROPERTIES LABELS "unit;scheduler")
set_tests_properties(test_ticket_spinlock test_mcs_spinlock test_phase_rwlock test_capability_mutex test_lock_manager PROPERTIES LABELS "unit;sync")
# test_ticket_spinlock contains a time-sensitive FIFO fairness test that
# relies on a 10ms sleep; it fails when CTest parallelism starves threads.
# Reserve all logical CPUs so the test runs in a quiet environment.
set_tests_properties(test_ticket_spinlock PROPERTIES PROCESSORS 4)
set_tests_properties(test_fips202 test_ntt_roundtrip test_montgomery_reduce test_kyber_constants PROPERTIES LABELS "unit;crypto")
set_tests_properties(test_octonion test_fano_multiply test_core_types PROPERTIES LABELS "unit;math")
set_tests_properties(test_service_manager PROPERTIES LABELS "unit;kernel")

# --- Kyber512 E2E Compile/Constant Validation ---
xinim_add_host_test(test_kyber_e2e
    test/test_kyber_e2e.cpp
)
target_include_directories(test_kyber_e2e PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)
target_compile_definitions(test_kyber_e2e PRIVATE KYBER_K=2)
set_tests_properties(test_kyber_e2e PROPERTIES LABELS "unit;crypto")

# --- Kyber768 KEM Full Round-Trip Tests ---
xinim_add_host_test(test_kyber_kem
    test/test_kyber_kem.cpp
    src/crypto/kyber_impl/fips202.cpp
    src/crypto/kyber_impl/ntt.cpp
    src/crypto/kyber_impl/poly.cpp
    src/crypto/kyber_impl/polyvec.cpp
    src/crypto/kyber_impl/cbd.cpp
    src/crypto/kyber_impl/reduce.cpp
    src/crypto/kyber_impl/verify.cpp
    src/crypto/kyber_impl/symmetric.cpp
    src/crypto/kyber_impl/indcpa.cpp
    src/crypto/kyber_impl/kem.cpp
    src/crypto/kyber_impl/randombytes.cpp
)
target_include_directories(test_kyber_kem PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)
set_tests_properties(test_kyber_kem PROPERTIES LABELS "unit;crypto")

# --- Hardware Crypto (AES-NI + SHA-NI) Tests ---
xinim_add_host_test(test_hw_crypto
    test/test_hw_crypto.cpp
    src/crypto/aes_hw.cpp
    src/crypto/sha2_hw.cpp
)
target_compile_options(test_hw_crypto PRIVATE -maes -msha -msse4.1 -mssse3)
target_compile_definitions(test_hw_crypto PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_hw_crypto PROPERTIES LABELS "unit;crypto")

# --- Recovery DAG (Resurrection Server) Tests ---
xinim_add_host_test(test_recovery_dag
    test/test_recovery_dag.cpp
    src/kernel/recovery/recovery_dag.cpp
)
set_tests_properties(test_recovery_dag PROPERTIES LABELS "unit;kernel")

# --- Service Manager API Tests ---
xinim_add_host_test(test_service_manager_api
    test/test_service_manager_api.cpp
    src/kernel/service.cpp
)
set_tests_properties(test_service_manager_api PROPERTIES LABELS "unit;kernel")

# --- Net Driver Stub Tests ---
xinim_add_host_test(test_net_driver_stub
    test/test_net_driver_stub.cpp
    src/kernel/net_driver.cpp
)
target_include_directories(test_net_driver_stub PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_tests_properties(test_net_driver_stub PROPERTIES LABELS "unit;kernel")

# --- Syscall Dispatch Table Tests ---
xinim_add_host_test(test_syscall_dispatch
    test/test_syscall_dispatch.cpp
)
target_include_directories(test_syscall_dispatch PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_syscall_dispatch PROPERTIES LABELS "unit;kernel")

# --- IPC Channel Tests (lattice Channel inline methods, header-only) ---
xinim_add_host_test(test_ipc_channel
    test/test_ipc_channel.cpp
)
target_include_directories(test_ipc_channel PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_ipc_channel PROPERTIES LABELS "unit;ipc;kernel")

# --- Unified Scheduler Tests ---
xinim_add_host_test(test_unified_scheduler
    test/test_unified_scheduler.cpp
    src/kernel/unified_scheduler.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)
target_compile_definitions(test_unified_scheduler PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_unified_scheduler PROPERTIES LABELS "unit;scheduler")

# --- Heap Allocator Tests ---
xinim_add_host_test(test_heap_allocator
    test/test_heap_allocator.cpp
    src/kernel/heap.cpp
)
set_tests_properties(test_heap_allocator PROPERTIES LABELS "unit;kernel")

# --- Process Lifecycle Tests ---
xinim_add_host_test(test_process_lifecycle
    test/test_process_lifecycle.cpp
    src/kernel/unified_scheduler.cpp
    src/kernel/process_lifecycle.cpp
    src/kernel/heap.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)
target_compile_definitions(test_process_lifecycle PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_process_lifecycle PROPERTIES LABELS "unit;kernel")

# --- Lock Timeout Tests ---
xinim_add_host_test(test_lock_timeout
    test/test_lock_timeout.cpp
)
target_include_directories(test_lock_timeout PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_lock_timeout PROPERTIES LABELS "unit;sync")

# --- IPC Blocking Tests ---
xinim_add_host_test(test_ipc_blocking
    test/test_ipc_blocking.cpp
)
target_include_directories(test_ipc_blocking PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_ipc_blocking PROPERTIES LABELS "unit;ipc;kernel")

# --- IPC Exhaustion Tests ---
xinim_add_host_test(test_ipc_exhaustion
    test/test_ipc_exhaustion.cpp
)
target_include_directories(test_ipc_exhaustion PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_ipc_exhaustion PROPERTIES LABELS "unit;ipc;kernel")

# --- ELF Parser Tests ---
# Links elf_loader.cpp which calls early_serial; serial_stub.cpp satisfies that dep.
xinim_add_host_test(test_elf_parser
    test/test_elf_parser.cpp
    src/kernel/elf_loader.cpp
    test/stubs/serial_stub.cpp
)
target_include_directories(test_elf_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src/kernel
)
# serial_stub.cpp provides no-op methods; the class has private members only used
# in the real Serial16550 implementation.  Suppress the unused-field warning for
# this host-only stub target.
target_compile_options(test_elf_parser PRIVATE -Wno-unused-private-field)
set_tests_properties(test_elf_parser PROPERTIES LABELS "unit;kernel")

# --- Bare-Metal VFS Tests ---
# Links all bare-metal VFS subsystem sources (freestanding-compatible, no kernel deps).
xinim_add_host_test(test_bare_vfs
    test/test_bare_vfs.cpp
    src/vfs/core_init.cpp
    src/vfs/inode_table.cpp
    src/vfs/vnode_table.cpp
    src/vfs/dirent.cpp
    src/vfs/path_walk.cpp
    src/vfs/seed.cpp
    src/vfs/ramfs_ops.cpp
    src/vfs/mount_table.cpp
    src/vfs/fd_table.cpp
    src/vfs/buffer_cache.cpp
)
target_include_directories(test_bare_vfs PRIVATE
    ${CMAKE_SOURCE_DIR}/src/vfs
    ${CMAKE_SOURCE_DIR}/include
)
set_tests_properties(test_bare_vfs PROPERTIES LABELS "unit;vfs;kernel")

# --- VFS Extended Tests (v1.4.0: lseek, dup/dup2, arena free list) ---
xinim_add_host_test(test_vfs_extended
    test/test_vfs_extended.cpp
    src/vfs/core_init.cpp
    src/vfs/inode_table.cpp
    src/vfs/vnode_table.cpp
    src/vfs/dirent.cpp
    src/vfs/path_walk.cpp
    src/vfs/seed.cpp
    src/vfs/ramfs_ops.cpp
    src/vfs/mount_table.cpp
    src/vfs/fd_table.cpp
    src/vfs/buffer_cache.cpp
)
target_include_directories(test_vfs_extended PRIVATE
    ${CMAKE_SOURCE_DIR}/src/vfs
    ${CMAKE_SOURCE_DIR}/include
)
set_tests_properties(test_vfs_extended PROPERTIES LABELS "unit;vfs;kernel")

xinim_add_host_test(test_bootfs_promote
    test/test_bootfs_promote.cpp
    src/kernel/i486/bootfs.cpp
    src/kernel/x86_64/i486_runtime_compat.cpp
    src/vfs/core_init.cpp
    src/vfs/inode_table.cpp
    src/vfs/vnode_table.cpp
    src/vfs/dirent.cpp
    src/vfs/path_walk.cpp
    src/vfs/seed.cpp
    src/vfs/bootfs_promote.cpp
    src/vfs/ramfs_ops.cpp
    src/vfs/mount_table.cpp
    src/vfs/fd_table.cpp
    src/vfs/buffer_cache.cpp
)
target_include_directories(test_bootfs_promote PRIVATE
    ${CMAKE_SOURCE_DIR}/src/vfs
    ${CMAKE_SOURCE_DIR}/src/kernel
    ${CMAKE_SOURCE_DIR}/include
)
set_tests_properties(test_bootfs_promote PROPERTIES LABELS "unit;vfs;bootfs;kernel")

add_test(
    NAME xash_host_smoke_test
    COMMAND sh ${CMAKE_SOURCE_DIR}/test/userland/xash_smoke_test.sh
)
set_tests_properties(xash_host_smoke_test PROPERTIES
    LABELS "integration;userland;shell"
    ENVIRONMENT
        "XINIM_ARTIFACT_ROOT=${XINIM_ARTIFACT_ROOT};XINIM_STATE_ROOT=${XINIM_STATE_ROOT};XINIM_PROJECT_ROOT=${XINIM_PROJECT_ROOT};XINIM_BUILD_ROOT=${XINIM_BUILD_ROOT}"
    TIMEOUT 15
)

add_test(
    NAME hosted_commands_smoke_test
    COMMAND sh ${CMAKE_SOURCE_DIR}/test/userland/host_commands_smoke_test.sh
)
set_tests_properties(hosted_commands_smoke_test PROPERTIES
    LABELS "integration;userland;commands"
    ENVIRONMENT "XINIM_HOSTBIN=${XINIM_HOST_COMMAND_RUNTIME_DIR}"
    TIMEOUT 15
)

add_test(
    NAME userland_hello_host_test
    COMMAND $<TARGET_FILE:xinim_userland_hello_host>
)
set_tests_properties(userland_hello_host_test PROPERTIES
    LABELS "integration;userland"
    PASS_REGULAR_EXPRESSION "Hello from XINIM userspace!"
    TIMEOUT 15
)

add_test(
    NAME posix_language_check
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/verify_posix_language.py --repo-root ${CMAKE_SOURCE_DIR}
)
set_tests_properties(posix_language_check PROPERTIES
    LABELS "docs;claims;posix"
    TIMEOUT 15
)

xinim_add_host_test(test_exec_stack
    test/test_exec_stack.cpp
    src/kernel/exec_stack.cpp
    test/stubs/serial_stub.cpp
)
target_compile_options(test_exec_stack PRIVATE -Wno-unused-private-field)
set_tests_properties(test_exec_stack PROPERTIES LABELS "unit;kernel;exec")

xinim_add_host_test(test_pmm
    test/test_pmm.cpp
    src/mm/pmm.cpp
    src/kernel/log.cpp
)
set_tests_properties(test_pmm PROPERTIES LABELS "unit;mm")

xinim_add_host_test(test_monotonic
    test/test_monotonic.cpp
    src/kernel/time/monotonic.cpp
)
set_tests_properties(test_monotonic PROPERTIES LABELS "unit;kernel;time")

xinim_add_host_test(test_uaccess
    test/test_uaccess.cpp
    src/kernel/uaccess.cpp
)
set_tests_properties(test_uaccess PROPERTIES LABELS "unit;kernel;uaccess")

xinim_add_host_test(test_kernel_fd_table
    test/test_kernel_fd_table.cpp
    src/kernel/fd_table.cpp
)
target_include_directories(test_kernel_fd_table PRIVATE
    ${CMAKE_SOURCE_DIR}
)
set_tests_properties(test_kernel_fd_table PROPERTIES LABELS "unit;kernel;fd")

xinim_add_host_test(test_kernel_syscall_table
    test/test_kernel_syscall_table.cpp
    src/kernel/syscall_table.cpp
    src/kernel/abi/lattice_changer.cpp
    test/stubs/serial_stub.cpp
)
target_include_directories(test_kernel_syscall_table PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/kernel
)
target_compile_options(test_kernel_syscall_table PRIVATE -Wno-unused-private-field)
target_compile_definitions(test_kernel_syscall_table PRIVATE XINIM_ENABLE_LATTICE_CHANGER=1)
set_tests_properties(test_kernel_syscall_table PROPERTIES LABELS "unit;kernel;syscall")

xinim_add_host_test(test_abi_lattice_changer
    test/test_abi_lattice_changer.cpp
    src/kernel/abi/lattice_changer.cpp
)
target_include_directories(test_abi_lattice_changer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
set_tests_properties(test_abi_lattice_changer PROPERTIES LABELS "unit;abi;syscall")

xinim_add_host_test(test_kernel_pipe
    test/test_kernel_pipe.cpp
    src/kernel/pipe.cpp
)
target_include_directories(test_kernel_pipe PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/kernel
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_definitions(test_kernel_pipe PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_kernel_pipe PROPERTIES LABELS "unit;kernel;pipe")

xinim_add_host_test(test_pci_driver_bindings
    test/test_pci_driver_bindings.cpp
)
target_include_directories(test_pci_driver_bindings PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_definitions(test_pci_driver_bindings PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_pci_driver_bindings PROPERTIES LABELS "unit;pci;drivers;x86")

xinim_add_host_test(test_meminfo
    test/test_meminfo.cpp
    src/mm/meminfo.cpp
    src/mm/dma_allocator.cpp
    src/mm/pmm.cpp
    src/kernel/log.cpp
)
set_tests_properties(test_meminfo PROPERTIES LABELS "unit;mm;kernel")

xinim_add_host_test(test_memory_initialize
    test/test_memory_initialize.cpp
    src/mm/memory.cpp
)
set_tests_properties(test_memory_initialize PROPERTIES LABELS "unit;mm")

xinim_add_host_test(test_dma_compat
    test/test_dma_compat.cpp
    src/mm/dma.cpp
    src/mm/dma_allocator.cpp
    src/mm/pmm.cpp
    src/kernel/log.cpp
)
set_tests_properties(test_dma_compat PROPERTIES LABELS "unit;mm;dma")

if(TARGET xinim_fs_host_compat)
    xinim_add_host_test(test_mode_parser
        src/commands/tests/test_mode_parser.cpp
    )
    set_tests_properties(test_mode_parser PROPERTIES LABELS "unit;userland;commands")

    xinim_add_host_test(test_xinim_fs_links
        src/commands/tests/test_xinim_fs_links.cpp
    )
    target_link_libraries(test_xinim_fs_links PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_links PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_links PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_links PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_change_permissions
        src/commands/tests/test_xinim_fs_change_permissions.cpp
    )
    target_link_libraries(test_xinim_fs_change_permissions PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_change_permissions PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_change_permissions PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_change_permissions PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_touch_operations
        src/commands/tests/test_xinim_fs_touch_operations.cpp
    )
    target_link_libraries(test_xinim_fs_touch_operations PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_touch_operations PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_touch_operations PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_touch_operations PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_remove
        src/commands/tests/test_xinim_fs_remove.cpp
    )
    target_link_libraries(test_xinim_fs_remove PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_remove PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_remove PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_remove PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_copy_symlink
        src/commands/tests/test_xinim_fs_copy_symlink.cpp
    )
    target_link_libraries(test_xinim_fs_copy_symlink PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_copy_symlink PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_copy_symlink PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_copy_symlink PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_rename
        src/commands/tests/test_xinim_fs_rename.cpp
    )
    target_link_libraries(test_xinim_fs_rename PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_rename PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_rename PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_rename PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_get_status
        src/commands/tests/test_xinim_fs_get_full_status.cpp
    )
    target_link_libraries(test_xinim_fs_get_status PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_get_status PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_get_status PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_get_status PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_create_directory
        src/commands/tests/test_xinim_fs_create_directory.cpp
    )
    target_link_libraries(test_xinim_fs_create_directory PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_create_directory PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_create_directory PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_create_directory PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_create_directories
        src/commands/tests/test_xinim_fs_create_directories.cpp
    )
    target_link_libraries(test_xinim_fs_create_directories PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_create_directories PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_create_directories PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_create_directories PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_copy_file
        src/commands/tests/test_xinim_fs_copy_file.cpp
    )
    target_link_libraries(test_xinim_fs_copy_file PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_copy_file PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_copy_file PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_copy_file PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_copy
        src/commands/tests/test_xinim_fs_copy.cpp
    )
    target_link_libraries(test_xinim_fs_copy PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_copy PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_copy PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_copy PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_xinim_fs_chown_ops
        src/commands/tests/test_xinim_fs_chown_ops.cpp
    )
    target_link_libraries(test_xinim_fs_chown_ops PRIVATE xinim_fs_host_compat)
    target_compile_options(test_xinim_fs_chown_ops PRIVATE -fexceptions -frtti)
    target_link_options(test_xinim_fs_chown_ops PRIVATE -fexceptions -frtti)
    set_tests_properties(test_xinim_fs_chown_ops PROPERTIES LABELS "unit;fs;commands")

    xinim_add_host_test(test_mined_command_registry
        src/commands/tests/test_mined_command_registry.cpp
    )
    set_tests_properties(test_mined_command_registry PROPERTIES LABELS "unit;commands;mined")

    xinim_add_host_test(test_mined_key_command_map
        src/commands/tests/test_mined_key_command_map.cpp
    )
    set_tests_properties(test_mined_key_command_map PROPERTIES LABELS "unit;commands;mined")
endif()

# --- Boot / Integration Tests (QEMU-based, require kernel image) ---
# These are registered but only meaningful when the kernel is built.

if(TARGET xinim_x86_64_image)
    add_test(
        NAME prepare_x86_64_image
        COMMAND ${CMAKE_SOURCE_DIR}/test/boot/prepare_boot_image.sh xinim_x86_64_image
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(prepare_x86_64_image PROPERTIES
        LABELS "integration;boot;prepare"
        ENVIRONMENT
            "XINIM_ARTIFACT_ROOT=${XINIM_ARTIFACT_ROOT};XINIM_STATE_ROOT=${XINIM_STATE_ROOT};XINIM_PROJECT_ROOT=${XINIM_PROJECT_ROOT};XINIM_BUILD_ROOT=${XINIM_BUILD_ROOT};XINIM_IMAGE_ROOT=${XINIM_IMAGE_ROOT};XINIM_LOG_ROOT=${XINIM_LOG_ROOT};XINIM_ACTIVE_BUILD_DIR=${CMAKE_BINARY_DIR};XINIM_TOOLS_ROOT=${XINIM_TOOLS_ROOT}"
        SKIP_RETURN_CODE 77
        FIXTURES_SETUP x86_64_image
        TIMEOUT 120
    )

    add_test(
        NAME boot_smoke_test
        COMMAND ${CMAKE_SOURCE_DIR}/test/boot/smoke_test.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(boot_smoke_test PROPERTIES
        LABELS "integration;boot"
        ENVIRONMENT
            "XINIM_ARTIFACT_ROOT=${XINIM_ARTIFACT_ROOT};XINIM_STATE_ROOT=${XINIM_STATE_ROOT};XINIM_PROJECT_ROOT=${XINIM_PROJECT_ROOT};XINIM_BUILD_ROOT=${XINIM_BUILD_ROOT};XINIM_IMAGE_ROOT=${XINIM_IMAGE_ROOT};XINIM_LOG_ROOT=${XINIM_LOG_ROOT}"
        FIXTURES_REQUIRED x86_64_image
        SKIP_RETURN_CODE 77
        TIMEOUT 30
    )

    add_test(
        NAME x86_64_shell_test
        COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_64_shell_test.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(x86_64_shell_test PROPERTIES
        LABELS "integration;kshell"
        ENVIRONMENT
            "XINIM_ARTIFACT_ROOT=${XINIM_ARTIFACT_ROOT};XINIM_STATE_ROOT=${XINIM_STATE_ROOT};XINIM_PROJECT_ROOT=${XINIM_PROJECT_ROOT};XINIM_BUILD_ROOT=${XINIM_BUILD_ROOT};XINIM_IMAGE_ROOT=${XINIM_IMAGE_ROOT};XINIM_LOG_ROOT=${XINIM_LOG_ROOT}"
        FIXTURES_REQUIRED x86_64_image
        SKIP_RETURN_CODE 77
        TIMEOUT 60
    )
endif()
endif()

# =============================================================================
# Documentation Target (Doxygen)
# =============================================================================

find_program(DOXYGEN_EXECUTABLE doxygen)
if(DOXYGEN_EXECUTABLE)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/docs)
    configure_file(
        ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in
        ${CMAKE_BINARY_DIR}/Doxyfile
        @ONLY
    )
    add_custom_target(xinim_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found; 'cmake --build --preset debug --target xinim_docs' will not work. Install doxygen to generate API docs.")
endif()

# =============================================================================
# Build Targets status
# =============================================================================

message(STATUS "XINIM Baseline Kernel Target Configured.")
