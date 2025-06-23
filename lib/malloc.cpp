#include <cstdlib>

/**
 * @file malloc.cpp
 * @brief Thin wrappers around the C standard library allocation routines.
 */

/// Allocate memory using the system allocator.
char *malloc(unsigned size) noexcept { return static_cast<char *>(std::malloc(size)); }

/// Reallocate a memory block using the system allocator.
char *realloc(char *old, unsigned size) noexcept {
    return static_cast<char *>(std::realloc(old, size));
}

/// Free a memory block previously allocated with malloc/realloc.
void free(char *p) noexcept { std::free(p); }
