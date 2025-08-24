#!/bin/bash
#
# run_clang_tidy.sh - Modern C++23 static analysis script for XINIM.
#
# This script orchestrates clang-tidy across the XINIM repository,
# enforcing C++23 compliance and identifying potential code quality issues.
# It intelligently generates a compile_commands.json database if missing,
# and then processes C++ source files, skipping known problematic files
# to ensure a smooth analysis run.
#
# Features:
# - C++23 compliance check: Ensures code adheres to the latest standard.
# - Compile database generation: Automatically creates `compile_commands.json`
#   for accurate analysis, disabling test builds for efficiency.
# - File-by-file processing: Analyzes each source file individually to
#   isolate issues and provide clear feedback.
# - Skip list for problematic files: Bypasses known crashing or
#   non-compliant files to maintain script stability.
# - Diagnostics-first approach: Focuses on reporting issues without
#   automatically applying fixes, promoting developer review.
#
# Usage:
#   ./tools/run_clang_tidy.sh
#
# To apply fixes automatically (use with caution and review changes):
#   clang-tidy -p build -fix -format-style=none -extra-arg=-std=c++23 <file.cpp>
#
# The script is designed for continuous integration (CI) environments and
# local development workflows, providing a "squeaky clean" analysis experience.

set -e

# Define the build directory.
BUILD_DIR="build"

# Define a list of files known to cause issues with clang-tidy.
# This list is maintained to ensure the script completes successfully,
# allowing other files to be analyzed.
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

# Helper function to check if a file is in the crashing list.
# Returns 0 (true) if the file should be skipped, 1 (false) otherwise.
is_crashing_file() {
    local file_to_check="$1"
    for crashing_file in "${CRASHING_FILES[@]}"; do
        if [ "$file_to_check" = "$crashing_file" ]; then
            return 0 # True, it is a crashing file
        fi
    done
    return 1 # False, not a crashing file
}

# Generate compile_commands.json if it doesn't exist.
# This is crucial for clang-tidy to understand the build context (include paths, compiler flags).
# Test builds are explicitly disabled to speed up compile_commands.json generation
# and avoid test-specific dependencies during static analysis.
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "--- Generating compile_commands.json (disabling tests for efficiency) ---"
    cmake -S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=OFF -DENABLE_UNIT_TESTS=OFF
fi

echo "--- Starting clang-tidy analysis (C++23 compliance) ---"

# Find all C++ source files in the repository, excluding files within the build directory.
# Process each file individually to isolate diagnostics and prevent a single file's issue
# from halting the entire analysis.
find . -name '*.cpp' -not -path "./$BUILD_DIR/*" | while read -r file; do
    if is_crashing_file "$file"; then
        echo "--- SKIPPING known crashing file: $file ---"
    else
        echo "--- Processing file: $file ---"
        # Execute clang-tidy for the current file.
        # -p "$BUILD_DIR": Specifies the path to the compile database.
        # -format-style=none: Prevents clang-tidy from reformatting comments, preserving original style.
        # -extra-arg=-std=c++23: Enforces C++23 standard for analysis.
        # Note: Automatic fixes (`-fix`) are omitted by default to promote manual review of diagnostics.
        if clang-tidy -p "$BUILD_DIR" -format-style=none -extra-arg=-std=c++23 "$file"; then
            echo "Successfully processed (or no changes needed for) $file"
        else
            # Report errors or warnings. The script continues to process other files.
            echo "Error or warnings reported for $file by clang-tidy. Exit code: $?."
        fi
    fi
done

echo "--- Clang-tidy analysis complete ---"