# commands/CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(MinixCommands CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the xinim::fs library subdirectory (assuming lib is sibling to commands)
# Use CMAKE_CURRENT_BINARY_DIR to ensure out-of-source builds for the lib
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/xinim_fs ${CMAKE_CURRENT_BINARY_DIR}/lib/xinim_fs)

# Enable testing and add the tests subdirectory
enable_testing() # Call this before add_subdirectory for tests
add_subdirectory(tests ${CMAKE_CURRENT_BINARY_DIR}/tests)

# This CMake file builds each userland command as a separate executable.

# -----------------------------------------------------------------------------
# Explicit enumeration of standalone command sources. Using an explicit list
# avoids accidentally building backup or experimental files and mirrors the
# behaviour of traditional handwritten makefiles used in academia.
# -----------------------------------------------------------------------------
set(SINGLE_COMMANDS
    ar
    basename
    cal
    cat
    cc
    chmem
    chmod
    chown
    clr
    cmp
    comm
    cp
    date
    dd
    df
    dosread
    echo
    getlf
    grep
    gres
    head
    kill
    libpack
    libupack
    ln
    login
    lpr
    ls
    make
    mkdir
    mkfs
    mknod
    mount
    mv
    od
    passwd
    pr
    pr_modern
    pwd
    rev
    rm
    rmdir
    roff
    sh1
    sh3
    sh4
    sh5
    shar
    size
    sleep
    sort
    sort_modern
    split
    stty
    su
    sum
    svcctl
    sync
    tail
    tar
    tar_modern
    tee
    time
    touch
    tr
    umount
    uniq
    update
    wc
    x
)

foreach(COMMAND_NAME IN LISTS SINGLE_COMMANDS)
    add_executable(minix_cmd_${COMMAND_NAME} ${COMMAND_NAME}.cpp)
    target_link_libraries(minix_cmd_${COMMAND_NAME} PRIVATE minix_libc xinim_fs)
    target_include_directories(minix_cmd_${COMMAND_NAME} PUBLIC
      "."
      "${CMAKE_SOURCE_DIR}/include"
      "${CMAKE_SOURCE_DIR}/h"
    )
    # Apply a consistent set of rigorous compile options.
    target_compile_options(minix_cmd_${COMMAND_NAME} PRIVATE
      -O3
      -Wall
      -Wextra
      -Wpedantic
      -Werror
    )
endforeach()

# -----------------------------------------------------------------------------
# Multi-file command targets
# -----------------------------------------------------------------------------

# Modern MINED editor target with explicit dependencies.
set(MINED_SOURCES
    mined_main.cpp
    mined_editor.cpp
    mined.cpp
)

add_executable(minix_cmd_mined ${MINED_SOURCES})
target_link_libraries(minix_cmd_mined PRIVATE minix_libc xinim_fs)
target_include_directories(minix_cmd_mined PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
)
target_compile_options(minix_cmd_mined PRIVATE
    -O3
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)