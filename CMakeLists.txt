# XINIM - Modern C++23 Post-Quantum Microkernel Operating System
# CMake Build Configuration (Generated from xmake - Secondary Build System)

cmake_minimum_required(VERSION 3.20)
project(XINIM VERSION 1.0.0 LANGUAGES C CXX ASM)

# =============================================================================
# Build Configuration
# =============================================================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build types
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# =============================================================================
# Compiler Flags
# =============================================================================

# Warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    $<$<COMPILE_LANGUAGE:CXX>:-Werror>
)

# Architecture-specific flags
add_compile_definitions(__XINIM_X86_64__)
add_compile_options(-march=x86-64 -mtune=generic)

# POSIX compliance
add_compile_definitions(_XOPEN_SOURCE=700 _GNU_SOURCE)
add_compile_options(-pthread)
add_link_options(-pthread -lrt)

# =============================================================================
# Dependencies
# =============================================================================

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)

# =============================================================================
# Include Directories
# =============================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/xinim
    ${CMAKE_SOURCE_DIR}/include/xinim/drivers
    ${CMAKE_SOURCE_DIR}/third_party/limine
    ${LIBSODIUM_INCLUDE_DIRS}
)

# =============================================================================
# Main XINIM Kernel Target
# =============================================================================

add_executable(xinim
    src/main.cpp
    src/kernel/kernel.cpp
    src/kernel/system.cpp
    src/kernel/syscall.cpp
    src/kernel/proc.cpp
    src/kernel/schedule.cpp
    src/kernel/service.cpp
    src/kernel/tty.cpp
    src/kernel/panic.cpp
)

target_link_libraries(xinim PRIVATE
    ${LIBSODIUM_LIBRARIES}
    pthread
    rt
)

# =============================================================================
# Sanitizer Targets
# =============================================================================

# AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    add_executable(xinim-asan
        src/main.cpp
        src/kernel/kernel.cpp
        src/mm/memory.cpp
    )
    target_compile_options(xinim-asan PRIVATE
        -fsanitize=address
        -fno-omit-frame-pointer
        -O1 -g
    )
    target_link_options(xinim-asan PRIVATE -fsanitize=address)
    target_include_directories(xinim-asan PRIVATE include include/xinim)
endif()

# ThreadSanitizer
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    add_executable(xinim-tsan
        src/main.cpp
        src/kernel/kernel.cpp
        src/mm/memory.cpp
    )
    target_compile_options(xinim-tsan PRIVATE -fsanitize=thread -O1 -g)
    target_link_options(xinim-tsan PRIVATE -fsanitize=thread)
    target_include_directories(xinim-tsan PRIVATE include include/xinim)
endif()

# UndefinedBehaviorSanitizer
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(ENABLE_UBSAN)
    add_executable(xinim-ubsan
        src/main.cpp
        src/kernel/kernel.cpp
        src/mm/memory.cpp
    )
    target_compile_options(xinim-ubsan PRIVATE -fsanitize=undefined -O1 -g)
    target_link_options(xinim-ubsan PRIVATE -fsanitize=undefined)
    target_include_directories(xinim-ubsan PRIVATE include include/xinim)
endif()

# =============================================================================
# Coverage Target
# =============================================================================

option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    add_executable(xinim-coverage
        src/main.cpp
        src/kernel/kernel.cpp
        src/mm/memory.cpp
    )
    target_compile_options(xinim-coverage PRIVATE
        --coverage
        -fprofile-arcs
        -ftest-coverage
        -O0 -g
    )
    target_link_options(xinim-coverage PRIVATE --coverage)
    target_link_libraries(xinim-coverage PRIVATE gcov)
    target_include_directories(xinim-coverage PRIVATE include include/xinim)
    
    # Coverage target
    add_custom_target(coverage
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' '*/third_party/*' --output-file coverage_filtered.info
        COMMAND genhtml coverage_filtered.info --output-directory coverage_html --demangle-cpp
        COMMAND echo "Coverage report: coverage_html/index.html"
        DEPENDS xinim-coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# =============================================================================
# Analysis Targets
# =============================================================================

# Format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.hpp
        ${CMAKE_SOURCE_DIR}/include/*.hpp
        ${CMAKE_SOURCE_DIR}/include/*.h
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting source code with clang-format"
    )
endif()

# Lint target (clang-tidy)
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/kernel/*.cpp
        COMMENT "Running clang-tidy"
    )
endif()

# Formal verification
add_custom_target(verify
    COMMAND python3 ${CMAKE_SOURCE_DIR}/specs/verify_all.py
    COMMENT "Running formal verification (Z3)"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# All analysis
add_custom_target(analyze
    COMMAND bash ${CMAKE_SOURCE_DIR}/tools/run_analysis.sh
    COMMENT "Running comprehensive analysis"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Advanced analysis
add_custom_target(analyze-advanced
    COMMAND bash ${CMAKE_SOURCE_DIR}/tools/run_advanced_analysis.sh
    COMMENT "Running advanced analysis suite"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# =============================================================================
# Documentation
# =============================================================================

find_program(DOXYGEN doxygen)
if(DOXYGEN)
    add_custom_target(docs
        COMMAND ${DOXYGEN} ${CMAKE_SOURCE_DIR}/Doxyfile
        COMMENT "Generating Doxygen documentation"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# =============================================================================
# Testing
# =============================================================================

enable_testing()

# POSIX tests
if(EXISTS ${CMAKE_SOURCE_DIR}/src/posix/test.cpp)
    add_executable(posix-test src/posix/test.cpp)
    target_link_libraries(posix-test PRIVATE pthread)
    add_test(NAME POSIXTest COMMAND posix-test)
endif()

# =============================================================================
# Installation
# =============================================================================

install(TARGETS xinim
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/xinim
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "XINIM Build Configuration")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Sanitizers:")
message(STATUS "    ASAN: ${ENABLE_ASAN}")
message(STATUS "    TSAN: ${ENABLE_TSAN}")
message(STATUS "    UBSAN: ${ENABLE_UBSAN}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  xinim          - Main kernel binary")
message(STATUS "  format         - Format code with clang-format")
message(STATUS "  lint           - Run clang-tidy")
message(STATUS "  verify         - Run formal verification")
message(STATUS "  analyze        - Run comprehensive analysis")
message(STATUS "  analyze-advanced - Run advanced analysis suite")
message(STATUS "  docs           - Generate Doxygen documentation")
if(ENABLE_COVERAGE)
    message(STATUS "  coverage       - Generate coverage report")
endif()
