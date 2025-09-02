cmake_minimum_required(VERSION 3.25)
project(XINIM VERSION 1.0.0 LANGUAGES CXX C ASM)

# ══════════════════════════════════════════════════════════════════════════════
# XINIM - CLEAN & UNIFIED BUILD SYSTEM
# World's First Pure C++23 Operating System with POSIX SUSv5 Compliance
# ══════════════════════════════════════════════════════════════════════════════

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

# ──────────────────────────────────────────────────────────────────────────────
# § 5. SOURCE FILE COLLECTION
# ──────────────────────────────────────────────────────────────────────────────

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