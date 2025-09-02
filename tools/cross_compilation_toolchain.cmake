# XINIM Cross-Compilation Toolchain Configuration
# World's first C++23 bare-metal cross-compilation system

cmake_minimum_required(VERSION 3.28)

# ═══════════════════════════════════════════════════════════════════════════
# Cross-Compilation Toolchain Matrix
# ═══════════════════════════════════════════════════════════════════════════

# Supported target architectures
set(XINIM_SUPPORTED_ARCHITECTURES
    "x86_64"
    "aarch64"
    "riscv64"
    "arm-cortex-m"
    "powerpc64"
    CACHE STRING "Supported target architectures"
)

# Target system types
set(XINIM_SUPPORTED_SYSTEMS
    "bare-metal"
    "embedded-linux"
    "freestanding"
    CACHE STRING "Supported target systems"
)

# ═══════════════════════════════════════════════════════════════════════════
# Toolchain Detection and Configuration
# ═══════════════════════════════════════════════════════════════════════════

function(xinim_configure_cross_toolchain TARGET_ARCH TARGET_SYSTEM)
    message(STATUS "Configuring XINIM cross-compilation for ${TARGET_ARCH}-${TARGET_SYSTEM}")
    
    # Set system name for cross-compilation
    set(CMAKE_SYSTEM_NAME Generic PARENT_SCOPE)
    set(CMAKE_SYSTEM_PROCESSOR ${TARGET_ARCH} PARENT_SCOPE)
    
    # Configure toolchain based on architecture
    if(TARGET_ARCH STREQUAL "x86_64")
        xinim_configure_x86_64_toolchain(${TARGET_SYSTEM})
    elseif(TARGET_ARCH STREQUAL "aarch64")
        xinim_configure_aarch64_toolchain(${TARGET_SYSTEM})
    elseif(TARGET_ARCH STREQUAL "riscv64")
        xinim_configure_riscv64_toolchain(${TARGET_SYSTEM})
    elseif(TARGET_ARCH STREQUAL "arm-cortex-m")
        xinim_configure_arm_cortex_m_toolchain(${TARGET_SYSTEM})
    elseif(TARGET_ARCH STREQUAL "powerpc64")
        xinim_configure_powerpc64_toolchain(${TARGET_SYSTEM})
    else()
        message(FATAL_ERROR "Unsupported target architecture: ${TARGET_ARCH}")
    endif()
    
    # Set common cross-compilation flags
    xinim_set_cross_compilation_flags(${TARGET_ARCH} ${TARGET_SYSTEM})
endfunction()

# ═══════════════════════════════════════════════════════════════════════════
# Architecture-Specific Toolchain Configuration
# ═══════════════════════════════════════════════════════════════════════════

function(xinim_configure_x86_64_toolchain TARGET_SYSTEM)
    if(TARGET_SYSTEM STREQUAL "bare-metal")
        # x86_64 bare-metal toolchain
        set(CMAKE_C_COMPILER x86_64-elf-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER x86_64-elf-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER x86_64-elf-as PARENT_SCOPE)
        set(CMAKE_LINKER x86_64-elf-ld PARENT_SCOPE)
        set(CMAKE_AR x86_64-elf-ar PARENT_SCOPE)
        set(CMAKE_OBJCOPY x86_64-elf-objcopy PARENT_SCOPE)
        set(CMAKE_OBJDUMP x86_64-elf-objdump PARENT_SCOPE)
        set(CMAKE_SIZE x86_64-elf-size PARENT_SCOPE)
        
        # x86_64 specific flags
        set(ARCH_SPECIFIC_FLAGS 
            "-m64"
            "-march=x86-64"
            "-mcmodel=kernel"
            "-mno-red-zone"
            "-mno-mmx"
            "-mno-sse"
            "-mno-sse2"
            PARENT_SCOPE
        )
        
        # Linker script for x86_64 bare-metal
        set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/kernel/arch/x86_64/linker.ld" PARENT_SCOPE)
        
    elseif(TARGET_SYSTEM STREQUAL "embedded-linux")
        # x86_64 embedded Linux
        set(CMAKE_C_COMPILER x86_64-linux-gnu-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER x86_64-linux-gnu-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER x86_64-linux-gnu-as PARENT_SCOPE)
        
        set(ARCH_SPECIFIC_FLAGS 
            "-m64"
            "-march=x86-64"
            PARENT_SCOPE
        )
    endif()
    
    message(STATUS "✓ Configured x86_64 toolchain for ${TARGET_SYSTEM}")
