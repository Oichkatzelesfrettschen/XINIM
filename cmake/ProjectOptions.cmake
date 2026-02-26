# Project-wide options and defaults.

include_guard()

option(XINIM_ENABLE_WERROR "Treat warnings as errors" ON)
# XINIM_ENABLE_SANITIZERS removed: bare-metal kernel cannot link sanitizer runtimes.
# Sanitizers may be revisited for host-side tests only in a future phase.

function(xinim_apply_target_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_compile_definitions(${target} PRIVATE
        XINIM_ARCH_X86_64
        _XOPEN_SOURCE=700
        _GNU_SOURCE
    )
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
