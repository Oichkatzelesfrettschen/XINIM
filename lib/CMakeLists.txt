# This CMake file builds the Minix C standard library (minix_libc).
# It includes sources from the current directory (lib/) and the lib/minix/ subdirectory.

file(GLOB LIB_SRC
    CONFIGURE_DEPENDS
    "*.cpp"
    "minix/*.cpp"
    "io/src/*.cpp"
)

# Exclude legacy duplicates now provided inline.
list(REMOVE_ITEM LIB_SRC
    "${CMAKE_CURRENT_LIST_DIR}/getc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/putc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/io/src/stdio_compat.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/regexp.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/regsub.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/signal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sleep.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprintf.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/catchsig.cpp"  # Temporarily excluded due to signal conflicts
    "${CMAKE_CURRENT_LIST_DIR}/stb.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/syslib.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ungetc.cpp"
)

# Find OpenSSL for modern cryptographic primitives
find_package(OpenSSL REQUIRED)

# Define the static library target as minix_libc
add_library(minix_libc STATIC ${LIB_SRC})

# Add include directories for minix_libc
# PUBLIC: Targets linking to minix_libc will also include these directories.
# INTERFACE: Only targets linking to minix_libc will include these (if minix_libc itself doesn't need them for compilation).
# PRIVATE: Only for minix_libc's own compilation.
# Using PUBLIC for broad system headers that libc consumers would also need.
target_include_directories(minix_libc PUBLIC
    "${CMAKE_SOURCE_DIR}/include" # For standard headers like stdio.h, string.h etc. (now .hpp)
    "${CMAKE_SOURCE_DIR}/h"       # For Minix-specific system headers like type.h, const.h etc. (now .hpp)
)

# It might also need its own directory for internal headers, if any.
target_include_directories(minix_libc PRIVATE
    "." # For any headers specific to lib's internal implementation
)

# Link against OpenSSL's crypto library to expose SHA-256
target_link_libraries(minix_libc PUBLIC OpenSSL::Crypto)