endfunction()

function(xinim_configure_aarch64_toolchain TARGET_SYSTEM)
    if(TARGET_SYSTEM STREQUAL "bare-metal")
        # AArch64 bare-metal toolchain
        set(CMAKE_C_COMPILER aarch64-none-elf-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER aarch64-none-elf-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER aarch64-none-elf-as PARENT_SCOPE)
        set(CMAKE_LINKER aarch64-none-elf-ld PARENT_SCOPE)
        set(CMAKE_AR aarch64-none-elf-ar PARENT_SCOPE)
        set(CMAKE_OBJCOPY aarch64-none-elf-objcopy PARENT_SCOPE)
        set(CMAKE_OBJDUMP aarch64-none-elf-objdump PARENT_SCOPE)
        set(CMAKE_SIZE aarch64-none-elf-size PARENT_SCOPE)
        
        set(ARCH_SPECIFIC_FLAGS
            "-mcpu=cortex-a53"
            "-mabi=lp64"
            "-mlittle-endian"
            PARENT_SCOPE
        )
        
        set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/kernel/arch/aarch64/linker.ld" PARENT_SCOPE)
        
    elseif(TARGET_SYSTEM STREQUAL "embedded-linux")
        # AArch64 embedded Linux
        set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER aarch64-linux-gnu-as PARENT_SCOPE)
        
        set(ARCH_SPECIFIC_FLAGS
            "-mcpu=cortex-a53"
            PARENT_SCOPE
        )
    endif()
    
    message(STATUS "✓ Configured AArch64 toolchain for ${TARGET_SYSTEM}")
endfunction()

function(xinim_configure_riscv64_toolchain TARGET_SYSTEM)
    if(TARGET_SYSTEM STREQUAL "bare-metal")
        # RISC-V 64-bit bare-metal toolchain
        set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER riscv64-unknown-elf-as PARENT_SCOPE)
        set(CMAKE_LINKER riscv64-unknown-elf-ld PARENT_SCOPE)
        set(CMAKE_AR riscv64-unknown-elf-ar PARENT_SCOPE)
        set(CMAKE_OBJCOPY riscv64-unknown-elf-objcopy PARENT_SCOPE)
        set(CMAKE_OBJDUMP riscv64-unknown-elf-objdump PARENT_SCOPE)
        set(CMAKE_SIZE riscv64-unknown-elf-size PARENT_SCOPE)
        
        set(ARCH_SPECIFIC_FLAGS
            "-march=rv64imafdc"
            "-mabi=lp64d"
            "-mcmodel=medany"
            PARENT_SCOPE
        )
        
        set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/kernel/arch/riscv64/linker.ld" PARENT_SCOPE)
        
    elseif(TARGET_SYSTEM STREQUAL "embedded-linux")
        # RISC-V 64-bit embedded Linux
        set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER riscv64-linux-gnu-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER riscv64-linux-gnu-as PARENT_SCOPE)
        
        set(ARCH_SPECIFIC_FLAGS
            "-march=rv64gc"
            PARENT_SCOPE
        )
    endif()
    
    message(STATUS "✓ Configured RISC-V 64-bit toolchain for ${TARGET_SYSTEM}")
endfunction()

