cmake_minimum_required(VERSION 3.5)
project(minix1 C)
enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

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
  set(CMAKE_C_COMPILER "${CROSS_PREFIX}gcc")
  set(CMAKE_ASM_NASM_COMPILER "${CROSS_PREFIX}nasm")
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

add_subdirectory(lib)
add_subdirectory(kernel)
add_subdirectory(mm)
add_subdirectory(fs)
add_subdirectory(commands)
add_subdirectory(tools)

