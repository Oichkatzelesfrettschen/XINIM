cmake_minimum_required(VERSION 3.5)
project(minix1 CXX C)

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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────────────────────
# 3. Global include paths
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
# 4. Cross-compilation support
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
# 5. Baseline optimisation flags (native or cross)
#    x86-64-v1: MMX + SSE + SSE2, widest deployable subset.
# ─────────────────────────────────────────────────────────────────────────────
set(BASE_CPU_FLAGS
    "-march=x86-64"
    "-mtune=generic"
    "-msse"
    "-mmmx"
    "-msse2"
    "-mfpu=sse"
)
add_compile_options(${BASE_CPU_FLAGS})

# ─────────────────────────────────────────────────────────────────────────────
# 6. Wini driver selection
# ─────────────────────────────────────────────────────────────────────────────
option(DRIVER_AT "Use AT-style WD100x controller driver" OFF)
option(DRIVER_PC "Use PC/XT wini driver" OFF)

if(DRIVER_AT AND DRIVER_PC)
  message(FATAL_ERROR "Select only one of DRIVER_AT or DRIVER_PC")
endif()

if(DRIVER_AT)
  set(WINI_DRIVER at)
else()
  set(WINI_DRIVER pc)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 7. Build sub-components
# ─────────────────────────────────────────────────────────────────────────────
option(BUILD_SYSTEM "Build full Minix system components" OFF)

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
# 8. Unit tests
# ─────────────────────────────────────────────────────────────────────────────
option(ENABLE_UNIT_TESTS "Build unit tests" OFF)

if(ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
