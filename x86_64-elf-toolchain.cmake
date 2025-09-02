# Toolchain file for x86_64-elf cross-compilation on macOS
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compiler settings
set(CMAKE_C_COMPILER x86_64-elf-gcc)
set(CMAKE_CXX_COMPILER x86_64-elf-g++)
set(CMAKE_ASM_COMPILER x86_64-elf-as)
set(CMAKE_AR x86_64-elf-ar)
set(CMAKE_RANLIB x86_64-elf-ranlib)
set(CMAKE_OBJCOPY x86_64-elf-objcopy)
set(CMAKE_OBJDUMP x86_64-elf-objdump)

# Disable macOS-specific flags
set(CMAKE_OSX_SYSROOT "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "")
set(CMAKE_OSX_ARCHITECTURES "")

# Search paths
set(CMAKE_FIND_ROOT_PATH /opt/homebrew)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Compiler flags for bare metal
set(CMAKE_C_FLAGS "-ffreestanding -nostdlib -fno-builtin -fno-exceptions -fno-stack-protector -mno-red-zone -mcmodel=kernel")
set(CMAKE_CXX_FLAGS "-ffreestanding -nostdlib -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -mcmodel=kernel")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -z max-page-size=0x1000")

# Don't run compiler tests
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)