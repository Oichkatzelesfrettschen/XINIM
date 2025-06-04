# Unit tests for Minix library routines and basic system calls

# Library routine tests compile selected sources directly
add_executable(minix_test_lib
    test_lib.cpp
    "${CMAKE_SOURCE_DIR}/lib/strlen.cpp"  # Use CMAKE_SOURCE_DIR for clarity
    "${CMAKE_SOURCE_DIR}/lib/strcmp.cpp"
    "${CMAKE_SOURCE_DIR}/lib/rand.cpp"
)

# Ensure lib_test can find necessary headers from the C library's public interface
target_include_directories(minix_test_lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
)

target_compile_options(minix_test_lib PRIVATE -std=c++17 -fno-builtin)

add_test(NAME minix_test_lib COMMAND minix_test_lib)

# System call tests rely on the host C library only (or direct syscalls)
add_executable(minix_test_syscall test_syscall.cpp)
target_include_directories(minix_test_syscall PUBLIC
    "${CMAKE_SOURCE_DIR}/include" # May need some type definitions
    "${CMAKE_SOURCE_DIR}/h"
)
add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

# Add Stream architecture tests
add_executable(minix_test_streams test_streams.cpp)
target_link_libraries(minix_test_streams PRIVATE minix_libc)
target_include_directories(minix_test_streams PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
)
add_test(NAME minix_test_streams COMMAND minix_test_streams)
