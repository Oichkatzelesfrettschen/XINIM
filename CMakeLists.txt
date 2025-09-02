<<<<<<< HEAD
cmake_minimum_required(VERSION 3.25)
project(XINIM VERSION 1.0.0 LANGUAGES CXX C ASM)

# ══════════════════════════════════════════════════════════════════════════════
# XINIM - CLEAN & UNIFIED BUILD SYSTEM
# World's First Pure C++23 Operating System with POSIX SUSv5 Compliance
# ══════════════════════════════════════════════════════════════════════════════
=======
cmake_minimum_required(VERSION 3.20)
project(XINIM CXX C)

# ─────────────────────────────────────────────────────────────────────────────
# 1. Compiler autodetection (prefer Clang 18 → 20 → default `clang`)
# ─────────────────────────────────────────────────────────────────────────────
if(NOT CMAKE_C_COMPILER)
  find_program(CLANG_18 clang-18)
  find_program(CLANG_20 clang-20)
  set(CMAKE_C_COMPILER
      $<IF:$<BOOL:${CLANG_18}>,${CLANG_18},
        $<IF:$<BOOL:${CLANG_20}>,${CLANG_20},clang>>)
endif()

# Automatically enable libc++ when building with Clang
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
  link_libraries(c++ c++abi)
endif()
if(NOT CMAKE_CXX_COMPILER)
  find_program(CLANGXX_18 clang++-18)
  find_program(CLANGXX_20 clang++-20)
  set(CMAKE_CXX_COMPILER
      $<IF:$<BOOL:${CLANGXX_18}>,${CLANGXX_18},
        $<IF:$<BOOL:${CLANGXX_20}>,${CLANGXX_20},clang++>>)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 2. Language standards
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
>>>>>>> origin/master

# ──────────────────────────────────────────────────────────────────────────────
# § 1. LANGUAGE STANDARDS & POLICIES
# ──────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

<<<<<<< HEAD
# Modern CMake policies
cmake_policy(SET CMP0135 NEW)  # URL download timestamp
cmake_policy(SET CMP0127 NEW)  # cmake_dependent_option
cmake_policy(SET CMP0118 NEW)  # GENERATED property
cmake_policy(SET CMP0002 NEW)  # Logical target names

# ──────────────────────────────────────────────────────────────────────────────
# § 2. BUILD CONFIGURATION
# ──────────────────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# § 3. COMPILER DETECTION & FLAGS
# ──────────────────────────────────────────────────────────────────────────────
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)
include(CheckIPOSupported)

