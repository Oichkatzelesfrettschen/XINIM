# lib/xinim_fs/CMakeLists.txt
cmake_minimum_required(VERSION 3.12)
project(XinimFS CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the xinim_fs library
add_library(xinim_fs STATIC
    filesystem_ops.cpp
    # Add other .cpp files for xinim::fs as they are created
)

# Target include directories for xinim_fs library itself
# This allows filesystem_ops.cpp to find <xinim/filesystem.hpp>
# Assuming this CMakeLists.txt is in lib/xinim_fs/
# and the headers are in <project_root>/include/
target_include_directories(xinim_fs PUBLIC
    "${CMAKE_PROJECT_SOURCE_DIR}/include"
    # PUBLIC so anything linking xinim_fs also gets these include paths if needed,
    # though typically utilities will include "xinim/filesystem.hpp" directly via their own include paths.
    # For xinim_fs internal sources, PRIVATE might be more appropriate if it has its own internal headers.
    # For now, PUBLIC is fine for making sure <xinim/filesystem.hpp> is found by its sources.
)

target_link_libraries(xinim_fs PRIVATE xinim_common)

# Required for std::filesystem if not automatically handled by C++23 setting
# For some older CMake/compilers, explicitly linking might be needed.
# Modern CMake with C++17+ usually handles this.
# For C++23, this should definitely be handled.
# target_link_libraries(xinim_fs PRIVATE stdc++fs) # Example, if needed
