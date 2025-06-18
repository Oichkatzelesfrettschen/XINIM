file(GLOB MM_SRC "*.cpp") # Use quotes for robustness

add_executable(minix_mm_server ${MM_SRC})

# MM server should link against the C library (minix_libc)
# Assuming minixlib will be renamed to minix_libc from lib/CMakeLists.txt
target_link_libraries(minix_mm_server PRIVATE minix_libc)

# Add include directories that might be needed by MM source files
target_include_directories(minix_mm_server PUBLIC
    "."     # For headers in mm/ itself like mproc.h, const.h, glo.h, type.h, param.h
    "${CMAKE_SOURCE_DIR}/include" # For global includes
    "${CMAKE_SOURCE_DIR}/h"       # For global includes from h/
)
