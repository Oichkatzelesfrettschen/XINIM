# KERNEL/CMakeLists.txt
# This CMakeLists.txt is based on the draft design from docs/DRAFT_CMAKELISTS.md

# Kernel sources are compiled as an OBJECT library first
add_library(XINIM_KERNEL_LIB OBJECT
    # CORE sources (illustrative, list actual files)
    # Example: CORE/main.cpp, CORE/pmm.cpp, CORE/vmm.cpp, etc.
    # For now, to avoid errors if these files don't exist yet, keep it minimal.
    # If kernel/main.cpp or similar exists at KERNEL/main.cpp:
    # main.cpp
    # If it's in KERNEL/CORE/main.cpp:
    # CORE/main.cpp

    # ARCH specific sources (conditionally)
    # Example for x86_64 (these are just examples, replace with actual files)
    # ARCH/${XINIM_ARCH}/boot/early_setup.S # Startup assembly
    # ARCH/${XINIM_ARCH}/cpu/gdt.cpp
    # ARCH/${XINIM_ARCH}/cpu/idt.cpp
    # ARCH/${XINIM_ARCH}/mm/paging_setup.cpp
)

# Add kernel source files from subdirectories if they exist
# These paths assume a KERNEL/CORE, KERNEL/ARCH/X86_64 etc. structure
# list(APPEND KERNEL_CORE_SOURCES
#    CORE/main.cpp CORE/pmm.cpp CORE/vmm.cpp CORE/scheduler.cpp
#    CORE/process.cpp CORE/syscall.cpp CORE/timer.cpp
# )
# target_sources(XINIM_KERNEL_LIB PRIVATE ${KERNEL_CORE_SOURCES})

# if(XINIM_ARCH STREQUAL "x86_64")
#    list(APPEND KERNEL_ARCH_SOURCES
#        ARCH/X86_64/boot/boot.S # This might be part of the executable directly
#        ARCH/X86_64/cpu/apic.cpp
#        ARCH/X86_64/mm/paging.cpp
#    )
#    target_sources(XINIM_KERNEL_LIB PRIVATE ${KERNEL_ARCH_SOURCES})
# endif()


target_compile_options(XINIM_KERNEL_LIB PRIVATE
    -nostdlib
    -ffreestanding
    # Add other kernel-specific compile flags from top-level CMakeLists.txt
    # -fno-stack-protector
    # -mgeneral-regs-only # If not using FPU/SIMD in kernel initially
)

# Public include directories for the kernel library itself (e.g., for XINIM_KERNEL executable)
# These are headers that the kernel's own components might need from each other,
# not necessarily for external modules yet.
target_include_directories(XINIM_KERNEL_LIB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include # For potential KERNEL/include/kernel_api.hpp
    # Add other internal include paths as modules are developed:
    # ${CMAKE_CURRENT_SOURCE_DIR}/CORE/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/MM/include
)

# Private include directories (e.g., architecture specific)
target_include_directories(XINIM_KERNEL_LIB PRIVATE
    # ${CMAKE_CURRENT_SOURCE_DIR}/ARCH/${XINIM_ARCH}/include
)

# --- IPC Submodule ---
# This adds the KERNEL/IPC directory, which defines 'xinim_ipc_core' and 'math_algebras' libraries.
add_subdirectory(IPC)

# Link XINIM_KERNEL_LIB against the IPC core library.
# math_algebras is linked publicly by xinim_ipc_core, so it should be available transitively.
target_link_libraries(XINIM_KERNEL_LIB PRIVATE
    xinim_ipc_core
    # math_algebras # Explicitly linking math_algebras too, though transitive via ipc_core
)


# --- Final Kernel Executable ---
# This target links the kernel library object files with arch-specific boot code
# and the linker script.
# The list of sources for add_executable should primarily be the entry point assembly
# and then use $<TARGET_OBJECTS:XINIM_KERNEL_LIB>.

# For x86_64, boot.S is often the entry point.
# This assumes boot.S is at KERNEL/ARCH/${XINIM_ARCH}/boot/boot.S or similar
# For now, if boot.S is at the project root, adjust path or move it.
# If KERNEL/ARCH/${XINIM_ARCH}/boot/boot.S does not exist, this will error.
# Let's assume a placeholder for now if the actual boot.S path is not confirmed.
# file(GLOB KERNEL_BOOT_ASM "ARCH/${XINIM_ARCH}/boot/boot.S") # Check if it exists
# if(NOT KERNEL_BOOT_ASM)
#    message(WARNING "Kernel boot assembly (boot.S) not found at ARCH/${XINIM_ARCH}/boot/boot.S. Using placeholder.")
#    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy_boot.S "SECTION .text\nGLOBAL _start\n_start:\n  HLT")
#    set(KERNEL_BOOT_ASM ${CMAKE_CURRENT_BINARY_DIR}/dummy_boot.S)
# endif()

add_executable(XINIM_KERNEL
    # ${KERNEL_BOOT_ASM} # Actual assembly entry point
    # For now, let XINIM_KERNEL_LIB provide all sources, assuming one .cpp has kmain.
    # If boot.S is essential for linking, it needs to be correctly pathed.
    # The example from docs/DRAFT_CMAKELISTS.md had ARCH/${XINIM_ARCH}/boot/boot.S here.
    # If that file isn't present, this will fail.
    # To make this runnable, we need at least one source file for XINIM_KERNEL that has main/kmain
    # or ensure XINIM_KERNEL_LIB has the entry points and correctly links.
    # For now, we assume XINIM_KERNEL_LIB contains kmain and boot.S is linked via XINIM_KERNEL_LIB or here.
    $<TARGET_OBJECTS:XINIM_KERNEL_LIB>
)

# Linker script and options
# The path to linker script needs to be correct, e.g. KERNEL/ARCH/X86_64/boot/linker.ld
# set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ARCH/${XINIM_ARCH}/boot/linker.ld)
# if(NOT EXISTS ${LINKER_SCRIPT})
#    message(WARNING "Linker script not found at ${LINKER_SCRIPT}. Linker may fail.")
# endif()

target_link_options(XINIM_KERNEL PRIVATE
    # "-T${LINKER_SCRIPT}" # Correct path to linker script
    -nostdlib
    -Wl,--gc-sections
    # Add other linker flags, e.g. -n for no page alignment of sections if needed
)

# The kernel itself doesn't link libraries in the traditional sense for freestanding,
# but it does need to link its own object files and potentially static utility libraries.
# `math_algebras` and `xinim_ipc_core` are linked to XINIM_KERNEL_LIB,
# their object files become part of XINIM_KERNEL through $<TARGET_OBJECTS:XINIM_KERNEL_LIB>.

# Installation (example, adjust destination as needed for bootloader)
# install(TARGETS XINIM_KERNEL
#    DESTINATION ${CMAKE_INSTALL_PREFIX}/boot
#    COMPONENT kernel
# )
# install(FILES ${LINKER_SCRIPT} # If linker script needs to be installed
#    DESTINATION ${CMAKE_INSTALL_PREFIX}/boot
#    COMPONENT kernel
# )

message(STATUS "KERNEL/CMakeLists.txt processed.")
