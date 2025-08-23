# commands/tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(XinimCommandsTests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable CTest
enable_testing()

# Define the test sources
set(XINIM_FS_TEST_SOURCES
    test_xinim_fs_create_directory.cpp
    test_xinim_fs_change_permissions.cpp
    test_xinim_fs_get_full_status.cpp
    # Add future xinim::fs test files here
)

set(MODE_PARSER_TEST_SOURCES
    test_mode_parser.cpp
    # Add future parser/utility logic test files here
)

# Common include directories for tests
# Assuming this CMakeLists.txt is in commands/tests/
# and headers are in <project_root>/include/
# and xinim_fs library is defined by CMakeLists.txt in ../../lib/xinim_fs

# Build and register tests for xinim::fs methods
foreach(TEST_SOURCE_FILE ${XINIM_FS_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE_FILE})
    # Link against xinim_fs (defined in lib/xinim_fs/CMakeLists.txt)
    # and std::filesystem (often needed explicitly for older CMake or some toolchains)
    # Modern CMake with C++17+ usually links std::filesystem automatically if <filesystem> is included.
    target_link_libraries(${TEST_NAME} PRIVATE xinim_fs stdc++fs xinim_common) # stdc++fs for GCC/Clang if needed
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Build and register tests for ModeParser (and potentially other utility logic)
foreach(TEST_SOURCE_FILE ${MODE_PARSER_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE_FILE})
    # ModeParser tests might not need xinim_fs if ModeParser is self-contained
    # or included directly. If ModeParser becomes part of a utility library, link that.
    # For now, assuming it might need std::filesystem if it uses types from there.
    target_link_libraries(${TEST_NAME} PRIVATE stdc++fs xinim_common) # Link std::filesystem if needed
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# To make this work, the main commands/CMakeLists.txt will need to:
# 1. add_subdirectory(../lib/xinim_fs)  (or similar path to where xinim_fs is defined)
# 2. add_subdirectory(tests)
# And utilities in commands/CMakeLists.txt will need to target_link_libraries(... xinim_fs)
