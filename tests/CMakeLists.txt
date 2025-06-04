#Unit tests for Minix library routines and basic system calls

#Library routine tests compile selected sources directly
add_executable(minix_test_lib
    test_lib.cpp
    "${CMAKE_SOURCE_DIR}/lib/strlen.cpp"  # Use CMAKE_SOURCE_DIR for clarity
    "${CMAKE_SOURCE_DIR}/lib/strcmp.cpp"
    "${CMAKE_SOURCE_DIR}/lib/rand.cpp"
)

#Ensure lib_test can find necessary headers from the C library's public interface
target_include_directories(minix_test_lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
)

target_compile_options(minix_test_lib PRIVATE -std=c++17 -fno-builtin)

add_test(NAME minix_test_lib COMMAND minix_test_lib)

#System call tests rely on the host C library only(or direct syscalls)
add_executable(minix_test_syscall test_syscall.cpp)
target_include_directories(minix_test_syscall PUBLIC
    "${CMAKE_SOURCE_DIR}/include" # May need some type definitions
    "${CMAKE_SOURCE_DIR}/h"
)
add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

#Add Stream architecture tests
add_executable(minix_test_streams test_streams.cpp)
target_link_libraries(minix_test_streams PRIVATE minix_libc)
target_include_directories(minix_test_streams PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
)
add_test(NAME minix_test_streams COMMAND minix_test_streams)

#Wormhole unit test
add_executable(minix_test_fastpath test_fastpath.cpp
    "${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp")
target_include_directories(minix_test_fastpath PUBLIC
    "${CMAKE_SOURCE_DIR}/kernel"  # For fastpath headers
)
add_test(NAME minix_test_fastpath COMMAND minix_test_fastpath)

#Semantic region unit test
add_executable(minix_test_semantic_region test_semantic_region.cpp)
target_include_directories(minix_test_semantic_region PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
add_test(NAME minix_test_semantic_region COMMAND minix_test_semantic_region)

#Stream foundation verification test
add_executable(minix_test_stream_foundation
    test_stream_foundation.cpp
    "${CMAKE_SOURCE_DIR}/lib/io/src/file_stream.cpp"
    "${CMAKE_SOURCE_DIR}/lib/io/src/file_operations.cpp"
    "${CMAKE_SOURCE_DIR}/lib/io/src/standard_streams.cpp"
)
set_target_properties(minix_test_stream_foundation PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(minix_test_stream_foundation PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
add_test(NAME minix_test_stream_foundation COMMAND minix_test_stream_foundation)
