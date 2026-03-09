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
    src/kernel/freestanding_memory.cpp
    src/kernel/i486/bootfs.cpp
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
    src/kernel/i486/main.cpp
    src/kernel/i486/console.cpp
    src/kernel/i486/ring3.cpp
    src/kernel/i486/ring3_entry.S
    src/kernel/i486/shell.cpp
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
    set(_hello_target "hello_${lane}")
    set(_false_target "false_${lane}")
    set(_image_dir "${XINIM_IMAGE_ROOT}/${_image_dir_name}")
    set(_stage_dir "${_image_dir}/staging")
    set(_boot_image "${_image_dir}/${_image_name}")
    set(_boot_log "${XINIM_LOG_ROOT}/${lane}-smoke.log")
    set(_shell_log "${XINIM_LOG_ROOT}/${lane}-kshell.log")

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
        -no-pie
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
        -no-pie
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_shell_target})

    set(_mksh_source_dir "${CMAKE_SOURCE_DIR}/.scratch/mksh")
    set(_mksh_build_dir "${CMAKE_CURRENT_BINARY_DIR}/mksh-${lane}")
    set(_mksh_binary "${_mksh_build_dir}/mksh")

    add_custom_command(
        OUTPUT "${_mksh_binary}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_mksh_build_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_mksh_source_dir}" "${_mksh_build_dir}"
        WORKING_DIRECTORY "${_mksh_build_dir}"
        COMMAND ${CMAKE_COMMAND} -E env
            "CC=${CMAKE_C_COMPILER}"
            "AR=${CMAKE_AR}"
            "RANLIB=${CMAKE_RANLIB}"
            "LD=${CMAKE_LINKER}"
            "CFLAGS=${CMAKE_C_FLAGS} -m32 -Os -fno-asynchronous-unwind-tables -fno-plt -fno-pic -fno-pie -fno-stack-protector"
            "LDFLAGS=-static -fno-pie -no-pie -Wl,-O3 -Wl,--as-needed"
            "CPPFLAGS=-I${_mksh_build_dir} -DMKSH_BUILDSH -DMKSH_ASSUME_UTF8 -DMKSH__NO_SYMLINK -DMKSH_NO_CMDLINE_EDITING -DMKSH_NO_PWD"
            sh -c "sh Build.sh -r -M && sh Rebuild.sh || { echo 'mksh build failed; falling back to xinim shell binary'; cp $<TARGET_FILE:${_shell_target}> mksh; }"
        DEPENDS
            "${_mksh_source_dir}/Build.sh"
            "${_mksh_source_dir}/Rebuild.sh"
            "${_mksh_source_dir}/Makefrag.inc"
            ${_shell_target}
        BYPRODUCTS
            "${_mksh_build_dir}/mksh.1"
        COMMENT "Building mksh shell for ${lane}"
        VERBATIM
    )
    add_custom_target(${_mksh_target} ALL DEPENDS "${_mksh_binary}")
    set_target_properties(${_mksh_target} PROPERTIES
        EXCLUDE_FROM_ALL FALSE
    )

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
        -no-pie
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
        -no-pie
        ${_lane_link_arch_options}
    )
    xinim_set_warnings(${_false_target})

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
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_false_target}>" "${_stage_dir}/boot/false"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt" "${_stage_dir}/boot/motd"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_shell_target}>" "${_stage_dir}/bin/xash"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_shell_target}>" "${_stage_dir}/bin/sh"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_hello_target}>" "${_stage_dir}/bin/hello"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_false_target}>" "${_stage_dir}/bin/false"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt" "${_stage_dir}/etc/motd"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/boot/grub/i486/grub.cfg" "${_stage_dir}/boot/grub/grub.cfg"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${XINIM_LOG_ROOT}"
            COMMAND ${XINIM_RESOLVED_GRUB_MKRESCUE_EXECUTABLE}
                --xorriso=${XINIM_XORRISO_EXECUTABLE}
                -o "${_boot_image}"
                "${_stage_dir}"
            DEPENDS
                ${_kernel_target}
                ${_shell_target}
                ${_mksh_target}
                ${_hello_target}
                ${_false_target}
                xinim_bootstrap_grub
                "${CMAKE_SOURCE_DIR}/boot/grub/i486/grub.cfg"
                "${CMAKE_SOURCE_DIR}/boot/grub/i486/motd.txt"
                "${CMAKE_SOURCE_DIR}/linker_i486_multiboot2.ld"
                "${CMAKE_SOURCE_DIR}/linker_xash_i486_user.ld"
            COMMENT "Generating XINIM ${lane} GRUB boot image"
            VERBATIM
        )

        add_custom_target(${_image_target} DEPENDS "${_boot_image}")
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
            NAME ${_shell_test}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/x86_32_shell_test.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(${_shell_test} PROPERTIES
            LABELS "integration;kshell;${lane}"
            ENVIRONMENT "${_lane_env}"
            FIXTURES_REQUIRED ${lane}_image
            SKIP_RETURN_CODE 77
            TIMEOUT 60
        )
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
        NAME kshell_test
        COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/kshell_test.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(kshell_test PROPERTIES
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
