cmake_minimum_required(VERSION 3.5)
project(minix1 C)
enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

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

