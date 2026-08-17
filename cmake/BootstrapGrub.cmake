cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED XINIM_GRUB_VERSION OR XINIM_GRUB_VERSION STREQUAL "")
    set(XINIM_GRUB_VERSION "2.14")
endif()

if(NOT DEFINED XINIM_GRUB_ARCHIVE_SHA256 OR
   XINIM_GRUB_ARCHIVE_SHA256 STREQUAL "")
    message(FATAL_ERROR "XINIM_GRUB_ARCHIVE_SHA256 must be set")
endif()

if(NOT DEFINED XINIM_GRUB_ROOT OR XINIM_GRUB_ROOT STREQUAL "")
    message(FATAL_ERROR "XINIM_GRUB_ROOT must be set")
endif()

if(NOT DEFINED XINIM_GRUB_SOURCE_DIR OR XINIM_GRUB_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "XINIM_GRUB_SOURCE_DIR must be set")
endif()

if(NOT DEFINED XINIM_GRUB_BUILD_DIR OR XINIM_GRUB_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "XINIM_GRUB_BUILD_DIR must be set")
endif()

if(NOT DEFINED XINIM_GRUB_INSTALL_ROOT OR XINIM_GRUB_INSTALL_ROOT STREQUAL "")
    message(FATAL_ERROR "XINIM_GRUB_INSTALL_ROOT must be set")
endif()

set(XINIM_GRUB_ARCHIVE_NAME "grub-${XINIM_GRUB_VERSION}.tar.xz")
set(XINIM_GRUB_ARCHIVE_URL
    "https://ftp.gnu.org/gnu/grub/${XINIM_GRUB_ARCHIVE_NAME}")
set(XINIM_GRUB_ARCHIVE_PATH "${XINIM_GRUB_ROOT}/downloads/${XINIM_GRUB_ARCHIVE_NAME}")
set(XINIM_GRUB_BOOTSTRAP_EXECUTABLE "${XINIM_GRUB_INSTALL_ROOT}/bin/grub-mkrescue")
set(_required_paths
    "${XINIM_GRUB_BOOTSTRAP_EXECUTABLE}"
    "${XINIM_GRUB_INSTALL_ROOT}/lib/grub/i386-pc"
)

set(_all_present TRUE)
foreach(_path IN LISTS _required_paths)
    if(NOT EXISTS "${_path}")
        set(_all_present FALSE)
        break()
    endif()
endforeach()

if(_all_present)
    message(STATUS "GNU GRUB already present at ${XINIM_GRUB_INSTALL_ROOT}")
    return()
endif()

find_program(MAKE_EXECUTABLE make REQUIRED)
find_program(TAR_EXECUTABLE tar REQUIRED)

file(MAKE_DIRECTORY "${XINIM_GRUB_ROOT}")
file(MAKE_DIRECTORY "${XINIM_GRUB_ROOT}/downloads")

if(NOT EXISTS "${XINIM_GRUB_ARCHIVE_PATH}")
    message(STATUS "Downloading GNU GRUB ${XINIM_GRUB_VERSION} from ${XINIM_GRUB_ARCHIVE_URL}")
    file(
        DOWNLOAD
        "${XINIM_GRUB_ARCHIVE_URL}"
        "${XINIM_GRUB_ARCHIVE_PATH}"
        EXPECTED_HASH "SHA256=${XINIM_GRUB_ARCHIVE_SHA256}"
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()

file(REMOVE_RECURSE "${XINIM_GRUB_SOURCE_DIR}" "${XINIM_GRUB_BUILD_DIR}")
file(MAKE_DIRECTORY "${XINIM_GRUB_ROOT}/source")

execute_process(
    COMMAND "${TAR_EXECUTABLE}" -xf "${XINIM_GRUB_ARCHIVE_PATH}" -C "${XINIM_GRUB_ROOT}/source"
    COMMAND_ERROR_IS_FATAL ANY
)

file(MAKE_DIRECTORY "${XINIM_GRUB_BUILD_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            CC=clang
            CXX=clang++
            CFLAGS=
            CXXFLAGS=
            CPPFLAGS=
            LDFLAGS=
            "TARGET_CC=clang"
            "TARGET_CCAS=clang"
            "${XINIM_GRUB_SOURCE_DIR}/configure"
            --prefix=${XINIM_GRUB_INSTALL_ROOT}
            --disable-werror
            --disable-nls
            --with-platform=pc
            --target=i386
    WORKING_DIRECTORY "${XINIM_GRUB_BUILD_DIR}"
    COMMAND_ERROR_IS_FATAL ANY
)

set(_xinim_grub_core_makefile "${XINIM_GRUB_BUILD_DIR}/grub-core/Makefile")
file(READ "${_xinim_grub_core_makefile}" _xinim_grub_core_makefile_contents)
string(REPLACE
    "TARGET_IMG_BASE_LDOPT = -Wl,--image-base"
    "TARGET_IMG_BASE_LDOPT = -Wl,-Ttext"
    _xinim_grub_core_makefile_contents
    "${_xinim_grub_core_makefile_contents}"
)
file(WRITE "${_xinim_grub_core_makefile}" "${_xinim_grub_core_makefile_contents}")

include(ProcessorCount)
ProcessorCount(_xinim_grub_jobs)
if(NOT _xinim_grub_jobs OR _xinim_grub_jobs LESS 1)
    set(_xinim_grub_jobs 1)
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            CC=clang
            CXX=clang++
            CFLAGS=
            CXXFLAGS=
            CPPFLAGS=
            LDFLAGS=
            "${MAKE_EXECUTABLE}" "-j${_xinim_grub_jobs}"
    WORKING_DIRECTORY "${XINIM_GRUB_BUILD_DIR}"
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            CC=clang
            CXX=clang++
            CFLAGS=
            CXXFLAGS=
            CPPFLAGS=
            LDFLAGS=
            "${MAKE_EXECUTABLE}" install
    WORKING_DIRECTORY "${XINIM_GRUB_BUILD_DIR}"
    COMMAND_ERROR_IS_FATAL ANY
)

foreach(_path IN LISTS _required_paths)
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Missing expected GNU GRUB artifact: ${_path}")
    endif()
endforeach()
