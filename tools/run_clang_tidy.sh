#!/bin/bash
# Run clang-tidy across the repository with C++23 compliance.
# This script generates compile_commands if needed and then
# executes clang-tidy with automatic fixes. Comment lines are
# left untouched since clang-tidy is invoked with FormatStyle=none.
# MODIFIED: Process files one by one to isolate crashes.
# MODIFIED: Skip known crashing files.
# MODIFIED: Changed shebang to /bin/bash for array and function support.

set -e
BUILD_DIR="build"

# Create build directory and compile database if missing
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Generating compile_commands.json..."
    cmake -S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

CRASHING_FILES=(
    "./commands/ln.cpp"
    "./commands/basename.cpp"
    "./commands/ar.cpp"
    "./tools/fsck.cpp"
    "./tools/diskio.cpp"
    "./tools/bootblok1.cpp"
    "./tools/build.cpp"
    "./mm/utility.cpp"
)

is_crashing_file() {
    local file_to_check="$1"
    for crashing_file in "${CRASHING_FILES[@]}"; do
        if [ "$file_to_check" = "$crashing_file" ]; then
            return 0 # True, it is a crashing file
        fi
    done
    return 1 # False, not a crashing file
}

# Find all C++ source files excluding the build directory
find . -name '*.cpp' -not -path "./$BUILD_DIR/*" | while read -r file; do
    if is_crashing_file "$file"; then
        echo "--- SKIPPING known crashing file: $file ---"
    else
        echo "--- Processing file: $file ---"
        if clang-tidy -p "$BUILD_DIR" -fix -format-style=none -extra-arg=-std=c++23 "$file"; then
            echo "Successfully processed (or no changes needed for) $file"
        else
            echo "Error or warnings reported for $file by clang-tidy. Exit code: $?. Fixes might not have been applied if errors were severe."
        fi
    fi
done

echo "Clang-tidy run complete."