# Detect compiler family
set(XINIM_COMPILER_CLANG OFF)
set(XINIM_COMPILER_GCC OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(XINIM_COMPILER_CLANG ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(XINIM_COMPILER_GCC ON)
endif()

# Base flags
set(XINIM_CXX_FLAGS
    $<$<OR:$<BOOL:${XINIM_COMPILER_CLANG}>,$<BOOL:${XINIM_COMPILER_GCC}>>:
        -Wall -Wextra -Wpedantic -stdlib=libc++
    >
)

set(XINIM_RELEASE_FLAGS
    $<$<CONFIG:Release>:
        -O3 -march=native -DNDEBUG
    >
=======
# ─────────────────────────────────────────────────────────────────────────────
# 3. Cross-compilation support
# ─────────────────────────────────────────────────────────────────────────────
option(CROSS_COMPILE_X86_64 "Cross-compile for freestanding x86-64" OFF)
set(CROSS_PREFIX "" CACHE STRING "Prefix for cross-compilation toolchain")

if(CROSS_COMPILE_X86_64)
  if(CROSS_PREFIX STREQUAL "")
    set(CROSS_PREFIX "x86_64-elf-")
  endif()

  set(CMAKE_SYSTEM_NAME Generic)
  set(CMAKE_SYSTEM_PROCESSOR x86_64)

  set(CMAKE_C_COMPILER   "${CROSS_PREFIX}clang")
  set(CMAKE_CXX_COMPILER "${CROSS_PREFIX}clang++")
  set(CMAKE_AR           "${CROSS_PREFIX}ar")
  set(CMAKE_RANLIB       "${CROSS_PREFIX}ranlib")
  set(CMAKE_LINKER       "${CROSS_PREFIX}ld")

  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -m64")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 4. Global include paths
# ─────────────────────────────────────────────────────────────────────────────
include_directories(
  h
  include
  commands
  fs
  kernel
  mm
  lib
)

# ─────────────────────────────────────────────────────────────────────────────
# 5. Baseline optimisation flags (native or cross)
#    x86-64-v1: MMX + SSE + SSE2, widest deployable subset.
# ─────────────────────────────────────────────────────────────────────────────
set(BASE_CPU_FLAGS
    "-march=x86-64"
    "-mtune=generic"
    "-msse"
    "-mmmx"
    "-msse2"
>>>>>>> origin/master
)

set(XINIM_DEBUG_FLAGS
    $<$<CONFIG:Debug>:
        -O0 -g3 -D_DEBUG
    >
)

# ──────────────────────────────────────────────────────────────────────────────
# § 4. INCLUDE DIRECTORIES
# ──────────────────────────────────────────────────────────────────────────────
set(XINIM_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/xinim
    ${CMAKE_BINARY_DIR}/generated
)

<<<<<<< HEAD
# ──────────────────────────────────────────────────────────────────────────────
# § 5. SOURCE FILE COLLECTION
# ──────────────────────────────────────────────────────────────────────────────
=======
# ─────────────────────────────────────────────────────────────────────────────
# 6. Optional analysis and sanitizers
# ─────────────────────────────────────────────────────────────────────────────
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(ENABLE_CPPCHECK "Run cppcheck during compilation" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_C_CLANG_TIDY   "${CLANG_TIDY_EXE};--warnings-as-errors=*")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--warnings-as-errors=*")
  else()
    message(FATAL_ERROR "clang-tidy requested but executable not found")
  endif()
endif()

if(ENABLE_CPPCHECK)
  find_program(CPPCHECK_EXE NAMES cppcheck)
  if(CPPCHECK_EXE)
    set(CMAKE_C_CPPCHECK   "${CPPCHECK_EXE};--enable=all;--inconclusive;--suppress=missingIncludeSystem;--error-exitcode=1")
    set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE};--enable=all;--inconclusive;--suppress=missingIncludeSystem;--error-exitcode=1")
  else()
    message(FATAL_ERROR "cppcheck requested but executable not found")
  endif()
endif()

if(ENABLE_ASAN AND ENABLE_TSAN)
  message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously")
endif()

if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer)
endif()

if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread -fno-omit-frame-pointer)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 7. Wini driver selection
# ─────────────────────────────────────────────────────────────────────────────
option(DRIVER_AT "Use AT-style WD100x controller driver" OFF)
option(DRIVER_PC "Use PC/XT wini driver" OFF)
>>>>>>> origin/master

# Function to collect sources safely
function(xinim_collect_sources OUT_VAR DIR PATTERNS)
    set(ALL_SOURCES)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${DIR}")
        foreach(PATTERN ${PATTERNS})
            file(GLOB_RECURSE FOUND_FILES "${CMAKE_SOURCE_DIR}/${DIR}/${PATTERN}")
            foreach(FILE ${FOUND_FILES})
                # Skip problematic test files and duplicates
                get_filename_component(FILENAME ${FILE} NAME)
                if(NOT FILENAME MATCHES "^(1-[0-9]|2-[0-9]|3-[0-9]|4-[0-9]|[0-9]-[0-9]|s-c[0-9]|testfrmw|stress|threads_scenarii)" 
                   AND NOT FILE MATCHES "tests/posix_conformance/"
                   AND NOT FILENAME MATCHES " [0-9]\\."
                   AND NOT FILENAME MATCHES "test_fat_time [0-9]")
                    list(APPEND ALL_SOURCES ${FILE})
                endif()
            endforeach()
        endif()
    endif()
    set(${OUT_VAR} ${ALL_SOURCES} PARENT_SCOPE)
