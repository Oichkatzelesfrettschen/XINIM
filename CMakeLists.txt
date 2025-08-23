cmake_minimum_required(VERSION 3.20)
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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# ##############################################################################
# 4. Build profiles and optional tooling
# ##############################################################################

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Performance" "Release")

set(XINIM_COMPILE_FEATURES)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND XINIM_COMPILE_FEATURES -O0 -g)
elseif(CMAKE_BUILD_TYPE STREQUAL "Performance")
  list(APPEND XINIM_COMPILE_FEATURES -O3 -g -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  list(APPEND XINIM_COMPILE_FEATURES -O3 -DNDEBUG)
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_LTO "Enable link time optimisation" OFF)

set(XINIM_LINK_FEATURES)
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE MATCHES "Debug|Performance")
  list(APPEND XINIM_COMPILE_FEATURES -fsanitize=address)
  list(APPEND XINIM_LINK_FEATURES -fsanitize=address)
endif()

if(ENABLE_UBSAN AND CMAKE_BUILD_TYPE MATCHES "Debug|Performance")
  list(APPEND XINIM_COMPILE_FEATURES -fsanitize=undefined)
  list(APPEND XINIM_LINK_FEATURES -fsanitize=undefined)
endif()

if(ENABLE_LTO AND CMAKE_BUILD_TYPE MATCHES "Release|Performance")
  list(APPEND XINIM_COMPILE_FEATURES -flto)
  list(APPEND XINIM_LINK_FEATURES -flto)
endif()

# ##############################################################################
# 5. Baseline optimisation flags (native or cross)
#    x86-64-v1: MMX + SSE + SSE2, widest deployable subset.
# ##############################################################################
set(BASE_CPU_FLAGS
    "-march=x86-64"
    "-mtune=generic"
    "-msse"
    "-mmmx"
    "-msse2"
)

set(XINIM_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/h
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/commands
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/mm
    ${CMAKE_SOURCE_DIR}/lib
)

add_library(xinim_common INTERFACE)
target_compile_options(xinim_common INTERFACE
    ${BASE_CPU_FLAGS}
    -Wall -Wextra -Wpedantic -Werror
    ${XINIM_COMPILE_FEATURES}
)
target_link_options(xinim_common INTERFACE ${XINIM_LINK_FEATURES})
target_include_directories(xinim_common INTERFACE ${XINIM_INCLUDE_DIRS})


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
# 8. Unit tests
# ─────────────────────────────────────────────────────────────────────────────
option(ENABLE_UNIT_TESTS "Build unit tests" ON)

if(ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 9. Documentation target
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
