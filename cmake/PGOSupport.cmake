include_guard()

# WHY: Profile-Guided Optimization (PGO) for bare-metal kernels requires a
# two-phase build.  Phase 1 (generate) instruments every function; the kernel
# runs in VirtualBox and dumps raw counter data to COM1 serial.  Phase 2 (use)
# builds with the merged profile to guide inlining, branch prediction, and
# block layout.  Thin LTO amplifies PGO by applying feedback across TUs.
#
# WHAT: Provides the XINIM_PGO_MODE cache variable and
#       xinim_apply_pgo_flags(<target>) helper consumed by kernel and userland
#       targets that should participate in PGO.
#
# HOW:  cmake -DXINIM_PGO_MODE=generate ...
#       Build, boot in VBox, collect COM1 log.
#       python3 scripts/extract_pgo_profile.py --log <com1.log> --out pgo.profraw
#       llvm-profdata merge pgo.profraw -o pgo.profdata
#       cmake -DXINIM_PGO_MODE=use -DXINIM_PGO_DATA=pgo.profdata ...

set(XINIM_PGO_MODE "none" CACHE STRING
    "PGO mode: none | generate | use")
set_property(CACHE XINIM_PGO_MODE PROPERTY STRINGS none generate use)

set(XINIM_PGO_DATA "" CACHE FILEPATH
    "Path to merged .profdata file for PGO use mode")

if(NOT XINIM_PGO_MODE STREQUAL "none" AND
   NOT XINIM_PGO_MODE STREQUAL "generate" AND
   NOT XINIM_PGO_MODE STREQUAL "use")
    message(FATAL_ERROR
        "Unknown XINIM_PGO_MODE='${XINIM_PGO_MODE}'. "
        "Expected one of: none, generate, use.")
endif()

if(XINIM_PGO_MODE STREQUAL "use" AND NOT XINIM_PGO_DATA)
    message(FATAL_ERROR
        "XINIM_PGO_DATA must be set to a .profdata path when "
        "XINIM_PGO_MODE=use.")
endif()

# Apply PGO compile/link flags to the given target.
# No-op when XINIM_PGO_MODE=none.
function(xinim_apply_pgo_flags target)
    if(XINIM_PGO_MODE STREQUAL "generate")
        target_compile_options(${target} PRIVATE -fprofile-instr-generate)
        target_link_options(${target} PRIVATE -fprofile-instr-generate)
        # Link the Clang profile runtime (bare-metal i386 variant).
        # OS-dependent symbols are stubbed in pgo_dump.cpp.
        find_library(_clang_profile_rt
            NAMES clang_rt.profile-i386
            HINTS "/usr/lib/clang/${CMAKE_CXX_COMPILER_VERSION}/lib/linux"
                  "/usr/lib/clang/22/lib/linux"
                  "/usr/lib/clang/21/lib/linux"
                  "/usr/lib/clang/20/lib/linux"
        )
        if(_clang_profile_rt)
            target_link_libraries(${target} PRIVATE "${_clang_profile_rt}")
        else()
            message(WARNING
                "libclang_rt.profile-i386.a not found; PGO generate build "
                "may fail to link.  Set _clang_profile_rt manually if needed.")
        endif()
    elseif(XINIM_PGO_MODE STREQUAL "use")
        target_compile_options(${target} PRIVATE
            -fprofile-instr-use=${XINIM_PGO_DATA})
        target_link_options(${target} PRIVATE
            -fprofile-instr-use=${XINIM_PGO_DATA})
    endif()
endfunction()
