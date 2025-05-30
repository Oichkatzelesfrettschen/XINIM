# Unit tests for Minix library routines and basic system calls

# Library routine tests compile selected sources directly
add_executable(lib_test
    test_lib.cpp
    ${CMAKE_SOURCE_DIR}/lib/strlen.cpp
    ${CMAKE_SOURCE_DIR}/lib/strcmp.cpp
    ${CMAKE_SOURCE_DIR}/lib/rand.cpp
)

target_compile_options(lib_test PRIVATE -std=c90 -fno-builtin)

add_test(NAME lib_test COMMAND lib_test)

# System call tests rely on the host C library only
add_executable(syscall_test test_syscall.cpp)
add_test(NAME syscall_test COMMAND syscall_test)
