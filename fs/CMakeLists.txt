file(GLOB FS_SRC "*.cpp") # Use quotes for robustness

add_executable(minix_fs_server ${FS_SRC})

# FS server should link against the C library (minix_libc)
# Assuming minixlib will be renamed to minix_libc from lib/CMakeLists.txt
target_link_libraries(minix_fs_server PRIVATE minix_libc)

# Add include directories that might be needed by FS source files
target_include_directories(minix_fs_server PUBLIC
    "."     # For headers in fs/ itself (e.g., buf.h, const.h, dev.h, etc.)
    "${CMAKE_SOURCE_DIR}/include" # For global includes
    "${CMAKE_SOURCE_DIR}/h"       # For global includes from h/
)
