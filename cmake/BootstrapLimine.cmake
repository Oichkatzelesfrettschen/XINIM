cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED XINIM_LIMINE_VERSION OR XINIM_LIMINE_VERSION STREQUAL "")
    set(XINIM_LIMINE_VERSION "v10.8.3-binary")
endif()

if(NOT DEFINED XINIM_LIMINE_ROOT OR XINIM_LIMINE_ROOT STREQUAL "")
    message(FATAL_ERROR "XINIM_LIMINE_ROOT must be set")
endif()

set(XINIM_LIMINE_REPO "https://github.com/limine-bootloader/limine.git")
set(_required_files
    "${XINIM_LIMINE_ROOT}/limine"
    "${XINIM_LIMINE_ROOT}/limine-bios.sys"
    "${XINIM_LIMINE_ROOT}/limine-bios-cd.bin"
    "${XINIM_LIMINE_ROOT}/limine-uefi-cd.bin"
    "${XINIM_LIMINE_ROOT}/BOOTX64.EFI"
)

set(_all_present TRUE)
foreach(_path IN LISTS _required_files)
    if(NOT EXISTS "${_path}")
        set(_all_present FALSE)
        break()
    endif()
endforeach()

if(_all_present)
    message(STATUS "Limine assets already present at ${XINIM_LIMINE_ROOT}")
    return()
endif()

find_program(GIT_EXECUTABLE git REQUIRED)
find_program(MAKE_EXECUTABLE make REQUIRED)

file(MAKE_DIRECTORY "${XINIM_LIMINE_ROOT}")
if(NOT EXISTS "${XINIM_LIMINE_ROOT}/.git")
    file(REMOVE_RECURSE "${XINIM_LIMINE_ROOT}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" clone
                "${XINIM_LIMINE_REPO}"
                --branch "${XINIM_LIMINE_VERSION}"
                --depth 1
                "${XINIM_LIMINE_ROOT}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

execute_process(
    COMMAND "${MAKE_EXECUTABLE}"
    WORKING_DIRECTORY "${XINIM_LIMINE_ROOT}"
    COMMAND_ERROR_IS_FATAL ANY
)

foreach(_path IN LISTS _required_files)
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Missing expected Limine asset: ${_path}")
    endif()
endforeach()
