# TESTS/KERNEL/IPC/ALGEBRAIC/CMakeLists.txt

# Common settings for tests in this directory
set(TEST_INCLUDE_DIR ${CMAKE_SOURCE_DIR}) # To find KERNEL/IPC/ALGEBRAIC/... headers
set(LIB_COMPILE_STANDARD cxx_std_17) # Match library's standard or exceed

# --- Quaternion Test ---
add_executable(quaternion_algebraic_test
    quaternion_test.cpp
)

target_link_libraries(quaternion_algebraic_test PRIVATE
    math_algebras # Defined in KERNEL/IPC/ALGEBRAIC/CMakeLists.txt
)

target_include_directories(quaternion_algebraic_test PRIVATE
    ${TEST_INCLUDE_DIR}
)
target_compile_features(quaternion_algebraic_test PRIVATE ${LIB_COMPILE_STANDARD})

if (CMAKE_THREAD_PREFER_PTHREAD AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows") # PThreads not typical on Windows for std::thread
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(quaternion_algebraic_test PRIVATE Threads::Threads)
    endif()
endif()

add_test(NAME QuaternionAlgebraicTest COMMAND quaternion_algebraic_test)


# --- Octonion Test ---
add_executable(octonion_algebraic_test
    octonion_test.cpp
)

target_link_libraries(octonion_algebraic_test PRIVATE
    math_algebras # Defined in KERNEL/IPC/ALGEBRAIC/CMakeLists.txt
)

target_include_directories(octonion_algebraic_test PRIVATE
    ${TEST_INCLUDE_DIR}
)
target_compile_features(octonion_algebraic_test PRIVATE ${LIB_COMPILE_STANDARD})

add_test(NAME OctonionAlgebraicTest COMMAND octonion_algebraic_test)


# --- Sedenion Test ---
add_executable(sedenion_algebraic_test
    sedenion_test.cpp
)

target_link_libraries(sedenion_algebraic_test PRIVATE
    math_algebras # Defined in KERNEL/IPC/ALGEBRAIC/CMakeLists.txt
)

target_include_directories(sedenion_algebraic_test PRIVATE
    ${TEST_INCLUDE_DIR}
)
target_compile_features(sedenion_algebraic_test PRIVATE ${LIB_COMPILE_STANDARD})

add_test(NAME SedenionAlgebraicTest COMMAND sedenion_algebraic_test)


# General options for test executables if needed, e.g., output directory
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin_tests) # Example
