cmake_minimum_required(VERSION 3.5)
project(minix1 CXX C)

# ------------------------------------------------------------------------------
# Compiler configuration
# ------------------------------------------------------------------------------

# Require Clang 18 when no explicit compiler is provided.  Fail early if the
# tool chain is missing so users know to run tools/setup.sh first.
if(NOT CMAKE_C_COMPILER)
  find_program(CLANG_18 clang-18)
  if(NOT CLANG_18)
    message(FATAL_ERROR "clang-18 is required but was not found. Run tools/setup.sh")
  endif()
  set(CMAKE_C_COMPILER ${CLANG_18})
endif()

if(NOT CMAKE_CXX_COMPILER)
  find_program(CLANGXX_18 clang++-18)
  if(NOT CLANGXX_18)
    message(FATAL_ERROR "clang++-18 is required but was not found. Run tools/setup.sh")
  endif()
  set(CMAKE_CXX_COMPILER ${CLANGXX_18})
endif()


# Enforce a strict C++23 environment across the project
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global optimisation flags tuned for modern x86-64 CPUs.  These are applied to
# every target built in this project, including the kernel, libraries and tools.
add_compile_options(
  -march=x86-64-v1
  -msse2
  -msse3
  -mssse3
  -msse4.1
  -msse4.2
)

# Add global include directories
include_directories(h)
include_directories(include)
# Add subdirectory-specific include paths that might be expected by sources
include_directories(commands) # For includes like "mined.hpp" from commands/mined1.cpp
include_directories(fs)       # For includes like "buf.hpp", "const.hpp" from fs sources
include_directories(kernel)   # For includes like "proc.hpp", "const.hpp" from kernel sources
include_directories(mm)       # For includes like "mproc.hpp", "const.hpp" from mm sources
include_directories(lib)      # For includes like "lib.h" (now lib.hpp if it exists)

# Optional cross compilation support for x86_64.
option(CROSS_COMPILE_X86_64 "Cross compile for bare x86_64" OFF)
set(CROSS_PREFIX "" CACHE STRING "Prefix for cross compilation tools")

if(CROSS_COMPILE_X86_64)
  # Configure compilers and tools using the provided prefix.
  if(CROSS_PREFIX STREQUAL "")
    set(CROSS_PREFIX "x86_64-elf-")
  endif()
  set(CMAKE_SYSTEM_NAME Generic)
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
  # Use clang-18 for cross compilation as well
  set(CMAKE_C_COMPILER "${CROSS_PREFIX}clang-18")
  set(CMAKE_CXX_COMPILER "${CROSS_PREFIX}clang++-18")
  set(CMAKE_AR "${CROSS_PREFIX}ar")
  set(CMAKE_RANLIB "${CROSS_PREFIX}ranlib")
  set(CMAKE_LINKER "${CROSS_PREFIX}ld")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
endif()

# Options for wini driver selection
option(DRIVER_AT "Use AT wini driver" OFF)
option(DRIVER_PC "Use PC/XT wini driver" OFF)

if(DRIVER_AT AND DRIVER_PC)
  message(FATAL_ERROR "Select only one of DRIVER_AT or DRIVER_PC")
endif()

# default to PC/XT driver
if(DRIVER_AT)
  set(WINI_DRIVER at)
else()
  set(WINI_DRIVER pc)
endif()

option(BUILD_SYSTEM "Build Minix system components" OFF)

if(BUILD_SYSTEM)
  add_subdirectory(lib)
  add_subdirectory(kernel)
  add_subdirectory(mm)
  add_subdirectory(fs)
  add_subdirectory(commands)
  add_subdirectory(tools)
endif()

add_subdirectory(crypto)

option(ENABLE_UNIT_TESTS "Build unit tests" OFF)
if(ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
