// Pure C++23 implementation of POSIX 'true' utility
// No libc dependencies - only libc++ (C++ standard library)

#include <cstdlib>  // For EXIT_SUCCESS

// Main entry point - minimal implementation
// POSIX true: Always returns success (0)
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // C++23 attributes to mark parameters as intentionally unused
    // No processing needed - true always succeeds
    return EXIT_SUCCESS;  // Guaranteed to be 0 by C++ standard
}