endfunction()

# Collect core sources
xinim_collect_sources(KERNEL_SOURCES kernel "*.cpp;*.c;*.S")
xinim_collect_sources(MM_SOURCES mm "*.cpp;*.c")
xinim_collect_sources(FS_SOURCES fs "*.cpp;*.c")
xinim_collect_sources(LIB_SOURCES lib "*.cpp;*.c")
xinim_collect_sources(CRYPTO_SOURCES crypto "*.cpp;*.c")
xinim_collect_sources(COMMAND_SOURCES commands "*.cpp;*.c")

# Collect XINIM test sources (excluding POSIX test suite)
xinim_collect_sources(XINIM_TEST_SOURCES tests "*.cpp;*.c")

# Filter out our own renamed test files to avoid conflicts
set(CLEAN_TEST_SOURCES)
foreach(TEST_FILE ${XINIM_TEST_SOURCES})
    get_filename_component(FILENAME ${TEST_FILE} NAME)
    # Only include our properly named test files
    if(FILENAME MATCHES "^(basic_|memory_|filesystem_|network_|crypto_|scheduler_|service_|lattice_|hypercomplex_|wait_graph_|extended_test_)"
       OR FILENAME MATCHES "^test_(wait_graph|service_serialization|kyber|shared_secret_failure|constant_time_equal|net_driver|fastpath|lattice|scheduler|poll_network|hypercomplex|fat_time|service_contract|service_manager|two_node|svcctl)")
        list(APPEND CLEAN_TEST_SOURCES ${TEST_FILE})
    endif()
endforeach()

# ──────────────────────────────────────────────────────────────────────────────
# § 6. INTERFACE LIBRARIES
# ──────────────────────────────────────────────────────────────────────────────

# Common configuration interface
add_library(xinim_common_interface INTERFACE)
target_compile_features(xinim_common_interface INTERFACE cxx_std_23)
target_compile_options(xinim_common_interface INTERFACE
    ${XINIM_CXX_FLAGS}
    ${XINIM_RELEASE_FLAGS}
    ${XINIM_DEBUG_FLAGS}
)
target_include_directories(xinim_common_interface INTERFACE ${XINIM_INCLUDE_DIRS})

# Kernel interface
add_library(xinim_kernel_interface INTERFACE)
target_compile_options(xinim_kernel_interface INTERFACE
    -ffreestanding -nostdlib -fno-builtin
    -fno-exceptions -fno-rtti
)
target_link_libraries(xinim_kernel_interface INTERFACE xinim_common_interface)

# ──────────────────────────────────────────────────────────────────────────────
# § 7. CORE LIBRARIES
# ──────────────────────────────────────────────────────────────────────────────

# Core libraries
if(KERNEL_SOURCES)
    add_library(xinim_kernel STATIC ${KERNEL_SOURCES})
    target_link_libraries(xinim_kernel PUBLIC xinim_kernel_interface)
endif()

if(MM_SOURCES)
    add_library(xinim_mm STATIC ${MM_SOURCES})
    target_link_libraries(xinim_mm PUBLIC xinim_common_interface)
endif()

<<<<<<< HEAD
if(FS_SOURCES)
    add_library(xinim_fs STATIC ${FS_SOURCES})
    target_link_libraries(xinim_fs PUBLIC xinim_common_interface)
endif()

if(LIB_SOURCES)
    add_library(xinim_libc STATIC ${LIB_SOURCES})
    target_link_libraries(xinim_libc PUBLIC xinim_common_interface)
endif()

if(CRYPTO_SOURCES)
    add_library(xinim_crypto STATIC ${CRYPTO_SOURCES})
    target_link_libraries(xinim_crypto PUBLIC xinim_common_interface)
    
    # Link external crypto libraries if available
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(xinim_crypto PRIVATE OpenSSL::Crypto)
    endif()
