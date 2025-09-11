#include <cstdlib>

/**
 * @file malloc.cpp
 * @brief Thin wrappers around the C standard library allocation routines.
 *
 * @sideeffects Request and release heap memory.
 * @thread_safety Follows the thread safety of the underlying allocator.
 * @compat malloc(3), realloc(3), free(3)
 * @example
 * char *p = malloc(10);
 * p = realloc(p, 20);
 * free(p);
 */

/// Allocate memory using the system allocator.
char *malloc(unsigned size) noexcept { return static_cast<char *>(std::malloc(size)); }

/// Reallocate a memory block using the system allocator.
char *realloc(char *old, unsigned size) noexcept {
    return static_cast<char *>(std::realloc(old, size));
}

/// Free a memory block previously allocated with malloc/realloc.
void free(char *p) noexcept { std::free(p); }
