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

file(GLOB CMD_SOURCES "*.cpp")
# Filter out legacy duplicates, placeholder sources, and the compiler driver.
list(FILTER CMD_SOURCES EXCLUDE REGEX ".*\\s2\\.cpp$")
list(FILTER CMD_SOURCES EXCLUDE REGEX "pr_modern\\.cpp$")
list(FILTER CMD_SOURCES EXCLUDE REGEX "sort_modern\\.cpp$")
list(FILTER CMD_SOURCES EXCLUDE REGEX "tar_modern\\.cpp$")
list(FILTER CMD_SOURCES EXCLUDE REGEX "cc\\.cpp$")


# TODO: Exclude main files of multi-file applications like 'mined' from this generic loop.
# They should have their own targets defined or be handled by a dedicated CMakeLists.txt
# (e.g. if CMakeLists_mined.txt is integrated).
# Example exclusion (adjust pattern as needed):
# list(FILTER CMD_SOURCES EXCLUDE REGEX "mined.*\.cpp$")
# list(FILTER CMD_SOURCES EXCLUDE REGEX "filesystem_ops\.cpp$") # if it were here by mistake

foreach(CMD_SOURCE_FILE ${CMD_SOURCES})
  get_filename_component(COMMAND_NAME ${CMD_SOURCE_FILE} NAME_WE)
  set(TARGET_NAME minix_cmd_${COMMAND_NAME})

  # Avoid creating targets for cpp files that are part of libraries or manually handled.
  # This is a simple check; a more robust way is to explicitly list command sources
  # or use a more refined GLOB/filtering.
  # For this exercise, we assume single .cpp file per command for this loop.
  # Files like 'filesystem_ops.cpp' are in their own library (xinim_fs)
  # and test files are handled by commands/tests/CMakeLists.txt.
  # If CMD_SOURCES picks up files from subdirectories like 'tests/', they need to be excluded.
  # A simple way is to ensure GLOB doesn't recurse, or filter by path.
  # For now, assume GLOB gets only top-level .cpp files in 'commands/'.

  # A more robust way to skip library sources if they were accidentally included:
  # if (TARGET_NAME STREQUAL "minix_cmd_filesystem_ops" OR TARGET_NAME STREQUAL "minix_cmd_another_lib_file")
  #   continue()
  # endif()


  add_executable(${TARGET_NAME} ${CMD_SOURCE_FILE})

  # Link each command against the C library (minix_libc) and xinim_fs
  target_link_libraries(${TARGET_NAME} PRIVATE minix_libc xinim_fs)

  # Add include directories for each command
  target_include_directories(${TARGET_NAME} PUBLIC
    "."  # For any local headers specific to a command (rare)
    # CMAKE_SOURCE_DIR here should be the root of the project if this commands/CMakeLists.txt
    # is part of a larger build added via add_subdirectory() from a root CMakeLists.txt.
    # If this commands/CMakeLists.txt IS the root, then CMAKE_SOURCE_DIR is commands/.
    # Assuming it's part of a larger project, so CMAKE_SOURCE_DIR is the project root.
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
  )

  if(COMMAND_NAME STREQUAL "cc")
    target_compile_options(${TARGET_NAME} PRIVATE -O3 -flto)
    target_link_options(${TARGET_NAME} PRIVATE -flto)
  endif()
endforeach()

# Explicit compiler driver target
add_executable(minix_cmd_cc cc.cpp)
target_link_libraries(minix_cmd_cc PRIVATE minix_libc xinim_fs)
target_include_directories(minix_cmd_cc PUBLIC
  "."
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/h"
)