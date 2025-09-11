// Pure C++23 implementation of POSIX 'false' utility
// No libc dependencies - only libc++ (C++ standard library)

#include <cstdlib>  // For EXIT_FAILURE

// Main entry point - minimal implementation
// POSIX false: Always returns failure (non-zero)
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // C++23 attributes to mark parameters as intentionally unused
    // No processing needed - false always fails
    return EXIT_FAILURE;  // Guaranteed to be non-zero by C++ standard
}