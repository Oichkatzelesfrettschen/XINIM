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
        # -Wshadow: promoted to error in v1.2.0 (zero violations confirmed).
        # -Wconversion/-Wsign-conversion: still warning (pre-existing int/size_t
        #   mismatches in service.cpp, lock_manager.cpp, wait_graph.cpp, and
        #   kyber_impl/; blocked from promotion until those are cleaned up in v1.3.0).
        list(APPEND warn_flags
            -Wshadow
            -Werror=shadow
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
