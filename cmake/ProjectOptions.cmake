# Project-wide options and defaults.

include_guard()

option(XINIM_ENABLE_WERROR "Treat warnings as errors" ON)
option(XINIM_ENABLE_LATTICE_CHANGER
    "Enable tagged lattice-changer ABI transform for foreign syscall numbers" ON)
# XINIM_ENABLE_SANITIZERS removed: bare-metal kernel cannot link sanitizer runtimes.
# Sanitizers may be revisited for host-side tests only in a future phase.

function(xinim_apply_target_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_23)

    get_target_property(_xinim_target_arch ${target} XINIM_TARGET_ARCH)

    if(NOT _xinim_target_arch)
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" OR
           CMAKE_SYSTEM_PROCESSOR STREQUAL "i586" OR
           CMAKE_SYSTEM_PROCESSOR STREQUAL "i486" OR
           CMAKE_SYSTEM_PROCESSOR STREQUAL "i386")
            set(_xinim_target_arch i386)
        else()
            set(_xinim_target_arch x86_64)
        endif()
    endif()

    if(_xinim_target_arch STREQUAL "i386" OR
       _xinim_target_arch STREQUAL "i486" OR
       _xinim_target_arch STREQUAL "i586" OR
       _xinim_target_arch STREQUAL "i686" OR
       _xinim_target_arch MATCHES "^x86_32_")
        set(_XINIM_ARCH_DEF XINIM_ARCH_I386)
    else()
        set(_XINIM_ARCH_DEF XINIM_ARCH_X86_64)
    endif()

    target_compile_definitions(${target} PRIVATE
        ${_XINIM_ARCH_DEF}
        _XOPEN_SOURCE=700
        _GNU_SOURCE
    )
    if(XINIM_ENABLE_LATTICE_CHANGER)
        target_compile_definitions(${target} PRIVATE XINIM_ENABLE_LATTICE_CHANGER=1)
    endif()
    # Assembly files in this project use AT&T syntax (the GAS/Clang default).
    # Do not add -masm=intel here.
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/xinim
        ${PROJECT_SOURCE_DIR}/include/xinim/drivers
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/kernel
        ${PROJECT_SOURCE_DIR}/src/crypto
        ${PROJECT_SOURCE_DIR}/third_party/limine
    )
endfunction()
