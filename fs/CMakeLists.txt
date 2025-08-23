cmake_minimum_required(VERSION 3.12)

# File system server build configuration
# --------------------------------------
# Explicitly enumerate each translation unit to avoid unintentionally pulling
# in editor backups or experimental sources.  This mirrors the behaviour of the
# original handwritten makefiles and keeps dependencies transparent.

set(FS_SRC
    cache.cpp
    compat.cpp
    device.cpp
    filedes.cpp
    inode.cpp
    link.cpp
    main.cpp
    misc.cpp
    mount.cpp
    open.cpp
    path.cpp
    pipe.cpp
    protect.cpp
    putc.cpp
    read.cpp
    stadir.cpp
    super.cpp
    table.cpp
    time.cpp
    utility.cpp
    write.cpp
)

add_executable(minix_fs_server ${FS_SRC})

# Link against the Minix C library implemented in lib/
target_link_libraries(minix_fs_server PRIVATE minix_libc xinim_common)

# Include paths specific to the file system sources
target_include_directories(minix_fs_server PRIVATE .)

