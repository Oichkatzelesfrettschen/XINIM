# Compiler warning policies for Xinim.

function(xinim_set_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(warn_flags
            -Wall
            -Wextra
            -Wpedantic
        )
        if (XINIM_ENABLE_WERROR)
            list(APPEND warn_flags -Werror)
        endif()
        # Treat the high-signal warning classes as errors when
        # XINIM_ENABLE_WERROR is on; do not demote conversion diagnostics.
        list(APPEND warn_flags
            -Wshadow
            -Werror=shadow
            -Wconversion
            -Wsign-conversion
        )
        target_compile_options(${target} PRIVATE ${warn_flags})
    elseif (MSVC)
        set(warn_flags /W4 /permissive-)
        if (XINIM_ENABLE_WERROR)
            list(APPEND warn_flags /WX)
        endif()
        target_compile_options(${target} PRIVATE ${warn_flags})
    endif()
endfunction()