endif()

# ──────────────────────────────────────────────────────────────────────────────
# § 8. COMMAND UTILITIES
# ──────────────────────────────────────────────────────────────────────────────

# Build command utilities
foreach(CMD_SOURCE ${COMMAND_SOURCES})
    get_filename_component(CMD_NAME ${CMD_SOURCE} NAME_WE)
    # Skip test files and problematic names
    if(NOT CMD_NAME MATCHES "^test_" AND NOT CMD_NAME MATCHES "legacy_backup")
        add_executable(xinim_cmd_${CMD_NAME} ${CMD_SOURCE})
        target_link_libraries(xinim_cmd_${CMD_NAME} PRIVATE xinim_common_interface)
        
        # Link appropriate libraries based on command type
        if(TARGET xinim_libc)
            target_link_libraries(xinim_cmd_${CMD_NAME} PRIVATE xinim_libc)
        endif()
        if(TARGET xinim_fs AND (CMD_NAME MATCHES "(ls|cp|mv|rm|mkdir|rmdir|find|tar)"))
            target_link_libraries(xinim_cmd_${CMD_NAME} PRIVATE xinim_fs)
        endif()
        if(TARGET xinim_crypto AND (CMD_NAME MATCHES "(encrypt|hash|sign)"))
            target_link_libraries(xinim_cmd_${CMD_NAME} PRIVATE xinim_crypto)
        endif()
        
        # Set output name without prefix
        set_target_properties(xinim_cmd_${CMD_NAME} PROPERTIES OUTPUT_NAME ${CMD_NAME})
    endif()
endforeach()

# ──────────────────────────────────────────────────────────────────────────────
# § 9. KERNEL EXECUTABLE
# ──────────────────────────────────────────────────────────────────────────────

if(EXISTS "${CMAKE_SOURCE_DIR}/kernel/main.cpp" AND TARGET xinim_kernel)
    add_executable(xinim_kernel_exe kernel/main.cpp)
    target_link_libraries(xinim_kernel_exe PRIVATE xinim_kernel)
    
    # Link all subsystems
    if(TARGET xinim_mm)
        target_link_libraries(xinim_kernel_exe PRIVATE xinim_mm)
    endif()
    if(TARGET xinim_fs)
        target_link_libraries(xinim_kernel_exe PRIVATE xinim_fs)
    endif()
    if(TARGET xinim_crypto)
        target_link_libraries(xinim_kernel_exe PRIVATE xinim_crypto)
    endif()
    
    set_target_properties(xinim_kernel_exe PROPERTIES OUTPUT_NAME "xinim.elf")
    
    # Add linker script if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/kernel/linker.ld")
        set_target_properties(xinim_kernel_exe PROPERTIES
            LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/kernel/linker.ld -nostdlib"
        )
    endif()
endif()

# ──────────────────────────────────────────────────────────────────────────────
# § 10. TESTING (XINIM COMPONENTS ONLY)
# ──────────────────────────────────────────────────────────────────────────────

enable_testing()
include(CTest)

# Add our clean test sources only
foreach(TEST_SOURCE ${CLEAN_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    # Create unique test name
    string(REPLACE "/" "_" TEST_TARGET_NAME ${TEST_SOURCE})
    string(REGEX REPLACE "^.*tests_" "" TEST_TARGET_NAME ${TEST_TARGET_NAME})
    string(REGEX REPLACE "\\.cpp$" "" TEST_TARGET_NAME ${TEST_TARGET_NAME})
    set(TEST_TARGET_NAME "xinim_test_${TEST_TARGET_NAME}")
    
    add_executable(${TEST_TARGET_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE xinim_common_interface)
    
    # Link appropriate libraries
    if(TARGET xinim_libc)
        target_link_libraries(${TEST_TARGET_NAME} PRIVATE xinim_libc)
    endif()
    if(TARGET xinim_crypto)
        target_link_libraries(${TEST_TARGET_NAME} PRIVATE xinim_crypto)
    endif()
    if(TARGET xinim_kernel)
        target_link_libraries(${TEST_TARGET_NAME} PRIVATE xinim_kernel)
    endif()
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_TARGET_NAME})
endforeach()

# ──────────────────────────────────────────────────────────────────────────────
# § 11. DOCUMENTATION & UTILITIES
# ──────────────────────────────────────────────────────────────────────────────

# Custom targets
add_custom_target(format
    COMMAND find . -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" |
            grep -v build | head -100 | xargs clang-format -i
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting source code (first 100 files)"
)

add_custom_target(clean_build
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}
    COMMENT "Clean build directory"
)