function(xinim_configure_arm_cortex_m_toolchain TARGET_SYSTEM)
    # ARM Cortex-M bare-metal toolchain (embedded only)
    set(CMAKE_C_COMPILER arm-none-eabi-gcc PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER arm-none-eabi-g++ PARENT_SCOPE)
    set(CMAKE_ASM_COMPILER arm-none-eabi-as PARENT_SCOPE)
    set(CMAKE_LINKER arm-none-eabi-ld PARENT_SCOPE)
    set(CMAKE_AR arm-none-eabi-ar PARENT_SCOPE)
    set(CMAKE_OBJCOPY arm-none-eabi-objcopy PARENT_SCOPE)
    set(CMAKE_OBJDUMP arm-none-eabi-objdump PARENT_SCOPE)
    set(CMAKE_SIZE arm-none-eabi-size PARENT_SCOPE)
    
    set(ARCH_SPECIFIC_FLAGS
        "-mcpu=cortex-m4"
        "-mthumb"
        "-mfloat-abi=hard"
        "-mfpu=fpv4-sp-d16"
        PARENT_SCOPE
    )
    
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/kernel/arch/arm/linker.ld" PARENT_SCOPE)
    
    message(STATUS "✓ Configured ARM Cortex-M toolchain for ${TARGET_SYSTEM}")
endfunction()

function(xinim_configure_powerpc64_toolchain TARGET_SYSTEM)
    if(TARGET_SYSTEM STREQUAL "bare-metal")
        # PowerPC 64-bit bare-metal toolchain
        set(CMAKE_C_COMPILER powerpc64-linux-gnu-gcc PARENT_SCOPE)
        set(CMAKE_CXX_COMPILER powerpc64-linux-gnu-g++ PARENT_SCOPE)
        set(CMAKE_ASM_COMPILER powerpc64-linux-gnu-as PARENT_SCOPE)
        set(CMAKE_LINKER powerpc64-linux-gnu-ld PARENT_SCOPE)
        set(CMAKE_AR powerpc64-linux-gnu-ar PARENT_SCOPE)
        set(CMAKE_OBJCOPY powerpc64-linux-gnu-objcopy PARENT_SCOPE)
        set(CMAKE_OBJDUMP powerpc64-linux-gnu-objdump PARENT_SCOPE)
        set(CMAKE_SIZE powerpc64-linux-gnu-size PARENT_SCOPE)
        
        set(ARCH_SPECIFIC_FLAGS
            "-m64"
            "-mcpu=power8"
            "-mtune=power8"
            PARENT_SCOPE
        )
        
        set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/kernel/arch/powerpc64/linker.ld" PARENT_SCOPE)
    endif()
    
    message(STATUS "✓ Configured PowerPC 64-bit toolchain for ${TARGET_SYSTEM}")
endfunction()

# ═══════════════════════════════════════════════════════════════════════════
# Common Cross-Compilation Flags
# ═══════════════════════════════════════════════════════════════════════════

