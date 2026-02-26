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
        # Extra warnings enabled without -Werror until codebase is cleaned up.
        # Promote to errors after Phase 4 cleanup.
        list(APPEND warn_flags
            -Wshadow
            -Wno-error=shadow
            -Wconversion
            -Wno-error=conversion
            -Wsign-conversion
            -Wno-error=sign-conversion
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
