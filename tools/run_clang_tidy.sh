#!/bin/sh
# Run clang-tidy across the repository with C++17 compliance.
# This script generates compile_commands if needed and then
# executes clang-tidy with automatic fixes. Comment lines are
# left untouched since clang-tidy is invoked with FormatStyle=none.

set -e
BUILD_DIR="build"

# Create build directory and compile database if missing
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    cmake -S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

# Find all C source files excluding the build directory and run clang-tidy
find . -name '*.cpp' -not -path "./$BUILD_DIR/*" \
    | xargs -r clang-tidy -p "$BUILD_DIR" -fix -format-style=none -extra-arg=-std=c++17 "$@"