function(xinim_set_cross_compilation_flags TARGET_ARCH TARGET_SYSTEM)
    # C++23 standard flags
    set(CXX_STANDARD_FLAGS
        "-std=c++23"
        "-fno-exceptions"
        "-fno-rtti"
        "-fno-threadsafe-statics"
        "-fno-use-cxa-atexit"
        PARENT_SCOPE
    )
    
    # C17 standard flags
    set(C_STANDARD_FLAGS
        "-std=c17"
        "-fno-builtin"
        "-fno-stack-protector"
        PARENT_SCOPE
    )
    
    # Common bare-metal flags
    if(TARGET_SYSTEM STREQUAL "bare-metal" OR TARGET_SYSTEM STREQUAL "freestanding")
        set(FREESTANDING_FLAGS
            "-ffreestanding"
            "-fno-hosted"
            "-nostdlib"
            "-nostartfiles"
            "-nodefaultlibs"
            "-fno-common"
            "-ffunction-sections"
            "-fdata-sections"
            PARENT_SCOPE
        )
        
        # Bare-metal linker flags
        set(BARE_METAL_LINKER_FLAGS
            "-nostdlib"
            "-static"
            "-Wl,--gc-sections"
            "-Wl,--print-memory-usage"
            PARENT_SCOPE
        )
        
        if(DEFINED LINKER_SCRIPT AND EXISTS ${LINKER_SCRIPT})
            list(APPEND BARE_METAL_LINKER_FLAGS "-T${LINKER_SCRIPT}")
            set(BARE_METAL_LINKER_FLAGS ${BARE_METAL_LINKER_FLAGS} PARENT_SCOPE)
        endif()
    endif()
    
    # Optimization flags
    set(OPTIMIZATION_FLAGS
        "-Os"                    # Optimize for size
        "-fomit-frame-pointer"
        "-ffast-math"
        "-fno-math-errno"
        "-ffinite-math-only"
        PARENT_SCOPE
    )
    
    # Warning flags
    set(WARNING_FLAGS
        "-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Wno-unused-parameter"
        "-Wno-missing-field-initializers"
        PARENT_SCOPE
    )
    
    # Debug flags (for debug builds)
    set(DEBUG_FLAGS
        "-g3"
        "-gdwarf-4"
        "-fno-omit-frame-pointer"
        PARENT_SCOPE
    )
    
    message(STATUS "✓ Set cross-compilation flags for ${TARGET_ARCH}-${TARGET_SYSTEM}")
endfunction()

# ═══════════════════════════════════════════════════════════════════════════
# Cross-Compilation Build Targets
# ═══════════════════════════════════════════════════════════════════════════