# ──────────────────────────────────────────────────────────────────────────────
# § 12. BUILD SUMMARY
# ──────────────────────────────────────────────────────────────────────────────

list(LENGTH KERNEL_SOURCES KERNEL_COUNT)
list(LENGTH MM_SOURCES MM_COUNT)
list(LENGTH FS_SOURCES FS_COUNT)
list(LENGTH LIB_SOURCES LIB_COUNT)
list(LENGTH CRYPTO_SOURCES CRYPTO_COUNT)
list(LENGTH COMMAND_SOURCES COMMAND_COUNT)
list(LENGTH CLEAN_TEST_SOURCES TEST_COUNT)

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║                XINIM CLEAN BUILD SYSTEM                       ║")
message(STATUS "╠════════════════════════════════════════════════════════════════╣")
message(STATUS "║ Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "║ C++ Standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "║ C Standard:       C${CMAKE_C_STANDARD}")
message(STATUS "║ Compiler:         ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "╠════════════════════════════════════════════════════════════════╣")
message(STATUS "║ Kernel Sources:   ${KERNEL_COUNT}")
message(STATUS "║ MM Sources:       ${MM_COUNT}")
message(STATUS "║ FS Sources:       ${FS_COUNT}")
message(STATUS "║ Lib Sources:      ${LIB_COUNT}")
message(STATUS "║ Crypto Sources:   ${CRYPTO_COUNT}")
message(STATUS "║ Command Sources:  ${COMMAND_COUNT}")
message(STATUS "║ Test Sources:     ${TEST_COUNT}")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")
message(STATUS "")

# ══════════════════════════════════════════════════════════════════════════════
# END OF CLEAN BUILD SYSTEM
# ══════════════════════════════════════════════════════════════════════════════
=======
# ─────────────────────────────────────────────────────────────────────────────
# 8. Build sub-components
# ─────────────────────────────────────────────────────────────────────────────
option(BUILD_SYSTEM "Build full Minix system components" ON)

if(BUILD_SYSTEM)
  add_subdirectory(lib)
  add_subdirectory(kernel)
  add_subdirectory(mm)
  add_subdirectory(fs)
  add_subdirectory(commands)
  add_subdirectory(tools)
endif()

add_subdirectory(crypto)

# ─────────────────────────────────────────────────────────────────────────────
# 9. Unit tests
# ─────────────────────────────────────────────────────────────────────────────
option(ENABLE_UNIT_TESTS "Build unit tests" ON)

if(ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 10. Documentation target
# ─────────────────────────────────────────────────────────────────────────────
find_package(Doxygen)
if(DOXYGEN_FOUND)
  set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen")
  set(DOXYGEN_INPUT_DIRS
      "${CMAKE_CURRENT_SOURCE_DIR}/commands"
      "${CMAKE_CURRENT_SOURCE_DIR}/fs"
      "${CMAKE_CURRENT_SOURCE_DIR}/kernel"
      "${CMAKE_CURRENT_SOURCE_DIR}/mm"
      "${CMAKE_CURRENT_SOURCE_DIR}/lib"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto")
  list(JOIN DOXYGEN_INPUT_DIRS " " DOXYGEN_INPUT)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
else()
  message(STATUS "Doxygen not found. Documentation target will be unavailable.")
endif()
>>>>>>> origin/master
