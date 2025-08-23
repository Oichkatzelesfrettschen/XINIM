#include "../include/lib.hpp" // C++23 header
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Allocate memory and abort on failure.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory.
 * @sideeffects May terminate the process on allocation failure.
 * @thread_safety Thread-safe if underlying malloc is thread-safe.
 * @compat malloc(3)
 * @example
 * auto *buf = static_cast<char*>(safe_malloc(128));
 */
void *safe_malloc(size_t size) noexcept {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return ptr;
}

/**
 * @brief Free memory if pointer not @c nullptr.
 *
 * @param ptr Pointer previously allocated with ::safe_malloc.
 * @sideeffects Releases the memory pointed to by @p ptr.
 * @thread_safety Thread-safe if underlying free is thread-safe.
 * @compat free(3)
 * @example
 * safe_free(buf);
 */
void safe_free(void *ptr) noexcept {
    if (ptr != NULL) {
        free(ptr);
    }
}