function(xinim_add_cross_target TARGET_NAME TARGET_ARCH TARGET_SYSTEM)
    message(STATUS "Adding cross-compilation target: ${TARGET_NAME} (${TARGET_ARCH}-${TARGET_SYSTEM})")
    
    # Configure toolchain for this target
    xinim_configure_cross_toolchain(${TARGET_ARCH} ${TARGET_SYSTEM})
    
    # Create target-specific build directory
    set(TARGET_BUILD_DIR "${CMAKE_BINARY_DIR}/cross-${TARGET_ARCH}-${TARGET_SYSTEM}")
    file(MAKE_DIRECTORY ${TARGET_BUILD_DIR})
    
    # Combine all flags
    set(ALL_C_FLAGS)
    list(APPEND ALL_C_FLAGS ${C_STANDARD_FLAGS})
    list(APPEND ALL_C_FLAGS ${ARCH_SPECIFIC_FLAGS})
    list(APPEND ALL_C_FLAGS ${OPTIMIZATION_FLAGS})
    list(APPEND ALL_C_FLAGS ${WARNING_FLAGS})
    
    if(TARGET_SYSTEM STREQUAL "bare-metal" OR TARGET_SYSTEM STREQUAL "freestanding")
        list(APPEND ALL_C_FLAGS ${FREESTANDING_FLAGS})
    endif()
    
    set(ALL_CXX_FLAGS)
    list(APPEND ALL_CXX_FLAGS ${CXX_STANDARD_FLAGS})
    list(APPEND ALL_CXX_FLAGS ${ARCH_SPECIFIC_FLAGS})
    list(APPEND ALL_CXX_FLAGS ${OPTIMIZATION_FLAGS})
    list(APPEND ALL_CXX_FLAGS ${WARNING_FLAGS})
    
    if(TARGET_SYSTEM STREQUAL "bare-metal" OR TARGET_SYSTEM STREQUAL "freestanding")
        list(APPEND ALL_CXX_FLAGS ${FREESTANDING_FLAGS})
    endif()
    
    set(ALL_LINKER_FLAGS)
    if(TARGET_SYSTEM STREQUAL "bare-metal" OR TARGET_SYSTEM STREQUAL "freestanding")
        list(APPEND ALL_LINKER_FLAGS ${BARE_METAL_LINKER_FLAGS})
    endif()
    
    # Convert lists to strings
    string(JOIN " " C_FLAGS_STR ${ALL_C_FLAGS})
    string(JOIN " " CXX_FLAGS_STR ${ALL_CXX_FLAGS})
    string(JOIN " " LINKER_FLAGS_STR ${ALL_LINKER_FLAGS})
    
    # Create custom target for cross-compilation
    add_custom_target(${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "Building XINIM for ${TARGET_ARCH}-${TARGET_SYSTEM}"
        COMMAND ${CMAKE_COMMAND}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/xinim-cross-${TARGET_ARCH}.cmake
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_FLAGS="${C_FLAGS_STR}"
            -DCMAKE_CXX_FLAGS="${CXX_FLAGS_STR}"
            -DCMAKE_EXE_LINKER_FLAGS="${LINKER_FLAGS_STR}"
            -DXINIM_TARGET_ARCH=${TARGET_ARCH}
            -DXINIM_TARGET_SYSTEM=${TARGET_SYSTEM}
            -S${CMAKE_SOURCE_DIR}
            -B${TARGET_BUILD_DIR}
        COMMAND ${CMAKE_COMMAND} --build ${TARGET_BUILD_DIR} --parallel
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Cross-compiling XINIM for ${TARGET_ARCH}-${TARGET_SYSTEM}"
        VERBATIM
    )
    
    message(STATUS "✓ Added cross-compilation target: ${TARGET_NAME}")
endfunction()

# ═══════════════════════════════════════════════════════════════════════════
# Toolchain Validation
# ═══════════════════════════════════════════════════════════════════════════

function(xinim_validate_cross_toolchain TARGET_ARCH TARGET_SYSTEM)
    message(STATUS "Validating cross-compilation toolchain for ${TARGET_ARCH}-${TARGET_SYSTEM}")
    
    # Check if required tools are available
    set(REQUIRED_TOOLS)
    
    if(TARGET_ARCH STREQUAL "x86_64" AND TARGET_SYSTEM STREQUAL "bare-metal")
        list(APPEND REQUIRED_TOOLS 
            x86_64-elf-gcc 
            x86_64-elf-g++ 
            x86_64-elf-ld 
            x86_64-elf-ar
        )
    elseif(TARGET_ARCH STREQUAL "aarch64" AND TARGET_SYSTEM STREQUAL "bare-metal")
        list(APPEND REQUIRED_TOOLS 
            aarch64-none-elf-gcc 
            aarch64-none-elf-g++ 
            aarch64-none-elf-ld 
            aarch64-none-elf-ar
        )
    elseif(TARGET_ARCH STREQUAL "riscv64" AND TARGET_SYSTEM STREQUAL "bare-metal")
        list(APPEND REQUIRED_TOOLS 
            riscv64-unknown-elf-gcc 
            riscv64-unknown-elf-g++ 
            riscv64-unknown-elf-ld 
            riscv64-unknown-elf-ar
        )
    elseif(TARGET_ARCH STREQUAL "arm-cortex-m")
        list(APPEND REQUIRED_TOOLS 
            arm-none-eabi-gcc 
            arm-none-eabi-g++ 
            arm-none-eabi-ld 
            arm-none-eabi-ar
        )
    endif()
    
    set(MISSING_TOOLS)
    foreach(TOOL ${REQUIRED_TOOLS})
        find_program(${TOOL}_FOUND ${TOOL})
        if(NOT ${TOOL}_FOUND)
            list(APPEND MISSING_TOOLS ${TOOL})
        endif()
    endforeach()
    
    if(MISSING_TOOLS)
        message(WARNING "Missing cross-compilation tools for ${TARGET_ARCH}-${TARGET_SYSTEM}: ${MISSING_TOOLS}")
        message(STATUS "Install missing tools or disable cross-compilation for this target")
        return()
    endif()
    
    message(STATUS "✓ Cross-compilation toolchain validation passed for ${TARGET_ARCH}-${TARGET_SYSTEM}")
endfunction()

# ═══════════════════════════════════════════════════════════════════════════
# Cross-Compilation Summary and Targets
# ═══════════════════════════════════════════════════════════════════════════

function(xinim_setup_cross_compilation)
    message(STATUS "")
    message(STATUS "═══════════════════════════════════════════════════════════")
    message(STATUS "XINIM Cross-Compilation Toolchain Setup")
    message(STATUS "═══════════════════════════════════════════════════════════")
    
    # Create cross-compilation targets for supported combinations
    set(CROSS_TARGETS
        "xinim-x86_64-bare-metal;x86_64;bare-metal"
        "xinim-aarch64-bare-metal;aarch64;bare-metal"
        "xinim-riscv64-bare-metal;riscv64;bare-metal"
        "xinim-arm-cortex-m;arm-cortex-m;bare-metal"
        "xinim-x86_64-embedded-linux;x86_64;embedded-linux"
        "xinim-aarch64-embedded-linux;aarch64;embedded-linux"
    )
    
    foreach(TARGET_INFO ${CROSS_TARGETS})
        string(REPLACE ";" " " TARGET_INFO_DISPLAY "${TARGET_INFO}")
        string(REPLACE ";" "\\;" TARGET_LIST "${TARGET_INFO}")
        list(GET TARGET_LIST 0 TARGET_NAME)
        list(GET TARGET_LIST 1 TARGET_ARCH)
        list(GET TARGET_LIST 2 TARGET_SYSTEM)
        
        # Validate toolchain availability
        xinim_validate_cross_toolchain(${TARGET_ARCH} ${TARGET_SYSTEM})
        
        # Add cross-compilation target if toolchain is available
        find_program(TOOLCHAIN_AVAILABLE ${CMAKE_C_COMPILER})
        if(TOOLCHAIN_AVAILABLE)
            xinim_add_cross_target(${TARGET_NAME} ${TARGET_ARCH} ${TARGET_SYSTEM})
            message(STATUS "✓ ${TARGET_NAME}: ${TARGET_ARCH}-${TARGET_SYSTEM}")
        else()
            message(STATUS "✗ ${TARGET_NAME}: ${TARGET_ARCH}-${TARGET_SYSTEM} (toolchain not found)")
        endif()
    endforeach()
    
    # Create master cross-compilation target
    add_custom_target(xinim-cross-all
        COMMENT "Building XINIM for all available cross-compilation targets"
    )
    
    # Add dependencies for available targets
    foreach(TARGET_INFO ${CROSS_TARGETS})
        string(REPLACE ";" "\\;" TARGET_LIST "${TARGET_INFO}")
        list(GET TARGET_LIST 0 TARGET_NAME)
        if(TARGET ${TARGET_NAME})
            add_dependencies(xinim-cross-all ${TARGET_NAME})
        endif()
    endforeach()
    
    message(STATUS "")
    message(STATUS "Cross-compilation targets:")
    message(STATUS "  make xinim-cross-all          # Build all targets")
    message(STATUS "  make xinim-x86_64-bare-metal  # x86_64 bare-metal")
    message(STATUS "  make xinim-aarch64-bare-metal # AArch64 bare-metal")
    message(STATUS "  make xinim-riscv64-bare-metal # RISC-V 64-bit bare-metal")
    message(STATUS "  make xinim-arm-cortex-m       # ARM Cortex-M")
    message(STATUS "")
    message(STATUS "✓ XINIM cross-compilation system configured")
    message(STATUS "═══════════════════════════════════════════════════════════")
endfunction()

# Auto-setup cross-compilation if enabled
if(XINIM_ENABLE_CROSS_COMPILE)
    xinim_setup_cross_compilation()
endif()